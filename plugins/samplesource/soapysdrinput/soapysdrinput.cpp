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

#include "util/simpleserializer.h"

#include "SWGDeviceSettings.h"
#include "SWGSoapySDRInputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGSoapySDRReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "soapysdr/devicesoapysdr.h"

#include "soapysdrinputthread.h"
#include "soapysdrinput.h"

MESSAGE_CLASS_DEFINITION(SoapySDRInput::MsgConfigureSoapySDRInput, Message)
MESSAGE_CLASS_DEFINITION(SoapySDRInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(SoapySDRInput::MsgReportGainChange, Message)

SoapySDRInput::SoapySDRInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_deviceDescription("SoapySDRInput"),
    m_running(false),
    m_thread(nullptr)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_openSuccess = openDevice();
    initGainSettings(m_settings);
    initTunableElementsSettings(m_settings);
    initStreamArgSettings(m_settings);
    initDeviceArgSettings(m_settings);

    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SoapySDRInput::networkManagerFinished
    );
}

SoapySDRInput::~SoapySDRInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SoapySDRInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void SoapySDRInput::destroy()
{
    delete this;
}

bool SoapySDRInput::openDevice()
{
    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("SoapySDRInput::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("SoapySDRInput::openDevice: allocated SampleFifo");
    }

    // look for Rx buddies and get reference to the device object
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        qDebug("SoapySDRInput::openDevice: look in Rx buddies");

        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sourceBuddy->getBuddySharedPtr();

        if (!deviceSoapySDRShared)
        {
            qCritical("SoapySDRInput::openDevice: the source buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (!device)
        {
            qCritical("SoapySDRInput::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_device = device;
        m_deviceShared.m_deviceParams = deviceSoapySDRShared->m_deviceParams;
    }
    // look for Tx buddies and get reference to the device object
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("SoapySDRInput::openDevice: look in Tx buddies");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sinkBuddy->getBuddySharedPtr();

        if (!deviceSoapySDRShared)
        {
            qCritical("SoapySDRInput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (!device)
        {
            qCritical("SoapySDRInput::openDevice: cannot get device pointer from Tx buddy");
            return false;
        }

        m_deviceShared.m_device = device;
        m_deviceShared.m_deviceParams = deviceSoapySDRShared->m_deviceParams;
    }
    // There are no buddies then create the first SoapySDR device
    else
    {
        qDebug("SoapySDRInput::openDevice: open device here");
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        m_deviceShared.m_device = deviceSoapySDR.openSoapySDR(m_deviceAPI->getSamplingDeviceSequence(), m_deviceAPI->getHardwareUserArguments());

        if (!m_deviceShared.m_device)
        {
            qCritical("BladeRF2Input::openDevice: cannot open BladeRF2 device");
            return false;
        }

        m_deviceShared.m_deviceParams = new DeviceSoapySDRParams(m_deviceShared.m_device);
    }

    m_deviceShared.m_channel = m_deviceAPI->getDeviceItemIndex(); // publicly allocate channel
    m_deviceShared.m_source = this;
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void SoapySDRInput::closeDevice()
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
    m_deviceShared.m_source = nullptr;

    // No buddies so effectively close the device and delete parameters

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = nullptr;
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        deviceSoapySDR.closeSoapySdr(m_deviceShared.m_device);
        m_deviceShared.m_device = nullptr;
    }
}

void SoapySDRInput::getFrequencyRange(uint64_t& min, uint64_t& max)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);

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

void SoapySDRInput::getGlobalGainRange(int& min, int& max)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);

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

bool SoapySDRInput::isAGCSupported()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_hasAGC;
}

const std::vector<std::string>& SoapySDRInput::getAntennas()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_antennas;
}

const SoapySDR::RangeList& SoapySDRInput::getRateRanges()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_ratesRanges;
}

const SoapySDR::RangeList& SoapySDRInput::getBandwidthRanges()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_bandwidthsRanges;
}

int SoapySDRInput::getAntennaIndex(const std::string& antenna)
{
    const std::vector<std::string>& antennaList = getAntennas();
    std::vector<std::string>::const_iterator it = std::find(antennaList.begin(), antennaList.end(), antenna);

    if (it == antennaList.end()) {
        return -1;
    } else {
        return it - antennaList.begin();
    }
}

const std::vector<DeviceSoapySDRParams::FrequencySetting>& SoapySDRInput::getTunableElements()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_frequencySettings;
}

const std::vector<DeviceSoapySDRParams::GainSetting>& SoapySDRInput::getIndividualGainsRanges()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_gainSettings;
}

const SoapySDR::ArgInfoList& SoapySDRInput::getStreamArgInfoList()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_streamSettingsArgs;
}

const SoapySDR::ArgInfoList& SoapySDRInput::getDeviceArgInfoList()
{
    return  m_deviceShared.m_deviceParams->getDeviceArgs();
}

void SoapySDRInput::initGainSettings(SoapySDRInputSettings& settings)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    settings.m_individualGains.clear();
    settings.m_globalGain = 0;

    for (const auto &it : channelSettings->m_gainSettings) {
        settings.m_individualGains[QString(it.m_name.c_str())] = 0.0;
    }

    updateGains(m_deviceShared.m_device, m_deviceShared.m_channel, settings);
}

void SoapySDRInput::initTunableElementsSettings(SoapySDRInputSettings& settings)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
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

void SoapySDRInput::initStreamArgSettings(SoapySDRInputSettings& settings)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
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

void SoapySDRInput::initDeviceArgSettings(SoapySDRInputSettings& settings)
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

bool SoapySDRInput::hasDCAutoCorrection()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_hasDCAutoCorrection;
}

bool SoapySDRInput::hasDCCorrectionValue()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_hasDCOffsetValue;
}

bool SoapySDRInput::hasIQCorrectionValue()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_hasIQBalanceValue;
}

void SoapySDRInput::init()
{
    applySettings(m_settings, true);
}

SoapySDRInputThread *SoapySDRInput::findThread()
{
    if (!m_thread) // this does not own the thread
    {
        SoapySDRInputThread *soapySDRInputThread = nullptr;

        // find a buddy that has allocated the thread
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it)
        {
            SoapySDRInput *buddySource = ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_source;

            if (buddySource)
            {
                soapySDRInputThread = buddySource->getThread();

                if (soapySDRInputThread) {
                    break;
                }
            }
        }

        return soapySDRInputThread;
    }
    else
    {
        return m_thread; // own thread
    }
}

