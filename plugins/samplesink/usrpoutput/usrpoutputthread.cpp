///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-26 Jon Beniston, M7RCE <jon@beniston.com>                  //
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

#include <QDebug>

#include "dsp/samplesourcefifo.h"

#include "usrp/deviceusrpparam.h"
#include "usrpoutputthread.h"
#include "usrpoutput.h"

USRPOutputThread::USRPOutputThread(DeviceUSRPParams *deviceParams, int channel, SampleSourceFifo* sampleFifo,
    MessageQueue *reportQueue, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_packets(0),
    m_underflows(0),
    m_droppedPackets(0),
    m_buf(nullptr),
    m_bufSamples(0),
    m_sampleFifo(sampleFifo),
    m_log2Interp(0),
    m_deviceParams(deviceParams),
    m_channel(channel),
    m_reportQueue(reportQueue),
    m_detached(false)
{
    setObjectName("USRPOutput");
    // Executes in the thread this object was created in (the device engine thread), not in run()
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &USRPOutputThread::handleInputMessages);
}

USRPOutputThread::~USRPOutputThread()
{
    stopWork();
    delete[] m_buf;
}

// Set stream once it has been created. Must be called before startWork()
void USRPOutputThread::setStream(uhd::tx_streamer::sptr stream, size_t bufSamples)
{
    m_stream = stream;
    m_bufSamples = bufSamples;
    delete[] m_buf;
    // *2 as samples are I+Q
    m_buf = new qint16[2 * bufSamples];
    std::fill(m_buf, m_buf + 2 * bufSamples, 0);
}

// Release reference to stream, so it's destroyed before the device. Thread must be stopped.
void USRPOutputThread::releaseStream()
{
    m_stream = nullptr;
}

void USRPOutputThread::startWork()
{
    if (m_running) return; // return if running already

    if (!m_stream)
    {
        qWarning("USRPOutputThread::startWork: no stream");
        return;
    }

    // Reset stats
    m_packets = 0;
    m_underflows = 0;
    m_droppedPackets = 0;

    m_startWaitMutex.lock();
    start();
    while (!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }
    m_startWaitMutex.unlock();
}

void USRPOutputThread::stopWork()
{
    if (!m_running) return; // return if not running

    m_running = false;
    // run() ends the burst before exiting, so no underflow is reported when stopped,
    // so we don't need to wait for and discard one here (which was done with the device mutex held)
    wait();

    qDebug("USRPOutputThread::stopWork: stream stopped");
}

void USRPOutputThread::setLog2Interpolation(unsigned int log2_interp)
{
    m_log2Interp = log2_interp;
}

void USRPOutputThread::run()
{
    uhd::tx_metadata_t md;

    // Higher priority, so streaming isn't delayed by other threads
    DeviceUSRP::setStreamingThreadPriority();
    // Start a new burst, which is ended when stopped, so the device doesn't report an underflow
    md.start_of_burst = true;
    md.end_of_burst   = false;

    // Hold mutex, so wake can't occur between startWork() checking m_running and waiting
    m_startWaitMutex.lock();
    m_running = true;
    m_startWaiter.wakeAll();
    m_startWaitMutex.unlock();

    qDebug("USRPOutputThread::run");

    while (m_running)
    {
        qint32 writtenSamples = callback(m_buf, m_bufSamples);

        try
        {
            const size_t sentSamples = m_stream->send(m_buf, writtenSamples, md, DeviceUSRP::m_streamTimeout);
            m_packets++;
            if (sentSamples != writtenSamples) {
                qDebug("USRPOutputThread::run sent %ld/%ld samples", sentSamples, writtenSamples);
            }
        }
        catch (std::exception& e)
        {
            qDebug() << "USRPOutputThread::run: exception: " << e.what();
        }

        md.start_of_burst = false;
    }

    // End the burst, so the stream can be restarted cleanly, without an underflow being reported
    try
    {
        md.end_of_burst = true;
        m_stream->send(m_buf, 0, md);
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutputThread::run: exception sending end of burst: " << e.what();
    }

    m_running = false;
}

// Interpolate according to specified log2 (ex: log2=4 => interpolate=16)
// Returns the number of samples written to buf, which may be less than len, when the TX buffer is not divisible by the interpolation factor.
qint32 USRPOutputThread::callback(qint16* buf, qint32 len)
{
    // Read once, as it can be changed from another thread, and the same value must be used for the whole buffer
    const unsigned int log2Interp = m_log2Interp;
    const unsigned int interpolationFactor = 1U << log2Interp;
    SampleVector& data = m_sampleFifo->getData();
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;

    // Truncation is intentional here: reading more would overrun the fixed TX buffer when len is not divisible by interpolation factor.
    m_sampleFifo->read(static_cast<unsigned int>(len)/interpolationFactor, iPart1Begin, iPart1End, iPart2Begin, iPart2End);

    if (iPart1Begin != iPart1End) {
        callbackPart(buf, data, iPart1Begin, iPart1End, log2Interp);
    }

    const unsigned int shift = (iPart1End - iPart1Begin) * interpolationFactor;

    if (iPart2Begin != iPart2End) {
        callbackPart(buf + 2 * shift, data, iPart2Begin, iPart2End, log2Interp);
    }

    return ((iPart1End - iPart1Begin) + (iPart2End - iPart2Begin)) * interpolationFactor;
}

