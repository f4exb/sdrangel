///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, 2018 Edouard Griffiths, F4EXB                             //
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QMutexLocker>
#include <QDebug>
#include <cstddef>
#include <string.h>
#include "xtrx_api.h"

#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "dsp/dspcommands.h"
#include "dsp/filerecord.h"
#include "xtrxinput.h"
#include "xtrxinputthread.h"
#include "xtrx/devicextrxparam.h"
#include "xtrx/devicextrxshared.h"
#include "xtrx/devicextrx.h"

MESSAGE_CLASS_DEFINITION(XTRXInput::MsgConfigureXTRX, Message)
MESSAGE_CLASS_DEFINITION(XTRXInput::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXInput::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXInput::MsgReportStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(XTRXInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(XTRXInput::MsgStartStop, Message)

XTRXInput::XTRXInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_XTRXInputThread(0),
    m_deviceDescription("XTRXInput"),
    m_running(false)
{
    openDevice();

    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);
}

XTRXInput::~XTRXInput()
{
    if (m_running) {
        stop();
    }

    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
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

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
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

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
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
        strcpy(serial, qPrintable(m_deviceAPI->getSampleSourceSerial()));

        if (!m_deviceShared.m_dev->open(serial))
        {
            qCritical("XTRXInput::openDevice: cannot open BladeRF2 device");
            return false;
        }
    }

    m_deviceShared.m_channel = m_deviceAPI->getItemIndex(); // publicly allocate channel
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
    applySettings(m_settings, true, false);
}

