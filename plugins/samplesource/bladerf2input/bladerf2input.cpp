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

#include "SWGDeviceSettings.h"
#include "SWGBladeRF2InputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGBladeRF2InputReport.h"

#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "dsp/dspcommands.h"
#include "dsp/filerecord.h"
#include "dsp/dspengine.h"

#include "bladerf2/devicebladerf2shared.h"
#include "bladerf2/devicebladerf2.h"
#include "bladerf2inputthread.h"
#include "bladerf2input.h"


MESSAGE_CLASS_DEFINITION(BladeRF2Input::MsgConfigureBladeRF2, Message)
MESSAGE_CLASS_DEFINITION(BladeRF2Input::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(BladeRF2Input::MsgStartStop, Message)

BladeRF2Input::BladeRF2Input(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_deviceDescription("BladeRF2Input"),
    m_running(false),
    m_thread(0)
{
    openDevice();

    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);
}

BladeRF2Input::~BladeRF2Input()
{
    if (m_running) {
        stop();
    }

    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
}

void BladeRF2Input::destroy()
{
    delete this;
}

bool BladeRF2Input::openDevice()
{
    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("BladeRF2Input::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("BladeRF2Input::openDevice: allocated SampleFifo");
    }

    // look for Rx buddies and get reference to the device object
    // if there is a channel left take the first available
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        qDebug("BladeRF2Input::openDevice: look in Rx buddies");

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceBladeRF2Shared *deviceBladeRF2Shared = (DeviceBladeRF2Shared*) sourceBuddy->getBuddySharedPtr();

        if (deviceBladeRF2Shared == 0)
        {
            qCritical("BladeRF2Input::openDevice: the source buddy shared pointer is null");
            return false;
        }

        DeviceBladeRF2 *device = deviceBladeRF2Shared->m_dev;

        if (device == 0)
        {
            qCritical("BladeRF2Input::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_dev = device;
        int requestedChannel = m_deviceAPI->getItemIndex();

        if (requestedChannel == deviceBladeRF2Shared->m_channel)
        {
            qCritical("BladeRF2Input::openDevice: channel %u already in use", requestedChannel);
            return false;
        }

        if (!device->openRx(requestedChannel))
        {
            qCritical("BladeRF2Input::openDevice: channel %u cannot be enabled", requestedChannel);
            return false;
        }
        else
        {
            m_deviceShared.m_channel = requestedChannel;
            qDebug("BladeRF2Input::openDevice: channel %u enabled", requestedChannel);
        }
    }
    // look for Tx buddies and get reference to the device object
    // allocate the Rx channel unconditionally
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("BladeRF2Input::openDevice: look in Tx buddies");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceBladeRF2Shared *deviceBladeRF2Shared = (DeviceBladeRF2Shared*) sinkBuddy->getBuddySharedPtr();

        if (deviceBladeRF2Shared == 0)
        {
            qCritical("BladeRF2Input::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        DeviceBladeRF2 *device = deviceBladeRF2Shared->m_dev;

        if (device == 0)
        {
            qCritical("BladeRF2Input::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_dev = device;
        int requestedChannel = m_deviceAPI->getItemIndex();

        if (!device->openRx(requestedChannel))
        {
            qCritical("BladeRF2Input::openDevice: channel %u cannot be enabled", requestedChannel);
            return false;
        }
        else
        {
            m_deviceShared.m_channel = requestedChannel;
            qDebug("BladeRF2Input::openDevice: channel %u enabled", requestedChannel);
        }
    }
    // There are no buddies then create the first BladeRF2 device
    // allocate the Rx channel unconditionally
    else
    {
        qDebug("BladeRF2Input::openDevice: open device here");

        m_deviceShared.m_dev = new DeviceBladeRF2();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSampleSourceSerial()));

        if (!m_deviceShared.m_dev->open(serial))
        {
            qCritical("BladeRF2Input::openDevice: cannot open BladeRF2 device");
            return false;
        }

        int requestedChannel = m_deviceAPI->getItemIndex();

        if (!m_deviceShared.m_dev->openRx(requestedChannel))
        {
            qCritical("BladeRF2Input::openDevice: channel %u cannot be enabled", requestedChannel);
            return false;
        }
        else
        {
            m_deviceShared.m_channel = requestedChannel;
            qDebug("BladeRF2Input::openDevice: channel %u enabled", requestedChannel);
        }
    }

    m_deviceShared.m_source = this;
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void BladeRF2Input::closeDevice()
{
    if (m_deviceShared.m_dev == 0) { // was never open
        return;
    }

    if (m_running) {
        stop();
    }

    if (m_thread) { // stills own the thread => transfer to a buddy
        moveThreadToBuddy();
    }

    m_deviceShared.m_channel = -1;
    m_deviceShared.m_source = 0;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceShared.m_dev->close();
        delete m_deviceShared.m_dev;
        m_deviceShared.m_dev = 0;
    }
}

void BladeRF2Input::init()
{
    applySettings(m_settings, true);
}

BladeRF2InputThread *BladeRF2Input::findThread()
{
    if (m_thread == 0) // this does not own the thread
    {
        BladeRF2InputThread *bladerf2InputThread = 0;

        // find a buddy that has allocated the thread
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it)
        {
            BladeRF2Input *buddySource = ((DeviceBladeRF2Shared*) (*it)->getBuddySharedPtr())->m_source;

            if (buddySource)
            {
                bladerf2InputThread = buddySource->getThread();

                if (bladerf2InputThread) {
                    break;
                }
            }
        }

        return bladerf2InputThread;
    }
    else
    {
        return m_thread; // own thread
    }
}

void BladeRF2Input::moveThreadToBuddy()
{
    const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

    for (; it != sourceBuddies.end(); ++it)
    {
        BladeRF2Input *buddySource = ((DeviceBladeRF2Shared*) (*it)->getBuddySharedPtr())->m_source;

        if (buddySource)
        {
            buddySource->setThread(m_thread);
            m_thread = 0;  // zero for others
        }
    }
}

bool BladeRF2Input::start()
{
    if (!m_deviceShared.m_dev)
    {
        qDebug("BladerfInput::start: no device object");
        return false;
    }

    BladeRF2InputThread *bladerf2InputThread = findThread();
    bool needsStart = false;

    if (bladerf2InputThread) // if thread is already allocated
    {
        int nbOriginalChannels = bladerf2InputThread->getNbChannels();

        if (m_deviceShared.m_channel+1 > nbOriginalChannels) // expansion by deleting and re-creating the thread
        {
            SampleSinkFifo **fifos = new SampleSinkFifo*[nbOriginalChannels];
            unsigned int *log2Decims = new unsigned int[nbOriginalChannels];
            int *fcPoss = new int[nbOriginalChannels];

            for (int i = 0; i < nbOriginalChannels; i++) // save original FIFO references and data
            {
                fifos[i] = bladerf2InputThread->getFifo(i);
                log2Decims[i] = bladerf2InputThread->getLog2Decimation(i);
                fcPoss[i] = bladerf2InputThread->getFcPos(i);
            }

            bladerf2InputThread->stopWork();
            delete bladerf2InputThread;
            bladerf2InputThread = new BladeRF2InputThread(m_deviceShared.m_dev->getDev(), m_deviceShared.m_channel+1);
            m_thread = bladerf2InputThread; // take ownership

            for (int i = 0; i < nbOriginalChannels; i++) // restore original FIFO references
            {
                bladerf2InputThread->setFifo(i, fifos[i]);
                bladerf2InputThread->setLog2Decimation(i, log2Decims[i]);
                bladerf2InputThread->setFcPos(i, fcPoss[i]);
            }

            // remove old thread address from buddies (reset in all buddies)
            const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
            std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

            for (; it != sourceBuddies.end(); ++it) {
                ((DeviceBladeRF2Shared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
            }

            needsStart = true;
        }
    }
    else // first allocation
    {
        bladerf2InputThread = new BladeRF2InputThread(m_deviceShared.m_dev->getDev(), m_deviceShared.m_channel+1);
        m_thread = bladerf2InputThread; // take ownership
        needsStart = true;
    }

    bladerf2InputThread->setFifo(m_deviceShared.m_channel, &m_sampleFifo);
    bladerf2InputThread->setLog2Decimation(m_deviceShared.m_channel, m_settings.m_log2Decim);
    bladerf2InputThread->setFcPos(m_deviceShared.m_channel, (int) m_settings.m_fcPos);

    if (needsStart) {
        bladerf2InputThread->startWork();
    }

    applySettings(m_settings, true);

    qDebug("BladeRF2Input::start: started");
    m_running = true;

    return true;
}

void BladeRF2Input::stop()
{
    if (!m_running) {
        return;
    }

    BladeRF2InputThread *bladerf2InputThread = findThread();

    if (bladerf2InputThread == 0) // no thread allocated
    {
        return;
    }

    int nbOriginalChannels = bladerf2InputThread->getNbChannels();

    if (nbOriginalChannels == 1) // SI mode => just stop and delete the thread
    {
        bladerf2InputThread->stopWork();
        delete bladerf2InputThread;
        m_thread = 0;

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it) {
            ((DeviceBladeRF2Shared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
        }
    }
    else if (m_deviceShared.m_channel == nbOriginalChannels - 1) // remove last MI channel => reduce by deleting and re-creating the thread
    {
        bladerf2InputThread->stopWork();
        SampleSinkFifo **fifos = new SampleSinkFifo*[nbOriginalChannels-1];
        unsigned int *log2Decims = new unsigned int[nbOriginalChannels-1];
        int *fcPoss = new int[nbOriginalChannels-1];

        for (int i = 0; i < nbOriginalChannels-1; i++) // save original FIFO references
        {
            fifos[i] = bladerf2InputThread->getFifo(i);
            log2Decims[i] = bladerf2InputThread->getLog2Decimation(i);
            fcPoss[i] = bladerf2InputThread->getFcPos(i);
        }

        delete bladerf2InputThread;
        bladerf2InputThread = new BladeRF2InputThread(m_deviceShared.m_dev->getDev(), nbOriginalChannels-1);
        m_thread = bladerf2InputThread; // take ownership

        for (int i = 0; i < nbOriginalChannels-1; i++)  // restore original FIFO references
        {
            bladerf2InputThread->setFifo(i, fifos[i]);
            bladerf2InputThread->setLog2Decimation(i, log2Decims[i]);
            bladerf2InputThread->setFcPos(i, fcPoss[i]);
        }

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator it = sourceBuddies.begin();

        for (; it != sourceBuddies.end(); ++it) {
            ((DeviceBladeRF2Shared*) (*it)->getBuddySharedPtr())->m_source->setThread(0);
        }

        bladerf2InputThread->startWork();
    }
    else // remove channel from existing thread
    {
        bladerf2InputThread->setFifo(m_deviceShared.m_channel, 0); // remove FIFO
    }

    m_running = false;
}

QByteArray BladeRF2Input::serialize() const
{
    return m_settings.serialize();
}

bool BladeRF2Input::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureBladeRF2* message = MsgConfigureBladeRF2::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladeRF2* messageToGUI = MsgConfigureBladeRF2::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& BladeRF2Input::getDeviceDescription() const
{
    return m_deviceDescription;
}

int BladeRF2Input::getSampleRate() const
{
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2Decim));
}

quint64 BladeRF2Input::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void BladeRF2Input::setCenterFrequency(qint64 centerFrequency)
{
    BladeRF2InputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureBladeRF2* message = MsgConfigureBladeRF2::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladeRF2* messageToGUI = MsgConfigureBladeRF2::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

void BladeRF2Input::getFrequencyRange(uint64_t& min, uint64_t& max, int& step)
{
    if (m_deviceShared.m_dev) {
        m_deviceShared.m_dev->getFrequencyRangeRx(min, max, step);
    }
}

void BladeRF2Input::getSampleRateRange(int& min, int& max, int& step)
{
    if (m_deviceShared.m_dev) {
        m_deviceShared.m_dev->getSampleRateRangeRx(min, max, step);
    }
}

void BladeRF2Input::getBandwidthRange(int& min, int& max, int& step)
{
    if (m_deviceShared.m_dev) {
        m_deviceShared.m_dev->getBandwidthRangeRx(min, max, step);
    }
}

void BladeRF2Input::getGlobalGainRange(int& min, int& max, int& step)
{
    if (m_deviceShared.m_dev) {
        m_deviceShared.m_dev->getGlobalGainRangeRx(min, max, step);
    }
}

bool BladeRF2Input::handleMessage(const Message& message)
{
    if (MsgConfigureBladeRF2::match(message))
    {
        MsgConfigureBladeRF2& conf = (MsgConfigureBladeRF2&) message;
        qDebug() << "BladeRF2Input::handleMessage: MsgConfigureBladeRF2";

        if (!applySettings(conf.getSettings(), conf.getForce()))
        {
            qDebug("BladeRF2Input::handleMessage: MsgConfigureBladeRF2 config error");
        }

        return true;
    }
    else if (DeviceBladeRF2Shared::MsgReportBuddyChange::match(message))
    {
        DeviceBladeRF2Shared::MsgReportBuddyChange& report = (DeviceBladeRF2Shared::MsgReportBuddyChange&) message;
        struct bladerf *dev = m_deviceShared.m_dev->getDev();
        BladeRF2InputSettings settings = m_settings;
        int status;
        int tmp_int;
        unsigned int tmp_uint;
        uint64_t tmp_uint64;
        bool tmp_bool;
        bladerf_gain_mode tmp_gain_mode;

        // evaluate changes that may have been introduced by changes in a buddy

        if (dev) // The BladeRF device must have been open to do so
        {
            if (report.getRxElseTx()) // Rx buddy change: check for: frequency, gain mode and value, bias tee, sample rate, bandwidth
            {
                status = bladerf_get_sample_rate(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), &tmp_uint);

                if (status < 0) {
                    qCritical("BladeRF2Input::handleMessage: MsgReportBuddyChange: bladerf_get_sample_rate error: %s", bladerf_strerror(status));
                } else {
                    settings.m_devSampleRate = tmp_uint;
                }

                status = bladerf_get_frequency(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), &tmp_uint64);

                if (status < 0) {
                    qCritical("BladeRF2Input::handleMessage: MsgReportBuddyChange: bladerf_get_frequency error: %s", bladerf_strerror(status));
                } else {
                    settings.m_centerFrequency = tmp_uint64;
                }

                status = bladerf_get_bandwidth(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), &tmp_uint);

                if (status < 0) {
                    qCritical("BladeRF2Input::handleMessage: MsgReportBuddyChange: bladerf_get_bandwidth error: %s", bladerf_strerror(status));
                } else {
                    settings.m_bandwidth = tmp_uint;
                }

                status = bladerf_get_gain_mode(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), &tmp_gain_mode);

                if (status < 0) {
                    qCritical("BladeRF2Input::handleMessage: MsgReportBuddyChange: bladerf_get_gain_mode error: %s", bladerf_strerror(status));
                } else {
                    settings.m_gainMode = (int) tmp_gain_mode;
                }

                status = bladerf_get_gain(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), &tmp_int);

                if (status < 0) {
                    qCritical("BladeRF2Input::handleMessage: MsgReportBuddyChange: bladerf_get_gain error: %s", bladerf_strerror(status));
                } else {
                    settings.m_globalGain = tmp_int;
                }

                status = bladerf_get_bias_tee(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), &tmp_bool);

                if (status < 0) {
                    qCritical("BladeRF2Input::handleMessage: MsgReportBuddyChange: bladerf_get_bias_tee error: %s", bladerf_strerror(status));
                } else {
                    settings.m_biasTee = tmp_bool;
                }
            }
            else // Tx buddy change: check for sample rate change only
            {
                status = bladerf_get_sample_rate(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), &tmp_uint);

                if (status < 0) {
                    qCritical("BladeRF2Input::handleMessage: MsgReportBuddyChange: bladerf_get_sample_rate error: %s", bladerf_strerror(status));
                } else {
                    settings.m_devSampleRate = tmp_uint;
                }
            }

            // change DSP settings if buddy change introduced a change in center frequency or base rate
            if ((settings.m_centerFrequency != m_settings.m_centerFrequency) || (settings.m_devSampleRate != m_settings.m_devSampleRate))
            {
                int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Decim);
                DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
                m_fileSink->handleMessage(*notif); // forward to file sink
                m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
            }

            m_settings = settings; // acknowledge the new settings

            // propagate settings to GUI if any
            if (getMessageQueueToGUI())
            {
                MsgConfigureBladeRF2 *reportToGUI = MsgConfigureBladeRF2::create(m_settings, false);
                getMessageQueueToGUI()->push(reportToGUI);
            }
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "BladeRF2Input::handleMessage: MsgFileRecord: " << conf.getStartStop();

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
        qDebug() << "BladeRF2Input::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

