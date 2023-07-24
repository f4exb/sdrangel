///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGSoapySDROutputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGSoapySDRReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"
#include "soapysdr/devicesoapysdr.h"

#include "soapysdroutputthread.h"
#include "soapysdroutput.h"

MESSAGE_CLASS_DEFINITION(SoapySDROutput::MsgConfigureSoapySDROutput, Message)
MESSAGE_CLASS_DEFINITION(SoapySDROutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(SoapySDROutput::MsgReportGainChange, Message)

SoapySDROutput::SoapySDROutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_deviceDescription("SoapySDROutput"),
    m_running(false),
    m_thread(nullptr)
{
    m_deviceAPI->setNbSinkStreams(1);
    m_openSuccess = openDevice();
    initGainSettings(m_settings);
    initTunableElementsSettings(m_settings);
    initStreamArgSettings(m_settings);
    initDeviceArgSettings(m_settings);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SoapySDROutput::networkManagerFinished
    );
}

SoapySDROutput::~SoapySDROutput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SoapySDROutput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void SoapySDROutput::destroy()
{
    delete this;
}

bool SoapySDROutput::openDevice()
{
    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_settings.m_devSampleRate));

    // look for Tx buddies and get reference to the device object
    if (m_deviceAPI->getSinkBuddies().size() > 0) // look sink sibling first
    {
        qDebug("SoapySDROutput::openDevice: look in Tx buddies");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sinkBuddy->getBuddySharedPtr();

        if (!deviceSoapySDRShared)
        {
            qCritical("SoapySDROutput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (!device)
        {
            qCritical("SoapySDROutput::openDevice: cannot get device pointer from Tx buddy");
            return false;
        }

        m_deviceShared.m_device = device;
        m_deviceShared.m_deviceParams = deviceSoapySDRShared->m_deviceParams;
    }
    // look for Rx buddies and get reference to the device object
    else if (m_deviceAPI->getSourceBuddies().size() > 0) // then source
    {
        qDebug("SoapySDROutput::openDevice: look in Rx buddies");

        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sourceBuddy->getBuddySharedPtr();

        if (!deviceSoapySDRShared)
        {
            qCritical("SoapySDROutput::openDevice: the source buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (!device)
        {
            qCritical("SoapySDROutput::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_device = device;
        m_deviceShared.m_deviceParams = deviceSoapySDRShared->m_deviceParams;
    }
    // There are no buddies then create the first BladeRF2 device
    else
    {
        qDebug("SoapySDROutput::openDevice: open device here");
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        m_deviceShared.m_device = deviceSoapySDR.openSoapySDR(m_deviceAPI->getSamplingDeviceSequence(), m_deviceAPI->getHardwareUserArguments());

        if (!m_deviceShared.m_device)
        {
            qCritical("SoapySDROutput::openDevice: cannot open SoapySDR device");
            return false;
        }

        m_deviceShared.m_deviceParams = new DeviceSoapySDRParams(m_deviceShared.m_device);
    }

    m_deviceShared.m_channel = m_deviceAPI->getDeviceItemIndex(); // publicly allocate channel
    m_deviceShared.m_sink = this;
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void SoapySDROutput::closeDevice()
{
    if (!m_deviceShared.m_device) { // was never open
        return;
    }

    if (m_running) {
        stop();
    }

    if (m_thread) { // stills own the thread => transfer to a buddy
        moveThreadToBuddy();
    }

    m_deviceShared.m_channel = -1; // publicly release channel
    m_deviceShared.m_sink = nullptr;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        deviceSoapySDR.closeSoapySdr(m_deviceShared.m_device);
        m_deviceShared.m_device = nullptr;
    }
}

void SoapySDROutput::getFrequencyRange(uint64_t& min, uint64_t& max)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);

    if (channelSettings && (channelSettings->m_frequencySettings.size() > 0))
    {
        DeviceSoapySDRParams::FrequencySetting freqSettings = channelSettings->m_frequencySettings[0];
        SoapySDR::RangeList rangeList = freqSettings.m_ranges;

        if (rangeList.size() > 0)
        {
            SoapySDR::Range range = rangeList[0];
            min = range.minimum();
            max = range.maximum();
        }
        else
        {
            min = 0;
            max = 0;
        }
    }
    else
    {
        min = 0;
        max = 0;
    }
}

void SoapySDROutput::getGlobalGainRange(int& min, int& max)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);

    if (channelSettings)
    {
        min = channelSettings->m_gainRange.minimum();
        max = channelSettings->m_gainRange.maximum();
    }
    else
    {
        min = 0;
        max = 0;
    }
}

bool SoapySDROutput::isAGCSupported()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_hasAGC;
}

const SoapySDR::RangeList& SoapySDROutput::getRateRanges()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_ratesRanges;
}

const std::vector<std::string>& SoapySDROutput::getAntennas()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_antennas;
}

const SoapySDR::RangeList& SoapySDROutput::getBandwidthRanges()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_bandwidthsRanges;
}

const std::vector<DeviceSoapySDRParams::FrequencySetting>& SoapySDROutput::getTunableElements()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_frequencySettings;
}

const std::vector<DeviceSoapySDRParams::GainSetting>& SoapySDROutput::getIndividualGainsRanges()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_gainSettings;
}

const SoapySDR::ArgInfoList& SoapySDROutput::getDeviceArgInfoList()
{
    return  m_deviceShared.m_deviceParams->getDeviceArgs();
}

void SoapySDROutput::initGainSettings(SoapySDROutputSettings& settings)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    settings.m_individualGains.clear();
    settings.m_globalGain = 0;

    for (const auto &it : channelSettings->m_gainSettings) {
        settings.m_individualGains[QString(it.m_name.c_str())] = 0.0;
    }

    updateGains(m_deviceShared.m_device, m_deviceShared.m_channel, settings);
}

void SoapySDROutput::initTunableElementsSettings(SoapySDROutputSettings& settings)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    settings.m_tunableElements.clear();
    bool first = true;

    for (const auto &it : channelSettings->m_frequencySettings)
    {
        if (first)
        {
            first = false;
            continue;
        }

        settings.m_tunableElements[QString(it.m_name.c_str())] = 0.0;
    }

    updateTunableElements(m_deviceShared.m_device, m_deviceShared.m_channel, settings);
}

const SoapySDR::ArgInfoList& SoapySDROutput::getStreamArgInfoList()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_streamSettingsArgs;
}

void SoapySDROutput::initStreamArgSettings(SoapySDROutputSettings& settings)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    settings.m_streamArgSettings.clear();

    for (const auto &it : channelSettings->m_streamSettingsArgs)
    {
        if (it.type == SoapySDR::ArgInfo::BOOL) {
            settings.m_streamArgSettings[QString(it.key.c_str())] = QVariant(it.value == "true");
        } else if (it.type == SoapySDR::ArgInfo::INT) {
            settings.m_streamArgSettings[QString(it.key.c_str())] = QVariant(atoi(it.value.c_str()));
        } else if (it.type == SoapySDR::ArgInfo::FLOAT) {
            settings.m_streamArgSettings[QString(it.key.c_str())] = QVariant(atof(it.value.c_str()));
        } else if (it.type == SoapySDR::ArgInfo::STRING) {
            settings.m_streamArgSettings[QString(it.key.c_str())] = QVariant(it.value.c_str());
        }
    }
}

