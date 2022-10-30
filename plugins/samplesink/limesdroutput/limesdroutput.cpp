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
#include "SWGLimeSdrOutputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGLimeSdrOutputReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "limesdroutputthread.h"
#include "limesdr/devicelimesdrparam.h"
#include "limesdr/devicelimesdr.h"
#include "limesdroutput.h"

MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgConfigureLimeSDR, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgCalibrationResult, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgReportStreamInfo, Message)


LimeSDROutput::LimeSDROutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_limeSDROutputThread(nullptr),
    m_deviceDescription("LimeSDROutput"),
    m_running(false),
    m_channelAcquired(false)
{
    m_deviceAPI->setNbSinkStreams(1);
    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_settings.m_devSampleRate));
    m_streamId.handle = 0;
    suspendRxBuddies();
    suspendTxBuddies();
    openDevice();
    resumeTxBuddies();
    resumeRxBuddies();
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LimeSDROutput::networkManagerFinished
    );
}

LimeSDROutput::~LimeSDROutput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LimeSDROutput::networkManagerFinished
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

void LimeSDROutput::destroy()
{
    delete this;
}

bool LimeSDROutput::openDevice()
{
    int requestedChannel = m_deviceAPI->getDeviceItemIndex();

    // look for Tx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSinkBuddies().size() > 0) // look sink sibling first
    {
        qDebug("LimeSDROutput::openDevice: look in Ix buddies");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        //m_deviceShared = *((DeviceLimeSDRShared *) sinkBuddy->getBuddySharedPtr()); // copy shared data
        DeviceLimeSDRShared *deviceLimeSDRShared = (DeviceLimeSDRShared*) sinkBuddy->getBuddySharedPtr();
        m_deviceShared.m_deviceParams = deviceLimeSDRShared->m_deviceParams;

        DeviceLimeSDRParams *deviceParams = m_deviceShared.m_deviceParams; // get device parameters

        if (deviceParams == 0)
        {
            qCritical("LimeSDROutput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("LimeSDROutput::openDevice: getting device parameters from Tx buddy");
        }

        if (m_deviceAPI->getSinkBuddies().size() == deviceParams->m_nbTxChannels)
        {
            qCritical("LimeSDROutput::openDevice: no more Tx channels available in device");
            return false; // no more Tx channels available in device
        }
        else
        {
            qDebug("LimeSDROutput::openDevice: at least one more Tx channel is available in device");
        }

        // check if the requested channel is busy and abort if so (should not happen if device management is working correctly)

        for (unsigned int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
        {
            DeviceAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
            DeviceLimeSDRShared *buddyShared = (DeviceLimeSDRShared *) buddy->getBuddySharedPtr();

            if (buddyShared->m_channel == requestedChannel)
            {
                qCritical("LimeSDROutput::openDevice: cannot open busy channel %u", requestedChannel);
                return false;
            }
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // look for Rx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSourceBuddies().size() > 0) // then source
    {
        qDebug("LimeSDROutput::openDevice: look in Rx buddies");

        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        //m_deviceShared = *((DeviceLimeSDRShared *) sourceBuddy->getBuddySharedPtr()); // copy parameters
        DeviceLimeSDRShared *deviceLimeSDRShared = (DeviceLimeSDRShared*) sourceBuddy->getBuddySharedPtr();
        m_deviceShared.m_deviceParams = deviceLimeSDRShared->m_deviceParams;

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("LimeSDROutput::openDevice: cannot get device parameters from Rx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("LimeSDROutput::openDevice: getting device parameters from Rx buddy");
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // There are no buddies then create the first LimeSDR common parameters
    // open the device this will also populate common fields
    // take the first Tx channel
    else
    {
        qDebug("LimeSDROutput::openDevice: open device here");

        m_deviceShared.m_deviceParams = new DeviceLimeSDRParams();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));
        m_deviceShared.m_deviceParams->open(serial);
        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    return true;
}

void LimeSDROutput::suspendRxBuddies()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("LimeSDROutput::suspendRxBuddies (%lu)", sourceBuddies.size());

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

void LimeSDROutput::suspendTxBuddies()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("LimeSDROutput::suspendTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();

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

void LimeSDROutput::resumeRxBuddies()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("LimeSDROutput::resumeRxBuddies (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSource)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void LimeSDROutput::resumeTxBuddies()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("LimeSDROutput::resumeTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void LimeSDROutput::closeDevice()
{
    if (m_deviceShared.m_deviceParams->getDevice() == 0) { // was never open
        return;
    }

    if (m_running) stop();

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSourceBuddies().size() == 0) && (m_deviceAPI->getSinkBuddies().size() == 0))
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }

    m_deviceShared.m_channel = -1; // effectively release the channel for the possible buddies
}

bool LimeSDROutput::acquireChannel()
{
    suspendRxBuddies();
    suspendTxBuddies();

    // acquire the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_TX, m_deviceShared.m_channel, true) != 0)
    {
        qCritical("LimeSDROutput::acquireChannel: cannot enable Tx channel %d", m_deviceShared.m_channel);
        return false;
    }
    else
    {
        qDebug("LimeSDROutput::acquireChannel: Tx channel %d enabled", m_deviceShared.m_channel);
    }

    // set up the stream

    m_streamId.channel =  m_deviceShared.m_channel; // channel number
    m_streamId.fifoSize = 1024 * 256;               // fifo size in samples
    m_streamId.throughputVsLatency = 0.5;           // optimize for min latency
    m_streamId.isTx = true;                         // TX channel
    m_streamId.dataFmt = lms_stream_t::LMS_FMT_I12; // 12-bit integers

    if (LMS_SetupStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId) != 0)
    {
        qCritical("LimeSDROutput::acquireChannel: cannot setup the stream on Tx channel %d", m_deviceShared.m_channel);
        resumeTxBuddies();
        resumeRxBuddies();
        return false;
    }
    else
    {
        qDebug("LimeSDROutput::acquireChannel: stream set up on Tx channel %d", m_deviceShared.m_channel);
    }

    resumeTxBuddies();
    resumeRxBuddies();

    m_channelAcquired = true;

    return true;
}

void LimeSDROutput::releaseChannel()
{
    suspendRxBuddies();
    suspendTxBuddies();

    // destroy the stream

    if (LMS_DestroyStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId) != 0)
    {
        qWarning("LimeSDROutput::releaseChannel: cannot destroy the stream on Tx channel %d", m_deviceShared.m_channel);
    }
    else
    {
        qDebug("LimeSDROutput::releaseChannel: stream destroyed on Tx channel %d", m_deviceShared.m_channel);
    }

    m_streamId.handle = 0;

    // release the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_TX, m_deviceShared.m_channel, false) != 0)
    {
        qWarning("LimeSDROutput::releaseChannel: cannot disable Tx channel %d", m_deviceShared.m_channel);
    }
    else
    {
        qDebug("LimeSDROutput::releaseChannel: Tx channel %d released", m_deviceShared.m_channel);
    }

    resumeTxBuddies();
    resumeRxBuddies();

    m_channelAcquired = false;
}

void LimeSDROutput::init()
{
    applySettings(m_settings, QList<QString>(), true, false);
}

bool LimeSDROutput::start()
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

    m_limeSDROutputThread = new LimeSDROutputThread(&m_streamId, &m_sampleSourceFifo);
    qDebug("LimeSDROutput::start: thread created");

    applySettings(m_settings, QList<QString>(), true);

    m_limeSDROutputThread->setLog2Interpolation(m_settings.m_log2SoftInterp);
    m_limeSDROutputThread->startWork();

    m_deviceShared.m_thread = m_limeSDROutputThread;
    m_running = true;

    return true;
}

void LimeSDROutput::stop()
{
    qDebug("LimeSDROutput::stop");

    if (m_limeSDROutputThread)
    {
        m_limeSDROutputThread->stopWork();
        delete m_limeSDROutputThread;
        m_limeSDROutputThread = 0;
    }

    m_deviceShared.m_thread = 0;
    m_running = false;

    releaseChannel();
}

QByteArray LimeSDROutput::serialize() const
{
    return m_settings.serialize();
}

bool LimeSDROutput::deserialize(const QByteArray& data)
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

const QString& LimeSDROutput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int LimeSDROutput::getSampleRate() const
{
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2SoftInterp));
}

