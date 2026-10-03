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
#include "SWGUSRPOutputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGUSRPOutputReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
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

    // Call unconditionally, rather than checking m_running, as m_running is only set at the end of start(),
    // which could still be running in the device engine thread. stop() checks m_running with m_mutex held,
    // so waits for start() to complete. This must be done before the device mutex is locked below,
    // as stop() locks the buddies mutex, which must be locked before the device mutex
    stop();

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

void USRPOutput::destroy()
{
    delete this;
}

bool USRPOutput::openDevice()
{
    bool ret = true;

    // Set shared pointer first, so it is valid for buddies, even if we return early on error
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();

    // look for Tx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSinkBuddies().size() > 0) // look sink sibling first
    {
        qDebug("USRPOutput::openDevice: look in Tx buddies");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        //m_deviceShared = *((DeviceUSRPShared *) sinkBuddy->getBuddySharedPtr()); // copy shared data
        DeviceUSRPShared *deviceUSRPShared = (DeviceUSRPShared*) sinkBuddy->getBuddySharedPtr();

        if (deviceUSRPShared == nullptr)
        {
            qCritical("USRPOutput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

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

            if (buddyShared && (buddyShared->m_channel == requestedChannel))
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

        if (deviceUSRPShared == nullptr)
        {
            qCritical("USRPOutput::openDevice: the source buddy shared pointer is null");
            return false;
        }

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
            // We need to set setBuddySharedPtr below even if open fails
            ret = false;
        }
        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }

    return ret;
}

// Get device parameters shared with buddies, or nullptr if there are no buddies. Called from GUI thread.
DeviceUSRPParams *USRPOutput::getBuddyDeviceParams() const
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

void USRPOutput::suspendRxBuddies()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("USRPOutput::suspendRxBuddies (%lu)", sourceBuddies.size());

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

void USRPOutput::suspendTxBuddies()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("USRPOutput::suspendTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceUSRPShared *buddySharedPtr = (DeviceUSRPShared *) (*itSink)->getBuddySharedPtr();

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

void USRPOutput::resumeRxBuddies()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("USRPOutput::resumeRxBuddies (%lu)", sourceBuddies.size());

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

void USRPOutput::resumeTxBuddies()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("USRPOutput::resumeTxBuddies (%lu)", sinkBuddies.size());

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

void USRPOutput::closeDevice()
{
    // Already stopped in destructor, before the device mutex was locked

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

bool USRPOutput::acquireChannel(const USRPOutputSettings& settings)
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
            USRPOutputThread::applyDeviceSettings(m_deviceShared.m_deviceParams, m_deviceShared.m_channel, settings, QList<QString>(), true, true, false, getInputMessageQueue());
            // Wide bandwidth while setting up the stream is not essential, so don't fail to create the stream if it can't be set
            try {
                usrp->set_tx_bandwidth(56000000, m_deviceShared.m_channel);
            } catch (std::exception& e) {
                qWarning() << "USRPOutput::acquireChannel: could not set bandwidth: " << e.what();
            }

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

            // Wait for reference and LO to lock. Use actual clock source, in case requested clock wasn't detected
            DeviceUSRP::waitForLock(usrp, QString::fromStdString(usrp->get_clock_source(0)), m_deviceShared.m_channel, false);

            // Now we can set desired bandwidth
            usrp->set_tx_bandwidth(settings.m_lpfBW, m_deviceShared.m_channel);
        }
        catch (std::exception& e)
        {
            qCritical() << "USRPOutput::acquireChannel: exception: " << e.what();
        }

        // Can't run without a stream
        success = m_streamId != nullptr;
    }

    resumeTxBuddies();
    resumeRxBuddies();

    m_channelAcquired = success;

    return success;
}

void USRPOutput::releaseChannel()
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

    m_channelAcquired = false;
}

void USRPOutput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