void SoapySDROutput::initDeviceArgSettings(SoapySDROutputSettings& settings)
{
    settings.m_deviceArgSettings.clear();

    for (const auto &it : m_deviceShared.m_deviceParams->getDeviceArgs())
    {
        if (it.type == SoapySDR::ArgInfo::BOOL) {
            settings.m_deviceArgSettings[QString(it.key.c_str())] = QVariant(it.value == "true");
        } else if (it.type == SoapySDR::ArgInfo::INT) {
            settings.m_deviceArgSettings[QString(it.key.c_str())] = QVariant(atoi(it.value.c_str()));
        } else if (it.type == SoapySDR::ArgInfo::FLOAT) {
            settings.m_deviceArgSettings[QString(it.key.c_str())] = QVariant(atof(it.value.c_str()));
        } else if (it.type == SoapySDR::ArgInfo::STRING) {
            settings.m_deviceArgSettings[QString(it.key.c_str())] = QVariant(it.value.c_str());
        }
    }
}

bool SoapySDROutput::hasDCAutoCorrection()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_hasDCAutoCorrection;
}

bool SoapySDROutput::hasDCCorrectionValue()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_hasDCOffsetValue;
}

bool SoapySDROutput::hasIQCorrectionValue()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_hasIQBalanceValue;
}

void SoapySDROutput::init()
{
    applySettings(m_settings, true);
}

SoapySDROutputThread *SoapySDROutput::findThread()
{
    if (!m_thread) // this does not own the thread
    {
        SoapySDROutputThread *soapySDROutputThread = nullptr;

        // find a buddy that has allocated the thread
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

        for (; it != sinkBuddies.end(); ++it)
        {
            SoapySDROutput *buddySink = ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_sink;

            if (buddySink)
            {
                soapySDROutputThread = buddySink->getThread();

                if (soapySDROutputThread) {
                    break;
                }
            }
        }

        return soapySDROutputThread;
    }
    else
    {
        return m_thread; // own thread
    }
}

void SoapySDROutput::moveThreadToBuddy()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

    for (; it != sinkBuddies.end(); ++it)
    {
        SoapySDROutput *buddySink = ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_sink;

        if (buddySink)
        {
            buddySink->setThread(m_thread);
            m_thread = nullptr;  // zero for others
        }
    }
}

bool SoapySDROutput::start()
{
    // There is a single thread per physical device (Tx side). This thread is unique and referenced by a unique
    // buddy in the group of sink buddies associated with this physical device.
    //
    // This start method is responsible for managing the thread and channel enabling when the streaming of a Tx channel is started
    //
    // It checks the following conditions
    //   - the thread is allocated or not (by itself or one of its buddies). If it is it grabs the thread pointer.
    //   - the requested channel is the first (0) or the following
    //
    // There are two possible working modes:
    //   - Single Output (SO) with only one channel streaming. This HAS to be channel 0.
    //   - Multiple Output (MO) with two or more channels. It MUST be in this configuration if any channel other than 0
    //     is used. For example when we will run with only channel 2 streaming from the client perspective the channels 0 and 1
    //     will actually be enabled and streaming but zero samples will be sent to it.
    //
    // It manages the transition form SO where only one channel (the first or channel 0) should be running to the
    // Multiple Output (MO) if the requested channel is 1 or more. More generally it checks if the requested channel is within the current
    // channel range allocated in the thread or past it. To perform the transition it stops the thread, deletes it and creates a new one.
    // It marks the thread as needing start.
    //
    // If the requested channel is within the thread channel range (this thread being already allocated) it simply removes its FIFO reference
    // so that the samples are not taken from the FIFO anymore and leaves the thread unchanged (no stop, no delete/new)
    //
    // If there is no thread allocated it creates a new one with a number of channels that fits the requested channel. That is
    // 1 if channel 0 is requested (SO mode) and 3 if channel 2 is requested (MO mode). It marks the thread as needing start.
    //
    // Eventually it registers the FIFO in the thread. If the thread has to be started it enables the channels up to the number of channels
    // allocated in the thread and starts the thread.
    //
    // Note: this is quite similar to the BladeRF2 start handling. The main difference is that the channel allocation (enabling) process is
    // done in the thread object.

    if (!m_openSuccess)
    {
        qWarning("SoapySDROutput::start: cannot start device");
        return false;
    }

    if (!m_deviceShared.m_device)
    {
        qDebug("SoapySDROutput::start: no device object");
        return false;
    }

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    SoapySDROutputThread *soapySDROutputThread = findThread();
    bool needsStart = false;

    if (soapySDROutputThread) // if thread is already allocated
    {
        qDebug("SoapySDROutput::start: thread is already allocated");

        int nbOriginalChannels = soapySDROutputThread->getNbChannels();

        if (requestedChannel+1 > nbOriginalChannels) // expansion by deleting and re-creating the thread
        {
            qDebug("SoapySDROutput::start: expand channels. Re-allocate thread and take ownership");

            SampleSourceFifo **fifos = new SampleSourceFifo*[nbOriginalChannels];
            unsigned int *log2Interps = new unsigned int[nbOriginalChannels];

            for (int i = 0; i < nbOriginalChannels; i++) // save original FIFO references and data
            {
                fifos[i] = soapySDROutputThread->getFifo(i);
                log2Interps[i] = soapySDROutputThread->getLog2Interpolation(i);
            }

            soapySDROutputThread->stopWork();
            delete soapySDROutputThread;
            soapySDROutputThread = new SoapySDROutputThread(m_deviceShared.m_device, requestedChannel+1);
            m_thread = soapySDROutputThread; // take ownership

            for (int i = 0; i < nbOriginalChannels; i++) // restore original FIFO references
            {
                soapySDROutputThread->setFifo(i, fifos[i]);
                soapySDROutputThread->setLog2Interpolation(i, log2Interps[i]);
            }

            // remove old thread address from buddies (reset in all buddies).  The address being held only in the owning sink.
            const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
            std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

            for (; it != sinkBuddies.end(); ++it) {
                ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_sink->setThread(0);
            }

            delete[] log2Interps;
            delete[] fifos;

            needsStart = true;
        }
        else
        {
            qDebug("SoapySDROutput::start: keep buddy thread");
        }
    }
    else // first allocation
    {
        qDebug("SoapySDROutput::start: allocate thread and take ownership");
        soapySDROutputThread = new SoapySDROutputThread(m_deviceShared.m_device, requestedChannel+1);
        m_thread = soapySDROutputThread; // take ownership
        needsStart = true;
    }

    soapySDROutputThread->setFifo(requestedChannel, &m_sampleSourceFifo);
    soapySDROutputThread->setLog2Interpolation(requestedChannel, m_settings.m_log2Interp);

    if (needsStart)
    {
        qDebug("SoapySDROutput::start: (re)start buddy thread");
        soapySDROutputThread->setSampleRate(m_settings.m_devSampleRate);
        soapySDROutputThread->startWork();
    }

    qDebug("SoapySDROutput::start: started");
    m_running = true;

    return true;
}