void USRPOutputThread::callbackPart(qint16* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd, unsigned int log2Interp)
{
    SampleVector::iterator beginRead = data.begin() + iBegin;
    const int len = 2 * (iEnd - iBegin) * (1 << log2Interp);

    if (log2Interp == 0)
    {
        m_interpolators.interpolate1(&beginRead, buf, len);
    }
    else
    {
        switch (log2Interp)
        {
        case 1:
            m_interpolators.interpolate2_cen(&beginRead, buf, len);
            break;
        case 2:
            m_interpolators.interpolate4_cen(&beginRead, buf, len);
            break;
        case 3:
            m_interpolators.interpolate8_cen(&beginRead, buf, len);
            break;
        case 4:
            m_interpolators.interpolate16_cen(&beginRead, buf, len);
            break;
        case 5:
            m_interpolators.interpolate32_cen(&beginRead, buf, len);
            break;
        case 6:
            m_interpolators.interpolate64_cen(&beginRead, buf, len);
            break;
        default:
            break;
        }
    }
}

void USRPOutputThread::getStreamStatus(bool& active, quint32& underflows, quint32& droppedPackets)
{
    uhd::async_metadata_t md;

    if (!m_stream)
    {
        active = false;
        underflows = 0;
        droppedPackets = 0;
        return;
    }

    // Don't wait for messages (default timeout is 0.1s), as this is called from the GUI thread
    // Drain all pending messages, so counts are accurate
    QMutexLocker asyncMsgLocker(&m_asyncMsgMutex);

    try
    {
        while (m_stream->recv_async_msg(md, 0.0))
        {
            if ((md.event_code & uhd::async_metadata_t::event_code_t::EVENT_CODE_UNDERFLOW)
                || (md.event_code & uhd::async_metadata_t::event_code_t::EVENT_CODE_UNDERFLOW_IN_PACKET)) {
                m_underflows++;
            }
            if ((md.event_code & uhd::async_metadata_t::event_code_t::EVENT_CODE_SEQ_ERROR)
                || (md.event_code & uhd::async_metadata_t::event_code_t::EVENT_CODE_SEQ_ERROR_IN_BURST)) {
                m_droppedPackets++;
            }
        }
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutputThread::getStreamStatus: exception: " << e.what();
    }
    //qDebug() << "USRPOutputThread::getStreamStatus " << m_packets << " " << m_underflows << " " << m_droppedPackets;
    active = m_packets > 0;
    underflows = m_underflows;
    droppedPackets = m_droppedPackets;
}


// Stop applying settings to the device and sending reports, waiting for any settings being applied to complete.
// Used when stopping from a thread other than the device engine thread (E.g. when USRPOutput is being destroyed)
void USRPOutputThread::detach()
{
    QMutexLocker mutexLocker(&m_configMutex);
    m_detached = true;
}

