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
#include "limesdrinput.h"
#include "limesdrinputthread.h"
#include "limesdr/devicelimesdrparam.h"

MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgConfigureLimeSDR, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgSetReferenceConfig, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgReportLimeSDRToGUI, Message)


LimeSDRInput::LimeSDRInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_limeSDRInputThread(0),
    m_deviceDescription(),
    m_running(false)
{
    openDevice();
}

LimeSDRInput::~LimeSDRInput()
{
    if (m_running) stop();
    closeDevice();
}

bool LimeSDRInput::openDevice()
{
    // look for Rx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        m_deviceShared = *((DeviceLimeSDRShared *) sourceBuddy->getBuddySharedPtr()); // copy shared data
        DeviceLimeSDRParams *deviceParams = m_deviceShared.m_deviceParams; // get device parameters

        if (deviceParams == 0)
        {
            qCritical("LimeSDRInput::openDevice: cannot get device parameters from Rx buddy");
            return false; // the device params should have been created by the buddy
        }

        if (m_deviceAPI->getSourceBuddies().size() == deviceParams->m_nbRxChannels)
        {
            return false; // no more Rx channels available in device
        }
        // look for unused channel number
        char *busyChannels = new char[deviceParams->m_nbRxChannels];
        memset(busyChannels, 0, deviceParams->m_nbRxChannels);

        for (int i = 0; i < m_deviceAPI->getSourceBuddies().size(); i++)
        {
            DeviceSourceAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
            DeviceLimeSDRShared *buddyShared = (DeviceLimeSDRShared *) buddy->getBuddySharedPtr();
            busyChannels[buddyShared->m_channel] = 1;
        }

        std::size_t ch = 0;

        for (;ch < deviceParams->m_nbRxChannels; ch++)
        {
            if (busyChannels[ch] == 0) {
                break; // first available is the good one
            }
        }

        m_deviceShared.m_channel = ch;
        delete[] busyChannels;
    }
    // look for Tx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        m_deviceShared = *((DeviceLimeSDRShared *) sinkBuddy->getBuddySharedPtr()); // copy parameters

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("LimeSDRInput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }

        m_deviceShared.m_channel = 0; // take first channel
    }
    // There are no buddies then create the first LimeSDR common parameters
    // open the device this will also populate common fields
    // take the first Rx channel
    else
    {
        m_deviceShared.m_deviceParams = new DeviceLimeSDRParams();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSampleSourceSerial()));
        m_deviceShared.m_deviceParams->open(serial);
        m_deviceShared.m_channel = 0; // take first channel
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    // acquire the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_RX, m_deviceShared.m_channel, true) != 0)
    {
        qCritical("LimeSDRInput::openDevice: cannot enable Rx channel %lu", m_deviceShared.m_channel);
        return false;
    }

    return true;
}

void LimeSDRInput::closeDevice()
{
    if (m_deviceShared.m_deviceParams->getDevice() == 0) { // was never open
        return;
    }

    // release the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_RX, m_deviceShared.m_channel, false) != 0)
    {
        qWarning("LimeSDRInput::closeDevice: cannot disable Rx channel %lu", m_deviceShared.m_channel);
    }

    m_deviceShared.m_channel = -1;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }
}

bool LimeSDRInput::start()
{
    if (!m_deviceShared.m_deviceParams->getDevice()) {
        return false;
    }

    if (m_running) stop();

    // set up the stream

    m_streamId.channel =  m_deviceShared.m_channel; //channel number
    m_streamId.fifoSize = 1024 * 128;               //fifo size in samples
    m_streamId.throughputVsLatency = 1.0;           //optimize for max throughput
    m_streamId.isTx = false;                        //RX channel
    m_streamId.dataFmt = lms_stream_t::LMS_FMT_I12; //12-bit integers

    if (LMS_SetupStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId) != 0)
    {
        qCritical("LimeSDRInput::start: cannot setup the stream on Rx channel %lu", m_deviceShared.m_channel);
        return false;
    }

    // start / stop streaming is done in the thread.

    if ((m_limeSDRInputThread = new LimeSDRInputThread(&m_streamId, &m_sampleFifo)) == 0)
    {
        qFatal("LimeSDRInput::start: out of memory");
        stop();
        return false;
    }

    m_limeSDRInputThread->setLog2Decimation(m_settings.m_log2SoftDecim);
    m_limeSDRInputThread->setFcPos((int) m_settings.m_fcPos);

    m_limeSDRInputThread->startWork();

    applySettings(m_settings, true);
    m_running = true;

    return true;
}

