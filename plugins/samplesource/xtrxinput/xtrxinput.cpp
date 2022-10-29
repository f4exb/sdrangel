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
#include "SWGXtrxInputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGXtrxInputReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "xtrxinput.h"
#include "xtrxinputthread.h"
#include "xtrx/devicextrxparam.h"
#include "xtrx/devicextrxshared.h"
#include "xtrx/devicextrx.h"

MESSAGE_CLASS_DEFINITION(XTRXInput::MsgConfigureXTRX, Message)
MESSAGE_CLASS_DEFINITION(XTRXInput::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXInput::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXInput::MsgReportClockGenChange, Message)
MESSAGE_CLASS_DEFINITION(XTRXInput::MsgReportStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXInput::MsgStartStop, Message)

XTRXInput::XTRXInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_XTRXInputThread(nullptr),
    m_deviceDescription("XTRXInput"),
    m_running(false)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    openDevice();

    m_deviceAPI->setNbSourceStreams(1);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &XTRXInput::networkManagerFinished
    );
}

XTRXInput::~XTRXInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &XTRXInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void XTRXInput::destroy()
{
    delete this;
}

bool XTRXInput::openDevice()
{
    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("XTRXInput::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("XTRXInput::openDevice: allocated SampleFifo");
    }

    // look for Rx buddies and get reference to the device object
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        qDebug("XTRXInput::openDevice: look in Rx buddies");

        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceXTRXShared *deviceXTRXShared = (DeviceXTRXShared*) sourceBuddy->getBuddySharedPtr();

        if (deviceXTRXShared == 0)
        {
            qCritical("XTRXInput::openDevice: the source buddy shared pointer is null");
            return false;
        }

        DeviceXTRX *device = deviceXTRXShared->m_dev;

        if (device == 0)
        {
            qCritical("XTRXInput::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_dev = device;
    }
    // look for Tx buddies and get reference to the device object
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("XTRXInput::openDevice: look in Tx buddies");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceXTRXShared *deviceXTRXShared = (DeviceXTRXShared*) sinkBuddy->getBuddySharedPtr();

        if (deviceXTRXShared == 0)
        {
            qCritical("XTRXInput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        DeviceXTRX *device = deviceXTRXShared->m_dev;

        if (device == 0)
        {
            qCritical("XTRXInput::openDevice: cannot get device pointer from Tx buddy");
            return false;
        }

        m_deviceShared.m_dev = device;
    }
    // There are no buddies then create the first BladeRF2 device
    else
    {
        qDebug("XTRXInput::openDevice: open device here");

        m_deviceShared.m_dev = new DeviceXTRX();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));

        if (!m_deviceShared.m_dev->open(serial))
        {
            qCritical("XTRXInput::openDevice: cannot open BladeRF2 device");
            return false;
        }
    }

    m_deviceShared.m_channel = m_deviceAPI->getDeviceItemIndex(); // publicly allocate channel
    m_deviceShared.m_source = this;
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void XTRXInput::closeDevice()
{
    if (m_deviceShared.m_dev == 0) { // was never open
        return;
    }

    if (m_running) {
        stop();
    }

    if (m_XTRXInputThread) { // stills own the thread => transfer to a buddy
        moveThreadToBuddy();
    }

    m_deviceShared.m_channel = -1; // publicly release channel
    m_deviceShared.m_source = 0;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceShared.m_dev->close();
        delete m_deviceShared.m_dev;
        m_deviceShared.m_dev = 0;
    }
}

void XTRXInput::init()
{
    applySettings(m_settings, QList<QString>(), true, false);
}

XTRXInputThread *XTRXInput::findThread()
{
    if (!m_XTRXInputThread) // this does not own the thread
    {
        XTRXInputThread *xtrxInputThread = nullptr;

        // find a buddy that has allocated the thread
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it)
        {
            XTRXInput *buddySource = ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_source;

            if (buddySource)
            {
                xtrxInputThread = buddySource->getThread();

                if (xtrxInputThread) {
                    break;
                }
            }
        }

        return xtrxInputThread;
    }
    else
    {
        return m_XTRXInputThread; // own thread
    }
}

void XTRXInput::moveThreadToBuddy()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator it = sourceBuddies.begin();

    for (; it != sourceBuddies.end(); ++it)
    {
        XTRXInput *buddySource = ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_source;

        if (buddySource)
        {
            buddySource->setThread(m_XTRXInputThread);
            m_XTRXInputThread = nullptr;  // zero for others
        }
    }
}

