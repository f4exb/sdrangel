///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Copyright (C) 2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <errno.h>
#include <algorithm>
#include <functional>
#include <mutex>

#include <QDebug>

#include <uhd/types/stream_cmd.hpp>

#include "usrp/deviceusrpparam.h"
#include "usrpinputthread.h"
#include "usrpinput.h"

USRPInputThread::USRPInputThread(DeviceUSRPParams *deviceParams, int channel, SampleSinkFifo* sampleFifo,
  ReplayBuffer<qint16> *replayBuffer, MessageQueue *reportQueue, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_started(false),
    m_runStarted(false),
    m_packets(0),
    m_overflows(0),
    m_timeouts(0),
    m_buf(nullptr),
    m_bufSamples(0),
    m_sampleFifo(sampleFifo),
    m_replayBuffer(replayBuffer),
    m_log2Decim(0),
    m_deviceParams(deviceParams),
    m_channel(channel),
    m_reportQueue(reportQueue),
    m_detached(false)
{
    setObjectName("USRPInput");
    // Executes in the thread this object was created in (the device engine thread), not in run()
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &USRPInputThread::handleInputMessages);
}

USRPInputThread::~USRPInputThread()
{
    stopWork();
    delete[] m_buf;
}

// Set stream once it has been created. Must be called before startWork()
void USRPInputThread::setStream(uhd::rx_streamer::sptr stream, size_t bufSamples)
{
    m_stream = stream;
    m_bufSamples = bufSamples;
    m_convertBuffer.resize(bufSamples);
    delete[] m_buf;
    // *2 as samples are I+Q
    m_buf = new qint16[2*bufSamples];
    std::fill(m_buf, m_buf + 2*bufSamples, 0);
}

void USRPInputThread::issueStreamCmd(bool start)
{
    uhd::stream_cmd_t stream_cmd(start ? uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS : uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    stream_cmd.num_samps = size_t(0);
    stream_cmd.stream_now = true;
    stream_cmd.time_spec = uhd::time_spec_t();

    if (m_stream)
    {
        m_stream->issue_stream_cmd(stream_cmd);
        qDebug() << "USRPInputThread::issueStreamCmd " << (start ? "start" : "stop");
    }
    else
    {
        qDebug() << "USRPInputThread::issueStreamCmd m_stream is null";
    }
}

// Release reference to stream, so it's destroyed before the device. Thread must be stopped.
void USRPInputThread::releaseStream()
{
    m_stream = nullptr;
}

void USRPInputThread::startWork()
{
    if (m_started) return; // return if running already

    if (!m_stream)
    {
        qWarning("USRPInputThread::startWork: no stream");
        return;
    }

    try
    {
        // Start streaming
        issueStreamCmd(true);

        // Reset stats
        m_packets = 0;
        m_overflows = 0;
        m_timeouts = 0;

        qDebug("USRPInputThread::startWork: stream started");
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInputThread::startWork: exception: " << e.what();
    }

    // Wait for run() to start. Use a separate flag to m_running, as run() may exit immediately on error
    m_runStarted = false;
    m_started = true;
    m_startWaitMutex.lock();
    start();
    while (!m_runStarted) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }
    m_startWaitMutex.unlock();
}

void USRPInputThread::stopWork()
{
    if (!m_started) return; // return if not running

    m_started = false;
    m_running = false;
    wait();

    try
    {
        uhd::rx_metadata_t md;

        // Stop streaming
        issueStreamCmd(false);

        // Clear out any data left in the stream, otherwise we'll get an
        // exception 'recv buffer smaller than vrt packet offset' when restarting
        // Limit number of iterations, in case device has been disconnected
        const int maxFlushes = 1000;
        md.end_of_burst = false;
        md.error_code = uhd::rx_metadata_t::ERROR_CODE_NONE;
        for (int i = 0; (i < maxFlushes) && !md.end_of_burst && (md.error_code != uhd::rx_metadata_t::ERROR_CODE_TIMEOUT); i++)
        {
            try
            {
                md.reset();
                recv(md, 0.1);
            }
            catch (std::exception& e)
            {
                qDebug() << "USRPInputThread::stopWork: exception while flushing buffers: " << e.what();
                break;
            }
        }

        qDebug("USRPInputThread::stopWork: stream stopped");
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInputThread::stopWork: exception: " << e.what();
    }
}