void SoapySDROutput::stop()
{
    // This stop method is responsible for managing the thread and channel disabling when the streaming of
    // a Tx channel is stopped
    //
    // If the thread is currently managing only one channel (SO mode). The thread can be just stopped and deleted.
    // Then the channel is closed (disabled).
    //
    // If the thread is currently managing many channels (MO mode) and we are removing the last channel. The transition
    // from MO to SO or reduction of MO size is handled by stopping the thread, deleting it and creating a new one
    // with the maximum number of channels needed if (and only if) there is still a channel active.
    //
    // If the thread is currently managing many channels (MO mode) but the channel being stopped is not the last
    // channel then the FIFO reference is simply removed from the thread so that this FIFO will not be used anymore.
    // In this case the channel is not closed (this is managed in the thread object) so that other channels can continue with the
    // same configuration. The device continues streaming on this channel but the samples are set to all zeros.

    if (!m_running) {
        return;
    }

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    SoapySDROutputThread *soapySDROutputThread = findThread();

    if (!soapySDROutputThread) { // no thread allocated
        return;
    }

    int nbOriginalChannels = soapySDROutputThread->getNbChannels();

    if (nbOriginalChannels == 1) // SO mode => just stop and delete the thread
    {
        qDebug("SoapySDROutput::stop: SO mode. Just stop and delete the thread");
        soapySDROutputThread->stopWork();
        delete soapySDROutputThread;
        m_thread = nullptr;

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

        for (; it != sinkBuddies.end(); ++it) {
            ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_sink->setThread(0);
        }
    }
    else if (requestedChannel == nbOriginalChannels - 1) // remove last MO channel => reduce by deleting and re-creating the thread
    {
        qDebug("SoapySDROutput::stop: MO mode. Reduce by deleting and re-creating the thread");
        soapySDROutputThread->stopWork();
        SampleSourceFifo **fifos = new SampleSourceFifo*[nbOriginalChannels-1];
        unsigned int *log2Interps = new unsigned int[nbOriginalChannels-1];
        int highestActiveChannelIndex = -1;

        for (int i = 0; i < nbOriginalChannels-1; i++) // save original FIFO references
        {
            fifos[i] = soapySDROutputThread->getFifo(i);

            if ((soapySDROutputThread->getFifo(i)) && (i > highestActiveChannelIndex)) {
                highestActiveChannelIndex = i;
            }

            log2Interps[i] = soapySDROutputThread->getLog2Interpolation(i);
        }

        delete soapySDROutputThread;
        m_thread = nullptr;

        if (highestActiveChannelIndex >= 0)
        {
            soapySDROutputThread = new SoapySDROutputThread(m_deviceShared.m_device, highestActiveChannelIndex+1);
            m_thread = soapySDROutputThread; // take ownership

            for (int i = 0; i < nbOriginalChannels-1; i++)  // restore original FIFO references
            {
                soapySDROutputThread->setFifo(i, fifos[i]);
                soapySDROutputThread->setLog2Interpolation(i, log2Interps[i]);
            }
        }
        else
        {
            qDebug("SoapySDROutput::stop: do not re-create thread as there are no more FIFOs active");
        }

        // remove old thread address from buddies (reset in all buddies). The address being held only in the owning sink.
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

        for (; it != sinkBuddies.end(); ++it) {
            ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_sink->setThread(0);
        }

        if (highestActiveChannelIndex >= 0)
        {
            qDebug("SoapySDROutput::stop: restarting the thread");
            soapySDROutputThread->startWork();
        }

        delete[] log2Interps;
        delete[] fifos;
    }
    else // remove channel from existing thread
    {
        qDebug("SoapySDROutput::stop: MO mode. Not changing MO configuration. Just remove FIFO reference");
        soapySDROutputThread->setFifo(requestedChannel, nullptr); // remove FIFO
    }

    applySettings(m_settings, true); // re-apply forcibly to set sample rate with the new number of channels

    m_running = false;
}

QByteArray SoapySDROutput::serialize() const
{
    return m_settings.serialize();
}

bool SoapySDROutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureSoapySDROutput* message = MsgConfigureSoapySDROutput::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSoapySDROutput* messageToGUI = MsgConfigureSoapySDROutput::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& SoapySDROutput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SoapySDROutput::getSampleRate() const
{
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2Interp));
}

