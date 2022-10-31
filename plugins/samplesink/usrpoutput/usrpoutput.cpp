///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <uhd/usrp/multi_usrp.hpp>

#include "SWGDeviceSettings.h"
#include "SWGUSRPOutputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGUSRPOutputReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "usrpoutputthread.h"
#include "usrp/deviceusrpparam.h"
#include "usrp/deviceusrp.h"
#include "usrpoutput.h"

MESSAGE_CLASS_DEFINITION(USRPOutput::MsgConfigureUSRP, Message)
MESSAGE_CLASS_DEFINITION(USRPOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(USRPOutput::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(USRPOutput::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(USRPOutput::MsgReportStreamInfo, Message)


USRPOutput::USRPOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_usrpOutputThread(nullptr),
    m_deviceDescription("USRPOutput"),
    m_running(false),
    m_channelAcquired(false),
    m_bufSamples(0)
{
    m_deviceAPI->setNbSinkStreams(1);
    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_settings.m_devSampleRate));
    m_streamId = nullptr;
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
        &USRPOutput::networkManagerFinished
    );
}

USRPOutput::~USRPOutput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &USRPOutput::networkManagerFinished
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

void USRPOutput::destroy()
{
    delete this;
}

bool USRPOutput::openDevice()
{
    int requestedChannel = m_deviceAPI->getDeviceItemIndex();

    // look for Tx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSinkBuddies().size() > 0) // look sink sibling first
    {
        qDebug("USRPOutput::openDevice: look in Ix buddies");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        //m_deviceShared = *((DeviceUSRPShared *) sinkBuddy->getBuddySharedPtr()); // copy shared data
        DeviceUSRPShared *deviceUSRPShared = (DeviceUSRPShared*) sinkBuddy->getBuddySharedPtr();
        m_deviceShared.m_deviceParams = deviceUSRPShared->m_deviceParams;

        DeviceUSRPParams *deviceParams = m_deviceShared.m_deviceParams; // get device parameters

        if (deviceParams == 0)
        {
            qCritical("USRPOutput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("USRPOutput::openDevice: getting device parameters from Tx buddy");
        }

        if (m_deviceAPI->getSinkBuddies().size() == deviceParams->m_nbTxChannels)
        {
            qCritical("USRPOutput::openDevice: no more Tx channels available in device");
            return false; // no more Tx channels available in device
        }
        else
        {
            qDebug("USRPOutput::openDevice: at least one more Tx channel is available in device");
        }

        // check if the requested channel is busy and abort if so (should not happen if device management is working correctly)

        for (unsigned int i = 0; i < m_deviceAPI->getSinkBuddies().size(); i++)
        {
            DeviceAPI *buddy = m_deviceAPI->getSinkBuddies()[i];
            DeviceUSRPShared *buddyShared = (DeviceUSRPShared *) buddy->getBuddySharedPtr();

            if (buddyShared->m_channel == requestedChannel)
            {
                qCritical("USRPOutput::openDevice: cannot open busy channel %u", requestedChannel);
                return false;
            }
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // look for Rx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSourceBuddies().size() > 0) // then source
    {
        qDebug("USRPOutput::openDevice: look in Rx buddies");

        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        //m_deviceShared = *((DeviceUSRPShared *) sourceBuddy->getBuddySharedPtr()); // copy parameters
        DeviceUSRPShared *deviceUSRPShared = (DeviceUSRPShared*) sourceBuddy->getBuddySharedPtr();
        m_deviceShared.m_deviceParams = deviceUSRPShared->m_deviceParams;

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("USRPOutput::openDevice: cannot get device parameters from Rx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("USRPOutput::openDevice: getting device parameters from Rx buddy");
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // There are no buddies then create the first USRP common parameters
    // open the device this will also populate common fields
    // take the first Tx channel
    else
    {
        qDebug("USRPOutput::openDevice: open device here");

        m_deviceShared.m_deviceParams = new DeviceUSRPParams();
        QString deviceStr;
        // If a non-discoverable device, serial with be of the form USRP-N
        if (m_deviceAPI->getSamplingDeviceSerial().startsWith("USRP"))
        {
            deviceStr = m_deviceAPI->getHardwareUserArguments();
        }
        else
        {
            deviceStr = m_deviceAPI->getSamplingDeviceSerial();
            if (m_deviceAPI->getHardwareUserArguments().size() != 0) {
                deviceStr = deviceStr + ',' + m_deviceAPI->getHardwareUserArguments();
            }
        }
        if (!m_deviceShared.m_deviceParams->open(deviceStr, false))
        {
            qCritical("USRPOutput::openDevice: failed to open device");
            return false;
        }
        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    return true;
}

void USRPOutput::suspendRxBuddies()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("USRPOutput::suspendRxBuddies (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceUSRPShared *buddySharedPtr = (DeviceUSRPShared *) (*itSource)->getBuddySharedPtr();

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

void USRPOutput::suspendTxBuddies()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("USRPOutput::suspendTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceUSRPShared *buddySharedPtr = (DeviceUSRPShared *) (*itSink)->getBuddySharedPtr();

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

void USRPOutput::resumeRxBuddies()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("USRPOutput::resumeRxBuddies (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceUSRPShared *buddySharedPtr = (DeviceUSRPShared *) (*itSource)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void USRPOutput::resumeTxBuddies()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("USRPOutput::resumeTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceUSRPShared *buddySharedPtr = (DeviceUSRPShared *) (*itSink)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void USRPOutput::closeDevice()
{
    if (m_deviceShared.m_deviceParams->getDevice() == nullptr) { // was never open
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

bool USRPOutput::acquireChannel()
{
    suspendRxBuddies();
    suspendTxBuddies();

    if (m_streamId == nullptr)
    {
        try
        {
            uhd::usrp::multi_usrp::sptr usrp = m_deviceShared.m_deviceParams->getDevice();

            // Apply settings before creating stream
            // However, don't set LPF to <10MHz at this stage, otherwise there is massive TX LO leakage
            applySettings(m_settings, QList<QString>(), true, true);
            usrp->set_tx_bandwidth(56000000, m_deviceShared.m_channel);

            // set up the stream
            std::string cpu_format("sc16");
            std::string wire_format("sc16");
            std::vector<size_t> channel_nums;
            channel_nums.push_back(m_deviceShared.m_channel);

            uhd::stream_args_t stream_args(cpu_format, wire_format);
            stream_args.channels = channel_nums;

            m_streamId = usrp->get_tx_stream(stream_args);

            // Match our transmit buffer size to what UHD uses
            m_bufSamples = m_streamId->get_max_num_samps();

            // Wait for reference and LO to lock
            DeviceUSRP::waitForLock(usrp, m_settings.m_clockSource, m_deviceShared.m_channel);

            // Now we can set desired bandwidth
            usrp->set_tx_bandwidth(m_settings.m_lpfBW, m_deviceShared.m_channel);
        }
        catch (std::exception& e)
        {
            qDebug() << "USRPOutput::acquireChannel: exception: " << e.what();
        }
    }

    resumeTxBuddies();
    resumeRxBuddies();

    m_channelAcquired = true;

    return true;
}

void USRPOutput::releaseChannel()
{
    suspendRxBuddies();
    suspendTxBuddies();

    // destroy the stream
    m_streamId = nullptr;

    resumeTxBuddies();
    resumeRxBuddies();

    m_channelAcquired = false;
}

void USRPOutput::init()
{
    applySettings(m_settings, QList<QString>(), false, true);
}

bool USRPOutput::start()
{
    if (!m_deviceShared.m_deviceParams->getDevice()) {
        return false;
    }

    if (m_running) { stop(); }

    if (!acquireChannel()) {
        return false;
    }

    m_usrpOutputThread = new USRPOutputThread(m_streamId, m_bufSamples, &m_sampleSourceFifo);
    qDebug("USRPOutput::start: thread created");

    m_usrpOutputThread->setLog2Interpolation(m_settings.m_log2SoftInterp);
    m_usrpOutputThread->startWork();

    m_deviceShared.m_thread = m_usrpOutputThread;
    m_running = true;

    return true;
}

void USRPOutput::stop()
{
    qDebug("USRPOutput::stop");

    if (m_usrpOutputThread)
    {
        m_usrpOutputThread->stopWork();
        delete m_usrpOutputThread;
        m_usrpOutputThread = nullptr;
    }

    m_deviceShared.m_thread = 0;
    m_running = false;

    releaseChannel();
}

QByteArray USRPOutput::serialize() const
{
    return m_settings.serialize();
}

bool USRPOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureUSRP* message = MsgConfigureUSRP::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUSRP* messageToGUI = MsgConfigureUSRP::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& USRPOutput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int USRPOutput::getSampleRate() const
{
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2SoftInterp));
}

quint64 USRPOutput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void USRPOutput::setCenterFrequency(qint64 centerFrequency)
{
    USRPOutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureUSRP* message = MsgConfigureUSRP::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUSRP* messageToGUI = MsgConfigureUSRP::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

int USRPOutput::getChannelIndex()
{
    return m_deviceShared.m_channel;
}

void USRPOutput::getLORange(float& minF, float& maxF) const
{
    try
    {
        minF = m_deviceShared.m_deviceParams->m_loRangeTx.start();
        maxF = m_deviceShared.m_deviceParams->m_loRangeTx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutput::getLORange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

void USRPOutput::getSRRange(float& minF, float& maxF) const
{
    try
    {
        minF = m_deviceShared.m_deviceParams->m_srRangeTx.start();
        maxF = m_deviceShared.m_deviceParams->m_srRangeTx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutput::getLORange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

void USRPOutput::getLPRange(float& minF, float& maxF) const
{
    try
    {
        minF = m_deviceShared.m_deviceParams->m_lpfRangeTx.start();
        maxF = m_deviceShared.m_deviceParams->m_lpfRangeTx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutput::getLORange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

void USRPOutput::getGainRange(float& minF, float& maxF) const
{
    try
    {
        minF = m_deviceShared.m_deviceParams->m_gainRangeTx.start();
        maxF = m_deviceShared.m_deviceParams->m_gainRangeTx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutput::getLORange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

QStringList USRPOutput::getTxAntennas() const
{
    return m_deviceShared.m_deviceParams->m_txAntennas;
}

QStringList USRPOutput::getClockSources() const
{
    return m_deviceShared.m_deviceParams->m_clockSources;
}

bool USRPOutput::handleMessage(const Message& message)
{
    if (MsgConfigureUSRP::match(message))
    {
        MsgConfigureUSRP& conf = (MsgConfigureUSRP&) message;
        qDebug() << "USRPOutput::handleMessage: MsgConfigureUSRP";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), false, conf.getForce())) {
            qDebug("USRPOutput::handleMessage config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "USRPOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (DeviceUSRPShared::MsgReportBuddyChange::match(message))
    {
        DeviceUSRPShared::MsgReportBuddyChange& report = (DeviceUSRPShared::MsgReportBuddyChange&) message;

        if (!report.getRxElseTx())
        {
            // Tx buddy changed settings, we need to copy
            m_settings.m_devSampleRate   = report.getDevSampleRate();
            m_settings.m_centerFrequency = report.getCenterFrequency();
            m_settings.m_loOffset        = report.getLOOffset();
        }
        // Master clock rate is common between all buddies
        int masterClockRate = report.getMasterClockRate();
        if (masterClockRate > 0)
            m_settings.m_masterClockRate = masterClockRate;
        qDebug() << "USRPOutput::handleMessage MsgReportBuddyChange";
        qDebug() << "m_masterClockRate " << m_settings.m_masterClockRate;

        DSPSignalNotification *notif = new DSPSignalNotification(
                m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp),
                m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        if (getMessageQueueToGUI())
        {
            DeviceUSRPShared::MsgReportBuddyChange *reportToGUI = DeviceUSRPShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset,  m_settings.m_masterClockRate, false);
            getMessageQueueToGUI()->push(reportToGUI);
        }

        return true;
    }
    else if (DeviceUSRPShared::MsgReportClockSourceChange::match(message))
    {
        DeviceUSRPShared::MsgReportClockSourceChange& report = (DeviceUSRPShared::MsgReportClockSourceChange&) message;

        m_settings.m_clockSource = report.getClockSource();

        if (getMessageQueueToGUI())
        {
            DeviceUSRPShared::MsgReportClockSourceChange *reportToGUI = DeviceUSRPShared::MsgReportClockSourceChange::create(
                    m_settings.m_clockSource);
            getMessageQueueToGUI()->push(reportToGUI);
        }

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
        if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
        {
            if ((m_streamId != nullptr) && m_channelAcquired)
            {
                bool active;
                quint32 underflows;
                quint32 droppedPackets;

                m_usrpOutputThread->getStreamStatus(active, underflows, droppedPackets);
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        true, // success
                        active,
                        underflows,
                        droppedPackets
                        );
                m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
            }
            else
            {
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(false, false, 0, 0);
                m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool USRPOutput::applySettings(const USRPOutputSettings& settings, const QList<QString>& settingsKeys, bool preGetStream, bool force)
{
    qDebug() << "USRPOutput::applySettings: preGetStream:" << preGetStream << " force:" << force << settings.getDebugString(settingsKeys, force);
    bool forwardChangeOwnDSP = false;
    bool forwardChangeTxDSP  = false;
    bool forwardChangeAllDSP = false;
    bool forwardClockSource  = false;
    bool checkRates          = false;

    try
    {
        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;

        // apply settings

        if (settingsKeys.contains("clockSource") || force)
        {
            if (m_deviceShared.m_deviceParams->getDevice() && (m_channelAcquired || preGetStream))
            {
                try
                {
                    m_deviceShared.m_deviceParams->getDevice()->set_clock_source(settings.m_clockSource.toStdString(), 0);
                    forwardClockSource = true;
                    qDebug() << "USRPOutput::applySettings: clock set to" << settings.m_clockSource;
                }
                catch (std::exception &e)
                {
                    // An exception will be thrown if the clock is not detected
                    // however, get_clock_source called below will still say the clock has is set
                    qCritical() << "USRPOutput::applySettings: could not set clock " << settings.m_clockSource;
                    // So, default back to internal
                    m_deviceShared.m_deviceParams->getDevice()->set_clock_source("internal", 0);
                    // notify GUI that source couldn't be set
                    forwardClockSource = true;
                }
            }
            else
            {
                qCritical() << "USRPOutput::applySettings: could not set clock to " << settings.m_clockSource;
            }
        }

        if (settingsKeys.contains("devSampleRate") || force)
        {
            forwardChangeAllDSP = true;

            if (m_deviceShared.m_deviceParams->getDevice() && (m_channelAcquired || preGetStream))
            {
                m_deviceShared.m_deviceParams->getDevice()->set_tx_rate(settings.m_devSampleRate, m_deviceShared.m_channel);
                qDebug("USRPOutput::applySettings: set sample rate set to %d", settings.m_devSampleRate);
                checkRates = true;
            }
        }

        if (settingsKeys.contains("centerFrequency")
            || settingsKeys.contains("loOffset")
            || settingsKeys.contains("transverterMode")
            || settingsKeys.contains("transverterDeltaFrequency")
            || force)
        {
            forwardChangeTxDSP = true;

            if (m_deviceShared.m_deviceParams->getDevice() && (m_channelAcquired || preGetStream))
            {
                if (settings.m_loOffset != 0)
                {
                    uhd::tune_request_t tune_request(deviceCenterFrequency, settings.m_loOffset);
                    m_deviceShared.m_deviceParams->getDevice()->set_tx_freq(tune_request, m_deviceShared.m_channel);
                }
                else
                {
                    uhd::tune_request_t tune_request(deviceCenterFrequency);
                    m_deviceShared.m_deviceParams->getDevice()->set_tx_freq(tune_request, m_deviceShared.m_channel);
                }
                m_deviceShared.m_centerFrequency = deviceCenterFrequency; // for buddies
                qDebug("USRPOutput::applySettings: frequency set to %lld with LO offset %d", deviceCenterFrequency, settings.m_loOffset);
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
                DeviceUSRPShared::m_sampleFifoMinRate);
#endif
            m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(fifoRate));
            qDebug("USRPOutput::applySettings: resize FIFO: rate %u", fifoRate);
        }

        if (settingsKeys.contains("gain") || force)
        {
            if (m_deviceShared.m_deviceParams->getDevice() && (m_channelAcquired || preGetStream))
            {
                m_deviceShared.m_deviceParams->getDevice()->set_tx_gain(settings.m_gain, m_deviceShared.m_channel);
                qDebug() << "USRPOutput::applySettings: Gain set to " << settings.m_gain;
            }
        }

        if (settingsKeys.contains("lpfBW") || force)
        {
            // Don't set bandwidth before get_tx_stream (See above)
            if (m_deviceShared.m_deviceParams->getDevice() && m_channelAcquired)
            {
                m_deviceShared.m_deviceParams->getDevice()->set_tx_bandwidth(settings.m_lpfBW, m_deviceShared.m_channel);
                qDebug("USRPOutput::applySettings: LPF BW: %f for channel %d", settings.m_lpfBW, m_deviceShared.m_channel);
            }
        }

        if (settingsKeys.contains("log2SoftInterp") || force)
        {
            forwardChangeOwnDSP = true;
            m_deviceShared.m_log2Soft = settings.m_log2SoftInterp; // for buddies

            if (m_usrpOutputThread)
            {
                m_usrpOutputThread->setLog2Interpolation(settings.m_log2SoftInterp);
                qDebug() << "USRPOutput::applySettings: set soft interpolation to " << (1<<settings.m_log2SoftInterp);
            }
        }

        if (settingsKeys.contains("antennaPath") || force)
        {
            if (m_deviceShared.m_deviceParams->getDevice() && (m_channelAcquired || preGetStream))
            {
                m_deviceShared.m_deviceParams->getDevice()->set_tx_antenna(settings.m_antennaPath.toStdString(), m_deviceShared.m_channel);
                qDebug("USRPOutput::applySettings: set antenna path to %s on channel %d", qPrintable(settings.m_antennaPath), m_deviceShared.m_channel);
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

        if (checkRates)
        {
            // Check if requested rate could actually be met and what master clock rate we ended up with
            double actualSampleRate = m_deviceShared.m_deviceParams->getDevice()->get_tx_rate(m_deviceShared.m_channel);
            qDebug("USRPOutput::applySettings: actual sample rate %f", actualSampleRate);
            double masterClockRate = m_deviceShared.m_deviceParams->getDevice()->get_master_clock_rate();
            qDebug("USRPOutput::applySettings: master_clock_rate %f", masterClockRate);
            m_settings.m_devSampleRate = actualSampleRate;
            m_settings.m_masterClockRate = masterClockRate;
        }

        // forward changes to buddies or oneself

        if (forwardChangeAllDSP)
        {
            qDebug("USRPOutput::applySettings: forward change to all buddies");

            // send to self first
            DSPSignalNotification *notif = new DSPSignalNotification(
                    m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp),
                    m_settings.m_centerFrequency);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

            // send to sink buddies
            const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
            std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

            for (; itSink != sinkBuddies.end(); ++itSink)
            {
                DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                        m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, false);
                (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
            }

            // send to source buddies
            const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
            std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

            for (; itSource != sourceBuddies.end(); ++itSource)
            {
                DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                        m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, false);
                (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
            }

            // send to GUI so it can see master clock rate and if actual rate differs
            if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
            {
                DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                        m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, false);
                m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
            }
        }
        else if (forwardChangeTxDSP)
        {
            qDebug("USRPOutput::applySettings: forward change to Tx buddies");

            int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp);

            // send to self first
            DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

            // send to sink buddies
            const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
            std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

            for (; itSink != sinkBuddies.end(); ++itSink)
            {
                DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                        m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, false);
                (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
            }
        }
        else if (forwardChangeOwnDSP)
        {
            qDebug("USRPOutput::applySettings: forward change to self only");

            int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp);
            DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
        }

        if (forwardClockSource)
        {
            // get what clock is actually set, in case requested clock couldn't be set
            if (m_deviceShared.m_deviceParams->getDevice())
            {
                try
                {
                    m_settings.m_clockSource = QString::fromStdString(m_deviceShared.m_deviceParams->getDevice()->get_clock_source(0));
                    qDebug() << "USRPOutput::applySettings: clock source is " << m_settings.m_clockSource;
                }
                catch (std::exception &e)
                {
                    qDebug() << "USRPOutput::applySettings: could not get clock source";
                }
            }

            // send to GUI in case requested clock isn't detected
            if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
            {
                DeviceUSRPShared::MsgReportClockSourceChange *report = DeviceUSRPShared::MsgReportClockSourceChange::create(
                        m_settings.m_clockSource);
                m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
            }

            // send to source buddies
            const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
            std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

            for (; itSource != sourceBuddies.end(); ++itSource)
            {
                DeviceUSRPShared::MsgReportClockSourceChange *report = DeviceUSRPShared::MsgReportClockSourceChange::create(
                        m_settings.m_clockSource);
                (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
            }

            // send to sink buddies
            const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
            std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

            for (; itSink != sinkBuddies.end(); ++itSink)
            {
                DeviceUSRPShared::MsgReportClockSourceChange *report = DeviceUSRPShared::MsgReportClockSourceChange::create(
                        m_settings.m_clockSource);
                (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
            }
        }

        return true;
    }
    catch (std::exception &e)
    {
        qDebug() << "USRPOutput::applySettings: exception: " << e.what();
        return false;
    }
}

int USRPOutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setUsrpOutputSettings(new SWGSDRangel::SWGUSRPOutputSettings());
    response.getUsrpOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int USRPOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    USRPOutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureUSRP *msg = MsgConfigureUSRP::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureUSRP *msgToGUI = MsgConfigureUSRP::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void USRPOutput::webapiUpdateDeviceSettings(
        USRPOutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("antennaPath")) {
        settings.m_antennaPath = *response.getUsrpOutputSettings()->getAntennaPath();
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getUsrpOutputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getUsrpOutputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("loOffset")) {
        settings.m_loOffset = response.getUsrpOutputSettings()->getLoOffset();
    }
    if (deviceSettingsKeys.contains("clockSource")) {
        settings.m_clockSource = *response.getUsrpOutputSettings()->getClockSource();
    }
    if (deviceSettingsKeys.contains("gain")) {
        settings.m_gain = response.getUsrpOutputSettings()->getGain();
    }
    if (deviceSettingsKeys.contains("log2SoftInterp")) {
        settings.m_log2SoftInterp = response.getUsrpOutputSettings()->getLog2SoftInterp();
    }
    if (deviceSettingsKeys.contains("lpfBW")) {
        settings.m_lpfBW = response.getUsrpOutputSettings()->getLpfBw();
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getUsrpOutputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getUsrpOutputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getUsrpOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getUsrpOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getUsrpOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getUsrpOutputSettings()->getReverseApiDeviceIndex();
    }
}

int USRPOutput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setUsrpOutputReport(new SWGSDRangel::SWGUSRPOutputReport());
    response.getUsrpOutputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}
void USRPOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const USRPOutputSettings& settings)
{
    response.getUsrpOutputSettings()->setAntennaPath(new QString(settings.m_antennaPath));
    response.getUsrpOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getUsrpOutputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getUsrpOutputSettings()->setLoOffset(settings.m_loOffset);
    response.getUsrpOutputSettings()->setClockSource(new QString(settings.m_clockSource));
    response.getUsrpOutputSettings()->setGain(settings.m_gain);
    response.getUsrpOutputSettings()->setLog2SoftInterp(settings.m_log2SoftInterp);
    response.getUsrpOutputSettings()->setLpfBw(settings.m_lpfBW);
    response.getUsrpOutputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getUsrpOutputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getUsrpOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getUsrpOutputSettings()->getReverseApiAddress()) {
        *response.getUsrpOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getUsrpOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getUsrpOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getUsrpOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int USRPOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int USRPOutput::webapiRun(
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

void USRPOutput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    bool success = false;
    bool active = false;
    quint32 underflows = 0;
    quint32 droppedPackets = 0;

    if ((m_streamId != nullptr) && (m_usrpOutputThread != nullptr) && m_channelAcquired)
    {
        m_usrpOutputThread->getStreamStatus(active, underflows, droppedPackets);
        success = true;
    }

    response.getUsrpOutputReport()->setSuccess(success ? 1 : 0);
    response.getUsrpOutputReport()->setStreamActive(active ? 1 : 0);
    response.getUsrpOutputReport()->setUnderrunCount(underflows);
    response.getUsrpOutputReport()->setDroppedPacketsCount(droppedPackets);
}

void USRPOutput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const USRPOutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("USRP"));
    swgDeviceSettings->setUsrpOutputSettings(new SWGSDRangel::SWGUSRPOutputSettings());
    SWGSDRangel::SWGUSRPOutputSettings *swgUsrpOutputSettings = swgDeviceSettings->getUsrpOutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("antennaPath") || force) {
        swgUsrpOutputSettings->setAntennaPath(new QString(settings.m_antennaPath));
    }
    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgUsrpOutputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgUsrpOutputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("loOffset") || force) {
        swgUsrpOutputSettings->setLoOffset(settings.m_loOffset);
    }
    if (deviceSettingsKeys.contains("clockSource") || force) {
        swgUsrpOutputSettings->setClockSource(new QString(settings.m_clockSource));
    }
    if (deviceSettingsKeys.contains("gain") || force) {
        swgUsrpOutputSettings->setGain(settings.m_gain);
    }
    if (deviceSettingsKeys.contains("log2SoftInterp") || force) {
        swgUsrpOutputSettings->setLog2SoftInterp(settings.m_log2SoftInterp);
    }
    if (deviceSettingsKeys.contains("lpfBW") || force) {
        swgUsrpOutputSettings->setLpfBw(settings.m_lpfBW);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgUsrpOutputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgUsrpOutputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
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

void USRPOutput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("USRP"));

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

void USRPOutput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "USRPOutput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("USRPOutput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