void SoapySDRInput::moveThreadToBuddy()
{
    const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceAPI*>::const_iterator it = sourceBuddies.begin();

    for (; it != sourceBuddies.end(); ++it)
    {
        SoapySDRInput *buddySource = ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_source;

        if (buddySource)
        {
            buddySource->setThread(m_thread);
            m_thread = nullptr;  // zero for others
        }
    }
}

bool SoapySDRInput::start()
{
    // There is a single thread per physical device (Rx side). This thread is unique and referenced by a unique
    // buddy in the group of source buddies associated with this physical device.
    //
    // This start method is responsible for managing the thread and number of channels when the streaming of a Rx channel is started
    //
    // It checks the following conditions
    //   - the thread is allocated or not (by itself or one of its buddies). If it is it grabs the thread pointer.
    //   - the requested channel is the first (0) or the following
    //
    // There are two possible working modes:
    //   - Single Input (SI) with only one channel streaming. This HAS to be channel 0.
    //   - Multiple Input (MI) with two or more channels. It MUST be in this configuration if any channel other than 0
    //     is used irrespective of what you actually do with samples coming from ignored channels.
    //     For example When we will run with only channel 2 streaming from the client perspective the channels 0 and 1 will actually
    //     be enabled and streaming but its samples will just be disregarded.
    //     This means that all channels up to the highest in index being used are activated.
    //
    // It manages the transition form SI where only one channel (the first or channel 0) should be running to the
    // Multiple Input (MI) if the requested channel is 1 or more. More generally it checks if the requested channel is within the current
    // channel range allocated in the thread or past it. To perform the transition it stops the thread, deletes it and creates a new one.
    // It marks the thread as needing start.
    //
    // If the requested channel is within the thread channel range (this thread being already allocated) it simply adds its FIFO reference
    // so that the samples are fed to the FIFO and leaves the thread unchanged (no stop, no delete/new)
    //
    // If there is no thread allocated it creates a new one with a number of channels that fits the requested channel. That is
    // 1 if channel 0 is requested (SI mode) and 3 if channel 2 is requested (MI mode). It marks the thread as needing start.
    //
    // Eventually it registers the FIFO in the thread. If the thread has to be started it enables the channels up to the number of channels
    // allocated in the thread and starts the thread.
    //
    // Note: this is quite similar to the BladeRF2 start handling. The main difference is that the channel allocation (enabling) process is
    // done in the thread object.

    if (!m_openSuccess)
    {
        qWarning("SoapySDRInput::start: cannot start device");
        return false;
    }

    if (!m_deviceShared.m_device)
    {
        qDebug("SoapySDRInput::start: no device object");
        return false;
    }

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    SoapySDRInputThread *soapySDRInputThread = findThread();
    bool needsStart = false;

    if (soapySDRInputThread) // if thread is already allocated
    {
        qDebug("SoapySDRInput::start: thread is already allocated");

        int nbOriginalChannels = soapySDRInputThread->getNbChannels();

        if (requestedChannel+1 > nbOriginalChannels) // expansion by deleting and re-creating the thread
        {
            qDebug("SoapySDRInput::start: expand channels. Re-allocate thread and take ownership");

            SampleSinkFifo **fifos = new SampleSinkFifo*[nbOriginalChannels];
            unsigned int *log2Decims = new unsigned int[nbOriginalChannels];
            int *fcPoss = new int[nbOriginalChannels];

            for (int i = 0; i < nbOriginalChannels; i++) // save original FIFO references and data
            {
                fifos[i] = soapySDRInputThread->getFifo(i);
                log2Decims[i] = soapySDRInputThread->getLog2Decimation(i);
                fcPoss[i] = soapySDRInputThread->getFcPos(i);
            }

            soapySDRInputThread->stopWork();
            delete soapySDRInputThread;
            soapySDRInputThread = new SoapySDRInputThread(m_deviceShared.m_device, requestedChannel+1);
            m_thread = soapySDRInputThread; // take ownership

            for (int i = 0; i < nbOriginalChannels; i++) // restore original FIFO references
            {
                soapySDRInputThread->setFifo(i, fifos[i]);
                soapySDRInputThread->setLog2Decimation(i, log2Decims[i]);
                soapySDRInputThread->setFcPos(i, fcPoss[i]);
            }

            // remove old thread address from buddies (reset in all buddies). The address being held only in the owning source.
            const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
            std::vector<DeviceAPI*>::const_iterator it = sourceBuddies.begin();

            for (; it != sourceBuddies.end(); ++it) {
                ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
            }

            delete[] fcPoss;
            delete[] log2Decims;
            delete[] fifos;

            needsStart = true;
        }
        else
        {
            qDebug("SoapySDRInput::start: keep buddy thread");
        }
    }
    else // first allocation
    {
        qDebug("SoapySDRInput::start: allocate thread and take ownership");
        soapySDRInputThread = new SoapySDRInputThread(m_deviceShared.m_device, requestedChannel+1);
        m_thread = soapySDRInputThread; // take ownership
        needsStart = true;
    }

    soapySDRInputThread->setFifo(requestedChannel, &m_sampleFifo);
    soapySDRInputThread->setLog2Decimation(requestedChannel, m_settings.m_log2Decim);
    soapySDRInputThread->setFcPos(requestedChannel, (int) m_settings.m_fcPos);

    if (needsStart)
    {
        qDebug("SoapySDRInput::start: (re)start buddy thread");
        soapySDRInputThread->setSampleRate(m_settings.m_devSampleRate);
        soapySDRInputThread->startWork();
    }

    qDebug("SoapySDRInput::start: started");
    m_running = true;

    return true;
}