// Called from the device engine thread
bool USRPOutput::start()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return true;
    }

    if (!m_deviceShared.m_deviceParams || !m_deviceShared.m_deviceParams->getDevice()) {
        return false;
    }

    // openDevice() can fail after the device params have been set, without a channel being allocated.
    // UHD would treat channel -1 as all channels, which would reconfigure buddies' channels
    if (m_deviceShared.m_channel < 0)
    {
        qCritical("USRPOutput::start: no channel allocated");
        return false;
    }

    // Create thread before acquiring the channel, so settings changed while the channel is
    // being acquired are queued to it, to be applied once start() has returned.
    USRPOutputThread *thread = new USRPOutputThread(m_deviceShared.m_deviceParams, m_deviceShared.m_channel,
        &m_sampleSourceFifo, getInputMessageQueue());
    // Publish the thread and take a copy of the settings atomically with respect to applySettings, so
    // settings changed in the GUI thread while the channel is being acquired are either in the copy or
    // queued to the thread, to be applied once start() has returned
    USRPOutputSettings settings;
    {
        QMutexLocker settingsLocker(&m_settingsMutex);
        QMutexLocker threadLocker(&m_threadMutex);
        m_usrpOutputThread = thread;
        settings = m_settings;
    }

    if (!acquireChannel(settings))
    {
        {
            QMutexLocker threadLocker(&m_threadMutex);
            m_usrpOutputThread = nullptr;
        }
        reapplyLostGPIO(thread);
        delete thread;
        return false;
    }

    {
        QMutexLocker threadLocker(&m_threadMutex);
        thread->setStream(m_streamId, m_bufSamples);
    }
    thread->setLog2Interpolation(settings.m_log2SoftInterp);

    // Size FIFO for the sample rate, in case a resize was lost when stopping.
    // This is safe here, as we are in the device engine thread (the FIFO writer) and streaming (the reader) hasn't started.
    // Any resize requested since the thread was published is queued to it, to be performed after this.
    {
        unsigned int fifoRate = (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2SoftInterp);
        fifoRate = fifoRate < 48000U ? 48000U : fifoRate; // DeviceUSRPShared::m_sampleFifoMinRate
        m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(fifoRate));
    }

    {
        // Start streaming and make the thread visible to buddies atomically, so a buddy that is acquiring
        // its channel (holding the device mutex) can't miss suspending us
        QMutexLocker deviceLocker(&m_deviceShared.m_deviceParams->m_mutex);
        thread->startWork();
        m_deviceShared.m_thread = thread;
    }

    qDebug("USRPOutput::start: thread started");

    m_running = true;

    return true;
}

// Usually called from the device engine thread, but can be called from the GUI thread in the destructor
void USRPOutput::stop()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug("USRPOutput::stop");
    m_running = false;

    USRPOutputThread *thread;
    {
        QMutexLocker threadLocker(&m_threadMutex);
        thread = m_usrpOutputThread;
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

        // Only clear after streaming has stopped, so resizeSampleSourceFifo doesn't resize the FIFO
        // directly while it is being read. A resize queued to the detached thread is lost, but the
        // FIFO is resized when next started.
        {
            QMutexLocker threadLocker(&m_threadMutex);
            m_usrpOutputThread = nullptr;
        }

        // Release stream, so it's destroyed before the device, even if the thread is deleted later
        thread->releaseStream();
        reapplyLostGPIO(thread);

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

// When the thread exists, settings (including GPIO) are sent to it to be applied. If the thread is then deleted
// (failed start, or stopped) before handling them, they are lost. Other settings are applied when next started,
// but GPIO is also applied when not running, so if a GPIO change was lost, re-apply GPIO in the GUI thread,
// which will use the direct path, as the thread pointer has been cleared.
// Only do this if a change was actually lost, as Rx and Tx buddies share the same GPIO bank, so unnecessarily
// re-applying our settings could overwrite a buddy's.
// Must be called after the thread pointer has been cleared (so no more messages can be queued to the thread)
// and after the thread has been detached (or from the device engine thread, which the thread belongs to),
// so the thread can't be handling its messages. Not needed when called in the GUI thread, as we're being deleted.
void USRPOutput::reapplyLostGPIO(USRPOutputThread *thread)
{
    if (QThread::currentThread() == this->thread()) {
        return;
    }

    bool gpioLost = false;
    Message *message;

    while ((message = thread->getInputMessageQueue()->pop()) != nullptr)
    {
        if (MsgConfigureUSRP::match(*message))
        {
            const MsgConfigureUSRP& conf = (const MsgConfigureUSRP&) *message;
            const QList<QString>& keys = conf.getSettingsKeys();
            gpioLost = gpioLost || conf.getForce() || keys.contains("gpioDir") || keys.contains("gpioPins");
        }

        delete message;
    }

    if (gpioLost)
    {
        // Read the settings when this is handled in the GUI thread, rather than now, so settings changes queued
        // to the GUI thread before this (E.g. from the web API) aren't overwritten with older values
        QMetaObject::invokeMethod(this, [this]() {
            m_inputMessageQueue.push(MsgConfigureUSRP::create(m_settings, QList<QString>{"gpioDir", "gpioPins"}, false));
        }, Qt::QueuedConnection);
    }
}

QByteArray USRPOutput::serialize() const
{
    QMutexLocker settingsLocker(&m_settingsMutex);
    return m_settings.serialize();
}

bool USRPOutput::deserialize(const QByteArray& data)
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

const QString& USRPOutput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int USRPOutput::getSampleRate() const
{
    QMutexLocker settingsLocker(&m_settingsMutex);
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2SoftInterp));
}

