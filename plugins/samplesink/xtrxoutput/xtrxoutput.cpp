///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, 2018 Edouard Griffiths, F4EXB                             //
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////
#include <cstddef>
#include <string.h>
#include "xtrx_api.h"

#include <QMutexLocker>
#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGXtrxOutputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGXtrxOutputReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "xtrxoutput.h"
#include "xtrxoutputthread.h"
#include "xtrx/devicextrxparam.h"
#include "xtrx/devicextrxshared.h"
#include "xtrx/devicextrx.h"

MESSAGE_CLASS_DEFINITION(XTRXOutput::MsgConfigureXTRX, Message)
MESSAGE_CLASS_DEFINITION(XTRXOutput::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXOutput::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXOutput::MsgReportClockGenChange, Message)
MESSAGE_CLASS_DEFINITION(XTRXOutput::MsgReportStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXOutput::MsgStartStop, Message)

XTRXOutput::XTRXOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_XTRXOutputThread(0),
    m_deviceDescription("XTRXOutput"),
    m_running(false)
{
    openDevice();
    m_deviceAPI->setNbSinkStreams(1);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &XTRXOutput::networkManagerFinished
    );
}

XTRXOutput::~XTRXOutput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &XTRXOutput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void XTRXOutput::destroy()
{
    delete this;
}

bool XTRXOutput::openDevice()
{
    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_settings.m_devSampleRate));

    // look for Tx buddies and get reference to the device object
    if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("XTRXOutput::openDevice: look in Tx buddies");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceXTRXShared *deviceXTRXShared = (DeviceXTRXShared*) sinkBuddy->getBuddySharedPtr();

        if (deviceXTRXShared == 0)
        {
            qCritical("XTRXOutput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        DeviceXTRX *device = deviceXTRXShared->m_dev;

        if (device == 0)
        {
            qCritical("XTRXOutput::openDevice: cannot get device pointer from Tx buddy");
            return false;
        }

        m_deviceShared.m_dev = device;
    }
    // look for Rx buddies and get reference to the device object
    else if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        qDebug("XTRXOutput::openDevice: look in Rx buddies");

        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceXTRXShared *deviceXTRXShared = (DeviceXTRXShared*) sourceBuddy->getBuddySharedPtr();

        if (deviceXTRXShared == 0)
        {
            qCritical("XTRXOutput::openDevice: the source buddy shared pointer is null");
            return false;
        }

        DeviceXTRX *device = deviceXTRXShared->m_dev;

        if (device == 0)
        {
            qCritical("XTRXOutput::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_dev = device;
    }
    // There are no buddies then create the first BladeRF2 device
    else
    {
        qDebug("XTRXOutput::openDevice: open device here");

        m_deviceShared.m_dev = new DeviceXTRX();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));

        if (!m_deviceShared.m_dev->open(serial))
        {
            qCritical("XTRXOutput::openDevice: cannot open BladeRF2 device");
            return false;
        }
    }

    m_deviceShared.m_channel = m_deviceAPI->getDeviceItemIndex(); // publicly allocate channel
    m_deviceShared.m_sink = this;
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void XTRXOutput::closeDevice()
{
    if (m_deviceShared.m_dev == 0) { // was never open
        return;
    }

    if (m_running) {
        stop();
    }

    if (m_XTRXOutputThread) { // stills own the thread => transfer to a buddy
        moveThreadToBuddy();
    }

    m_deviceShared.m_channel = -1; // publicly release channel
    m_deviceShared.m_sink = 0;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceShared.m_dev->close();
        delete m_deviceShared.m_dev;
        m_deviceShared.m_dev = 0;
    }
}

void XTRXOutput::init()
{
    applySettings(m_settings, QList<QString>(), true, false);
}

XTRXOutputThread *XTRXOutput::findThread()
{
    if (m_XTRXOutputThread == 0) // this does not own the thread
    {
        XTRXOutputThread *xtrxOutputThread = 0;

        // find a buddy that has allocated the thread
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

        for (; it != sinkBuddies.end(); ++it)
        {
            XTRXOutput *buddySink = ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_sink;

            if (buddySink)
            {
                xtrxOutputThread = buddySink->getThread();

                if (xtrxOutputThread) {
                    break;
                }
            }
        }

        return xtrxOutputThread;
    }
    else
    {
        return m_XTRXOutputThread; // own thread
    }
}

void XTRXOutput::moveThreadToBuddy()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

    for (; it != sinkBuddies.end(); ++it)
    {
        XTRXOutput *buddySink = ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_sink;

        if (buddySink)
        {
            buddySink->setThread(m_XTRXOutputThread);
            m_XTRXOutputThread = 0;  // zero for others
        }
    }
}