void LimeSDRInput::stop()
{
    if (m_limeSDRInputThread != 0)
    {
        m_limeSDRInputThread->stopWork();
        delete m_limeSDRInputThread;
        m_limeSDRInputThread = 0;
    }

    // destroy the stream
    LMS_DestroyStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId);

    m_running = false;
}

const QString& LimeSDRInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int LimeSDRInput::getSampleRate() const
{
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<(m_settings.m_log2HardDecim + m_settings.m_log2SoftDecim)));
}

quint64 LimeSDRInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

std::size_t LimeSDRInput::getChannelIndex()
{
    return m_deviceShared.m_channel;
}

void LimeSDRInput::getLORange(float& minF, float& maxF, float& stepF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_loRangeRx;
    minF = range.min;
    maxF = range.max;
    stepF = range.step;
    qDebug("LimeSDRInput::getLORange: min: %f max: %f step: %f", range.min, range.max, range.step);
}

void LimeSDRInput::getSRRange(float& minF, float& maxF, float& stepF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_srRangeRx;
    minF = range.min;
    maxF = range.max;
    stepF = range.step;
    qDebug("LimeSDRInput::getSRRange: min: %f max: %f step: %f", range.min, range.max, range.step);
}

void LimeSDRInput::getLPRange(float& minF, float& maxF, float& stepF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_lpfRangeRx;
    float step = range.step < 1000.0f ? 1000.0 : range.step;
    minF = range.min;
    maxF = range.max;
    stepF = step;
    qDebug("LimeSDRInput::getLPRange: min: %f max: %f step: %f", range.min, range.max, range.step);
}

int LimeSDRInput::getLPIndex(float lpfBW) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_lpfRangeRx;
    float step = range.step < 1000.0f ? 1000.0 : range.step;
    return (int) ((lpfBW - range.min) / step);
}

float LimeSDRInput::getLPValue(int index) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_lpfRangeRx;
    float step = range.step < 1000.0f ? 1000.0 : range.step;
    return index * step;
}

uint32_t LimeSDRInput::getHWLog2Decim() const
{
    return m_deviceShared.m_deviceParams->m_log2OvSRRx;
}

bool LimeSDRInput::handleMessage(const Message& message)
{
    if (MsgConfigureLimeSDR::match(message))
    {
        MsgConfigureLimeSDR& conf = (MsgConfigureLimeSDR&) message;
        qDebug() << "LimeSDRInput::handleMessage: MsgConfigureLimeSDR";

        if (!applySettings(conf.getSettings(), false))
        {
            qDebug("LimeSDRInput::handleMessage config error");
        }

        return true;
    }
    else if (MsgSetReferenceConfig::match(message))
    {
        MsgSetReferenceConfig& conf = (MsgSetReferenceConfig&) message;
        qDebug() << "LimeSDRInput::handleMessage: MsgSetReferenceLimeSDR";
        m_settings = conf.getSettings();
        return true;
    }
    else
    {
        return false;
    }
}