quint64 USRPOutput::getCenterFrequency() const
{
    QMutexLocker settingsLocker(&m_settingsMutex);
    return m_settings.m_centerFrequency;
}

void USRPOutput::setCenterFrequency(qint64 centerFrequency)
{
    QMutexLocker settingsLocker(&m_settingsMutex);
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
    if (!m_deviceShared.m_deviceParams)
    {
        minF = 0.0f;
        maxF = 0.0f;
        return;
    }

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
    if (!m_deviceShared.m_deviceParams)
    {
        minF = 0.0f;
        maxF = 0.0f;
        return;
    }

    try
    {
        minF = m_deviceShared.m_deviceParams->m_srRangeTx.start();
        maxF = m_deviceShared.m_deviceParams->m_srRangeTx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutput::getSRRange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

void USRPOutput::getLPRange(float& minF, float& maxF) const
{
    if (!m_deviceShared.m_deviceParams)
    {
        minF = 0.0f;
        maxF = 0.0f;
        return;
    }

    try
    {
        minF = m_deviceShared.m_deviceParams->m_lpfRangeTx.start();
        maxF = m_deviceShared.m_deviceParams->m_lpfRangeTx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutput::getLPRange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

void USRPOutput::getGainRange(float& minF, float& maxF) const
{
    if (!m_deviceShared.m_deviceParams)
    {
        minF = 0.0f;
        maxF = 0.0f;
        return;
    }

    try
    {
        minF = m_deviceShared.m_deviceParams->m_gainRangeTx.start();
        maxF = m_deviceShared.m_deviceParams->m_gainRangeTx.stop();
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutput::getGainRange: exception: " << e.what();
        minF = 0.0f;
        maxF = 0.0f;
    }
}

QStringList USRPOutput::getTxAntennas() const
{
    return m_deviceShared.m_deviceParams ? m_deviceShared.m_deviceParams->m_txAntennas : QStringList();
}

QStringList USRPOutput::getClockSources() const
{
    return m_deviceShared.m_deviceParams ? m_deviceShared.m_deviceParams->m_clockSources : QStringList();
}

bool USRPOutput::handleMessage(const Message& message)
{
    if (MsgConfigureUSRP::match(message))
    {
        MsgConfigureUSRP& conf = (MsgConfigureUSRP&) message;
        qDebug() << "USRPOutput::handleMessage: MsgConfigureUSRP";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce())) {
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
            qDebug() << "USRPOutput::handleMessage: MsgReportDeviceSettings: devSampleRate:" << devSampleRate << "masterClockRate:" << masterClockRate;

            if ((devSampleRate != m_settings.m_devSampleRate) || (masterClockRate != m_settings.m_masterClockRate))
            {
                if (devSampleRate != m_settings.m_devSampleRate) {
                    resizeSampleSourceFifo(devSampleRate, m_settings.m_log2SoftInterp);
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
        int devSampleRate = m_settings.m_devSampleRate;

        // Tx buddy changed settings. Only copy them if the buddy's channel shares our LO (E.g. B210),
        // not if it has an independent front-end (E.g. X310 with two daughterboards)
        if (!report.getRxElseTx() && DeviceUSRP::channelsShareLO(device, false, report.getChannel(), m_deviceShared.m_channel))
        {
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

        if (m_settings.m_devSampleRate != devSampleRate) {
            resizeSampleSourceFifo(m_settings.m_devSampleRate, m_settings.m_log2SoftInterp);
        }

        // A buddy changing the master clock rate may have changed our sample rate, so have the device
        // engine thread read back the actual rate, which will be reported via MsgReportDeviceSettings
        {
            QMutexLocker threadLocker(&m_threadMutex);

            if (m_usrpOutputThread) {
                m_usrpOutputThread->getInputMessageQueue()->push(DeviceUSRPShared::MsgReadDeviceSampleRate::create());
            }
        }

        notifySampleRateChange();

        return true;
    }
    else if (DeviceUSRPShared::MsgReportClockSourceChange::match(message))
    {
        DeviceUSRPShared::MsgReportClockSourceChange& report = (DeviceUSRPShared::MsgReportClockSourceChange&) message;
        QMutexLocker settingsLocker(&m_settingsMutex);

        m_settings.m_clockSource = report.getClockSource();

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
            bool temperatureValid = DeviceUSRP::getTemperature(deviceParams->getDevice(), false, m_deviceShared.m_channel, temperature);
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

            if (m_usrpOutputThread)
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

void USRPOutput::resizeSampleSourceFifo(int devSampleRate, unsigned int log2Interp)
{
#if defined(_MSC_VER)
    unsigned int fifoRate = (unsigned int) devSampleRate / (1<<log2Interp);
    fifoRate = fifoRate < 48000U ? 48000U : fifoRate;
#else
    unsigned int fifoRate = std::max(
        (unsigned int) devSampleRate / (1<<log2Interp),
        DeviceUSRPShared::m_sampleFifoMinRate);
#endif
    unsigned int size = SampleSourceFifo::getSizePolicy(fifoRate);
    QMutexLocker threadLocker(&m_threadMutex);

    if (m_usrpOutputThread)
    {
        // Samples are written to the FIFO in the device engine thread, so resize it there, with streaming paused
        m_usrpOutputThread->getInputMessageQueue()->push(DeviceUSRPShared::MsgResizeSampleFifo::create(size));
    }
    else
    {
        m_sampleSourceFifo.resize(size);
    }

    qDebug("USRPOutput::resizeSampleSourceFifo: rate %u size %u", fifoRate, size);
}

bool USRPOutput::applySettings(const USRPOutputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    QMutexLocker settingsLocker(&m_settingsMutex);
    qDebug() << "USRPOutput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    bool changeOwnDSP = false;
    bool changeTxDSP  = false;
    bool changeAllDSP = false;

    if (settingsKeys.contains("devSampleRate") || force) {
        changeAllDSP = true;
    }

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("loOffset")
        || settingsKeys.contains("transverterMode")
        || settingsKeys.contains("transverterDeltaFrequency")
        || force)
    {
        changeTxDSP = true;

        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
        m_deviceShared.m_centerFrequency = deviceCenterFrequency; // for buddies
    }

    if (settingsKeys.contains("devSampleRate")
       || settingsKeys.contains("log2SoftInterp") || force)
    {
        resizeSampleSourceFifo(settings.m_devSampleRate, settings.m_log2SoftInterp);
    }

    if (settingsKeys.contains("log2SoftInterp") || force)
    {
        changeOwnDSP = true;
        m_deviceShared.m_log2Soft = settings.m_log2SoftInterp; // for buddies
    }

    // Applying settings to the device can take a long time (E.g. sample rate, clock source and bandwidth),
    // so when running, they are applied in the device engine thread, rather than blocking the GUI thread.
    // Values actually set are reported back via DeviceUSRPShared::MsgReportDeviceSettings.
    // When not running, they are applied when the channel is acquired in start(), except for GPIO,
    // which doesn't require the channel to be acquired.
    bool threadRunning = false;
    {
        QMutexLocker threadLocker(&m_threadMutex);

        if (m_usrpOutputThread)
        {
            m_usrpOutputThread->getInputMessageQueue()->push(MsgConfigureUSRP::create(settings, settingsKeys, force));
            threadRunning = true;
        }
    }

    if (!threadRunning && m_deviceShared.m_deviceParams && (settingsKeys.contains("gpioDir") || settingsKeys.contains("gpioPins") || force))
    {
        // Don't block the GUI thread if a buddy is configuring the device (E.g. waiting for LO lock). Retry later instead.
        if (m_deviceShared.m_deviceParams->m_mutex.tryLock())
        {
            USRPOutputThread::applyDeviceSettings(m_deviceShared.m_deviceParams, m_deviceShared.m_channel, settings, settingsKeys, force, false, false, nullptr);
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

    // forward changes to buddies or oneself
    // If the device coerces the sample rate, this will be sent again with the actual rate, once reported

    // When not running, the device hasn't been changed, so buddies don't need to be informed.
    // They will be informed of the actual settings when we are started.
    if (!threadRunning)
    {
        if (changeAllDSP || changeTxDSP || changeOwnDSP) {
            forwardChangeOwnDSP();
        }
    }
    else if (changeAllDSP)
    {
        forwardChangeAllDSP();
    }
    else if (changeTxDSP)
    {
        forwardChangeTxDSP();
    }
    else if (changeOwnDSP)
    {
        forwardChangeOwnDSP();
    }

    return true;
}

// Forward sample rate and frequency to self, all buddies and GUI
void USRPOutput::forwardChangeAllDSP()
{
    qDebug("USRPOutput::forwardChangeAllDSP");

    // send to self first
    forwardChangeOwnDSP();

    // send to sink buddies
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, false, m_deviceShared.m_channel);
        (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
    }

    // send to source buddies
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, false, m_deviceShared.m_channel);
        (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
    }

    // send to GUI so it can see master clock rate and if actual rate differs
    if (m_deviceAPI->getSamplingDeviceGUIMessageQueue())
    {
        DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, false, m_deviceShared.m_channel);
        m_deviceAPI->getSamplingDeviceGUIMessageQueue()->push(report);
    }
}

// Forward sample rate and frequency to self and Tx buddies
void USRPOutput::forwardChangeTxDSP()
{
    qDebug("USRPOutput::forwardChangeTxDSP");

    // send to self first
    forwardChangeOwnDSP();

    // send to sink buddies
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceUSRPShared::MsgReportBuddyChange *report = DeviceUSRPShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset, m_settings.m_masterClockRate, false, m_deviceShared.m_channel);
        (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
    }
}

// Forward sample rate and frequency to self only
void USRPOutput::forwardChangeOwnDSP()
{
    int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftInterp);
    DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
}

// Forward sample rate and frequency to self and GUI, but not buddies (E.g. when changed by a buddy)
void USRPOutput::notifySampleRateChange()
{
    forwardChangeOwnDSP();

    if (getMessageQueueToGUI())
    {
        DeviceUSRPShared::MsgReportBuddyChange *reportToGUI = DeviceUSRPShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_centerFrequency, m_settings.m_loOffset,  m_settings.m_masterClockRate, false);
        getMessageQueueToGUI()->push(reportToGUI);
    }
}

// Forward clock source to GUI and all buddies
void USRPOutput::forwardClockSource()
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
void USRPOutput::forwardDeviceInfo(bool temperatureValid, float temperature)
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

int USRPOutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    QMutexLocker settingsLocker(&m_settingsMutex);
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
    QMutexLocker settingsLocker(&m_settingsMutex);
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
    if (deviceSettingsKeys.contains("title")) {
        settings.m_title = *response.getUsrpOutputSettings()->getTitle();
    }
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
        settings.m_log2SoftInterp = qBound(0, response.getUsrpOutputSettings()->getLog2SoftInterp(), 6);
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
    if (deviceSettingsKeys.contains("gpioDir")) {
        settings.m_gpioDir = response.getUsrpOutputSettings()->getGpioDir();
    }
    if (deviceSettingsKeys.contains("gpioPins")) {
        settings.m_gpioPins = response.getUsrpOutputSettings()->getGpioPins();
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
    if (response.getUsrpOutputSettings()->getTitle()) {
        *response.getUsrpOutputSettings()->getTitle() = settings.m_title;
    } else {
        response.getUsrpOutputSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getUsrpOutputSettings()->getAntennaPath()) {
        *response.getUsrpOutputSettings()->getAntennaPath() = settings.m_antennaPath;
    } else {
        response.getUsrpOutputSettings()->setAntennaPath(new QString(settings.m_antennaPath));
    }
    response.getUsrpOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getUsrpOutputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getUsrpOutputSettings()->setLoOffset(settings.m_loOffset);
    if (response.getUsrpOutputSettings()->getClockSource()) {
        *response.getUsrpOutputSettings()->getClockSource() = settings.m_clockSource;
    } else {
        response.getUsrpOutputSettings()->setClockSource(new QString(settings.m_clockSource));
    }
    response.getUsrpOutputSettings()->setGain(settings.m_gain);
    response.getUsrpOutputSettings()->setLog2SoftInterp(settings.m_log2SoftInterp);
    response.getUsrpOutputSettings()->setLpfBw(settings.m_lpfBW);
    response.getUsrpOutputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getUsrpOutputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getUsrpOutputSettings()->setGpioDir(settings.m_gpioDir);
    response.getUsrpOutputSettings()->setGpioPins(settings.m_gpioPins);
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

    QMutexLocker threadLocker(&m_threadMutex);

    if (m_usrpOutputThread)
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

    if (deviceSettingsKeys.contains("title") || force) {
        swgUsrpOutputSettings->setTitle(new QString(settings.m_title));
    }
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
    if (deviceSettingsKeys.contains("gpioDir") || force) {
        swgUsrpOutputSettings->setGpioDir(settings.m_gpioDir);
    }
    if (deviceSettingsKeys.contains("gpioPins") || force) {
        swgUsrpOutputSettings->setGpioPins(settings.m_gpioPins);
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
