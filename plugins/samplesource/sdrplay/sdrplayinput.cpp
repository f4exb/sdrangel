///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2018 beta-tester <alpha-beta-release@gmx.net>                   //
// Copyright (C) 2022 ericek111 <erik.brocko@letemsvetemapplem.eu>               //
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
#include <limits>

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGSDRPlayReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "sdrplayinput.h"

#include <device/deviceapi.h>

#include "sdrplaythread.h"

MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgConfigureSDRPlay, Message)
MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgReportSDRPlayGains, Message)
MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgReportSDRPlayCapabilities, Message)
MESSAGE_CLASS_DEFINITION(SDRPlayInput::MsgStartStop, Message)

SDRPlayInput::SDRPlayInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_variant(SDRPlayUndef),
    m_boardName("Unknown"),
    m_tunerCount(1),
    m_frontendCaps(),
    m_settings(),
	m_dev(0),
    m_sdrPlayThread(nullptr),
    m_deviceDescription("SDRPlay"),
    m_devNumber(0),
    m_streamSampleRate(0),
    m_running(false)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    openDevice();
    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SDRPlayInput::networkManagerFinished
    );
}

SDRPlayInput::~SDRPlayInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SDRPlayInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void SDRPlayInput::destroy()
{
    delete this;
}

void SDRPlayInput::setMessageQueueToGUI(MessageQueue *queue)
{
    m_guiMessageQueue = queue;
    reportCapabilities();
    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureSDRPlay::create(m_settings, QList<QString>(), true));
    }
}

void SDRPlayInput::updateCapabilities(unsigned int tunerIndex)
{
    memset(&m_frontendCaps, 0, sizeof(m_frontendCaps));
    m_frontendCaps.antenna_hiz = -1;

    if (m_dev && mirisdr_get_frontend_caps(m_dev, tunerIndex, &m_frontendCaps) < 0) {
        qWarning("SDRPlayInput::updateCapabilities: cannot read tuner %u capabilities", tunerIndex);
    }
}

void SDRPlayInput::sanitizeFrontEndSettings(SDRPlaySettings& settings) const
{
    if (settings.m_antenna < MIRISDR_ANTENNA_A ||
        settings.m_antenna > MIRISDR_ANTENNA_TEST ||
        !(m_frontendCaps.antennas & (1U << settings.m_antenna)))
    {
        settings.m_antenna = MIRISDR_ANTENNA_A;
        for (int antenna = MIRISDR_ANTENNA_A; antenna <= MIRISDR_ANTENNA_TEST; ++antenna) {
            if (m_frontendCaps.antennas & (1U << antenna)) {
                settings.m_antenna = antenna;
                break;
            }
        }
    }

    bool validAttenuator = false;
    for (int i = 0; i < m_frontendCaps.attenuator_count; ++i) {
        validAttenuator = validAttenuator || settings.m_attenuator == m_frontendCaps.attenuator[i];
    }
    if (!validAttenuator) {
        settings.m_attenuator = m_frontendCaps.attenuator_count ? m_frontendCaps.attenuator[0] : 0;
    }

    settings.m_notchMask &= m_frontendCaps.notches;
    settings.m_frontEndFlags &= m_frontendCaps.flags;
    settings.m_biasT = settings.m_biasT && m_frontendCaps.bias;
    settings.m_directSampling = m_frontendCaps.direct_sampling ? settings.m_directSampling : MIRISDR_DS_OFF;
    if (settings.m_directSampling != MIRISDR_DS_OFF &&
        settings.m_directSampling != MIRISDR_DS_ON &&
        settings.m_directSampling != MIRISDR_DS_AUTO) {
        settings.m_directSampling = MIRISDR_DS_OFF;
    }
    settings.m_amplifierBypass = settings.m_amplifierBypass && m_frontendCaps.amp_bypass_hz;
}

qint64 SDRPlayInput::calculateDeviceCenterFrequency(const SDRPlaySettings& settings, quint64 centerFrequency) const
{
    const uint32_t sampleRate = m_streamSampleRate ?
        m_streamSampleRate : SDRPlaySampleRates::getRate(settings.m_devSampleRateIndex);

    return DeviceSampleSource::calculateDeviceCenterFrequency(
        centerFrequency,
        0,
        settings.m_log2Decim,
        (DeviceSampleSource::fcPos_t) settings.m_fcPos,
        sampleRate,
        DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
        false);
}

qint64 SDRPlayInput::calculateTunerLO(const SDRPlaySettings& settings, quint64 centerFrequency) const
{
    const qint64 deviceCenterFrequency = calculateDeviceCenterFrequency(settings, centerFrequency);
    const qint64 df = (deviceCenterFrequency * settings.m_LOppmTenths) / 10000000LL;
    const qint64 correctedFrequency = qBound<qint64>(
        1,
        deviceCenterFrequency + df,
        std::numeric_limits<uint32_t>::max());

    return correctedFrequency + SDRPlayIF::getIF(settings.m_ifFrequencyIndex);
}