bool XTRXOutput::start()
{
    // There is a single thread per physical device (Tx side). This thread is unique and referenced by a unique
    // buddy in the group of sink buddies associated with this physical device.
    //
    // This start method is responsible for managing the thread when the streaming of a Tx channel is started
    //
    // It checks the following conditions
    //   - the thread is allocated or not (by itself or one of its buddies). If it is it grabs the thread pointer.
    //   - the requested channel is another channel (one is already streaming).
    //
    // The XTRX support library lets you work in two possible modes:
    //   - Single Output (SO) with only one channel streaming. This can be channel 0 or 1 (channels can be swapped - unlike with BladeRF2).
    //   - Multiple Output (MO) with two channels streaming using interleaved samples. It MUST be in this configuration if both channels are
    //     streaming.
    //
    // It manages the transition form SO where only one channel is running to the  Multiple Input (MO) if the both channels are requested.
    // To perform the transition it stops the thread, deletes it and creates a new one.
    // It marks the thread as needing start.
    //
    // If there is no thread allocated it means we are in SO mode and it creates a new one with the requested channel.
    // It marks the thread as needing start.
    //
    // Eventually it registers the FIFO in the thread. If the thread has to be started it enables the channels up to the number of channels
    // allocated in the thread and starts the thread.

    if (!m_deviceShared.m_dev || !m_deviceShared.m_dev->getDevice())
    {
        qDebug("XTRXOutput::start: no device object");
        return false;
    }

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    XTRXOutputThread *xtrxOutputThread = findThread();
    bool needsStart = false;

    if (xtrxOutputThread) // if thread is already allocated
    {
        qDebug("XTRXOutput::start: thread is already allocated");

        unsigned int nbOriginalChannels = xtrxOutputThread->getNbChannels();

        // if one channel is already allocated it must be the other one so we'll end up with both channels
        // thus we expand by deleting and re-creating the thread
        if (nbOriginalChannels != 0)
        {
            qDebug("XTRXOutput::start: expand channels. Re-allocate thread and take ownership");

            SampleSourceFifo **fifos = new SampleSourceFifo*[2];
            unsigned int *log2Interps = new unsigned int[2];

            for (int i = 0; i < 2; i++) // save original FIFO references and data
            {
                fifos[i] = xtrxOutputThread->getFifo(i);
                log2Interps[i] = xtrxOutputThread->getLog2Interpolation(i);
            }

            xtrxOutputThread->stopWork();
            delete xtrxOutputThread;
            xtrxOutputThread = new XTRXOutputThread(m_deviceShared.m_dev->getDevice(), 2); // MO mode (2 channels)
            m_XTRXOutputThread = xtrxOutputThread; // take ownership
            m_deviceShared.m_thread = xtrxOutputThread;

            for (int i = 0; i < 2; i++) // restore original FIFO references
            {
                xtrxOutputThread->setFifo(i, fifos[i]);
                xtrxOutputThread->setLog2Interpolation(i, log2Interps[i]);
            }

            // remove old thread address from buddies (reset in all buddies). The address being held only in the owning source.
            const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
            std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

            for (; it != sinkBuddies.end(); ++it)
            {
                ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_sink->setThread(0);
                ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_thread = 0;
            }

            // was used as temporary storage:
            delete[] fifos;
            delete[] log2Interps;

            needsStart = true;
        }
        else
        {
            qDebug("XTRXOutput::start: keep buddy thread");
        }
    }
    else // first allocation
    {
        qDebug("XTRXOutput::start: allocate thread and take ownership");
        xtrxOutputThread = new XTRXOutputThread(m_deviceShared.m_dev->getDevice(), 1, requestedChannel);
        m_XTRXOutputThread = xtrxOutputThread; // take ownership
        m_deviceShared.m_thread = xtrxOutputThread;
        needsStart = true;
    }

    xtrxOutputThread->setFifo(requestedChannel, &m_sampleSourceFifo);
    xtrxOutputThread->setLog2Interpolation(requestedChannel, m_settings.m_log2SoftInterp);

    applySettings(m_settings, QList<QString>(), true);

    if (needsStart)
    {
        qDebug("XTRXOutput::start: (re)start thread");
        xtrxOutputThread->startWork();
    }

    qDebug("XTRXOutput::start: started");
    m_running = true;

    return true;
}

