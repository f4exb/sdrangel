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


#include <string.h>
#include <errno.h>

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGDeviceState.h"
#include "SWGDeviceSettings.h"
#include "SWGBladeRF2InputSettings.h"
#include "SWGDeviceReport.h"
#include "SWGBladeRF2OutputReport.h"

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"
#include "bladerf2/devicebladerf2shared.h"
#include "bladerf2/devicebladerf2.h"

#include "bladerf2outputthread.h"
#include "bladerf2output.h"

MESSAGE_CLASS_DEFINITION(BladeRF2Output::MsgConfigureBladeRF2, Message)
MESSAGE_CLASS_DEFINITION(BladeRF2Output::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(BladeRF2Output::MsgReportGainRange, Message)

BladeRF2Output::BladeRF2Output(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_dev(nullptr),
    m_thread(nullptr),
    m_deviceDescription("BladeRF2Output"),
    m_running(false)
{
    openDevice();
    m_deviceAPI->setNbSinkStreams(1);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &BladeRF2Output::networkManagerFinished
    );
}

BladeRF2Output::~BladeRF2Output()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &BladeRF2Output::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void BladeRF2Output::destroy()
{
    delete this;
}

bool BladeRF2Output::openDevice()
{
    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_settings.m_devSampleRate));

    // look for Tx buddies and get reference to the device object
    if (m_deviceAPI->getSinkBuddies().size() > 0) // look sink sibling first
    {
        qDebug("BladeRF2Output::openDevice: look in Tx buddies");

        DeviceAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceBladeRF2Shared *deviceBladeRF2Shared = (DeviceBladeRF2Shared*) sinkBuddy->getBuddySharedPtr();

        if (deviceBladeRF2Shared == 0)
        {
            qCritical("BladeRF2Output::openDevice: the sink buddy shared pointer is null");
            return false;
        }

        DeviceBladeRF2 *device = deviceBladeRF2Shared->m_dev;

        if (device == 0)
        {
            qCritical("BladeRF2Output::openDevice: cannot get device pointer from Tx buddy");
            return false;
        }

        m_deviceShared.m_dev = device;
    }
    // look for Rx buddies and get reference to the device object
    else if (m_deviceAPI->getSourceBuddies().size() > 0) // then source
    {
        qDebug("BladeRF2Output::openDevice: look in Rx buddies");

        DeviceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceBladeRF2Shared *deviceBladeRF2Shared = (DeviceBladeRF2Shared*) sourceBuddy->getBuddySharedPtr();

        if (deviceBladeRF2Shared == 0)
        {
            qCritical("BladeRF2Output::openDevice: the source buddy shared pointer is null");
            return false;
        }

        DeviceBladeRF2 *device = deviceBladeRF2Shared->m_dev;

        if (device == 0)
        {
            qCritical("BladeRF2Output::openDevice: cannot get device pointer from Rx buddy");
            return false;
        }

        m_deviceShared.m_dev = device;
    }
    // There are no buddies then create the first BladeRF2 device
    else
    {
        qDebug("BladeRF2Output::openDevice: open device here");

        m_deviceShared.m_dev = new DeviceBladeRF2();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSamplingDeviceSerial()));

        if (!m_deviceShared.m_dev->open(serial))
        {
            qCritical("BladeRF2Output::openDevice: cannot open BladeRF2 device");
            return false;
        }
    }

    m_deviceShared.m_channel = m_deviceAPI->getDeviceItemIndex(); // publicly allocate channel
    m_deviceShared.m_sink = this;
    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API
    return true;
}

void BladeRF2Output::closeDevice()
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

    m_deviceShared.m_channel = -1; // publicly release channel
    m_deviceShared.m_sink = 0;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceShared.m_dev->close();
        delete m_deviceShared.m_dev;
        m_deviceShared.m_dev = 0;
    }
}

void BladeRF2Output::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