bool XTRXInput::start()
{
    // There is a single thread per physical device (Rx side). This thread is unique and referenced by a unique
    // buddy in the group of source buddies associated with this physical device.
    //
    // This start method is responsible for managing the thread when the streaming of a Rx channel is started
    //
    // It checks the following conditions
    //   - the thread is allocated or not (by itself or one of its buddies). If it is it grabs the thread pointer.
    //   - the requested channel is another channel (one is already streaming).
    //
    // The XTRX support library lets you work in two possible modes:
    //   - Single Input (SI) with only one channel streaming. This can be channel 0 or 1 (channels can be swapped - unlike with BladeRF2).
    //   - Multiple Input (MI) with two channels streaming using interleaved samples. It MUST be in this configuration if both channels are
    //     streaming.
    //
    // It manages the transition form SI where only one channel is running to the  Multiple Input (MI) if the both channels are requested.
    // To perform the transition it stops the thread, deletes it and creates a new one.
    // It marks the thread as needing start.
    //
    // If there is no thread allocated it means we are in SI mode and it creates a new one with the requested channel.
    // It marks the thread as needing start.
    //
    // Eventually it registers the FIFO in the thread. If the thread has to be started it enables the channels up to the number of channels
    // allocated in the thread and starts the thread.

    if (!m_deviceShared.m_dev || !m_deviceShared.m_dev->getDevice())
    {
        qDebug("XTRXInput::start: no device object");
        return false;
    }

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    XTRXInputThread *xtrxInputThread = findThread();
    bool needsStart = false;

    if (xtrxInputThread) // if thread is already allocated
    {
        qDebug("XTRXInput::start: thread is already allocated");

        unsigned int nbOriginalChannels = xtrxInputThread->getNbChannels();

        // if one channel is already allocated it must be the other one so we'll end up with both channels
        // thus we expand by deleting and re-creating the thread
        if (nbOriginalChannels != 0)
        {
            qDebug("XTRXInput::start: expand channels. Re-allocate thread and take ownership");

            SampleSinkFifo **fifos = new SampleSinkFifo*[2];
            unsigned int *log2Decims = new unsigned int[2];

            for (int i = 0; i < 2; i++) // save original FIFO references and data
            {
                fifos[i] = xtrxInputThread->getFifo(i);
                log2Decims[i] = xtrxInputThread->getLog2Decimation(i);
            }

            xtrxInputThread->stopWork();
            delete xtrxInputThread;
            xtrxInputThread = new XTRXInputThread(m_deviceShared.m_dev->getDevice(), 2); // MI mode (2 channels)
            xtrxInputThread->setIQOrder(m_settings.m_iqOrder);
            m_deviceShared.m_thread = xtrxInputThread;
            m_XTRXInputThread = xtrxInputThread; // take ownership

            for (int i = 0; i < 2; i++) // restore original FIFO references
            {
                xtrxInputThread->setFifo(i, fifos[i]);
                xtrxInputThread->setLog2Decimation(i, log2Decims[i]);
            }

            // remove old thread address from buddies (reset in all buddies). The address being held only in the owning source.
            const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
            std::vector<DeviceAPI*>::const_iterator it = sourceBuddies.begin();

            for (; it != sourceBuddies.end(); ++it)
            {
                ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
                ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_thread = 0;
            }

            // was used as temporary storage:
            delete[] fifos;
            delete[] log2Decims;

            needsStart = true;
        }
        else
        {
            qDebug("XTRXInput::start: keep buddy thread");
        }
    }
    else // first allocation
    {
        qDebug("XTRXInput::start: allocate thread and take ownership");
        xtrxInputThread = new XTRXInputThread(m_deviceShared.m_dev->getDevice(), 1, requestedChannel);
        m_XTRXInputThread = xtrxInputThread; // take ownership
        m_deviceShared.m_thread = xtrxInputThread;
        needsStart = true;
    }

    xtrxInputThread->setFifo(requestedChannel, &m_sampleFifo);
    xtrxInputThread->setLog2Decimation(requestedChannel, m_settings.m_log2SoftDecim);

    applySettings(m_settings, QList<QString>(), true);

    if (needsStart)
    {
        qDebug("XTRXInput::start: (re)start thread");
        xtrxInputThread->startWork();
    }

    qDebug("XTRXInput::start: started");
    m_running = true;

    return true;
}