void SoapySDRInput::stop()
{
    // This stop method is responsible for managing the thread and channel disabling when the streaming of
    // a Rx channel is stopped
    //
    // If the thread is currently managing only one channel (SI mode). The thread can be just stopped and deleted.
    // Then the channel is closed (disabled).
    //
    // If the thread is currently managing many channels (MI mode) and we are removing the last channel. The transition
    // or reduction of MI size is handled by stopping the thread, deleting it and creating a new one
    // with the maximum number of channels needed if (and only if) there is still a channel active.
    //
    // If the thread is currently managing many channels (MI mode) but the channel being stopped is not the last
    // channel then the FIFO reference is simply removed from the thread so that it will not stream into this FIFO
    // anymore. In this case the channel is not closed (this is managed in the thread object) so that other channels
    // can continue with the same configuration. The device continues streaming on this channel but the samples are simply
    // dropped (by removing FIFO reference).
    //
    // Note: this is quite similar to the BladeRF2 stop handling. The main difference is that the channel allocation (enabling) process is
    // done in the thread object.

    if (!m_running) {
        return;
    }

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    SoapySDRInputThread *soapySDRInputThread = findThread();

    if (!soapySDRInputThread) { // no thread allocated
        return;
    }

    int nbOriginalChannels = soapySDRInputThread->getNbChannels();

    if (nbOriginalChannels == 1) // SI mode => just stop and delete the thread
    {
        qDebug("SoapySDRInput::stop: SI mode. Just stop and delete the thread");
        soapySDRInputThread->stopWork();
        delete soapySDRInputThread;
        m_thread = nullptr;

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it) {
            ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
        }
    }
    else if (requestedChannel == nbOriginalChannels - 1) // remove last MI channel => reduce by deleting and re-creating the thread
    {
        qDebug("SoapySDRInput::stop: MI mode. Reduce by deleting and re-creating the thread");
        soapySDRInputThread->stopWork();
        SampleSinkFifo **fifos = new SampleSinkFifo*[nbOriginalChannels-1];
        unsigned int *log2Decims = new unsigned int[nbOriginalChannels-1];
        int *fcPoss = new int[nbOriginalChannels-1];
        int highestActiveChannelIndex = -1;

        for (int i = 0; i < nbOriginalChannels-1; i++) // save original FIFO references and get the channel with highest index
        {
            fifos[i] = soapySDRInputThread->getFifo(i);

            if ((soapySDRInputThread->getFifo(i) != 0) && (i > highestActiveChannelIndex)) {
                highestActiveChannelIndex = i;
            }

            log2Decims[i] = soapySDRInputThread->getLog2Decimation(i);
            fcPoss[i] = soapySDRInputThread->getFcPos(i);
        }

        delete soapySDRInputThread;
        m_thread = nullptr;

        if (highestActiveChannelIndex >= 0) // there is at least one channel still active
        {
            soapySDRInputThread = new SoapySDRInputThread(m_deviceShared.m_device, highestActiveChannelIndex+1);
            m_thread = soapySDRInputThread; // take ownership

            for (int i = 0; i < highestActiveChannelIndex; i++)  // restore original FIFO references
            {
                soapySDRInputThread->setFifo(i, fifos[i]);
                soapySDRInputThread->setLog2Decimation(i, log2Decims[i]);
                soapySDRInputThread->setFcPos(i, fcPoss[i]);
            }
        }
        else
        {
            qDebug("SoapySDRInput::stop: do not re-create thread as there are no more FIFOs active");
        }

        // remove old thread address from buddies (reset in all buddies). The address being held only in the owning source.
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it) {
            ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
        }

        if (highestActiveChannelIndex >= 0)
        {
            qDebug("SoapySDRInput::stop: restarting the thread");
            soapySDRInputThread->startWork();
        }

        delete[] fcPoss;
        delete[] log2Decims;
        delete[] fifos;
    }
    else // remove channel from existing thread
    {
        qDebug("SoapySDRInput::stop: MI mode. Not changing MI configuration. Just remove FIFO reference");
        soapySDRInputThread->setFifo(requestedChannel, nullptr); // remove FIFO
    }

    m_running = false;
}

QByteArray SoapySDRInput::serialize() const
{
    SimpleSerializer s(1);
    return s.final();
}

bool SoapySDRInput::deserialize(const QByteArray& data)
{
    (void) data;
    return false;
}

const QString& SoapySDRInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SoapySDRInput::getSampleRate() const
{
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2Decim));
}