BladeRF2OutputThread *BladeRF2Output::findThread()
{
    if (m_thread == 0) // this does not own the thread
    {
        BladeRF2OutputThread *bladeRF2OutputThread = 0;

        // find a buddy that has allocated the thread
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

        for (; it != sinkBuddies.end(); ++it)
        {
            BladeRF2Output *buddySink = ((DeviceBladeRF2Shared*) (*it)->getBuddySharedPtr())->m_sink;

            if (buddySink)
            {
                bladeRF2OutputThread = buddySink->getThread();

                if (bladeRF2OutputThread) {
                    break;
                }
            }
        }

        return bladeRF2OutputThread;
    }
    else
    {
        return m_thread; // own thread
    }
}

void BladeRF2Output::moveThreadToBuddy()
{
    const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

    for (; it != sinkBuddies.end(); ++it)
    {
        BladeRF2Output *buddySink = ((DeviceBladeRF2Shared*) (*it)->getBuddySharedPtr())->m_sink;

        if (buddySink)
        {
            buddySink->setThread(m_thread);
            m_thread = 0;  // zero for others
        }
    }
}

bool BladeRF2Output::start()
{
    // There is a single thread per physical device (Tx side). This thread is unique and referenced by a unique
    // buddy in the group of sink buddies associated with this physical device.
    //
    // This start method is responsible for managing the thread and channel enabling when the streaming of a Tx channel is started
    //
    // It checks the following conditions
    //   - the thread is allocated or not (by itself or one of its buddies). If it is it grabs the thread pointer.
    //   - the requested channel is the first (0) or the following (just 1 in BladeRF 2 case)
    //
    // The BladeRF support library lets you work in two possible modes:
    //   - Single Output (SO) with only one channel streaming. This HAS to be channel 0.
    //   - Multiple Output (MO) with two channels streaming using interleaved samples. It MUST be in this configuration if channel 1
    //     is used. When we will run with only channel 1 streaming from the client perspective the channel 0 will actually be enabled
    //     and streaming but zero samples will be sent to it.
    //
    // It manages the transition form SO where only one channel (the first or channel 0) should be running to the
    // Multiple Output (MO) if the requested channel is 1. More generally it checks if the requested channel is within the current
    // channel range allocated in the thread or past it. To perform the transition it stops the thread, deletes it and creates a new one.
    // It marks the thread as needing start.
    //
    // If the requested channel is within the thread channel range (this thread being already allocated) it simply removes its FIFO reference
    // so that the samples are not taken from the FIFO anymore and leaves the thread unchanged (no stop, no delete/new)
    //
    // If there is no thread allocated it creates a new one with a number of channels that fits the requested channel. That is
    // 1 if channel 0 is requested (SO mode) and 2 if channel 1 is requested (MO mode). It marks the thread as needing start.
    //
    // Eventually it registers the FIFO in the thread. If the thread has to be started it enables the channels up to the number of channels
    // allocated in the thread and starts the thread.

    if (!m_deviceShared.m_dev)
    {
        qDebug("BladeRF2Output::start: no device object");
        return false;
    }

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    BladeRF2OutputThread *bladeRF2OutputThread = findThread();
    bool needsStart = false;

    if (bladeRF2OutputThread) // if thread is already allocated
    {
        qDebug("BladeRF2Output::start: thread is already allocated");

        int nbOriginalChannels = bladeRF2OutputThread->getNbChannels();

        if (requestedChannel+1 > nbOriginalChannels) // expansion by deleting and re-creating the thread
        {
            qDebug("BladeRF2Output::start: expand channels. Re-allocate thread and take ownership");

            SampleSourceFifo **fifos = new SampleSourceFifo*[nbOriginalChannels];
            unsigned int *log2Interps = new unsigned int[nbOriginalChannels];

            for (int i = 0; i < nbOriginalChannels; i++) // save original FIFO references and data
            {
                fifos[i] = bladeRF2OutputThread->getFifo(i);
                log2Interps[i] = bladeRF2OutputThread->getLog2Interpolation(i);
            }

            bladeRF2OutputThread->stopWork();
            delete bladeRF2OutputThread;
            bladeRF2OutputThread = new BladeRF2OutputThread(m_deviceShared.m_dev->getDev(), requestedChannel+1);
            m_thread = bladeRF2OutputThread; // take ownership

            for (int i = 0; i < nbOriginalChannels; i++) // restore original FIFO references
            {
                bladeRF2OutputThread->setFifo(i, fifos[i]);
                bladeRF2OutputThread->setLog2Interpolation(i, log2Interps[i]);
            }

            // remove old thread address from buddies (reset in all buddies)
            const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
            std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

            for (; it != sinkBuddies.end(); ++it) {
                ((DeviceBladeRF2Shared*) (*it)->getBuddySharedPtr())->m_sink->setThread(0);
            }

            // close all channels
            for (int i = bladeRF2OutputThread->getNbChannels()-1; i >= 0; i--) {
                m_deviceShared.m_dev->closeTx(i);
            }

            // was used as temporary storage:
            delete[] fifos;
            delete[] log2Interps;

            needsStart = true;
        }
        else
        {
            qDebug("BladeRF2Output::start: keep buddy thread");
        }
    }
    else // first allocation
    {
        qDebug("BladeRF2Output::start: allocate thread and take ownership");
        bladeRF2OutputThread = new BladeRF2OutputThread(m_deviceShared.m_dev->getDev(), requestedChannel+1);
        m_thread = bladeRF2OutputThread; // take ownership
        needsStart = true;
    }

    bladeRF2OutputThread->setFifo(requestedChannel, &m_sampleSourceFifo);
    bladeRF2OutputThread->setLog2Interpolation(requestedChannel, m_settings.m_log2Interp);

    applySettings(m_settings, QList<QString>(), true); // re-apply forcibly to set sample rate with the new number of channels

    if (needsStart)
    {
        qDebug("BladeRF2Output::start: enabling channel(s) and (re)starting the thread");

        for (unsigned int i = 0; i < bladeRF2OutputThread->getNbChannels(); i++) // open all channels
        {
            if (!m_deviceShared.m_dev->openTx(i)) {
                qCritical("BladeRF2Output::start: channel %u cannot be enabled", i);
            }
        }

        bladeRF2OutputThread->startWork();
    }

    qDebug("BladeRF2Output::start: started");
    m_running = true;

    return true;
}

