///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/devicesinkapi.h"
#include "device/devicesourceapi.h"
#include "soapysdr/devicesoapysdr.h"

#include "soapysdroutputthread.h"
#include "soapysdroutput.h"

MESSAGE_CLASS_DEFINITION(SoapySDROutput::MsgConfigureSoapySDROutput, Message)
MESSAGE_CLASS_DEFINITION(SoapySDROutput::MsgStartStop, Message)

SoapySDROutput::SoapySDROutput(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_deviceDescription("SoapySDROutput"),
    m_running(false),
    m_thread(0)
{
    openDevice();
}

SoapySDROutput::~SoapySDROutput()
{
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
    m_sampleSourceFifo.resize(m_settings.m_devSampleRate/(1<<(m_settings.m_log2Interp <= 4 ? m_settings.m_log2Interp : 4)));

    // look for Tx buddies and get reference to the device object
    if (m_deviceAPI->getSinkBuddies().size() > 0) // look sink sibling first
    {
        qDebug("SoapySDROutput::openDevice: look in Tx buddies");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sinkBuddy->getBuddySharedPtr();

        if (deviceSoapySDRShared == 0)
        {
            qCritical("SoapySDROutput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (device == 0)
        {
            qCritical("SoapySDROutput::openDevice: cannot get device pointer from Tx buddy");
            return false;
        }

        m_deviceShared.m_device = device;
    }
    // look for Rx buddies and get reference to the device object
    else if (m_deviceAPI->getSourceBuddies().size() > 0) // then source
    {
        qDebug("SoapySDROutput::openDevice: look in Rx buddies");

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sourceBuddy->getBuddySharedPtr();

        if (deviceSoapySDRShared == 0)
        {
            qCritical("SoapySDROutput::openDevice: the source buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (device == 0)
        {
            qCritical("SoapySDROutput::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_device = device;
    }
    // There are no buddies then create the first BladeRF2 device
    else
    {
        qDebug("SoapySDROutput::openDevice: open device here");
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        m_deviceShared.m_device = deviceSoapySDR.openSoapySDR(m_deviceAPI->getSampleSinkSequence());

        if (!m_deviceShared.m_device)
        {
            qCritical("SoapySDROutput::openDevice: cannot open SoapySDR device");
            return false;
        }
    }

    m_deviceShared.m_channel = m_deviceAPI->getItemIndex(); // publicly allocate channel
    m_deviceShared.m_sink = this;
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void SoapySDROutput::closeDevice()
{
    if (m_deviceShared.m_device == 0) { // was never open
        return;
    }

    if (m_running) {
        stop();
    }

    if (m_thread) { // stills own the thread => transfer to a buddy
        moveThreadToBuddy();
    }

    m_deviceShared.m_channel = -1; // publicly release channel
    m_deviceShared.m_sink = 0;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        deviceSoapySDR.closeSoapySdr(m_deviceShared.m_device);
        m_deviceShared.m_device = 0;
    }
}

void SoapySDROutput::getFrequencyRange(uint64_t& min, uint64_t& max)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);

    if (channelSettings && (channelSettings->m_frequencySettings.size() > 0))
    {
        DeviceSoapySDRParams::FrequencySetting freqSettings = channelSettings->m_frequencySettings[0];
        SoapySDR::RangeList rangeList = freqSettings.m_ranges;

        if (rangeList.size() > 0) // TODO: handle multiple ranges
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

const SoapySDR::RangeList& SoapySDROutput::getRateRanges()
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getTxChannelSettings(m_deviceShared.m_channel);
    return channelSettings->m_ratesRanges;
}

void SoapySDROutput::init()
{
    applySettings(m_settings, true);
}

SoapySDROutputThread *SoapySDROutput::findThread()
{
    if (m_thread == 0) // this does not own the thread
    {
        SoapySDROutputThread *soapySDROutputThread = 0;

        // find a buddy that has allocated the thread
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator it = sinkBuddies.begin();

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
    const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceSinkAPI*>::const_iterator it = sinkBuddies.begin();

    for (; it != sinkBuddies.end(); ++it)
    {
        SoapySDROutput *buddySink = ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_sink;

        if (buddySink)
        {
            buddySink->setThread(m_thread);
            m_thread = 0;  // zero for others
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


    if (!m_deviceShared.m_device)
    {
        qDebug("SoapySDROutput::start: no device object");
        return false;
    }

    int requestedChannel = m_deviceAPI->getItemIndex();
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
            const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
            std::vector<DeviceSinkAPI*>::const_iterator it = sinkBuddies.begin();

            for (; it != sinkBuddies.end(); ++it) {
                ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_sink->setThread(0);
            }

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
        qDebug("SoapySDROutput::start: (re)sart buddy thread");
        soapySDROutputThread->startWork();
    }

    applySettings(m_settings, true); // re-apply forcibly to set sample rate with the new number of channels

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

    int requestedChannel = m_deviceAPI->getItemIndex();
    SoapySDROutputThread *soapySDROutputThread = findThread();

    if (soapySDROutputThread == 0) { // no thread allocated
        return;
    }

    int nbOriginalChannels = soapySDROutputThread->getNbChannels();

    if (nbOriginalChannels == 1) // SO mode => just stop and delete the thread
    {
        qDebug("SoapySDROutput::stop: SO mode. Just stop and delete the thread");
        soapySDROutputThread->stopWork();
        delete soapySDROutputThread;
        m_thread = 0;

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator it = sinkBuddies.begin();

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

            if ((soapySDROutputThread->getFifo(i) != 0) && (i > highestActiveChannelIndex)) {
                highestActiveChannelIndex = i;
            }

            log2Interps[i] = soapySDROutputThread->getLog2Interpolation(i);
        }

        delete soapySDROutputThread;
        m_thread = 0;

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
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator it = sinkBuddies.begin();

        for (; it != sinkBuddies.end(); ++it) {
            ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_sink->setThread(0);
        }

        if (highestActiveChannelIndex >= 0)
        {
            qDebug("SoapySDROutput::stop: restarting the thread");
            soapySDROutputThread->startWork();
        }
    }
    else // remove channel from existing thread
    {
        qDebug("SoapySDROutput::stop: MO mode. Not changing MO configuration. Just remove FIFO reference");
        soapySDROutputThread->setFifo(requestedChannel, 0); // remove FIFO
    }

    applySettings(m_settings, true); // re-apply forcibly to set sample rate with the new number of channels

    m_running = false;
}

QByteArray SoapySDROutput::serialize() const
{
    return m_settings.serialize();
}

bool SoapySDROutput::deserialize(const QByteArray& data __attribute__((unused)))
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

bool SoapySDROutput::handleMessage(const Message& message __attribute__((unused)))
{
    return false;
}