quint64 LimeSDROutput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency + (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);
}

void LimeSDROutput::setCenterFrequency(qint64 centerFrequency)
{
    LimeSDROutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency - (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);

    MsgConfigureLimeSDR* message = MsgConfigureLimeSDR::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDR* messageToGUI = MsgConfigureLimeSDR::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

std::size_t LimeSDROutput::getChannelIndex()
{
    return m_deviceShared.m_channel;
}

void LimeSDROutput::getLORange(float& minF, float& maxF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_loRangeTx;
    minF = range.min;
    maxF = range.max;
    qDebug("LimeSDROutput::getLORange: min: %f max: %f", range.min, range.max);
}

void LimeSDROutput::getSRRange(float& minF, float& maxF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_srRangeTx;
    minF = range.min;
    maxF = range.max;
    qDebug("LimeSDROutput::getSRRange: min: %f max: %f", range.min, range.max);
}

void LimeSDROutput::getLPRange(float& minF, float& maxF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_lpfRangeTx;
    minF = range.min;
    maxF = range.max;
    qDebug("LimeSDROutput::getLPRange: min: %f max: %f", range.min, range.max);
}

uint32_t LimeSDROutput::getHWLog2Interp() const
{
    return m_deviceShared.m_deviceParams->m_log2OvSRTx;
}

DeviceLimeSDRParams::LimeType LimeSDROutput::getLimeType() const
{
    if (m_deviceShared.m_deviceParams) {
        return m_deviceShared.m_deviceParams->m_type;
    } else {
        return DeviceLimeSDRParams::LimeUndefined;
    }
}

bool LimeSDROutput::handleMessage(const Message& message)
{
    if (MsgConfigureLimeSDR::match(message))
    {
        MsgConfigureLimeSDR& conf = (MsgConfigureLimeSDR&) message;
        qDebug() << "LimeSDROutput::handleMessage: MsgConfigureLimeSDR";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce())) {
            qDebug("LimeSDROutput::handleMessage config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "LimeSDROutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine()) {
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
    else if (DeviceLimeSDRShared::MsgReportBuddyChange::match(message))
    {
        DeviceLimeSDRShared::MsgReportBuddyChange& report = (DeviceLimeSDRShared::MsgReportBuddyChange&) message;
        QList<QString> settingsKeys;

        if (report.getRxElseTx() && m_running)
        {
            double host_Hz;
            double rf_Hz;

            if (LMS_GetSampleRate(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    m_deviceShared.m_channel,
                    &host_Hz,
                    &rf_Hz) < 0)
            {
                qDebug("LimeSDROutput::handleMessage: MsgReportBuddyChange: LMS_GetSampleRate() failed");
            }
            else
            {
                m_settings.m_devSampleRate = roundf(host_Hz);
                int hard = roundf(rf_Hz) / m_settings.m_devSampleRate;
                m_settings.m_log2HardInterp = log2(hard);
                settingsKeys.append("devSampleRate");
                settingsKeys.append("log2HardInterp");

                qDebug() << "LimeSDROutput::handleMessage: MsgReportBuddyChange:"
                         << " host_Hz: " << host_Hz
                         << " rf_Hz: " << rf_Hz
                         << " m_devSampleRate: " << m_settings.m_devSampleRate
                         << " log2Hard: " << hard
                         << " m_log2HardInterp: " << m_settings.m_log2HardInterp;
            }

        }
        else
        {
            m_settings.m_devSampleRate   = report.getDevSampleRate();
            m_settings.m_log2HardInterp  = report.getLog2HardDecimInterp();
            m_settings.m_centerFrequency = report.getCenterFrequency();
            settingsKeys.append("devSampleRate");
            settingsKeys.append("log2HardInterp");
            settingsKeys.append("centerFrequency");
        }

        if (m_settings.m_ncoEnable) // need to reset NCO after sample rate change
        {
            applySettings(m_settings, settingsKeys, false, true);
        }

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        DSPSignalNotification *notif = new DSPSignalNotification(
                m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp),
                m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        DeviceLimeSDRShared::MsgReportBuddyChange *reportToGUI = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_log2HardInterp, m_settings.m_centerFrequency, false);
        getMessageQueueToGUI()->push(reportToGUI);

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

        m_settings.m_gpioDir  = report.getGPIODir();
        m_settings.m_gpioPins = report.getGPIOPins();

        // no GUI for the moment only REST API

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
//        qDebug() << "LimeSDROutput::handleMessage: MsgGetStreamInfo";
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
            qDebug("LimeSDROutput::handleMessage: MsgGetDeviceInfo: cannot get temperature");
        }

        if ((m_deviceShared.m_deviceParams->m_type != DeviceLimeSDRParams::LimeMini)
            && (m_deviceShared.m_deviceParams->m_type != DeviceLimeSDRParams::LimeUndefined))
        {
            if (m_deviceShared.m_deviceParams->getDevice() && (LMS_GPIORead(m_deviceShared.m_deviceParams->getDevice(), &gpioPins, 1) != 0)) {
                qDebug("LimeSDROutput::handleMessage: MsgGetDeviceInfo: cannot get GPIO pins values");
            }
        }

        // send to oneself
        if (getMessageQueueToGUI())
        {
            DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp, gpioPins);
            getMessageQueueToGUI()->push(report);
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
    else
    {
        return false;
    }
}

bool LimeSDROutput::applySettings(const LimeSDROutputSettings& settings, const QList<QString>& settingsKeys, bool force, bool forceNCOFrequency)
{
    qDebug().noquote() << "LimeSDROutput::applySettings: force:" << force
        << " forceNCOFrequency:" << forceNCOFrequency
        << settings.getDebugString(settingsKeys, force);
    bool forwardChangeOwnDSP = false;
    bool forwardChangeTxDSP  = false;
    bool forwardChangeAllDSP = false;
    bool forwardClockSource  = false;
    bool forwardGPIOChange   = false;
    bool ownThreadWasRunning = false;
    bool doCalibration       = false;
    bool doLPCalibration     = false;
    double clockGenFreq      = 0.0;
//  QMutexLocker mutexLocker(&m_mutex);

    qint64 deviceCenterFrequency = settings.m_centerFrequency;
    deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
    deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;

    if (LMS_GetClockFreq(m_deviceShared.m_deviceParams->getDevice(), LMS_CLOCK_CGEN, &clockGenFreq) != 0) {
        qCritical("LimeSDROutput::applySettings: could not get clock gen frequency");
    } else {
        qDebug() << "LimeSDROutput::applySettings: clock gen frequency: " << clockGenFreq;
    }

    // apply settings

    if (settingsKeys.contains("gain") || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (LMS_SetGaindB(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    m_deviceShared.m_channel,
                    settings.m_gain) < 0)
            {
                qDebug("LimeSDROutput::applySettings: LMS_SetGaindB() failed");
            }
            else
            {
                doCalibration = true;
                qDebug() << "LimeSDROutput::applySettings: Gain set to " << settings.m_gain;
            }
        }
    }

    if (settingsKeys.contains("devSampleRate")
       || settingsKeys.contains("log2HardInterp") || force)
    {
        forwardChangeAllDSP = true; //m_settings.m_devSampleRate != settings.m_devSampleRate;

        if (m_deviceShared.m_deviceParams->getDevice())
        {
            if (LMS_SetSampleRateDir(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    settings.m_devSampleRate,
                    1<<settings.m_log2HardInterp) < 0)
            {
                qCritical("LimeSDROutput::applySettings: could not set sample rate to %d with oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardInterp);
            }
            else
            {
                m_deviceShared.m_deviceParams->m_log2OvSRTx = settings.m_log2HardInterp;
                m_deviceShared.m_deviceParams->m_sampleRate = settings.m_devSampleRate;
                //doCalibration = true;
                forceNCOFrequency = true;
                qDebug("LimeSDROutput::applySettings: set sample rate set to %d with oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardInterp);
            }
        }
    }

    if (settingsKeys.contains("devSampleRate")
       || settingsKeys.contains("log2SoftInterp") || force)
    {
#if defined(_MSC_VER)
        unsigned int fifoRate = (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftInterp);
        fifoRate = fifoRate < 48000U ? 48000U : fifoRate;
#else
        unsigned int fifoRate = std::max(
            (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftInterp),
            DeviceLimeSDRShared::m_sampleFifoMinRate);
#endif
        m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(fifoRate));
        qDebug("LimeSDROutput::applySettings: resize FIFO: rate %u", fifoRate);
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
                    LMS_CH_TX,
                    m_deviceShared.m_channel,
                    settings.m_lpfFIREnable,
                    settings.m_lpfFIRBW) < 0)
            {
                qCritical("LimeSDROutput::applySettings: could %s and set LPF FIR to %f Hz",
                        settings.m_lpfFIREnable ? "enable" : "disable",
                        settings.m_lpfFIRBW);
            }
            else
            {
                //doCalibration = true;
                qDebug("LimeSDROutput::applySettings: %sd and set LPF FIR to %f Hz",
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
                    LMS_CH_TX,
                    m_deviceShared.m_channel,
                    settings.m_ncoEnable,
                    settings.m_ncoFrequency))
            {
                //doCalibration = true;
                m_deviceShared.m_ncoFrequency = settings.m_ncoEnable ? settings.m_ncoFrequency : 0; // for buddies
                qDebug("LimeSDROutput::applySettings: %sd and set NCO to %d Hz",
                        settings.m_ncoEnable ? "enable" : "disable",
                        settings.m_ncoFrequency);
            }
            else
            {
                qCritical("LimeSDROutput::applySettings: could not %s and set NCO to %d Hz",
                        settings.m_ncoEnable ? "enable" : "disable",
                        settings.m_ncoFrequency);
            }
        }
    }

    if (settingsKeys.contains("log2SoftInterp") || force)
    {
        forwardChangeOwnDSP = true;
        m_deviceShared.m_log2Soft = settings.m_log2SoftInterp; // for buddies

        if (m_limeSDROutputThread)
        {
            m_limeSDROutputThread->setLog2Interpolation(settings.m_log2SoftInterp);
            qDebug() << "LimeSDROutput::applySettings: set soft interpolation to " << (1<<settings.m_log2SoftInterp);
        }
    }

    if (settingsKeys.contains("antennaPath") || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (DeviceLimeSDR::setTxAntennaPath(m_deviceShared.m_deviceParams->getDevice(),
                    m_deviceShared.m_channel,
                    settings.m_antennaPath))
            {
                doCalibration = true;
                qDebug("LimeSDROutput::applySettings: set antenna path to %d",
                        (int) settings.m_antennaPath);
            }
            else
            {
                qCritical("LimeSDROutput::applySettings: could not set antenna path to %d",
                        (int) settings.m_antennaPath);
            }
        }
    }

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("transverterMode")
        || settingsKeys.contains("transverterDeltaFrequency")
        || force)
    {
        forwardChangeTxDSP = true;

        if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
        {
            if (LMS_SetClockFreq(m_deviceShared.m_deviceParams->getDevice(), LMS_CLOCK_SXT, deviceCenterFrequency) < 0)
            {
                qCritical("LimeSDROutput::applySettings: could not set frequency to %lld", deviceCenterFrequency);
            }
            else
            {
                doCalibration = true;
                m_deviceShared.m_centerFrequency = deviceCenterFrequency; // for buddies
                qDebug("LimeSDROutput::applySettings: frequency set to %lld", deviceCenterFrequency);
            }
        }
    }

    if (settingsKeys.contains("extClock") ||
        settingsKeys.contains("extClockFreq") || force)
    {
        if (DeviceLimeSDR::setClockSource(m_deviceShared.m_deviceParams->getDevice(),
                settings.m_extClock,
                settings.m_extClockFreq))
        {
            forwardClockSource = true;
            doCalibration = true;
            qDebug("LimeSDROutput::applySettings: clock set to %s (Ext: %d Hz)",
                    settings.m_extClock ? "external" : "internal",
                    settings.m_extClockFreq);
        }
        else
        {
            qCritical("LimeSDROutput::applySettings: could not set clock to %s (Ext: %d Hz)",
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
                qCritical("LimeSDROutput::applySettings: could not set GPIO directions to %u", settings.m_gpioDir);
            }
            else
            {
                forwardGPIOChange = true;
                qDebug("LimeSDROutput::applySettings: GPIO directions set to %u", settings.m_gpioDir);
            }
        }

        if (settingsKeys.contains("gpioPins") || force)
        {
            if (LMS_GPIOWrite(m_deviceShared.m_deviceParams->getDevice(), &settings.m_gpioPins, 1) != 0)
            {
                qCritical("LimeSDROutput::applySettings: could not set GPIO pins to %u", settings.m_gpioPins);
            }
            else
            {
                forwardGPIOChange = true;
                qDebug("LimeSDROutput::applySettings: GPIO pins set to %u", settings.m_gpioPins);
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
        qCritical("LimeSDROutput::applySettings: could not get clock gen frequency");
    }
    else
    {
        qDebug() << "LimeSDROutput::applySettings: clock gen frequency after: " << clockGenFreqAfter;
        doCalibration = doCalibration || (clockGenFreqAfter != clockGenFreq);
    }

    if ((doCalibration || doLPCalibration) && m_channelAcquired)
    {
        if (m_limeSDROutputThread && m_limeSDROutputThread->isRunning())
        {
            m_limeSDROutputThread->stopWork();
            ownThreadWasRunning = true;
        }

        suspendRxBuddies();
        suspendTxBuddies();

        if (doCalibration)
        {
            double bw = std::max((double)m_settings.m_devSampleRate, 2500000.0); // Min supported calibration bandwidth is 2.5MHz
            bool calibrationOK = LMS_Calibrate(m_deviceShared.m_deviceParams->getDevice(),
                                                LMS_CH_TX,
                                                m_deviceShared.m_channel,
                                                bw,
                                                0) == 0;
            if (!calibrationOK) {
                qCritical("LimeSDROutput::applySettings: calibration failed on Tx channel %d", m_deviceShared.m_channel);
            } else {
                qDebug("LimeSDROutput::applySettings: calibration successful on Tx channel %d", m_deviceShared.m_channel);
            }
            if (m_guiMessageQueue) {
                m_guiMessageQueue->push(MsgCalibrationResult::create(calibrationOK));
            }
        }

        if (doLPCalibration)
        {
            if (LMS_SetLPFBW(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    m_deviceShared.m_channel,
                    m_settings.m_lpfBW) < 0)
            {
                qCritical("LimeSDROutput::applySettings: could not set LPF to %f Hz", m_settings.m_lpfBW);
            }
            else
            {
                qDebug("LimeSDROutput::applySettings: LPF set to %f Hz", m_settings.m_lpfBW);
            }
        }

        resumeTxBuddies();
        resumeRxBuddies();

        if (ownThreadWasRunning) {
            m_limeSDROutputThread->startWork();
        }
    }

    // forward changes to buddies or oneself

    if (forwardChangeAllDSP)
    {
        qDebug("LimeSDROutput::applySettings: forward change to all buddies");

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(
                m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp),
                m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardInterp, m_settings.m_centerFrequency, false);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
        }

        // send to source buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardInterp, m_settings.m_centerFrequency, false);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeTxDSP)
    {
        qDebug("LimeSDROutput::applySettings: forward change to Tx buddies");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp);
        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardInterp, m_settings.m_centerFrequency, false);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeOwnDSP)
    {
        qDebug("LimeSDROutput::applySettings: forward change to self only");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp);
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
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportGPIOChange *report = DeviceLimeSDRShared::MsgReportGPIOChange::create(
                    m_settings.m_gpioDir, m_settings.m_gpioPins);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportGPIOChange *report = DeviceLimeSDRShared::MsgReportGPIOChange::create(
                    m_settings.m_gpioDir, m_settings.m_gpioPins);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }

    return true;
}

int LimeSDROutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setLimeSdrOutputSettings(new SWGSDRangel::SWGLimeSdrOutputSettings());
    response.getLimeSdrOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int LimeSDROutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    LimeSDROutputSettings settings = m_settings;
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

void LimeSDROutput::webapiUpdateDeviceSettings(
        LimeSDROutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("antennaPath")) {
        settings.m_antennaPath = (LimeSDROutputSettings::PathRFE) response.getLimeSdrOutputSettings()->getAntennaPath();
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getLimeSdrOutputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getLimeSdrOutputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("extClock")) {
        settings.m_extClock = response.getLimeSdrOutputSettings()->getExtClock() != 0;
    }
    if (deviceSettingsKeys.contains("extClockFreq")) {
        settings.m_extClockFreq = response.getLimeSdrOutputSettings()->getExtClockFreq();
    }
    if (deviceSettingsKeys.contains("gain")) {
        settings.m_gain = response.getLimeSdrOutputSettings()->getGain();
    }
    if (deviceSettingsKeys.contains("log2HardInterp")) {
        settings.m_log2HardInterp = response.getLimeSdrOutputSettings()->getLog2HardInterp();
    }
    if (deviceSettingsKeys.contains("log2SoftInterp")) {
        settings.m_log2SoftInterp = response.getLimeSdrOutputSettings()->getLog2SoftInterp();
    }
    if (deviceSettingsKeys.contains("lpfBW")) {
        settings.m_lpfBW = response.getLimeSdrOutputSettings()->getLpfBw();
    }
    if (deviceSettingsKeys.contains("lpfFIREnable")) {
        settings.m_lpfFIREnable = response.getLimeSdrOutputSettings()->getLpfFirEnable() != 0;
    }
    if (deviceSettingsKeys.contains("lpfFIRBW")) {
        settings.m_lpfFIRBW = response.getLimeSdrOutputSettings()->getLpfFirbw();
    }
    if (deviceSettingsKeys.contains("ncoEnable")) {
        settings.m_ncoEnable = response.getLimeSdrOutputSettings()->getNcoEnable() != 0;
    }
    if (deviceSettingsKeys.contains("ncoFrequency")) {
        settings.m_ncoFrequency = response.getLimeSdrOutputSettings()->getNcoFrequency();
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getLimeSdrOutputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getLimeSdrOutputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("gpioDir")) {
        settings.m_gpioDir = response.getLimeSdrOutputSettings()->getGpioDir() & 0xFF;
    }
    if (deviceSettingsKeys.contains("gpioPins")) {
        settings.m_gpioPins = response.getLimeSdrOutputSettings()->getGpioPins() & 0xFF;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLimeSdrOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLimeSdrOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLimeSdrOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLimeSdrOutputSettings()->getReverseApiDeviceIndex();
    }
}