void BladeRF2Output::stop()
{
    // This stop method is responsible for managing the thread and channel disabling when the streaming of
    // a Tx channel is stopped
    //
    // If the thread is currently managing only one channel (SO mode). The thread can be just stopped and deleted.
    // Then the channel is closed (disabled).
    //
    // If the thread is currently managing many channels (MO mode) and we are removing the last channel. The transition
    // from MO to SO or reduction of MO size is handled by stopping the thread, deleting it and creating a new one
    // with one channel less if (and only if) there is still a channel active.
    //
    // If the thread is currently managing many channels (MO mode) but the channel being stopped is not the last
    // channel then the FIFO reference is simply removed from the thread so that this FIFO will not be used anymore.
    // In this case the channel is not closed (disabled) so that other channels can continue with the
    // same configuration. The device continues streaming on this channel but the samples are set to all zeros.

    if (!m_running) {
        return;
    }

    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    BladeRF2OutputThread *bladeRF2OutputThread = findThread();

    if (bladeRF2OutputThread == 0) { // no thread allocated
        return;
    }

    int nbOriginalChannels = bladeRF2OutputThread->getNbChannels();

    if (nbOriginalChannels == 1) // SO mode => just stop and delete the thread
    {
        qDebug("BladeRF2Output::stop: SO mode. Just stop and delete the thread");
        bladeRF2OutputThread->stopWork();
        delete bladeRF2OutputThread;
        m_thread = 0;

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

        for (; it != sinkBuddies.end(); ++it) {
            ((DeviceBladeRF2Shared*) (*it)->getBuddySharedPtr())->m_sink->setThread(0);
        }

        m_deviceShared.m_dev->closeTx(0); // close the unique channel
    }
    else if (requestedChannel == nbOriginalChannels - 1) // remove last MO channel => reduce by deleting and re-creating the thread
    {
        qDebug("BladeRF2Output::stop: MO mode. Reduce by deleting and re-creating the thread");
        bladeRF2OutputThread->stopWork();
        SampleSourceFifo **fifos = new SampleSourceFifo*[nbOriginalChannels-1];
        unsigned int *log2Interps = new unsigned int[nbOriginalChannels-1];
        bool stillActiveFIFO = false;

        for (int i = 0; i < nbOriginalChannels-1; i++) // save original FIFO references
        {
            fifos[i] = bladeRF2OutputThread->getFifo(i);
            stillActiveFIFO = stillActiveFIFO || (bladeRF2OutputThread->getFifo(i) != 0);
            log2Interps[i] = bladeRF2OutputThread->getLog2Interpolation(i);
        }

        delete bladeRF2OutputThread;
        m_thread = 0;

        if (stillActiveFIFO)
        {
            bladeRF2OutputThread = new BladeRF2OutputThread(m_deviceShared.m_dev->getDev(), nbOriginalChannels-1);
            m_thread = bladeRF2OutputThread; // take ownership

            for (int i = 0; i < nbOriginalChannels-1; i++)  // restore original FIFO references
            {
                bladeRF2OutputThread->setFifo(i, fifos[i]);
                bladeRF2OutputThread->setLog2Interpolation(i, log2Interps[i]);
            }
        }
        else
        {
            qDebug("BladeRF2Output::stop: do not re-create thread as there are no more FIFOs active");
        }

        // remove old thread address from buddies (reset in all buddies)
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator it = sinkBuddies.begin();

        for (; it != sinkBuddies.end(); ++it) {
            ((DeviceBladeRF2Shared*) (*it)->getBuddySharedPtr())->m_sink->setThread(0);
        }

        // close all channels
        for (int i = nbOriginalChannels-1; i >= 0; i--) {
            m_deviceShared.m_dev->closeTx(i);
        }

        if (stillActiveFIFO)
        {
            qDebug("BladeRF2Output::stop: enabling channel(s) and restarting the thread");

            for (unsigned int i = 0; i < bladeRF2OutputThread->getNbChannels(); i++) // open all channels
            {
                if (!m_deviceShared.m_dev->openTx(i)) {
                    qCritical("BladeRF2Output::start: channel %u cannot be enabled", i);
                }
            }

            bladeRF2OutputThread->startWork();
        }

        // was used as temporary storage:
        delete[] fifos;
        delete[] log2Interps;
    }
    else // remove channel from existing thread
    {
        qDebug("BladeRF2Output::stop: MO mode. Not changing MO configuration. Just remove FIFO reference");
        bladeRF2OutputThread->setFifo(requestedChannel, 0); // remove FIFO
    }

    applySettings(m_settings, QList<QString>(), true); // re-apply forcibly to set sample rate with the new number of channels

    m_running = false;
}