quint64 SoapySDRInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void SoapySDRInput::setCenterFrequency(qint64 centerFrequency)
{
    SoapySDRInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureSoapySDRInput *message = MsgConfigureSoapySDRInput::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSoapySDRInput* messageToGUI = MsgConfigureSoapySDRInput::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool SoapySDRInput::setDeviceCenterFrequency(SoapySDR::Device *dev, int requestedChannel, quint64 freq_hz, int loPpmTenths)
{
    qint64 df = ((qint64)freq_hz * loPpmTenths) / 10000000LL;
    freq_hz -= df;

    try
    {
        dev->setFrequency(SOAPY_SDR_RX,
                requestedChannel,
                m_deviceShared.m_deviceParams->getRxChannelMainTunableElementName(requestedChannel),
                freq_hz);
        qDebug("SoapySDRInput::setDeviceCenterFrequency: setFrequency(%llu)", freq_hz);
        return true;
    }
    catch (const std::exception &ex)
    {
        qCritical("SoapySDRInput::applySettings: could not set frequency: %llu: %s", freq_hz, ex.what());
        return false;
    }
}

void SoapySDRInput::updateGains(SoapySDR::Device *dev, int requestedChannel, SoapySDRInputSettings& settings)
{
    if (!dev) {
        return;
    }

    try
    {
        settings.m_globalGain = round(dev->getGain(SOAPY_SDR_RX, requestedChannel));

        for (const auto &name : settings.m_individualGains.keys()) {
            settings.m_individualGains[name] = dev->getGain(SOAPY_SDR_RX, requestedChannel, name.toStdString());
        }
    }
    catch (const std::exception &ex)
    {
        qCritical("SoapySDRInput::updateGains: caught exception: %s", ex.what());
    }
}

void SoapySDRInput::updateTunableElements(SoapySDR::Device *dev, int requestedChannel, SoapySDRInputSettings& settings)
{
    if (!dev) {
        return;
    }

    try
    {
        for (const auto &name : settings.m_tunableElements.keys()) {
            settings.m_tunableElements[name] = dev->getFrequency(SOAPY_SDR_RX, requestedChannel, name.toStdString());
        }
    }
    catch (const std::exception &ex)
    {
        qCritical("SoapySDRInput::updateTunableElements: caught exception: %s", ex.what());
    }
}

bool SoapySDRInput::handleMessage(const Message& message)
{
    if (MsgConfigureSoapySDRInput::match(message))
    {
        MsgConfigureSoapySDRInput& conf = (MsgConfigureSoapySDRInput&) message;
        qDebug() << "SoapySDRInput::handleMessage: MsgConfigureSoapySDRInput";

        if (!applySettings(conf.getSettings(), conf.getForce())) {
            qDebug("SoapySDRInput::handleMessage: MsgConfigureSoapySDRInput config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SoapySDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
        DeviceSoapySDRShared::MsgReportBuddyChange& report = (DeviceSoapySDRShared::MsgReportBuddyChange&) message;
        SoapySDRInputSettings settings = m_settings;
        settings.m_fcPos = (SoapySDRInputSettings::fcPos_t) report.getFcPos();
        //bool fromRxBuddy = report.getRxElseTx();

        double centerFrequency = m_deviceShared.m_device->getFrequency(
                SOAPY_SDR_RX,
                requestedChannel,
                m_deviceShared.m_deviceParams->getRxChannelMainTunableElementName(requestedChannel));

        settings.m_centerFrequency = round(centerFrequency/1000.0) * 1000;
        settings.m_devSampleRate = round(m_deviceShared.m_device->getSampleRate(SOAPY_SDR_RX, requestedChannel));
        settings.m_bandwidth = round(m_deviceShared.m_device->getBandwidth(SOAPY_SDR_RX, requestedChannel));

        SoapySDRInputThread *inputThread = findThread();

        if (inputThread)
        {
            inputThread->setFcPos(requestedChannel, (int) settings.m_fcPos);
        }

        m_settings = settings;

        // propagate settings to GUI if any
        if (getMessageQueueToGUI())
        {
            MsgConfigureSoapySDRInput *reportToGUI = MsgConfigureSoapySDRInput::create(m_settings, false);
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
                qDebug("SoapySDRInput::handleMessage: MsgReportDeviceArgsChange: device argument %s set to %s",
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

bool SoapySDRInput::applySettings(const SoapySDRInputSettings& settings, bool force)
{
    bool forwardChangeOwnDSP = false;
    bool forwardChangeToBuddies  = false;
    bool globalGainChanged = false;
    bool individualGainsChanged = false;
    bool deviceArgsChanged = false;
    QList<QString> reverseAPIKeys;

    SoapySDR::Device *dev = m_deviceShared.m_device;
    SoapySDRInputThread *inputThread = findThread();
    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    qint64 xlatedDeviceCenterFrequency = settings.m_centerFrequency;
    xlatedDeviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
    xlatedDeviceCenterFrequency = xlatedDeviceCenterFrequency < 0 ? 0 : xlatedDeviceCenterFrequency;

    if ((m_settings.m_softDCCorrection != settings.m_softDCCorrection) || force) {
        reverseAPIKeys.append("softDCCorrection");
    }
    if ((m_settings.m_softIQCorrection != settings.m_softIQCorrection) || force) {
        reverseAPIKeys.append("softIQCorrection");
    }

    if ((m_settings.m_softDCCorrection != settings.m_softDCCorrection) ||
        (m_settings.m_softIQCorrection != settings.m_softIQCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_softDCCorrection, settings.m_softIQCorrection);
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
                dev->setSampleRate(SOAPY_SDR_RX, requestedChannel, settings.m_devSampleRate);
                qDebug() << "SoapySDRInput::applySettings: setSampleRate OK: " << settings.m_devSampleRate;

                if (inputThread)
                {
                    bool wasRunning = inputThread->isRunning();
                    inputThread->stopWork();
                    inputThread->setSampleRate(settings.m_devSampleRate);

                    if (wasRunning) {
                        inputThread->startWork();
                    }
                }
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDRInput::applySettings: could not set sample rate: %d: %s",
                        settings.m_devSampleRate, ex.what());
            }
        }
    }

    if ((m_settings.m_fcPos != settings.m_fcPos) || force)
    {
        reverseAPIKeys.append("fcPos");

        if (inputThread)
        {
            inputThread->setFcPos(requestedChannel, (int) settings.m_fcPos);
            qDebug() << "SoapySDRInput::applySettings: set fc pos (enum) to " << (int) settings.m_fcPos;
        }
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        reverseAPIKeys.append("log2Decim");
        forwardChangeOwnDSP = true;
        SoapySDRInputThread *inputThread = findThread();

        if (inputThread)
        {
            inputThread->setLog2Decimation(requestedChannel, settings.m_log2Decim);
            qDebug() << "SoapySDRInput::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if ((m_settings.m_iqOrder != settings.m_iqOrder) || force)
    {
        reverseAPIKeys.append("iqOrder");
        SoapySDRInputThread *inputThread = findThread();

        if (inputThread)
        {
            inputThread->setIQOrder(settings.m_iqOrder);
            qDebug() << "SoapySDRInput::applySettings: set IQ order to " << (settings.m_iqOrder ? "IQ" : "QI");
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
        || (m_settings.m_fcPos != settings.m_fcPos)
        || (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                xlatedDeviceCenterFrequency,
                0,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                false);

        forwardChangeOwnDSP = true;
        forwardChangeToBuddies = true;

        if (dev) {
            setDeviceCenterFrequency(dev, requestedChannel, deviceCenterFrequency, settings.m_LOppmTenths);
        }
    }

    if ((m_settings.m_antenna != settings.m_antenna) || force)
    {
        reverseAPIKeys.append("antenna");

        if (dev)
        {
            try
            {
                dev->setAntenna(SOAPY_SDR_RX, requestedChannel, settings.m_antenna.toStdString());
                qDebug("SoapySDRInput::applySettings: set antenna to %s", settings.m_antenna.toStdString().c_str());
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDRInput::applySettings: cannot set antenna to %s: %s",
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
                dev->setBandwidth(SOAPY_SDR_RX, requestedChannel, settings.m_bandwidth);
                qDebug("SoapySDRInput::applySettings: bandwidth set to %u", settings.m_bandwidth);
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDRInput::applySettings: cannot set bandwidth to %u: %s",
                        settings.m_bandwidth, ex.what());
            }
        }
    }

    for (const auto &oname : m_settings.m_tunableElements.keys())
    {
        auto nvalue = settings.m_tunableElements.find(oname);

        if (nvalue != settings.m_tunableElements.end() && ((m_settings.m_tunableElements[oname] != *nvalue) ||force))
        {
            if (dev)
            {
                try
                {
                    dev->setFrequency(SOAPY_SDR_RX, requestedChannel, oname.toStdString(), *nvalue);
                    qDebug("SoapySDRInput::applySettings: tunable element %s frequency set to %lf",
                            oname.toStdString().c_str(), *nvalue);
                }
                catch (const std::exception &ex)
                {
                    qCritical("SoapySDRInput::applySettings: cannot set tunable element %s to %lf: %s",
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
                dev->setGain(SOAPY_SDR_RX, requestedChannel, settings.m_globalGain);
                qDebug("SoapySDRInput::applySettings: set global gain to %d", settings.m_globalGain);
                globalGainChanged = true;
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDRInput::applySettings: cannot set global gain to %d: %s",
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
                    dev->setGain(SOAPY_SDR_RX, requestedChannel, oname.toStdString(), *nvalue);
                    qDebug("SoapySDRInput::applySettings: individual gain %s set to %lf",
                            oname.toStdString().c_str(), *nvalue);
                    individualGainsChanged = true;
                }
                catch (const std::exception &ex)
                {
                    qCritical("SoapySDRInput::applySettings: cannot set individual gain %s to %lf: %s",
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
                dev->setGainMode(SOAPY_SDR_RX, requestedChannel, settings.m_autoGain);
                qDebug("SoapySDRInput::applySettings: %s AGC", settings.m_autoGain ? "set" : "unset");
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDRInput::applySettings: cannot %s AGC", settings.m_autoGain ? "set" : "unset");
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
                dev->setDCOffsetMode(SOAPY_SDR_RX, requestedChannel, settings.m_autoDCCorrection);
                qDebug("SoapySDRInput::applySettings: %s DC auto correction", settings.m_autoDCCorrection ? "set" : "unset");
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDRInput::applySettings: cannot %s DC auto correction", settings.m_autoDCCorrection ? "set" : "unset");
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
                dev->setDCOffset(SOAPY_SDR_RX, requestedChannel, settings.m_dcCorrection);
                qDebug("SoapySDRInput::applySettings: DC offset correction set to (%lf, %lf)", settings.m_dcCorrection.real(), settings.m_dcCorrection.imag());
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDRInput::applySettings: cannot set DC offset correction to (%lf, %lf)", settings.m_dcCorrection.real(), settings.m_dcCorrection.imag());
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
                dev->setIQBalance(SOAPY_SDR_RX, requestedChannel, settings.m_iqCorrection);
                qDebug("SoapySDRInput::applySettings: IQ balance correction set to (%lf, %lf)", settings.m_iqCorrection.real(), settings.m_iqCorrection.imag());
            }
            catch (const std::exception &ex)
            {
                qCritical("SoapySDRInput::applySettings: cannot set IQ balance correction to (%lf, %lf)", settings.m_iqCorrection.real(), settings.m_iqCorrection.imag());
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
                    dev->writeSetting(SOAPY_SDR_RX, requestedChannel, oname.toStdString(), nvalue->toString().toStdString());
                    qDebug("SoapySDRInput::applySettings: stream argument %s set to %s",
                            oname.toStdString().c_str(), nvalue->toString().toStdString().c_str());
                }
                catch (const std::exception &ex)
                {
                    qCritical("SoapySDRInput::applySettings: cannot set stream argument %s to %s: %s",
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
                    qDebug("SoapySDRInput::applySettings: device argument %s set to %s",
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
        int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Decim);
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
                    (int) settings.m_fcPos,
                    settings.m_devSampleRate,
                    true);
            itSource->getSamplingDeviceInputMessageQueue()->push(report);
        }

        for (const auto &itSink : sinkBuddies)
        {
            DeviceSoapySDRShared::MsgReportBuddyChange *report = DeviceSoapySDRShared::MsgReportBuddyChange::create(
                    settings.m_centerFrequency,
                    settings.m_LOppmTenths,
                    (int) settings.m_fcPos,
                    settings.m_devSampleRate,
                    true);
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

    qDebug() << "SoapySDRInput::applySettings: "
            << " m_transverterMode: " << m_settings.m_transverterMode
            << " m_transverterDeltaFrequency: " << m_settings.m_transverterDeltaFrequency
            << " m_centerFrequency: " << m_settings.m_centerFrequency << " Hz"
            << " m_LOppmTenths: " << m_settings.m_LOppmTenths
            << " m_log2Decim: " << m_settings.m_log2Decim
            << " m_iqOrder: " << m_settings.m_iqOrder
            << " m_fcPos: " << m_settings.m_fcPos
            << " m_devSampleRate: " << m_settings.m_devSampleRate
            << " m_softDCCorrection: " << m_settings.m_softDCCorrection
            << " m_softIQCorrection: " << m_settings.m_softIQCorrection
            << " m_antenna: " << m_settings.m_antenna
            << " m_bandwidth: " << m_settings.m_bandwidth
            << " m_globalGain: " << m_settings.m_globalGain
            << " force: " << force;

        QMap<QString, double>::const_iterator doubleIt = m_settings.m_individualGains.begin();

        for(; doubleIt != m_settings.m_individualGains.end(); ++doubleIt) {
            qDebug("SoapySDRInput::applySettings: m_individualGains[%s]: %lf", doubleIt.key().toStdString().c_str(), doubleIt.value());
        }

        doubleIt = m_settings.m_tunableElements.begin();

        for(; doubleIt != m_settings.m_tunableElements.end(); ++doubleIt) {
            qDebug("SoapySDRInput::applySettings: m_tunableElements[%s]: %lf", doubleIt.key().toStdString().c_str(), doubleIt.value());
        }

        QMap<QString, QVariant>::const_iterator varIt = m_settings.m_deviceArgSettings.begin();

        for(; varIt != m_settings.m_deviceArgSettings.end(); ++varIt)
        {
            qDebug("SoapySDRInput::applySettings: m_deviceArgSettings[%s] (type %d): %s",
                varIt.key().toStdString().c_str(),
                (int) varIt.value().type(), // bool: 1, int: 2, double: 6, string: 10 (http://doc.qt.io/archives/qt-4.8/qvariant.html)
                varIt.value().toString().toStdString().c_str());
        }

        varIt = m_settings.m_streamArgSettings.begin();

        for(; varIt != m_settings.m_streamArgSettings.end(); ++varIt)
        {
            qDebug("SoapySDRInput::applySettings: m_streamArgSettings[%s] (type %d): %s",
                varIt.key().toStdString().c_str(),
                (int) varIt.value().type(),
                varIt.value().toString().toStdString().c_str());
        }

    return true;
}

int SoapySDRInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setSoapySdrInputSettings(new SWGSDRangel::SWGSoapySDRInputSettings());
    response.getSoapySdrInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int SoapySDRInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    SoapySDRInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureSoapySDRInput *msg = MsgConfigureSoapySDRInput::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSoapySDRInput *msgToGUI = MsgConfigureSoapySDRInput::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void SoapySDRInput::webapiUpdateDeviceSettings(
        SoapySDRInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    for (int i = 0; i < deviceSettingsKeys.count(); i++) {
        qDebug("SoapySDRInput::webapiUpdateDeviceSettings %s", qPrintable(deviceSettingsKeys.at(i)));
    }

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getSoapySdrInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getSoapySdrInputSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getSoapySdrInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getSoapySdrInputSettings()->getBandwidth();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getSoapySdrInputSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getSoapySdrInputSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        settings.m_fcPos = static_cast<SoapySDRInputSettings::fcPos_t>(response.getSoapySdrInputSettings()->getFcPos());
    }
    if (deviceSettingsKeys.contains("softDCCorrection")) {
        settings.m_softDCCorrection = response.getSoapySdrInputSettings()->getSoftDcCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("softIQCorrection")) {
        settings.m_softIQCorrection = response.getSoapySdrInputSettings()->getSoftIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getSoapySdrInputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getSoapySdrInputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("antenna")) {
        settings.m_antenna = *response.getSoapySdrInputSettings()->getAntenna();
    }

    if (deviceSettingsKeys.contains("tunableElements"))
    {
        QList<SWGSDRangel::SWGArgValue*> *tunableElements = response.getSoapySdrInputSettings()->getTunableElements();

        for (const auto &itArg : *tunableElements)
        {
            QMap<QString, double>::iterator itSettings = settings.m_tunableElements.find(*(itArg->getKey()));

            if (itSettings != settings.m_tunableElements.end())
            {
                QVariant v = webapiVariantFromArgValue(itArg);
                itSettings.value() = v.toDouble();
            }
            else
            {
                QVariant v = webapiVariantFromArgValue(itArg);
                settings.m_tunableElements.insert(*itArg->getKey(), v.toDouble());
            }
        }
    }

    if (deviceSettingsKeys.contains("globalGain")) {
        settings.m_globalGain = response.getSoapySdrInputSettings()->getGlobalGain();
    }

    if (deviceSettingsKeys.contains("individualGains"))
    {
        QList<SWGSDRangel::SWGArgValue*> *individualGains = response.getSoapySdrInputSettings()->getIndividualGains();

        for (const auto &itArg : *individualGains)
        {
            QMap<QString, double>::iterator itSettings = settings.m_individualGains.find(*(itArg->getKey()));

            if (itSettings != settings.m_individualGains.end())
            {
                QVariant v = webapiVariantFromArgValue(itArg);
                itSettings.value() = v.toDouble();
            }
            else
            {
                QVariant v = webapiVariantFromArgValue(itArg);
                settings.m_individualGains.insert(*itArg->getKey(), v.toDouble());
            }
        }
    }

    if (deviceSettingsKeys.contains("autoGain")) {
        settings.m_autoGain = response.getSoapySdrInputSettings()->getAutoGain() != 0;
    }
    if (deviceSettingsKeys.contains("autoDCCorrection")) {
        settings.m_autoDCCorrection = response.getSoapySdrInputSettings()->getAutoDcCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("autoIQCorrection")) {
        settings.m_autoIQCorrection = response.getSoapySdrInputSettings()->getAutoIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("dcCorrection"))
    {
        settings.m_dcCorrection.real(response.getSoapySdrInputSettings()->getDcCorrection()->getReal());
        settings.m_dcCorrection.imag(response.getSoapySdrInputSettings()->getDcCorrection()->getImag());
    }
    if (deviceSettingsKeys.contains("iqCorrection"))
    {
        settings.m_iqCorrection.real(response.getSoapySdrInputSettings()->getIqCorrection()->getReal());
        settings.m_iqCorrection.imag(response.getSoapySdrInputSettings()->getIqCorrection()->getImag());
    }

    if (deviceSettingsKeys.contains("streamArgSettings"))
    {
        QList<SWGSDRangel::SWGArgValue*> *streamArgSettings = response.getSoapySdrInputSettings()->getStreamArgSettings();

        for (const auto itArg : *streamArgSettings)
        {
            QMap<QString, QVariant>::iterator itSettings = settings.m_streamArgSettings.find(*itArg->getKey());

            if (itSettings != settings.m_streamArgSettings.end()) {
                itSettings.value() = webapiVariantFromArgValue(itArg);
            } else {
                settings.m_streamArgSettings.insert(*itArg->getKey(), webapiVariantFromArgValue(itArg));
            }
        }
    }

    if (deviceSettingsKeys.contains("deviceArgSettings"))
    {
        QList<SWGSDRangel::SWGArgValue*> *deviceArgSettings = response.getSoapySdrInputSettings()->getDeviceArgSettings();

        for (const auto itArg : *deviceArgSettings)
        {
            QMap<QString, QVariant>::iterator itSettings = settings.m_deviceArgSettings.find(*itArg->getKey());

            if (itSettings != settings.m_deviceArgSettings.end()) {
                itSettings.value() = webapiVariantFromArgValue(itArg);
            } else {
                settings.m_deviceArgSettings.insert(*itArg->getKey(), webapiVariantFromArgValue(itArg));
            }
        }
    }

    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSoapySdrInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSoapySdrInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSoapySdrInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getSoapySdrInputSettings()->getReverseApiDeviceIndex();
    }
}

int SoapySDRInput::webapiReportGet(SWGSDRangel::SWGDeviceReport& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setSoapySdrInputReport(new SWGSDRangel::SWGSoapySDRReport());
    response.getSoapySdrInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int SoapySDRInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int SoapySDRInput::webapiRun(
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

void SoapySDRInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SoapySDRInputSettings& settings)
{
    response.getSoapySdrInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getSoapySdrInputSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getSoapySdrInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getSoapySdrInputSettings()->setLog2Decim(settings.m_log2Decim);
    response.getSoapySdrInputSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getSoapySdrInputSettings()->setFcPos((int) settings.m_fcPos);
    response.getSoapySdrInputSettings()->setSoftDcCorrection(settings.m_softDCCorrection ? 1 : 0);
    response.getSoapySdrInputSettings()->setSoftIqCorrection(settings.m_softIQCorrection ? 1 : 0);
    response.getSoapySdrInputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getSoapySdrInputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    if (response.getSoapySdrInputSettings()->getAntenna()) {
        *response.getSoapySdrInputSettings()->getAntenna() = settings.m_antenna;
    } else {
        response.getSoapySdrInputSettings()->setAntenna(new QString(settings.m_antenna));
    }

    if (response.getSoapySdrInputSettings()->getTunableElements()) {
        response.getSoapySdrInputSettings()->getTunableElements()->clear();
    } else {
        response.getSoapySdrInputSettings()->setTunableElements(new QList<SWGSDRangel::SWGArgValue*>);
    }

    for (const auto& itName : settings.m_tunableElements.keys())
    {
        response.getSoapySdrInputSettings()->getTunableElements()->append(new SWGSDRangel::SWGArgValue);
        response.getSoapySdrInputSettings()->getTunableElements()->back()->setKey(new QString(itName));
        double value = settings.m_tunableElements.value(itName);
        response.getSoapySdrInputSettings()->getTunableElements()->back()->setValueString(new QString(tr("%1").arg(value)));
        response.getSoapySdrInputSettings()->getTunableElements()->back()->setValueType(new QString("float"));
    }

    response.getSoapySdrInputSettings()->setBandwidth(settings.m_bandwidth);
    response.getSoapySdrInputSettings()->setGlobalGain(settings.m_globalGain);

    if (response.getSoapySdrInputSettings()->getIndividualGains()) {
        response.getSoapySdrInputSettings()->getIndividualGains()->clear();
    } else {
        response.getSoapySdrInputSettings()->setIndividualGains(new QList<SWGSDRangel::SWGArgValue*>);
    }

    for (const auto& itName : settings.m_individualGains.keys())
    {
        response.getSoapySdrInputSettings()->getIndividualGains()->append(new SWGSDRangel::SWGArgValue);
        response.getSoapySdrInputSettings()->getIndividualGains()->back()->setKey(new QString(itName));
        double value = settings.m_individualGains.value(itName);
        response.getSoapySdrInputSettings()->getIndividualGains()->back()->setValueString(new QString(tr("%1").arg(value)));
        response.getSoapySdrInputSettings()->getIndividualGains()->back()->setValueType(new QString("float"));
    }

    response.getSoapySdrInputSettings()->setAutoGain(settings.m_autoGain ? 1 : 0);
    response.getSoapySdrInputSettings()->setAutoDcCorrection(settings.m_autoDCCorrection ? 1 : 0);
    response.getSoapySdrInputSettings()->setAutoIqCorrection(settings.m_autoIQCorrection ? 1 : 0);

    if (!response.getSoapySdrInputSettings()->getDcCorrection()) {
        response.getSoapySdrInputSettings()->setDcCorrection(new SWGSDRangel::SWGComplex());
    }

    response.getSoapySdrInputSettings()->getDcCorrection()->setReal(settings.m_dcCorrection.real());
    response.getSoapySdrInputSettings()->getDcCorrection()->setImag(settings.m_dcCorrection.imag());

    if (!response.getSoapySdrInputSettings()->getIqCorrection()) {
        response.getSoapySdrInputSettings()->setIqCorrection(new SWGSDRangel::SWGComplex());
    }

    response.getSoapySdrInputSettings()->getIqCorrection()->setReal(settings.m_iqCorrection.real());
    response.getSoapySdrInputSettings()->getIqCorrection()->setImag(settings.m_iqCorrection.imag());

    if (response.getSoapySdrInputSettings()->getStreamArgSettings()) {
        response.getSoapySdrInputSettings()->getStreamArgSettings()->clear();
    } else {
        response.getSoapySdrInputSettings()->setStreamArgSettings(new QList<SWGSDRangel::SWGArgValue*>);
    }

    for (const auto& itName : settings.m_streamArgSettings.keys())
    {
        response.getSoapySdrInputSettings()->getStreamArgSettings()->append(new SWGSDRangel::SWGArgValue);
        response.getSoapySdrInputSettings()->getStreamArgSettings()->back()->setKey(new QString(itName));
        const QVariant& v = settings.m_streamArgSettings.value(itName);
        webapiFormatArgValue(v, response.getSoapySdrInputSettings()->getStreamArgSettings()->back());
    }

    if (response.getSoapySdrInputSettings()->getDeviceArgSettings()) {
        response.getSoapySdrInputSettings()->getDeviceArgSettings()->clear();
    } else {
        response.getSoapySdrInputSettings()->setDeviceArgSettings(new QList<SWGSDRangel::SWGArgValue*>);
    }

    for (const auto& itName : settings.m_deviceArgSettings.keys())
    {
        response.getSoapySdrInputSettings()->getDeviceArgSettings()->append(new SWGSDRangel::SWGArgValue);
        response.getSoapySdrInputSettings()->getDeviceArgSettings()->back()->setKey(new QString(itName));
        const QVariant& v = settings.m_deviceArgSettings.value(itName);
        webapiFormatArgValue(v, response.getSoapySdrInputSettings()->getDeviceArgSettings()->back());
    }

    response.getSoapySdrInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSoapySdrInputSettings()->getReverseApiAddress()) {
        *response.getSoapySdrInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSoapySdrInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSoapySdrInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSoapySdrInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void SoapySDRInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);

    response.getSoapySdrInputReport()->setDeviceSettingsArgs(new QList<SWGSDRangel::SWGArgInfo*>);

    for (const auto& itArg : m_deviceShared.m_deviceParams->getDeviceArgs())
    {
        response.getSoapySdrInputReport()->getDeviceSettingsArgs()->append(new SWGSDRangel::SWGArgInfo());
        webapiFormatArgInfo(itArg, response.getSoapySdrInputReport()->getDeviceSettingsArgs()->back());
    }

    response.getSoapySdrInputReport()->setStreamSettingsArgs(new QList<SWGSDRangel::SWGArgInfo*>);

    for (const auto& itArg : channelSettings->m_streamSettingsArgs)
    {
        response.getSoapySdrInputReport()->getStreamSettingsArgs()->append(new SWGSDRangel::SWGArgInfo());
        webapiFormatArgInfo(itArg, response.getSoapySdrInputReport()->getStreamSettingsArgs()->back());
    }

    response.getSoapySdrInputReport()->setFrequencySettingsArgs(new QList<SWGSDRangel::SWGArgInfo*>);

    for (const auto& itArg : channelSettings->m_frequencySettingsArgs)
    {
        response.getSoapySdrInputReport()->getFrequencySettingsArgs()->append(new SWGSDRangel::SWGArgInfo());
        webapiFormatArgInfo(itArg, response.getSoapySdrInputReport()->getFrequencySettingsArgs()->back());
    }

    response.getSoapySdrInputReport()->setHasAgc(channelSettings->m_hasAGC ? 1 : 0);
    response.getSoapySdrInputReport()->setHasDcAutoCorrection(channelSettings->m_hasDCAutoCorrection ? 1 : 0);
    response.getSoapySdrInputReport()->setHasDcOffsetValue(channelSettings->m_hasDCOffsetValue ? 1 : 0);
    response.getSoapySdrInputReport()->setHasFrequencyCorrectionValue(channelSettings->m_hasFrequencyCorrectionValue ? 1 : 0);
    response.getSoapySdrInputReport()->setHasIqBalanceValue(channelSettings->m_hasIQBalanceValue ? 1 : 0);

    if (channelSettings->m_antennas.size() != 0)
    {
        response.getSoapySdrInputReport()->setAntennas(new QList<QString *>);

        for (const auto& itAntenna : channelSettings->m_antennas) {
            response.getSoapySdrInputReport()->getAntennas()->append(new QString(itAntenna.c_str()));
        }
    }

    if ((channelSettings->m_gainRange.maximum() != 0.0) || (channelSettings->m_gainRange.minimum() != 0.0))
    {
        response.getSoapySdrInputReport()->setGainRange(new SWGSDRangel::SWGRangeFloat());
        response.getSoapySdrInputReport()->getGainRange()->setMin(channelSettings->m_gainRange.minimum());
        response.getSoapySdrInputReport()->getGainRange()->setMax(channelSettings->m_gainRange.maximum());
    }

    if (channelSettings->m_gainSettings.size() != 0)
    {
        response.getSoapySdrInputReport()->setGainSettings(new QList<SWGSDRangel::SWGSoapySDRGainSetting*>);

        for (const auto& itGain : channelSettings->m_gainSettings)
        {
            response.getSoapySdrInputReport()->getGainSettings()->append(new SWGSDRangel::SWGSoapySDRGainSetting());
            response.getSoapySdrInputReport()->getGainSettings()->back()->setRange(new SWGSDRangel::SWGRangeFloat());
            response.getSoapySdrInputReport()->getGainSettings()->back()->getRange()->setMin(itGain.m_range.minimum());
            response.getSoapySdrInputReport()->getGainSettings()->back()->getRange()->setMax(itGain.m_range.maximum());
            response.getSoapySdrInputReport()->getGainSettings()->back()->setName(new QString(itGain.m_name.c_str()));
        }
    }

    if (channelSettings->m_frequencySettings.size() != 0)
    {
        response.getSoapySdrInputReport()->setFrequencySettings(new QList<SWGSDRangel::SWGSoapySDRFrequencySetting*>);

        for (const auto& itFreq : channelSettings->m_frequencySettings)
        {
            response.getSoapySdrInputReport()->getFrequencySettings()->append(new SWGSDRangel::SWGSoapySDRFrequencySetting());
            response.getSoapySdrInputReport()->getFrequencySettings()->back()->setRanges(new QList<SWGSDRangel::SWGRangeFloat*>);

            for (const auto itRange : itFreq.m_ranges)
            {
                response.getSoapySdrInputReport()->getFrequencySettings()->back()->getRanges()->append(new SWGSDRangel::SWGRangeFloat());
                response.getSoapySdrInputReport()->getFrequencySettings()->back()->getRanges()->back()->setMin(itRange.minimum());
                response.getSoapySdrInputReport()->getFrequencySettings()->back()->getRanges()->back()->setMax(itRange.maximum());
            }

            response.getSoapySdrInputReport()->getFrequencySettings()->back()->setName(new QString(itFreq.m_name.c_str()));
        }
    }

    if (channelSettings->m_ratesRanges.size() != 0)
    {
        response.getSoapySdrInputReport()->setRatesRanges(new QList<SWGSDRangel::SWGRangeFloat*>);

        for (const auto itRange : channelSettings->m_ratesRanges)
        {
            response.getSoapySdrInputReport()->getRatesRanges()->append(new SWGSDRangel::SWGRangeFloat());
            response.getSoapySdrInputReport()->getRatesRanges()->back()->setMin(itRange.minimum());
            response.getSoapySdrInputReport()->getRatesRanges()->back()->setMax(itRange.maximum());
        }
    }

    if (channelSettings->m_bandwidthsRanges.size() != 0)
    {
        response.getSoapySdrInputReport()->setBandwidthsRanges(new QList<SWGSDRangel::SWGRangeFloat*>);

        for (const auto itBandwidth : channelSettings->m_bandwidthsRanges)
        {
            response.getSoapySdrInputReport()->getBandwidthsRanges()->append(new SWGSDRangel::SWGRangeFloat());
            response.getSoapySdrInputReport()->getBandwidthsRanges()->back()->setMin(itBandwidth.minimum());
            response.getSoapySdrInputReport()->getBandwidthsRanges()->back()->setMax(itBandwidth.maximum());
        }
    }
}

QVariant SoapySDRInput::webapiVariantFromArgValue(SWGSDRangel::SWGArgValue *argValue)
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

void SoapySDRInput::webapiFormatArgValue(const QVariant& v, SWGSDRangel::SWGArgValue *argValue)
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

void SoapySDRInput::webapiFormatArgInfo(const SoapySDR::ArgInfo& arg, SWGSDRangel::SWGArgInfo *argInfo)
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

void SoapySDRInput::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const SoapySDRInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // Single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SoapySDR"));
    swgDeviceSettings->setSoapySdrInputSettings(new SWGSDRangel::SWGSoapySDRInputSettings());
    swgDeviceSettings->getSoapySdrInputSettings()->init();
    SWGSDRangel::SWGSoapySDRInputSettings *swgSoapySDRInputSettings = swgDeviceSettings->getSoapySdrInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgSoapySDRInputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgSoapySDRInputSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgSoapySDRInputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("bandwidth") || force) {
        swgSoapySDRInputSettings->setBandwidth(settings.m_bandwidth);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgSoapySDRInputSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgSoapySDRInputSettings->setIqOrder(settings.m_iqOrder);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgSoapySDRInputSettings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("softDCCorrection") || force) {
        swgSoapySDRInputSettings->setSoftDcCorrection(settings.m_softDCCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("softIQCorrection") || force) {
        swgSoapySDRInputSettings->setSoftIqCorrection(settings.m_softIQCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgSoapySDRInputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgSoapySDRInputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("antenna") || force) {
        swgSoapySDRInputSettings->setAntenna(new QString(settings.m_antenna));
    }
    if (deviceSettingsKeys.contains("globalGain") || force) {
        swgSoapySDRInputSettings->setGlobalGain(settings.m_globalGain);
    }
    if (deviceSettingsKeys.contains("autoGain") || force) {
        swgSoapySDRInputSettings->setAutoGain(settings.m_autoGain ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("autoDCCorrection") || force) {
        swgSoapySDRInputSettings->setAutoDcCorrection(settings.m_autoDCCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("autoIQCorrection") || force) {
        swgSoapySDRInputSettings->setAutoIqCorrection(settings.m_autoIQCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dcCorrection") || force)
    {
        swgSoapySDRInputSettings->setDcCorrection(new SWGSDRangel::SWGComplex());
        swgSoapySDRInputSettings->getDcCorrection()->setReal(settings.m_dcCorrection.real());
        swgSoapySDRInputSettings->getDcCorrection()->setImag(settings.m_dcCorrection.imag());
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force)
    {
        swgSoapySDRInputSettings->setIqCorrection(new SWGSDRangel::SWGComplex());
        swgSoapySDRInputSettings->getIqCorrection()->setReal(settings.m_iqCorrection.real());
        swgSoapySDRInputSettings->getIqCorrection()->setImag(settings.m_iqCorrection.imag());
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

void SoapySDRInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // Single Tx
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

void SoapySDRInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SoapySDRInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SoapySDRInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