bool BladeRF2Input::applySettings(const BladeRF2InputSettings& settings, bool force)
{
    bool forwardChangeOwnDSP = false;
    bool forwardChangeRxBuddies  = false;
    bool forwardChangeTxBuddies = false;

    struct bladerf *dev = m_deviceShared.m_dev->getDev();
    qDebug() << "BladeRF2Input::applySettings: m_dev: " << dev;

    if ((m_settings.m_dcBlock != settings.m_dcBlock) ||
        (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
    {
        forwardChangeOwnDSP = true;
        forwardChangeRxBuddies = true;
        forwardChangeTxBuddies = true;

        if (dev != 0)
        {
            unsigned int actualSamplerate;
            int status = bladerf_set_sample_rate(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), settings.m_devSampleRate, &actualSamplerate);

            if (status < 0)
            {
                qCritical("BladeRF2Input::applySettings: could not set sample rate: %d: %s",
                        settings.m_devSampleRate, bladerf_strerror(status));
            }
            else
            {
                qDebug() << "BladeRF2Input::applySettings: bladerf_set_sample_rate(BLADERF_MODULE_RX) actual sample rate is " << actualSamplerate;
            }
        }
    }

    if ((m_settings.m_bandwidth != settings.m_bandwidth) || force)
    {
        forwardChangeRxBuddies = true;

        if (dev != 0)
        {
            unsigned int actualBandwidth;
            int status = bladerf_set_bandwidth(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), settings.m_bandwidth, &actualBandwidth);

            if(status < 0)
            {
                qCritical("BladeRF2Input::applySettings: could not set bandwidth: %d: %s",
                        settings.m_bandwidth, bladerf_strerror(status));
            }
            else
            {
                qDebug() << "BladeRF2Input::applySettings: bladerf_set_bandwidth(BLADERF_MODULE_RX) actual bandwidth is " << actualBandwidth;
            }
        }
    }

    if ((m_settings.m_fcPos != settings.m_fcPos) || force)
    {
        BladeRF2InputThread *inputThread = findThread();

        if (inputThread != 0)
        {
            inputThread->setFcPos(m_deviceShared.m_channel, (int) settings.m_fcPos);
            qDebug() << "BladeRF2Input::applySettings: set fc pos (enum) to " << (int) settings.m_fcPos;
        }
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        forwardChangeOwnDSP = true;
        BladeRF2InputThread *inputThread = findThread();

        if (inputThread != 0)
        {
            inputThread->setLog2Decimation(m_deviceShared.m_channel, settings.m_log2Decim);
            qDebug() << "BladeRF2Input::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_devSampleRate != settings.m_devSampleRate)
        || (m_settings.m_fcPos != settings.m_fcPos)
        || (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                0,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate);

        forwardChangeOwnDSP = true;
        forwardChangeRxBuddies = true;

        if (dev != 0)
        {
            int status = bladerf_set_frequency(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), deviceCenterFrequency);

            if (status < 0) {
                qWarning("BladeRF2Input::applySettings: bladerf_set_frequency(%lld) failed: %s",
                        settings.m_centerFrequency, bladerf_strerror(status));
            } else {
                qDebug("BladeRF2Input::applySettings: bladerf_set_frequency(%lld)", settings.m_centerFrequency);
            }
        }
    }

    if ((m_settings.m_biasTee != settings.m_biasTee) || force)
    {
        forwardChangeRxBuddies = true;
        m_deviceShared.m_dev->setBiasTeeRx(settings.m_biasTee);
    }

    if ((m_settings.m_globalGain != settings.m_globalGain) || force)
    {
        forwardChangeRxBuddies = true;

        if (dev)
        {
            int status = bladerf_set_gain(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), settings.m_globalGain);

            if (status < 0) {
                qWarning("BladeRF2Input::applySettings: bladerf_set_gain(%d) failed: %s",
                        settings.m_globalGain, bladerf_strerror(status));
            } else {
                qDebug("BladeRF2Input::applySettings: bladerf_set_gain(%d)", settings.m_globalGain);
            }
        }
    }

    if ((m_settings.m_gainMode != settings.m_gainMode) || force)
    {
        forwardChangeRxBuddies = true;

        if (dev)
        {
            int status = bladerf_set_gain_mode(dev, BLADERF_CHANNEL_RX(m_deviceShared.m_channel), (bladerf_gain_mode) settings.m_gainMode);

            if (status < 0) {
                qWarning("BladeRF2Input::applySettings: bladerf_set_gain_mode(%d) failed: %s",
                        settings.m_gainMode, bladerf_strerror(status));
            } else {
                qDebug("BladeRF2Input::applySettings: bladerf_set_gain_mode(%d)", settings.m_gainMode);
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

    if (forwardChangeRxBuddies)
    {
        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceBladeRF2Shared::MsgReportBuddyChange *report = DeviceBladeRF2Shared::MsgReportBuddyChange::create(true);
            (*itSource)->getSampleSourceInputMessageQueue()->push(report);
        }
    }

    if (forwardChangeTxBuddies)
    {
        // send to sink buddies
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceBladeRF2Shared::MsgReportBuddyChange *report = DeviceBladeRF2Shared::MsgReportBuddyChange::create(true);
            (*itSink)->getSampleSinkInputMessageQueue()->push(report);
        }
    }

    m_settings = settings;

    qDebug() << "BladeRF2Input::applySettings: "
            << " m_centerFrequency: " << m_settings.m_centerFrequency << " Hz"
            << " m_bandwidth: " << m_settings.m_bandwidth
            << " m_log2Decim: " << m_settings.m_log2Decim
            << " m_fcPos: " << m_settings.m_fcPos
            << " m_devSampleRate: " << m_settings.m_devSampleRate
            << " m_globalGain: " << m_settings.m_globalGain
            << " m_gainMode: " << m_settings.m_gainMode
            << " m_dcBlock: " << m_settings.m_dcBlock
            << " m_iqCorrection: " << m_settings.m_iqCorrection
            << " m_biasTee: " << m_settings.m_biasTee;

    return true;
}