void XTRXOutput::stop()
{
    // This stop method is responsible for managing the thread when the streaming of a Rx channel is stopped
    //
    // If the thread is currently managing only one channel (SO mode). The thread can be just stopped and deleted.
    // Then the channel is closed.
    //
    // If the thread is currently managing both channels (MO mode) then we are removing one channel. Thus we must
    // transition from MO to SO. This transition is handled by stopping the thread, deleting it and creating a new one
    // managing a single channel.

    if (!m_running) {
        return;
    }

    int removedChannel = m_deviceAPI->getDeviceItemIndex(); // channel to remove
    int requestedChannel = removedChannel ^ 1; // channel to keep (opposite channel)
    XTRXOutputThread *xtrxOutputThread = findThread();

    if (xtrxOutputThread == 0) { // no thread allocated
        return;
    }

    int nbOriginalChannels = xtrxOutputThread->getNbChannels();

    if (nbOriginalChannels == 1) // SO mode => just stop and delete the thread
    {
        qDebug("XTRXOutput::stop: SO mode. Just stop and delete the thread");
        xtrxOutputThread->stopWork();
        delete xtrxOutputThread;
        m_XTRXOutputThread = 0;
        m_deviceShared.m_thread = 0;

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

        for (; it != sinkBuddies.end(); ++it) {
            ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_sink->setThread(nullptr);
        }
    }
    else if (nbOriginalChannels == 2) // Reduce from MO to SO by deleting and re-creating the thread
    {
        qDebug("XTRXOutput::stop: MO mode. Reduce by deleting and re-creating the thread");
        xtrxOutputThread->stopWork();
        delete xtrxOutputThread;
        xtrxOutputThread = new XTRXOutputThread(m_deviceShared.m_dev->getDevice(), 1, requestedChannel);
        m_XTRXOutputThread = xtrxOutputThread; // take ownership
        m_deviceShared.m_thread = xtrxOutputThread;

        xtrxOutputThread->setFifo(requestedChannel, &m_sampleSourceFifo);
        xtrxOutputThread->setLog2Interpolation(requestedChannel, m_settings.m_log2SoftInterp);

        // remove old thread address from buddies (reset in all buddies). The address being held only in the owning source.
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

        for (; it != sinkBuddies.end(); ++it) {
            ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_sink->setThread(nullptr);
        }

        applySettings(m_settings, QList<QString>(), true);
        xtrxOutputThread->startWork();
    }

    m_running = false;
}

void XTRXOutput::suspendRxThread()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("XTRXOutput::suspendRxThread (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceXTRXShared *buddySharedPtr = (DeviceXTRXShared *) (*itSource)->getBuddySharedPtr();

        if (buddySharedPtr->m_thread && buddySharedPtr->m_thread->isRunning())
        {
            buddySharedPtr->m_thread->stopWork();
            buddySharedPtr->m_threadWasRunning = true;
        }
        else
        {
            buddySharedPtr->m_threadWasRunning = false;
        }
    }
}

void XTRXOutput::resumeRxThread()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("XTRXOutput::resumeRxThread (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceXTRXShared *buddySharedPtr = (DeviceXTRXShared *) (*itSource)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

QByteArray XTRXOutput::serialize() const
{
    return m_settings.serialize();
}

bool XTRXOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureXTRX* message = MsgConfigureXTRX::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureXTRX* messageToGUI = MsgConfigureXTRX::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& XTRXOutput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int XTRXOutput::getSampleRate() const
{
    double rate = m_settings.m_devSampleRate;

    if (m_deviceShared.m_dev) {
        rate = m_deviceShared.m_dev->getActualOutputRate();
    }

    return (int)((rate / (1<<m_settings.m_log2SoftInterp)));
}

uint32_t XTRXOutput::getDevSampleRate() const
{
    if (m_deviceShared.m_dev) {
        return m_deviceShared.m_dev->getActualOutputRate();
    } else {
        return m_settings.m_devSampleRate;
    }
}

uint32_t XTRXOutput::getLog2HardInterp() const
{
    if (m_deviceShared.m_dev && (m_deviceShared.m_dev->getActualOutputRate() != 0.0)) {
        return log2(m_deviceShared.m_dev->getClockGen() / m_deviceShared.m_dev->getActualOutputRate() / 4);
    } else {
        return m_settings.m_log2HardInterp;
    }
}

double XTRXOutput::getClockGen() const
{
    if (m_deviceShared.m_dev) {
        return m_deviceShared.m_dev->getClockGen();
    } else {
        return 0.0;
    }
}

quint64 XTRXOutput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency + (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);
}