int LimeSDROutput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLimeSdrOutputReport(new SWGSDRangel::SWGLimeSdrOutputReport());
    response.getLimeSdrOutputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}
void LimeSDROutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const LimeSDROutputSettings& settings)
{
    response.getLimeSdrOutputSettings()->setAntennaPath((int) settings.m_antennaPath);
    response.getLimeSdrOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getLimeSdrOutputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getLimeSdrOutputSettings()->setExtClock(settings.m_extClock ? 1 : 0);
    response.getLimeSdrOutputSettings()->setExtClockFreq(settings.m_extClockFreq);
    response.getLimeSdrOutputSettings()->setGain(settings.m_gain);
    response.getLimeSdrOutputSettings()->setLog2HardInterp(settings.m_log2HardInterp);
    response.getLimeSdrOutputSettings()->setLog2SoftInterp(settings.m_log2SoftInterp);
    response.getLimeSdrOutputSettings()->setLpfBw(settings.m_lpfBW);
    response.getLimeSdrOutputSettings()->setLpfFirEnable(settings.m_lpfFIREnable ? 1 : 0);
    response.getLimeSdrOutputSettings()->setLpfFirbw(settings.m_lpfFIRBW);
    response.getLimeSdrOutputSettings()->setNcoEnable(settings.m_ncoEnable ? 1 : 0);
    response.getLimeSdrOutputSettings()->setNcoFrequency(settings.m_ncoFrequency);
    response.getLimeSdrOutputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getLimeSdrOutputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getLimeSdrOutputSettings()->setGpioDir(settings.m_gpioDir);
    response.getLimeSdrOutputSettings()->setGpioPins(settings.m_gpioPins);
    response.getLimeSdrOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLimeSdrOutputSettings()->getReverseApiAddress()) {
        *response.getLimeSdrOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLimeSdrOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLimeSdrOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLimeSdrOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int LimeSDROutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int LimeSDROutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgStartStop *messagetoGui = MsgStartStop::create(run);
        m_guiMessageQueue->push(messagetoGui);
    }

    return 200;
}

