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

#include <QDebug>

#include "util/simpleserializer.h"

#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "dsp/dspcommands.h"
#include "dsp/filerecord.h"
#include "dsp/dspengine.h"
#include "soapysdr/devicesoapysdr.h"

#include "soapysdrinputthread.h"
#include "soapysdrinput.h"

MESSAGE_CLASS_DEFINITION(SoapySDRInput::MsgConfigureSoapySDRInput, Message)
MESSAGE_CLASS_DEFINITION(SoapySDRInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(SoapySDRInput::MsgStartStop, Message)

SoapySDRInput::SoapySDRInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_deviceDescription("SoapySDRInput"),
    m_running(false),
    m_thread(0)
{
    openDevice();

    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);
}

SoapySDRInput::~SoapySDRInput()
{
    if (m_running) {
        stop();
    }

    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;

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

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sourceBuddy->getBuddySharedPtr();

        if (deviceSoapySDRShared == 0)
        {
            qCritical("SoapySDRInput::openDevice: the source buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (device == 0)
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

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceSoapySDRShared *deviceSoapySDRShared = (DeviceSoapySDRShared*) sinkBuddy->getBuddySharedPtr();

        if (deviceSoapySDRShared == 0)
        {
            qCritical("SoapySDRInput::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        SoapySDR::Device *device = deviceSoapySDRShared->m_device;

        if (device == 0)
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
        m_deviceShared.m_device = deviceSoapySDR.openSoapySDR(m_deviceAPI->getSampleSourceSequence());

        if (!m_deviceShared.m_device)
        {
            qCritical("BladeRF2Input::openDevice: cannot open BladeRF2 device");
            return false;
        }

        m_deviceShared.m_deviceParams = new DeviceSoapySDRParams(m_deviceShared.m_device);
    }

    m_deviceShared.m_channel = m_deviceAPI->getItemIndex(); // publicly allocate channel
    m_deviceShared.m_source = this;
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void SoapySDRInput::closeDevice()
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
    m_deviceShared.m_source = 0;

    // No buddies so effectively close the device and delete parameters

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
        DeviceSoapySDR& deviceSoapySDR = DeviceSoapySDR::instance();
        deviceSoapySDR.closeSoapySdr(m_deviceShared.m_device);
        m_deviceShared.m_device = 0;
    }
}

void SoapySDRInput::getFrequencyRange(uint64_t& min, uint64_t& max)
{
    const DeviceSoapySDRParams::ChannelSettings* channelSettings = m_deviceShared.m_deviceParams->getRxChannelSettings(m_deviceShared.m_channel);

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

void SoapySDRInput::init()
{
    applySettings(m_settings, true);
}

SoapySDRInputThread *SoapySDRInput::findThread()
{
    if (m_thread == 0) // this does not own the thread
    {
        SoapySDRInputThread *soapySDRInputThread = 0;

        // find a buddy that has allocated the thread
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

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
    const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

    for (; it != sourceBuddies.end(); ++it)
    {
        SoapySDRInput *buddySource = ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_source;

        if (buddySource)
        {
            buddySource->setThread(m_thread);
            m_thread = 0;  // zero for others
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

    if (!m_deviceShared.m_device)
    {
        qDebug("SoapySDRInput::start: no device object");
        return false;
    }

    int requestedChannel = m_deviceAPI->getItemIndex();
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
            const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
            std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

            for (; it != sourceBuddies.end(); ++it) {
                ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
            }

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
        qDebug("SoapySDRInput::start: (re)sart buddy thread");
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

    int requestedChannel = m_deviceAPI->getItemIndex();
    SoapySDRInputThread *soapySDRInputThread = findThread();

    if (soapySDRInputThread == 0) { // no thread allocated
        return;
    }

    int nbOriginalChannels = soapySDRInputThread->getNbChannels();

    if (nbOriginalChannels == 1) // SI mode => just stop and delete the thread
    {
        qDebug("SoapySDRInput::stop: SI mode. Just stop and delete the thread");
        soapySDRInputThread->stopWork();
        delete soapySDRInputThread;
        m_thread = 0;

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

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
        m_thread = 0;

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
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it) {
            ((DeviceSoapySDRShared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
        }

        if (highestActiveChannelIndex >= 0)
        {
            qDebug("SoapySDRInput::stop: restarting the thread");
            soapySDRInputThread->startWork();
        }
    }
    else // remove channel from existing thread
    {
        qDebug("SoapySDRInput::stop: MI mode. Not changing MI configuration. Just remove FIFO reference");
        soapySDRInputThread->setFifo(requestedChannel, 0); // remove FIFO
    }

    m_running = false;
}

QByteArray SoapySDRInput::serialize() const
{
    SimpleSerializer s(1);
    return s.final();
}

bool SoapySDRInput::deserialize(const QByteArray& data __attribute__((unused)))
{
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

void SoapySDRInput::setCenterFrequency(qint64 centerFrequency __attribute__((unused)))
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
    freq_hz += df;

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

bool SoapySDRInput::handleMessage(const Message& message __attribute__((unused)))
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
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "SoapySDRInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop())
        {
            if (m_settings.m_fileRecordName.size() != 0) {
                m_fileSink->setFileName(m_settings.m_fileRecordName);
            } else {
                m_fileSink->genUniqueFileName(m_deviceAPI->getDeviceUID());
            }

            m_fileSink->startRecording();
        }
        else
        {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SoapySDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (DeviceSoapySDRShared::MsgReportBuddyChange::match(message))
    {
        int requestedChannel = m_deviceAPI->getItemIndex();
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
    else
    {
        return false;
    }
}

bool SoapySDRInput::applySettings(const SoapySDRInputSettings& settings, bool force)
{
    bool forwardChangeOwnDSP = false;
    bool forwardChangeToBuddies  = false;

    SoapySDR::Device *dev = m_deviceShared.m_device;
    SoapySDRInputThread *inputThread = findThread();
    int requestedChannel = m_deviceAPI->getItemIndex();
    qint64 xlatedDeviceCenterFrequency = settings.m_centerFrequency;
    xlatedDeviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
    xlatedDeviceCenterFrequency = xlatedDeviceCenterFrequency < 0 ? 0 : xlatedDeviceCenterFrequency;

    if ((m_settings.m_dcBlock != settings.m_dcBlock) ||
        (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
    {
        forwardChangeOwnDSP = true;
        forwardChangeToBuddies = true;

        if (dev != 0)
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
        if (inputThread != 0)
        {
            inputThread->setFcPos(requestedChannel, (int) settings.m_fcPos);
            qDebug() << "SoapySDRInput::applySettings: set fc pos (enum) to " << (int) settings.m_fcPos;
        }
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        forwardChangeOwnDSP = true;
        SoapySDRInputThread *inputThread = findThread();

        if (inputThread != 0)
        {
            inputThread->setLog2Decimation(requestedChannel, settings.m_log2Decim);
            qDebug() << "SoapySDRInput::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
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
                settings.m_devSampleRate);

        forwardChangeOwnDSP = true;
        forwardChangeToBuddies = true;

        if (dev != 0) {
            setDeviceCenterFrequency(dev, requestedChannel, deviceCenterFrequency, settings.m_LOppmTenths);
        }
    }

    if ((m_settings.m_antenna != settings.m_antenna) || force)
    {
        if (dev != 0)
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
        forwardChangeToBuddies = true;

        if (dev != 0)
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

    if (forwardChangeOwnDSP)
    {
        int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (forwardChangeToBuddies)
    {
        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();

        for (const auto &itSource : sourceBuddies)
        {
            DeviceSoapySDRShared::MsgReportBuddyChange *report = DeviceSoapySDRShared::MsgReportBuddyChange::create(
                    settings.m_centerFrequency,
                    settings.m_LOppmTenths,
                    (int) settings.m_fcPos,
                    settings.m_devSampleRate,
                    true);
            itSource->getSampleSourceInputMessageQueue()->push(report);
        }

        for (const auto &itSink : sinkBuddies)
        {
            DeviceSoapySDRShared::MsgReportBuddyChange *report = DeviceSoapySDRShared::MsgReportBuddyChange::create(
                    settings.m_centerFrequency,
                    settings.m_LOppmTenths,
                    (int) settings.m_fcPos,
                    settings.m_devSampleRate,
                    true);
            itSink->getSampleSinkInputMessageQueue()->push(report);
        }
    }

    m_settings = settings;

    qDebug() << "SoapySDRInput::applySettings: "
            << " m_transverterMode: " << m_settings.m_transverterMode
            << " m_transverterDeltaFrequency: " << m_settings.m_transverterDeltaFrequency
            << " m_centerFrequency: " << m_settings.m_centerFrequency << " Hz"
            << " m_LOppmTenths: " << m_settings.m_LOppmTenths
            << " m_log2Decim: " << m_settings.m_log2Decim
            << " m_fcPos: " << m_settings.m_fcPos
            << " m_devSampleRate: " << m_settings.m_devSampleRate
            << " m_dcBlock: " << m_settings.m_dcBlock
            << " m_iqCorrection: " << m_settings.m_iqCorrection
            << " m_antenna: " << m_settings.m_antenna
            << " m_bandwidth: " << m_settings.m_bandwidth;

    return true;
}
