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

#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "dsp/dspcommands.h"
#include "limesdroutputthread.h"
#include "limesdr/devicelimesdrparam.h"
#include "limesdr/devicelimesdr.h"
#include "limesdroutput.h"

MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgConfigureLimeSDR, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgSetReferenceConfig, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgReportLimeSDRToGUI, Message)
MESSAGE_CLASS_DEFINITION(LimeSDROutput::MsgReportStreamInfo, Message)


LimeSDROutput::LimeSDROutput(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_limeSDROutputThread(0),
    m_deviceDescription(),
    m_running(false),
    m_firstConfig(true)
{
    m_streamId.handle = 0;
    openDevice();
}

LimeSDROutput::~LimeSDROutput()
{
    if (m_running) stop();
    closeDevice();
}

bool LimeSDROutput::openDevice()
{
    // look for Tx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSinkBuddies().size() > 0) // look sink sibling first
    {
        qDebug("LimeSDROutput::openDevice: look in Ix buddies");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        m_deviceShared = *((DeviceLimeSDRShared *) sinkBuddy->getBuddySharedPtr()); // copy shared data
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

        // look for unused channel number
        char *busyChannels = new char[deviceParams->m_nbTxChannels];
        memset(busyChannels, 0, deviceParams->m_nbTxChannels);

        for (int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
        {
            DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
            DeviceLimeSDRShared *buddyShared = (DeviceLimeSDRShared *) buddy->getBuddySharedPtr();
            busyChannels[buddyShared->m_channel] = 1;

            if (buddyShared->m_thread) { // suspend Tx buddy's thread for proper stream allocation later
                buddyShared->m_thread->stopWork();
            }
        }

        std::size_t ch = 0;

        for (;ch < deviceParams->m_nbTxChannels; ch++)
        {
            if (busyChannels[ch] == 0) {
                break; // first available is the good one
            }
        }

        m_deviceShared.m_channel = ch;
        delete[] busyChannels;
    }
    // look for Rx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSourceBuddies().size() > 0) // then source
    {
        qDebug("LimeSDROutput::openDevice: look in Rx buddies");

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        m_deviceShared = *((DeviceLimeSDRShared *) sourceBuddy->getBuddySharedPtr()); // copy parameters

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("LimeSDROutput::openDevice: cannot get device parameters from Rx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("LimeSDROutput::openDevice: getting device parameters from Rx buddy");
        }

        m_deviceShared.m_channel = 0; // take first channel
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
        m_deviceShared.m_channel = 0; // take first channel
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    // acquire the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_TX, m_deviceShared.m_channel, true) != 0)
    {
        qCritical("LimeSDROutput::openDevice: cannot enable Tx channel %lu", m_deviceShared.m_channel);
        return false;
    }
    else
    {
        qDebug("LimeSDROutput::openDevice: Tx channel %lu enabled", m_deviceShared.m_channel);
    }

    // set up the stream

    m_streamId.channel =  m_deviceShared.m_channel; // channel number
    m_streamId.fifoSize = 1024 * 128;               // fifo size in samples
    m_streamId.throughputVsLatency = 1.0;           // optimize for max throughput
    m_streamId.isTx = true;                         // TX channel
    m_streamId.dataFmt = lms_stream_t::LMS_FMT_I12; // 12-bit integers

    if (LMS_SetupStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId) != 0)
    {
        qCritical("LimeSDROutput::start: cannot setup the stream on Tx channel %lu", m_deviceShared.m_channel);
        return false;
    }
    else
    {
        qDebug("LimeSDROutput::start: stream set up on Tx channel %lu", m_deviceShared.m_channel);
    }

    // resume Tx buddy's threads

    for (int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
    {
        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
        DeviceLimeSDRShared *buddyShared = (DeviceLimeSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->startWork();
        }
    }

    return true;
}

void LimeSDROutput::closeDevice()
{
    if (m_deviceShared.m_deviceParams->getDevice() == 0) { // was never open
        return;
    }

    // suspend Tx buddy's threads

    for (int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
    {
        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
        DeviceLimeSDRShared *buddyShared = (DeviceLimeSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->stopWork();
        }
    }

    // destroy the stream
    LMS_DestroyStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId);
    m_streamId.handle = 0;

    // release the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_TX, m_deviceShared.m_channel, false) != 0)
    {
        qWarning("LimeSDROutput::closeDevice: cannot disable Tx channel %lu", m_deviceShared.m_channel);
    }

    m_deviceShared.m_channel = -1;

    // resume Tx buddy's threads

    for (int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
    {
        const DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
        DeviceLimeSDRShared *buddyShared = (DeviceLimeSDRShared *) buddy->getBuddySharedPtr();

        if (buddyShared->m_thread) {
            buddyShared->m_thread->startWork();
        }
    }

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSourceBuddies().size() == 0) && (m_deviceAPI->getSinkBuddies().size() == 0))
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }
}

bool LimeSDROutput::start()
{
    if (!m_deviceShared.m_deviceParams->getDevice()) {
        return false;
    }

    if (m_running) stop();

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
    if (m_limeSDROutputThread != 0)
    {
        m_limeSDROutputThread->stopWork();
        delete m_limeSDROutputThread;
        m_limeSDROutputThread = 0;
    }

    m_deviceShared.m_thread = 0;
    m_running = false;
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

        if (!applySettings(conf.getSettings(), m_firstConfig))
        {
            qDebug("LimeSDROutput::handleMessage config error");
        }
        else
        {
            m_firstConfig = false;
        }

        return true;
    }
    else if (MsgSetReferenceConfig::match(message))
    {
        MsgSetReferenceConfig& conf = (MsgSetReferenceConfig&) message;
        qDebug() << "LimeSDROutput::handleMessage: MsgSetReferenceConfig";
        m_settings = conf.getSettings();
        m_deviceShared.m_ncoFrequency = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0; // for buddies
        m_deviceShared.m_centerFrequency = m_settings.m_centerFrequency; // for buddies
        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
//        qDebug() << "LimeSDROutput::handleMessage: MsgGetStreamInfo";
        lms_stream_status_t status;

        if (m_streamId.handle && (LMS_GetStreamStatus(&m_streamId, &status) == 0))
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
            m_deviceAPI->getDeviceOutputMessageQueue()->push(report);
        }
        else
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
            m_deviceAPI->getDeviceOutputMessageQueue()->push(report);
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool LimeSDROutput::applySettings(const LimeSDROutputSettings& settings, bool force)
{
    bool forwardChangeOwnDSP = false;
    bool forwardChangeTxDSP  = false;
    bool forwardChangeAllDSP = false;
    bool suspendOwnThread    = false;
    bool suspendTxThread     = false;
    bool suspendAllThread    = false;
    bool doCalibration = false;
//  QMutexLocker mutexLocker(&m_mutex);

    // determine if buddies threads or own thread need to be suspended

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
    {
        suspendAllThread = true;
    }

    if ((m_settings.m_log2HardInterp != settings.m_log2HardInterp) ||
        (m_settings.m_centerFrequency != settings.m_centerFrequency) || force)
    {
        suspendTxThread = true;
    }

    if ((m_settings.m_gain != settings.m_gain) ||
        (m_settings.m_lpfBW != settings.m_lpfBW) ||
        (m_settings.m_lpfFIRBW != settings.m_lpfFIRBW) ||
        (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) ||
        (m_settings.m_ncoEnable != settings.m_ncoEnable) ||
        (m_settings.m_ncoFrequency != settings.m_ncoFrequency) || force)
    {
        suspendOwnThread = true;
    }

    // suspend buddies threads or own thread

    if (suspendAllThread)
    {
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSource)->getBuddySharedPtr();

            if (buddySharedPtr->m_thread) {
                buddySharedPtr->m_thread->stopWork();
            }
        }

        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();

            if (buddySharedPtr->m_thread) {
                buddySharedPtr->m_thread->stopWork();
            }
        }

        if (m_limeSDROutputThread) {
            m_limeSDROutputThread->stopWork();
        }
    }
    else if (suspendTxThread)
    {
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();

            if (buddySharedPtr->m_thread) {
                buddySharedPtr->m_thread->stopWork();
            }
        }

        if (m_limeSDROutputThread) {
            m_limeSDROutputThread->stopWork();
        }
    }
    else if (suspendOwnThread)
    {
        if (m_limeSDROutputThread) {
            m_limeSDROutputThread->stopWork();
        }
    }

    // apply settings

    if ((m_settings.m_gain != settings.m_gain) || force)
    {
        m_settings.m_gain = settings.m_gain;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
        {
            if (LMS_SetGaindB(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    m_deviceShared.m_channel,
                    m_settings.m_gain) < 0)
            {
                qDebug("LimeSDROutput::applySettings: LMS_SetGaindB() failed");
            }
            else
            {
                doCalibration = true;
                qDebug() << "LimeSDROutput::applySettings: Gain set to " << m_settings.m_gain;
            }
        }
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
       || (m_settings.m_log2HardInterp != settings.m_log2HardInterp) || force)
    {
        forwardChangeTxDSP  = m_settings.m_log2HardInterp != settings.m_log2HardInterp;
        forwardChangeAllDSP = m_settings.m_devSampleRate != settings.m_devSampleRate;

        m_settings.m_devSampleRate = settings.m_devSampleRate;
        m_settings.m_log2HardInterp = settings.m_log2HardInterp;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
        {
            if (LMS_SetSampleRateDir(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    m_settings.m_devSampleRate,
                    1<<m_settings.m_log2HardInterp) < 0)
            {
                qCritical("LimeSDROutput::applySettings: could not set sample rate to %d with oversampling of %d",
                        m_settings.m_devSampleRate,
                        1<<m_settings.m_log2HardInterp);
            }
            else
            {
                m_deviceShared.m_deviceParams->m_log2OvSRTx = m_settings.m_log2HardInterp;
                m_deviceShared.m_deviceParams->m_sampleRate = m_settings.m_devSampleRate;
                doCalibration = true;
                qDebug("LimeSDROutput::applySettings: set sample rate set to %d with oversampling of %d",
                        m_settings.m_devSampleRate,
                        1<<m_settings.m_log2HardInterp);
            }
        }
    }

    if ((m_settings.m_lpfBW != settings.m_lpfBW) || force)
    {
        m_settings.m_lpfBW = settings.m_lpfBW;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
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
                doCalibration = true;
                qDebug("LimeSDROutput::applySettings: LPF set to %f Hz", m_settings.m_lpfBW);
            }
        }
    }

    if ((m_settings.m_lpfFIRBW != settings.m_lpfFIRBW) ||
        (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) || force)
    {
        m_settings.m_lpfFIRBW = settings.m_lpfFIRBW;
        m_settings.m_lpfFIREnable = settings.m_lpfFIREnable;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
        {
            if (LMS_SetGFIRLPF(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    m_deviceShared.m_channel,
                    m_settings.m_lpfFIREnable,
                    m_settings.m_lpfFIRBW) < 0)
            {
                qCritical("LimeSDROutput::applySettings: could %s and set LPF FIR to %f Hz",
                        m_settings.m_lpfFIREnable ? "enable" : "disable",
                        m_settings.m_lpfFIRBW);
            }
            else
            {
                doCalibration = true;
                qDebug("LimeSDROutput::applySettings: %sd and set LPF FIR to %f Hz",
                        m_settings.m_lpfFIREnable ? "enable" : "disable",
                        m_settings.m_lpfFIRBW);
            }
        }
    }

    if ((m_settings.m_ncoFrequency != settings.m_ncoFrequency) ||
        (m_settings.m_ncoEnable != settings.m_ncoEnable) || force)
    {
        m_settings.m_ncoFrequency = settings.m_ncoFrequency;
        m_settings.m_ncoEnable = settings.m_ncoEnable;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
        {
            if (DeviceLimeSDR::setNCOFrequency(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    m_deviceShared.m_channel,
                    m_settings.m_ncoEnable,
                    m_settings.m_ncoFrequency))
            {
                doCalibration = true;
                forwardChangeOwnDSP = true;
                m_deviceShared.m_ncoFrequency = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0; // for buddies
                qDebug("LimeSDROutput::applySettings: %sd and set NCO to %d Hz",
                        m_settings.m_ncoEnable ? "enable" : "disable",
                        m_settings.m_ncoFrequency);
            }
            else
            {
                qCritical("LimeSDROutput::applySettings: could not %s and set NCO to %d Hz",
                        m_settings.m_ncoEnable ? "enable" : "disable",
                        m_settings.m_ncoFrequency);
            }
        }
    }

    if ((m_settings.m_log2SoftInterp != settings.m_log2SoftInterp) || force)
    {
        m_settings.m_log2SoftInterp = settings.m_log2SoftInterp;
        forwardChangeOwnDSP = true;
        m_deviceShared.m_log2Soft = m_settings.m_log2SoftInterp; // for buddies

        if (m_limeSDROutputThread != 0)
        {
            m_limeSDROutputThread->setLog2Interpolation(m_settings.m_log2SoftInterp);
            qDebug() << "LimeSDROutput::applySettings: set soft decimation to " << (1<<m_settings.m_log2SoftInterp);
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force)
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
        forwardChangeTxDSP = true;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
        {
            if (LMS_SetLOFrequency(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_TX,
                    m_deviceShared.m_channel, // same for both channels anyway but switches antenna port automatically
                    m_settings.m_centerFrequency) < 0)
            {
                qCritical("LimeSDROutput::applySettings: could not set frequency to %lu", m_settings.m_centerFrequency);
            }
            else
            {
                doCalibration = true;
                m_deviceShared.m_centerFrequency = m_settings.m_centerFrequency; // for buddies
                qDebug("LimeSDROutput::applySettings: frequency set to %lu", m_settings.m_centerFrequency);
            }
        }
    }


    if (doCalibration)
    {
        if (LMS_Calibrate(m_deviceShared.m_deviceParams->getDevice(),
                LMS_CH_TX,
                m_deviceShared.m_channel,
                m_settings.m_lpfBW,
                0) < 0)
        {
            qCritical("LimeSDROutput::applySettings: calibration failed on Rx channel %lu", m_deviceShared.m_channel);
        }
        else
        {
            qDebug("LimeSDROutput::applySettings: calibration successful on Rx channel %lu", m_deviceShared.m_channel);
        }
    }

    // resume buddies threads or own thread

    if (suspendAllThread)
    {
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSource)->getBuddySharedPtr();

            if (buddySharedPtr->m_thread) {
                buddySharedPtr->m_thread->startWork();
            }
        }

        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();

            if (buddySharedPtr->m_thread) {
                buddySharedPtr->m_thread->startWork();
            }
        }

        if (m_limeSDROutputThread) {
            m_limeSDROutputThread->startWork();
        }
    }
    else if (suspendTxThread)
    {
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();

            if (buddySharedPtr->m_thread) {
                buddySharedPtr->m_thread->startWork();
            }
        }

        if (m_limeSDROutputThread) {
            m_limeSDROutputThread->startWork();
        }
    }
    else if (suspendOwnThread)
    {
        if (m_limeSDROutputThread) {
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
        m_deviceAPI->getDeviceInputMessageQueue()->push(notif);

        // send to sink buddies
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();
            int buddyNCOFreq = buddySharedPtr->m_ncoFrequency;
            uint32_t buddyLog2SoftInterp = buddySharedPtr->m_log2Soft;
            DSPSignalNotification *notif = new DSPSignalNotification(
                    m_settings.m_devSampleRate/(1<<buddyLog2SoftInterp),
                    m_settings.m_centerFrequency + buddyNCOFreq); // do not change center frequency
            (*itSink)->getDeviceInputMessageQueue()->push(notif);
            MsgReportLimeSDRToGUI *report = MsgReportLimeSDRToGUI::create(
                    m_settings.m_centerFrequency,
                    m_settings.m_devSampleRate,
                    m_settings.m_log2HardInterp);
            (*itSink)->getDeviceOutputMessageQueue()->push(report);
        }

        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSource)->getBuddySharedPtr();
            uint64_t buddyCenterFreq = buddySharedPtr->m_centerFrequency;
            int buddyNCOFreq = buddySharedPtr->m_ncoFrequency;
            uint32_t buddyLog2SoftDecim = buddySharedPtr->m_log2Soft;
            DSPSignalNotification *notif = new DSPSignalNotification(
                    m_settings.m_devSampleRate/(1<<buddyLog2SoftDecim),
                    buddyCenterFreq + buddyNCOFreq);
            (*itSource)->getDeviceInputMessageQueue()->push(notif);
            DeviceLimeSDRShared::MsgCrossReportToGUI *report = DeviceLimeSDRShared::MsgCrossReportToGUI::create(m_settings.m_devSampleRate);
            (*itSource)->getDeviceOutputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeTxDSP)
    {
        qDebug("LimeSDROutput::applySettings: forward change to Tx buddies");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp);
        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceInputMessageQueue()->push(notif);

        // send to sink buddies
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();
            uint64_t buddyCenterFreq = buddySharedPtr->m_centerFrequency;
            int buddyNCOFreq = buddySharedPtr->m_ncoFrequency;
            DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, buddyCenterFreq + buddyNCOFreq); // do not change center frequency
            (*itSink)->getDeviceInputMessageQueue()->push(notif);
            MsgReportLimeSDRToGUI *report = MsgReportLimeSDRToGUI::create(
                    m_settings.m_centerFrequency,
                    m_settings.m_devSampleRate,
                    m_settings.m_log2HardInterp);
            (*itSink)->getDeviceOutputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeOwnDSP)
    {
        qDebug("LimeSDROutput::applySettings: forward change to self only");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp);
        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceInputMessageQueue()->push(notif);
    }

    qDebug() << "LimeSDROutput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
            << " device stream sample rate: " << m_settings.m_devSampleRate << "S/s"
            << " sample rate with soft decimation: " << m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp) << "S/s";

    return true;
}

