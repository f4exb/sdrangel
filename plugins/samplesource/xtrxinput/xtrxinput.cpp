///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
    m_running(false),
    m_channelAcquired(false)
{

    suspendRxBuddies();
    suspendTxBuddies();
    openDevice();
    resumeTxBuddies();
    resumeRxBuddies();

    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);
}

XTRXInput::~XTRXInput()
{
    if (m_running) stop();
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    suspendRxBuddies();
    suspendTxBuddies();
    closeDevice();
    resumeTxBuddies();
    resumeRxBuddies();
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

    xtrx_channel_t requestedChannel = m_deviceAPI->getItemIndex() ? XTRX_CH_B : XTRX_CH_A;

    // look for Rx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        qDebug("XTRXInput::openDevice: look in Rx buddies");

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        m_deviceShared = *((DeviceXTRXShared *) sourceBuddy->getBuddySharedPtr()); // copy shared data
        DeviceXTRXParams *deviceParams = m_deviceShared.m_deviceParams; // get device parameters

        if (deviceParams == 0)
        {
            qCritical("XTRXInput::openDevice: cannot get device parameters from Rx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("XTRXInput::openDevice: getting device parameters from Rx buddy");
        }

        if (m_deviceAPI->getSourceBuddies().size() == deviceParams->m_nbRxChannels)
        {
            qCritical("XTRXInput::openDevice: no more Rx channels available in device");
            return false; // no more Rx channels available in device
        }
        else
        {
            qDebug("XTRXInput::openDevice: at least one more Rx channel is available in device");
        }

        // check if the requested channel is busy and abort if so (should not happen if device management is working correctly)

        char *busyChannels = new char[deviceParams->m_nbRxChannels];
        memset(busyChannels, 0, deviceParams->m_nbRxChannels);

        for (unsigned int i = 0; i < m_deviceAPI->getSourceBuddies().size(); i++)
        {
            DeviceSourceAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
            DeviceXTRXShared *buddyShared = (DeviceXTRXShared *) buddy->getBuddySharedPtr();

            if (buddyShared->m_channel == requestedChannel)
            {
                qCritical("XTRXInput::openDevice: cannot open busy channel %u", requestedChannel);
                return false;
            }
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
        delete[] busyChannels;
    }
    // look for Tx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("XTRXInput::openDevice: look in Tx buddies");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        m_deviceShared = *((DeviceXTRXShared *) sinkBuddy->getBuddySharedPtr()); // copy parameters

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("XTRXInput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("XTRXInput::openDevice: getting device parameters from Tx buddy");
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // There are no buddies then create the first XTRX common parameters
    // open the device this will also populate common fields
    // take the first Rx channel
    else
    {
        qDebug("XTRXInput::openDevice: open device here");

        m_deviceShared.m_deviceParams = new DeviceXTRXParams();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSampleSourceSerial()));

        if (!m_deviceShared.m_deviceParams->open(serial)) {
            delete m_deviceShared.m_deviceParams;
            m_deviceShared.m_deviceParams = 0;

            return false;
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    return true;
}

void XTRXInput::suspendRxBuddies()
{
    const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("XTRXInput::suspendRxBuddies (%lu)", sourceBuddies.size());

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

void XTRXInput::suspendTxBuddies()
{
    const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("XTRXInput::suspendTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceXTRXShared *buddySharedPtr = (DeviceXTRXShared *) (*itSink)->getBuddySharedPtr();

        if (buddySharedPtr->m_thread) {
            buddySharedPtr->m_thread->stopWork();
            buddySharedPtr->m_threadWasRunning = true;
        }
        else
        {
            buddySharedPtr->m_threadWasRunning = false;
        }
    }
}

void XTRXInput::resumeRxBuddies()
{
    const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("XTRXInput::resumeRxBuddies (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceXTRXShared *buddySharedPtr = (DeviceXTRXShared *) (*itSource)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void XTRXInput::resumeTxBuddies()
{
    const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("XTRXInput::resumeTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceXTRXShared *buddySharedPtr = (DeviceXTRXShared *) (*itSink)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void XTRXInput::closeDevice()
{
    if (m_deviceShared.m_deviceParams->getDevice() == 0) { // was never open
        return;
    }

    if (m_running) { stop(); }

    m_deviceShared.m_channel = XTRX_CH_AB;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }
}

bool XTRXInput::acquireChannel()
{
    //suspendRxBuddies();
    //suspendTxBuddies();

    qDebug("XTRXInput::acquireChannel: stream set up on Rx channel %d", m_deviceShared.m_channel);

    //resumeTxBuddies();
    //resumeRxBuddies();

    m_channelAcquired = true;
    return true;
}

void XTRXInput::releaseChannel()
{
    //suspendRxBuddies();
    //suspendTxBuddies();

    qDebug("XTRXInput::releaseChannel: Rx channel %d disabled", m_deviceShared.m_channel);

    //resumeTxBuddies();
    //resumeRxBuddies();

    // The channel will be effectively released to be reused in another device set only at close time

    m_channelAcquired = false;
}

void XTRXInput::init()
{
    applySettings(m_settings, true, false);
}

bool XTRXInput::start()
{
    if (!m_deviceShared.m_deviceParams->getDevice()) {
        return false;
    }

    if (m_running) {
        stop();
    }

    if (!acquireChannel()) {
        return false;
    }

    applySettings(m_settings, true);

    // start / stop streaming is done in the thread.

    if ((m_XTRXInputThread = new XTRXInputThread(&m_deviceShared, &m_sampleFifo)) == 0)
    {
        qFatal("XTRXInput::start: cannot create thread");
        stop();
        return false;
    }
    else
    {
        qDebug("XTRXInput::start: thread created");
    }

    m_XTRXInputThread->setLog2Decimation(m_settings.m_log2SoftDecim);
    m_XTRXInputThread->startWork();

    m_deviceShared.m_thread = m_XTRXInputThread;
    m_running = true;

    return true;
}

void XTRXInput::stop()
{
    qDebug("XTRXInput::stop");
    disconnect(m_XTRXInputThread, SIGNAL(finished()), this, SLOT(threadFinished()));

    if (m_XTRXInputThread != 0)
    {
        m_XTRXInputThread->stopWork();
        delete m_XTRXInputThread;
        m_XTRXInputThread = 0;
    }

    m_deviceShared.m_thread = 0;
    m_running = false;

    releaseChannel();
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

uint32_t XTRXInput::getHWLog2Decim() const
{
    return m_deviceShared.m_deviceParams->m_log2OvSRRx;
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

            xtrx_val_get(m_deviceShared.m_deviceParams->getDevice(),
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

        if (!m_deviceShared.m_deviceParams->getDevice() || ((board_temp = m_deviceShared.get_board_temperature() / 256.0) == 0.0)) {
            qDebug("XTRXInput::handleMessage: MsgGetDeviceInfo: cannot get board temperature");
        }

        if (!m_deviceShared.m_deviceParams->getDevice()) {
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
    if (xtrx_set_gain(m_deviceShared.m_deviceParams->getDevice(),
                      XTRX_CH_AB /*m_deviceShared.m_channel*/,
                      XTRX_RX_LNA_GAIN,
                      gain,
                      NULL) < 0)
    {
        qDebug("XTRXInput::applySettings: xtrx_set_gain(LNA) failed");
    }
    else
    {
        qDebug() << "XTRXInput::applySettings: Gain (LNA) set to " << gain;
    }
}

void XTRXInput::apply_gain_tia(double gain)
{
    if (xtrx_set_gain(m_deviceShared.m_deviceParams->getDevice(),
                      XTRX_CH_AB /*m_deviceShared.m_channel*/,
                      XTRX_RX_TIA_GAIN,
                      gain,
                      NULL) < 0)
    {
        qDebug("XTRXInput::applySettings: xtrx_set_gain(TIA) failed");
    }
    else
    {
        qDebug() << "XTRXInput::applySettings: Gain (TIA) set to " << gain;
    }
}

void XTRXInput::apply_gain_pga(double gain)
{
    if (xtrx_set_gain(m_deviceShared.m_deviceParams->getDevice(),
                      XTRX_CH_AB /*m_deviceShared.m_channel*/,
                      XTRX_RX_PGA_GAIN,
                      gain,
                      NULL) < 0)
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
    bool forwardChangeOwnDSP = false;
    bool forwardChangeRxDSP  = false;
    bool forwardChangeAllDSP = false;
    bool forwardClockSource  = false;
    bool ownThreadWasRunning = false;
    bool doLPCalibration = false;
    bool doChangeSampleRate = false;
    bool doChangeFreq = false;

    bool doGainAuto = false;
    bool doGainLna = false;
    bool doGainTia = false;
    bool doGainPga = false;

    // apply settings

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if ((m_settings.m_pwrmode != settings.m_pwrmode)) {
        if (xtrx_val_set(m_deviceShared.m_deviceParams->getDevice(),
                         XTRX_TRX, XTRX_CH_AB, XTRX_LMS7_PWR_MODE, settings.m_pwrmode) < 0)
        {
            qCritical("XTRXInput::applySettings: could not set power mode %d",
                      settings.m_pwrmode);
        }
    }

    if ((m_settings.m_extClock != settings.m_extClock) ||
            (settings.m_extClock && (m_settings.m_extClockFreq != settings.m_extClockFreq)) || force)
    {

        xtrx_set_ref_clk(m_deviceShared.m_deviceParams->getDevice(),
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

        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            doChangeSampleRate = true;
        }
    }

    if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
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
            if (m_settings.m_lnaGain != settings.m_lnaGain)
            {
                doGainLna = true;
            }
            if (m_settings.m_tiaGain != settings.m_tiaGain)
            {
                doGainTia = true;
            }
            if (m_settings.m_pgaGain != settings.m_pgaGain)
            {
                doGainPga = true;
            }
        }
    }

    if ((m_settings.m_lpfBW != settings.m_lpfBW) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
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
        m_deviceShared.m_log2Soft = settings.m_log2SoftDecim; // for buddies

        if (m_XTRXInputThread != 0)
        {
            m_XTRXInputThread->setLog2Decimation(settings.m_log2SoftDecim);
            qDebug() << "XTRXInput::applySettings: set soft decimation to " << (1<<settings.m_log2SoftDecim);
        }
    }

    if ((m_settings.m_antennaPath != settings.m_antennaPath) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (xtrx_set_antenna(m_deviceShared.m_deviceParams->getDevice(),
                                 settings.m_antennaPath) < 0)
            {
                qCritical("XTRXInput::applySettings: could not set antenna path to %d",
                          (int) settings.m_antennaPath);
            }
            else
            {
                qDebug("XTRXInput::applySettings: set antenna path to %d on channel %d",
                       (int) settings.m_antennaPath,
                       m_deviceShared.m_channel);
            }
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force)
    {
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
        if (m_XTRXInputThread && m_XTRXInputThread->isRunning())
        {
            m_XTRXInputThread->stopWork();
            ownThreadWasRunning = true;
        }

        suspendRxBuddies();
        suspendTxBuddies();

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
            m_deviceShared.m_deviceParams->m_log2OvSRRx = settings.m_log2HardDecim;
            m_deviceShared.m_deviceParams->m_sampleRate = settings.m_devSampleRate;
            doChangeFreq = true;
            forceNCOFrequency = true;

            qDebug("XTRXInput::applySettings: sample rate set to %f with oversampling of %d",
                   settings.m_devSampleRate,
                   1<<settings.m_log2HardDecim);
        }

        // TODO hangs!!!
        resumeTxBuddies();
        resumeRxBuddies();

        if (ownThreadWasRunning) {
            m_XTRXInputThread->startWork();
        }
    }

    if (doLPCalibration)
    {
        if (xtrx_tune_rx_bandwidth(m_deviceShared.m_deviceParams->getDevice(),
                                   m_deviceShared.m_channel,
                                   m_settings.m_lpfBW,
                                   NULL) < 0)
        {
            qCritical("XTRXInput::applySettings: could not set LPF to %f Hz", m_settings.m_lpfBW);
        }
        else
        {
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

        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (xtrx_tune(m_deviceShared.m_deviceParams->getDevice(),
                          XTRX_TUNE_RX_FDD,
                          settings.m_centerFrequency,
                          NULL) < 0)
            {
                qCritical("XTRXInput::applySettings: could not set frequency to %lu", settings.m_centerFrequency);
            }
            else
            {
                //doCalibration = true;
                m_deviceShared.m_centerFrequency = settings.m_centerFrequency; // for buddies
                qDebug("XTRXInput::applySettings: frequency set to %lu", settings.m_centerFrequency);
            }
        }
    }

    if (forceNCOFrequency)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (xtrx_tune(m_deviceShared.m_deviceParams->getDevice(),
                          XTRX_TUNE_BB_RX,
                          /* m_deviceShared.m_channel, */
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
                m_deviceShared.m_ncoFrequency = settings.m_ncoEnable ? settings.m_ncoFrequency : 0; // for buddies
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
