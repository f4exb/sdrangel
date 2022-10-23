///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <QMutexLocker>
#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>

#include "lime/LimeSuite.h"

#include "SWGDeviceSettings.h"
#include "SWGLimeSdrInputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGLimeSdrInputReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "limesdrinput.h"
#include "limesdrinputthread.h"
#include "limesdr/devicelimesdrparam.h"
#include "limesdr/devicelimesdrshared.h"
#include "limesdr/devicelimesdr.h"

MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgConfigureLimeSDR, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgReportStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgCalibrationResult, Message)

LimeSDRInput::LimeSDRInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_limeSDRInputThread(nullptr),
    m_deviceDescription("LimeSDRInput"),
    m_running(false),
    m_channelAcquired(false)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_streamId.handle = 0;
    suspendRxBuddies();
    suspendTxBuddies();
    openDevice();
    resumeTxBuddies();
    resumeRxBuddies();

    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LimeSDRInput::networkManagerFinished
    );
}

LimeSDRInput::~LimeSDRInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LimeSDRInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    suspendRxBuddies();
    suspendTxBuddies();
    closeDevice();
    resumeTxBuddies();
    resumeRxBuddies();
}

void LimeSDRInput::destroy()
{
    delete this;
}

bool LimeSDRInput::openDevice()
{
    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("LimeSDRInput::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("LimeSDRInput::openDevice: allocated SampleFifo");
    }

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();

    // look for Rx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        qDebug("LimeSDRInput::openDevice: look in Rx buddies");

        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        //m_deviceShared = *((DeviceLimeSDRShared *) sourceBuddy->getBuddySharedPtr()); // copy shared data
        DeviceLimeSDRShared *deviceLimeSDRShared = (DeviceLimeSDRShared*) sourceBuddy->getBuddySharedPtr();

        if (deviceLimeSDRShared == 0)
        {
            qCritical("LimeSDRInput::openDevice: the source buddy shared pointer is null");
            return false;
        }

        m_deviceShared.m_deviceParams = deviceLimeSDRShared->m_deviceParams;

        DeviceLimeSDRParams *deviceParams = m_deviceShared.m_deviceParams; // get device parameters

        if (deviceParams == 0)
        {
            qCritical("LimeSDRInput::openDevice: cannot get device parameters from Rx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("LimeSDRInput::openDevice: getting device parameters from Rx buddy");
        }

        if (m_deviceAPI->getSourceBuddies().size() == deviceParams->m_nbRxChannels)
        {
            qCritical("LimeSDRInput::openDevice: no more Rx channels available in device");
            return false; // no more Rx channels available in device
        }
        else
        {
            qDebug("LimeSDRInput::openDevice: at least one more Rx channel is available in device");
        }

        // check if the requested channel is busy and abort if so (should not happen if device management is working correctly)

        for (unsigned int i = 0; i < m_deviceAPI->getSourceBuddies().size(); i++)
        {
            DeviceAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
            DeviceLimeSDRShared *buddyShared = (DeviceLimeSDRShared *) buddy->getBuddySharedPtr();

            if (buddyShared->m_channel == requestedChannel)
            {
                qCritical("LimeSDRInput::openDevice: cannot open busy channel %u", requestedChannel);
                return false;
            }
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // look for Tx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("LimeSDRInput::openDevice: look in Tx buddies");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        //m_deviceShared = *((DeviceLimeSDRShared *) sinkBuddy->getBuddySharedPtr()); // copy parameters
        DeviceLimeSDRShared *deviceLimeSDRShared = (DeviceLimeSDRShared*) sinkBuddy->getBuddySharedPtr();

        if (deviceLimeSDRShared == 0)
        {
            qCritical("LimeSDRInput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        m_deviceShared.m_deviceParams = deviceLimeSDRShared->m_deviceParams;

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("LimeSDRInput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("LimeSDRInput::openDevice: getting device parameters from Tx buddy");
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // There are no buddies then create the first LimeSDR common parameters
    // open the device this will also populate common fields
    // take the first Rx channel
    else
    {
        qDebug("LimeSDRInput::openDevice: open device here");

        m_deviceShared.m_deviceParams = new DeviceLimeSDRParams();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));
        m_deviceShared.m_deviceParams->open(serial);
        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    return true;
}

void LimeSDRInput::suspendRxBuddies()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("LimeSDRInput::suspendRxBuddies (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSource)->getBuddySharedPtr();

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

void LimeSDRInput::suspendTxBuddies()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("LimeSDRInput::suspendTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();

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

void LimeSDRInput::resumeRxBuddies()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("LimeSDRInput::resumeRxBuddies (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSource)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void LimeSDRInput::resumeTxBuddies()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("LimeSDRInput::resumeTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void LimeSDRInput::closeDevice()
{
    if (m_deviceShared.m_deviceParams->getDevice() == 0) { // was never open
        return;
    }

    if (m_running) { stop(); }

    m_deviceShared.m_channel = -1;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }
}

bool LimeSDRInput::acquireChannel()
{
    suspendRxBuddies();
    suspendTxBuddies();

    // acquire the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_RX, m_deviceShared.m_channel, true) != 0)
    {
        qCritical("LimeSDRInput::acquireChannel: cannot enable Rx channel %d", m_deviceShared.m_channel);
        return false;
    }
    else
    {
        qDebug("LimeSDRInput::acquireChannel: Rx channel %d enabled", m_deviceShared.m_channel);
    }

    // set up the stream

    m_streamId.channel =  m_deviceShared.m_channel; // channel number
    m_streamId.fifoSize = 1024 * 256;               // fifo size in samples
    m_streamId.throughputVsLatency = 0.5;           // optimize for min latency
    m_streamId.isTx = false;                        // RX channel
    m_streamId.dataFmt = lms_stream_t::LMS_FMT_I12; // 12-bit integers

    if (LMS_SetupStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId) != 0)
    {
        qCritical("LimeSDRInput::acquireChannel: cannot setup the stream on Rx channel %d", m_deviceShared.m_channel);
        resumeTxBuddies();
        resumeRxBuddies();
        return false;
    }
    else
    {
        qDebug("LimeSDRInput::acquireChannel: stream set up on Rx channel %d", m_deviceShared.m_channel);
    }

    resumeTxBuddies();
    resumeRxBuddies();

    m_channelAcquired = true;

    return true;
}

void LimeSDRInput::releaseChannel()
{
    suspendRxBuddies();
    suspendTxBuddies();

    // destroy the stream
    if (LMS_DestroyStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId) != 0)
    {
        qWarning("LimeSDRInput::releaseChannel: cannot destroy the stream on Rx channel %d", m_deviceShared.m_channel);
    }
    else
    {
        qDebug("LimeSDRInput::releaseChannel: stream destroyed on Rx channel %d", m_deviceShared.m_channel);
    }

    m_streamId.handle = 0;

    // release the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_RX, m_deviceShared.m_channel, false) != 0)
    {
        qWarning("LimeSDRInput::releaseChannel: cannot disable Rx channel %d", m_deviceShared.m_channel);
    }
    else
    {
        qDebug("LimeSDRInput::releaseChannel: Rx channel %d disabled", m_deviceShared.m_channel);
    }

    resumeTxBuddies();
    resumeRxBuddies();

    // The channel will be effectively released to be reused in another device set only at close time

    m_channelAcquired = false;
}

void LimeSDRInput::init()
{
    applySettings(m_settings, QList<QString>(), true, false);
}

bool LimeSDRInput::start()
{
    if (!m_deviceShared.m_deviceParams->getDevice()) {
        return false;
    }

    if (m_running) { stop(); }

    if (!acquireChannel())
    {
        return false;
    }

    // start / stop streaming is done in the thread.

    m_limeSDRInputThread = new LimeSDRInputThread(&m_streamId, &m_sampleFifo);
    qDebug("LimeSDRInput::start: thread created");

    applySettings(m_settings, QList<QString>(), true);

    m_limeSDRInputThread->setLog2Decimation(m_settings.m_log2SoftDecim);
    m_limeSDRInputThread->setIQOrder(m_settings.m_iqOrder);
    m_limeSDRInputThread->startWork();

    m_deviceShared.m_thread = m_limeSDRInputThread;
    m_running = true;

    return true;
}

void LimeSDRInput::stop()
{
    qDebug("LimeSDRInput::stop");

    if (m_limeSDRInputThread)
    {
        m_limeSDRInputThread->stopWork();
        delete m_limeSDRInputThread;
        m_limeSDRInputThread = nullptr;
    }

    m_deviceShared.m_thread = 0;
    m_running = false;

    releaseChannel();
}

QByteArray LimeSDRInput::serialize() const
{
    return m_settings.serialize();
}

bool LimeSDRInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureLimeSDR* message = MsgConfigureLimeSDR::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDR* messageToGUI = MsgConfigureLimeSDR::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& LimeSDRInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int LimeSDRInput::getSampleRate() const
{
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2SoftDecim));
}

quint64 LimeSDRInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency + (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);
}

void LimeSDRInput::setCenterFrequency(qint64 centerFrequency)
{
    LimeSDRInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency - (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);

    MsgConfigureLimeSDR* message = MsgConfigureLimeSDR::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDR* messageToGUI = MsgConfigureLimeSDR::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

std::size_t LimeSDRInput::getChannelIndex()
{
    return m_deviceShared.m_channel;
}

void LimeSDRInput::getLORange(float& minF, float& maxF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_loRangeRx;
    minF = range.min;
    maxF = range.max;
    qDebug("LimeSDRInput::getLORange: min: %f max: %f", range.min, range.max);
}

void LimeSDRInput::getSRRange(float& minF, float& maxF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_srRangeRx;
    minF = range.min;
    maxF = range.max;
    qDebug("LimeSDRInput::getSRRange: min: %f max: %f", range.min, range.max);
}

void LimeSDRInput::getLPRange(float& minF, float& maxF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_lpfRangeRx;
    minF = range.min;
    maxF = range.max;
    qDebug("LimeSDRInput::getLPRange: min: %f max: %f", range.min, range.max);
}

uint32_t LimeSDRInput::getHWLog2Decim() const
{
    return m_deviceShared.m_deviceParams->m_log2OvSRRx;
}

DeviceLimeSDRParams::LimeType LimeSDRInput::getLimeType() const
{
    if (m_deviceShared.m_deviceParams) {
        return m_deviceShared.m_deviceParams->m_type;
    } else {
        return DeviceLimeSDRParams::LimeUndefined;
    }
}

bool LimeSDRInput::handleMessage(const Message& message)
{
    if (MsgConfigureLimeSDR::match(message))
    {
        MsgConfigureLimeSDR& conf = (MsgConfigureLimeSDR&) message;
        qDebug() << "LimeSDRInput::handleMessage: MsgConfigureLimeSDR";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce())) {
            qDebug("LimeSDRInput::handleMessage config error");
        }

        return true;
    }
    else if (DeviceLimeSDRShared::MsgReportBuddyChange::match(message))
    {
        DeviceLimeSDRShared::MsgReportBuddyChange& report = (DeviceLimeSDRShared::MsgReportBuddyChange&) message;

        if (report.getRxElseTx())
        {
            m_settings.m_devSampleRate   = report.getDevSampleRate();
            m_settings.m_log2HardDecim   = report.getLog2HardDecimInterp();
            m_settings.m_centerFrequency = report.getCenterFrequency();
        }
        else if (m_running)
        {
            double host_Hz;
            double rf_Hz;

            if (LMS_GetSampleRate(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    &host_Hz,
                    &rf_Hz) < 0)
            {
                qDebug("LimeSDRInput::handleMessage: MsgReportBuddyChange: LMS_GetSampleRate() failed");
            }
            else
            {
                m_settings.m_devSampleRate = roundf(host_Hz);
                int hard = roundf(rf_Hz) / m_settings.m_devSampleRate;
                m_settings.m_log2HardDecim = log2(hard);

                qDebug() << "LimeSDRInput::handleMessage: MsgReportBuddyChange:"
                         << " host_Hz: " << host_Hz
                         << " rf_Hz: " << rf_Hz
                         << " m_devSampleRate: " << m_settings.m_devSampleRate
                         << " log2Hard: " << hard
                         << " m_log2HardDecim: " << m_settings.m_log2HardDecim;
//                int adcdac_rate = report.getDevSampleRate() * (1<<report.getLog2HardDecimInterp());
//                m_settings.m_devSampleRate = adcdac_rate / (1<<m_settings.m_log2HardDecim); // new device to host sample rate
            }
        }

        if (m_settings.m_ncoEnable) // need to reset NCO after sample rate change
        {
            applySettings(
                m_settings,
                QList<QString>{"devSampleRate", "log2HardDecim", "m_centerFrequency"},
                false,
                true
            );
        }

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        DSPSignalNotification *notif = new DSPSignalNotification(
                m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim),
                m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        if (getMessageQueueToGUI())
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *reportToGUI = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
            getMessageQueueToGUI()->push(reportToGUI);
        }

        return true;
    }
    else if (DeviceLimeSDRShared::MsgReportClockSourceChange::match(message))
    {
        DeviceLimeSDRShared::MsgReportClockSourceChange& report = (DeviceLimeSDRShared::MsgReportClockSourceChange&) message;

        m_settings.m_extClock     = report.getExtClock();
        m_settings.m_extClockFreq = report.getExtClockFeq();

        if (getMessageQueueToGUI())
        {
            DeviceLimeSDRShared::MsgReportClockSourceChange *reportToGUI = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                    m_settings.m_extClock, m_settings.m_extClockFreq);
            getMessageQueueToGUI()->push(reportToGUI);
        }

        return true;
    }
    else if (DeviceLimeSDRShared::MsgReportGPIOChange::match(message))
    {
        DeviceLimeSDRShared::MsgReportGPIOChange& report = (DeviceLimeSDRShared::MsgReportGPIOChange&) message;

        m_settings.m_gpioDir     = report.getGPIODir();
        m_settings.m_gpioPins = report.getGPIOPins();

        // no GUI for the moment only REST API

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
//        qDebug() << "LimeSDRInput::handleMessage: MsgGetStreamInfo";
        lms_stream_status_t status;

        if (m_streamId.handle && (LMS_GetStreamStatus(&m_streamId, &status) == 0))
        {
            if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
            {
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        true, // Success
                        status.active,
                        status.fifoFilledCount,
                        status.fifoSize,
                        status.underrun,
                        status.overrun,
                        status.droppedPackets,
                        status.linkRate,
                        status.timestamp);
                m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
            }
        }
        else
        {
            if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
            {
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        false, // Success
                        false, // status.active,
                        0,     // status.fifoFilledCount,
                        16384, // status.fifoSize,
                        0,     // status.underrun,
                        0,     // status.overrun,
                        0,     // status.droppedPackets,
                        0,     // status.linkRate,
                        0);    // status.timestamp);
                m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else if (MsgGetDeviceInfo::match(message))
    {
        double temp = 0.0;
        uint8_t gpioPins = 0;

        if (m_deviceShared.m_deviceParams->getDevice() && (LMS_GetChipTemperature(m_deviceShared.m_deviceParams->getDevice(), 0, &temp) != 0)) {
            qDebug("LimeSDRInput::handleMessage: MsgGetDeviceInfo: cannot get temperature");
        }

        if ((m_deviceShared.m_deviceParams->m_type != DeviceLimeSDRParams::LimeMini)
            && (m_deviceShared.m_deviceParams->m_type != DeviceLimeSDRParams::LimeUndefined))
        {
            if (m_deviceShared.m_deviceParams->getDevice() && (LMS_GPIORead(m_deviceShared.m_deviceParams->getDevice(), &gpioPins, 1) != 0)) {
                qDebug("LimeSDROutput::handleMessage: MsgGetDeviceInfo: cannot get GPIO pins values");
            }
        }

        // send to oneself
        if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
        {
            DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp, gpioPins);
            m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
        }

        // send to source buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            if ((*itSource)->getSamplingDeviceGUIMessageQueue())
            {
                DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp, gpioPins);
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
                DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp, gpioPins);
                (*itSink)->getSamplingDeviceGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "LimeSDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool LimeSDRInput::applySettings(const LimeSDRInputSettings& settings, const QList<QString>& settingsKeys, bool force, bool forceNCOFrequency)
{
    qDebug() << "LimeSDRInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);
    bool forwardChangeOwnDSP = false;
    bool forwardChangeRxDSP  = false;
    bool forwardChangeAllDSP = false;
    bool forwardClockSource  = false;
    bool forwardGPIOChange   = false;
    bool ownThreadWasRunning = false;
    bool doCalibration = false;
    bool doLPCalibration = false;
    double clockGenFreq      = 0.0;

//  QMutexLocker mutexLocker(&m_mutex);

    qint64 deviceCenterFrequency = settings.m_centerFrequency;
    deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
    deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;

    if (LMS_GetClockFreq(m_deviceShared.m_deviceParams->getDevice(), LMS_CLOCK_CGEN, &clockGenFreq) != 0) {
        qCritical("LimeSDRInput::applySettings: could not get clock gen frequency");
    } else {
        qDebug() << "LimeSDRInput::applySettings: clock gen frequency: " << clockGenFreq;
    }

    // apply settings

    if (settingsKeys.contains("dcBlock") || settingsKeys.contains("iqCorrection") || force) {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if (settingsKeys.contains("gainMode") || force)
    {
        if (settings.m_gainMode == LimeSDRInputSettings::GAIN_AUTO)
        {
            if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
            {
                if (LMS_SetGaindB(m_deviceShared.m_deviceParams->getDevice(),
                        LMS_CH_RX,
                        m_deviceShared.m_channel,
                        settings.m_gain) < 0)
                {
                    qDebug("LimeSDRInput::applySettings: LMS_SetGaindB() failed");
                }
                else
                {
                    doCalibration = true;
                    qDebug() << "LimeSDRInput::applySettings: Gain (auto) set to " << settings.m_gain;
                }
            }
        }
        else
        {
            if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
            {
                if (DeviceLimeSDR::SetRFELNA_dB(m_deviceShared.m_deviceParams->getDevice(),
                        m_deviceShared.m_channel,
                        settings.m_lnaGain))
                {
                    doCalibration = true;
                    qDebug() << "LimeSDRInput::applySettings: LNA gain (manual) set to " << settings.m_lnaGain;
                }
                else
                {
                    qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRFELNA_dB() failed");
                }

                if (DeviceLimeSDR::SetRFETIA_dB(m_deviceShared.m_deviceParams->getDevice(),
                        m_deviceShared.m_channel,
                        settings.m_tiaGain))
                {
                    doCalibration = true;
                    qDebug() << "LimeSDRInput::applySettings: TIA gain (manual) set to " << settings.m_tiaGain;
                }
                else
                {
                    qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRFETIA_dB() failed");
                }

                if (DeviceLimeSDR::SetRBBPGA_dB(m_deviceShared.m_deviceParams->getDevice(),
                        m_deviceShared.m_channel,
                        settings.m_pgaGain))
                {
                    doCalibration = true;
                    qDebug() << "LimeSDRInput::applySettings: PGA gain (manual) set to " << settings.m_pgaGain;
                }
                else
                {
                    qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRBBPGA_dB() failed");
                }
            }
        }
    }

    if ((m_settings.m_gainMode == LimeSDRInputSettings::GAIN_AUTO) && settingsKeys.contains("gain"))
    {
        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (LMS_SetGaindB(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    settings.m_gain) < 0)
            {
                qDebug("LimeSDRInput::applySettings: LMS_SetGaindB() failed");
            }
            else
            {
                doCalibration = true;
                qDebug() << "LimeSDRInput::applySettings: Gain (auto) set to " << settings.m_gain;
            }
        }
    }

    if ((m_settings.m_gainMode == LimeSDRInputSettings::GAIN_MANUAL) && settingsKeys.contains("lnaGain"))
    {
        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (DeviceLimeSDR::SetRFELNA_dB(m_deviceShared.m_deviceParams->getDevice(),
                    m_deviceShared.m_channel,
                    settings.m_lnaGain))
            {
                doCalibration = true;
                qDebug() << "LimeSDRInput::applySettings: LNA gain (manual) set to " << settings.m_lnaGain;
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRFELNA_dB() failed");
            }
        }
    }

    if ((m_settings.m_gainMode == LimeSDRInputSettings::GAIN_MANUAL) && settingsKeys.contains("tiaGain"))
    {
        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (DeviceLimeSDR::SetRFETIA_dB(m_deviceShared.m_deviceParams->getDevice(),
                    m_deviceShared.m_channel,
                    settings.m_tiaGain))
            {
                doCalibration = true;
                qDebug() << "LimeSDRInput::applySettings: TIA gain (manual) set to " << settings.m_tiaGain;
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRFETIA_dB() failed");
            }
        }
    }

    if ((m_settings.m_gainMode == LimeSDRInputSettings::GAIN_MANUAL) && settingsKeys.contains("pgaGain"))
    {
        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (DeviceLimeSDR::SetRBBPGA_dB(m_deviceShared.m_deviceParams->getDevice(),
                    m_deviceShared.m_channel,
                    settings.m_pgaGain))
            {
                doCalibration = true;
                qDebug() << "LimeSDRInput::applySettings: PGA gain (manual) set to " << settings.m_pgaGain;
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRBBPGA_dB() failed");
            }
        }
    }

    if (settingsKeys.contains("devSampleRate")
       || settingsKeys.contains("log2HardDecim") || force)
    {
        forwardChangeAllDSP = true; //m_settings.m_devSampleRate != settings.m_devSampleRate;

        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (LMS_SetSampleRateDir(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    settings.m_devSampleRate,
                    1<<settings.m_log2HardDecim) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could not set sample rate to %d with oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardDecim);
            }
            else
            {
                m_deviceShared.m_deviceParams->m_log2OvSRRx = settings.m_log2HardDecim;
                m_deviceShared.m_deviceParams->m_sampleRate = settings.m_devSampleRate;
                //doCalibration = true;
                forceNCOFrequency = true;
                qDebug("LimeSDRInput::applySettings: set sample rate set to %d with oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardDecim);
            }
        }
    }

    if (settingsKeys.contains("lpfBW") || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired) {
            doLPCalibration = true;
        }
    }

    if (settingsKeys.contains("lpfFIRBW") ||
        settingsKeys.contains("lpfFIREnable") || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (LMS_SetGFIRLPF(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    settings.m_lpfFIREnable,
                    settings.m_lpfFIRBW) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could %s and set LPF FIR to %f Hz",
                        settings.m_lpfFIREnable ? "enable" : "disable",
                        settings.m_lpfFIRBW);
            }
            else
            {
                //doCalibration = true;
                qDebug("LimeSDRInput::applySettings: %sd and set LPF FIR to %f Hz",
                        settings.m_lpfFIREnable ? "enable" : "disable",
                        settings.m_lpfFIRBW);
            }
        }
    }

    if (settingsKeys.contains("ncoFrequency") ||
        settingsKeys.contains("ncoEnable") || force || forceNCOFrequency)
    {
        forwardChangeOwnDSP = true;

        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (DeviceLimeSDR::setNCOFrequency(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    settings.m_ncoEnable,
                    settings.m_ncoFrequency))
            {
                //doCalibration = true;
                m_deviceShared.m_ncoFrequency = settings.m_ncoEnable ? settings.m_ncoFrequency : 0; // for buddies
                qDebug("LimeSDRInput::applySettings: %sd and set NCO to %d Hz",
                        settings.m_ncoEnable ? "enable" : "disable",
                        settings.m_ncoFrequency);
            }
            else
            {
                qCritical("LimeSDRInput::applySettings: could not %s and set NCO to %d Hz",
                        settings.m_ncoEnable ? "enable" : "disable",
                        settings.m_ncoFrequency);
            }
        }
    }

    if (settingsKeys.contains("log2SoftDecim") || force)
    {
        forwardChangeOwnDSP = true;
        m_deviceShared.m_log2Soft = settings.m_log2SoftDecim; // for buddies

        if (m_limeSDRInputThread)
        {
            m_limeSDRInputThread->setLog2Decimation(settings.m_log2SoftDecim);
            qDebug() << "LimeSDRInput::applySettings: set soft decimation to " << (1<<settings.m_log2SoftDecim);
        }
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_limeSDRInputThread) {
            m_limeSDRInputThread->setIQOrder(settings.m_iqOrder);
        }
    }

    if (settingsKeys.contains("antennaPath") || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (DeviceLimeSDR::setRxAntennaPath(m_deviceShared.m_deviceParams->getDevice(),
                    m_deviceShared.m_channel,
                    settings.m_antennaPath))
            {
                doCalibration = true;
                qDebug("LimeSDRInput::applySettings: set antenna path to %d on channel %d",
                        (int) settings.m_antennaPath,
                        m_deviceShared.m_channel);
            }
            else
            {
                qCritical("LimeSDRInput::applySettings: could not set antenna path to %d",
                        (int) settings.m_antennaPath);
            }
        }
    }

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("transverterMode")
        || settingsKeys.contains("transverterDeltaFrequency")
        || force)
    {
        forwardChangeRxDSP = true;

        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (LMS_SetClockFreq(m_deviceShared.m_deviceParams->getDevice(), LMS_CLOCK_SXR, deviceCenterFrequency) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could not set frequency to %lld", deviceCenterFrequency);
            }
            else
            {
                doCalibration = true;
                m_deviceShared.m_centerFrequency = deviceCenterFrequency; // for buddies
                qDebug("LimeSDRInput::applySettings: frequency set to %lld", deviceCenterFrequency);
            }
        }
    }

    if (settingsKeys.contains("extClock") ||
        (settings.m_extClock && settingsKeys.contains("extClockFreq")) || force)
    {
        if (DeviceLimeSDR::setClockSource(m_deviceShared.m_deviceParams->getDevice(),
                settings.m_extClock,
                settings.m_extClockFreq))
        {
            forwardClockSource = true;
            doCalibration = true;
            qDebug("LimeSDRInput::applySettings: clock set to %s (Ext: %d Hz)",
                    settings.m_extClock ? "external" : "internal",
                    settings.m_extClockFreq);
        }
        else
        {
            qCritical("LimeSDRInput::applySettings: could not set clock to %s (Ext: %d Hz)",
                    settings.m_extClock ? "external" : "internal",
                    settings.m_extClockFreq);
        }
    }

    if ((m_deviceShared.m_deviceParams->m_type != DeviceLimeSDRParams::LimeMini)
        && (m_deviceShared.m_deviceParams->m_type != DeviceLimeSDRParams::LimeUndefined))
    {
        if (settingsKeys.contains("gpioDir") || force)
        {
            if (LMS_GPIODirWrite(m_deviceShared.m_deviceParams->getDevice(), &settings.m_gpioDir, 1) != 0)
            {
                qCritical("LimeSDRInput::applySettings: could not set GPIO directions to %u", settings.m_gpioDir);
            }
            else
            {
                forwardGPIOChange = true;
                qDebug("LimeSDRInput::applySettings: GPIO directions set to %u", settings.m_gpioDir);
            }
        }

        if (settingsKeys.contains("gpioPins") || force)
        {
            if (LMS_GPIOWrite(m_deviceShared.m_deviceParams->getDevice(), &settings.m_gpioPins, 1) != 0)
            {
                qCritical("LimeSDRInput::applySettings: could not set GPIO pins to %u", settings.m_gpioPins);
            }
            else
            {
                forwardGPIOChange = true;
                qDebug("LimeSDRInput::applySettings: GPIO pins set to %u", settings.m_gpioPins);
            }
        }
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

    double clockGenFreqAfter;

    if (LMS_GetClockFreq(m_deviceShared.m_deviceParams->getDevice(), LMS_CLOCK_CGEN, &clockGenFreqAfter) != 0)
    {
        qCritical("LimeSDRInput::applySettings: could not get clock gen frequency");
    }
    else
    {
        qDebug() << "LimeSDRInput::applySettings: clock gen frequency after: " << clockGenFreqAfter;
        doCalibration = doCalibration || (clockGenFreqAfter != clockGenFreq);
    }

    if (doCalibration || doLPCalibration)
    {
        if (m_limeSDRInputThread && m_limeSDRInputThread->isRunning())
        {
            m_limeSDRInputThread->stopWork();
            ownThreadWasRunning = true;
        }

        suspendRxBuddies();
        suspendTxBuddies();

        if (doCalibration)
        {
            double bw = std::max((double)m_settings.m_devSampleRate, 2500000.0); // Min supported calibration bandwidth is 2.5MHz
            bool calibrationOK = LMS_Calibrate(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    bw,
                    0) == 0;
            if (!calibrationOK) {
                qCritical("LimeSDRInput::applySettings: calibration failed on Rx channel %d", m_deviceShared.m_channel);
            } else {
                qDebug("LimeSDRInput::applySettings: calibration successful on Rx channel %d", m_deviceShared.m_channel);
            }
            if (m_guiMessageQueue) {
                m_guiMessageQueue->push(MsgCalibrationResult::create(calibrationOK));
            }
        }

        if (doLPCalibration)
        {
            if (LMS_SetLPFBW(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    m_settings.m_lpfBW) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could not set LPF to %f Hz", m_settings.m_lpfBW);
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: LPF set to %f Hz", m_settings.m_lpfBW);
            }
        }

        resumeTxBuddies();
        resumeRxBuddies();

        if (ownThreadWasRunning) {
            m_limeSDRInputThread->startWork();
        }
    }

    // forward changes to buddies or oneself

    if (forwardChangeAllDSP)
    {
        qDebug("LimeSDRInput::applySettings: forward change to all buddies");

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(
                m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim),
                m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        // send to source buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeRxDSP)
    {
        qDebug("LimeSDRInput::applySettings: forward change to Rx buddies");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim);
        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        // send to source buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeOwnDSP)
    {
        qDebug("LimeSDRInput::applySettings: forward change to self only");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim);
        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (forwardClockSource)
    {
        // send to source buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportClockSourceChange *report = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                    m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportClockSourceChange *report = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                    m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }

    if (forwardGPIOChange)
    {
        // send to source buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportClockSourceChange *report = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                    m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportClockSourceChange *report = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                    m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }

    QLocale loc;

    qDebug().noquote() << "LimeSDRInput::applySettings: center freq: "
        << " force: " << force
        << " forceNCOFrequency: " << forceNCOFrequency
        << " doCalibration: " << doCalibration
        << " doLPCalibration: " << doLPCalibration;

    return true;
}

int LimeSDRInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setLimeSdrInputSettings(new SWGSDRangel::SWGLimeSdrInputSettings());
    response.getLimeSdrInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int LimeSDRInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    LimeSDRInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureLimeSDR *msg = MsgConfigureLimeSDR::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLimeSDR *msgToGUI = MsgConfigureLimeSDR::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void LimeSDRInput::webapiUpdateDeviceSettings(
        LimeSDRInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("antennaPath")) {
        settings.m_antennaPath = (LimeSDRInputSettings::PathRFE) response.getLimeSdrInputSettings()->getAntennaPath();
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getLimeSdrInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getLimeSdrInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getLimeSdrInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("extClock")) {
        settings.m_extClock = response.getLimeSdrInputSettings()->getExtClock() != 0;
    }
    if (deviceSettingsKeys.contains("extClockFreq")) {
        settings.m_extClockFreq = response.getLimeSdrInputSettings()->getExtClockFreq();
    }
    if (deviceSettingsKeys.contains("gain")) {
        settings.m_gain = response.getLimeSdrInputSettings()->getGain();
    }
    if (deviceSettingsKeys.contains("gainMode")) {
        settings.m_gainMode = (LimeSDRInputSettings::GainMode) response.getLimeSdrInputSettings()->getGainMode();
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getLimeSdrInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("lnaGain")) {
        settings.m_lnaGain = response.getLimeSdrInputSettings()->getLnaGain();
    }
    if (deviceSettingsKeys.contains("log2HardDecim")) {
        settings.m_log2HardDecim = response.getLimeSdrInputSettings()->getLog2HardDecim();
    }
    if (deviceSettingsKeys.contains("log2SoftDecim")) {
        settings.m_log2SoftDecim = response.getLimeSdrInputSettings()->getLog2SoftDecim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getLimeSdrInputSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("lpfBW")) {
        settings.m_lpfBW = response.getLimeSdrInputSettings()->getLpfBw();
    }
    if (deviceSettingsKeys.contains("lpfFIREnable")) {
        settings.m_lpfFIREnable = response.getLimeSdrInputSettings()->getLpfFirEnable() != 0;
    }
    if (deviceSettingsKeys.contains("lpfFIRBW")) {
        settings.m_lpfFIRBW = response.getLimeSdrInputSettings()->getLpfFirbw();
    }
    if (deviceSettingsKeys.contains("ncoEnable")) {
        settings.m_ncoEnable = response.getLimeSdrInputSettings()->getNcoEnable() != 0;
    }
    if (deviceSettingsKeys.contains("ncoFrequency")) {
        settings.m_ncoFrequency = response.getLimeSdrInputSettings()->getNcoFrequency();
    }
    if (deviceSettingsKeys.contains("pgaGain")) {
        settings.m_pgaGain = response.getLimeSdrInputSettings()->getPgaGain();
    }
    if (deviceSettingsKeys.contains("tiaGain")) {
        settings.m_tiaGain = response.getLimeSdrInputSettings()->getTiaGain();
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getLimeSdrInputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getLimeSdrInputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("gpioDir")) {
        settings.m_gpioDir = response.getLimeSdrInputSettings()->getGpioDir() & 0xFF;
    }
    if (deviceSettingsKeys.contains("gpioPins")) {
        settings.m_gpioPins = response.getLimeSdrInputSettings()->getGpioPins() & 0xFF;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLimeSdrInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLimeSdrInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLimeSdrInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLimeSdrInputSettings()->getReverseApiDeviceIndex();
    }
}

void LimeSDRInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const LimeSDRInputSettings& settings)
{
    response.getLimeSdrInputSettings()->setAntennaPath((int) settings.m_antennaPath);
    response.getLimeSdrInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getLimeSdrInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getLimeSdrInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getLimeSdrInputSettings()->setExtClock(settings.m_extClock ? 1 : 0);
    response.getLimeSdrInputSettings()->setExtClockFreq(settings.m_extClockFreq);
    response.getLimeSdrInputSettings()->setGain(settings.m_gain);
    response.getLimeSdrInputSettings()->setGainMode((int) settings.m_gainMode);
    response.getLimeSdrInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getLimeSdrInputSettings()->setLnaGain(settings.m_lnaGain);
    response.getLimeSdrInputSettings()->setLog2HardDecim(settings.m_log2HardDecim);
    response.getLimeSdrInputSettings()->setLog2SoftDecim(settings.m_log2SoftDecim);
    response.getLimeSdrInputSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getLimeSdrInputSettings()->setLpfBw(settings.m_lpfBW);
    response.getLimeSdrInputSettings()->setLpfFirEnable(settings.m_lpfFIREnable ? 1 : 0);
    response.getLimeSdrInputSettings()->setLpfFirbw(settings.m_lpfFIRBW);
    response.getLimeSdrInputSettings()->setNcoEnable(settings.m_ncoEnable ? 1 : 0);
    response.getLimeSdrInputSettings()->setNcoFrequency(settings.m_ncoFrequency);
    response.getLimeSdrInputSettings()->setPgaGain(settings.m_pgaGain);
    response.getLimeSdrInputSettings()->setTiaGain(settings.m_tiaGain);
    response.getLimeSdrInputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getLimeSdrInputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getLimeSdrInputSettings()->setGpioDir(settings.m_gpioDir);
    response.getLimeSdrInputSettings()->setGpioPins(settings.m_gpioPins);
    response.getLimeSdrInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLimeSdrInputSettings()->getReverseApiAddress()) {
        *response.getLimeSdrInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLimeSdrInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLimeSdrInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLimeSdrInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int LimeSDRInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLimeSdrInputReport(new SWGSDRangel::SWGLimeSdrInputReport());
    response.getLimeSdrInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int LimeSDRInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int LimeSDRInput::webapiRun(
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

void LimeSDRInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    bool success = false;
    double temp = 0.0;
    uint8_t gpioDir = 0;
    uint8_t gpioPins = 0;
    lms_stream_status_t status;
    status.active = false;
    status.fifoFilledCount = 0;
    status.fifoSize = 1;
    status.underrun = 0;
    status.overrun = 0;
    status.droppedPackets = 0;
    status.linkRate = 0.0;
    status.timestamp = 0;

    success = (m_streamId.handle && (LMS_GetStreamStatus(&m_streamId, &status) == 0));

    response.getLimeSdrInputReport()->setSuccess(success ? 1 : 0);
    response.getLimeSdrInputReport()->setStreamActive(status.active ? 1 : 0);
    response.getLimeSdrInputReport()->setFifoSize(status.fifoSize);
    response.getLimeSdrInputReport()->setFifoFill(status.fifoFilledCount);
    response.getLimeSdrInputReport()->setUnderrunCount(status.underrun);
    response.getLimeSdrInputReport()->setOverrunCount(status.overrun);
    response.getLimeSdrInputReport()->setDroppedPacketsCount(status.droppedPackets);
    response.getLimeSdrInputReport()->setLinkRate(status.linkRate);
    response.getLimeSdrInputReport()->setHwTimestamp(status.timestamp);

    if (m_deviceShared.m_deviceParams->getDevice())
    {
        LMS_GetChipTemperature(m_deviceShared.m_deviceParams->getDevice(), 0, &temp);
        LMS_GPIODirRead(m_deviceShared.m_deviceParams->getDevice(), &gpioDir, 1);
        LMS_GPIORead(m_deviceShared.m_deviceParams->getDevice(), &gpioPins, 1);
    }

    response.getLimeSdrInputReport()->setTemperature(temp);
    response.getLimeSdrInputReport()->setGpioDir(gpioDir);
    response.getLimeSdrInputReport()->setGpioPins(gpioPins);
}

void LimeSDRInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const LimeSDRInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("LimeSDR"));
    swgDeviceSettings->setLimeSdrInputSettings(new SWGSDRangel::SWGLimeSdrInputSettings());
    SWGSDRangel::SWGLimeSdrInputSettings *swgLimeSdrInputSettings = swgDeviceSettings->getLimeSdrInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("antennaPath") || force) {
        swgLimeSdrInputSettings->setAntennaPath((int) settings.m_antennaPath);
    }
    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgLimeSdrInputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgLimeSdrInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgLimeSdrInputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("extClock") || force) {
        swgLimeSdrInputSettings->setExtClock(settings.m_extClock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("extClockFreq") || force) {
        swgLimeSdrInputSettings->setExtClockFreq(settings.m_extClockFreq);
    }
    if (deviceSettingsKeys.contains("gain") || force) {
        swgLimeSdrInputSettings->setGain(settings.m_gain);
    }
    if (deviceSettingsKeys.contains("gainMode") || force) {
        swgLimeSdrInputSettings->setGainMode((int) settings.m_gainMode);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgLimeSdrInputSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lnaGain") || force) {
        swgLimeSdrInputSettings->setLnaGain(settings.m_lnaGain);
    }
    if (deviceSettingsKeys.contains("log2HardDecim") || force) {
        swgLimeSdrInputSettings->setLog2HardDecim(settings.m_log2HardDecim);
    }
    if (deviceSettingsKeys.contains("log2SoftDecim") || force) {
        swgLimeSdrInputSettings->setLog2SoftDecim(settings.m_log2SoftDecim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgLimeSdrInputSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfBW") || force) {
        swgLimeSdrInputSettings->setLpfBw(settings.m_lpfBW);
    }
    if (deviceSettingsKeys.contains("lpfFIREnable") || force) {
        swgLimeSdrInputSettings->setLpfFirEnable(settings.m_lpfFIREnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfFIRBW") || force) {
        swgLimeSdrInputSettings->setLpfFirbw(settings.m_lpfFIRBW);
    }
    if (deviceSettingsKeys.contains("ncoEnable") || force) {
        swgLimeSdrInputSettings->setNcoEnable(settings.m_ncoEnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoFrequency") || force) {
        swgLimeSdrInputSettings->setNcoFrequency(settings.m_ncoFrequency);
    }
    if (deviceSettingsKeys.contains("pgaGain") || force) {
        swgLimeSdrInputSettings->setPgaGain(settings.m_pgaGain);
    }
    if (deviceSettingsKeys.contains("tiaGain") || force) {
        swgLimeSdrInputSettings->setTiaGain(settings.m_tiaGain);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgLimeSdrInputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgLimeSdrInputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("gpioDir") || force) {
        swgLimeSdrInputSettings->setGpioDir(settings.m_gpioDir & 0xFF);
    }
    if (deviceSettingsKeys.contains("gpioPins") || force) {
        swgLimeSdrInputSettings->setGpioPins(settings.m_gpioPins & 0xFF);
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

void LimeSDRInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("LimeSDR"));

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

void LimeSDRInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LimeSDRInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("LimeSDRInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