// Receive samples into m_buf. recv() calls for different streams on the same device
// may need to be serialised (See DeviceUSRPRecvLock)
size_t USRPInputThread::recv(uhd::rx_metadata_t& md, double timeout)
{
    if (!m_deviceParams) {
        return m_stream->recv(m_buf, m_bufSamples, md, timeout);
    }

    // Only wait for a short time while holding the lock, so if our stream isn't receiving data
    // (E.g. it's being restarted after a timeout), streams of buddies aren't blocked for long, which would cause them to overflow.
    // The buffer is at most a few packets, so should be filled well within this time, at the sample rates where locking is used
    const double maxLockedTimeout = 0.05;
    double remaining = timeout;

    while (true)
    {
        const double lockedTimeout = std::min(remaining, maxLockedTimeout);
        size_t samplesReceived;

        {
            std::lock_guard<DeviceUSRPRecvLock> recvLock(m_deviceParams->m_recvLock);
            samplesReceived = m_stream->recv(m_buf, m_bufSamples, md, lockedTimeout);
        }

        if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
            return samplesReceived;
        }

        if (samplesReceived > 0)
        {
            // Partially filled buffer: return what we have, rather than reporting a timeout, which would restart the stream
            md.error_code = uhd::rx_metadata_t::ERROR_CODE_NONE;
            return samplesReceived;
        }

        remaining -= lockedTimeout;

        if (remaining <= 0.0) {
            return samplesReceived; // Timed out
        }
    }
}

void USRPInputThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

void USRPInputThread::run()
{
    uhd::rx_metadata_t md;

    // Higher priority, so streaming isn't delayed by other threads
    DeviceUSRP::setStreamingThreadPriority();

    // Hold mutex, so wake can't occur between startWork() checking m_runStarted and waiting
    m_startWaitMutex.lock();
    m_running = true;
    m_runStarted = true;
    m_startWaiter.wakeAll();
    m_startWaitMutex.unlock();

    try
    {
        while (m_running)
        {
            md.reset();
            const size_t samples_received = recv(md, DeviceUSRP::m_streamTimeout);

            m_packets++;
            if (samples_received != m_bufSamples)
            {
                qDebug("USRPInputThread::run - received %zu/%zu samples", samples_received, m_bufSamples);
            }
            if (md.error_code ==  uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
            {
                qDebug("USRPInputThread::run - timeout - ending thread");
                m_timeouts++;
                // Restart streaming
                issueStreamCmd(false);
                issueStreamCmd(true);
                qDebug("USRPInputThread::run - timeout - restarting");
            }
            else if (md.error_code ==  uhd::rx_metadata_t::ERROR_CODE_OVERFLOW)
            {
                qDebug("USRPInputThread::run - overflow");
                m_overflows++;
            }
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_LATE_COMMAND)
                qDebug("USRPInputThread::run - late command error");
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_BROKEN_CHAIN)
                qDebug("USRPInputThread::run - broken chain error");
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_ALIGNMENT)
                qDebug("USRPInputThread::run - alignment error");
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_BAD_PACKET)
                qDebug("USRPInputThread::run - bad packet error");

            if (samples_received > 0)
                callbackIQ(m_buf, 2 * samples_received);
        }
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInputThread::run: exception: " << e.what();
    }

    m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void USRPInputThread::callbackIQ(const qint16* inBuf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    // Save data to replay buffer
    m_replayBuffer->lock();
    bool replayEnabled = m_replayBuffer->getSize() > 0;
    if (replayEnabled) {
        m_replayBuffer->write(inBuf, len);
    }

    const qint16* buf = inBuf;
    qint32 remaining = len;

    while (remaining > 0)
    {
        // Choose between live data or replayed data
        if (replayEnabled && m_replayBuffer->useReplay()) {
            len = m_replayBuffer->read(remaining, buf);
        } else {
            len = remaining;
        }
        remaining -= len;

        switch (m_log2Decim)
        {
        case 0:
            m_decimatorsIQ.decimate1(&it, buf, len);
            break;
        case 1:
            m_decimatorsIQ.decimate2_cen(&it, buf, len);
            break;
        case 2:
            m_decimatorsIQ.decimate4_cen(&it, buf, len);
            break;
        case 3:
            m_decimatorsIQ.decimate8_cen(&it, buf, len);
            break;
        case 4:
            m_decimatorsIQ.decimate16_cen(&it, buf, len);
            break;
        case 5:
            m_decimatorsIQ.decimate32_cen(&it, buf, len);
            break;
        case 6:
            m_decimatorsIQ.decimate64_cen(&it, buf, len);
            break;
        default:
            break;
        }
    }

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}