void XTRXOutput::setCenterFrequency(qint64 centerFrequency)
{
    XTRXOutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency - (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);

    MsgConfigureXTRX* message = MsgConfigureXTRX::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureXTRX* messageToGUI = MsgConfigureXTRX::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

std::size_t XTRXOutput::getChannelIndex()
{
    return m_deviceShared.m_channel;
}

void XTRXOutput::getLORange(float& minF, float& maxF, float& stepF) const
{
    minF = 29e6;
    maxF = 3840e6;
    stepF = 10;
    qDebug("XTRXOutput::getLORange: min: %f max: %f step: %f",
           minF, maxF, stepF);
}

void XTRXOutput::getSRRange(float& minF, float& maxF, float& stepF) const
{
    minF = 100e3;
    maxF = 120e6;
    stepF = 10;
    qDebug("XTRXOutput::getSRRange: min: %f max: %f step: %f",
           minF, maxF, stepF);
}

void XTRXOutput::getLPRange(float& minF, float& maxF, float& stepF) const
{
    minF = 500e3;
    maxF = 130e6;
    stepF = 10;
    qDebug("XTRXOutput::getLPRange: min: %f max: %f step: %f",
           minF, maxF, stepF);
}

bool XTRXOutput::handleMessage(const Message& message)
{
    if (MsgConfigureXTRX::match(message))
    {
        MsgConfigureXTRX& conf = (MsgConfigureXTRX&) message;
        qDebug() << "XTRXOutput::handleMessage: MsgConfigureXTRX";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(),  conf.getForce())) {
            qDebug("XTRXOutput::handleMessage config error");
        }

        return true;
    }
    else if (DeviceXTRXShared::MsgReportBuddyChange::match(message))
    {
        DeviceXTRXShared::MsgReportBuddyChange& report = (DeviceXTRXShared::MsgReportBuddyChange&) message;

        if (!report.getRxElseTx())
        {
            m_settings.m_devSampleRate   = report.getDevSampleRate();
            m_settings.m_log2HardInterp   = report.getLog2HardDecimInterp();
            m_settings.m_centerFrequency = report.getCenterFrequency();
        }
        else
        {
            m_settings.m_devSampleRate = m_deviceShared.m_dev->getActualOutputRate();
            m_settings.m_log2HardInterp = getLog2HardInterp();

            qDebug() << "XTRXOutput::handleMessage: MsgReportBuddyChange:"
                     << " host_Hz: " << m_deviceShared.m_dev->getActualOutputRate()
                     << " dac_Hz: " << m_deviceShared.m_dev->getClockGen() / 4
                     << " m_log2HardInterp: " << m_settings.m_log2HardInterp;
        }

        if (m_settings.m_ncoEnable) // need to reset NCO after sample rate change
        {
            applySettings(m_settings, QList<QString>{"ncoEnable"}, false, true);
        }

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        DSPSignalNotification *notif = new DSPSignalNotification(
                    m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp),
                    m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        if (getMessageQueueToGUI())
        {
            DeviceXTRXShared::MsgReportBuddyChange *reportToGUI = DeviceXTRXShared::MsgReportBuddyChange::create(
                        m_settings.m_devSampleRate, m_settings.m_log2HardInterp, m_settings.m_centerFrequency, true);
            getMessageQueueToGUI()->push(reportToGUI);
        }

        return true;
    }
    else if (DeviceXTRXShared::MsgReportClockSourceChange::match(message))
    {
        DeviceXTRXShared::MsgReportClockSourceChange& report = (DeviceXTRXShared::MsgReportClockSourceChange&) message;

        m_settings.m_extClock     = report.getExtClock();
        m_settings.m_extClockFreq = report.getExtClockFeq();

        if (getMessageQueueToGUI())
        {
            DeviceXTRXShared::MsgReportClockSourceChange *reportToGUI = DeviceXTRXShared::MsgReportClockSourceChange::create(
                        m_settings.m_extClock, m_settings.m_extClockFreq);
            getMessageQueueToGUI()->push(reportToGUI);
        }

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
        if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
        {
            uint64_t fifolevel = 0;

            if (m_deviceShared.m_dev && m_deviceShared.m_dev->getDevice()) {
                xtrx_val_get(m_deviceShared.m_dev->getDevice(), XTRX_TX, XTRX_CH_AB, XTRX_PERF_LLFIFO, &fifolevel);
            }

            MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        true,
                        true,
                        fifolevel,
                        65536);

            if (m_deviceAPI->getSamplingDeviceGUIMessageQueue()) {
                m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else if (MsgGetDeviceInfo::match(message))
    {
        double board_temp = 0.0;
        bool gps_locked = false;

        if (!m_deviceShared.m_dev->getDevice() || ((board_temp = m_deviceShared.get_board_temperature() / 256.0) == 0.0)) {
            qDebug("XTRXOutput::handleMessage: MsgGetDeviceInfo: cannot get board temperature");
        }

        if (!m_deviceShared.m_dev->getDevice()) {
            qDebug("XTRXOutput::handleMessage: MsgGetDeviceInfo: cannot get GPS lock status");
        } else {
            gps_locked = m_deviceShared.get_gps_status();
        }

        // send to oneself
        if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
        {
            DeviceXTRXShared::MsgReportDeviceInfo *report = DeviceXTRXShared::MsgReportDeviceInfo::create(board_temp, gps_locked);
            m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            if ((*itSink)->getSamplingDeviceGUIMessageQueue())
            {
                DeviceXTRXShared::MsgReportDeviceInfo *report = DeviceXTRXShared::MsgReportDeviceInfo::create(board_temp, gps_locked);
                (*itSink)->getSamplingDeviceGUIMessageQueue()->push(report);
            }
        }

        // send to source buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            if ((*itSource)->getSamplingDeviceGUIMessageQueue())
            {
                DeviceXTRXShared::MsgReportDeviceInfo *report = DeviceXTRXShared::MsgReportDeviceInfo::create(board_temp, gps_locked);
                (*itSource)->getSamplingDeviceGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "XTRXOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine())
            {
                m_deviceAPI->startDeviceEngine();
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine();
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool XTRXOutput::applySettings(const XTRXOutputSettings& settings, const QList<QString>& settingsKeys, bool force, bool forceNCOFrequency)
{
    qDebug() << "XTRXOutput::applySettings: force:" << force << " forceNCOFrequency:" << forceNCOFrequency << settings.getDebugString(settingsKeys, force);
    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    XTRXOutputThread *outputThread = findThread();

    bool forwardChangeOwnDSP = false;
    bool forwardChangeTxDSP  = false;
    bool forwardChangeAllDSP = false;
    bool forwardClockSource  = false;
    bool doLPCalibration = false;
    bool doChangeSampleRate = false;
    bool doChangeFreq = false;

    // apply settings

    if (settingsKeys.contains("pwrmode") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_val_set(m_deviceShared.m_dev->getDevice(),
                    XTRX_TRX,
                    m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
                    XTRX_LMS7_PWR_MODE,
                    settings.m_pwrmode) < 0) {
                qCritical("XTRXOutput::applySettings: could not set power mode %d", settings.m_pwrmode);
            }
        }
    }

    if (settingsKeys.contains("extClock")
       || (settings.m_extClock && settingsKeys.contains("extClockFreq")) || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            xtrx_set_ref_clk(m_deviceShared.m_dev->getDevice(),
                             (settings.m_extClock) ? settings.m_extClockFreq : 0,
                             (settings.m_extClock) ? XTRX_CLKSRC_EXT : XTRX_CLKSRC_INT);
            {
                forwardClockSource = true;
                doChangeSampleRate = true;
                doChangeFreq = true;
                qDebug("XTRXOutput::applySettings: clock set to %s (Ext: %d Hz)",
                       settings.m_extClock ? "external" : "internal",
                       settings.m_extClockFreq);
            }
        }
    }

    if (settingsKeys.contains("devSampleRate")
       || settingsKeys.contains("log2HardInterp") || force)
    {
        forwardChangeAllDSP = true; //m_settings.m_devSampleRate != settings.m_devSampleRate;

        if (m_deviceShared.m_dev->getDevice()) {
            doChangeSampleRate = true;
        }
    }

    if (settingsKeys.contains("gain") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_set_gain(m_deviceShared.m_dev->getDevice(),
                    m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
                    XTRX_TX_PAD_GAIN,
                    settings.m_gain,
                    0) < 0) {
                qDebug("XTRXOutput::applySettings: xtrx_set_gain(PAD) failed");
            } else {
                qDebug() << "XTRXOutput::applySettings: Gain (PAD) set to " << settings.m_gain;
            }
        }
    }

    if (settingsKeys.contains("lpfBW") || force)
    {
        if (m_deviceShared.m_dev->getDevice()) {
            doLPCalibration = true;
        }
    }

#if 0
    if ((m_settings.m_lpfFIRBW != settings.m_lpfFIRBW) ||
            (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (LMS_SetGFIRLPF(m_deviceShared.m_deviceParams->getDevice(),
                               LMS_CH_RX,
                               m_deviceShared.m_channel,
                               settings.m_lpfFIREnable,
                               settings.m_lpfFIRBW) < 0)
            {
                qCritical("XTRXOutput::applySettings: could %s and set LPF FIR to %f Hz",
                          settings.m_lpfFIREnable ? "enable" : "disable",
                          settings.m_lpfFIRBW);
            }
            else
            {
                //doCalibration = true;
                qDebug("XTRXOutput::applySettings: %sd and set LPF FIR to %f Hz",
                       settings.m_lpfFIREnable ? "enable" : "disable",
                       settings.m_lpfFIRBW);
            }
        }
    }
#endif

    if (settingsKeys.contains("log2SoftInterp") || force)
    {
        forwardChangeOwnDSP = true;

        if (outputThread)
        {
            outputThread->setLog2Interpolation(requestedChannel, settings.m_log2SoftInterp);
            qDebug() << "XTRXOutput::applySettings: set soft interpolation to " << (1<<settings.m_log2SoftInterp);
        }
    }

    if (settingsKeys.contains("devSampleRate")
     || settingsKeys.contains("log2SoftInterp") || force)
    {
        unsigned int fifoRate = std::max(
            (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftInterp),
            DeviceXTRXShared::m_sampleFifoMinRate);
        m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(fifoRate));
    }

    if (settingsKeys.contains("antennaPath") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_set_antenna(m_deviceShared.m_dev->getDevice(), settings.m_antennaPath) < 0) {
                qCritical("XTRXOutput::applySettings: could not set antenna path to %d", (int) settings.m_antennaPath);
            } else {
                qDebug("XTRXOutput::applySettings: set antenna path to %d", (int) settings.m_antennaPath);
            }
        }
    }

    if (settingsKeys.contains("centerFrequency") || force) {
        doChangeFreq = true;
    }

    if (settingsKeys.contains("ncoFrequency")
       || settingsKeys.contains("ncoEnable") || force)
    {
        forceNCOFrequency = true;
    }

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (doChangeSampleRate && (settings.m_devSampleRate != 0))
    {
        // XTRXOutputThread *txThread = findThread();

        // if (txThread && txThread->isRunning())
        // {
        //     txThread->stopWork();
        //     txThreadWasRunning = true;
        // }

        // suspendRxThread();

        double master = (m_settings.m_log2HardInterp == 0) ? 0 : (m_settings.m_devSampleRate * 4 * (1 << m_settings.m_log2HardInterp));

        int res = m_deviceShared.m_dev->setSamplerate(
            m_settings.m_devSampleRate,
            master,
            true
        );

        doChangeFreq = true;
        forceNCOFrequency = true;
        forwardChangeAllDSP = true;
        m_settings.m_devSampleRate = m_deviceShared.m_dev->getActualOutputRate();
        m_settings.m_log2HardInterp = getLog2HardInterp();

        qDebug("XTRXOutput::applySettings: sample rate set %s to %f with hard interpolation of %d",
            (res < 0) ? "changed" : "unchanged",
            m_settings.m_devSampleRate,
            m_settings.m_log2HardInterp);

        // resumeRxThread();

        // if (txThreadWasRunning) {
        //     txThread->startWork();
        // }
    }

    if (doLPCalibration)
    {
        if (xtrx_tune_tx_bandwidth(m_deviceShared.m_dev->getDevice(),
                m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
                m_settings.m_lpfBW,
                0) < 0) {
            qCritical("XTRXOutput::applySettings: could not set LPF to %f Hz", m_settings.m_lpfBW);
        } else {
            qDebug("XTRXOutput::applySettings: LPF set to %f Hz", m_settings.m_lpfBW);
        }
    }

    if (doChangeFreq)
    {
        forwardChangeTxDSP = true;

        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_tune(m_deviceShared.m_dev->getDevice(),
                    XTRX_TUNE_TX_FDD,
                    m_settings.m_centerFrequency,
                    0) < 0) {
                qCritical("XTRXOutput::applySettings: could not set frequency to %lu", m_settings.m_centerFrequency);
            } else {
                //doCalibration = true;
                qDebug("XTRXOutput::applySettings: frequency set to %lu", m_settings.m_centerFrequency);
            }
        }
    }

    if (forceNCOFrequency)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_tune_ex(m_deviceShared.m_dev->getDevice(),
                    XTRX_TUNE_BB_TX,
                    m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
                    (m_settings.m_ncoEnable) ? m_settings.m_ncoFrequency : 0,
                    NULL) < 0)
            {
                qCritical("XTRXOutput::applySettings: could not %s and set NCO to %d Hz",
                          m_settings.m_ncoEnable ? "enable" : "disable",
                          m_settings.m_ncoFrequency);
            }
            else
            {
                forwardChangeOwnDSP = true;
                qDebug("XTRXOutput::applySettings: %sd and set NCO to %d Hz",
                       m_settings.m_ncoEnable ? "enable" : "disable",
                       m_settings.m_ncoFrequency);
            }
        }
    }

    // forward changes to buddies or oneself

    if (forwardChangeAllDSP)
    {
        qDebug("XTRXOutput::applySettings: forward change to all buddies");

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(getSampleRate(), m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        if (getMessageQueueToGUI())
        {
            MsgReportClockGenChange *report = MsgReportClockGenChange::create();
            getMessageQueueToGUI()->push(report);
        }

        // send to source buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceXTRXShared::MsgReportBuddyChange *report = DeviceXTRXShared::MsgReportBuddyChange::create(
                    getDevSampleRate(), getLog2HardInterp(), m_settings.m_centerFrequency, true);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceXTRXShared::MsgReportBuddyChange *report = DeviceXTRXShared::MsgReportBuddyChange::create(
                    getDevSampleRate(), getLog2HardInterp(), m_settings.m_centerFrequency, true);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeTxDSP)
    {
        qDebug("XTRXOutput::applySettings: forward change to Tx buddies");

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(getSampleRate(), m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        if (getMessageQueueToGUI())
        {
            MsgReportClockGenChange *report = MsgReportClockGenChange::create();
            getMessageQueueToGUI()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceXTRXShared::MsgReportBuddyChange *report = DeviceXTRXShared::MsgReportBuddyChange::create(
                    getDevSampleRate(), getLog2HardInterp(), m_settings.m_centerFrequency, true);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeOwnDSP)
    {
        qDebug("XTRXOutput::applySettings: forward change to self only");

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        DSPSignalNotification *notif = new DSPSignalNotification(getSampleRate(), m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        if (getMessageQueueToGUI())
        {
            MsgReportClockGenChange *report = MsgReportClockGenChange::create();
            getMessageQueueToGUI()->push(report);
        }
    }

    if (forwardClockSource)
    {
        // send to source buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceXTRXShared::MsgReportClockSourceChange *report = DeviceXTRXShared::MsgReportClockSourceChange::create(
                        m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceXTRXShared::MsgReportClockSourceChange *report = DeviceXTRXShared::MsgReportClockSourceChange::create(
                        m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }

    return true;
}

int XTRXOutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setXtrxOutputSettings(new SWGSDRangel::SWGXtrxOutputSettings());
    response.getXtrxOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int XTRXOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    XTRXOutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureXTRX *msg = MsgConfigureXTRX::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureXTRX *msgToGUI = MsgConfigureXTRX::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void XTRXOutput::webapiUpdateDeviceSettings(
        XTRXOutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getXtrxOutputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getXtrxOutputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("log2HardInterp")) {
        settings.m_log2HardInterp = response.getXtrxOutputSettings()->getLog2HardInterp();
    }
    if (deviceSettingsKeys.contains("log2SoftInterp")) {
        settings.m_log2SoftInterp = response.getXtrxOutputSettings()->getLog2SoftInterp();
    }
    if (deviceSettingsKeys.contains("lpfBW")) {
        settings.m_lpfBW = response.getXtrxOutputSettings()->getLpfBw();
    }
    if (deviceSettingsKeys.contains("gain")) {
        settings.m_gain = response.getXtrxOutputSettings()->getGain();
    }
    if (deviceSettingsKeys.contains("ncoEnable")) {
        settings.m_ncoEnable = response.getXtrxOutputSettings()->getNcoEnable() != 0;
    }
    if (deviceSettingsKeys.contains("ncoFrequency")) {
        settings.m_ncoFrequency = response.getXtrxOutputSettings()->getNcoFrequency();
    }
    if (deviceSettingsKeys.contains("antennaPath")) {
        settings.m_antennaPath = (xtrx_antenna_t) response.getXtrxOutputSettings()->getAntennaPath();
    }
    if (deviceSettingsKeys.contains("extClock")) {
        settings.m_extClock = response.getXtrxOutputSettings()->getExtClock() != 0;
    }
    if (deviceSettingsKeys.contains("extClockFreq")) {
        settings.m_extClockFreq = response.getXtrxOutputSettings()->getExtClockFreq();
    }
    if (deviceSettingsKeys.contains("pwrmode")) {
        settings.m_pwrmode = response.getXtrxOutputSettings()->getPwrmode();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getXtrxOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getXtrxOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getXtrxOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getXtrxOutputSettings()->getReverseApiDeviceIndex();
    }
}

void XTRXOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const XTRXOutputSettings& settings)
{
    response.getXtrxOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getXtrxOutputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getXtrxOutputSettings()->setLog2HardInterp(settings.m_log2HardInterp);
    response.getXtrxOutputSettings()->setLog2SoftInterp(settings.m_log2SoftInterp);
    response.getXtrxOutputSettings()->setLpfBw(settings.m_lpfBW);
    response.getXtrxOutputSettings()->setGain(settings.m_gain);
    response.getXtrxOutputSettings()->setNcoEnable(settings.m_ncoEnable ? 1 : 0);
    response.getXtrxOutputSettings()->setNcoFrequency(settings.m_ncoFrequency);
    response.getXtrxOutputSettings()->setAntennaPath((int) settings.m_antennaPath);
    response.getXtrxOutputSettings()->setExtClock(settings.m_extClock ? 1 : 0);
    response.getXtrxOutputSettings()->setExtClockFreq(settings.m_extClockFreq);
    response.getXtrxOutputSettings()->setPwrmode(settings.m_pwrmode);
    response.getXtrxOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getXtrxOutputSettings()->getReverseApiAddress()) {
        *response.getXtrxOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getXtrxOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getXtrxOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getXtrxOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int XTRXOutput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setXtrxOutputReport(new SWGSDRangel::SWGXtrxOutputReport());
    response.getXtrxOutputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int XTRXOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int XTRXOutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
}

void XTRXOutput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    int ret;
    bool success = false;
    double temp = 0.0;
    bool gpsStatus = false;
    uint64_t fifolevel = 0;
    uint32_t fifosize = 1<<16;

    if (m_deviceShared.m_dev && m_deviceShared.m_dev->getDevice())
    {
        ret = xtrx_val_get(m_deviceShared.m_dev->getDevice(),
                     XTRX_TX, XTRX_CH_AB, XTRX_PERF_LLFIFO, &fifolevel);
        success = (ret >= 0);
        temp = m_deviceShared.get_board_temperature() / 256.0;
        gpsStatus = m_deviceShared.get_gps_status();
    }

    response.getXtrxOutputReport()->setSuccess(success ? 1 : 0);
    response.getXtrxOutputReport()->setFifoSize(fifosize);
    response.getXtrxOutputReport()->setFifoFill(fifolevel);
    response.getXtrxOutputReport()->setTemperature(temp);
    response.getXtrxOutputReport()->setGpsLock(gpsStatus ? 1 : 0);
}