XTRXInputThread *XTRXInput::findThread()
{
    if (m_XTRXInputThread == 0) // this does not own the thread
    {
        XTRXInputThread *xtrxInputThread = 0;

        // find a buddy that has allocated the thread
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

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
    const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

    for (; it != sourceBuddies.end(); ++it)
    {
        XTRXInput *buddySource = ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_source;

        if (buddySource)
        {
            buddySource->setThread(m_XTRXInputThread);
            m_XTRXInputThread = 0;  // zero for others
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

    int requestedChannel = m_deviceAPI->getItemIndex();
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
            m_XTRXInputThread = xtrxInputThread; // take ownership

            for (int i = 0; i < 2; i++) // restore original FIFO references
            {
                xtrxInputThread->setFifo(i, fifos[i]);
                xtrxInputThread->setLog2Decimation(i, log2Decims[i]);
            }

            // remove old thread address from buddies (reset in all buddies). The address being held only in the owning source.
            const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
            std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

            for (; it != sourceBuddies.end(); ++it) {
                ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
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
        needsStart = true;
    }

    xtrxInputThread->setFifo(requestedChannel, &m_sampleFifo);
    xtrxInputThread->setLog2Decimation(requestedChannel, m_settings.m_log2SoftDecim);

    if (needsStart)
    {
        qDebug("XTRXInput::start: (re)sart thread");
        xtrxInputThread->startWork();
    }

    applySettings(m_settings, true);

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

    int requestedChannel = m_deviceAPI->getItemIndex(); // channel to remove
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
        m_XTRXInputThread = 0;

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it) {
            ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
        }
    }
    else if (nbOriginalChannels == 2) // Reduce from MI to SI by deleting and re-creating the thread
    {
        qDebug("XTRXInput::stop: MI mode. Reduce by deleting and re-creating the thread");
        xtrxInputThread->stopWork();
        SampleSinkFifo **fifos = new SampleSinkFifo*[2];
        unsigned int *log2Decims = new unsigned int[2];

        for (int i = 0; i < 2; i++) // save original FIFO references
        {
            fifos[i] = xtrxInputThread->getFifo(i);
            log2Decims[i] = xtrxInputThread->getLog2Decimation(i);
        }

        delete xtrxInputThread;
        m_XTRXInputThread = 0;

        xtrxInputThread = new XTRXInputThread(m_deviceShared.m_dev->getDevice(), 1, requestedChannel ^ 1); // leave opposite channel
        m_XTRXInputThread = xtrxInputThread; // take ownership

        for (int i = 0; i < nbOriginalChannels-1; i++)  // restore original FIFO references
        {
            xtrxInputThread->setFifo(i, fifos[i]);
            xtrxInputThread->setLog2Decimation(i, log2Decims[i]);
        }

        // remove old thread address from buddies (reset in all buddies). The address being held only in the owning source.
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it) {
            ((DeviceXTRXShared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
        }

        xtrxInputThread->startWork();

        // was used as temporary storage:
        delete[] fifos;
        delete[] log2Decims;
    }

    m_running = false;
}

void XTRXInput::suspendTxThread()
{
    // TODO: activate when output is managed
//    XTRXOutputThread *xtrxOutputThread = 0;
//
//    // find a buddy that has allocated the thread
//    const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
//    std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin()
//
//    for (; itSink != sinkBuddies.end(); ++itSink)
//    {
//        XTRXOutput *buddySink = ((DeviceXTRXShared*) (*itSink)->getBuddySharedPtr())->m_sink;
//
//        if (buddySink)
//        {
//            xtrxOutputThread = buddySink->getThread();
//
//            if (xtrxOutputThread) {
//                break;
//            }
//        }
//    }
//
//    if (xtrxOutputThread) {
//        xtrxOutputThread->stopWork();
//    }
}

void XTRXInput::resumeTxThread()
{
    // TODO: activate when output is managed
//    XTRXOutputThread *xtrxOutputThread = 0;
//
//    // find a buddy that has allocated the thread
//    const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
//    std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin()
//
//    for (; itSink != sinkBuddies.end(); ++itSink)
//    {
//        XTRXOutput *buddySink = ((DeviceXTRXShared*) (*itSink)->getBuddySharedPtr())->m_sink;
//
//        if (buddySink)
//        {
//            xtrxOutputThread = buddySink->getThread();
//
//            if (xtrxOutputThread) {
//                break;
//            }
//        }
//    }
//
//    if (xtrxOutputThread) {
//        xtrxOutputThread->startWork();
//    }
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

    MsgConfigureXTRX* message = MsgConfigureXTRX::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureXTRX* messageToGUI = MsgConfigureXTRX::create(m_settings, true);
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
    return (int)((rate / (1<<m_settings.m_log2SoftDecim)));
}

quint64 XTRXInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency + (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);
}

void XTRXInput::setCenterFrequency(qint64 centerFrequency)
{
    XTRXInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency - (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);

    MsgConfigureXTRX* message = MsgConfigureXTRX::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureXTRX* messageToGUI = MsgConfigureXTRX::create(settings, false);
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

        if (!applySettings(conf.getSettings(), conf.getForce()))
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
            m_settings.m_devSampleRate = m_deviceShared.m_inputRate;
            m_settings.m_log2HardDecim = log2(m_deviceShared.m_masterRate / m_deviceShared.m_inputRate / 4);

            qDebug() << "XTRXInput::handleMessage: MsgReportBuddyChange:"
                     << " host_Hz: " << m_deviceShared.m_inputRate
                     << " rf_Hz: " << m_deviceShared.m_masterRate / 4
                     << " m_log2HardDecim: " << m_settings.m_log2HardDecim;
        }

        if (m_settings.m_ncoEnable) // need to reset NCO after sample rate change
        {
            applySettings(m_settings, true, true);
        }

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        DSPSignalNotification *notif = new DSPSignalNotification(
                    m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim),
                    m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        DeviceXTRXShared::MsgReportBuddyChange *reportToGUI = DeviceXTRXShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
        getMessageQueueToGUI()->push(reportToGUI);

        return true;
    }
    else if (DeviceXTRXShared::MsgReportClockSourceChange::match(message))
    {
        DeviceXTRXShared::MsgReportClockSourceChange& report = (DeviceXTRXShared::MsgReportClockSourceChange&) message;

        m_settings.m_extClock     = report.getExtClock();
        m_settings.m_extClockFreq = report.getExtClockFeq();

        DeviceXTRXShared::MsgReportClockSourceChange *reportToGUI = DeviceXTRXShared::MsgReportClockSourceChange::create(
                    m_settings.m_extClock, m_settings.m_extClockFreq);
        getMessageQueueToGUI()->push(reportToGUI);

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
        if (m_deviceAPI->getSampleSourceGUIMessageQueue())
        {
            uint64_t fifolevel = 0;

            xtrx_val_get(m_deviceShared.m_dev->getDevice(),
                         XTRX_RX, XTRX_CH_AB, XTRX_PERF_LLFIFO, &fifolevel);

            MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        true,
                        true,
                        fifolevel,
                        65536);

            if (m_deviceAPI->getSampleSourceGUIMessageQueue()) {
                m_deviceAPI->getSampleSourceGUIMessageQueue()->push(report);
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
        if (m_deviceAPI->getSampleSourceGUIMessageQueue())
        {
            DeviceXTRXShared::MsgReportDeviceInfo *report = DeviceXTRXShared::MsgReportDeviceInfo::create(board_temp, gps_locked);
            m_deviceAPI->getSampleSourceGUIMessageQueue()->push(report);
        }

        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            if ((*itSource)->getSampleSourceGUIMessageQueue())
            {
                DeviceXTRXShared::MsgReportDeviceInfo *report = DeviceXTRXShared::MsgReportDeviceInfo::create(board_temp, gps_locked);
                (*itSource)->getSampleSourceGUIMessageQueue()->push(report);
            }
        }

        // send to sink buddies
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            if ((*itSink)->getSampleSinkGUIMessageQueue())
            {
                DeviceXTRXShared::MsgReportDeviceInfo *report = DeviceXTRXShared::MsgReportDeviceInfo::create(board_temp, gps_locked);
                (*itSink)->getSampleSinkGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "XTRXInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop()) {
            m_fileSink->startRecording();
        } else {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "XTRXInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initAcquisition())
            {
                m_deviceAPI->startAcquisition();
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
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
        qDebug("XTRXInput::applySettings: xtrx_set_gain(LNA) failed");
    } else {
        qDebug() << "XTRXInput::applySettings: Gain (LNA) set to " << gain;
    }
}

void XTRXInput::apply_gain_tia(double gain)
{
    if (xtrx_set_gain(m_deviceShared.m_dev->getDevice(),
            m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
            XTRX_RX_TIA_GAIN,
            gain,
            0) < 0) {
        qDebug("XTRXInput::applySettings: xtrx_set_gain(TIA) failed");
    } else {
        qDebug() << "XTRXInput::applySettings: Gain (TIA) set to " << gain;
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
        qDebug("XTRXInput::applySettings: xtrx_set_gain(PGA) failed");
    }
    else
    {
        qDebug() << "XTRXInput::applySettings: Gain (PGA) set to " << gain;
    }
}

bool XTRXInput::applySettings(const XTRXInputSettings& settings, bool force, bool forceNCOFrequency)
{
    int requestedChannel = m_deviceAPI->getItemIndex();
    XTRXInputThread *inputThread = findThread();

    bool forwardChangeOwnDSP = false;
    bool forwardChangeRxDSP  = false;
    bool forwardChangeAllDSP = false;
    bool forwardClockSource  = false;
    bool rxThreadWasRunning = false;
    bool doLPCalibration = false;
    bool doChangeSampleRate = false;
    bool doChangeFreq = false;

    bool doGainAuto = false;
    bool doGainLna = false;
    bool doGainTia = false;
    bool doGainPga = false;

    // apply settings

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force) {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force) {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if ((m_settings.m_pwrmode != settings.m_pwrmode))
    {
        if (xtrx_val_set(m_deviceShared.m_dev->getDevice(),
                XTRX_TRX,
                m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
                XTRX_LMS7_PWR_MODE,
                settings.m_pwrmode) < 0) {
            qCritical("XTRXInput::applySettings: could not set power mode %d", settings.m_pwrmode);
        }
    }

    if ((m_settings.m_extClock != settings.m_extClock) ||
            (settings.m_extClock && (m_settings.m_extClockFreq != settings.m_extClockFreq)) || force)
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

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
            || (m_settings.m_log2HardDecim != settings.m_log2HardDecim) || force)
    {
        forwardChangeAllDSP = true; //m_settings.m_devSampleRate != settings.m_devSampleRate;

        if (m_deviceShared.m_dev->getDevice() != 0) {
            doChangeSampleRate = true;
        }
    }

    if (m_deviceShared.m_dev->getDevice() != 0)
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
            doGainAuto = true;
        }
        else if (m_settings.m_gainMode == XTRXInputSettings::GAIN_MANUAL)
        {
            if (m_settings.m_lnaGain != settings.m_lnaGain) {
                doGainLna = true;
            }
            if (m_settings.m_tiaGain != settings.m_tiaGain) {
                doGainTia = true;
            }
            if (m_settings.m_pgaGain != settings.m_pgaGain) {
                doGainPga = true;
            }
        }
    }

    if ((m_settings.m_lpfBW != settings.m_lpfBW) || force)
    {
        if (m_deviceShared.m_dev->getDevice() != 0) {
            doLPCalibration = true;
        }
    }