void USRPInputThread::getStreamStatus(bool& active, quint32& overflows, quint32& timeouts)
{
    //qDebug() << "USRPInputThread::getStreamStatus " << m_packets << " " << m_overflows << " " << m_timeouts;
    active = m_packets > 0;
    overflows = m_overflows;
    timeouts = m_timeouts;
}


// Stop applying settings to the device and sending reports, waiting for any settings being applied to complete.
// Used when stopping from a thread other than the device engine thread (E.g. when USRPInput is being destroyed)
void USRPInputThread::detach()
{
    QMutexLocker mutexLocker(&m_configMutex);
    m_detached = true;
}

void USRPInputThread::handleInputMessages()
{
    QMutexLocker mutexLocker(&m_configMutex);
    Message* message;

    // Merge consecutive configuration messages, so we only apply the latest settings.
    // This avoids a backlog of slow device calls (E.g. when a slider is dragged)
    bool pendingConfig = false;
    USRPInputSettings settings;
    QList<QString> settingsKeys;
    bool force = false;

    auto applyPendingConfig = [&]() {
        if (pendingConfig)
        {
            if (settingsKeys.contains("log2SoftDecim") || force) {
                setLog2Decimation(settings.m_log2SoftDecim);
            }
            applyDeviceSettings(m_deviceParams, m_channel, settings, settingsKeys, force, true, true, m_reportQueue);
            pendingConfig = false;
            settingsKeys.clear();
            force = false;
        }
    };

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (m_detached)
        {
            delete message;
            continue;
        }

        if (USRPInput::MsgConfigureUSRP::match(*message))
        {
            const USRPInput::MsgConfigureUSRP& conf = (const USRPInput::MsgConfigureUSRP&) *message;

            if (conf.getForce() || !pendingConfig)
            {
                settings = conf.getSettings();
                settingsKeys = conf.getSettingsKeys();
                force = force || conf.getForce();
            }
            else
            {
                settings.applySettings(conf.getSettingsKeys(), conf.getSettings());
                for (const auto& key : conf.getSettingsKeys())
                {
                    if (!settingsKeys.contains(key)) {
                        settingsKeys.append(key);
                    }
                }
            }
            pendingConfig = true;
        }
        else
        {
            // Keep ordering with respect to other messages
            applyPendingConfig();

            if (DeviceUSRPShared::MsgReadDeviceSampleRate::match(*message)) {
                readBackSampleRate();
            } else {
                qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
            }
        }

        delete message;
    }

    if (!m_detached) {
        applyPendingConfig();
    }
}

void USRPInputThread::readBackSampleRate()
{
    uhd::usrp::multi_usrp::sptr device = m_deviceParams ? m_deviceParams->getDevice() : nullptr;

    if (!device) {
        return;
    }

    QMutexLocker mutexLocker(&m_deviceParams->m_mutex);

    try
    {
        double sampleRate = DeviceUSRP::getSampleRate(device, true, m_channel);
        double masterClockRate = device->get_master_clock_rate();

        if (m_reportQueue) {
            m_reportQueue->push(DeviceUSRPShared::MsgReportDeviceSettings::create(true, sampleRate, masterClockRate, false, "", false));
        }
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInputThread::readBackSampleRate: exception: " << e.what();
    }
}