void XTRXOutput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const XTRXOutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // Single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("XTRX"));
    swgDeviceSettings->setXtrxOutputSettings(new SWGSDRangel::SWGXtrxOutputSettings());
    SWGSDRangel::SWGXtrxOutputSettings *swgXtrxOutputSettings = swgDeviceSettings->getXtrxOutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgXtrxOutputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgXtrxOutputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("log2HardInterp") || force) {
        swgXtrxOutputSettings->setLog2HardInterp(settings.m_log2HardInterp);
    }
    if (deviceSettingsKeys.contains("log2SoftInterp") || force) {
        swgXtrxOutputSettings->setLog2SoftInterp(settings.m_log2SoftInterp);
    }
    if (deviceSettingsKeys.contains("ncoEnable") || force) {
        swgXtrxOutputSettings->setNcoEnable(settings.m_ncoEnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoFrequency") || force) {
        swgXtrxOutputSettings->setNcoFrequency(settings.m_ncoFrequency);
    }
    if (deviceSettingsKeys.contains("lpfBW") || force) {
        swgXtrxOutputSettings->setLpfBw(settings.m_lpfBW);
    }
    if (deviceSettingsKeys.contains("antennaPath") || force) {
        swgXtrxOutputSettings->setAntennaPath((int) settings.m_antennaPath);
    }
    if (deviceSettingsKeys.contains("gain") || force) {
        swgXtrxOutputSettings->setGain(settings.m_gain);
    }
    if (deviceSettingsKeys.contains("extClock") || force) {
        swgXtrxOutputSettings->setExtClock(settings.m_extClock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("extClockFreq") || force) {
        swgXtrxOutputSettings->setExtClockFreq(settings.m_extClockFreq);
    }
    if (deviceSettingsKeys.contains("pwrmode") || force) {
        swgXtrxOutputSettings->setPwrmode(settings.m_pwrmode);
    }

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void XTRXOutput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // Single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("XTRX"));

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply;

    if (start) {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "POST", buffer);
    } else {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "DELETE", buffer);
    }

    buffer->setParent(reply);
    delete swgDeviceSettings;
}

void XTRXOutput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "XTRXOutput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("XTRXOutput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