#if 0
    if ((m_settings.m_lpfFIRBW != settings.m_lpfFIRBW) ||
            (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
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

    if ((m_settings.m_log2SoftDecim != settings.m_log2SoftDecim) || force)
    {
        forwardChangeOwnDSP = true;

        if (inputThread != 0)
        {
            inputThread->setLog2Decimation(requestedChannel, settings.m_log2SoftDecim);
            qDebug() << "XTRXInput::applySettings: set soft decimation to " << (1<<settings.m_log2SoftDecim);
        }
    }

    if ((m_settings.m_antennaPath != settings.m_antennaPath) || force)
    {
        if (m_deviceShared.m_dev->getDevice() != 0)
        {
            if (xtrx_set_antenna(m_deviceShared.m_dev->getDevice(), settings.m_antennaPath) < 0) {
                qCritical("XTRXInput::applySettings: could not set antenna path to %d", (int) settings.m_antennaPath);
            } else {
                qDebug("XTRXInput::applySettings: set antenna path to %d", (int) settings.m_antennaPath);
            }
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force) {
        doChangeFreq = true;
    }

    if ((m_settings.m_ncoFrequency != settings.m_ncoFrequency) ||
                (m_settings.m_ncoEnable != settings.m_ncoEnable) || force)
    {
        forceNCOFrequency = true;
    }


    m_settings = settings;

    if (doChangeSampleRate)
    {
        XTRXInputThread *rxThread = findThread();

        if (rxThread && rxThread->isRunning())
        {
            rxThread->stopWork();
            rxThreadWasRunning = true;
        }

        suspendTxThread();

        double master = (settings.m_log2HardDecim == 0) ? 0 : (settings.m_devSampleRate * 4 * (1 << settings.m_log2HardDecim));

        if (m_deviceShared.set_samplerate(settings.m_devSampleRate,
                                          master, //(settings.m_devSampleRate<<settings.m_log2HardDecim)*4,
                                          false) < 0)
        {
            qCritical("XTRXInput::applySettings: could not set sample rate to %f with oversampling of %d",
                      settings.m_devSampleRate,
                      1<<settings.m_log2HardDecim);
        }
        else
        {
            doChangeFreq = true;
            forceNCOFrequency = true;

            qDebug("XTRXInput::applySettings: sample rate set to %f with oversampling of %d",
                   settings.m_devSampleRate,
                   1<<settings.m_log2HardDecim);
        }

        resumeTxThread();

        if (rxThreadWasRunning) {
            rxThread->startWork();
        }
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

    if (doGainAuto)
    {
        apply_gain_auto(m_settings.m_gain);
    }
    if (doGainLna)
    {
        apply_gain_lna(m_settings.m_lnaGain);
    }
    if (doGainTia)
    {
        apply_gain_tia(tia_to_db(m_settings.m_tiaGain));
    }
    if (doGainPga)
    {
        apply_gain_pga(m_settings.m_pgaGain);
    }

    if (doChangeFreq)
    {
        forwardChangeRxDSP = true;

        if (m_deviceShared.m_dev->getDevice() != 0)
        {
            if (xtrx_tune(m_deviceShared.m_dev->getDevice(),
                    XTRX_TUNE_RX_FDD,
                    settings.m_centerFrequency,
                    0) < 0) {
                qCritical("XTRXInput::applySettings: could not set frequency to %lu", settings.m_centerFrequency);
            } else {
                //doCalibration = true;
                qDebug("XTRXInput::applySettings: frequency set to %lu", settings.m_centerFrequency);
            }
        }
    }

    if (forceNCOFrequency)
    {
        if (m_deviceShared.m_dev->getDevice() != 0)
        {
            if (xtrx_tune_ex(m_deviceShared.m_dev->getDevice(),
                    XTRX_TUNE_BB_RX,
                    m_deviceShared.m_channel == 0 ? XTRX_CH_A : XTRX_CH_B,
                    (settings.m_ncoEnable) ? settings.m_ncoFrequency : 0,
                    NULL) < 0)
            {
                qCritical("XTRXInput::applySettings: could not %s and set NCO to %d Hz",
                          settings.m_ncoEnable ? "enable" : "disable",
                          settings.m_ncoFrequency);
            }
            else
            {
                forwardChangeOwnDSP = true;
                qDebug("XTRXInput::applySettings: %sd and set NCO to %d Hz",
                       settings.m_ncoEnable ? "enable" : "disable",
                       settings.m_ncoFrequency);
            }
        }
    }

    // forward changes to buddies or oneself

    if (forwardChangeAllDSP)
    {
        qDebug("XTRXInput::applySettings: forward change to all buddies");

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(
                    m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim),
                    m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceXTRXShared::MsgReportBuddyChange *report = DeviceXTRXShared::MsgReportBuddyChange::create(
                        m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
            (*itSource)->getSampleSourceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceXTRXShared::MsgReportBuddyChange *report = DeviceXTRXShared::MsgReportBuddyChange::create(
                        m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
            (*itSink)->getSampleSinkInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeRxDSP)
    {
        qDebug("XTRXInput::applySettings: forward change to Rx buddies");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim);
        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceXTRXShared::MsgReportBuddyChange *report = DeviceXTRXShared::MsgReportBuddyChange::create(
                        m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
            (*itSource)->getSampleSourceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeOwnDSP)
    {
        qDebug("XTRXInput::applySettings: forward change to self only");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim);
        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency + ncoShift);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (forwardClockSource)
    {
        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceXTRXShared::MsgReportClockSourceChange *report = DeviceXTRXShared::MsgReportClockSourceChange::create(
                        m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSource)->getSampleSourceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceXTRXShared::MsgReportClockSourceChange *report = DeviceXTRXShared::MsgReportClockSourceChange::create(
                        m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSink)->getSampleSinkInputMessageQueue()->push(report);
        }
    }

    qDebug() << "XTRXInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
             << " device stream sample rate: " << m_settings.m_devSampleRate << "S/s"
             << " sample rate with soft decimation: " << m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim) << "S/s"
             << " m_gain: " << m_settings.m_gain
             << " m_lpfBW: " << m_settings.m_lpfBW
             << " m_ncoEnable: " << m_settings.m_ncoEnable
             << " m_ncoFrequency: " << m_settings.m_ncoFrequency
             << " m_antennaPath: " << m_settings.m_antennaPath
             << " m_extClock: " << m_settings.m_extClock
             << " m_extClockFreq: " << m_settings.m_extClockFreq
             << " force: " << force
             << " forceNCOFrequency: " << forceNCOFrequency
             << " doLPCalibration: " << doLPCalibration;

    return true;
}