// Apply settings to the device and send what was actually set to reportQueue.
// applyChannelSettings - apply settings that require the channel to be acquired. If false, only GPIO settings are applied.
// applyBandwidth - apply LPF bandwidth, which shouldn't be applied until after the stream is created
void USRPInputThread::applyDeviceSettings(
    DeviceUSRPParams *deviceParams,
    int channel,
    const USRPInputSettings& settings,
    const QList<QString>& settingsKeys,
    bool force,
    bool applyChannelSettings,
    bool applyBandwidth,
    MessageQueue *reportQueue)
{
    uhd::usrp::multi_usrp::sptr device = deviceParams ? deviceParams->getDevice() : nullptr;

    if (!device) {
        return;
    }

    QMutexLocker mutexLocker(&deviceParams->m_mutex);
    bool reapplySomeSettings = false;
    bool checkRates          = false;
    bool checkClockSource    = false;

    // Apply each setting separately, so a failure (E.g. an antenna name not supported by the
    // daughterboard) doesn't prevent other settings from being applied
    auto apply = [](const char *what, const std::function<void()>& func) {
        try {
            func();
        } catch (std::exception &e) {
            qWarning() << "USRPInputThread::applyDeviceSettings: failed to set" << what << ":" << e.what();
        }
    };

    auto setGain = [&]() {
        if (settings.m_gainMode == USRPInputSettings::GAIN_AUTO)
        {
            try {
                device->set_rx_agc(true, channel);
                qDebug() << "USRPInputThread::applyDeviceSettings: AGC enabled for channel " << channel;
            } catch (uhd::not_implemented_error &e) {
                qDebug() << "USRPInputThread::applyDeviceSettings: AGC not implemented on this radio. Please set to manual.";
            }
        }
        else
        {
            try {
                device->set_rx_agc(false, channel);
            } catch (uhd::not_implemented_error &e) {
                // Ignore
            }
            device->set_rx_gain(settings.m_gain, channel);
            qDebug() << "USRPInputThread::applyDeviceSettings: gain set to " << settings.m_gain << " for channel " << channel;
        }
    };

    if (applyChannelSettings)
    {
        if (settingsKeys.contains("clockSource") || force)
        {
            try
            {
                device->set_clock_source(settings.m_clockSource.toStdString(), 0);
                qDebug() << "USRPInputThread::applyDeviceSettings: clock set to " << settings.m_clockSource;
            }
            catch (std::exception &e)
            {
                // An exception will be thrown if the clock is not detected
                // however, get_clock_source called below will still say the clock has is set
                qCritical() << "USRPInputThread::applyDeviceSettings: could not set clock " << settings.m_clockSource;
                // So, default back to internal
                apply("clock source to internal", [&]() { device->set_clock_source("internal", 0); });
            }
            // Report actual clock source, in case requested clock couldn't be set
            checkClockSource = true;
            reapplySomeSettings = true;
        }

        if (settingsKeys.contains("devSampleRate") || force)
        {
            apply("sample rate", [&]() {
                DeviceUSRP::setSampleRate(device, true, channel, settings.m_devSampleRate);
                qDebug("USRPInputThread::applyDeviceSettings: sample rate set to %d", settings.m_devSampleRate);
            });
            checkRates = true;
            reapplySomeSettings = true;
        }

        if (settingsKeys.contains("centerFrequency")
            || settingsKeys.contains("loOffset")
            || settingsKeys.contains("transverterMode")
            || settingsKeys.contains("transverterDeltaFrequency")
            || force)
        {
            apply("frequency", [&]() {
                qint64 deviceCenterFrequency = settings.m_centerFrequency;
                deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
                deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
                uhd::tune_request_t tuneRequest = settings.m_loOffset != 0
                    ? uhd::tune_request_t(deviceCenterFrequency, settings.m_loOffset)
                    : uhd::tune_request_t(deviceCenterFrequency);
                device->set_rx_freq(tuneRequest, channel);
                qDebug("USRPInputThread::applyDeviceSettings: frequency set to %lld with LO offset %d", deviceCenterFrequency, settings.m_loOffset);
            });
        }

        if (settingsKeys.contains("dcBlock") || force) {
            apply("DC offset correction", [&]() { device->set_rx_dc_offset(settings.m_dcBlock, channel); });
        }

        if (settingsKeys.contains("iqCorrection") || force) {
            apply("IQ correction", [&]() { device->set_rx_iq_balance(settings.m_iqCorrection, channel); });
        }

        if (settingsKeys.contains("gainMode") || settingsKeys.contains("gain") || force) {
            apply("gain", setGain);
        }

        if (applyBandwidth && (settingsKeys.contains("lpfBW") || force))
        {
            apply("LPF bandwidth", [&]() {
                device->set_rx_bandwidth(settings.m_lpfBW, channel);
                qDebug("USRPInputThread::applyDeviceSettings: LPF BW: %f for channel %d", settings.m_lpfBW, channel);
            });
        }

        if (settingsKeys.contains("antennaPath") || force)
        {
            apply("antenna", [&]() {
                device->set_rx_antenna(settings.m_antennaPath.toStdString(), channel);
                qDebug("USRPInputThread::applyDeviceSettings: antenna set to %s on channel %d", qPrintable(settings.m_antennaPath), channel);
            });
        }
    }

    const std::string gpioBank = "FP0"; // Front Panel GPIO

    if (settingsKeys.contains("gpioDir") || force)
    {
        apply("GPIO direction", [&]() {
            std::vector<std::string> banks = device->get_gpio_banks(0);

            if (std::find(banks.begin(), banks.end(), gpioBank) != banks.end())
            {
                device->set_gpio_attr(gpioBank, "CTRL", ~settings.m_gpioDir, 0xff); // 0 for GPIO, 1 for ATR
                device->set_gpio_attr(gpioBank, "DDR", settings.m_gpioDir, 0xff); // 0 for input, 1 for output
                qDebug() << "USRPInputThread::applyDeviceSettings: set GPIO dir to" << settings.m_gpioDir;
            }
        });
    }

    if (settingsKeys.contains("gpioPins") || force)
    {
        apply("GPIO pins", [&]() {
            std::vector<std::string> banks = device->get_gpio_banks(0);

            if (std::find(banks.begin(), banks.end(), gpioBank) != banks.end())
            {
                device->set_gpio_attr(gpioBank, "OUT", settings.m_gpioPins, 0xff);
                qDebug() << "USRPInputThread::applyDeviceSettings: set GPIO pins to" << settings.m_gpioPins;
            }
        });
    }

    if (reapplySomeSettings && applyBandwidth)
    {
        // Need to re-set bandwidth (and AGC / gain for Rx) after changing sample rate or clock source
        apply("LPF bandwidth", [&]() { device->set_rx_bandwidth(settings.m_lpfBW, channel); });
        apply("gain", setGain);
    }

    // Report what was actually set, so USRPInput can update its settings and inform buddies and GUI
    double sampleRate = 0.0;
    double masterClockRate = 0.0;
    QString clockSource;

    if (checkRates)
    {
        try
        {
            // Check if requested rate could actually be met and what master clock rate we ended up with
            sampleRate = DeviceUSRP::getSampleRate(device, true, channel);
            masterClockRate = device->get_master_clock_rate();
            qDebug("USRPInputThread::applyDeviceSettings: actual sample rate %f master_clock_rate %f", sampleRate, masterClockRate);
        }
        catch (std::exception &e)
        {
            qDebug() << "USRPInputThread::applyDeviceSettings: could not get sample rate: " << e.what();
            checkRates = false;
        }
    }

    if (checkClockSource)
    {
        try
        {
            clockSource = QString::fromStdString(device->get_clock_source(0));
            qDebug() << "USRPInputThread::applyDeviceSettings: clock source is " << clockSource;
        }
        catch (std::exception &e)
        {
            qDebug() << "USRPInputThread::applyDeviceSettings: could not get clock source: " << e.what();
            checkClockSource = false;
        }
    }

    if ((checkRates || checkClockSource) && reportQueue) {
        reportQueue->push(DeviceUSRPShared::MsgReportDeviceSettings::create(checkRates, sampleRate, masterClockRate, checkClockSource, clockSource, true));
    }
}