bool LimeSDRInput::applySettings(const LimeSDRInputSettings& settings, bool force)
{
    bool forwardChangeOwnDSP = false;
    bool forwardChangeRxDSP  = false;
    bool forwardChangeAllDSP = false;
//  QMutexLocker mutexLocker(&m_mutex);

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force)
    {
        m_settings.m_dcBlock = settings.m_dcBlock;
        m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_settings.m_iqCorrection = settings.m_iqCorrection;
        m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
    }

    if ((m_settings.m_gain != settings.m_gain) || force)
    {
        m_settings.m_gain = settings.m_gain;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
        {
            if (LMS_SetGaindB(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    m_settings.m_gain) < 0)
            {
                qDebug("LimeSDRInput::applySettings: LMS_SetGaindB() failed");
            }
            else
            {
                qDebug() << "LimeSDRInput::applySettings: Gain set to " << m_settings.m_gain;
            }
        }
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
       || (m_settings.m_log2HardDecim != settings.m_log2HardDecim) || force)
    {
        forwardChangeRxDSP  = m_settings.m_log2HardDecim != settings.m_log2HardDecim;
        forwardChangeAllDSP = m_settings.m_devSampleRate != settings.m_devSampleRate;

        m_settings.m_devSampleRate = settings.m_devSampleRate;
        m_settings.m_log2HardDecim = settings.m_log2HardDecim;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
        {
            if (LMS_SetSampleRateDir(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_settings.m_devSampleRate,
                    1<<m_settings.m_log2HardDecim) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could not set sample rate to %d with oversampling of %d",
                        m_settings.m_devSampleRate,
                        1<<m_settings.m_log2HardDecim);
            }
            else
            {
                m_deviceShared.m_deviceParams->m_log2OvSRRx = m_settings.m_log2HardDecim;
                m_deviceShared.m_deviceParams->m_sampleRate = m_settings.m_devSampleRate;
                qDebug("LimeSDRInput::applySettings: set sample rate set to %d with oversampling of %d",
                        m_settings.m_devSampleRate,
                        1<<m_settings.m_log2HardDecim);
            }
        }
    }

    if ((m_settings.m_lpfBW != settings.m_lpfBW) || force)
    {
        m_settings.m_lpfBW = settings.m_lpfBW;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
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
    }

    if ((m_settings.m_lpfFIRBW != settings.m_lpfFIRBW) ||
        (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) || force)
    {
        m_settings.m_lpfFIRBW = settings.m_lpfFIRBW;
        m_settings.m_lpfFIREnable = settings.m_lpfFIREnable;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
        {
            if (LMS_SetGFIRLPF(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    m_settings.m_lpfFIREnable,
                    m_settings.m_lpfFIRBW) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could %s and set LPF FIR to %f Hz",
                        m_settings.m_lpfFIREnable ? "enable" : "disable",
                        m_settings.m_lpfFIRBW);
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: %sd and set LPF FIR to %f Hz",
                        m_settings.m_lpfFIREnable ? "enable" : "disable",
                        m_settings.m_lpfFIRBW);
            }
        }
    }

    if ((m_settings.m_log2SoftDecim != settings.m_log2SoftDecim) || force)
    {
        m_settings.m_log2SoftDecim = settings.m_log2SoftDecim;
        forwardChangeOwnDSP = true;

        if (m_limeSDRInputThread != 0)
        {
            m_limeSDRInputThread->setLog2Decimation(m_settings.m_log2SoftDecim);
            qDebug() << "LimeSDRInput::applySettings: set soft decimation to " << (1<<m_settings.m_log2SoftDecim);
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force)
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
        forwardChangeRxDSP = true;

        if (m_deviceShared.m_deviceParams->getDevice() != 0)
        {
            if (LMS_SetLOFrequency(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel, // same for both channels anyway but switches antenna port automatically
                    m_settings.m_centerFrequency ) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could not set frequency to %lu", m_settings.m_centerFrequency);
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: frequency set to %lu", m_settings.m_centerFrequency);
            }
        }
    }

    if (forwardChangeAllDSP)
    {
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();
        int sampleRate = m_settings.m_devSampleRate/(1<<(m_settings.m_log2HardDecim + m_settings.m_log2SoftDecim));

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
            (*itSource)->getDeviceInputMessageQueue()->push(notif);
            MsgReportLimeSDRToGUI *report = MsgReportLimeSDRToGUI::create(
                    m_settings.m_centerFrequency,
                    m_settings.m_devSampleRate,
                    m_settings.m_log2HardDecim);
            (*itSource)->getDeviceInputMessageQueue()->push(report);
        }

        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
            (*itSink)->getDeviceInputMessageQueue()->push(notif);
            MsgReportLimeSDRToGUI *report = MsgReportLimeSDRToGUI::create(
                    m_settings.m_centerFrequency,
                    m_settings.m_devSampleRate,
                    m_settings.m_log2HardDecim);
            (*itSink)->getDeviceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeRxDSP)
    {
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();
        int sampleRate = m_settings.m_devSampleRate/(1<<(m_settings.m_log2HardDecim + m_settings.m_log2SoftDecim));

        for (; it != sourceBuddies.end(); ++it)
        {
            DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
            (*it)->getDeviceInputMessageQueue()->push(notif);
            MsgReportLimeSDRToGUI *report = MsgReportLimeSDRToGUI::create(
                    m_settings.m_centerFrequency,
                    m_settings.m_devSampleRate,
                    m_settings.m_log2HardDecim);
            (*it)->getDeviceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeOwnDSP)
    {
        int sampleRate = m_settings.m_devSampleRate/(1<<(m_settings.m_log2HardDecim + m_settings.m_log2SoftDecim));
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceInputMessageQueue()->push(notif);
    }

    qDebug() << "LimeSDRInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
            << " device sample rate: " << m_settings.m_devSampleRate << "S/s"
            << " sample rate after soft decimation: " << m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim) << "S/s";

    return true;
}