bool SDRPlayInput::sanitizeCenterFrequency(SDRPlaySettings& settings) const
{
    if (m_frontendCaps.range_count <= 0) {
        return false;
    }

    static constexpr qint64 hiZMaximumLO = 30000000;
    const bool hiZSelected = m_frontendCaps.antenna_hiz >= 0 &&
        settings.m_antenna == m_frontendCaps.antenna_hiz;
    const quint64 originalCenterFrequency = settings.m_centerFrequency;
    const qint64 originalLO = calculateTunerLO(settings, originalCenterFrequency);

    for (int i = 0; i < m_frontendCaps.range_count; ++i)
    {
        const qint64 rangeHigh = hiZSelected ?
            std::min<qint64>(m_frontendCaps.range[i][1], hiZMaximumLO) :
            m_frontendCaps.range[i][1];

        if (originalLO >= m_frontendCaps.range[i][0] && originalLO < rangeHigh) {
            return false;
        }
    }

    // Find the logical center-frequency interval corresponding to each valid LO
    // interval. The mapping is monotonic but includes decimation placement, PPM
    // correction and the selected low IF, so use a lower-bound search rather
    // than duplicating and inverting that arithmetic.
    const quint64 searchEnd = static_cast<quint64>(std::numeric_limits<uint32_t>::max()) + 1U;
    const auto lowerBoundForLO = [this, &settings, searchEnd](quint64 targetLO) {
        quint64 low = 0;
        quint64 high = searchEnd;

        while (low < high)
        {
            const quint64 middle = low + (high - low) / 2U;
            if (calculateTunerLO(settings, middle) < static_cast<qint64>(targetLO)) {
                low = middle + 1U;
            } else {
                high = middle;
            }
        }

        return low;
    };

    bool found = false;
    quint64 nearestCenterFrequency = originalCenterFrequency;
    quint64 nearestDistance = std::numeric_limits<quint64>::max();

    for (int i = 0; i < m_frontendCaps.range_count; ++i)
    {
        const quint64 rangeLow = m_frontendCaps.range[i][0];
        const quint64 rangeHigh = hiZSelected ?
            std::min<quint64>(m_frontendCaps.range[i][1], hiZMaximumLO) :
            m_frontendCaps.range[i][1];

        if (rangeHigh <= rangeLow) {
            continue;
        }

        const quint64 firstCenter = lowerBoundForLO(rangeLow);
        const quint64 afterLastCenter = lowerBoundForLO(rangeHigh);

        if (firstCenter >= searchEnd || firstCenter >= afterLastCenter) {
            continue;
        }

        const quint64 lastCenter = afterLastCenter - 1U;
        const quint64 candidate = qBound(firstCenter, originalCenterFrequency, lastCenter);
        const qint64 candidateLO = calculateTunerLO(settings, candidate);

        if (candidateLO < static_cast<qint64>(rangeLow) || candidateLO >= static_cast<qint64>(rangeHigh)) {
            continue;
        }

        const quint64 distance = candidate > originalCenterFrequency ?
            candidate - originalCenterFrequency : originalCenterFrequency - candidate;

        if (!found || distance < nearestDistance)
        {
            found = true;
            nearestCenterFrequency = candidate;
            nearestDistance = distance;
        }
    }

    if (!found) {
        qWarning("SDRPlayInput::sanitizeCenterFrequency: no usable center frequency for the current tuning settings");
        return false;
    }

    settings.m_centerFrequency = nearestCenterFrequency;
    qWarning("SDRPlayInput::sanitizeCenterFrequency: adjusted center frequency from %llu to %llu Hz",
        static_cast<unsigned long long>(originalCenterFrequency),
        static_cast<unsigned long long>(nearestCenterFrequency));
    return true;
}

void SDRPlayInput::reportCapabilities()
{
    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgReportSDRPlayCapabilities::create(
            m_boardName,
            m_tunerCount,
            m_settings.m_tunerIndex,
            m_frontendCaps));
    }
}

bool SDRPlayInput::configureStream()
{
    mirisdr_stream_config_t config;
    mirisdr_stream_result_t result;
    mirisdr_stream_config_default(&config);
    memset(&result, 0, sizeof(result));

    config.rate = SDRPlaySampleRates::getRate(m_settings.m_devSampleRateIndex);
    config.transfer = "BULK";
    config.gap_fill = 1;
    config.follow_tune = 1;
    config.baseband = 1;
    config.tuners = 1 << m_settings.m_tunerIndex;

    const int res = mirisdr_set_stream(m_dev, &config, &result);

    if (res < 0)
    {
        qCritical("SDRPlayInput::configureStream: could not configure %u S/s baseband stream on tuner %u",
            config.rate, m_settings.m_tunerIndex);
        return false;
    }

    qDebug("SDRPlayInput::configureStream: rate %u S/s, ADC rate %u S/s, format %s, tuner mask 0x%x",
        result.rate,
        result.adc_rate,
        result.format ? result.format : "unknown",
        result.tuners);
    m_streamSampleRate = result.rate;
    return true;
}