void LimeSDROutput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
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

    response.getLimeSdrOutputReport()->setSuccess(success ? 1 : 0);
    response.getLimeSdrOutputReport()->setStreamActive(status.active ? 1 : 0);
    response.getLimeSdrOutputReport()->setFifoSize(status.fifoSize);
    response.getLimeSdrOutputReport()->setFifoFill(status.fifoFilledCount);
    response.getLimeSdrOutputReport()->setUnderrunCount(status.underrun);
    response.getLimeSdrOutputReport()->setOverrunCount(status.overrun);
    response.getLimeSdrOutputReport()->setDroppedPacketsCount(status.droppedPackets);
    response.getLimeSdrOutputReport()->setLinkRate(status.linkRate);
    response.getLimeSdrOutputReport()->setHwTimestamp(status.timestamp);

    if (m_deviceShared.m_deviceParams->getDevice())
    {
        LMS_GetChipTemperature(m_deviceShared.m_deviceParams->getDevice(), 0, &temp);
        LMS_GPIODirRead(m_deviceShared.m_deviceParams->getDevice(), &gpioDir, 1);
        LMS_GPIORead(m_deviceShared.m_deviceParams->getDevice(), &gpioPins, 1);
    }

    response.getLimeSdrOutputReport()->setTemperature(temp);
    response.getLimeSdrOutputReport()->setGpioDir(gpioDir);
    response.getLimeSdrOutputReport()->setGpioPins(gpioPins);
}