int BladeRF2Input::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setBladeRf2InputSettings(new SWGSDRangel::SWGBladeRF2InputSettings());
    response.getBladeRf2InputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int BladeRF2Input::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    BladeRF2InputSettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getBladeRf2InputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getBladeRf2InputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getBladeRf2InputSettings()->getBandwidth();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getBladeRf2InputSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        settings.m_fcPos = static_cast<BladeRF2InputSettings::fcPos_t>(response.getBladeRf2InputSettings()->getFcPos());
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getBladeRf2InputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getBladeRf2InputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("biasTee")) {
        settings.m_biasTee = response.getBladeRf2InputSettings()->getBiasTee() != 0;
    }
    if (deviceSettingsKeys.contains("gainMode")) {
        settings.m_gainMode = response.getBladeRf2InputSettings()->getGainMode();
    }
    if (deviceSettingsKeys.contains("globalGain")) {
        settings.m_globalGain = response.getBladeRf2InputSettings()->getGlobalGain();
    }
    if (deviceSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getBladeRf1InputSettings()->getFileRecordName();
    }

    MsgConfigureBladeRF2 *msg = MsgConfigureBladeRF2::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBladeRF2 *msgToGUI = MsgConfigureBladeRF2::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

int BladeRF2Input::webapiReportGet(SWGSDRangel::SWGDeviceReport& response, QString& errorMessage __attribute__((unused)))
{
    response.setBladeRf2InputReport(new SWGSDRangel::SWGBladeRF2InputReport());
    response.getBladeRf2InputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void BladeRF2Input::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const BladeRF2InputSettings& settings)
{
    response.getBladeRf2InputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getBladeRf2InputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getBladeRf2InputSettings()->setBandwidth(settings.m_bandwidth);
    response.getBladeRf2InputSettings()->setLog2Decim(settings.m_log2Decim);
    response.getBladeRf2InputSettings()->setFcPos((int) settings.m_fcPos);
    response.getBladeRf2InputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getBladeRf2InputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getBladeRf2InputSettings()->setBiasTee(settings.m_biasTee ? 1 : 0);
    response.getBladeRf2InputSettings()->setGainMode(settings.m_gainMode);
    response.getBladeRf2InputSettings()->setGlobalGain(settings.m_globalGain);

    if (response.getBladeRf2InputSettings()->getFileRecordName()) {
        *response.getBladeRf2InputSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getBladeRf2InputSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }
}