void XTRXInput::stop()
{
    // This stop method is responsible for managing the thread when the streaming of a Rx channel is stopped
    //
    // If the thread is currently managing only one channel (SI mode). The thread can be just stopped and deleted.
    // Then the channel is closed.
    //
    // If the thread is currently managing both channels (MI mode) then we are removing one channel. Thus we must
    // transition from MI to SI. This transition is handled by stopping the thread, deleting it and creating a new one
    // managing a single channel.

    if (!m_running) {
        return;
    }

    int removedChannel = m_deviceAPI->getDeviceItemIndex(); // channel to remove
    int requestedChannel = removedChannel ^ 1; // channel to keep (opposite channel)
    XTRXInputThread *xtrxInputThread = findThread();

    if (xtrxInputThread == 0) { // no thread allocated
        return;
    }

    int nbOriginalChannels = xtrxInputThread->getNbChannels();

    if (nbOriginalChannels == 1) // SI mode => just stop and delete the thread
    {
        qDebug("XTRXInput::stop: SI mode. Just stop and delete the thread");
        xtrxInputThread->stopWork();
        delete xtrxInputThread;
        m_XTRXInputThread = nullptr;
        m_deviceShared.m_thread = nullptr;

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it) {
            ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_source->setThread(nullptr);
        }
    }
    else if (nbOriginalChannels == 2) // Reduce from MI to SI by deleting and re-creating the thread
    {
        qDebug("XTRXInput::stop: MI mode. Reduce by deleting and re-creating the thread");
        xtrxInputThread->stopWork();
        delete xtrxInputThread;
        xtrxInputThread = new XTRXInputThread(m_deviceShared.m_dev->getDevice(), 1, requestedChannel);
        m_XTRXInputThread = xtrxInputThread; // take ownership
        m_deviceShared.m_thread = xtrxInputThread;

        xtrxInputThread->setIQOrder(m_settings.m_iqOrder);
        xtrxInputThread->setFifo(requestedChannel, &m_sampleFifo);
        xtrxInputThread->setLog2Decimation(requestedChannel, m_settings.m_log2SoftDecim);

        // remove old thread address from buddies (reset in all buddies). The address being held only in the owning source.
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it) {
            ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_source->setThread(nullptr);
        }

        applySettings(m_settings, QList<QString>(), true);
        xtrxInputThread->startWork();
    }

    m_running = false;
}