bool SDRPlayInput::openDevice()
{
    m_devNumber = m_deviceAPI->getSamplingDeviceSequence();

    if (m_dev != 0)
    {
        closeDevice();
    }

    int res;
    //int numberOfGains;

    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("SDRPlayInput::openDevice: could not allocate SampleFifo");
        return false;
    }

    mirisdr_open_config_t openConfig;
    mirisdr_open_config_default(&openConfig);
    openConfig.index = m_devNumber;
    openConfig.hw_flavour = MIRISDR_HW_AUTO;

    if ((res = mirisdr_open_ex(&m_dev, &openConfig)) < 0)
    {
        qCritical("SDRPlayInput::openDevice: could not open SDRPlay #%d: %s", m_devNumber, strerror(errno));
        return false;
    }

    char vendor[256];
    char product[256];
    char serial[256];

    vendor[0] = '\0';
    product[0] = '\0';
    serial[0] = '\0';

    if ((res = mirisdr_get_device_usb_strings(m_devNumber, vendor, product, serial)) < 0)
    {
        qCritical("SDRPlayInput::openDevice: error accessing USB device");
        stop();
        return false;
    }

    qWarning("SDRPlayInput::openDevice: %s %s, SN: %s", vendor, product, serial);
    m_deviceDescription = QString("%1 (SN %2)").arg(product).arg(serial);

    const char *boardName = mirisdr_get_board_name(m_dev);
    m_boardName = boardName ? QString::fromUtf8(boardName) : QString::fromUtf8(product);
    const int hwFlavour = mirisdr_get_hw_flavour(m_dev);

    if (hwFlavour == MIRISDR_HW_RSP1A) {
        m_variant = SDRPlayRSP1A;
    } else if (hwFlavour == MIRISDR_HW_RSP1B) {
        m_variant = SDRPlayRSP1B;
    } else if (hwFlavour == MIRISDR_HW_RSP2) {
        m_variant = SDRPlayRSP2;
    } else if (hwFlavour == MIRISDR_HW_RSPDUO) {
        m_variant = SDRPlayRSPduo;
    } else if (hwFlavour == MIRISDR_HW_RSPDXR2) {
        m_variant = SDRPlayRSPdxR2;
    } else if (hwFlavour == MIRISDR_HW_MIRICSTV) {
        m_variant = SDRPlayMiricsTV;
    } else {
        m_variant = SDRPlayRSP1;
    }

    m_tunerCount = mirisdr_get_tuner_count(m_dev);
    if (m_tunerCount < 1) {
        m_tunerCount = 1;
    }
    if (m_settings.m_tunerIndex >= static_cast<uint32_t>(m_tunerCount)) {
        m_settings.m_tunerIndex = 0;
    }
    updateCapabilities(m_settings.m_tunerIndex);
    sanitizeFrontEndSettings(m_settings);
    sanitizeCenterFrequency(m_settings);

    qDebug("SDRPlayInput::openDevice: board: %s, flavour: %d, tuners: %d",
        qPrintable(m_boardName), hwFlavour, m_tunerCount);

    return true;
}

bool SDRPlayInput::start()
{
    QMutexLocker mutexLocker(&m_mutex);
    int res;

    if (m_running) {
        return true;
    }

    if (!m_dev) {
        return false;
    }

    if (!configureStream()) {
        return false;
    }

	if ((res = mirisdr_reset_buffer(m_dev)) < 0)
	{
		qCritical("SDRPlayInput::start: could not reset USB EP buffers: %s", strerror(errno));
		stop();
		return false;
	}

    m_sdrPlayThread = new SDRPlayThread(m_dev, &m_sampleFifo);
    m_sdrPlayThread->setLog2Decimation(m_settings.m_log2Decim);
    m_sdrPlayThread->setFcPos((int) m_settings.m_fcPos);
    m_sdrPlayThread->setIQOrder(m_settings.m_iqOrder);
    if (!m_sdrPlayThread->startWork())
    {
        delete m_sdrPlayThread;
        m_sdrPlayThread = nullptr;
        return false;
    }
    m_running = true;

	mutexLocker.unlock();
	applySettings(m_settings, QList<QString>(), true, true);

	return true;
}

void SDRPlayInput::closeDevice()
{
    if (m_dev != 0)
    {
        mirisdr_close(m_dev);
        m_dev = 0;
        m_streamSampleRate = 0;
    }

    m_deviceDescription.clear();
}

void SDRPlayInput::init()
{
    applySettings(m_settings, QList<QString>(), true, true);
}

void SDRPlayInput::stop()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

    m_running = false;

    if(m_sdrPlayThread)
    {
        m_sdrPlayThread->stopWork();
        delete m_sdrPlayThread;
        m_sdrPlayThread = nullptr;
    }
}

QByteArray SDRPlayInput::serialize() const
{
    return m_settings.serialize();
}

bool SDRPlayInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureSDRPlay* message = MsgConfigureSDRPlay::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSDRPlay* messageToGUI = MsgConfigureSDRPlay::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& SDRPlayInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SDRPlayInput::getSampleRate() const
{
    int rate = m_streamSampleRate ? m_streamSampleRate : SDRPlaySampleRates::getRate(m_settings.m_devSampleRateIndex);
    return rate / (1<<m_settings.m_log2Decim);
}