QByteArray BladeRF2Output::serialize() const
{
    return m_settings.serialize();
}

bool BladeRF2Output::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureBladeRF2* message = MsgConfigureBladeRF2::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladeRF2* messageToGUI = MsgConfigureBladeRF2::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& BladeRF2Output::getDeviceDescription() const
{
    return m_deviceDescription;
}

int BladeRF2Output::getSampleRate() const
{
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2Interp));
}

quint64 BladeRF2Output::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void BladeRF2Output::setCenterFrequency(qint64 centerFrequency)
{
    BladeRF2OutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureBladeRF2* message = MsgConfigureBladeRF2::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladeRF2* messageToGUI = MsgConfigureBladeRF2::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool BladeRF2Output::setDeviceCenterFrequency(struct bladerf *dev, int requestedChannel, quint64 freq_hz, int loPpmTenths)
{
    qint64 df = ((qint64)freq_hz * loPpmTenths) / 10000000LL;
    freq_hz += df;

    int status = bladerf_set_frequency(dev, BLADERF_CHANNEL_TX(requestedChannel), freq_hz);

    if (status < 0) {
        qWarning("BladeRF2Output::setDeviceCenterFrequency: bladerf_set_frequency(%lld) failed: %s",
                freq_hz, bladerf_strerror(status));
        return false;
    }
    else
    {
        qDebug("BladeRF2Output::setDeviceCenterFrequency: bladerf_set_frequency(%lld)", freq_hz);
        return true;
    }
}

void BladeRF2Output::getFrequencyRange(uint64_t& min, uint64_t& max, int& step, float& scale)
{
    if (m_deviceShared.m_dev) {
        m_deviceShared.m_dev->getFrequencyRangeTx(min, max, step, scale);
    }
}

void BladeRF2Output::getSampleRateRange(int& min, int& max, int& step, float& scale)
{
    if (m_deviceShared.m_dev) {
        m_deviceShared.m_dev->getSampleRateRangeTx(min, max, step, scale);
    }
}

void BladeRF2Output::getBandwidthRange(int& min, int& max, int& step, float& scale)
{
    if (m_deviceShared.m_dev) {
        m_deviceShared.m_dev->getBandwidthRangeTx(min, max, step, scale);
    }
}

void BladeRF2Output::getGlobalGainRange(int& min, int& max, int& step, float& scale)
{
    if (m_deviceShared.m_dev) {
        m_deviceShared.m_dev->getGlobalGainRangeTx(min, max, step, scale);
    }
}

bool BladeRF2Output::handleMessage(const Message& message)
{
    if (MsgConfigureBladeRF2::match(message))
    {
        MsgConfigureBladeRF2& conf = (MsgConfigureBladeRF2&) message;
        qDebug() << "BladeRF2Output::handleMessage: MsgConfigureBladeRF2";

        if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce()))
        {
            qDebug("BladeRF2Output::handleMessage: MsgConfigureBladeRF2 config error");
        }

        return true;
    }
    else if (DeviceBladeRF2Shared::MsgReportBuddyChange::match(message))
    {
        DeviceBladeRF2Shared::MsgReportBuddyChange& report = (DeviceBladeRF2Shared::MsgReportBuddyChange&) message;
        struct bladerf *dev = m_deviceShared.m_dev->getDev();
        BladeRF2OutputSettings settings = m_settings;
        int status;
        unsigned int tmp_uint;
        bool tmp_bool;

        // evaluate changes that may have been introduced by changes in a buddy

        if (dev) // The BladeRF device must have been open to do so
        {
            int requestedChannel = m_deviceAPI->getDeviceItemIndex();
            QList<QString> settingsKeys;

            if (report.getRxElseTx()) // Rx buddy change: check for sample rate change only
            {
                settings.m_devSampleRate = report.getDevSampleRate();
                settingsKeys.append("devSampleRate");
//                status = bladerf_get_sample_rate(dev, BLADERF_CHANNEL_TX(requestedChannel), &tmp_uint);
//
//                if (status < 0) {
//                    qCritical("BladeRF2Output::handleMessage: MsgReportBuddyChange: bladerf_get_sample_rate error: %s", bladerf_strerror(status));
//                } else {
//                    settings.m_devSampleRate = tmp_uint;
//                }
            }
            else // Tx buddy change: check for: frequency, gain mode and value, bias tee, sample rate, bandwidth
            {
                settings.m_devSampleRate = report.getDevSampleRate();
                settings.m_LOppmTenths = report.getLOppmTenths();
                settings.m_centerFrequency = report.getCenterFrequency();
                settingsKeys.append("devSampleRate");
                settingsKeys.append("LOppmTenths");
                settingsKeys.append("centerFrequency");

                status = bladerf_get_bandwidth(dev, BLADERF_CHANNEL_TX(requestedChannel), &tmp_uint);

                if (status < 0) {
                    qCritical("BladeRF2Output::handleMessage: MsgReportBuddyChange: bladerf_get_bandwidth error: %s", bladerf_strerror(status));
                } else {
                    settings.m_bandwidth = tmp_uint;
                    settingsKeys.append("bandwidth");
                }

                status = bladerf_get_bias_tee(dev, BLADERF_CHANNEL_TX(requestedChannel), &tmp_bool);

                if (status < 0) {
                    qCritical("BladeRF2Output::handleMessage: MsgReportBuddyChange: bladerf_get_bias_tee error: %s", bladerf_strerror(status));
                } else {
                    settings.m_biasTee = tmp_bool;
                    settingsKeys.append("biasTee");
                }
            }

            // change DSP settings if buddy change introduced a change in center frequency or base rate
            if (settingsKeys.contains("centerFrequency") || settingsKeys.contains("devSampleRate"))
            {
                int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Interp);
                DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
                m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
            }

            m_settings.applySettings(settingsKeys, settings); // acknowledge the new settings

            // propagate settings to GUI if any
            if (getMessageQueueToGUI())
            {
                MsgConfigureBladeRF2 *reportToGUI = MsgConfigureBladeRF2::create(m_settings, settingsKeys, false);
                getMessageQueueToGUI()->push(reportToGUI);
            }
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "BladeRF2Input::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else
    {
        return false;
    }
}