void XTRXInput::suspendTxThread()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("XTRXInput::suspendTxThread (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceXTRXShared *buddySharedPtr = (DeviceXTRXShared *) (*itSink)->getBuddySharedPtr();

        if ((buddySharedPtr->m_thread) && buddySharedPtr->m_thread->isRunning())
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

void XTRXInput::resumeTxThread()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("XTRXInput::resumeTxThread (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceXTRXShared *buddySharedPtr = (DeviceXTRXShared *) (*itSink)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

QByteArray XTRXInput::serialize() const
{
    return m_settings.serialize();
}

bool XTRXInput::deserialize(const QByteArray& data)
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

const QString& XTRXInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int XTRXInput::getSampleRate() const
{
    double rate = m_settings.m_devSampleRate;

    if (m_deviceShared.m_dev) {
        rate = m_deviceShared.m_dev->getActualInputRate();
    }

    return (int)((rate / (1<<m_settings.m_log2SoftDecim)));
}

uint32_t XTRXInput::getDevSampleRate() const
{
    if (m_deviceShared.m_dev) {
        return m_deviceShared.m_dev->getActualInputRate();
    } else {
        return m_settings.m_devSampleRate;
    }
}

uint32_t XTRXInput::getLog2HardDecim() const
{
    if (m_deviceShared.m_dev && (m_deviceShared.m_dev->getActualInputRate() != 0.0)) {
        return log2(m_deviceShared.m_dev->getClockGen() / m_deviceShared.m_dev->getActualInputRate() / 4);
    } else {
        return m_settings.m_log2HardDecim;
    }
}

double XTRXInput::getClockGen() const
{
    if (m_deviceShared.m_dev) {
        return m_deviceShared.m_dev->getClockGen();
    } else {
        return 0.0;
    }
}

quint64 XTRXInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency + (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);
}

void XTRXInput::setCenterFrequency(qint64 centerFrequency)
{
    XTRXInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency - (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);

    MsgConfigureXTRX* message = MsgConfigureXTRX::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureXTRX* messageToGUI = MsgConfigureXTRX::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

std::size_t XTRXInput::getChannelIndex()
{
    return m_deviceShared.m_channel;
}

void XTRXInput::getLORange(float& minF, float& maxF, float& stepF) const
{
    minF = 29e6;
    maxF = 3840e6;
    stepF = 10;
    qDebug("XTRXInput::getLORange: min: %f max: %f step: %f",
           minF, maxF, stepF);
}

void XTRXInput::getSRRange(float& minF, float& maxF, float& stepF) const
{
    minF = 100e3;
    maxF = 120e6;
    stepF = 10;
    qDebug("XTRXInput::getSRRange: min: %f max: %f step: %f",
           minF, maxF, stepF);
}

void XTRXInput::getLPRange(float& minF, float& maxF, float& stepF) const
{
    minF = 500e3;
    maxF = 130e6;
    stepF = 10;
    qDebug("XTRXInput::getLPRange: min: %f max: %f step: %f",
           minF, maxF, stepF);
}

bool XTRXInput::handleMessage(const Message& message)
{
    if (MsgConfigureXTRX::match(message))
    {
        MsgConfigureXTRX& conf = (MsgConfigureXTRX&) message;
        qDebug() << "XTRXInput::handleMessage: MsgConfigureXTRX";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce()))
        {
            qDebug("XTRXInput::handleMessage config error");
        }

        return true;
    }
    else if (DeviceXTRXShared::MsgReportBuddyChange::match(message))
    {
        DeviceXTRXShared::MsgReportBuddyChange& report = (DeviceXTRXShared::MsgReportBuddyChange&) message;

        if (report.getRxElseTx())
        {
            m_settings.m_devSampleRate   = report.getDevSampleRate();
            m_settings.m_log2HardDecim   = report.getLog2HardDecimInterp();
            m_settings.m_centerFrequency = report.getCenterFrequency();
        }
        else
        {
            m_settings.m_devSampleRate = m_deviceShared.m_dev->getActualInputRate();
            m_settings.m_log2HardDecim = getLog2HardDecim();
        }

        qDebug() << "XTRXInput::handleMessage: MsgReportBuddyChange:"
                    << " host_Hz: " << m_deviceShared.m_dev->getActualInputRate()
                    << " adc_Hz: " << m_deviceShared.m_dev->getClockGen() / 4
                    << " m_log2HardDecim: " << m_settings.m_log2HardDecim;

        if (m_settings.m_ncoEnable) { // need to reset NCO after sample rate change
            applySettings(m_settings, QList<QString>{"ncoEnable"}, false, true);
        }

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        DSPSignalNotification *notif = new DSPSignalNotification(
                    m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim),
                    m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        if (getMessageQueueToGUI())
        {
            DeviceXTRXShared::MsgReportBuddyChange *reportToGUI = DeviceXTRXShared::MsgReportBuddyChange::create(
                        m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
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
                xtrx_val_get(m_deviceShared.m_dev->getDevice(), XTRX_RX, XTRX_CH_AB, XTRX_PERF_LLFIFO, &fifolevel);
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
            qDebug("XTRXInput::handleMessage: MsgGetDeviceInfo: cannot get board temperature");
        }

        if (!m_deviceShared.m_dev->getDevice()) {
            qDebug("XTRXInput::handleMessage: MsgGetDeviceInfo: cannot get GPS lock status");
        } else {
            gps_locked = m_deviceShared.get_gps_status();
        }

        // send to oneself
        if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
        {
            DeviceXTRXShared::MsgReportDeviceInfo *report = DeviceXTRXShared::MsgReportDeviceInfo::create(board_temp, gps_locked);
            m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
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

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "XTRXInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

static double tia_to_db(unsigned idx)
{
    switch (idx) {
    case 1: return 12;
    case 2: return 9;
    default: return 0;
    }
}

void XTRXInput::apply_gain_auto(uint32_t gain)
{
    uint32_t lna, tia, pga;

    DeviceXTRX::getAutoGains(gain, lna, tia, pga);

    apply_gain_lna(lna);
    apply_gain_tia(tia_to_db(tia));
    apply_gain_pga(pga);
}

void XTRXInput::apply_gain_lna(double gain)
{
    if (xtrx_set_gain(m_deviceShared.m_dev->getDevice(),
            m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
            XTRX_RX_LNA_GAIN,
            gain,
            0) < 0) {
        qDebug("XTRXInput::apply_gain_lna: xtrx_set_gain(LNA) failed");
    } else {
        qDebug() << "XTRXInput::apply_gain_lna: Gain (LNA) set to " << gain;
    }
}

void XTRXInput::apply_gain_tia(double gain)
{
    if (xtrx_set_gain(m_deviceShared.m_dev->getDevice(),
            m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
            XTRX_RX_TIA_GAIN,
            gain,
            0) < 0) {
        qDebug("XTRXInput::apply_gain_tia: xtrx_set_gain(TIA) failed");
    } else {
        qDebug() << "XTRXInput::apply_gain_tia: Gain (TIA) set to " << gain;
    }
}

void XTRXInput::apply_gain_pga(double gain)
{
    if (xtrx_set_gain(m_deviceShared.m_dev->getDevice(),
            m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
            XTRX_RX_PGA_GAIN,
            gain,
            0) < 0)
    {
        qDebug("XTRXInput::apply_gain_pga: xtrx_set_gain(PGA) failed");
    }
    else
    {
        qDebug() << "XTRXInput::apply_gain_pga: Gain (PGA) set to " << gain;
    }
}

bool XTRXInput::applySettings(const XTRXInputSettings& settings, const QList<QString>& settingsKeys, bool force, bool forceNCOFrequency)
{
    qDebug() << "XTRXInput::applySettings: force:" << force << " forceNCOFrequency:" << forceNCOFrequency << settings.getDebugString(settingsKeys, force);
    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    XTRXInputThread *inputThread = findThread();

    bool forwardChangeOwnDSP = false;
    bool forwardChangeRxDSP  = false;
    bool forwardChangeAllDSP = false;
    bool forwardClockSource  = false;
    bool doLPCalibration = false;
    bool doChangeSampleRate = false;
    bool doChangeFreq = false;

    bool doGainAuto = false;
    bool doGainLna = false;
    bool doGainTia = false;
    bool doGainPga = false;

    if (settingsKeys.contains("dcBlock") || settingsKeys.contains("iqCorrection") || force) {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if (settingsKeys.contains("pwrmode") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_val_set(m_deviceShared.m_dev->getDevice(),
                    XTRX_TRX,
                    m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
                    XTRX_LMS7_PWR_MODE,
                    settings.m_pwrmode) < 0) {
                qCritical("XTRXInput::applySettings: could not set power mode %d", settings.m_pwrmode);
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
                qDebug("XTRXInput::applySettings: clock set to %s (Ext: %d Hz)",
                       settings.m_extClock ? "external" : "internal",
                       settings.m_extClockFreq);
            }
        }
    }

    if (settingsKeys.contains("devSampleRate")
       || settingsKeys.contains("log2HardDecim") || force)
    {
        forwardChangeAllDSP = true; //m_settings.m_devSampleRate != settings.m_devSampleRate;

        if (m_deviceShared.m_dev->getDevice()) {
            doChangeSampleRate = true;
        }
    }

    if (m_deviceShared.m_dev->getDevice())
    {
        if ((m_settings.m_gainMode != settings.m_gainMode) || force)
        {
            if (settings.m_gainMode == XTRXInputSettings::GAIN_AUTO)
            {
                doGainAuto = true;
            }
            else
            {
                doGainLna = true;
                doGainTia = true;
                doGainPga = true;
            }
        }
        else if (m_settings.m_gainMode == XTRXInputSettings::GAIN_AUTO)
        {
            if (settingsKeys.contains("gain")) {
                doGainAuto = true;
            }
        }
        else if (m_settings.m_gainMode == XTRXInputSettings::GAIN_MANUAL)
        {
            if (settingsKeys.contains("lnaGain")) {
                doGainLna = true;
            }
            if (settingsKeys.contains("tiasGain")) {
                doGainTia = true;
            }
            if (settingsKeys.contains("pgaGain")) {
                doGainPga = true;
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
                qCritical("XTRXInput::applySettings: could %s and set LPF FIR to %f Hz",
                          settings.m_lpfFIREnable ? "enable" : "disable",
                          settings.m_lpfFIRBW);
            }
            else
            {
                //doCalibration = true;
                qDebug("XTRXInput::applySettings: %sd and set LPF FIR to %f Hz",
                       settings.m_lpfFIREnable ? "enable" : "disable",
                       settings.m_lpfFIRBW);
            }
        }
    }
#endif

    if (settingsKeys.contains("log2SoftDecim") || force)
    {
        forwardChangeOwnDSP = true;

        if (inputThread)
        {
            inputThread->setLog2Decimation(requestedChannel, settings.m_log2SoftDecim);
            qDebug() << "XTRXInput::applySettings: set soft decimation to " << (1<<settings.m_log2SoftDecim);
        }
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (inputThread)
        {
            inputThread->setIQOrder(settings.m_iqOrder);
            qDebug() << "XTRXInput::applySettings: set IQ order to " << (settings.m_iqOrder ? "IQ" : "QI");
        }
    }

    if (settingsKeys.contains("antennaPath") || force)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_set_antenna(m_deviceShared.m_dev->getDevice(), settings.m_antennaPath) < 0) {
                qCritical("XTRXInput::applySettings: could not set antenna path to %d", (int) settings.m_antennaPath);
            } else {
                qDebug("XTRXInput::applySettings: set antenna path to %d", (int) settings.m_antennaPath);
            }
        }
    }

    if (settingsKeys.contains("centerFrequency") || force)
    {
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

    if (doChangeSampleRate && (m_settings.m_devSampleRate != 0))
    {
        // XTRXInputThread *rxThread = findThread();

        // if (rxThread && rxThread->isRunning())
        // {
        //     rxThread->stopWork();
        //     rxThreadWasRunning = true;
        // }

        // suspendTxThread();

        double master = (m_settings.m_log2HardDecim == 0) ? 0 : (m_settings.m_devSampleRate * 4 * (1 << m_settings.m_log2HardDecim));

        int res = m_deviceShared.m_dev->setSamplerate(
            m_settings.m_devSampleRate,
            master,
            false
        );

        doChangeFreq = true;
        forceNCOFrequency = true;
        forwardChangeAllDSP = true;
        m_settings.m_devSampleRate = m_deviceShared.m_dev->getActualInputRate();
        m_settings.m_log2HardDecim = getLog2HardDecim();

        qDebug("XTRXInput::applySettings: sample rate set %s to %f with hard decimation of %d",
            (res < 0) ? "changed" : "unchanged",
            m_settings.m_devSampleRate,
            m_settings.m_log2HardDecim);

        // resumeTxThread();

        // if (rxThreadWasRunning) {
        //     rxThread->startWork();
        // }
    }

    if (doLPCalibration)
    {
        if (xtrx_tune_rx_bandwidth(m_deviceShared.m_dev->getDevice(),
                m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
                m_settings.m_lpfBW,
                0) < 0) {
            qCritical("XTRXInput::applySettings: could not set LPF to %f Hz", m_settings.m_lpfBW);
        } else {
            qDebug("XTRXInput::applySettings: LPF set to %f Hz", m_settings.m_lpfBW);
        }
    }

    if (doGainAuto) {
        apply_gain_auto(m_settings.m_gain);
    }
    if (doGainLna) {
        apply_gain_lna(m_settings.m_lnaGain);
    }
    if (doGainTia) {
        apply_gain_tia(tia_to_db(m_settings.m_tiaGain));
    }
    if (doGainPga) {
        apply_gain_pga(m_settings.m_pgaGain);
    }

    if (doChangeFreq)
    {
        forwardChangeRxDSP = true;

        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_tune(m_deviceShared.m_dev->getDevice(),
                    XTRX_TUNE_RX_FDD,
                    m_settings.m_centerFrequency,
                    0) < 0) {
                qCritical("XTRXInput::applySettings: could not set frequency to %lu", m_settings.m_centerFrequency);
            } else {
                //doCalibration = true;
                qDebug("XTRXInput::applySettings: frequency set to %lu", m_settings.m_centerFrequency);
            }
        }
    }

    if (forceNCOFrequency)
    {
        if (m_deviceShared.m_dev->getDevice())
        {
            if (xtrx_tune_ex(m_deviceShared.m_dev->getDevice(),
                    XTRX_TUNE_BB_RX,
                    m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
                    (m_settings.m_ncoEnable) ? m_settings.m_ncoFrequency : 0,
                    NULL) < 0)
            {
                qCritical("XTRXInput::applySettings: could not %s and set NCO to %d Hz",
                          m_settings.m_ncoEnable ? "enable" : "disable",
                          m_settings.m_ncoFrequency);
            }
            else
            {
                forwardChangeOwnDSP = true;
                qDebug("XTRXInput::applySettings: %sd and set NCO to %d Hz",
                       m_settings.m_ncoEnable ? "enable" : "disable",
                       m_settings.m_ncoFrequency);
            }
        }
    }

    // forward changes to buddies or oneself

    if (forwardChangeAllDSP)
    {
        qDebug("XTRXInput::applySettings: forward change to all buddies");

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
                    getDevSampleRate(), getLog2HardDecim(), m_settings.m_centerFrequency, true);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceXTRXShared::MsgReportBuddyChange *report = DeviceXTRXShared::MsgReportBuddyChange::create(
                    getDevSampleRate(), getLog2HardDecim(), m_settings.m_centerFrequency, true);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeRxDSP)
    {
        qDebug("XTRXInput::applySettings: forward change to Rx buddies");

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
                        getDevSampleRate(), getLog2HardDecim(), m_settings.m_centerFrequency, true);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeOwnDSP)
    {
        qDebug("XTRXInput::applySettings: forward change to self only");

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

int XTRXInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setXtrxInputSettings(new SWGSDRangel::SWGXtrxInputSettings());
    response.getXtrxInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int XTRXInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    XTRXInputSettings settings = m_settings;
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

void XTRXInput::webapiUpdateDeviceSettings(
        XTRXInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getXtrxInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getXtrxInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("log2HardDecim")) {
        settings.m_log2HardDecim = response.getXtrxInputSettings()->getLog2HardDecim();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getXtrxInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getXtrxInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("log2SoftDecim")) {
        settings.m_log2SoftDecim = response.getXtrxInputSettings()->getLog2SoftDecim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getXtrxInputSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("lpfBW")) {
        settings.m_lpfBW = response.getXtrxInputSettings()->getLpfBw();
    }
    if (deviceSettingsKeys.contains("gain")) {
        settings.m_gain = response.getXtrxInputSettings()->getGain();
    }
    if (deviceSettingsKeys.contains("ncoEnable")) {
        settings.m_ncoEnable = response.getXtrxInputSettings()->getNcoEnable() != 0;
    }
    if (deviceSettingsKeys.contains("ncoFrequency")) {
        settings.m_ncoFrequency = response.getXtrxInputSettings()->getNcoFrequency();
    }
    if (deviceSettingsKeys.contains("antennaPath")) {
        settings.m_antennaPath = (xtrx_antenna_t) response.getXtrxInputSettings()->getAntennaPath();
    }
    if (deviceSettingsKeys.contains("gainMode")) {
        settings.m_gainMode = (XTRXInputSettings::GainMode) response.getXtrxInputSettings()->getGainMode();
    }
    if (deviceSettingsKeys.contains("lnaGain")) {
        settings.m_lnaGain = response.getXtrxInputSettings()->getLnaGain();
    }
    if (deviceSettingsKeys.contains("tiaGain")) {
        settings.m_tiaGain = response.getXtrxInputSettings()->getTiaGain();
    }
    if (deviceSettingsKeys.contains("pgaGain")) {
        settings.m_pgaGain = response.getXtrxInputSettings()->getPgaGain();
    }
    if (deviceSettingsKeys.contains("extClock")) {
        settings.m_extClock = response.getXtrxInputSettings()->getExtClock() != 0;
    }
    if (deviceSettingsKeys.contains("extClockFreq")) {
        settings.m_extClockFreq = response.getXtrxInputSettings()->getExtClockFreq();
    }
    if (deviceSettingsKeys.contains("pwrmode")) {
        settings.m_pwrmode = response.getXtrxInputSettings()->getPwrmode();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getXtrxInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getXtrxInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getXtrxInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getXtrxInputSettings()->getReverseApiDeviceIndex();
    }
}

void XTRXInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const XTRXInputSettings& settings)
{
    response.getXtrxInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getXtrxInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getXtrxInputSettings()->setLog2HardDecim(settings.m_log2HardDecim);
    response.getXtrxInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getXtrxInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getXtrxInputSettings()->setLog2SoftDecim(settings.m_log2SoftDecim);
    response.getXtrxInputSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getXtrxInputSettings()->setLpfBw(settings.m_lpfBW);
    response.getXtrxInputSettings()->setGain(settings.m_gain);
    response.getXtrxInputSettings()->setNcoEnable(settings.m_ncoEnable ? 1 : 0);
    response.getXtrxInputSettings()->setNcoFrequency(settings.m_ncoFrequency);
    response.getXtrxInputSettings()->setAntennaPath((int) settings.m_antennaPath);
    response.getXtrxInputSettings()->setGainMode((int) settings.m_gainMode);
    response.getXtrxInputSettings()->setLnaGain(settings.m_lnaGain);
    response.getXtrxInputSettings()->setTiaGain(settings.m_tiaGain);
    response.getXtrxInputSettings()->setPgaGain(settings.m_pgaGain);
    response.getXtrxInputSettings()->setExtClock(settings.m_extClock ? 1 : 0);
    response.getXtrxInputSettings()->setExtClockFreq(settings.m_extClockFreq);
    response.getXtrxInputSettings()->setPwrmode(settings.m_pwrmode);

    response.getXtrxInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getXtrxInputSettings()->getReverseApiAddress()) {
        *response.getXtrxInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getXtrxInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getXtrxInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getXtrxInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int XTRXInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setXtrxInputReport(new SWGSDRangel::SWGXtrxInputReport());
    response.getXtrxInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int XTRXInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int XTRXInput::webapiRun(
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

void XTRXInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
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
                     XTRX_RX, XTRX_CH_AB, XTRX_PERF_LLFIFO, &fifolevel);
        success = (ret >= 0);
        temp = m_deviceShared.get_board_temperature() / 256.0;
        gpsStatus = m_deviceShared.get_gps_status();
    }

    response.getXtrxInputReport()->setSuccess(success ? 1 : 0);
    response.getXtrxInputReport()->setFifoSize(fifosize);
    response.getXtrxInputReport()->setFifoFill(fifolevel);
    response.getXtrxInputReport()->setTemperature(temp);
    response.getXtrxInputReport()->setGpsLock(gpsStatus ? 1 : 0);
}

void XTRXInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const XTRXInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("XTRX"));
    swgDeviceSettings->setXtrxInputSettings(new SWGSDRangel::SWGXtrxInputSettings());
    SWGSDRangel::SWGXtrxInputSettings *swgXtrxInputSettings = swgDeviceSettings->getXtrxInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgXtrxInputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgXtrxInputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("log2HardDecim") || force) {
        swgXtrxInputSettings->setLog2HardDecim(settings.m_log2HardDecim);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgXtrxInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgXtrxInputSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("log2SoftDecim") || force) {
        swgXtrxInputSettings->setLog2SoftDecim(settings.m_log2SoftDecim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgXtrxInputSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoEnable") || force) {
        swgXtrxInputSettings->setNcoEnable(settings.m_ncoEnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoFrequency") || force) {
        swgXtrxInputSettings->setNcoFrequency(settings.m_ncoFrequency);
    }
    if (deviceSettingsKeys.contains("lpfBW") || force) {
        swgXtrxInputSettings->setLpfBw(settings.m_lpfBW);
    }
    if (deviceSettingsKeys.contains("antennaPath") || force) {
        swgXtrxInputSettings->setAntennaPath((int) settings.m_antennaPath);
    }
    if (deviceSettingsKeys.contains("gainMode") || force) {
        swgXtrxInputSettings->setGainMode((int) settings.m_gainMode);
    }
    if (deviceSettingsKeys.contains("gain") || force) {
        swgXtrxInputSettings->setGain(settings.m_gain);
    }
    if (deviceSettingsKeys.contains("lnaGain") || force) {
        swgXtrxInputSettings->setLnaGain(settings.m_lnaGain);
    }
    if (deviceSettingsKeys.contains("tiaGain") || force) {
        swgXtrxInputSettings->setTiaGain(settings.m_tiaGain);
    }
    if (deviceSettingsKeys.contains("pgaGain") || force) {
        swgXtrxInputSettings->setPgaGain(settings.m_pgaGain);
    }
    if (deviceSettingsKeys.contains("extClock") || force) {
        swgXtrxInputSettings->setExtClock(settings.m_extClock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("extClockFreq") || force) {
        swgXtrxInputSettings->setExtClockFreq(settings.m_extClockFreq);
    }
    if (deviceSettingsKeys.contains("pwrmode") || force) {
        swgXtrxInputSettings->setPwrmode(settings.m_pwrmode);
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

void XTRXInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
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

void XTRXInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "XTRXInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("XTRXInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