void BladeRF2Input::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    DeviceBladeRF2 *device = m_deviceShared.m_dev;

    if (device)
    {
        int min, max, step;
        uint64_t f_min, f_max;

        device->getBandwidthRangeRx(min, max, step);

        response.getBladeRf2InputReport()->setBandwidthRange(new SWGSDRangel::SWGRange);
        response.getBladeRf2InputReport()->getBandwidthRange()->setMin(min);
        response.getBladeRf2InputReport()->getBandwidthRange()->setMax(max);
        response.getBladeRf2InputReport()->getBandwidthRange()->setStep(step);

        device->getFrequencyRangeRx(f_min, f_max, step);

        response.getBladeRf2InputReport()->setFrequencyRange(new SWGSDRangel::SWGRange);
        response.getBladeRf2InputReport()->getFrequencyRange()->setMin(f_min);
        response.getBladeRf2InputReport()->getFrequencyRange()->setMax(f_max);
        response.getBladeRf2InputReport()->getFrequencyRange()->setStep(step);

        device->getGlobalGainRangeRx(min, max, step);

        response.getBladeRf2InputReport()->setGlobalGainRange(new SWGSDRangel::SWGRange);
        response.getBladeRf2InputReport()->getGlobalGainRange()->setMin(min);
        response.getBladeRf2InputReport()->getGlobalGainRange()->setMax(max);
        response.getBladeRf2InputReport()->getGlobalGainRange()->setStep(step);

        device->getSampleRateRangeRx(min, max, step);

        response.getBladeRf2InputReport()->setSampleRateRange(new SWGSDRangel::SWGRange);
        response.getBladeRf2InputReport()->getSampleRateRange()->setMin(min);
        response.getBladeRf2InputReport()->getSampleRateRange()->setMax(max);
        response.getBladeRf2InputReport()->getSampleRateRange()->setStep(step);
    }
}

int BladeRF2Input::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int BladeRF2Input::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
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