quint64 SoapySDROutput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void SoapySDROutput::setCenterFrequency(qint64 centerFrequency)
{
    SoapySDROutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureSoapySDROutput* message = MsgConfigureSoapySDROutput::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSoapySDROutput* messageToGUI = MsgConfigureSoapySDROutput::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool SoapySDROutput::setDeviceCenterFrequency(SoapySDR::Device *dev, int requestedChannel, quint64 freq_hz, int loPpmTenths)
{
    qint64 df = ((qint64)freq_hz * loPpmTenths) / 10000000LL;
    freq_hz += df;

    try
    {
        dev->setFrequency(SOAPY_SDR_TX,
                requestedChannel,
                m_deviceShared.m_deviceParams->getTxChannelMainTunableElementName(requestedChannel),
                freq_hz);
        qDebug("SoapySDROutput::setDeviceCenterFrequency: setFrequency(%llu)", freq_hz);
        return true;
    }
    catch (const std::exception &ex)
    {
        qCritical("SoapySDROutput::applySettings: could not set frequency: %llu: %s", freq_hz, ex.what());
        return false;
    }
}

void SoapySDROutput::updateGains(SoapySDR::Device *dev, int requestedChannel, SoapySDROutputSettings& settings)
{
    if (!dev) {
        return;
    }

    try
    {
        settings.m_globalGain = round(dev->getGain(SOAPY_SDR_TX, requestedChannel));

        for (const auto &name : settings.m_individualGains.keys()) {
            settings.m_individualGains[name] = dev->getGain(SOAPY_SDR_TX, requestedChannel, name.toStdString());
        }
    }
    catch (const std::exception &ex)
    {
        qCritical("SoapySDROutput::updateGains: caught exception: %s", ex.what());
    }
}

void SoapySDROutput::updateTunableElements(SoapySDR::Device *dev, int requestedChannel, SoapySDROutputSettings& settings)
{
    if (!dev) {
        return;
    }

    try
    {
        for (const auto &name : settings.m_tunableElements.keys()) {
            settings.m_tunableElements[name] = dev->getFrequency(SOAPY_SDR_TX, requestedChannel, name.toStdString());
        }
    }
    catch (const std::exception &ex)
    {
        qCritical("SoapySDROutput::updateTunableElements: caught exception: %s", ex.what());
    }
}

bool SoapySDROutput::handleMessage(const Message& message)
{
    if (MsgConfigureSoapySDROutput::match(message))
    {
        MsgConfigureSoapySDROutput& conf = (MsgConfigureSoapySDROutput&) message;
        qDebug() << "SoapySDROutput::handleMessage: MsgConfigureSoapySDROutput";

        if (!applySettings(conf.getSettings(), conf.getForce())) {
            qDebug("SoapySDROutput::handleMessage: MsgConfigureSoapySDROutput config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SoapySDROutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (DeviceSoapySDRShared::MsgReportBuddyChange::match(message))
    {
        int requestedChannel = m_deviceAPI->getDeviceItemIndex();
        //DeviceSoapySDRShared::MsgReportBuddyChange& report = (DeviceSoapySDRShared::MsgReportBuddyChange&) message;
        SoapySDROutputSettings settings = m_settings;
        //bool fromRxBuddy = report.getRxElseTx();

        double centerFrequency = m_deviceShared.m_device->getFrequency(
                SOAPY_SDR_TX,
                requestedChannel,
                m_deviceShared.m_deviceParams->getTxChannelMainTunableElementName(requestedChannel));

        settings.m_centerFrequency = round(centerFrequency/1000.0) * 1000;
        settings.m_devSampleRate = round(m_deviceShared.m_device->getSampleRate(SOAPY_SDR_TX, requestedChannel));
        settings.m_bandwidth = round(m_deviceShared.m_device->getBandwidth(SOAPY_SDR_TX, requestedChannel));

        //SoapySDROutputThread *outputThread = findThread();

        m_settings = settings;

        // propagate settings to GUI if any
        if (getMessageQueueToGUI())
        {
            MsgConfigureSoapySDROutput *reportToGUI = MsgConfigureSoapySDROutput::create(m_settings, false);
            getMessageQueueToGUI()->push(reportToGUI);
        }

        return true;
    }
    else if (DeviceSoapySDRShared::MsgReportDeviceArgsChange::match(message))
    {
        DeviceSoapySDRShared::MsgReportDeviceArgsChange& report = (DeviceSoapySDRShared::MsgReportDeviceArgsChange&) message;
        QMap<QString, QVariant> deviceArgSettings = report.getDeviceArgSettings();

        for (const auto &oname : m_settings.m_deviceArgSettings.keys())
        {
            auto nvalue = deviceArgSettings.find(oname);

            if (nvalue != deviceArgSettings.end() && (m_settings.m_deviceArgSettings[oname] != *nvalue))
            {
                m_settings.m_deviceArgSettings[oname] = *nvalue;
                qDebug("SoapySDROutput::handleMessage: MsgReportDeviceArgsChange: device argument %s set to %s",
                        oname.toStdString().c_str(), nvalue->toString().toStdString().c_str());
            }
        }

        // propagate settings to GUI if any
        if (getMessageQueueToGUI())
        {
            DeviceSoapySDRShared::MsgReportDeviceArgsChange *reportToGUI = DeviceSoapySDRShared::MsgReportDeviceArgsChange::create(
                    m_settings.m_deviceArgSettings);
            getMessageQueueToGUI()->push(reportToGUI);
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool SoapySDROutput::applySettings(const SoapySDROutputSettings& settings, bool force)
{
    bool forwardChangeOwnDSP = false;
    bool forwardChangeToBuddies  = false;
    bool globalGainChanged = false;
    bool individualGainsChanged = false;
    bool deviceArgsChanged = false;
    QList<QString> reverseAPIKeys;

    SoapySDR::Device *dev = m_deviceShared.m_device;
    SoapySDROutputThread *outputThread = findThread();
    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    qint64 xlatedDeviceCenterFrequency = settings.m_centerFrequency;
    xlatedDeviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
    xlatedDeviceCenterFrequency = xlatedDeviceCenterFrequency < 0 ? 0 : xlatedDeviceCenterFrequency;

    // resize FIFO
    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || (m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        SoapySDROutputThread *soapySDROutputThread = findThread();
        SampleSourceFifo *fifo = nullptr;

        if (soapySDROutputThread)
        {
            fifo = soapySDROutputThread->getFifo(requestedChannel);
            soapySDROutputThread->setFifo(requestedChannel, nullptr);
        }

        unsigned int fifoRate = std::max(
            (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2Interp),
            DeviceSoapySDRShared::m_sampleFifoMinRate);
        m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(fifoRate));

        if (fifo) {
            soapySDROutputThread->setFifo(requestedChannel, &m_sampleSourceFifo);
        }
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
    {
        reverseAPIKeys.append("devSampleRate");
        forwardChangeOwnDSP = true;
        forwardChangeToBuddies = true;

        if (dev)
        {
            try
            {
                dev->setSampleRate(SOAPY_SDR_TX, requestedChannel, settings.m_devSampleRate);
                qDebug() << "SoapySDROutput::applySettings: setSampleRate OK: " << settings.m_devSampleRate;

                if (outputThread)
                {
                    bool wasRunning = outputThread->isRunning();
                    outputThread->stopWork();
                    outputThread->setSampleRate(settings.m_devSampleRate);

                    if (wasRunning) {
                        outputThread->startWork();
                    }
                }
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDROutput::applySettings: could not set sample rate: %d: %s",
                        settings.m_devSampleRate, ex.what());
            }
        }
    }

    if ((m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        reverseAPIKeys.append("log2Interp");
        forwardChangeOwnDSP = true;

        if (outputThread)
        {
            outputThread->setLog2Interpolation(requestedChannel, settings.m_log2Interp);
            qDebug() << "SoapySDROutput::applySettings: set decimation to " << (1<<settings.m_log2Interp);
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force) {
        reverseAPIKeys.append("centerFrequency");
    }
    if ((m_settings.m_transverterMode != settings.m_transverterMode) || force) {
        reverseAPIKeys.append("transverterMode");
    }
    if ((m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency) || force) {
        reverseAPIKeys.append("transverterDeltaFrequency");
    }
    if ((m_settings.m_LOppmTenths != settings.m_LOppmTenths) || force) {
        reverseAPIKeys.append("LOppmTenths");
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_transverterMode != settings.m_transverterMode)
        || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency)
        || (m_settings.m_LOppmTenths != settings.m_LOppmTenths)
        || (m_settings.m_devSampleRate != settings.m_devSampleRate)
        || (m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        forwardChangeOwnDSP = true;
        forwardChangeToBuddies = true;

        if (dev) {
            setDeviceCenterFrequency(dev, requestedChannel, settings.m_centerFrequency, settings.m_LOppmTenths);
        }
    }

    if ((m_settings.m_antenna != settings.m_antenna) || force)
    {
        reverseAPIKeys.append("antenna");

        if (dev)
        {
            try
            {
                dev->setAntenna(SOAPY_SDR_TX, requestedChannel, settings.m_antenna.toStdString());
                qDebug("SoapySDROutput::applySettings: set antenna to %s", settings.m_antenna.toStdString().c_str());
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDROutput::applySettings: cannot set antenna to %s: %s",
                        settings.m_antenna.toStdString().c_str(), ex.what());
            }
        }
    }

    if ((m_settings.m_bandwidth != settings.m_bandwidth) || force)
    {
        reverseAPIKeys.append("bandwidth");
        forwardChangeToBuddies = true;

        if (dev)
        {
            try
            {
                dev->setBandwidth(SOAPY_SDR_TX, requestedChannel, settings.m_bandwidth);
                qDebug("SoapySDROutput::applySettings: bandwidth set to %u", settings.m_bandwidth);
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDROutput::applySettings: cannot set bandwidth to %u: %s",
                        settings.m_bandwidth, ex.what());
            }
        }
    }

    for (const auto &oname : m_settings.m_tunableElements.keys())
    {
        auto nvalue = settings.m_tunableElements.find(oname);

        if (nvalue != settings.m_tunableElements.end() && ((m_settings.m_tunableElements[oname] != *nvalue) || force))
        {
            if (dev)
            {
                try
                {
                    dev->setFrequency(SOAPY_SDR_TX, requestedChannel, oname.toStdString(), *nvalue);
                    qDebug("SoapySDROutput::applySettings: tunable element %s frequency set to %lf",
                            oname.toStdString().c_str(), *nvalue);
                }
                catch (const std::exception &ex)
                {
                    qCritical("SoapySDROutput::applySettings: cannot set tunable element %s to %lf: %s",
                            oname.toStdString().c_str(), *nvalue, ex.what());
                }
            }

            m_settings.m_tunableElements[oname] = *nvalue;
        }
    }

    if ((m_settings.m_globalGain != settings.m_globalGain) || force)
    {
        reverseAPIKeys.append("globalGain");

        if (dev)
        {
            try
            {
                dev->setGain(SOAPY_SDR_TX, requestedChannel, settings.m_globalGain);
                qDebug("SoapySDROutput::applySettings: set global gain to %d", settings.m_globalGain);
                globalGainChanged = true;
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDROutput::applySettings: cannot set global gain to %d: %s",
                        settings.m_globalGain, ex.what());
            }
        }
    }

    for (const auto &oname : m_settings.m_individualGains.keys())
    {
        auto nvalue = settings.m_individualGains.find(oname);

        if (nvalue != settings.m_individualGains.end() && ((m_settings.m_individualGains[oname] != *nvalue) || force))
        {
            if (dev)
            {
                try
                {
                    dev->setGain(SOAPY_SDR_TX, requestedChannel, oname.toStdString(), *nvalue);
                    qDebug("SoapySDROutput::applySettings: individual gain %s set to %lf",
                            oname.toStdString().c_str(), *nvalue);
                    individualGainsChanged = true;
                }
                catch (const std::exception &ex)
                {
                    qCritical("SoapySDROutput::applySettings: cannot set individual gain %s to %lf: %s",
                            oname.toStdString().c_str(), *nvalue, ex.what());
                }
            }

            m_settings.m_individualGains[oname] = *nvalue;
        }
    }

    if ((m_settings.m_autoGain != settings.m_autoGain) || force)
    {
        reverseAPIKeys.append("autoGain");

        if (dev)
        {
            try
            {
                dev->setGainMode(SOAPY_SDR_TX, requestedChannel, settings.m_autoGain);
                qDebug("SoapySDROutput::applySettings: %s AGC", settings.m_autoGain ? "set" : "unset");
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDROutput::applySettings: cannot %s AGC", settings.m_autoGain ? "set" : "unset");
            }
        }
    }

    if ((m_settings.m_autoDCCorrection != settings.m_autoDCCorrection) || force)
    {
        reverseAPIKeys.append("autoDCCorrection");

        if ((dev) && hasDCAutoCorrection())
        {
            try
            {
                dev->setDCOffsetMode(SOAPY_SDR_TX, requestedChannel, settings.m_autoDCCorrection);
                qDebug("SoapySDROutput::applySettings: %s DC auto correction", settings.m_autoDCCorrection ? "set" : "unset");
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDROutput::applySettings: cannot %s DC auto correction", settings.m_autoDCCorrection ? "set" : "unset");
            }
        }
    }

    if ((m_settings.m_dcCorrection != settings.m_dcCorrection) || force)
    {
        reverseAPIKeys.append("dcCorrection");

        if ((dev) && hasDCCorrectionValue())
        {
            try
            {
                dev->setDCOffset(SOAPY_SDR_TX, requestedChannel, settings.m_dcCorrection);
                qDebug("SoapySDROutput::applySettings: DC offset correction set to (%lf, %lf)", settings.m_dcCorrection.real(), settings.m_dcCorrection.imag());
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDROutput::applySettings: cannot set DC offset correction to (%lf, %lf)", settings.m_dcCorrection.real(), settings.m_dcCorrection.imag());
            }
        }
    }

    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        reverseAPIKeys.append("iqCorrection");

        if ((dev) && hasIQCorrectionValue())
        {
            try
            {
                dev->setIQBalance(SOAPY_SDR_TX, requestedChannel, settings.m_iqCorrection);
                qDebug("SoapySDROutput::applySettings: IQ balance correction set to (%lf, %lf)", settings.m_iqCorrection.real(), settings.m_iqCorrection.imag());
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDROutput::applySettings: cannot set IQ balance correction to (%lf, %lf)", settings.m_iqCorrection.real(), settings.m_iqCorrection.imag());
            }
        }
    }

    for (const auto &oname : m_settings.m_streamArgSettings.keys())
    {
        auto nvalue = settings.m_streamArgSettings.find(oname);

        if (nvalue != settings.m_streamArgSettings.end() && ((m_settings.m_streamArgSettings[oname] != *nvalue) || force))
        {
            if (dev)
            {
                try
                {
                    dev->writeSetting(SOAPY_SDR_TX, requestedChannel, oname.toStdString(), nvalue->toString().toStdString());
                    qDebug("SoapySDROutput::applySettings: stream argument %s set to %s",
                            oname.toStdString().c_str(), nvalue->toString().toStdString().c_str());
                }
                catch (const std::exception &ex)
                {
                    qCritical("SoapySDROutput::applySettings: cannot set stream argument %s to %s: %s",
                            oname.toStdString().c_str(), nvalue->toString().toStdString().c_str(), ex.what());
                }
            }

            m_settings.m_streamArgSettings[oname] = *nvalue;
        }
    }

    for (const auto &oname : m_settings.m_deviceArgSettings.keys())
    {
        auto nvalue = settings.m_deviceArgSettings.find(oname);

        if (nvalue != settings.m_deviceArgSettings.end() && ((m_settings.m_deviceArgSettings[oname] != *nvalue) || force))
        {
            if (dev)
            {
                try
                {
                    dev->writeSetting(oname.toStdString(), nvalue->toString().toStdString());
                    qDebug("SoapySDROutput::applySettings: device argument %s set to %s",
                            oname.toStdString().c_str(), nvalue->toString().toStdString().c_str());
                }
                catch (const std::exception &ex)
                {
                    qCritical("SoapySDRInput::applySettings: cannot set device argument %s to %s: %s",
                            oname.toStdString().c_str(), nvalue->toString().toStdString().c_str(), ex.what());
                }
            }

            m_settings.m_deviceArgSettings[oname] = *nvalue;
            deviceArgsChanged = true;
        }
    }

    if (forwardChangeOwnDSP)
    {
        int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Interp);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (forwardChangeToBuddies)
    {
        // send to buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();

        for (const auto &itSource : sourceBuddies)
        {
            DeviceSoapySDRShared::MsgReportBuddyChange *report = DeviceSoapySDRShared::MsgReportBuddyChange::create(
                    settings.m_centerFrequency,
                    settings.m_LOppmTenths,
                    2,
                    settings.m_devSampleRate,
                    false);
            itSource->getSamplingDeviceInputMessageQueue()->push(report);
        }

        for (const auto &itSink : sinkBuddies)
        {
            DeviceSoapySDRShared::MsgReportBuddyChange *report = DeviceSoapySDRShared::MsgReportBuddyChange::create(
                    settings.m_centerFrequency,
                    settings.m_LOppmTenths,
                    2,
                    settings.m_devSampleRate,
                    false);
            itSink->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }

    if (deviceArgsChanged)
    {
        // send to buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();

        for (const auto &itSource : sourceBuddies)
        {
            DeviceSoapySDRShared::MsgReportDeviceArgsChange *report = DeviceSoapySDRShared::MsgReportDeviceArgsChange::create(
                    settings.m_deviceArgSettings);
            itSource->getSamplingDeviceInputMessageQueue()->push(report);
        }

        for (const auto &itSink : sinkBuddies)
        {
            DeviceSoapySDRShared::MsgReportDeviceArgsChange *report = DeviceSoapySDRShared::MsgReportDeviceArgsChange::create(
                    settings.m_deviceArgSettings);
            itSink->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);

        if (fullUpdate || force) {
            webapiReverseSendSettings(reverseAPIKeys, settings, true);
        } else if (reverseAPIKeys.size() != 0) {
            webapiReverseSendSettings(reverseAPIKeys, settings, false);
        }
    }

    m_settings = settings;

    if (globalGainChanged || individualGainsChanged)
    {
        if (dev) {
            updateGains(dev, requestedChannel, m_settings);
        }

        if (getMessageQueueToGUI())
        {
            MsgReportGainChange *report = MsgReportGainChange::create(m_settings, globalGainChanged, individualGainsChanged);
            getMessageQueueToGUI()->push(report);
        }
    }

    qDebug() << "SoapySDROutput::applySettings: "
            << " m_transverterMode: " << m_settings.m_transverterMode
            << " m_transverterDeltaFrequency: " << m_settings.m_transverterDeltaFrequency
            << " m_centerFrequency: " << m_settings.m_centerFrequency << " Hz"
            << " m_LOppmTenths: " << m_settings.m_LOppmTenths
            << " m_log2Interp: " << m_settings.m_log2Interp
            << " m_devSampleRate: " << m_settings.m_devSampleRate
            << " m_bandwidth: " << m_settings.m_bandwidth
            << " m_globalGain: " << m_settings.m_globalGain
            << " force: " << force;

    QMap<QString, double>::const_iterator doubleIt = m_settings.m_individualGains.begin();

    for(; doubleIt != m_settings.m_individualGains.end(); ++doubleIt) {
        qDebug("SoapySDROutput::applySettings: m_individualGains[%s]: %lf", doubleIt.key().toStdString().c_str(), doubleIt.value());
    }

    doubleIt = m_settings.m_tunableElements.begin();

    for(; doubleIt != m_settings.m_tunableElements.end(); ++doubleIt) {
        qDebug("SoapySDROutput::applySettings: m_tunableElements[%s]: %lf", doubleIt.key().toStdString().c_str(), doubleIt.value());
    }

    QMap<QString, QVariant>::const_iterator varIt = m_settings.m_deviceArgSettings.begin();

    for(; varIt != m_settings.m_deviceArgSettings.end(); ++varIt)
    {
        qDebug("SoapySDROutput::applySettings: m_deviceArgSettings[%s] (type %d): %s",
            varIt.key().toStdString().c_str(),
            (int) varIt.value().type(), // bool: 1, int: 2, double: 6, string: 10 (http://doc.qt.io/archives/qt-4.8/qvariant.html)
            varIt.value().toString().toStdString().c_str());
    }

    varIt = m_settings.m_streamArgSettings.begin();

    for(; varIt != m_settings.m_streamArgSettings.end(); ++varIt)
    {
        qDebug("SoapySDROutput::applySettings: m_streamArgSettings[%s] (type %d): %s",
            varIt.key().toStdString().c_str(),
            (int) varIt.value().type(),
            varIt.value().toString().toStdString().c_str());
    }

    return true;
}

int SoapySDROutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setSoapySdrOutputSettings(new SWGSDRangel::SWGSoapySDROutputSettings());
    response.getSoapySdrOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int SoapySDROutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    SoapySDROutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureSoapySDROutput *msg = MsgConfigureSoapySDROutput::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSoapySDROutput *msgToGUI = MsgConfigureSoapySDROutput::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void SoapySDROutput::webapiUpdateDeviceSettings(
        SoapySDROutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getSoapySdrOutputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getSoapySdrOutputSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getSoapySdrOutputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getSoapySdrOutputSettings()->getBandwidth();
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getSoapySdrOutputSettings()->getLog2Interp();
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getSoapySdrOutputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getSoapySdrOutputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("antenna")) {
        settings.m_antenna = *response.getSoapySdrOutputSettings()->getAntenna();
    }

    if (deviceSettingsKeys.contains("tunableElements"))
    {
        QList<SWGSDRangel::SWGArgValue*> *tunableElements = response.getSoapySdrOutputSettings()->getTunableElements();

        for (const auto &itArg : *tunableElements)
        {
            QMap<QString, double>::iterator itSettings = settings.m_tunableElements.find(*(itArg->getKey()));

            if (itSettings != settings.m_tunableElements.end())
            {
                QVariant v = webapiVariantFromArgValue(itArg);
                itSettings.value() = v.toDouble();
            }
        }
    }

    if (deviceSettingsKeys.contains("globalGain")) {
        settings.m_globalGain = response.getSoapySdrOutputSettings()->getGlobalGain();
    }

    if (deviceSettingsKeys.contains("individualGains"))
    {
        QList<SWGSDRangel::SWGArgValue*> *individualGains = response.getSoapySdrOutputSettings()->getIndividualGains();

        for (const auto &itArg : *individualGains)
        {
            QMap<QString, double>::iterator itSettings = settings.m_individualGains.find(*(itArg->getKey()));

            if (itSettings != settings.m_individualGains.end())
            {
                QVariant v = webapiVariantFromArgValue(itArg);
                itSettings.value() = v.toDouble();
            }
        }
    }

    if (deviceSettingsKeys.contains("autoGain")) {
        settings.m_autoGain = response.getSoapySdrOutputSettings()->getAutoGain() != 0;
    }
    if (deviceSettingsKeys.contains("autoDCCorrection")) {
        settings.m_autoDCCorrection = response.getSoapySdrOutputSettings()->getAutoDcCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("autoIQCorrection")) {
        settings.m_autoIQCorrection = response.getSoapySdrOutputSettings()->getAutoIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("dcCorrection"))
    {
        settings.m_dcCorrection.real(response.getSoapySdrOutputSettings()->getDcCorrection()->getReal());
        settings.m_dcCorrection.imag(response.getSoapySdrOutputSettings()->getDcCorrection()->getImag());
    }
    if (deviceSettingsKeys.contains("iqCorrection"))
    {
        settings.m_iqCorrection.real(response.getSoapySdrOutputSettings()->getIqCorrection()->getReal());
        settings.m_iqCorrection.imag(response.getSoapySdrOutputSettings()->getIqCorrection()->getImag());
    }

    if (deviceSettingsKeys.contains("streamArgSettings"))
    {
        QList<SWGSDRangel::SWGArgValue*> *streamArgSettings = response.getSoapySdrOutputSettings()->getStreamArgSettings();

        for (const auto itArg : *streamArgSettings)
        {
            QMap<QString, QVariant>::iterator itSettings = settings.m_streamArgSettings.find(*itArg->getKey());

            if (itSettings != settings.m_streamArgSettings.end()) {
                itSettings.value() = webapiVariantFromArgValue(itArg);
            }
        }
    }

    if (deviceSettingsKeys.contains("deviceArgSettings"))
    {
        QList<SWGSDRangel::SWGArgValue*> *deviceArgSettings = response.getSoapySdrOutputSettings()->getDeviceArgSettings();

        for (const auto itArg : *deviceArgSettings)
        {
            QMap<QString, QVariant>::iterator itSettings = settings.m_deviceArgSettings.find(*itArg->getKey());

            if (itSettings != settings.m_deviceArgSettings.end()) {
                itSettings.value() = webapiVariantFromArgValue(itArg);
            }
        }
    }

    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSoapySdrOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSoapySdrOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSoapySdrOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getSoapySdrOutputSettings()->getReverseApiDeviceIndex();
    }
}

int SoapySDROutput::webapiReportGet(SWGSDRangel::SWGDeviceReport& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setSoapySdrOutputReport(new SWGSDRangel::SWGSoapySDRReport());
    response.getSoapySdrOutputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int SoapySDROutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int SoapySDROutput::webapiRun(
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

void SoapySDROutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SoapySDROutputSettings& settings)
{
    response.getSoapySdrOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getSoapySdrOutputSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getSoapySdrOutputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getSoapySdrOutputSettings()->setLog2Interp(settings.m_log2Interp);
    response.getSoapySdrOutputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getSoapySdrOutputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    if (response.getSoapySdrOutputSettings()->getAntenna()) {
        *response.getSoapySdrOutputSettings()->getAntenna() = settings.m_antenna;
    } else {
        response.getSoapySdrOutputSettings()->setAntenna(new QString(settings.m_antenna));
    }

    if (response.getSoapySdrOutputSettings()->getTunableElements()) {
        response.getSoapySdrOutputSettings()->getTunableElements()->clear();
    } else {
        response.getSoapySdrOutputSettings()->setTunableElements(new QList<SWGSDRangel::SWGArgValue*>);
    }

    for (const auto& itName : settings.m_tunableElements.keys())
    {
        response.getSoapySdrOutputSettings()->getTunableElements()->append(new SWGSDRangel::SWGArgValue);
        response.getSoapySdrOutputSettings()->getTunableElements()->back()->setKey(new QString(  itName));
        double value = settings.m_tunableElements.value(itName);
        response.getSoapySdrOutputSettings()->getTunableElements()->back()->setValueString(new QString(tr("%1").arg(value)));
        response.getSoapySdrOutputSettings()->getTunableElements()->back()->setValueType(new QString("float"));
    }

    response.getSoapySdrOutputSettings()->setBandwidth(settings.m_bandwidth);
    response.getSoapySdrOutputSettings()->setGlobalGain(settings.m_globalGain);

    if (response.getSoapySdrOutputSettings()->getIndividualGains()) {
        response.getSoapySdrOutputSettings()->getIndividualGains()->clear();
    } else {
        response.getSoapySdrOutputSettings()->setIndividualGains(new QList<SWGSDRangel::SWGArgValue*>);
    }

    for (const auto& itName : settings.m_individualGains.keys())
    {
        response.getSoapySdrOutputSettings()->getIndividualGains()->append(new SWGSDRangel::SWGArgValue);
        response.getSoapySdrOutputSettings()->getIndividualGains()->back()->setKey(new QString(itName));
        double value = settings.m_individualGains.value(itName);
        response.getSoapySdrOutputSettings()->getIndividualGains()->back()->setValueString(new QString(tr("%1").arg(value)));
        response.getSoapySdrOutputSettings()->getIndividualGains()->back()->setValueType(new QString("float"));
    }

    response.getSoapySdrOutputSettings()->setAutoGain(settings.m_autoGain ? 1 : 0);
    response.getSoapySdrOutputSettings()->setAutoDcCorrection(settings.m_autoDCCorrection ? 1 : 0);
    response.getSoapySdrOutputSettings()->setAutoIqCorrection(settings.m_autoIQCorrection ? 1 : 0);

    if (!response.getSoapySdrOutputSettings()->getDcCorrection()) {
        response.getSoapySdrOutputSettings()->setDcCorrection(new SWGSDRangel::SWGComplex());
    }

    response.getSoapySdrOutputSettings()->getDcCorrection()->setReal(settings.m_dcCorrection.real());
    response.getSoapySdrOutputSettings()->getDcCorrection()->setImag(settings.m_dcCorrection.imag());

    if (!response.getSoapySdrOutputSettings()->getIqCorrection()) {
        response.getSoapySdrOutputSettings()->setIqCorrection(new SWGSDRangel::SWGComplex());
    }

    response.getSoapySdrOutputSettings()->getIqCorrection()->setReal(settings.m_iqCorrection.real());
    response.getSoapySdrOutputSettings()->getIqCorrection()->setImag(settings.m_iqCorrection.imag());

    if (response.getSoapySdrOutputSettings()->getStreamArgSettings()) {
        response.getSoapySdrOutputSettings()->getStreamArgSettings()->clear();
    } else {
        response.getSoapySdrOutputSettings()->setStreamArgSettings(new QList<SWGSDRangel::SWGArgValue*>);
    }

    for (const auto& itName : settings.m_streamArgSettings.keys())
    {
        response.getSoapySdrOutputSettings()->getStreamArgSettings()->append(new SWGSDRangel::SWGArgValue);
        response.getSoapySdrOutputSettings()->getStreamArgSettings()->back()->setKey(new QString(itName));
        const QVariant& v = settings.m_streamArgSettings.value(itName);
        webapiFormatArgValue(v, response.getSoapySdrOutputSettings()->getStreamArgSettings()->back());
    }

    if (response.getSoapySdrOutputSettings()->getDeviceArgSettings()) {
        response.getSoapySdrOutputSettings()->getDeviceArgSettings()->clear();
    } else {
        response.getSoapySdrOutputSettings()->setDeviceArgSettings(new QList<SWGSDRangel::SWGArgValue*>);
    }

    for (const auto& itName : settings.m_deviceArgSettings.keys())
    {
        response.getSoapySdrOutputSettings()->getDeviceArgSettings()->append(new SWGSDRangel::SWGArgValue);
        response.getSoapySdrOutputSettings()->getDeviceArgSettings()->back()->setKey(new QString(itName));
        const QVariant& v = settings.m_deviceArgSettings.value(itName);
        webapiFormatArgValue(v, response.getSoapySdrOutputSettings()->getDeviceArgSettings()->back());
    }

    response.getSoapySdrOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSoapySdrOutputSettings()->getReverseApiAddress()) {
        *response.getSoapySdrOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSoapySdrOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSoapySdrOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSoapySdrOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void SoapySDROutput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);

    response.getSoapySdrOutputReport()->setDeviceSettingsArgs(new QList<SWGSDRangel::SWGArgInfo*>);

    for (const auto& itArg : m_deviceShared.m_deviceParams->getDeviceArgs())
    {
        response.getSoapySdrOutputReport()->getDeviceSettingsArgs()->append(new SWGSDRangel::SWGArgInfo);
        webapiFormatArgInfo(itArg, response.getSoapySdrOutputReport()->getDeviceSettingsArgs()->back());
    }

    response.getSoapySdrOutputReport()->setStreamSettingsArgs(new QList<SWGSDRangel::SWGArgInfo*>);

    for (const auto& itArg : channelSettings->m_streamSettingsArgs)
    {
        response.getSoapySdrOutputReport()->getStreamSettingsArgs()->append(new SWGSDRangel::SWGArgInfo);
        webapiFormatArgInfo(itArg, response.getSoapySdrOutputReport()->getStreamSettingsArgs()->back());
    }

    response.getSoapySdrOutputReport()->setFrequencySettingsArgs(new QList<SWGSDRangel::SWGArgInfo*>);

    for (const auto& itArg : channelSettings->m_frequencySettingsArgs)
    {
        response.getSoapySdrOutputReport()->getFrequencySettingsArgs()->append(new SWGSDRangel::SWGArgInfo);
        webapiFormatArgInfo(itArg, response.getSoapySdrOutputReport()->getFrequencySettingsArgs()->back());
    }

    response.getSoapySdrOutputReport()->setHasAgc(channelSettings->m_hasAGC ? 1 : 0);
    response.getSoapySdrOutputReport()->setHasDcAutoCorrection(channelSettings->m_hasDCAutoCorrection ? 1 : 0);
    response.getSoapySdrOutputReport()->setHasDcOffsetValue(channelSettings->m_hasDCOffsetValue ? 1 : 0);
    response.getSoapySdrOutputReport()->setHasFrequencyCorrectionValue(channelSettings->m_hasFrequencyCorrectionValue ? 1 : 0);
    response.getSoapySdrOutputReport()->setHasIqBalanceValue(channelSettings->m_hasIQBalanceValue ? 1 : 0);

    if (channelSettings->m_antennas.size() != 0)
    {
        response.getSoapySdrOutputReport()->setAntennas(new QList<QString *>);

        for (const auto& itAntenna : channelSettings->m_antennas) {
            response.getSoapySdrOutputReport()->getAntennas()->append(new QString(itAntenna.c_str()));
        }
    }

    if ((channelSettings->m_gainRange.maximum() != 0.0) || (channelSettings->m_gainRange.minimum() != 0.0))
    {
        response.getSoapySdrOutputReport()->setGainRange(new SWGSDRangel::SWGRangeFloat());
        response.getSoapySdrOutputReport()->getGainRange()->setMin(channelSettings->m_gainRange.minimum());
        response.getSoapySdrOutputReport()->getGainRange()->setMax(channelSettings->m_gainRange.maximum());
    }

    if (channelSettings->m_gainSettings.size() != 0)
    {
        response.getSoapySdrOutputReport()->setGainSettings(new QList<SWGSDRangel::SWGSoapySDRGainSetting*>);

        for (const auto& itGain : channelSettings->m_gainSettings)
        {
            response.getSoapySdrOutputReport()->getGainSettings()->append(new SWGSDRangel::SWGSoapySDRGainSetting());
            response.getSoapySdrOutputReport()->getGainSettings()->back()->setRange(new SWGSDRangel::SWGRangeFloat());
            response.getSoapySdrOutputReport()->getGainSettings()->back()->getRange()->setMin(itGain.m_range.minimum());
            response.getSoapySdrOutputReport()->getGainSettings()->back()->getRange()->setMax(itGain.m_range.maximum());
            response.getSoapySdrOutputReport()->getGainSettings()->back()->setName(new QString(itGain.m_name.c_str()));
        }
    }

    if (channelSettings->m_frequencySettings.size() != 0)
    {
        response.getSoapySdrOutputReport()->setFrequencySettings(new QList<SWGSDRangel::SWGSoapySDRFrequencySetting*>);

        for (const auto& itFreq : channelSettings->m_frequencySettings)
        {
            response.getSoapySdrOutputReport()->getFrequencySettings()->append(new SWGSDRangel::SWGSoapySDRFrequencySetting());
            response.getSoapySdrOutputReport()->getFrequencySettings()->back()->setRanges(new QList<SWGSDRangel::SWGRangeFloat*>);

            for (const auto itRange : itFreq.m_ranges)
            {
                response.getSoapySdrOutputReport()->getFrequencySettings()->back()->getRanges()->append(new SWGSDRangel::SWGRangeFloat());
                response.getSoapySdrOutputReport()->getFrequencySettings()->back()->getRanges()->back()->setMin(itRange.minimum());
                response.getSoapySdrOutputReport()->getFrequencySettings()->back()->getRanges()->back()->setMax(itRange.maximum());
            }

            response.getSoapySdrOutputReport()->getFrequencySettings()->back()->setName(new QString(itFreq.m_name.c_str()));
        }
    }

    if (channelSettings->m_ratesRanges.size() != 0)
    {
        response.getSoapySdrOutputReport()->setRatesRanges(new QList<SWGSDRangel::SWGRangeFloat*>);

        for (const auto itRange : channelSettings->m_ratesRanges)
        {
            response.getSoapySdrOutputReport()->getRatesRanges()->append(new SWGSDRangel::SWGRangeFloat());
            response.getSoapySdrOutputReport()->getRatesRanges()->back()->setMin(itRange.minimum());
            response.getSoapySdrOutputReport()->getRatesRanges()->back()->setMax(itRange.maximum());
        }
    }

    if (channelSettings->m_bandwidthsRanges.size() != 0)
    {
        response.getSoapySdrOutputReport()->setBandwidthsRanges(new QList<SWGSDRangel::SWGRangeFloat*>);

        for (const auto itBandwidth : channelSettings->m_bandwidthsRanges)
        {
            response.getSoapySdrOutputReport()->getBandwidthsRanges()->append(new SWGSDRangel::SWGRangeFloat());
            response.getSoapySdrOutputReport()->getBandwidthsRanges()->back()->setMin(itBandwidth.minimum());
            response.getSoapySdrOutputReport()->getBandwidthsRanges()->back()->setMax(itBandwidth.maximum());
        }
    }
}

QVariant SoapySDROutput::webapiVariantFromArgValue(SWGSDRangel::SWGArgValue *argValue)
{
    if (*argValue->getValueType() == "bool") {
        return QVariant((bool) (*argValue->getValueString() == "1"));
    } else if (*argValue->getValueType() == "int") {
        return QVariant((int) (atoi(argValue->getValueString()->toStdString().c_str())));
    } else if (*argValue->getValueType() == "float") {
        return QVariant((double) (atof(argValue->getValueString()->toStdString().c_str())));
    } else {
        return QVariant(QString(*argValue->getValueString()));
    }
}

void SoapySDROutput::webapiFormatArgValue(const QVariant& v, SWGSDRangel::SWGArgValue *argValue)
{
    if (v.type() == QVariant::Bool)
    {
        argValue->setValueType(new QString("bool"));
        argValue->setValueString(new QString(v.toBool() ? "1" : "0"));
    }
    else if (v.type() == QVariant::Int)
    {
        argValue->setValueType(new QString("int"));
        argValue->setValueString(new QString(tr("%1").arg(v.toInt())));
    }
    else if (v.type() == QVariant::Double)
    {
        argValue->setValueType(new QString("float"));
        argValue->setValueString(new QString(tr("%1").arg(v.toDouble())));
    }
    else
    {
        argValue->setValueType(new QString("string"));
        argValue->setValueString(new QString(v.toString()));
    }
}

void SoapySDROutput::webapiFormatArgInfo(const SoapySDR::ArgInfo& arg, SWGSDRangel::SWGArgInfo *argInfo)
{
    argInfo->setKey(new QString(arg.key.c_str()));

    if (arg.type == SoapySDR::ArgInfo::BOOL) {
        argInfo->setValueType(new QString("bool"));
    } else if (arg.type == SoapySDR::ArgInfo::INT) {
        argInfo->setValueType(new QString("int"));
    } else if (arg.type == SoapySDR::ArgInfo::FLOAT) {
        argInfo->setValueType(new QString("float"));
    } else {
        argInfo->setValueType(new QString("string"));
    }

    argInfo->setValueString(new QString(arg.value.c_str()));
    argInfo->setName(new QString(arg.name.c_str()));
    argInfo->setDescription(new QString(arg.description.c_str()));
    argInfo->setUnits(new QString(arg.units.c_str()));

    if ((arg.range.minimum() != 0.0) || (arg.range.maximum() != 0.0))
    {
        argInfo->setRange(new SWGSDRangel::SWGRangeFloat());
        argInfo->getRange()->setMin(arg.range.minimum());
        argInfo->getRange()->setMax(arg.range.maximum());
    }

    argInfo->setValueOptions(new QList<QString*>);

    for (const auto& itOpt : arg.options) {
        argInfo->getValueOptions()->append(new QString(itOpt.c_str()));
    }

    argInfo->setOptionNames(new QList<QString*>);

    for (const auto& itOpt : arg.optionNames) {
        argInfo->getOptionNames()->append(new QString(itOpt.c_str()));
    }
}

void SoapySDROutput::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const SoapySDROutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // Single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SoapySDR"));
    swgDeviceSettings->setSoapySdrOutputSettings(new SWGSDRangel::SWGSoapySDROutputSettings());
    swgDeviceSettings->getSoapySdrOutputSettings()->init();
    SWGSDRangel::SWGSoapySDROutputSettings *swgSoapySDROutputSettings = swgDeviceSettings->getSoapySdrOutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgSoapySDROutputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgSoapySDROutputSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgSoapySDROutputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("bandwidth") || force) {
        swgSoapySDROutputSettings->setBandwidth(settings.m_bandwidth);
    }
    if (deviceSettingsKeys.contains("log2Interp") || force) {
        swgSoapySDROutputSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgSoapySDROutputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgSoapySDROutputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("antenna") || force) {
        swgSoapySDROutputSettings->setAntenna(new QString(settings.m_antenna));
    }
    if (deviceSettingsKeys.contains("globalGain") || force) {
        swgSoapySDROutputSettings->setGlobalGain(settings.m_globalGain);
    }
    if (deviceSettingsKeys.contains("autoGain") || force) {
        swgSoapySDROutputSettings->setAutoGain(settings.m_autoGain ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("autoDCCorrection") || force) {
        swgSoapySDROutputSettings->setAutoDcCorrection(settings.m_autoDCCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("autoIQCorrection") || force) {
        swgSoapySDROutputSettings->setAutoIqCorrection(settings.m_autoIQCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dcCorrection") || force)
    {
        swgSoapySDROutputSettings->setDcCorrection(new SWGSDRangel::SWGComplex());
        swgSoapySDROutputSettings->getDcCorrection()->setReal(settings.m_dcCorrection.real());
        swgSoapySDROutputSettings->getDcCorrection()->setImag(settings.m_dcCorrection.imag());
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force)
    {
        swgSoapySDROutputSettings->setIqCorrection(new SWGSDRangel::SWGComplex());
        swgSoapySDROutputSettings->getIqCorrection()->setReal(settings.m_iqCorrection.real());
        swgSoapySDROutputSettings->getIqCorrection()->setImag(settings.m_iqCorrection.imag());
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

void SoapySDROutput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // Single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SoapySDR"));

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

void SoapySDROutput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SoapySDROutput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SoapySDROutput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