void USRPOutputThread::handleInputMessages()
{
    QMutexLocker mutexLocker(&m_configMutex);
    Message* message;

    // Merge consecutive configuration messages, so we only apply the latest settings.
    // This avoids a backlog of slow device calls (E.g. when a slider is dragged)
    bool pendingConfig = false;
    USRPOutputSettings settings;
    QList<QString> settingsKeys;
    bool force = false;

    auto applyPendingConfig = [&]() {
        if (pendingConfig)
        {
            if (settingsKeys.contains("log2SoftInterp") || force) {
                setLog2Interpolation(settings.m_log2SoftInterp);
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

        if (USRPOutput::MsgConfigureUSRP::match(*message))
        {
            const USRPOutput::MsgConfigureUSRP& conf = (const USRPOutput::MsgConfigureUSRP&) *message;

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
            } else if (DeviceUSRPShared::MsgResizeSampleFifo::match(*message)) {
                resizeSampleFifo(((const DeviceUSRPShared::MsgResizeSampleFifo&) *message).getSize());
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

// Resize the sample FIFO. This executes in the device engine thread, which writes to the FIFO,
// so the only other thread that accesses it is the streaming thread, which is paused while it is resized.
void USRPOutputThread::resizeSampleFifo(unsigned int size)
{
    // Prevent buddies suspending / resuming the streaming thread at the same time
    QMutexLocker mutexLocker(&m_deviceParams->m_mutex);
    bool wasRunning = m_running;

    if (wasRunning) {
        stopWork();
    }

    m_sampleFifo->resize(size);
    qDebug("USRPOutputThread::resizeSampleFifo: %u", size);

    if (wasRunning) {
        startWork();
    }
}

void USRPOutputThread::readBackSampleRate()
{
    uhd::usrp::multi_usrp::sptr device = m_deviceParams ? m_deviceParams->getDevice() : nullptr;

    if (!device) {
        return;
    }

    QMutexLocker mutexLocker(&m_deviceParams->m_mutex);

    try
    {
        double sampleRate = DeviceUSRP::getSampleRate(device, false, m_channel);
        double masterClockRate = device->get_master_clock_rate();

        if (m_reportQueue) {
            m_reportQueue->push(DeviceUSRPShared::MsgReportDeviceSettings::create(true, sampleRate, masterClockRate, false, "", false));
        }
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutputThread::readBackSampleRate: exception: " << e.what();
    }
}

// Apply settings to the device and send what was actually set to reportQueue.
// applyChannelSettings - apply settings that require the channel to be acquired. If false, only GPIO settings are applied.
// applyBandwidth - apply LPF bandwidth, which shouldn't be applied until after the stream is created
void USRPOutputThread::applyDeviceSettings(
    DeviceUSRPParams *deviceParams,
    int channel,
    const USRPOutputSettings& settings,
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
            qWarning() << "USRPOutputThread::applyDeviceSettings: failed to set" << what << ":" << e.what();
        }
    };

    if (applyChannelSettings)
    {
        if (settingsKeys.contains("clockSource") || force)
        {
            try
            {
                device->set_clock_source(settings.m_clockSource.toStdString(), 0);
                qDebug() << "USRPOutputThread::applyDeviceSettings: clock set to " << settings.m_clockSource;
            }
            catch (std::exception &e)
            {
                // An exception will be thrown if the clock is not detected
                // however, get_clock_source called below will still say the clock has is set
                qCritical() << "USRPOutputThread::applyDeviceSettings: could not set clock " << settings.m_clockSource;
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
                DeviceUSRP::setSampleRate(device, false, channel, settings.m_devSampleRate);
                qDebug("USRPOutputThread::applyDeviceSettings: sample rate set to %d", settings.m_devSampleRate);
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
                device->set_tx_freq(tuneRequest, channel);
                qDebug("USRPOutputThread::applyDeviceSettings: frequency set to %lld with LO offset %d", deviceCenterFrequency, settings.m_loOffset);
            });
        }

        if (settingsKeys.contains("gain") || force) {
            apply("gain", [&]() { device->set_tx_gain(settings.m_gain, channel); });
        }

        if (applyBandwidth && (settingsKeys.contains("lpfBW") || force))
        {
            apply("LPF bandwidth", [&]() {
                device->set_tx_bandwidth(settings.m_lpfBW, channel);
                qDebug("USRPOutputThread::applyDeviceSettings: LPF BW: %f for channel %d", settings.m_lpfBW, channel);
            });
        }

        if (settingsKeys.contains("antennaPath") || force)
        {
            apply("antenna", [&]() {
                device->set_tx_antenna(settings.m_antennaPath.toStdString(), channel);
                qDebug("USRPOutputThread::applyDeviceSettings: antenna set to %s on channel %d", qPrintable(settings.m_antennaPath), channel);
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
                qDebug() << "USRPOutputThread::applyDeviceSettings: set GPIO dir to" << settings.m_gpioDir;
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
                qDebug() << "USRPOutputThread::applyDeviceSettings: set GPIO pins to" << settings.m_gpioPins;
            }
        });
    }

    if (reapplySomeSettings && applyBandwidth)
    {
        // Need to re-set bandwidth (and AGC / gain for Rx) after changing sample rate or clock source
        apply("LPF bandwidth", [&]() { device->set_tx_bandwidth(settings.m_lpfBW, channel); });
    }

    // Report what was actually set, so USRPOutput can update its settings and inform buddies and GUI
    double sampleRate = 0.0;
    double masterClockRate = 0.0;
    QString clockSource;

    if (checkRates)
    {
        try
        {
            // Check if requested rate could actually be met and what master clock rate we ended up with
            sampleRate = DeviceUSRP::getSampleRate(device, false, channel);
            masterClockRate = device->get_master_clock_rate();
            qDebug("USRPOutputThread::applyDeviceSettings: actual sample rate %f master_clock_rate %f", sampleRate, masterClockRate);
        }
        catch (std::exception &e)
        {
            qDebug() << "USRPOutputThread::applyDeviceSettings: could not get sample rate: " << e.what();
            checkRates = false;
        }
    }

    if (checkClockSource)
    {
        try
        {
            clockSource = QString::fromStdString(device->get_clock_source(0));
            qDebug() << "USRPOutputThread::applyDeviceSettings: clock source is " << clockSource;
        }
        catch (std::exception &e)
        {
            qDebug() << "USRPOutputThread::applyDeviceSettings: could not get clock source: " << e.what();
            checkClockSource = false;
        }
    }

    if ((checkRates || checkClockSource) && reportQueue) {
        reportQueue->push(DeviceUSRPShared::MsgReportDeviceSettings::create(checkRates, sampleRate, masterClockRate, checkClockSource, clockSource, true));
    }
}
