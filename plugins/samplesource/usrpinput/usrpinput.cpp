///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>         //
// Copyright (C) 2020, 2022 Jon Beniston, M7RCE <jon@beniston.com>               //
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
#include <cmath>
#include <string.h>

#include <QMutexLocker>
#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>
#include <QTimer>

#include <uhd/usrp/multi_usrp.hpp>

#include "SWGDeviceSettings.h"
#include "SWGUSRPInputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGUSRPInputReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "util/poweroftwo.h"
#include "usrpinput.h"
#include "usrpinputthread.h"
#include "usrp/deviceusrpparam.h"
#include "usrp/deviceusrpshared.h"
#include "usrp/deviceusrp.h"

MESSAGE_CLASS_DEFINITION(USRPInput::MsgConfigureUSRP, Message)
MESSAGE_CLASS_DEFINITION(USRPInput::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(USRPInput::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(USRPInput::MsgReportStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(USRPInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(USRPInput::MsgSaveReplay, Message)

USRPInput::USRPInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_usrpInputThread(nullptr),
    m_deviceDescription("USRPInput"),
    m_running(false),
    m_channelAcquired(false),
    m_bufSamples(0)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_streamId = nullptr;
    // Hold the shared device mutex (if there are buddies) across suspend...resume, so it can't interleave
    // with buddies doing the same in their device engine threads
    DeviceUSRPParams *buddyDeviceParams = getBuddyDeviceParams();

    if (buddyDeviceParams) {
        buddyDeviceParams->m_mutex.lock();
    }

    suspendRxBuddies();
    suspendTxBuddies();
    openDevice();
    resumeTxBuddies();
    resumeRxBuddies();

    if (buddyDeviceParams) {
        buddyDeviceParams->m_mutex.unlock();
    }

    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &USRPInput::networkManagerFinished
    );
}

USRPInput::~USRPInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &USRPInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    DeviceUSRPParams *deviceParams = m_deviceShared.m_deviceParams;
    bool hasBuddies = (m_deviceAPI->getSourceBuddies().size() > 0) || (m_deviceAPI->getSinkBuddies().size() > 0);

    if (deviceParams)
    {
        // Prevent buddies using our shared data after we've been deleted, but before buddy lists are cleared.
        // Buddies read this in their device engine threads with the device mutex held.
        QMutexLocker deviceLocker(&deviceParams->m_mutex);
        m_deviceAPI->setBuddySharedPtr(nullptr);
    }

    // Hold device mutex across suspend...resume, so it can't interleave with buddies doing the same.
    // If there are no buddies, the device params are deleted in closeDevice, but then there's nothing to protect.
    if (hasBuddies && deviceParams) {
        deviceParams->m_mutex.lock();
    }

    suspendRxBuddies();
    suspendTxBuddies();
    closeDevice();
    resumeTxBuddies();
    resumeRxBuddies();

    if (hasBuddies && deviceParams) {
        deviceParams->m_mutex.unlock();
    }
}

void USRPInput::destroy()
{
    delete this;
}

bool USRPInput::openDevice()
{
    bool ret = true;

    // Set shared pointer first, so it is valid for buddies, even if we return early on error
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    // B210 supports up to 50MSa/s, so a fairly large FIFO is probably a good idea
    // Should it be bigger still?
    if (!m_sampleFifo.setSize(2000000))
    {
        qCritical("USRPInput::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("USRPInput::openDevice: allocated SampleFifo");
    }

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();

    // look for Rx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        qDebug("USRPInput::openDevice: look in Rx buddies");

        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        //m_deviceShared = *((DeviceUSRPShared *) sourceBuddy->getBuddySharedPtr()); // copy shared data
        DeviceUSRPShared *deviceUSRPShared = (DeviceUSRPShared*) sourceBuddy->getBuddySharedPtr();

        if (deviceUSRPShared == 0)
        {
            qCritical("USRPInput::openDevice: the source buddy shared pointer is null");
            return false;
        }

        m_deviceShared.m_deviceParams = deviceUSRPShared->m_deviceParams;

        DeviceUSRPParams *deviceParams = m_deviceShared.m_deviceParams; // get device parameters

        if (deviceParams == 0)
        {
            qCritical("USRPInput::openDevice: cannot get device parameters from Rx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("USRPInput::openDevice: getting device parameters from Rx buddy");
        }

        if (m_deviceAPI->getSourceBuddies().size() == deviceParams->m_nbRxChannels)
        {
            qCritical("USRPInput::openDevice: no more Rx channels available in device");
            return false; // no more Rx channels available in device
        }
        else
        {
            qDebug("USRPInput::openDevice: at least one more Rx channel is available in device");
        }

        // check if the requested channel is busy and abort if so (should not happen if device management is working correctly)

        for (unsigned int i = 0; i < m_deviceAPI->getSourceBuddies().size(); i++)
        {
            DeviceAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
            DeviceUSRPShared *buddyShared = (DeviceUSRPShared *) buddy->getBuddySharedPtr();

            if (buddyShared && (buddyShared->m_channel == requestedChannel))
            {
                qCritical("USRPInput::openDevice: cannot open busy channel %u", requestedChannel);
                return false;
            }
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // look for Tx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("USRPInput::openDevice: look in Tx buddies");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        //m_deviceShared = *((DeviceUSRPShared *) sinkBuddy->getBuddySharedPtr()); // copy parameters
        DeviceUSRPShared *deviceUSRPShared = (DeviceUSRPShared*) sinkBuddy->getBuddySharedPtr();

        if (deviceUSRPShared == 0)
        {
            qCritical("USRPInput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        m_deviceShared.m_deviceParams = deviceUSRPShared->m_deviceParams;

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("USRPInput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("USRPInput::openDevice: getting device parameters from Tx buddy");
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // There are no buddies then create the first USRP common parameters
    // open the device this will also populate common fields
    // take the first Rx channel
    else
    {
        qDebug("USRPInput::openDevice: open device here");

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
            qCritical("USRPInput::openDevice: failed to open device");
            // We need to set setBuddySharedPtr below even if open fails
            ret = false;
        }
        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }

    return ret;
}

// Get device parameters shared with buddies, or nullptr if there are no buddies. Called from GUI thread.
DeviceUSRPParams *USRPInput::getBuddyDeviceParams() const
{
    for (const auto& buddy : m_deviceAPI->getSourceBuddies())
    {
        DeviceUSRPShared *buddyShared = (DeviceUSRPShared *) buddy->getBuddySharedPtr();

        if (buddyShared && buddyShared->m_deviceParams) {
            return buddyShared->m_deviceParams;
        }
    }

    for (const auto& buddy : m_deviceAPI->getSinkBuddies())
    {
        DeviceUSRPShared *buddyShared = (DeviceUSRPShared *) buddy->getBuddySharedPtr();

        if (buddyShared && buddyShared->m_deviceParams) {
            return buddyShared->m_deviceParams;
        }
    }

    return nullptr;
}

void USRPInput::suspendRxBuddies()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("USRPInput::suspendRxBuddies (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceUSRPShared *buddySharedPtr = (DeviceUSRPShared *) (*itSource)->getBuddySharedPtr();

        if (!buddySharedPtr || !buddySharedPtr->m_deviceParams) {
            continue;
        }

        // Device mutex prevents the buddy deleting its thread while we use it
        QMutexLocker deviceLocker(&buddySharedPtr->m_deviceParams->m_mutex);

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

void USRPInput::suspendTxBuddies()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("USRPInput::suspendTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceUSRPShared *buddySharedPtr = (DeviceUSRPShared *) (*itSink)->getBuddySharedPtr();

        if (!buddySharedPtr || !buddySharedPtr->m_deviceParams) {
            continue;
        }

        // Device mutex prevents the buddy deleting its thread while we use it
        QMutexLocker deviceLocker(&buddySharedPtr->m_deviceParams->m_mutex);

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

void USRPInput::resumeRxBuddies()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("USRPInput::resumeRxBuddies (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceUSRPShared *buddySharedPtr = (DeviceUSRPShared *) (*itSource)->getBuddySharedPtr();

        if (!buddySharedPtr || !buddySharedPtr->m_deviceParams) {
            continue;
        }

        // Device mutex prevents the buddy deleting its thread while we use it
        QMutexLocker deviceLocker(&buddySharedPtr->m_deviceParams->m_mutex);

        if (buddySharedPtr->m_threadWasRunning && buddySharedPtr->m_thread) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void USRPInput::resumeTxBuddies()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("USRPInput::resumeTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceUSRPShared *buddySharedPtr = (DeviceUSRPShared *) (*itSink)->getBuddySharedPtr();

        if (!buddySharedPtr || !buddySharedPtr->m_deviceParams) {
            continue;
        }

        // Device mutex prevents the buddy deleting its thread while we use it
        QMutexLocker deviceLocker(&buddySharedPtr->m_deviceParams->m_mutex);

        if (buddySharedPtr->m_threadWasRunning && buddySharedPtr->m_thread) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void USRPInput::closeDevice()
{
    if (m_running) { stop(); }

    m_deviceShared.m_channel = -1; // effectively release the channel for the possible buddies

    if (m_deviceShared.m_deviceParams == nullptr) { // was never created
        return;
    }

    // No buddies so effectively close the device
    // (params are deleted even if the device failed to open, to avoid a leak)

    if ((m_deviceAPI->getSourceBuddies().size() == 0) && (m_deviceAPI->getSinkBuddies().size() == 0))
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = nullptr;
    }
}

bool USRPInput::acquireChannel(const USRPInputSettings& settings)
{
    bool success = true;

    // Hold buddies and device mutexes across suspend...resume, so buddies doing the same at the same time
    // can't interleave (which could leave a buddy suspended), and buddy lists can't be changed while we use them
    QMutexLocker buddiesLocker(&DeviceAPI::getBuddiesMutex());
    QMutexLocker deviceLocker(&m_deviceShared.m_deviceParams->m_mutex);

    suspendRxBuddies();
    suspendTxBuddies();

    if (m_streamId == nullptr)
    {
        // Prevent buddies from configuring the device while we set up the stream
        QMutexLocker deviceLocker(&m_deviceShared.m_deviceParams->m_mutex);

        try
        {
            uhd::usrp::multi_usrp::sptr usrp = m_deviceShared.m_deviceParams->getDevice();

            // Apply settings before creating stream
            // However, don't set LPF to <10MHz at this stage, otherwise there is massive TX LO leakage
            // Actual settings are reported back to handleMessage, which forwards them to buddies and GUI
            USRPInputThread::applyDeviceSettings(m_deviceShared.m_deviceParams, m_deviceShared.m_channel, settings, QList<QString>(), true, true, false, getInputMessageQueue());
            usrp->set_rx_bandwidth(56000000, m_deviceShared.m_channel);

            // set up the stream
            std::string cpu_format("sc16");
            std::string wire_format("sc16");
            std::vector<size_t> channel_nums;
            channel_nums.push_back(m_deviceShared.m_channel);

            uhd::stream_args_t stream_args(cpu_format, wire_format);
            stream_args.channels = channel_nums;

            m_streamId = usrp->get_rx_stream(stream_args);

            // Read multiple packets per recv() call at high sample rates, to reduce overhead
            m_bufSamples = DeviceUSRP::getRecvBufferSamples(m_streamId->get_max_num_samps(), settings.m_devSampleRate);

            // Wait for reference and LO to lock. Use actual clock source, in case requested clock wasn't detected
            DeviceUSRP::waitForLock(usrp, QString::fromStdString(usrp->get_clock_source(0)), m_deviceShared.m_channel, true);

            // Now we can set desired bandwidth
            usrp->set_rx_bandwidth(settings.m_lpfBW, m_deviceShared.m_channel);
        }
        catch (std::exception& e)
        {
            qCritical() << "USRPInput::acquireChannel: exception: " << e.what();
        }

        // Can't run without a stream
        success = m_streamId != nullptr;
    }

    resumeTxBuddies();
    resumeRxBuddies();

    m_channelAcquired = success;

    return success;
}

void USRPInput::releaseChannel()
{
    // Hold buddies and device mutexes across suspend...resume, so buddies doing the same at the same time
    // can't interleave (which could leave a buddy suspended), and buddy lists can't be changed while we use them.
    // This also means the stream is destroyed with the device mutex held.
    QMutexLocker buddiesLocker(&DeviceAPI::getBuddiesMutex());
    QMutexLocker deviceLocker(&m_deviceShared.m_deviceParams->m_mutex);

    suspendRxBuddies();
    suspendTxBuddies();

    // destroy the stream
    m_streamId = nullptr;

    resumeTxBuddies();
    resumeRxBuddies();

    // The channel will be effectively released to be reused in another device set only at close time

    m_channelAcquired = false;
}

void USRPInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

// Called from the device engine thread
bool USRPInput::start()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return true;
    }

    if (!m_deviceShared.m_deviceParams || !m_deviceShared.m_deviceParams->getDevice()) {
        return false;
    }

    // Create thread before acquiring the channel, so settings changed while the channel is
    // being acquired are queued to it, to be applied once start() has returned.
    // start / stop streaming is done in the thread.
    USRPInputThread *thread = new USRPInputThread(m_deviceShared.m_deviceParams, m_deviceShared.m_channel,
        &m_sampleFifo, &m_replayBuffer, getInputMessageQueue());
    // Publish the thread and take a copy of the settings atomically with respect to applySettings, so
    // settings changed in the GUI thread while the channel is being acquired are either in the copy or
    // queued to the thread, to be applied once start() has returned
    USRPInputSettings settings;
    {
        QMutexLocker settingsLocker(&m_settingsMutex);
        QMutexLocker threadLocker(&m_threadMutex);
        m_usrpInputThread = thread;
        settings = m_settings;
    }

    if (!acquireChannel(settings))
    {
        {
            QMutexLocker threadLocker(&m_threadMutex);
            m_usrpInputThread = nullptr;
        }
        delete thread;
        return false;
    }

    {
        QMutexLocker threadLocker(&m_threadMutex);
        thread->setStream(m_streamId, m_bufSamples);
    }
    thread->setLog2Decimation(settings.m_log2SoftDecim);

    {
        // Start streaming and make the thread visible to buddies atomically, so a buddy that is acquiring
        // its channel (holding the device mutex) can't miss suspending us
        QMutexLocker deviceLocker(&m_deviceShared.m_deviceParams->m_mutex);
        thread->startWork();
        m_deviceShared.m_thread = thread;
    }

    qDebug("USRPInput::start: thread started");

    m_running = true;

    return true;
}

// Usually called from the device engine thread, but can be called from the GUI thread in the destructor
void USRPInput::stop()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug("USRPInput::stop");
    m_running = false;

    USRPInputThread *thread;
    {
        QMutexLocker threadLocker(&m_threadMutex);
        thread = m_usrpInputThread;
        m_usrpInputThread = nullptr;
    }

    if (thread)
    {
        // Wait for any settings being applied in the device engine thread to complete and prevent any more
        thread->detach();

        {
            // Clear before deleting the thread, so buddies can't access it while it's being destroyed.
            // Device mutex prevents buddies accessing it while we clear it and stop it
            QMutexLocker deviceLocker(&m_deviceShared.m_deviceParams->m_mutex);
            m_deviceShared.m_thread = nullptr;
            thread->stopWork();
        }

        // Release stream, so it's destroyed before the device, even if the thread is deleted later
        thread->releaseStream();

        QThread *ownerThread = thread->thread();
        if (!ownerThread || (ownerThread == QThread::currentThread()) || ownerThread->isFinished()) {
            delete thread;
        } else {
            // Called from another thread while the device engine thread is still running, which may have
            // a pending call to handleInputMessages, so delete it in the device engine thread
            thread->deleteLater();
        }
    }

    releaseChannel();
}

QByteArray USRPInput::serialize() const
{
    return m_settings.serialize();
}

bool USRPInput::deserialize(const QByteArray& data)
{
    bool success = true;

    {
        QMutexLocker settingsLocker(&m_settingsMutex);

        if (!m_settings.deserialize(data))
        {
            m_settings.resetToDefaults();
            success = false;
        }
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

const QString& USRPInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int USRPInput::getSampleRate() const
{
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2SoftDecim));
}

quint64 USRPInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void USRPInput::setCenterFrequency(qint64 centerFrequency)
{
    USRPInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureUSRP* message = MsgConfigureUSRP::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureUSRP* messageToGUI = MsgConfigureUSRP::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

int USRPInput::getChannelIndex()
{
    return m_deviceShared.m_channel;
}

void USRPInput::getLORange(float& minF, float& maxF) const
{
    if (!m_deviceShared.m_deviceParams)
    {
        minF = 0.0f;
        maxF = 0.0f;
        return;
    }

    try
    {
        minF = m_deviceShared.m_deviceParams->m_loRangeRx.start();
        maxF = m_deviceShared.m_deviceParams->m_loRangeRx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInput::getLORange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

void USRPInput::getSRRange(float& minF, float& maxF) const
{
    if (!m_deviceShared.m_deviceParams)
    {
        minF = 0.0f;
        maxF = 0.0f;
        return;
    }

    try
    {
        minF = m_deviceShared.m_deviceParams->m_srRangeRx.start();
        maxF = m_deviceShared.m_deviceParams->m_srRangeRx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInput::getSRRange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

void USRPInput::getLPRange(float& minF, float& maxF) const
{
    if (!m_deviceShared.m_deviceParams)
    {
        minF = 0.0f;
        maxF = 0.0f;
        return;
    }

    try
    {
        minF = m_deviceShared.m_deviceParams->m_lpfRangeRx.start();
        maxF = m_deviceShared.m_deviceParams->m_lpfRangeRx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInput::getLPRange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

void USRPInput::getGainRange(float& minF, float& maxF) const
{
    if (!m_deviceShared.m_deviceParams)
    {
        minF = 0.0f;
        maxF = 0.0f;
        return;
    }

    try
    {
        minF = m_deviceShared.m_deviceParams->m_gainRangeRx.start();
        maxF = m_deviceShared.m_deviceParams->m_gainRangeRx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInput::getGainRange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

QStringList USRPInput::getRxAntennas() const
{
    return m_deviceShared.m_deviceParams ? m_deviceShared.m_deviceParams->m_rxAntennas : QStringList();
}

QStringList USRPInput::getRxGainNames() const
{
    return m_deviceShared.m_deviceParams ? m_deviceShared.m_deviceParams->m_rxGainNames : QStringList();
}

QStringList USRPInput::getClockSources() const
{
    return m_deviceShared.m_deviceParams ? m_deviceShared.m_deviceParams->m_clockSources : QStringList();
}

bool USRPInput::handleMessage(const Message& message)
{
    if (MsgConfigureUSRP::match(message))
    {
        MsgConfigureUSRP& conf = (MsgConfigureUSRP&) message;
        qDebug() << "USRPInput::handleMessage: MsgConfigureUSRP";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce()))
        {
            qDebug("USRPInput::handleMessage config error");
        }

        return true;
    }
    else if (DeviceUSRPShared::MsgReportDeviceSettings::match(message))
    {
        // Settings actually set on the device, as read back in the device engine thread
        DeviceUSRPShared::MsgReportDeviceSettings& report = (DeviceUSRPShared::MsgReportDeviceSettings&) message;
        QMutexLocker settingsLocker(&m_settingsMutex);

        if (report.getClockSourceValid())
        {
            m_settings.m_clockSource = report.getClockSource();
            forwardClockSource();
        }

        if (report.getSampleRateValid())
        {
            int devSampleRate = (int) std::round(report.getSampleRate());
            int masterClockRate = (int) std::round(report.getMasterClockRate());
            qDebug() << "USRPInput::handleMessage: MsgReportDeviceSettings: devSampleRate:" << devSampleRate << "masterClockRate:" << masterClockRate;

            if ((devSampleRate != m_settings.m_devSampleRate) || (masterClockRate != m_settings.m_masterClockRate))
            {
                if (devSampleRate != m_settings.m_devSampleRate)
                {
                    m_replayBuffer.clear();
                    m_replayBuffer.setSize(m_settings.m_replayLength, devSampleRate);
                    m_replayBuffer.setReadOffset(((unsigned)(m_settings.m_replayOffset * devSampleRate)) * 2);
                }

                m_settings.m_devSampleRate = devSampleRate;
                m_settings.m_masterClockRate = masterClockRate;

                if (!report.getForwardToBuddies()) {
                    notifySampleRateChange();
                }
            }

            // Result of our own settings change, so the device has changed and buddies need to be informed,
            // even if the rate is as we requested, as buddies won't have been informed when we weren't running
            if (report.getForwardToBuddies()) {
                forwardChangeAllDSP();
            }
        }

        return true;
    }
    else if (DeviceUSRPShared::MsgReportBuddyChange::match(message))
    {
        DeviceUSRPShared::MsgReportBuddyChange& report = (DeviceUSRPShared::MsgReportBuddyChange&) message;
        QMutexLocker settingsLocker(&m_settingsMutex);
        uhd::usrp::multi_usrp::sptr device = m_deviceShared.m_deviceParams ? m_deviceShared.m_deviceParams->getDevice() : nullptr;

        // Rx buddy changed settings. Only copy them if the buddy's channel shares our LO (E.g. B210),
        // not if it has an independent front-end (E.g. X310 with two daughterboards)
        if (report.getRxElseTx() && DeviceUSRP::channelsShareLO(device, true, report.getChannel(), m_deviceShared.m_channel))
        {
            m_settings.m_devSampleRate   = report.getDevSampleRate();
            m_settings.m_centerFrequency = report.getCenterFrequency();
            m_settings.m_loOffset        = report.getLOOffset();
        }
        // Master clock rate is common between all buddies
        int masterClockRate = report.getMasterClockRate();
        if (masterClockRate > 0)
            m_settings.m_masterClockRate = masterClockRate;
        qDebug() << "USRPInput::handleMessage MsgReportBuddyChange";
        qDebug() << "m_masterClockRate " << m_settings.m_masterClockRate;

        // A buddy changing the master clock rate may have changed our sample rate, so have the device
        // engine thread read back the actual rate, which will be reported via MsgReportDeviceSettings
        {
            QMutexLocker threadLocker(&m_threadMutex);

            if (m_usrpInputThread) {
                m_usrpInputThread->getInputMessageQueue()->push(DeviceUSRPShared::MsgReadDeviceSampleRate::create());
            }
        }

        notifySampleRateChange();

        return true;
    }
    else if (DeviceUSRPShared::MsgReportClockSourceChange::match(message))
    {
        DeviceUSRPShared::MsgReportClockSourceChange& report = (DeviceUSRPShared::MsgReportClockSourceChange&) message;
        QMutexLocker settingsLocker(&m_settingsMutex);

        m_settings.m_clockSource  = report.getClockSource();

        if (getMessageQueueToGUI())
        {
            DeviceUSRPShared::MsgReportClockSourceChange *reportToGUI = DeviceUSRPShared::MsgReportClockSourceChange::create(
                    m_settings.m_clockSource);
            getMessageQueueToGUI()->push(reportToGUI);
        }

        return true;
    }
    else if (MsgGetDeviceInfo::match(message))
    {
        // Reading a sensor is quick, so do it here, but don't block the GUI thread
        // if the device is being configured in a device engine thread
        DeviceUSRPParams *deviceParams = m_deviceShared.m_deviceParams;

        if (deviceParams && deviceParams->getDevice() && deviceParams->m_mutex.tryLock())
        {
            double temperature = 0.0;
            bool temperatureValid = DeviceUSRP::getTemperature(deviceParams->getDevice(), true, m_deviceShared.m_channel, temperature);
            deviceParams->m_mutex.unlock();
            forwardDeviceInfo(temperatureValid, temperature);
        }

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
        if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
        {
            QMutexLocker threadLocker(&m_threadMutex);

            if (m_usrpInputThread)
            {
                bool active;
                quint32 overflows;
                quint32 timeouts;

                m_usrpInputThread->getStreamStatus(active, overflows, timeouts);
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        true,
                        active,
                        overflows,
                        timeouts);
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
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "USRPInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (MsgSaveReplay::match(message))
    {
        MsgSaveReplay& cmd = (MsgSaveReplay&) message;
        m_replayBuffer.save(cmd.getFilename(), m_settings.m_devSampleRate, getCenterFrequency());
        return true;
    }
    else
    {
        return false;
    }
}

bool USRPInput::applySettings(const USRPInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    QMutexLocker settingsLocker(&m_settingsMutex);
    qDebug() << "USRPInput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    bool changeOwnDSP = false;
    bool changeRxDSP  = false;
    bool changeAllDSP = false;

    if (settingsKeys.contains("devSampleRate") || force)
    {
        changeAllDSP = true;

        if (settings.m_devSampleRate != m_settings.m_devSampleRate) {
            m_replayBuffer.clear();
        }
    }

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("loOffset")
        || settingsKeys.contains("transverterMode")
        || settingsKeys.contains("transverterDeltaFrequency")
        || force)
    {
        changeRxDSP = true;

        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
        m_deviceShared.m_centerFrequency = deviceCenterFrequency; // for buddies
    }

    if (settingsKeys.contains("log2SoftDecim") || force)
    {
        changeOwnDSP = true;
        m_deviceShared.m_log2Soft = settings.m_log2SoftDecim; // for buddies
    }

    // Applying settings to the device can take a long time (E.g. sample rate, clock source and bandwidth),
    // so when running, they are applied in the device engine thread, rather than blocking the GUI thread.
    // Values actually set are reported back via DeviceUSRPShared::MsgReportDeviceSettings.
    // When not running, they are applied when the channel is acquired in start(), except for GPIO,
    // which doesn't require the channel to be acquired.
    bool threadRunning = false;
    {
        QMutexLocker threadLocker(&m_threadMutex);

        if (m_usrpInputThread)
        {
            m_usrpInputThread->getInputMessageQueue()->push(MsgConfigureUSRP::create(settings, settingsKeys, force));
            threadRunning = true;
        }
    }

    if (!threadRunning && m_deviceShared.m_deviceParams && (settingsKeys.contains("gpioDir") || settingsKeys.contains("gpioPins") || force))
    {
        // Don't block the GUI thread if a buddy is configuring the device (E.g. waiting for LO lock). Retry later instead.
        if (m_deviceShared.m_deviceParams->m_mutex.tryLock())
        {
            USRPInputThread::applyDeviceSettings(m_deviceShared.m_deviceParams, m_deviceShared.m_channel, settings, settingsKeys, force, false, false, nullptr);
            m_deviceShared.m_deviceParams->m_mutex.unlock();
        }
        else
        {
            QTimer::singleShot(100, this, [this]() {
                m_inputMessageQueue.push(MsgConfigureUSRP::create(m_settings, QList<QString>{"gpioDir", "gpioPins"}, false));
            });
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

    if (settingsKeys.contains("replayLength") || settingsKeys.contains("devSampleRate") || force) {
        m_replayBuffer.setSize(m_settings.m_replayLength, m_settings.m_devSampleRate);
    }

    if (settingsKeys.contains("replayOffset") || settingsKeys.contains("devSampleRate")  || force) {
        m_replayBuffer.setReadOffset(((unsigned)(m_settings.m_replayOffset * m_settings.m_devSampleRate)) * 2);
    }

    if (settingsKeys.contains("replayLoop") || force) {
        m_replayBuffer.setLoop(m_settings.m_replayLoop);
    }

    // forward changes to buddies or oneself
    // If the device coerces the sample rate, this will be sent again with the actual rate, once reported

    // When not running, the device hasn't been changed, so buddies don't need to be informed.
    // They will be informed of the actual settings when we are started.
    if (!threadRunning)
    {
        if (changeAllDSP || changeRxDSP || changeOwnDSP) {
            forwardChangeOwnDSP();
        }
    }
    else if (changeAllDSP)
    {
        forwardChangeAllDSP();
    }
    else if (changeRxDSP)
    {
        forwardChangeRxDSP();
    }
    else if (changeOwnDSP)
    {
        forwardChangeOwnDSP();
    }

    return true;
}

// Forward sample rate and frequency to self, all buddies and GUI
void USRPInput::forwardChangeAllDSP()
{
    qDebug("USRPInput::forwardChangeAllDSP");

    // send to self first
    forwardChangeOwnDSP();

    // send to source buddies
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, true, m_deviceShared.m_channel);
        (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
    }

    // send to sink buddies
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, true, m_deviceShared.m_channel);
        (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
    }

    // send to GUI so it can see master clock rate and if actual rate differs
    if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
    {
        DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, true, m_deviceShared.m_channel);
        m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
    }
}

// Forward sample rate and frequency to self and Rx buddies
void USRPInput::forwardChangeRxDSP()
{
    qDebug("USRPInput::forwardChangeRxDSP");

    // send to self first
    forwardChangeOwnDSP();

    // send to source buddies
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, true, m_deviceShared.m_channel);
        (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
    }
}

// Forward sample rate and frequency to self only
void USRPInput::forwardChangeOwnDSP()
{
    int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim);
    DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
}

// Forward sample rate and frequency to self and GUI, but not buddies (E.g. when changed by a buddy)
void USRPInput::notifySampleRateChange()
{
    forwardChangeOwnDSP();

    if (getMessageQueueToGUI())
    {
        DeviceUSRPShared::MsgReportBuddyChange *reportToGUI = DeviceUSRPShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, true);
        getMessageQueueToGUI()->push(reportToGUI);
    }
}

// Forward clock source to GUI and all buddies
void USRPInput::forwardClockSource()
{
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

// Forward device info to our GUI and the GUIs of all buddies
void USRPInput::forwardDeviceInfo(bool temperatureValid, float temperature)
{
    if (m_deviceAPI->getSamplingDeviceGUIMessageQueue()) {
        m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(DeviceUSRPShared::MsgReportDeviceInfo::create(temperatureValid, temperature));
    }

    for (const auto& buddy : m_deviceAPI->getSourceBuddies())
    {
        if (buddy->getSamplingDeviceGUIMessageQueue()) {
            buddy->getSamplingDeviceGUIMessageQueue()->push(DeviceUSRPShared::MsgReportDeviceInfo::create(temperatureValid, temperature));
        }
    }

    for (const auto& buddy : m_deviceAPI->getSinkBuddies())
    {
        if (buddy->getSamplingDeviceGUIMessageQueue()) {
            buddy->getSamplingDeviceGUIMessageQueue()->push(DeviceUSRPShared::MsgReportDeviceInfo::create(temperatureValid, temperature));
        }
    }
}

int USRPInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setUsrpInputSettings(new SWGSDRangel::SWGUSRPInputSettings());
    response.getUsrpInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int USRPInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    USRPInputSettings settings = m_settings;
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

void USRPInput::webapiUpdateDeviceSettings(
        USRPInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("title")) {
        settings.m_title = *response.getUsrpInputSettings()->getTitle();
    }
    if (deviceSettingsKeys.contains("antennaPath")) {
        settings.m_antennaPath = *response.getUsrpInputSettings()->getAntennaPath();
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getUsrpInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("loOffset")) {
        settings.m_loOffset = response.getUsrpInputSettings()->getLoOffset();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getUsrpInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getUsrpInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("clockSource")) {
        settings.m_clockSource = *response.getUsrpInputSettings()->getClockSource();
    }
    if (deviceSettingsKeys.contains("gain")) {
        settings.m_gain = response.getUsrpInputSettings()->getGain();
    }
    if (deviceSettingsKeys.contains("gainMode")) {
        settings.m_gainMode = (USRPInputSettings::GainMode) response.getUsrpInputSettings()->getGainMode();
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getUsrpInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("log2SoftDecim")) {
        settings.m_log2SoftDecim = qBound(0, response.getUsrpInputSettings()->getLog2SoftDecim(), 6);
    }
    if (deviceSettingsKeys.contains("lpfBW")) {
        settings.m_lpfBW = response.getUsrpInputSettings()->getLpfBw();
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getUsrpInputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getUsrpInputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("gpioDir")) {
        settings.m_gpioDir = response.getUsrpInputSettings()->getGpioDir();
    }
    if (deviceSettingsKeys.contains("gpioPins")) {
        settings.m_gpioPins = response.getUsrpInputSettings()->getGpioPins();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getUsrpInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getUsrpInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getUsrpInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getUsrpInputSettings()->getReverseApiDeviceIndex();
    }
}

void USRPInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const USRPInputSettings& settings)
{
    if (response.getUsrpInputSettings()->getTitle()) {
        *response.getUsrpInputSettings()->getTitle() = settings.m_title;
    } else {
        response.getUsrpInputSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getUsrpInputSettings()->getAntennaPath()) {
        *response.getUsrpInputSettings()->getAntennaPath() = settings.m_antennaPath;
    } else {
        response.getUsrpInputSettings()->setAntennaPath(new QString(settings.m_antennaPath));
    }
    response.getUsrpInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getUsrpInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getUsrpInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getUsrpInputSettings()->setLoOffset(settings.m_loOffset);
    if (response.getUsrpInputSettings()->getClockSource()) {
        *response.getUsrpInputSettings()->getClockSource() = settings.m_clockSource;
    } else {
        response.getUsrpInputSettings()->setClockSource(new QString(settings.m_clockSource));
    }
    response.getUsrpInputSettings()->setGain(settings.m_gain);
    response.getUsrpInputSettings()->setGainMode((int) settings.m_gainMode);
    response.getUsrpInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getUsrpInputSettings()->setLog2SoftDecim(settings.m_log2SoftDecim);
    response.getUsrpInputSettings()->setLpfBw(settings.m_lpfBW);
    response.getUsrpInputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getUsrpInputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getUsrpInputSettings()->setGpioDir(settings.m_gpioDir);
    response.getUsrpInputSettings()->setGpioPins(settings.m_gpioPins);
    response.getUsrpInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getUsrpInputSettings()->getReverseApiAddress()) {
        *response.getUsrpInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getUsrpInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getUsrpInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getUsrpInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int USRPInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setUsrpInputReport(new SWGSDRangel::SWGUSRPInputReport());
    response.getUsrpInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int USRPInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int USRPInput::webapiRun(
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

void USRPInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    bool success = false;
    bool active = false;
    quint32 overflows = 0;
    quint32 timeouts = 0;

    QMutexLocker threadLocker(&m_threadMutex);

    if (m_usrpInputThread)
    {
        m_usrpInputThread->getStreamStatus(active, overflows, timeouts);
        success = true;
    }

    response.getUsrpInputReport()->setSuccess(success ? 1 : 0);
    response.getUsrpInputReport()->setStreamActive(active ? 1 : 0);
    response.getUsrpInputReport()->setOverrunCount(overflows);
    response.getUsrpInputReport()->setTimeoutCount(timeouts);
}

void USRPInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const USRPInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("USRP"));
    swgDeviceSettings->setUsrpInputSettings(new SWGSDRangel::SWGUSRPInputSettings());
    SWGSDRangel::SWGUSRPInputSettings *swgUsrpInputSettings = swgDeviceSettings->getUsrpInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("title") || force) {
        swgUsrpInputSettings->setTitle(new QString(settings.m_title));
    }
    if (deviceSettingsKeys.contains("antennaPath") || force) {
        swgUsrpInputSettings->setAntennaPath(new QString(settings.m_antennaPath));
    }
    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgUsrpInputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("loOffset") || force) {
        swgUsrpInputSettings->setLoOffset(settings.m_loOffset);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgUsrpInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgUsrpInputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("clockSource") || force) {
        swgUsrpInputSettings->setClockSource(new QString(settings.m_clockSource));
    }
    if (deviceSettingsKeys.contains("gain") || force) {
        swgUsrpInputSettings->setGain(settings.m_gain);
    }
    if (deviceSettingsKeys.contains("gainMode") || force) {
        swgUsrpInputSettings->setGainMode((int) settings.m_gainMode);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgUsrpInputSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("log2SoftDecim") || force) {
        swgUsrpInputSettings->setLog2SoftDecim(settings.m_log2SoftDecim);
    }
    if (deviceSettingsKeys.contains("lpfBW") || force) {
        swgUsrpInputSettings->setLpfBw(settings.m_lpfBW);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgUsrpInputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgUsrpInputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("gpioDir") || force) {
        swgUsrpInputSettings->setGpioDir(settings.m_gpioDir);
    }
    if (deviceSettingsKeys.contains("gpioPins") || force) {
        swgUsrpInputSettings->setGpioPins(settings.m_gpioPins);
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

void USRPInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
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

void USRPInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "USRPInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("USRPInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
