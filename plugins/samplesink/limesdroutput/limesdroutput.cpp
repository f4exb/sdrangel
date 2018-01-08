///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include "lime/LimeSuite.h"

#include "SWGDeviceSettings.h"
#include "SWGLimeSdrOutputSettings.h"
#include "SWGDeviceState.h"

#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "limesdroutputthread.h"
#include "limesdr/devicelimesdrparam.h"
#include "limesdr/devicelimesdr.h"
#include "limesdroutput.h"

MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgConfigureLimeSDR, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgReportStreamInfo, Message)


LimeSDROutput::LimeSDROutput(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_limeSDROutputThread(0),
    m_deviceDescription("LimeSDROutput"),
    m_running(false),
    m_channelAcquired(false)
{
    m_sampleSourceFifo.resize(16*LIMESDROUTPUT_BLOCKSIZE);
    m_streamId.handle = 0;
    suspendRxBuddies();
    suspendTxBuddies();
    openDevice();
    resumeTxBuddies();
    resumeRxBuddies();
}

LimeSDROutput::~LimeSDROutput()
{
    if (m_running) stop();
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
    int requestedChannel = m_deviceAPI->getItemIndex();

    // look for Tx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSinkBuddies().size() > 0) // look sink sibling first
    {
        qDebug("LimeSDROutput::openDevice: look in Ix buddies");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
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

        char *busyChannels = new char[deviceParams->m_nbTxChannels];
        memset(busyChannels, 0, deviceParams->m_nbTxChannels);

        for (unsigned int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
        {
            DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
            DeviceLimeSDRShared *buddyShared = (DeviceLimeSDRShared *) buddy->getBuddySharedPtr();

            if (buddyShared->m_channel == requestedChannel)
            {
                qCritical("LimeSDROutput::openDevice: cannot open busy channel %u", requestedChannel);
                delete[] busyChannels;
                return false;
            }
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
        delete[] busyChannels;
    }
    // look for Rx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSourceBuddies().size() > 0) // then source
    {
        qDebug("LimeSDROutput::openDevice: look in Rx buddies");

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
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
        strcpy(serial, qPrintable(m_deviceAPI->getSampleSinkSerial()));
        m_deviceShared.m_deviceParams->open(serial);
        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    return true;
}

void LimeSDROutput::suspendRxBuddies()
{
    const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

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
    const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

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
    const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

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
    const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

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
    m_streamId.fifoSize = 1024 * 1024;              // fifo size in samples (SR / 10 take ~5MS/s)
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
    applySettings(m_settings, true, false);
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

    applySettings(m_settings, true);

    // start / stop streaming is done in the thread.

    if ((m_limeSDROutputThread = new LimeSDROutputThread(&m_streamId, &m_sampleSourceFifo)) == 0)
    {
        qFatal("LimeSDROutput::start: cannot create thread");
        stop();
        return false;
    }
    else
    {
        qDebug("LimeSDROutput::start: thread created");
    }

    m_limeSDROutputThread->setLog2Interpolation(m_settings.m_log2SoftInterp);

    m_limeSDROutputThread->startWork();

    m_deviceShared.m_thread = m_limeSDROutputThread;
    m_running = true;

    return true;
}

void LimeSDROutput::stop()
{
    qDebug("LimeSDROutput::stop");

    if (m_limeSDROutputThread != 0)
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

    MsgConfigureLimeSDR* message = MsgConfigureLimeSDR::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDR* messageToGUI = MsgConfigureLimeSDR::create(m_settings, true);
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
    return m_settings.m_centerFrequency;
}

void LimeSDROutput::setCenterFrequency(qint64 centerFrequency)
{
    LimeSDROutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureLimeSDR* message = MsgConfigureLimeSDR::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDR* messageToGUI = MsgConfigureLimeSDR::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

std::size_t LimeSDROutput::getChannelIndex()
{
    return m_deviceShared.m_channel;
}

void LimeSDROutput::getLORange(float& minF, float& maxF, float& stepF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_loRangeTx;
    minF = range.min;
    maxF = range.max;
    stepF = range.step;
    qDebug("LimeSDROutput::getLORange: min: %f max: %f step: %f", range.min, range.max, range.step);
}

void LimeSDROutput::getSRRange(float& minF, float& maxF, float& stepF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_srRangeTx;
    minF = range.min;
    maxF = range.max;
    stepF = range.step;
    qDebug("LimeSDROutput::getSRRange: min: %f max: %f step: %f", range.min, range.max, range.step);
}

void LimeSDROutput::getLPRange(float& minF, float& maxF, float& stepF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_lpfRangeTx;
    minF = range.min;
    maxF = range.max;
    stepF = range.step;
    qDebug("LimeSDROutput::getLPRange: min: %f max: %f step: %f", range.min, range.max, range.step);
}

uint32_t LimeSDROutput::getHWLog2Interp() const
{
    return m_deviceShared.m_deviceParams->m_log2OvSRTx;
}

bool LimeSDROutput::handleMessage(const Message& message)
{
    if (MsgConfigureLimeSDR::match(message))
    {
        MsgConfigureLimeSDR& conf = (MsgConfigureLimeSDR&) message;
        qDebug() << "LimeSDROutput::handleMessage: MsgConfigureLimeSDR";

        if (!applySettings(conf.getSettings(), conf.getForce()))
        {
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
            if (m_deviceAPI->initGeneration())
            {
                m_deviceAPI->startGeneration();
                DSPEngine::instance()->startAudioInput();
            }
        }
        else
        {
            m_deviceAPI->stopGeneration();
            DSPEngine::instance()->stopAudioInput();
        }

        return true;
    }
    else if (DeviceLimeSDRShared::MsgReportBuddyChange::match(message))
    {
        DeviceLimeSDRShared::MsgReportBuddyChange& report = (DeviceLimeSDRShared::MsgReportBuddyChange&) message;

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
        }

        if (m_settings.m_ncoEnable) // need to reset NCO after sample rate change
        {
            applySettings(m_settings, false, true);
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

        DeviceLimeSDRShared::MsgReportClockSourceChange *reportToGUI = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                m_settings.m_extClock, m_settings.m_extClockFreq);
        getMessageQueueToGUI()->push(reportToGUI);

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
//        qDebug() << "LimeSDROutput::handleMessage: MsgGetStreamInfo";
        lms_stream_status_t status;

        if (m_streamId.handle && (LMS_GetStreamStatus(&m_streamId, &status) == 0))
        {
            if (m_deviceAPI->getSampleSinkGUIMessageQueue())
            {
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        true, // Success
                        status.active,
                        status.fifoFilledCount,
                        status.fifoSize,
                        status.underrun,
                        status.overrun,
                        status.droppedPackets,
                        status.sampleRate,
                        status.linkRate,
                        status.timestamp);
                m_deviceAPI->getSampleSinkGUIMessageQueue()->push(report);
            }
        }
        else
        {
            if (m_deviceAPI->getSampleSinkGUIMessageQueue())
            {
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        false, // Success
                        false, // status.active,
                        0,     // status.fifoFilledCount,
                        16384, // status.fifoSize,
                        0,     // status.underrun,
                        0,     // status.overrun,
                        0,     // status.droppedPackets,
                        0,     // status.sampleRate,
                        0,     // status.linkRate,
                        0);    // status.timestamp);
                m_deviceAPI->getSampleSinkGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else if (MsgGetDeviceInfo::match(message))
    {
        double temp = 0.0;

        if (m_deviceShared.m_deviceParams->getDevice() && (LMS_GetChipTemperature(m_deviceShared.m_deviceParams->getDevice(), 0, &temp) == 0))
        {
            //qDebug("LimeSDROutput::handleMessage: MsgGetDeviceInfo: temperature: %f", temp);
        }
        else
        {
            qDebug("LimeSDROutput::handleMessage: MsgGetDeviceInfo: cannot get temperature");
        }

        // send to oneself
        if (getMessageQueueToGUI()) {
            DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp);
            getMessageQueueToGUI()->push(report);
        }

        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            if ((*itSource)->getSampleSourceGUIMessageQueue())
            {
                DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp);
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
                DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp);
                (*itSink)->getSampleSinkGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool LimeSDROutput::applySettings(const LimeSDROutputSettings& settings, bool force, bool forceNCOFrequency)
{
    bool forwardChangeOwnDSP = false;
    bool forwardChangeTxDSP  = false;
    bool forwardChangeAllDSP = false;
    bool forwardClockSource  = false;
    bool ownThreadWasRunning = false;
    bool doCalibration       = false;
    bool doLPCalibration     = false;
    double clockGenFreq      = 0.0;
//  QMutexLocker mutexLocker(&m_mutex);

    if (LMS_GetClockFreq(m_deviceShared.m_deviceParams->getDevice(), LMS_CLOCK_CGEN, &clockGenFreq) != 0)
    {
        qCritical("LimeSDROutput::applySettings: could not get clock gen frequency");
    }
    else
    {
        qDebug() << "LimeSDROutput::applySettings: clock gen frequency: " << clockGenFreq;
    }

    // apply settings

    if ((m_settings.m_gain != settings.m_gain) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
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

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
       || (m_settings.m_log2HardInterp != settings.m_log2HardInterp) || force)
    {
        forwardChangeAllDSP = true; //m_settings.m_devSampleRate != settings.m_devSampleRate;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
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

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
       || (m_settings.m_log2SoftInterp != settings.m_log2SoftInterp) || force)
    {
        int fifoSize = std::max(
                (int) ((settings.m_devSampleRate/(1<<settings.m_log2SoftInterp)) * DeviceLimeSDRShared::m_sampleFifoLengthInSeconds),
                DeviceLimeSDRShared::m_sampleFifoMinSize);
        qDebug("LimeSDROutput::applySettings: resize FIFO: %d", fifoSize);
        m_sampleSourceFifo.resize(fifoSize);
    }

    if ((m_settings.m_lpfBW != settings.m_lpfBW) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            doLPCalibration = true;
        }
    }

    if ((m_settings.m_lpfFIRBW != settings.m_lpfFIRBW) ||
        (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
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

    if ((m_settings.m_ncoFrequency != settings.m_ncoFrequency) ||
        (m_settings.m_ncoEnable != settings.m_ncoEnable) || force || forceNCOFrequency)
    {
        forwardChangeOwnDSP = true;

        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
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

    if ((m_settings.m_log2SoftInterp != settings.m_log2SoftInterp) || force)
    {
        forwardChangeOwnDSP = true;
        m_deviceShared.m_log2Soft = settings.m_log2SoftInterp; // for buddies

        if (m_limeSDROutputThread != 0)
        {
            m_limeSDROutputThread->setLog2Interpolation(settings.m_log2SoftInterp);
            qDebug() << "LimeSDROutput::applySettings: set soft decimation to " << (1<<settings.m_log2SoftInterp);
        }
    }

    if ((m_settings.m_antennaPath != settings.m_antennaPath) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (DeviceLimeSDR::setTxAntennaPath(m_deviceShared.m_deviceParams->getDevice(),
                    m_deviceShared.m_channel,
                    settings.m_antennaPath))
            {
                doCalibration = true;
                qDebug("LimeSDRInput::applySettings: set antenna path to %d",
                        (int) settings.m_antennaPath);
            }
            else
            {
                qCritical("LimeSDRInput::applySettings: could not set antenna path to %d",
                        (int) settings.m_antennaPath);
            }
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force)
    {
        forwardChangeTxDSP = true;

        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (LMS_SetLOFrequency(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    m_deviceShared.m_channel, // same for both channels anyway but switches antenna port automatically
                    settings.m_centerFrequency) < 0)
            {
                qCritical("LimeSDROutput::applySettings: could not set frequency to %lu", settings.m_centerFrequency);
            }
            else
            {
                doCalibration = true;
                m_deviceShared.m_centerFrequency = settings.m_centerFrequency; // for buddies
                qDebug("LimeSDROutput::applySettings: frequency set to %lu", settings.m_centerFrequency);
            }
        }
    }

    if ((m_settings.m_extClock != settings.m_extClock) ||
        (settings.m_extClock && (m_settings.m_extClockFreq != settings.m_extClockFreq)) || force)
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

    m_settings = settings;
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
            if (LMS_Calibrate(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    m_deviceShared.m_channel,
                    m_settings.m_devSampleRate,
                    0) < 0)
            {
                qCritical("LimeSDROutput::applySettings: calibration failed on Tx channel %d", m_deviceShared.m_channel);
            }
            else
            {
                qDebug("LimeSDROutput::applySettings: calibration successful on Tx channel %d", m_deviceShared.m_channel);
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
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardInterp, m_settings.m_centerFrequency, false);
            (*itSink)->getSampleSinkInputMessageQueue()->push(report);
        }

        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardInterp, m_settings.m_centerFrequency, false);
            (*itSource)->getSampleSourceInputMessageQueue()->push(report);
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
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardInterp, m_settings.m_centerFrequency, false);
            (*itSink)->getSampleSinkInputMessageQueue()->push(report);
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
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportClockSourceChange *report = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                    m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSource)->getSampleSourceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportClockSourceChange *report = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                    m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSink)->getSampleSinkInputMessageQueue()->push(report);
        }
    }

    QLocale loc;

    qDebug().noquote() << "LimeSDROutput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
            << " device stream sample rate: " << loc.toString(m_settings.m_devSampleRate) << "S/s"
            << " sample rate with soft interpolation: " << loc.toString( m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp)) << "S/s"
            << " DAC sample rate with hard interpolation: " << loc.toString(m_settings.m_devSampleRate*(1<<m_settings.m_log2HardInterp)) << "S/s"
            << " m_log2HardInterp: " << m_settings.m_log2HardInterp
            << " m_log2SoftInterp: " << m_settings.m_log2SoftInterp
            << " m_gain: " << m_settings.m_gain
            << " m_lpfBW: " << loc.toString(static_cast<int>(m_settings.m_lpfBW))
            << " m_lpfFIRBW: " << loc.toString(static_cast<int>(m_settings.m_lpfFIRBW))
            << " m_lpfFIREnable: " << m_settings.m_lpfFIREnable
            << " m_ncoEnable: " << m_settings.m_ncoEnable
            << " m_ncoFrequency: " << loc.toString(m_settings.m_ncoFrequency)
            << " m_antennaPath: " << m_settings.m_antennaPath
            << " m_extClock: " << m_settings.m_extClock
            << " m_extClockFreq: " << loc.toString(m_settings.m_extClockFreq)
            << " force: " << force
            << " forceNCOFrequency: " << forceNCOFrequency
            << " doCalibration: " << doCalibration
            << " doLPCalibration: " << doLPCalibration;

    return true;
}

int LimeSDROutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setLimeSdrOutputSettings(new SWGSDRangel::SWGLimeSdrOutputSettings());
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int LimeSDROutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    LimeSDROutputSettings settings = m_settings;

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

    MsgConfigureLimeSDR *msg = MsgConfigureLimeSDR::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLimeSDR *msgToGUI = MsgConfigureLimeSDR::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
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
}

int LimeSDROutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int LimeSDROutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
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