void LimeSDROutput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const LimeSDROutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("LimeSDR"));
    swgDeviceSettings->setLimeSdrOutputSettings(new SWGSDRangel::SWGLimeSdrOutputSettings());
    SWGSDRangel::SWGLimeSdrOutputSettings *swgLimeSdrOutputSettings = swgDeviceSettings->getLimeSdrOutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("antennaPath") || force) {
        swgLimeSdrOutputSettings->setAntennaPath((int) settings.m_antennaPath);
    }
    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgLimeSdrOutputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgLimeSdrOutputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("extClock") || force) {
        swgLimeSdrOutputSettings->setExtClock(settings.m_extClock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("extClockFreq") || force) {
        swgLimeSdrOutputSettings->setExtClockFreq(settings.m_extClockFreq);
    }
    if (deviceSettingsKeys.contains("gain") || force) {
        swgLimeSdrOutputSettings->setGain(settings.m_gain);
    }
    if (deviceSettingsKeys.contains("log2HardInterp") || force) {
        swgLimeSdrOutputSettings->setLog2HardInterp(settings.m_log2HardInterp);
    }
    if (deviceSettingsKeys.contains("log2SoftInterp") || force) {
        swgLimeSdrOutputSettings->setLog2SoftInterp(settings.m_log2SoftInterp);
    }
    if (deviceSettingsKeys.contains("lpfBW") || force) {
        swgLimeSdrOutputSettings->setLpfBw(settings.m_lpfBW);
    }
    if (deviceSettingsKeys.contains("lpfFIREnable") || force) {
        swgLimeSdrOutputSettings->setLpfFirEnable(settings.m_lpfFIREnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lpfFIRBW") || force) {
        swgLimeSdrOutputSettings->setLpfFirbw(settings.m_lpfFIRBW);
    }
    if (deviceSettingsKeys.contains("ncoEnable") || force) {
        swgLimeSdrOutputSettings->setNcoEnable(settings.m_ncoEnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("ncoFrequency") || force) {
        swgLimeSdrOutputSettings->setNcoFrequency(settings.m_ncoFrequency);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgLimeSdrOutputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgLimeSdrOutputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("gpioDir") || force) {
        swgLimeSdrOutputSettings->setGpioDir(settings.m_gpioDir & 0xFF);
    }
    if (deviceSettingsKeys.contains("gpioPins") || force) {
        swgLimeSdrOutputSettings->setGpioPins(settings.m_gpioPins & 0xFF);
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

void LimeSDROutput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
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

void LimeSDROutput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LimeSDROutput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("LimeSDROutput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