bool BladeRF2Output::applySettings(const BladeRF2OutputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "BladeRF2Output::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    bool forwardChangeOwnDSP = false;
    bool forwardChangeRxBuddies  = false;
    bool forwardChangeTxBuddies = false;

    struct bladerf *dev = m_deviceShared.m_dev->getDev();
    int requestedChannel = m_deviceAPI->getDeviceItemIndex();
    qint64 deviceCenterFrequency = settings.m_centerFrequency;
    deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
    deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;

    // if ((m_settings.m_devSampleRate != settings.m_devSampleRate) ||
    //     (m_settings.m_log2Interp != settings.m_log2Interp) || force)
    if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("log2Interp") || force)
    {
        BladeRF2OutputThread *bladeRF2OutputThread = findThread();
        SampleSourceFifo *fifo = nullptr;

        if (bladeRF2OutputThread)
        {
            fifo = bladeRF2OutputThread->getFifo(requestedChannel);
            bladeRF2OutputThread->setFifo(requestedChannel, 0);
        }

#if defined(_MSC_VER)
        unsigned int fifoRate = (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2Interp);
        fifoRate = fifoRate < 48000U ? 48000U : fifoRate;
#else
        unsigned int fifoRate = std::max(
            (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2Interp),
            DeviceBladeRF2Shared::m_sampleFifoMinRate);
#endif
        m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(fifoRate));

        if (fifo) {
            bladeRF2OutputThread->setFifo(requestedChannel, &m_sampleSourceFifo);
        }
    }

    if (settingsKeys.contains("devSampleRate") || force)
    {
        forwardChangeOwnDSP = true;
        forwardChangeRxBuddies = true;
        forwardChangeTxBuddies = true;

        if (dev != 0)
        {
            unsigned int actualSamplerate;

            int status = bladerf_set_sample_rate(dev, BLADERF_CHANNEL_TX(requestedChannel),
                    settings.m_devSampleRate,
                    &actualSamplerate);

            if (status < 0)
            {
                qCritical("BladeRF2Output::applySettings: could not set sample rate: %d: %s",
                        settings.m_devSampleRate, bladerf_strerror(status));
            }
            else
            {
                qDebug() << "BladeRF2Output::applySettings: bladerf_set_sample_rate: actual sample rate is " << actualSamplerate;
            }
        }
    }

    if (settingsKeys.contains("bandwidth") || force)
    {
        forwardChangeTxBuddies = true;

        if (dev != 0)
        {
            unsigned int actualBandwidth;
            int status = bladerf_set_bandwidth(dev, BLADERF_CHANNEL_TX(requestedChannel), settings.m_bandwidth, &actualBandwidth);

            if(status < 0)
            {
                qCritical("BladeRF2Output::applySettings: could not set bandwidth: %d: %s",
                        settings.m_bandwidth, bladerf_strerror(status));
            }
            else
            {
                qDebug() << "BladeRF2Output::applySettings: bladerf_set_bandwidth: actual bandwidth is " << actualBandwidth;
            }
        }
    }

    if (settingsKeys.contains("log2Interp") || force)
    {
        forwardChangeOwnDSP = true;
        BladeRF2OutputThread *outputThread = findThread();

        if (outputThread != 0)
        {
            outputThread->setLog2Interpolation(requestedChannel, settings.m_log2Interp);
            qDebug() << "BladeRF2Output::applySettings: set interpolation to " << (1<<settings.m_log2Interp);
        }
    }

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("transverterMode")
        || settingsKeys.contains("transverterDeltaFrequency")
        || settingsKeys.contains("LOppmTenths")
        || settingsKeys.contains("devSampleRate") || force)
    {
        forwardChangeOwnDSP = true;
        forwardChangeTxBuddies = true;

        if (dev != 0)
        {
            if (setDeviceCenterFrequency(dev, requestedChannel, deviceCenterFrequency, settings.m_LOppmTenths))
            {
                if (getMessageQueueToGUI())
                {
                    int min, max, step;
                    float scale;
                    getGlobalGainRange(min, max, step, scale);
                    MsgReportGainRange *msg = MsgReportGainRange::create(min, max, step, scale);
                    getMessageQueueToGUI()->push(msg);
                }
            }
        }
    }

    if (settingsKeys.contains("biasTee") || force)
    {
        forwardChangeTxBuddies = true;
        m_deviceShared.m_dev->setBiasTeeTx(settings.m_biasTee);
    }

    if (settingsKeys.contains("globalGain") || force)
    {
        forwardChangeTxBuddies = true;

        if (dev)
        {
//            qDebug("BladeRF2Output::applySettings: channel: %d gain: %d", requestedChannel, settings.m_globalGain);
            int status = bladerf_set_gain(dev, BLADERF_CHANNEL_TX(requestedChannel), settings.m_globalGain);

            if (status < 0) {
                qWarning("BladeRF2Output::applySettings: bladerf_set_gain(%d) failed: %s",
                        settings.m_globalGain, bladerf_strerror(status));
            } else {
                qDebug("BladeRF2Output::applySettings: bladerf_set_gain(%d)", settings.m_globalGain);
            }
        }
    }

    if (forwardChangeOwnDSP)
    {
        int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Interp);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (forwardChangeRxBuddies)
    {
        // send to source buddies
        const std::vector<DeviceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceBladeRF2Shared::MsgReportBuddyChange *report = DeviceBladeRF2Shared::MsgReportBuddyChange::create(
                    settings.m_centerFrequency,
                    settings.m_LOppmTenths,
                    2,
                    settings.m_devSampleRate, // need to forward actual rate to the Rx side
                    false);
            (*itSource)->getSamplingDeviceInputMessageQueue()->push(report);
        }
    }

    if (forwardChangeTxBuddies)
    {
        // send to sink buddies
        const std::vector<DeviceAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceBladeRF2Shared::MsgReportBuddyChange *report = DeviceBladeRF2Shared::MsgReportBuddyChange::create(
                    settings.m_centerFrequency,
                    settings.m_LOppmTenths,
                    2,
                    settings.m_devSampleRate,
                    false);
            (*itSink)->getSamplingDeviceInputMessageQueue()->push(report);
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

    return true;
}