quint64 SDRPlayInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void SDRPlayInput::setCenterFrequency(qint64 centerFrequency)
{
    SDRPlaySettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureSDRPlay* message = MsgConfigureSDRPlay::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSDRPlay* messageToGUI = MsgConfigureSDRPlay::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool SDRPlayInput::handleMessage(const Message& message)
{
    if (MsgConfigureSDRPlay::match(message))
    {
        MsgConfigureSDRPlay& conf = (MsgConfigureSDRPlay&) message;
        qDebug() << "SDRPlayInput::handleMessage: MsgConfigureSDRPlay";

        // A sample-rate or RSPduo tuner change needs the stream to be reconfigured.
        const bool streamConfigChanged =
            conf.getSettingsKeys().contains("devSampleRateIndex") ||
            conf.getSettingsKeys().contains("tunerIndex") ||
            (conf.getForce() &&
                (conf.getSettings().m_devSampleRateIndex != m_settings.m_devSampleRateIndex ||
                 conf.getSettings().m_tunerIndex != m_settings.m_tunerIndex));
        if ((m_dev != 0) && streamConfigChanged)
        {
            const bool wasRunning = m_running;
            if (conf.getForce()) {
                m_settings = conf.getSettings();
            } else {
                m_settings.applySettings(conf.getSettingsKeys(), conf.getSettings());
            }

            if (m_settings.m_tunerIndex >= static_cast<uint32_t>(m_tunerCount)) {
                m_settings.m_tunerIndex = 0;
            }
            m_streamSampleRate = 0;
            updateCapabilities(m_settings.m_tunerIndex);
            sanitizeFrontEndSettings(m_settings);
            sanitizeCenterFrequency(m_settings);
            reportCapabilities();
            if (m_guiMessageQueue) {
                m_guiMessageQueue->push(MsgConfigureSDRPlay::create(m_settings, QList<QString>(), true));
            }
            if (wasRunning) {
                stop();
                start();
            } else {
                applySettings(m_settings, QList<QString>(), true, true);
            }
        }
        // standard changes
        else
        {
            if (!applySettings(conf.getSettings(), conf.getSettingsKeys(), false, conf.getForce())) {
                qDebug("SDRPlayInput::handleMessage: config error");
            }
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SDRPlayInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

bool SDRPlayInput::applySettings(const SDRPlaySettings& settings, const QList<QString>& settingsKeys, bool forwardChange, bool force)
{
    qDebug() << "SDRPlayInput::applySettings: forwardChange: " << forwardChange << " force: " << force << settings.getDebugString(settingsKeys, force);
    QMutexLocker mutexLocker(&m_mutex);
    SDRPlaySettings requested = m_settings;

    if (force) {
        requested = settings;
    } else {
        requested.applySettings(settingsKeys, settings);
    }

    if (requested.m_tunerIndex >= static_cast<uint32_t>(m_tunerCount)) {
        requested.m_tunerIndex = 0;
    }
    sanitizeFrontEndSettings(requested);
    const bool centerFrequencyAdjusted = sanitizeCenterFrequency(requested);
    QList<QString> appliedSettingsKeys = settingsKeys;
    if (centerFrequencyAdjusted && !appliedSettingsKeys.contains("centerFrequency")) {
        appliedSettingsKeys.append("centerFrequency");
    }

    if (settingsKeys.contains("dcBlock") || settingsKeys.contains("iqCorrection") || force) {
        m_deviceAPI->configureCorrections(requested.m_dcBlock, requested.m_iqCorrection);
    }

    if (settingsKeys.contains("log2Decim") || force)
    {
        if (m_sdrPlayThread)
        {
            m_sdrPlayThread->setLog2Decimation(requested.m_log2Decim);
            qDebug() << "SDRPlayInput::applySettings: set decimation to " << (1<<requested.m_log2Decim);
        }
    }

    if (settingsKeys.contains("fcPos") || force)
    {
        if (m_sdrPlayThread)
        {
            m_sdrPlayThread->setFcPos((int) requested.m_fcPos);
            qDebug() << "SDRPlayInput: set fc pos (enum) to " << (int) requested.m_fcPos;
        }
    }

    if (settingsKeys.contains("iqOrder") || force)
    {
        if (m_sdrPlayThread)
        {
            m_sdrPlayThread->setIQOrder((int) requested.m_iqOrder);
            qDebug() << "SDRPlayInput: set IQ order to " << (requested.m_iqOrder ? "IQ" : "QI");
        }
    }

    const QStringList tunerKeys{
        "centerFrequency", "LOppmTenths", "fcPos", "log2Decim", "bandwidthIndex",
        "ifFrequencyIndex", "tunerGainMode", "tunerGain", "lnaOn", "mixerAmpOn",
        "basebandGain", "antenna", "biasT", "notchMask", "directSampling",
        "attenuator", "frontEndFlags", "amplifierBypass"
    };
    bool tuneNeeded = force;
    for (const QString& key : tunerKeys) {
        tuneNeeded = tuneNeeded || appliedSettingsKeys.contains(key);
    }

    bool success = true;
    if (tuneNeeded && m_dev)
    {
        const qint64 deviceCenterFrequency = calculateDeviceCenterFrequency(requested, requested.m_centerFrequency);
        success = applyTunerSettings(requested, deviceCenterFrequency);
        forwardChange = true;
    }

    if (requested.m_useReverseAPI)
    {
        bool fullUpdate = (appliedSettingsKeys.contains("useReverseAPI") && requested.m_useReverseAPI) ||
            appliedSettingsKeys.contains("reverseAPIAddress") ||
            appliedSettingsKeys.contains("reverseAPIPort") ||
            appliedSettingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(appliedSettingsKeys, requested, fullUpdate || force);
    }

    m_settings = requested;

    if (centerFrequencyAdjusted && m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureSDRPlay::create(
            m_settings, QList<QString>{"centerFrequency"}, false));
    }

    if (forwardChange)
    {
        int sampleRate = getSampleRate();
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    return success;
}

bool SDRPlayInput::applyTunerSettings(const SDRPlaySettings& settings, qint64 deviceCenterFrequency)
{
    mirisdr_tune_config_t config;
    mirisdr_tune_result_t result;
    memset(&result, 0, sizeof(result));

    if (mirisdr_get_tune(m_dev, settings.m_tunerIndex, &config, nullptr) < 0) {
        mirisdr_tune_config_default(&config);
    }

    const qint64 df = (deviceCenterFrequency * settings.m_LOppmTenths) / 10000000LL;
    const qint64 correctedFrequency = qBound<qint64>(
        1,
        deviceCenterFrequency + df,
        std::numeric_limits<uint32_t>::max());
    config.frequency = static_cast<uint32_t>(correctedFrequency);
    config.bandwidth = SDRPlayBandwidths::getBandwidth(settings.m_bandwidthIndex);
    config.if_freq = SDRPlayIF::getIF(settings.m_ifFrequencyIndex);
    config.low_if_auto = config.if_freq != 0;
    config.iq = MIRISDR_IQ_BOTH;
    config.gain.mode = settings.m_tunerGainMode ? MIRISDR_GAIN_TOTAL : MIRISDR_GAIN_STAGES;
    config.gain.total = settings.m_tunerGain;
    config.gain.lna = settings.m_lnaOn ? 1 : 0;
    config.gain.mixer = settings.m_mixerAmpOn ? 1 : 0;
    config.gain.mixbuffer = settings.m_lnaOn ? 24 : 0;
    config.gain.baseband = settings.m_basebandGain;
    config.gain.amp_bypass = settings.m_amplifierBypass ? 1 : 0;
    config.frontend.antenna = settings.m_antenna;
    config.frontend.bias = settings.m_biasT ? 1 : 0;
    config.frontend.notch = settings.m_notchMask;
    config.frontend.direct_sampling = settings.m_directSampling;
    config.frontend.attenuator = settings.m_attenuator;
    config.frontend.flags = settings.m_frontEndFlags;
    config.frontend.raw.fields = 0;
    config.strict = 0;

    if (mirisdr_tune(m_dev, settings.m_tunerIndex, &config, &result) < 0)
    {
        qCritical("SDRPlayInput::applyTunerSettings: tune failed for tuner %u at %u Hz",
            settings.m_tunerIndex, config.frequency);
        return false;
    }

    mirisdr_stream_result_t streamResult;
    memset(&streamResult, 0, sizeof(streamResult));
    if (m_streamSampleRate && mirisdr_get_stream(m_dev, nullptr, &streamResult) == 0) {
        m_streamSampleRate = streamResult.rate;
    }

    qDebug("SDRPlayInput::applyTunerSettings: tuner %u, LO %u Hz, bandwidth %u Hz, antenna %d, attenuation %d dB",
        settings.m_tunerIndex, result.lo, result.bandwidth, result.frontend.antenna, result.frontend.attenuator);

    if (getMessageQueueToGUI())
    {
        const int frontGain = (result.band == MIRISDR_BAND_AM1 || result.band == MIRISDR_BAND_AM2) ?
            result.gain.mixbuffer : result.gain.lna;
        getMessageQueueToGUI()->push(MsgReportSDRPlayGains::create(
            frontGain,
            result.gain.mixer,
            result.gain.baseband,
            result.gain.total));
    }

    return true;
}

bool SDRPlayInput::setDeviceCenterFrequency(quint64 freq_hz)
{
    qint64 df = ((qint64)freq_hz * m_settings.m_LOppmTenths) / 10000000LL;
    freq_hz += df;

    int r = mirisdr_set_center_freq(m_dev, static_cast<uint32_t>(freq_hz));

    if (r != 0)
    {
        qWarning("SDRPlayInput::setDeviceCenterFrequency: could not frequency to %llu Hz", freq_hz);
        return false;
    }
    else
    {
        qWarning("SDRPlayInput::setDeviceCenterFrequency: frequency set to %llu Hz", freq_hz);
        return true;
    }
}

int SDRPlayInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int SDRPlayInput::webapiRun(
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

int SDRPlayInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setSdrPlaySettings(new SWGSDRangel::SWGSDRPlaySettings());
    response.getSdrPlaySettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int SDRPlayInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    SDRPlaySettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureSDRPlay *msg = MsgConfigureSDRPlay::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSDRPlay *msgToGUI = MsgConfigureSDRPlay::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void SDRPlayInput::webapiUpdateDeviceSettings(
        SDRPlaySettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("title")) {
        settings.m_title = *response.getSdrPlaySettings()->getTitle();
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getSdrPlaySettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("tunerGain")) {
        settings.m_tunerGain = response.getSdrPlaySettings()->getTunerGain();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getSdrPlaySettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("frequencyBandIndex")) {
        settings.m_frequencyBandIndex = response.getSdrPlaySettings()->getFrequencyBandIndex();
    }
    if (deviceSettingsKeys.contains("ifFrequencyIndex")) {
        settings.m_ifFrequencyIndex = response.getSdrPlaySettings()->getIfFrequencyIndex();
    }
    if (deviceSettingsKeys.contains("bandwidthIndex")) {
        settings.m_bandwidthIndex = response.getSdrPlaySettings()->getBandwidthIndex();
    }
    if (deviceSettingsKeys.contains("devSampleRateIndex")) {
        settings.m_devSampleRateIndex = response.getSdrPlaySettings()->getDevSampleRateIndex();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getSdrPlaySettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getSdrPlaySettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("fcPos"))
    {
        int fcPos = response.getSdrPlaySettings()->getFcPos();
        fcPos = fcPos < 0 ? 0 : fcPos > 2 ? 2 : fcPos;
        settings.m_fcPos = (SDRPlaySettings::fcPos_t) fcPos;
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getSdrPlaySettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getSdrPlaySettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("tunerGainMode")) {
        settings.m_tunerGainMode = response.getSdrPlaySettings()->getTunerGainMode() != 0;
    }
    if (deviceSettingsKeys.contains("lnaOn")) {
        settings.m_lnaOn = response.getSdrPlaySettings()->getLnaOn() != 0;
    }
    if (deviceSettingsKeys.contains("mixerAmpOn")) {
        settings.m_mixerAmpOn = response.getSdrPlaySettings()->getMixerAmpOn() != 0;
    }
    if (deviceSettingsKeys.contains("basebandGain")) {
        settings.m_basebandGain = response.getSdrPlaySettings()->getBasebandGain();
    }
    if (deviceSettingsKeys.contains("tunerIndex")) {
        settings.m_tunerIndex = response.getSdrPlaySettings()->getTunerIndex();
    }
    if (deviceSettingsKeys.contains("antenna")) {
        settings.m_antenna = response.getSdrPlaySettings()->getAntenna();
    }
    if (deviceSettingsKeys.contains("biasT")) {
        settings.m_biasT = response.getSdrPlaySettings()->getBiasT() != 0;
    }
    if (deviceSettingsKeys.contains("notchMask")) {
        settings.m_notchMask = response.getSdrPlaySettings()->getNotchMask();
    }
    if (deviceSettingsKeys.contains("directSampling")) {
        settings.m_directSampling = response.getSdrPlaySettings()->getDirectSampling();
    }
    if (deviceSettingsKeys.contains("attenuator")) {
        settings.m_attenuator = response.getSdrPlaySettings()->getAttenuator();
    }
    if (deviceSettingsKeys.contains("frontEndFlags")) {
        settings.m_frontEndFlags = response.getSdrPlaySettings()->getFrontEndFlags();
    }
    if (deviceSettingsKeys.contains("amplifierBypass")) {
        settings.m_amplifierBypass = response.getSdrPlaySettings()->getAmplifierBypass() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSdrPlaySettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSdrPlaySettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSdrPlaySettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getSdrPlaySettings()->getReverseApiDeviceIndex();
    }
}

void SDRPlayInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SDRPlaySettings& settings)
{
    if (response.getSdrPlaySettings()->getTitle()) {
        *response.getSdrPlaySettings()->getTitle() = settings.m_title;
    } else {
        response.getSdrPlaySettings()->setTitle(new QString(settings.m_title));
    }

    response.getSdrPlaySettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getSdrPlaySettings()->setTunerGain(settings.m_tunerGain);
    response.getSdrPlaySettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getSdrPlaySettings()->setFrequencyBandIndex(settings.m_frequencyBandIndex);
    response.getSdrPlaySettings()->setIfFrequencyIndex(settings.m_ifFrequencyIndex);
    response.getSdrPlaySettings()->setBandwidthIndex(settings.m_bandwidthIndex);
    response.getSdrPlaySettings()->setDevSampleRateIndex(settings.m_devSampleRateIndex);
    response.getSdrPlaySettings()->setLog2Decim(settings.m_log2Decim);
    response.getSdrPlaySettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getSdrPlaySettings()->setFcPos((int) settings.m_fcPos);
    response.getSdrPlaySettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getSdrPlaySettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getSdrPlaySettings()->setTunerGainMode((int) settings.m_tunerGainMode);
    response.getSdrPlaySettings()->setLnaOn(settings.m_lnaOn ? 1 : 0);
    response.getSdrPlaySettings()->setMixerAmpOn(settings.m_mixerAmpOn ? 1 : 0);
    response.getSdrPlaySettings()->setBasebandGain(settings.m_basebandGain);
    response.getSdrPlaySettings()->setTunerIndex(settings.m_tunerIndex);
    response.getSdrPlaySettings()->setAntenna(settings.m_antenna);
    response.getSdrPlaySettings()->setBiasT(settings.m_biasT ? 1 : 0);
    response.getSdrPlaySettings()->setNotchMask(settings.m_notchMask);
    response.getSdrPlaySettings()->setDirectSampling(settings.m_directSampling);
    response.getSdrPlaySettings()->setAttenuator(settings.m_attenuator);
    response.getSdrPlaySettings()->setFrontEndFlags(settings.m_frontEndFlags);
    response.getSdrPlaySettings()->setAmplifierBypass(settings.m_amplifierBypass ? 1 : 0);

    response.getSdrPlaySettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSdrPlaySettings()->getReverseApiAddress()) {
        *response.getSdrPlaySettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSdrPlaySettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSdrPlaySettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSdrPlaySettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int SDRPlayInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSdrPlayReport(new SWGSDRangel::SWGSDRPlayReport());
    response.getSdrPlayReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void SDRPlayInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getSdrPlayReport()->setSampleRates(new QList<SWGSDRangel::SWGSampleRate*>);

    for (unsigned int i = 0; i < SDRPlaySampleRates::getNbRates(); i++)
    {
        response.getSdrPlayReport()->getSampleRates()->append(new SWGSDRangel::SWGSampleRate);
        response.getSdrPlayReport()->getSampleRates()->back()->setRate(SDRPlaySampleRates::getRate(i));
    }

    response.getSdrPlayReport()->setIntermediateFrequencies(new QList<SWGSDRangel::SWGFrequency*>);

    for (unsigned int i = 0; i < SDRPlayIF::getNbIFs(); i++)
    {
        response.getSdrPlayReport()->getIntermediateFrequencies()->append(new SWGSDRangel::SWGFrequency);
        response.getSdrPlayReport()->getIntermediateFrequencies()->back()->setFrequency(SDRPlayIF::getIF(i));
    }

    response.getSdrPlayReport()->setBandwidths(new QList<SWGSDRangel::SWGBandwidth*>);

    for (unsigned int i = 0; i < SDRPlayBandwidths::getNbBandwidths(); i++)
    {
        response.getSdrPlayReport()->getBandwidths()->append(new SWGSDRangel::SWGBandwidth);
        response.getSdrPlayReport()->getBandwidths()->back()->setBandwidth(SDRPlayBandwidths::getBandwidth(i));
    }

    response.getSdrPlayReport()->setFrequencyBands(new QList<SWGSDRangel::SWGFrequencyBand*>);

    for (unsigned int i = 0; i < SDRPlayBands::getNbBands(); i++)
    {
        response.getSdrPlayReport()->getFrequencyBands()->append(new SWGSDRangel::SWGFrequencyBand);
        if (response.getSdrPlayReport()->getFrequencyBands()->back()->getName()) {
            *response.getSdrPlayReport()->getFrequencyBands()->back()->getName() = SDRPlayBands::getBandName(i);
        } else {
            response.getSdrPlayReport()->getFrequencyBands()->back()->setName(new QString(SDRPlayBands::getBandName(i)));
        }
        response.getSdrPlayReport()->getFrequencyBands()->back()->setLowerBound(SDRPlayBands::getBandLow(i) * 1000);
        response.getSdrPlayReport()->getFrequencyBands()->back()->setHigherBound(SDRPlayBands::getBandHigh(i) * 1000);
    }
}

void SDRPlayInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const SDRPlaySettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SDRplay1"));
    swgDeviceSettings->setSdrPlaySettings(new SWGSDRangel::SWGSDRPlaySettings());
    SWGSDRangel::SWGSDRPlaySettings *swgSDRPlaySettings = swgDeviceSettings->getSdrPlaySettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("title") || force) {
        swgSDRPlaySettings->setTitle(new QString(settings.m_title));
    }
    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgSDRPlaySettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("tunerGain") || force) {
        swgSDRPlaySettings->setTunerGain(settings.m_tunerGain);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgSDRPlaySettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("frequencyBandIndex") || force) {
        swgSDRPlaySettings->setFrequencyBandIndex(settings.m_frequencyBandIndex);
    }
    if (deviceSettingsKeys.contains("ifFrequencyIndex") || force) {
        swgSDRPlaySettings->setIfFrequencyIndex(settings.m_ifFrequencyIndex);
    }
    if (deviceSettingsKeys.contains("bandwidthIndex") || force) {
        swgSDRPlaySettings->setBandwidthIndex(settings.m_bandwidthIndex);
    }
    if (deviceSettingsKeys.contains("devSampleRateIndex") || force) {
        swgSDRPlaySettings->setDevSampleRateIndex(settings.m_devSampleRateIndex);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgSDRPlaySettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgSDRPlaySettings->setIqOrder(settings.m_iqOrder);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgSDRPlaySettings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgSDRPlaySettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgSDRPlaySettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("tunerGainMode") || force) {
        swgSDRPlaySettings->setTunerGainMode(settings.m_tunerGainMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lnaOn") || force) {
        swgSDRPlaySettings->setLnaOn(settings.m_lnaOn ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("mixerAmpOn") || force) {
        swgSDRPlaySettings->setMixerAmpOn(settings.m_mixerAmpOn ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("basebandGain") || force) {
        swgSDRPlaySettings->setBasebandGain(settings.m_basebandGain);
    }
    if (deviceSettingsKeys.contains("tunerIndex") || force) {
        swgSDRPlaySettings->setTunerIndex(settings.m_tunerIndex);
    }
    if (deviceSettingsKeys.contains("antenna") || force) {
        swgSDRPlaySettings->setAntenna(settings.m_antenna);
    }
    if (deviceSettingsKeys.contains("biasT") || force) {
        swgSDRPlaySettings->setBiasT(settings.m_biasT ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("notchMask") || force) {
        swgSDRPlaySettings->setNotchMask(settings.m_notchMask);
    }
    if (deviceSettingsKeys.contains("directSampling") || force) {
        swgSDRPlaySettings->setDirectSampling(settings.m_directSampling);
    }
    if (deviceSettingsKeys.contains("attenuator") || force) {
        swgSDRPlaySettings->setAttenuator(settings.m_attenuator);
    }
    if (deviceSettingsKeys.contains("frontEndFlags") || force) {
        swgSDRPlaySettings->setFrontEndFlags(settings.m_frontEndFlags);
    }
    if (deviceSettingsKeys.contains("amplifierBypass") || force) {
        swgSDRPlaySettings->setAmplifierBypass(settings.m_amplifierBypass ? 1 : 0);
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

void SDRPlayInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SDRplay1"));

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

void SDRPlayInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SDRPlayInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SDRPlayInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

// ====================================================================

unsigned int SDRPlaySampleRates::m_rates[m_nb_rates] = {
        1536000,   // 0
        1792000,   // 1
        2000000,   // 2
        2048000,   // 3
        2304000,   // 4
        2400000,   // 5
        3072000,   // 6
        3200000,   // 7
        4000000,   // 8
        4096000,   // 9
        4608000,   // 10
        4800000,   // 11
        5000000,   // 12
        6000000,   // 13
        6144000,   // 14
        6400000,   // 15
        8000000,   // 16
        8192000,   // 17
        48000,     // 18 - libmirisdr-6 baseband output rates
        96000,     // 19
        192000,    // 20
        384000,    // 21
        768000,    // 22
        1024000,   // 23
        10000000,  // 24
        12000000,  // 25
        14000000,  // 26
        15000000,  // 27 - libmirisdr-6 baseband engine limit
};

unsigned int SDRPlaySampleRates::getRate(unsigned int rate_index)
{
    if (rate_index < m_nb_rates)
    {
        return m_rates[rate_index];
    }
    else
    {
        return m_rates[0];
    }
}

unsigned int SDRPlaySampleRates::getRateIndex(unsigned int rate)
{
    for (unsigned int i=0; i < m_nb_rates; i++)
    {
        if (rate == m_rates[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlaySampleRates::getNbRates()
{
    return SDRPlaySampleRates::m_nb_rates;
}

// ====================================================================

unsigned int SDRPlayBandwidths::m_bw[m_nb_bw] = {
        200000,    // 0
        300000,    // 1
        600000,    // 2
        1536000,   // 3
        5000000,   // 4
        6000000,   // 5
        7000000,   // 6
        8000000,   // 7
};

unsigned int SDRPlayBandwidths::getBandwidth(unsigned int bandwidth_index)
{
    if (bandwidth_index < m_nb_bw)
    {
        return m_bw[bandwidth_index];
    }
    else
    {
        return m_bw[0];
    }
}

unsigned int SDRPlayBandwidths::getBandwidthIndex(unsigned int bandwidth)
{
    for (unsigned int i=0; i < m_nb_bw; i++)
    {
        if (bandwidth == m_bw[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlayBandwidths::getNbBandwidths()
{
    return SDRPlayBandwidths::m_nb_bw;
}

// ====================================================================

unsigned int SDRPlayIF::m_if[m_nb_if] = {
        0,         // 0
        450000,    // 1
        1620000,   // 2
        2048000,   // 3
};

unsigned int SDRPlayIF::getIF(unsigned int if_index)
{
    if (if_index < m_nb_if)
    {
        return m_if[if_index];
    }
    else
    {
        return m_if[0];
    }
}

unsigned int SDRPlayIF::getIFIndex(unsigned int iff)
{
    for (unsigned int i=0; i < m_nb_if; i++)
    {
        if (iff == m_if[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlayIF::getNbIFs()
{
    return SDRPlayIF::m_nb_if;
}

// ====================================================================

/** Lower frequency bound in kHz inclusive */
unsigned int SDRPlayBands::m_bandLow[m_nb_bands] = {
        10,       // 0
        12000,    // 1
        30000,    // 2
        50000,    // 3
        120000,   // 4
        250000,   // 5
        380000,   // 6
        1000000,  // 7
};

/** Lower frequency bound in kHz exclusive */
unsigned int SDRPlayBands::m_bandHigh[m_nb_bands] = {
        12000,    // 0
        30000,    // 1
        50000,    // 2
        120000,   // 3
        250000,   // 4
        380000,   // 5
        1000000,  // 6
        2000000,  // 7
};

const char* SDRPlayBands::m_bandName[m_nb_bands] = {
        "10k-12M",    // 0
        "12-30M",     // 1
        "30-50M",     // 2
        "50-120M",    // 3
        "120-250M",   // 4
        "250-380M",   // 5
        "380M-1G",    // 6
        "1-2G",       // 7
};

QString SDRPlayBands::getBandName(unsigned int band_index)
{
    if (band_index < m_nb_bands)
    {
        return QString(m_bandName[band_index]);
    }
    else
    {
        return QString(m_bandName[0]);
    }
}

unsigned int SDRPlayBands::getBandLow(unsigned int band_index)
{
    if (band_index < m_nb_bands)
    {
        return m_bandLow[band_index];
    }
    else
    {
        return m_bandLow[0];
    }
}

unsigned int SDRPlayBands::getBandHigh(unsigned int band_index)
{
    if (band_index < m_nb_bands)
    {
        return m_bandHigh[band_index];
    }
    else
    {
        return m_bandHigh[0];
    }
}


unsigned int SDRPlayBands::getNbBands()
{
    return SDRPlayBands::m_nb_bands;
}