int BladeRF2Output::getNbChannels()
{
    BladeRF2OutputThread *bladeRF2OutputThread = findThread();

    if (bladeRF2OutputThread) {
        return bladeRF2OutputThread->getNbChannels();
    } else {
        return 0;
    }
}

int BladeRF2Output::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setBladeRf2OutputSettings(new SWGSDRangel::SWGBladeRF2OutputSettings());
    response.getBladeRf2OutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int BladeRF2Output::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    BladeRF2OutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureBladeRF2 *msg = MsgConfigureBladeRF2::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBladeRF2 *msgToGUI = MsgConfigureBladeRF2::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void BladeRF2Output::webapiUpdateDeviceSettings(
        BladeRF2OutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getBladeRf2OutputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getBladeRf2OutputSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getBladeRf2OutputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getBladeRf2OutputSettings()->getBandwidth();
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getBladeRf2OutputSettings()->getLog2Interp();
    }
    if (deviceSettingsKeys.contains("biasTee")) {
        settings.m_biasTee = response.getBladeRf2OutputSettings()->getBiasTee() != 0;
    }
    if (deviceSettingsKeys.contains("globalGain")) {
        settings.m_globalGain = response.getBladeRf2OutputSettings()->getGlobalGain();
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getBladeRf2OutputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getBladeRf2OutputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getBladeRf2OutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getBladeRf2OutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getBladeRf2OutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getBladeRf2OutputSettings()->getReverseApiDeviceIndex();
    }
}

int BladeRF2Output::webapiReportGet(SWGSDRangel::SWGDeviceReport& response, QString& errorMessage)
{
    (void) errorMessage;
    response.setBladeRf2OutputReport(new SWGSDRangel::SWGBladeRF2OutputReport());
    response.getBladeRf2OutputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void BladeRF2Output::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const BladeRF2OutputSettings& settings)
{
    response.getBladeRf2OutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getBladeRf2OutputSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getBladeRf2OutputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getBladeRf2OutputSettings()->setBandwidth(settings.m_bandwidth);
    response.getBladeRf2OutputSettings()->setLog2Interp(settings.m_log2Interp);
    response.getBladeRf2OutputSettings()->setBiasTee(settings.m_biasTee ? 1 : 0);
    response.getBladeRf2OutputSettings()->setGlobalGain(settings.m_globalGain);
    response.getBladeRf2OutputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getBladeRf2OutputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    response.getBladeRf2OutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getBladeRf2OutputSettings()->getReverseApiAddress()) {
        *response.getBladeRf2OutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getBladeRf2OutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getBladeRf2OutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getBladeRf2OutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void BladeRF2Output::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    DeviceBladeRF2 *device = m_deviceShared.m_dev;

    if (device)
    {
        int min, max, step;
        float scale;
        uint64_t f_min, f_max;

        device->getBandwidthRangeTx(min, max, step, scale);

        response.getBladeRf2OutputReport()->setBandwidthRange(new SWGSDRangel::SWGRange);
        response.getBladeRf2OutputReport()->getBandwidthRange()->setMin(min);
        response.getBladeRf2OutputReport()->getBandwidthRange()->setMax(max);
        response.getBladeRf2OutputReport()->getBandwidthRange()->setStep(step);
        response.getBladeRf2OutputReport()->getBandwidthRange()->setScale(scale);

        device->getFrequencyRangeTx(f_min, f_max, step, scale);

        response.getBladeRf2OutputReport()->setFrequencyRange(new SWGSDRangel::SWGFrequencyRange);
        response.getBladeRf2OutputReport()->getFrequencyRange()->setMin(f_min);
        response.getBladeRf2OutputReport()->getFrequencyRange()->setMax(f_max);
        response.getBladeRf2OutputReport()->getFrequencyRange()->setStep(step);
        response.getBladeRf2OutputReport()->getBandwidthRange()->setScale(scale);

        device->getGlobalGainRangeTx(min, max, step, scale);

        response.getBladeRf2OutputReport()->setGlobalGainRange(new SWGSDRangel::SWGRange);
        response.getBladeRf2OutputReport()->getGlobalGainRange()->setMin(min);
        response.getBladeRf2OutputReport()->getGlobalGainRange()->setMax(max);
        response.getBladeRf2OutputReport()->getGlobalGainRange()->setStep(step);
        response.getBladeRf2OutputReport()->getBandwidthRange()->setScale(scale);

        device->getSampleRateRangeTx(min, max, step, scale);

        response.getBladeRf2OutputReport()->setSampleRateRange(new SWGSDRangel::SWGRange);
        response.getBladeRf2OutputReport()->getSampleRateRange()->setMin(min);
        response.getBladeRf2OutputReport()->getSampleRateRange()->setMax(max);
        response.getBladeRf2OutputReport()->getSampleRateRange()->setStep(step);
        response.getBladeRf2OutputReport()->getBandwidthRange()->setScale(scale);
    }
}

int BladeRF2Output::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int BladeRF2Output::webapiRun(
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

void BladeRF2Output::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const BladeRF2OutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("BladeRF2"));
    swgDeviceSettings->setBladeRf2OutputSettings(new SWGSDRangel::SWGBladeRF2OutputSettings());
    SWGSDRangel::SWGBladeRF2OutputSettings *swgBladeRF2OutputSettings = swgDeviceSettings->getBladeRf2OutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgBladeRF2OutputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgBladeRF2OutputSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgBladeRF2OutputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("bandwidth") || force) {
        swgBladeRF2OutputSettings->setBandwidth(settings.m_bandwidth);
    }
    if (deviceSettingsKeys.contains("log2Interp") || force) {
        swgBladeRF2OutputSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (deviceSettingsKeys.contains("biasTee") || force) {
        swgBladeRF2OutputSettings->setBiasTee(settings.m_biasTee ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("globalGain") || force) {
        swgBladeRF2OutputSettings->setGlobalGain(settings.m_globalGain);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgBladeRF2OutputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgBladeRF2OutputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
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

void BladeRF2Output::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("BladeRF2"));

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

void BladeRF2Output::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "BladeRF2Output::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("BladeRF2Output::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }
}
