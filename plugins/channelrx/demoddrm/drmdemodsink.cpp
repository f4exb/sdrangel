///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include "drmdemodsink.h"

#include "dsp/datafifo.h"
#include "maincore.h"
#include "util/messagequeue.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "dsp/dsptypes.h"
#include "modemdrm/drmfacacquisition.h"
#include "modemdrm/drmparameters.h"
#include "modemdrm/drmreceiver.h"

void DRMDemodSink::enrichServices(std::vector<DRMServiceStatus>& services, const modemdrm::SDCData& sdc)
{
    for (DRMServiceStatus& service : services)
    {
        const auto label = std::find_if(
            sdc.labels.begin(), sdc.labels.end(),
            [&service](const modemdrm::SDCLabel& item) {
                return item.shortId == service.shortIdentifier;
            }
        );
        if (label != sdc.labels.end()) {
            service.label = QString::fromUtf8(label->utf8.data(), static_cast<int>(label->utf8.size()));
        }

        const auto locale = std::find_if(
            sdc.languageCountries.begin(), sdc.languageCountries.end(),
            [&service](const modemdrm::SDCLanguageCountry& item) {
                return item.shortId == service.shortIdentifier;
            }
        );
        if (locale != sdc.languageCountries.end())
        {
            service.language = QString::fromLatin1(
                locale->language.data(), static_cast<int>(locale->language.size())
            );
            service.country = QString::fromLatin1(
                locale->country.data(), static_cast<int>(locale->country.size())
            );
        }

        const auto audio = std::find_if(
            sdc.audio.begin(), sdc.audio.end(),
            [&service](const modemdrm::SDCAudioInformation& item) {
                return item.shortId == service.shortIdentifier
                    && !item.nextConfiguration;
            }
        );
        if (audio != sdc.audio.end())
        {
            service.streamId = audio->streamId;
            service.audioCoding = audio->audioCoding;
            service.audioMode = audio->audioMode;
            service.samplingRateCode = audio->samplingRateCode;
            service.sbr = audio->sbr;
            service.textFlag = audio->text;
        }
    }
}

std::string DRMDemodSink::mscConfigurationKey(const modemdrm::FACAcquisitionResult& fac, const modemdrm::SDCData& sdc
)
{
    if (fac.facBlocks.empty() || !sdc.multiplex.present) {
        return {};
    }

    const modemdrm::FACData& channel = fac.facBlocks.front();
    std::string key;
    key.reserve(96U);
    const auto append = [&key](int value) {
        key += std::to_string(value);
        key.push_back(',');
    };
    append(static_cast<int>(fac.coarse.mode));
    append(fac.occupancy);
    append(channel.mscMode);
    append(channel.shortInterleaving ? 1 : 0);
    append(sdc.multiplex.protectionLevelA);
    append(sdc.multiplex.protectionLevelB);
    for (const modemdrm::SDCStreamDescription& stream : sdc.multiplex.streams)
    {
        append(stream.streamId);
        append(stream.partALengthBytes);
        append(stream.partBLengthBytes);
    }

    return key;
}

std::string DRMDemodSink::audioConfigurationKey(const modemdrm::SDCData& sdc, int shortId)
{
    std::string key;
    const auto append = [&key](int value) {
        key += std::to_string(value);
        key.push_back(',');
    };
    for (const modemdrm::SDCAudioInformation& audio : sdc.audio)
    {
        if (audio.nextConfiguration || (shortId >= 0 && audio.shortId != shortId)) {
            continue;
        }

        append(audio.shortId);
        append(audio.streamId);
        append(audio.audioCoding);
        append(audio.sbr ? 1 : 0);
        append(audio.audioMode);
        append(audio.samplingRateCode);
        append(audio.text ? 1 : 0);
        append(audio.enhancement ? 1 : 0);
        append(audio.coderField);
        append(audio.surroundMode);
        append(static_cast<int>(audio.codecSpecificConfig.size()));
        for (std::uint8_t byte : audio.codecSpecificConfig) {
            append(byte);
        }
    }

    return key;
}

DRMDemodSink::DRMDemodSink() :
    m_channelSampleRate(DRMDemodSettings::ChannelSampleRate),
    m_channelFrequencyOffset(0),
    m_internalSampleRate(internalSampleRateFor(DRMDemodSettings::ChannelSampleRate)),
    m_audioFifo(AudioBufferSeconds * 48000),
    m_audioSampleRate(48000),
    m_audioDecoder(modemdrm::DRMAudioDecoder::create())
{
    m_capture.reserve(static_cast<std::size_t>(CaptureWindowSeconds * WideInternalSampleRate));
    m_status.outputAudioSampleRate = m_audioSampleRate;
    resetInputChain();
    updateSyncStatus();
    publishStatus();
}

int DRMDemodSink::internalSampleRateFor(int channelSampleRate)
{
    if (channelSampleRate <= 0) {
        return 0;
    }

    return channelSampleRate >= WideInternalSampleRate
        ? WideInternalSampleRate : NarrowInternalSampleRate;
}

int DRMDemodSink::getInternalSampleRate() const
{
    QMutexLocker locker(&m_statusMutex);
    return m_published.status.internalSampleRate;
}

void DRMDemodSink::resetInputChain()
{
    m_internalSampleRate = internalSampleRateFor(m_channelSampleRate);
    // Anti-alias edges at the internal rate: 0.4 fs passes the widest
    // acquisition band (mode E with its +/-5 kHz range at 192 kHz; modes A-D
    // at 48 kHz) and 0.5 fs keeps aliases outside it.
    m_resampler.configure(m_channelSampleRate, m_internalSampleRate,
        0.40 * m_internalSampleRate, 0.50 * m_internalSampleRate);
    m_bandFilter.configure(m_internalSampleRate);
    // The channelizer passes +/-channelRate/2 about its centre, where fR sits
    // at +offset; the resampler then preserves +/- its passband edge.
    const double channelHalf = m_channelSampleRate / 2.0;
    const double resamplerHalf = m_resampler.passbandHz();
    m_inputBandLowHz = std::max(-channelHalf - m_channelFrequencyOffset, -resamplerHalf);
    m_inputBandHighHz = std::min(channelHalf - m_channelFrequencyOffset, resamplerHalf);
    m_resampled.clear();
    m_filtered.clear();
    m_receivedSamples = 0;
    updateInputBand();
}

bool DRMDemodSink::spectrumUsable(modemdrm::Mode mode, int occupancy, bool spectralInversion) const
{
    return m_internalSampleRate > 0
        && modemdrm::Parameters::forMode(mode).symbolSamples(m_internalSampleRate) > 0
        && modemdrm::Parameters::spectrumWithinBand(mode, occupancy,
            m_inputBandLowHz, m_inputBandHighHz,
            m_settings.m_acquisitionSearchRangeHz, spectralInversion);
}

void DRMDemodSink::updateInputBand()
{
    // Pass the union of the candidate acquisition bands: every mode and
    // occupancy that could be present, or only the locked one once tracking.
    // Out-of-band noise and adjacent stations would otherwise dilute the
    // cyclic-prefix metric used for acquisition and tracking.
    double low = 0.0;
    double high = 0.0;
    bool any = false;
    const auto include = [&](modemdrm::Mode mode, int occupancy, bool inversion) {
        double lower = 0.0;
        double upper = 0.0;
        if (!spectrumUsable(mode, occupancy, inversion)
            || !modemdrm::Parameters::acquisitionBand(mode, occupancy,
                m_settings.m_acquisitionSearchRangeHz, inversion, lower, upper)) {
            return;
        }

        low = any ? std::min(low, lower) : lower;
        high = any ? std::max(high, upper) : upper;
        any = true;
    };
    const modemdrm::FACAcquisitionResult& locked = m_trackedFAC.valid ? m_trackedFAC : m_pendingFAC;
    if (locked.valid) {
        include(locked.coarse.mode, locked.occupancy, locked.spectralInversion);
    }
    else
    {
        for (const modemdrm::SpectrumParameters& spectrum : modemdrm::Parameters::spectra())
        {
            if (m_settings.m_forcedMode >= 0 && static_cast<int>(spectrum.mode) != m_settings.m_forcedMode) {
                continue;
            }

            if (m_settings.m_forcedOccupancy >= 0 && spectrum.occupancy != m_settings.m_forcedOccupancy) {
                continue;
            }

            // The orientation is detected, so pass both.
            include(spectrum.mode, spectrum.occupancy, false);
            include(spectrum.mode, spectrum.occupancy, true);
        }
    }

    if (!any)
    {
        low = -m_internalSampleRate / 2.0;
        high = m_internalSampleRate / 2.0;
    }

    if (low != m_bandFilter.lowHz() || high != m_bandFilter.highHz()) {
        m_bandFilter.setBand(low, high);
    }

    m_status.internalSampleRate = m_internalSampleRate;
    m_status.inputFilterLowHz = low;
    m_status.inputFilterHighHz = high;
}

void DRMDemodSink::start()
{
    QMutexLocker locker(&m_mutex);
    m_abort = false;
    {
        QMutexLocker magLocker(&m_magMutex);
        m_magSqSum = m_magSqPeak = 0.0;
        m_magSqSamples = 0;
        m_channelPower = -1.0;
    }

    m_capture.clear();
    clearDownstreamStatus(true);
    m_reacquireHint = modemdrm::FACSearchOptions();
    clearStationData();
    resetInputChain();
    m_syncState.start();
    updateSyncStatus();
    publishStatus();
}

void DRMDemodSink::stop()
{
    QMutexLocker locker(&m_mutex);
    {
        QMutexLocker magLocker(&m_magMutex);
        m_magSqSum = m_magSqPeak = 0.0;
        m_magSqSamples = 0;
        m_channelPower = -1.0;
    }

    m_capture.clear();
    clearDownstreamStatus(true);
    m_reacquireHint = modemdrm::FACSearchOptions();
    clearStationData();
    resetInputChain();
    m_syncState.stop();
    updateSyncStatus();
    publishStatus();
}

void DRMDemodSink::flush()
{
    QMutexLocker locker(&m_mutex);
    while (!m_abort && m_trackedFAC.valid && m_syncState.state() != modemdrm::SyncState::Stopped)
    {
        const auto fac = modemdrm::FACAcquisition::track(m_capture.data(), m_capture.size(),
            m_internalSampleRate, m_trackedFAC, m_trackingPrefixSamples);
        if (!fac.valid || modemdrm::DRMReceiver::maximumCompleteSuperframes(
                m_capture.size(), m_internalSampleRate, fac) == 0) {
            break;
        }

        const std::size_t consumed = std::min(processCapture(), m_capture.size());
        if (consumed == 0) {
            break;
        }

        m_capture.erase(m_capture.begin(), m_capture.begin() + static_cast<std::ptrdiff_t>(consumed));
    }

    updateInputBand();
    publishStatus();
}

void DRMDemodSink::reset(bool retune, bool keepQueuedAudio)
{
    QMutexLocker locker(&m_mutex);
    {
        QMutexLocker magLocker(&m_magMutex);
        m_magSqSum = m_magSqPeak = 0.0;
        m_magSqSamples = 0;
        m_channelPower = -1.0;
    }

    // A seek or loop keeps the station: reacquire from the lock just lost.
    if (retune)
    {
        m_reacquireHint = modemdrm::FACSearchOptions();
        clearStationData();
    }
    else
    {
        rememberLock();
    }

    m_capture.clear();
    clearDownstreamStatus(true, keepQueuedAudio);
    resetInputChain();
    if (retune) {
        m_syncState.resetOrRetune();
    } else {
        m_syncState.hardInputDiscontinuity();
    }

    updateSyncStatus();
    publishStatus();
}

void DRMDemodSink::feed(
    const SampleVector::const_iterator& begin,
    const SampleVector::const_iterator& end
)
{
    QMutexLocker locker(&m_mutex);
    if (m_syncState.state() == modemdrm::SyncState::Stopped
        || m_channelSampleRate <= 0 || m_internalSampleRate <= 0) {
        return;
    }

    // Power is accumulated locally and published under its own lock, so a
    // meter read never waits for a decode step.
    double powerSum = 0.0;
    double powerPeak = 0.0;
    int powerSamples = 0;
    const auto publishPower = [&]() {
        if (powerSamples == 0) {
            return;
        }

        QMutexLocker magLocker(&m_magMutex);
        if (m_magSqSamples > std::numeric_limits<int>::max() - powerSamples)
        {
            // Bound accounting in headless receivers with no meter reader.
            m_magSqSum /= m_magSqSamples;
            m_magSqSamples = 1;
        }

        m_magSqSum += powerSum;
        m_magSqPeak = std::max(m_magSqPeak, powerPeak);
        m_magSqSamples += powerSamples;
        // A level for reports that, unlike the meter's, is not consumed by
        // reading: averaged over about half a second.
        const double blockPower = powerSum / powerSamples;
        const double weight = std::min(1.0, powerSamples / (0.5 * m_internalSampleRate));
        m_channelPower = m_channelPower < 0.0 ? blockPower : m_channelPower + weight * (blockPower - m_channelPower);
        powerSum = powerPeak = 0.0;
        powerSamples = 0;
    };

    bool processed = false;
    std::size_t targetSamples = captureTargetSamples();
    for (auto it = begin; it != end; ++it)
    {
        const Complex sample(
            static_cast<float>(it->real() / SDR_RX_SCALEF),
            static_cast<float>(it->imag() / SDR_RX_SCALEF)
        );
        ++m_receivedSamples;
        // Channel rate -> fixed internal rate -> candidate band.
        m_resampled.clear();
        m_resampler.push(sample * m_nco.nextIQ(), m_resampled);
        for (const std::complex<float>& resampled : m_resampled)
        {
            m_filtered.clear();
            m_bandFilter.push(resampled, m_filtered);
            for (const std::complex<float>& filtered : m_filtered)
            {
                // The level of the DRM band (the candidate acquisition bands,
                // or the locked signal's), not of the whole channel.
                const double power = std::norm(filtered);
                powerSum += power;
                powerPeak = std::max(powerPeak, power);
                ++powerSamples;
                m_capture.push_back(filtered);
                if (m_capture.size() >= targetSamples)
                {
                    publishPower();
                    drainCapture();
                    processed = true;
                    targetSamples = captureTargetSamples();
                }
            }
        }
    }

    publishPower();
    if (processed) {
        publishStatus();
    }
}

void DRMDemodSink::drainCapture()
{
    std::size_t targetSamples = captureTargetSamples();
    while (!m_abort && m_capture.size() >= targetSamples)
    {
        const std::size_t available = m_capture.size();
        const std::size_t consumed = std::min(processCapture(), m_capture.size());
        const std::size_t nextTarget = captureTargetSamples();
        if (consumed == 0)
        {
            // Nothing consumed: the step must be waiting for more input. If it
            // did not ask for more, retry only after another 100 ms rather than
            // repeating the same work for every new sample.
            if (nextTarget <= available) {
                m_captureRetrySamples = available + static_cast<std::size_t>(m_internalSampleRate / 10);
            }

            break;
        }

        m_captureRetrySamples = 0;
        if (consumed == m_capture.size()) {
            m_capture.clear();
        }
        else
        {
            m_capture.erase(
                m_capture.begin(),
                m_capture.begin() + static_cast<std::ptrdiff_t>(consumed)
            );
        }

        targetSamples = captureTargetSamples();
        // Several superframes can decode in one feed (after a cold search):
        // let the GUI and API see each.
        publishStatus();
    }

    trimQueuedAudio();
    updateInputBand();
}

void DRMDemodSink::trimQueuedAudio()
{
    // A cold acquisition decodes its whole capture at once, and the audio
    // that burst queues would otherwise set the latency for the whole lock
    // (as would a sound card slower than the device). With the pre-roll on
    // (live playout), keep at most the pre-roll, one step (1.2 s, mode E
    // 0.4 s) and a margin, dropping the oldest audio.
    if (m_audioPreRollMilliseconds <= 0 || m_audioSampleRate <= 0) {
        return;
    }

    const int beyond = m_status.mode == 'E' ? MaximumQueuedAudioBeyondPreRollModeEMilliseconds
        : MaximumQueuedAudioBeyondPreRollMilliseconds;
    const std::uint64_t target = static_cast<std::uint64_t>(m_audioSampleRate)
        * static_cast<std::uint64_t>(m_audioPreRollMilliseconds + beyond) / 1000U;
    const std::uint32_t fill = m_audioFifo.fill();
    if (fill > target) {
        m_audioTrimmedSamples += m_audioFifo.drain(static_cast<std::uint32_t>(fill - target));
    }
}

// Muting takes effect at once: the audio already queued (seconds of it after
// a cold acquisition, or in mode E) is replaced by as much silence, which
// keeps the output's latency.
void DRMDemodSink::silenceQueuedAudio()
{
    const std::uint32_t queued = m_audioFifo.drain(m_audioFifo.fill());
    if (queued > 0)
    {
        const AudioVector silence(queued, AudioSample{0, 0});
        m_audioFifo.write(reinterpret_cast<const quint8 *>(silence.data()), queued);
    }
}

// Unmuting takes effect as soon as the next audio is decoded: what was queued
// while muted is silence, dropped down to the pre-roll (a margin against the
// output running dry, as at an audio start).
void DRMDemodSink::dropQueuedSilence()
{
    const std::uint32_t keep = static_cast<std::uint32_t>(std::max(m_audioSampleRate, 0))
        * static_cast<std::uint32_t>(std::max(m_audioPreRollMilliseconds, 0)) / 1000U;
    const std::uint32_t fill = m_audioFifo.fill();
    if (fill > keep) {
        m_audioFifo.drain(fill - keep);
    }
}

void DRMDemodSink::applyChannelSettings(
    int channelSampleRate,
    int channelFrequencyOffset,
    bool force
)
{
    QMutexLocker locker(&m_mutex);
    if (force || channelSampleRate != m_channelSampleRate
        || channelFrequencyOffset != m_channelFrequencyOffset)
    {
        m_channelSampleRate = channelSampleRate;
        m_channelFrequencyOffset = channelFrequencyOffset;
        if (channelSampleRate > 0) {
            m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
        }

        m_capture.clear();
        clearDownstreamStatus(true);
        m_reacquireHint = modemdrm::FACSearchOptions();
        clearStationData();
        resetInputChain();
        m_syncState.resetOrRetune();
        {
            QMutexLocker magLocker(&m_magMutex);
            m_channelPower = -1.0; // another frequency's level until measured
        }

        updateSyncStatus();
        publishStatus();
    }
}

void DRMDemodSink::applySettings(
    const DRMDemodSettings& settings,
    const QStringList& settingsKeys,
    bool force
)
{
    QMutexLocker locker(&m_mutex);
    DRMDemodSettings next = m_settings;
    if (force) {
        next = settings;
    } else {
        next.applySettings(settingsKeys, settings);
    }

    next.sanitize();
    // Only a changed value reacquires or restarts the service: an API or
    // reverse-API peer may resend unchanged keys.
    const bool reacquire = force
        || next.m_acquisitionSearchRangeHz != m_settings.m_acquisitionSearchRangeHz
        || next.m_forcedMode != m_settings.m_forcedMode
        || next.m_forcedOccupancy != m_settings.m_forcedOccupancy;
    const bool serviceChanged = !force
        && next.m_selectedServiceIdentifier != m_settings.m_selectedServiceIdentifier;
    const bool muted = next.m_audioMute && !m_settings.m_audioMute;
    const bool unmuted = !next.m_audioMute && m_settings.m_audioMute;
    m_settings = next;
    if (muted) {
        silenceQueuedAudio();
    } else if (unmuted) {
        dropQueuedSilence();
    }

    if (serviceChanged)
    {
        clearServiceStatus();
        clearServiceData();
    }

    if (reacquire)
    {
        m_capture.clear();
        clearDownstreamStatus(true);
        m_reacquireHint = modemdrm::FACSearchOptions();
        clearStationData();
        resetInputChain();
        m_syncState.resetOrRetune();
        updateSyncStatus();
    }

    publishStatus();
}

void DRMDemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate <= 0) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    const bool rateChanged = sampleRate != m_audioSampleRate;
    m_audioSampleRate = sampleRate;
    if (rateChanged)
    {
        m_audioFifo.setSize(static_cast<uint32_t>(AudioBufferSeconds * sampleRate));
        // An attached Demod Analyzer must follow the "demod" pipe's new rate.
        if (m_channel)
        {
            QList<ObjectPipe*> pipes;
            MainCore::instance()->getMessagePipes().getMessagePipes(m_channel, "reportdemod", pipes);
            for (ObjectPipe* pipe : pipes)
            {
                if (MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element)) {
                    messageQueue->push(MainCore::MsgChannelDemodReport::create(m_channel, sampleRate));
                }
            }
        }
    }

    m_audioFifo.clear();
    m_decoderAudioSampleRate = 0;
    m_audioInterpolatorDistance = 1.0f;
    m_audioInterpolatorDistanceRemain = 0.0f;
    m_audioSamplesWritten = 0;
    m_audioPreRollPending = true;
    m_status.audioActive = false;
    m_lastDecodedPCM = {};
    m_status.outputAudioSampleRate = sampleRate;
    publishStatus();
}

DRMDemodSink::PublishedStatus DRMDemodSink::snapshot() const
{
    // Worker context (m_mutex held): copy live state for publication.
    PublishedStatus published;
    published.status = m_status;
    published.status.capabilities = QStringList{QStringLiteral("nativeFAC"),
        QStringLiteral("nativeSDC"), QStringLiteral("nativeMSC")};
    if (m_audioDecoder)
    {
        const auto capabilities = m_audioDecoder->capabilities();
        published.status.audioDecoderBackend = QString::fromStdString(capabilities.backend);
        if (capabilities.aac) {
            published.status.capabilities.append(QStringLiteral("drmAAC"));
        }

        if (capabilities.xheAAC) {
            published.status.capabilities.append(QStringLiteral("drmXHEAAC"));
        }
    }

    published.locked = m_syncState.state() == modemdrm::SyncState::Locked;
    published.lastFAC = m_lastFAC;
    published.lastSDC = m_lastSDC;
    published.lastDecodedPCM = m_lastDecodedPCM;
    published.lastBroadcastTime = m_lastBroadcastTime;
    published.lastSlide = m_lastSlide;
    return published;
}

DRMDemodStatus DRMDemodSink::statusFrom(const PublishedStatus& published)
{
    DRMDemodStatus status = published.status;
    const auto now = Clock::now();
    const auto age = [now](Clock::time_point time) -> qint64 {
        return time == Clock::time_point{} ? -1
            : std::chrono::duration_cast<std::chrono::milliseconds>(now - time).count();
    };
    status.facAgeMs = age(published.lastFAC);
    status.sdcAgeMs = age(published.lastSDC);
    status.audioAgeMs = age(published.lastDecodedPCM);
    status.broadcastTimeAgeMs = age(published.lastBroadcastTime);
    status.slideshowAgeMs = age(published.lastSlide);
    // A-D delivers 1.2-second bursts. A stopped input must not leave a live
    // PCM indication indefinitely, and concealment alone is not decoded audio.
    status.audioActive = status.audioAgeMs >= 0 && status.audioAgeMs < 2500
        && published.locked;
    return status;
}

void DRMDemodSink::publishStatus()
{
    PublishedStatus published = snapshot();
    QMutexLocker locker(&m_statusMutex);
    m_published = std::move(published);
}

DRMDemodStatus DRMDemodSink::getStatus() const
{
    // GUI and HTTP threads must not wait for a decode step (FAC search, MSC
    // Viterbi and codec can take a large fraction of a second). Read live
    // state only when the worker is idle; otherwise use the last snapshot.
    if (m_mutex.tryLock())
    {
        const PublishedStatus live = snapshot();
        m_mutex.unlock();
        return statusFrom(live);
    }

    QMutexLocker locker(&m_statusMutex);
    return statusFrom(m_published);
}

int DRMDemodSink::getAudioSampleRate() const
{
    QMutexLocker locker(&m_statusMutex);
    return m_published.status.outputAudioSampleRate;
}

bool DRMDemodSink::getChannelPowerDB(double& db) const
{
    QMutexLocker locker(&m_magMutex);
    if (m_channelPower < 0.0) {
        return false;
    }

    db = 10.0 * std::log10(m_channelPower > 1.0e-20 ? m_channelPower : 1.0e-20);
    return true;
}

void DRMDemodSink::getMagSqLevels(double& average, double& peak, int& samples)
{
    QMutexLocker locker(&m_magMutex);
    samples = m_magSqSamples;
    average = samples ? m_magSqSum / samples : 1.0e-10;
    peak = samples ? m_magSqPeak : 1.0e-10;
    m_magSqSum = m_magSqPeak = 0.0;
    m_magSqSamples = 0;
}

void DRMDemodSink::clearDownstreamStatus(bool resetCaptureWindows, bool keepQueuedAudio)
{
    m_currentSDC = modemdrm::SDCData();
    m_facServiceCache.reset();
    m_currentSDCValid = false;
    m_lastReconfigurationIndex = -1;
    m_lastServiceCountCode = -1;
    m_trackedFAC = modemdrm::FACAcquisitionResult();
    m_trackingPrefixSamples = 0;
    m_pendingFAC = modemdrm::FACAcquisitionResult();
    m_pendingCaptureSamples = 0;
    m_captureRetrySamples = 0;
    m_sampleClock = modemdrm::SampleClockState();
    m_carrierNoise.reset();
    m_coldCaptureSeconds = 2;
    m_status.facValid = false;
    m_status.sdcValid = false;
    m_lastFAC = {};
    m_lastSDC = {};
    m_lastBroadcastTime = {};
    m_status.validFACBlocks = m_status.invalidFACBlocks = 0;
    m_status.validSDCBlocks = m_status.invalidSDCBlocks = m_status.skippedSDCBlocks = 0;
    clearServiceStatus(keepQueuedAudio);
    m_mscCellDeinterleaver.reset();
    m_mscConfigurationKey.clear();
    dataFramesLost();
    m_status.mode = '?';
    m_status.spectrumInverted = -1;
    m_status.occupancy = -1;
    m_status.effectiveSearchRangeHz = 0;
    m_status.carrierOffsetHz = 0.0;
    m_status.carrierOffsetAvailable = false;
    m_status.sampleClockErrorPPM = 0.0;
    m_status.sampleClockErrorAvailable = false;
    m_status.mscMERAvailable = false;
    m_status.mscMERDB = 0.0;
    m_status.mscMERCellCount = 0;
    m_status.serviceIdentifiers.clear();
    m_status.services.clear();
    if (resetCaptureWindows) {
        m_status.captureWindows = 0;
    }

    m_status.processedSuperframes = 0;
    m_status.retainedSamples = 0;
    m_status.textMessagePresent = false;
    m_status.textMessage.clear();
    m_status.invalidTextSegments = 0;
    m_status.broadcastTimeUTC.clear();
    m_status.localTimeOffsetPresent = false;
    m_status.localTimeOffsetMinutes = 0;
    m_status.afsIndex = -1;
    m_status.alternativeFrequencies.clear();
    m_status.error.clear();
}

void DRMDemodSink::clearServiceStatus(bool keepQueuedAudio)
{
    if (!keepQueuedAudio) {
        m_audioFifo.clear();
    }

    m_modeEAudioAssembler.reset();
    m_xheAudioParser.reset();
    m_textMessageDecoder.reset();
    if (m_audioDecoder) {
        m_audioDecoder->reset();
    }

    m_audioSamplesWritten = 0;
    m_audioPreRollPending = true;
    m_decoderAudioSampleRate = 0;
    m_decoderAudioChannels = 0;
    m_audioInterpolatorDistance = 1.0f;
    m_audioInterpolatorDistanceRemain = 0.0f;
    m_status.mscReady = false;
    m_status.audioActive = false;
    m_lastDecodedPCM = {};
    m_status.selectedServiceIdentifier = -1;
    m_status.processedSuperframes = 0;
    m_status.decodedMultiplexFrames = 0;
    m_status.invalidMultiplexFrames = 0;
    m_status.parsedAudioSuperframes = 0;
    m_status.invalidAudioSuperframes = 0;
    m_status.decodedPCMFrames = 0;
    m_status.concealedPCMFrames = 0;
    m_status.mscMERAvailable = false;
    m_status.mscMERDB = 0.0;
    m_status.mscMERCellCount = 0;
    m_status.decoderAudioSampleRate = 0;
    m_status.outputAudioSampleRate = m_audioSampleRate;
    m_status.audioChannels = 0;
    m_status.audioDroppedSamples = 0;
    m_status.textMessagePresent = false;
    m_status.textMessage.clear();
    m_status.invalidTextSegments = 0;
    m_status.slideshowPresent = false;
    m_status.websitePresent = false;
    m_status.dataService = false;
    m_status.error.clear();
}

// The selected service's slideshow and website take minutes to collect: kept
// through lock loss, loops and seeks, cleared when the service changes.
void DRMDemodSink::clearServiceData()
{
    m_slideshowDecoder.reset();
    m_websiteDecoder.reset();
    clearServiceDataStatus();
}

// The slideshow and website shown, as when the receiver moved its decoders
// to another service before decoding it.
void DRMDemodSink::clearServiceDataStatus()
{
    m_lastSlide = {};
    m_status.slideshowImage.clear();
    m_status.slideshowName.clear();
    m_status.slideshowMimeType.clear();
    m_status.slideshowImages = 0;
    m_status.slideshowValidPackets = 0;
    m_status.slideshowInvalidPackets = 0;
    ++m_status.slideshowUpdates;

    if (m_status.website) {
        ++m_status.websiteUpdates;
    }

    m_status.website.reset();
    m_status.websiteValidPackets = 0;
    m_status.websiteInvalidPackets = 0;
}

// The MSC cannot continue (lost lock, a skipped or failed superframe): the
// data components' partial data units go, their objects stay.
void DRMDemodSink::dataFramesLost()
{
    m_journalineDecoder.frameLost();
    m_slideshowDecoder.frameLost();
    m_websiteDecoder.frameLost();
}

// The station's data (Journaline, and the service's slideshow and website)
// is forgotten when the receiver forgets the station.
void DRMDemodSink::clearStationData()
{
    clearJournaline();
    clearServiceData();
    m_stationServices.clear();
    m_relockServices.clear();
    m_stationCheck = false;
}

// The services of the station whose data is kept. After a lost lock, one of
// them seen again keeps the data; every service of the multiplex found seen,
// none of them the station's, forgets it (another station on the frequency).
void DRMDemodSink::observeStation(std::uint32_t serviceIdentifier, int serviceCount)
{
    if (!m_stationCheck)
    {
        m_stationServices.insert(serviceIdentifier);
        return;
    }

    if (m_stationServices.count(serviceIdentifier) != 0U)
    {
        m_stationCheck = false;
        m_relockServices.clear();
        return;
    }

    m_relockServices.insert(serviceIdentifier);

    if (static_cast<int>(m_relockServices.size()) >= serviceCount)
    {
        const std::set<std::uint32_t> services = m_relockServices;
        clearStationData();
        m_stationServices = services;
    }
}

void DRMDemodSink::appendAudioSample(const Complex& sample, AudioVector& output) const
{
    const auto scale = [this](Real value) {
        if (m_settings.m_audioMute) {
            return static_cast<qint16>(0);
        }

        const Real amplified = value * m_settings.m_volume;
        return static_cast<qint16>(std::clamp(
            amplified,
            static_cast<Real>(std::numeric_limits<qint16>::min()),
            static_cast<Real>(std::numeric_limits<qint16>::max())
        ));
    };
    output.push_back({scale(sample.real()), scale(sample.imag())});
}

void DRMDemodSink::processPCMFrame(const modemdrm::PCMFrame& pcm)
{
    if (pcm.sampleRate <= 0 || (pcm.channels != 1 && pcm.channels != 2)
        || pcm.samples.empty()
        || pcm.samples.size() % static_cast<std::size_t>(pcm.channels) != 0) {
        return;
    }

    if (pcm.sampleRate != m_decoderAudioSampleRate
        || pcm.channels != m_decoderAudioChannels)
    {
        m_decoderAudioSampleRate = pcm.sampleRate;
        m_decoderAudioChannels = pcm.channels;
        m_audioInterpolator.create(
            16, pcm.sampleRate,
            std::min(pcm.sampleRate, m_audioSampleRate) / 2.2f
        );
        m_audioInterpolatorDistance = static_cast<Real>(pcm.sampleRate)
            / static_cast<Real>(m_audioSampleRate);
        m_audioInterpolatorDistanceRemain = 0.0f;
        m_status.decoderAudioSampleRate = pcm.sampleRate;
        m_status.audioChannels = pcm.channels;
    }

    const std::size_t inputFrames = pcm.samples.size() / static_cast<std::size_t>(pcm.channels);
    AudioVector output;
    output.reserve(static_cast<std::size_t>(
        std::ceil(inputFrames * static_cast<double>(m_audioSampleRate)
            / pcm.sampleRate)
    ) + 4U);

    for (std::size_t frame = 0; frame < inputFrames; ++frame)
    {
        const Real left = pcm.samples[frame * pcm.channels];
        const Real right = pcm.channels == 2
            ? pcm.samples[frame * pcm.channels + 1U] : left;
        const Complex input(left, right);
        Complex resampled;
        if (m_audioInterpolatorDistance == 1.0f) {
            appendAudioSample(input, output);
        }
        else if (m_audioInterpolatorDistance < 1.0f)
        {
            while (!m_audioInterpolator.interpolate(&m_audioInterpolatorDistanceRemain, input, &resampled))
            {
                appendAudioSample(resampled, output);
                m_audioInterpolatorDistanceRemain += m_audioInterpolatorDistance;
            }
        }
        else if (m_audioInterpolator.decimate(&m_audioInterpolatorDistanceRemain, input, &resampled))
        {
            appendAudioSample(resampled, output);
            m_audioInterpolatorDistanceRemain += m_audioInterpolatorDistance;
        }
    }

    if (!output.empty())
    {
        if (!pcm.concealed) {
            m_lastDecodedPCM = Clock::now();
        }

        // Once per audio start, and again if the output ran dry (a long gap
        // or a lost superframe in mode E)
        if (m_audioPreRollMilliseconds > 0
            && (m_audioPreRollPending || m_audioFifo.fill() == 0))
        {
            m_audioPreRollPending = false;
            const AudioVector silence(
                static_cast<std::size_t>(m_audioSampleRate) * static_cast<std::size_t>(m_audioPreRollMilliseconds) / 1000U,
                AudioSample{0, 0});
            m_audioFifo.write(reinterpret_cast<const quint8 *>(silence.data()), static_cast<uint32_t>(silence.size()));
        }

        const uint32_t written = m_audioFifo.write(
            reinterpret_cast<const quint8 *>(output.data()),
            static_cast<uint32_t>(output.size())
        );
        if (m_channel)
        {
            // Demod Analyzer and the MCP audio capture take mono audio at the
            // output rate from the channel's "demod" data pipes.
            QList<ObjectPipe*> dataPipes;
            MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);
            if (!dataPipes.isEmpty())
            {
                m_demodSamples.resize(output.size());
                for (std::size_t i = 0; i < output.size(); ++i) {
                    m_demodSamples[i] = static_cast<qint16>((static_cast<int>(output[i].l) + output[i].r) / 2);
                }

                for (ObjectPipe* pipe : dataPipes)
                {
                    if (DataFifo *fifo = qobject_cast<DataFifo*>(pipe->m_element))
                    {
                        fifo->write(reinterpret_cast<const quint8 *>(m_demodSamples.data()),
                            m_demodSamples.size() * sizeof(qint16), DataFifo::DataTypeI16);
                    }
                }
            }
        }

        m_audioSamplesWritten += written;
        // Preserve queued audio if the consumer stalls. Capacity covers the
        // cold-acquisition burst; any rejected tail is counted explicitly.
        m_status.audioDroppedSamples += output.size() - written;
    }
}

void DRMDemodSink::updateSyncStatus()
{
    m_status.syncState = QString::fromLatin1(
        modemdrm::SyncStateMachine::name(m_syncState.state())
    );
    m_status.epoch = m_syncState.epoch();
}

void DRMDemodSink::observeFAC(const modemdrm::FACAcquisitionResult& fac)
{
    for (const bool valid : fac.facBlockValid)
    {
        m_syncState.facBlock(valid, valid);
        if (valid) {
            ++m_status.validFACBlocks;
        } else {
            ++m_status.invalidFACBlocks;
        }
    }

    if (!fac.facBlockValid.empty())
    {
        m_lastFAC = Clock::now();
        m_status.facValid = fac.facBlockValid.back();
    }
}

std::size_t DRMDemodSink::captureTargetSamples() const
{
    std::size_t target = 0;

    if (m_pendingFAC.valid) {
        target = m_pendingCaptureSamples;
    } else if (!m_trackedFAC.valid) {
        target = static_cast<std::size_t>(m_coldCaptureSeconds) * static_cast<std::size_t>(m_internalSampleRate);
    }
    else
    {
        const auto& parameters = modemdrm::Parameters::forMode(m_trackedFAC.coarse.mode);
        // The superframe and (A-D) the next frame, which channel estimation
        // uses as context.
        const int edgeFrames = m_trackedFAC.coarse.mode == modemdrm::Mode::E ? 0 : 1;
        target = m_trackingPrefixSamples + static_cast<std::size_t>(
            parameters.symbolSamples(m_internalSampleRate)
                * (parameters.symbolsPerFrame * (parameters.framesPerSuperframe + edgeFrames) + 2));
    }

    return std::max(target, m_captureRetrySamples);
}

std::size_t DRMDemodSink::retainCapture(
    const modemdrm::FACAcquisitionResult& fac, std::size_t end)
{
    m_trackedFAC = fac;
    m_pendingFAC = modemdrm::FACAcquisitionResult();
    m_pendingCaptureSamples = 0;
    // Keep the guard before the next superframe and (A-D) its preceding
    // frame, which channel estimation uses as context.
    const auto& parameters = modemdrm::Parameters::forMode(fac.coarse.mode);
    const int edgeFrames = fac.coarse.mode == modemdrm::Mode::E ? 0 : 1;
    m_trackingPrefixSamples = std::min(end, static_cast<std::size_t>(
        parameters.guardSamples(m_internalSampleRate)
            + edgeFrames * parameters.symbolsPerFrame * parameters.symbolSamples(m_internalSampleRate)));
    ++m_superframeSequence;
    const std::size_t consumed = end - m_trackingPrefixSamples;
    m_status.retainedSamples = static_cast<int>(m_capture.size() - consumed);
    updateSyncStatus();
    return consumed;
}

void DRMDemodSink::rememberLock()
{
    if (!m_trackedFAC.valid) {
        return;
    }

    const auto& parameters = modemdrm::Parameters::forMode(m_trackedFAC.coarse.mode);
    m_reacquireHint = modemdrm::FACSearchOptions();
    m_reacquireHint.haveHint = true;
    m_reacquireHint.hintMode = static_cast<int>(m_trackedFAC.coarse.mode);
    m_reacquireHint.hintOccupancy = m_trackedFAC.occupancy;
    m_reacquireHint.hintSpectralInversion = m_trackedFAC.spectralInversion;
    m_reacquireHint.hintCarrierOffsetHz = m_trackedFAC.coarse.fractionalCarrierOffsetHz
        + m_trackedFAC.integerCarrierOffset * parameters.carrierSpacingHz();
}

modemdrm::FACSearchOptions DRMDemodSink::searchOptions() const
{
    modemdrm::FACSearchOptions options;
    options.configuredSearchRangeHz = m_settings.m_acquisitionSearchRangeHz;
    options.spectralInversion = -1; // normal, then mirrored
    options.forcedMode = m_settings.m_forcedMode;
    options.forcedOccupancy = m_settings.m_forcedOccupancy;
    options.passbandLowHz = m_inputBandLowHz;
    options.passbandHighHz = m_inputBandHighHz;
    options.cancel = &m_abort;
    // After losing lock, try the previous mode, occupancy and offset first;
    // the full configured search follows if that fails.
    if (m_reacquireHint.haveHint)
    {
        options.haveHint = true;
        options.hintMode = m_reacquireHint.hintMode;
        options.hintOccupancy = m_reacquireHint.hintOccupancy;
        options.hintCarrierOffsetHz = m_reacquireHint.hintCarrierOffsetHz;
        options.hintSpectralInversion = m_reacquireHint.hintSpectralInversion;
    }

    return options;
}

std::size_t DRMDemodSink::processCapture()
{
    ++m_status.captureWindows;
    // A forced spectrum the passband cannot hold in either orientation.
    if (m_settings.m_forcedMode >= 0 && m_settings.m_forcedOccupancy >= 0
        && !spectrumUsable(static_cast<modemdrm::Mode>(m_settings.m_forcedMode), m_settings.m_forcedOccupancy, false)
        && !spectrumUsable(static_cast<modemdrm::Mode>(m_settings.m_forcedMode), m_settings.m_forcedOccupancy, true))
    {
        clearDownstreamStatus();
        m_status.error = "DRM occupied carriers and acquisition range exceed the channel input passband";
        updateSyncStatus();
        return m_capture.size();
    }

    modemdrm::FACAcquisitionResult fac;
    const bool tracking = m_trackedFAC.valid;
    if (m_trackedFAC.valid)
    {
        fac = modemdrm::FACAcquisition::track(
            m_capture.data(), m_capture.size(), m_internalSampleRate,
            m_trackedFAC, m_trackingPrefixSamples
        );
    }

    if (!fac.valid && tracking)
    {
        // Count the chronological observations, including erased FAC blocks.
        const auto& parameters = modemdrm::Parameters::forMode(m_trackedFAC.coarse.mode);
        fac.facBlockValid.resize(parameters.framesPerSuperframe, false);
        observeFAC(fac);
        ++m_status.skippedSDCBlocks;
        m_status.mscReady = false;
        m_status.audioActive = false;
        m_status.error = "FAC tracking failed in the current superframe";
        const std::size_t end = m_trackingPrefixSamples + static_cast<std::size_t>(
            parameters.framesPerSuperframe * parameters.symbolsPerFrame
                * parameters.symbolSamples(m_internalSampleRate));
        // Preserve interleaver chronology for a bounded FAC gap in an otherwise
        // stable epoch. Cached signalling configures erasures only; it cannot
        // authorize control updates or treat the missing RF cells as decoded.
        if (m_syncState.state() == modemdrm::SyncState::Locked && m_currentSDCValid
            && m_mscCellDeinterleaver && m_mscSuperframeSequence + 1 == m_superframeSequence
            && m_lastReconfigurationIndex == 0
            && !m_currentSDC.nextMultiplex.present && m_currentSDC.nextAudio.empty()
            && fac.coarse.valid && end <= m_capture.size()
            && std::abs(static_cast<qint64>(fac.frameStartOffsetSamples)
                - static_cast<qint64>(m_trackingPrefixSamples)) <= parameters.guardSamples(m_internalSampleRate) / 2
            && (m_trackedFAC.facBlocks.front().identity == 0
                || m_trackedFAC.facBlocks.front().identity == 3))
        {
            auto erasedFAC = m_trackedFAC;
            erasedFAC.frameStartOffsetSamples = m_trackingPrefixSamples;
            modemdrm::ReceiverMSCReport report;
            std::string error;
            m_audioSamplesWritten = 0;
            if (modemdrm::DRMReceiver::decodeMSC(m_capture.data(), m_capture.size(),
                m_internalSampleRate, erasedFAC, m_currentSDC, report,
                m_settings.m_selectedServiceIdentifier, 1,
                [this](const modemdrm::PCMFrame& pcm) { processPCMFrame(pcm); },
                &error, &m_textMessageDecoder, m_audioDecoder.get(),
                m_mscCellDeinterleaver.get(), &m_modeEAudioAssembler, nullptr,
                &m_xheAudioParser, true, &m_facServiceCache, &m_sampleClock, &m_carrierNoise,
                &m_slideshowDecoder, &m_journalineDecoder, &m_websiteDecoder))
            {
                accumulateMSCReport(report);
                m_mscSuperframeSequence = m_superframeSequence;
                m_status.mscReady = report.mscReady;
                m_status.audioActive = m_audioSamplesWritten > 0;
                m_status.sdcValid = false;
                return retainCapture(m_trackedFAC, end);
            }
        }

        m_mscCellDeinterleaver.reset();
        m_mscConfigurationKey.clear();
        dataFramesLost();
        m_modeEAudioAssembler.reset();
        m_xheAudioParser.reset();
        if (m_syncState.state() == modemdrm::SyncState::Locked) {
            return retainCapture(m_trackedFAC, std::min(end, m_capture.size()));
        }

        // Lock lost. Drop only the superframe that failed (searching from it
        // just repeats the failure and delays reacquisition) and keep what
        // follows. The search then tries the mode, occupancy and offset just
        // lost before widening to the full configured range.
        rememberLock();
        clearDownstreamStatus();
        // The station found next may be another (a schedule change)
        m_stationCheck = !m_stationServices.empty();
        m_relockServices.clear();
        m_status.error = "FAC lock lost; reacquiring";
        updateSyncStatus();
        return std::min(end, m_capture.size());
    }

    if (!fac.valid && m_pendingFAC.valid)
    {
        // Resume the acquisition that was waiting for its first superframe.
        fac = m_pendingFAC;
        m_pendingFAC = modemdrm::FACAcquisitionResult();
        m_pendingCaptureSamples = 0;
    }

    if (!fac.valid)
    {
        fac = modemdrm::FACAcquisition::searchWindow(
            m_capture.data(), m_capture.size(), m_internalSampleRate,
            3, searchOptions()
        );
    }

    if (!fac.valid)
    {
        if (fac.coarse.valid)
        {
            m_syncState.candidateDetected();
            m_syncState.candidateRejected();
        }

        if (m_coldCaptureSeconds < CaptureWindowSeconds)
        {
            ++m_coldCaptureSeconds;
            m_status.error = "Waiting for a longer coherent FAC acquisition span";
            updateSyncStatus();
            return 0;
        }

        clearDownstreamStatus();
        m_status.error = "No coherent FAC sequence in the current capture window";
        m_status.retainedSamples = 0;
        updateSyncStatus();
        return m_capture.size();
    }

    if (!spectrumUsable(fac.coarse.mode, fac.occupancy, fac.spectralInversion))
    {
        m_syncState.candidateDetected();
        m_syncState.candidateRejected();
        clearDownstreamStatus();
        m_status.error = "DRM occupied carriers and acquisition range exceed the channel input passband";
        updateSyncStatus();
        return m_capture.size();
    }

    const auto& parameters = modemdrm::Parameters::forMode(fac.coarse.mode);
    int firstFrame = fac.coarse.mode == modemdrm::Mode::E ? 3 : -1;
    for (std::size_t i = 0; i < fac.facBlocks.size(); ++i)
    {
        if (fac.facBlocks[i].identity == 0 || fac.facBlocks[i].identity == 3)
        {
            firstFrame = static_cast<int>(i);
            break;
        }
    }

    if (firstFrame < 0) {
        return m_capture.size();
    }

    const std::size_t superframeEnd = fac.frameStartOffsetSamples + static_cast<std::size_t>(
        (firstFrame + parameters.framesPerSuperframe) * parameters.symbolsPerFrame
            * parameters.symbolSamples(m_internalSampleRate));
    if (superframeEnd > m_capture.size())
    {
        // While tracking, the capture target already covers the superframe;
        // the retry floor bounds any repeat.
        if (tracking) {
            return 0;
        }

        // The first complete superframe extends beyond the capture (for A-D,
        // whenever the first coherent FAC is not identity 0). Wait for it
        // rather than repeating the whole search on every new sample.
        const std::size_t limit = static_cast<std::size_t>(CaptureWindowSeconds + 3)
            * static_cast<std::size_t>(m_internalSampleRate);
        if (superframeEnd > limit)
        {
            clearDownstreamStatus();
            m_status.error = "DRM acquisition exceeds the capture window";
            updateSyncStatus();
            return m_capture.size();
        }

        m_pendingFAC = fac;
        m_pendingCaptureSamples = superframeEnd;
        return 0;
    }

    m_status.error.clear();
    m_status.mode = modemdrm::Parameters::modeName(fac.coarse.mode);
    m_status.spectrumInverted = fac.spectralInversion ? 1 : 0;
    m_status.occupancy = fac.occupancy;
    m_status.effectiveSearchRangeHz = modemdrm::Parameters::effectiveSearchRangeHz(
        fac.coarse.mode, m_settings.m_acquisitionSearchRangeHz
    );
    m_status.carrierOffsetAvailable = true;
    m_status.carrierOffsetHz = fac.coarse.fractionalCarrierOffsetHz
        + fac.integerCarrierOffset
            * modemdrm::Parameters::forMode(fac.coarse.mode).carrierSpacingHz();
    if (fac.facBlockValid.front())
    {
        const auto& channel = fac.facBlocks.front();
        if ((m_lastServiceCountCode >= 0 && m_lastServiceCountCode != channel.serviceCountCode)
            || (m_lastReconfigurationIndex > 0 && channel.reconfigurationIndex == 0)) {
            m_status.services.clear();
        }

        m_lastServiceCountCode = channel.serviceCountCode;
    }

    for (std::size_t blockIndex = 0; blockIndex < fac.facBlocks.size(); ++blockIndex)
    {
        if (!fac.facBlockValid[blockIndex]) {
            continue;
        }

        const modemdrm::FACData& block = fac.facBlocks[blockIndex];
        const auto* selected = m_facServiceCache.findIdentifier(
            static_cast<std::uint32_t>(m_status.selectedServiceIdentifier));
        const bool selectedRemapped = selected && std::any_of(block.services.begin(), block.services.end(),
            [selected](const modemdrm::FACService& service) {
                return service.shortIdentifier == selected->shortIdentifier
                    && (service.serviceIdentifier != selected->serviceIdentifier
                        || service.audioConditionalAccess != selected->audioConditionalAccess
                        || service.dataService != selected->dataService);
            });
        const bool cacheInvalidated = m_facServiceCache.observe(block);
        if (cacheInvalidated) {
            m_status.services.clear();
        }

        if (cacheInvalidated || selectedRemapped)
        {
            clearServiceStatus();
            clearServiceData();
        }

        for (const modemdrm::FACService& service : block.services)
        {
            observeStation(service.serviceIdentifier, block.serviceCount());
            auto found = std::find_if(m_status.services.begin(), m_status.services.end(),
                [&service](const DRMServiceStatus& status) {
                    return status.shortIdentifier == service.shortIdentifier;
                });
            // FAC service announcements rotate and individual slots can be
            // erased. Keep known services within the RF epoch, bounded by the
            // four short identifiers, replacing a remapped identifier.
            if (found == m_status.services.end())
            {
                m_status.services.emplace_back();
                found = std::prev(m_status.services.end());
            }
            else if (found->serviceIdentifier != service.serviceIdentifier) {
                *found = DRMServiceStatus{};
            }

            {
                DRMServiceStatus& status = *found;
                status.serviceIdentifier = service.serviceIdentifier;
                status.shortIdentifier = service.shortIdentifier;
                status.audioService = !service.dataService;
                status.conditionalAccess = service.audioConditionalAccess
                    || service.dataConditionalAccess;
                status.facLanguageCode = service.language;
                status.facServiceDescriptor = service.descriptor;
            }
        }
    }

    m_status.serviceIdentifiers.clear();
    for (const auto& service : m_status.services) {
        m_status.serviceIdentifiers.push_back(service.serviceIdentifier);
    }

    m_syncState.candidateDetected();
    observeFAC(fac);
    const bool firstFACFresh = !tracking || fac.facBlockValid.front();
    // A single erased first FAC need not destroy a known, stable multiplex.
    // Fresh later FAC blocks must confirm the same channel parameters (track
    // already checks those) and zero countdown. Freeze SDC/control updates;
    // neither a cached first FAC nor later slots can authorize promotion.
    const bool frozenConfigurationUsable = !firstFACFresh && m_currentSDCValid
        && m_lastReconfigurationIndex == 0 && !m_currentSDC.nextMultiplex.present
        && m_currentSDC.nextAudio.empty()
        && std::all_of(fac.facBlocks.begin(), fac.facBlocks.end(),
            [](const modemdrm::FACData& block) { return block.reconfigurationIndex == 0; });
    if (!firstFACFresh && !frozenConfigurationUsable)
    {
        ++m_status.skippedSDCBlocks;
        m_status.sdcValid = false;
        m_status.mscReady = false;
        m_status.audioActive = false;
        m_mscCellDeinterleaver.reset();
        m_mscConfigurationKey.clear();
        dataFramesLost();
        m_modeEAudioAssembler.reset();
        m_xheAudioParser.reset();
        return retainCapture(fac, superframeEnd);
    }

    const int reconfigurationIndex = firstFACFresh && static_cast<std::size_t>(firstFrame) < fac.facBlocks.size()
        ? fac.facBlocks[firstFrame].reconfigurationIndex : -1;
    const bool reconfigurationBoundary = m_lastReconfigurationIndex > 0
        && reconfigurationIndex == 0;
    modemdrm::SDCData promotedConfiguration;
    bool havePromotedConfiguration = false;
    if (reconfigurationBoundary)
    {
        promotedConfiguration = m_currentSDC;
        havePromotedConfiguration =
            modemdrm::SDCParser::promoteNextConfiguration(
                promotedConfiguration
            );
        m_currentSDC.multiplex = modemdrm::SDCMultiplexDescription();
        m_currentSDC.audio.clear();
        m_currentSDC.dataApplications.clear();
        m_currentSDC.nextMultiplex = modemdrm::SDCMultiplexDescription();
        m_currentSDC.nextAudio.clear();
        // Journaline's services are taken again from the new configuration
        if (m_journalineDecoder.reconfigure()) {
            publishJournaline();
        }

        if (havePromotedConfiguration)
        {
            m_currentSDC.multiplex = promotedConfiguration.multiplex;
            m_currentSDC.audio = promotedConfiguration.audio;
        }

        m_currentSDCValid = m_currentSDC.valid && m_currentSDC.multiplex.present;
    }

    if (reconfigurationIndex >= 0) {
        m_lastReconfigurationIndex = reconfigurationIndex;
    }

    modemdrm::SDCData sdc;
    std::string error;
    const bool decodedSDC = firstFACFresh && modemdrm::DRMReceiver::decodeSDC(
        m_capture.data(), m_capture.size(), m_internalSampleRate,
        fac, sdc, 1, &error
    );
    if (firstFACFresh)
    {
        m_lastSDC = Clock::now();
        if (decodedSDC) {
            ++m_status.validSDCBlocks;
        } else {
            ++m_status.invalidSDCBlocks;
        }
    }
    else
    {
        ++m_status.skippedSDCBlocks;
    }

    if (decodedSDC)
    {
        modemdrm::SDCParser::mergeBlock(m_currentSDC, sdc);
        if (reconfigurationBoundary && havePromotedConfiguration)
        {
            m_currentSDC.multiplex = promotedConfiguration.multiplex;
            m_currentSDC.audio = promotedConfiguration.audio;
        }

        m_currentSDCValid = m_currentSDC.valid;
        if (m_currentSDC.timeDate.present)
        {
            // Only a newly received entity renews the broadcast-time age.
            if (sdc.timeDate.present) {
                m_lastBroadcastTime = Clock::now();
            }

            m_status.broadcastTimeUTC = QStringLiteral("%1-%2-%3T%4:%5:00Z")
                .arg(m_currentSDC.timeDate.year, 4, 10, QLatin1Char('0'))
                .arg(m_currentSDC.timeDate.month, 2, 10, QLatin1Char('0'))
                .arg(m_currentSDC.timeDate.day, 2, 10, QLatin1Char('0'))
                .arg(m_currentSDC.timeDate.hour, 2, 10, QLatin1Char('0'))
                .arg(m_currentSDC.timeDate.minute, 2, 10, QLatin1Char('0'));
            m_status.localTimeOffsetPresent =
                m_currentSDC.timeDate.localTimeOffsetPresent;
            m_status.localTimeOffsetMinutes =
                m_currentSDC.timeDate.localTimeOffsetMinutes;
        }
        else
        {
            m_status.broadcastTimeUTC.clear();
            m_status.localTimeOffsetPresent = false;
            m_status.localTimeOffsetMinutes = 0;
        }

        m_status.afsIndex = m_currentSDC.afsIndex;
        m_status.alternativeFrequencies.clear();
        for (const modemdrm::SDCAlternativeFrequencyEntity& source :
            m_currentSDC.alternativeFrequencies)
        {
            DRMAFSStatus target;
            target.type = source.type;
            target.listVersion = source.listVersion;
            target.extension = source.extension;
            target.synchronousMultiplex = source.synchronousMultiplex;
            target.baseLayer = source.baseLayer;
            target.appliesToAllServices = source.appliesToAllServices;
            target.regionOrScheduleRestricted = source.regionOrScheduleRestricted;
            target.serviceRestrictionField = source.serviceRestrictionField;
            target.regionScheduleField = source.regionScheduleField;
            target.frequencyCodes = source.frequencyCodes;
            m_status.alternativeFrequencies.push_back(std::move(target));
        }
    }

    if (m_currentSDCValid) {
        enrichServices(m_status.services, m_currentSDC);
    }

    m_status.sdcValid = decodedSDC;
    if (!m_currentSDCValid)
    {
        // The MSC is skipped: the data components' packets stop here
        dataFramesLost();
        m_status.mscReady = false;
        m_status.audioActive = false;
        m_status.error = QString::fromStdString(error);
        return retainCapture(fac, superframeEnd);
    }

    // The deinterleaver depends on the channel coding and stream layout; the
    // codec, text and audio framing only on the decoded service's audio
    // descriptor, so another service's entity (arriving late, say) leaves the
    // interleaver chronology alone. Its output lags the input by the
    // interleaver depth, so a change of protection or stream lengths still
    // starts it again (a seamless reconfiguration would need each frame's
    // configuration carried through it).
    const std::string configurationKey = mscConfigurationKey(fac, m_currentSDC);
    // The service asked for, else the one the last decode chose automatically.
    const qint64 selectedService = m_settings.m_selectedServiceIdentifier >= 0
        ? m_settings.m_selectedServiceIdentifier : m_status.selectedServiceIdentifier;
    int selectedShortId = -1;
    if (selectedService >= 0)
    {
        if (const modemdrm::FACService* service = m_facServiceCache.findIdentifier(
                static_cast<std::uint32_t>(selectedService))) {
            selectedShortId = service->shortIdentifier;
        }
    }

    const std::string audioKey = audioConfigurationKey(m_currentSDC, selectedShortId);
    const auto resetAudioPath = [this]() {
        m_modeEAudioAssembler.reset();
        m_xheAudioParser.reset();
        m_textMessageDecoder.reset();
        if (m_audioDecoder) {
            m_audioDecoder->reset();
        }
    };
    // A superframe that did not reach the MSC (no usable SDC, for example)
    // breaks the interleaver chronology even when the configuration is
    // unchanged: start again rather than join non-adjacent frames.
    if (!m_mscCellDeinterleaver || configurationKey != m_mscConfigurationKey
        || m_mscSuperframeSequence + 1 != m_superframeSequence)
    {
        m_mscCellDeinterleaver =
            std::make_unique<modemdrm::MSCCellDeinterleaver>(
                fac.coarse.mode, !fac.facBlocks.front().shortInterleaving
            );
        m_mscConfigurationKey = configurationKey;
        resetAudioPath();
        // The data components' packets do not continue across it either
        dataFramesLost();
    }
    else if (selectedShortId == m_audioKeyShortId && audioKey != m_audioConfigurationKey) {
        resetAudioPath();
    }

    m_audioConfigurationKey = audioKey;
    m_audioKeyShortId = selectedShortId;

    modemdrm::ReceiverMSCReport report;
    error.clear();
    m_audioSamplesWritten = 0;
    const bool processed = modemdrm::DRMReceiver::decodeMSC(
        m_capture.data(), m_capture.size(), m_internalSampleRate,
        fac, m_currentSDC, report, m_settings.m_selectedServiceIdentifier,
        1, [this](const modemdrm::PCMFrame& pcm) { processPCMFrame(pcm); },
        &error, &m_textMessageDecoder, m_audioDecoder.get(),
        m_mscCellDeinterleaver.get(), &m_modeEAudioAssembler, nullptr,
        &m_xheAudioParser, false, &m_facServiceCache, &m_sampleClock, &m_carrierNoise,
        &m_slideshowDecoder, &m_journalineDecoder, &m_websiteDecoder
    );
    m_status.mscReady = processed && report.mscReady;
    accumulateMSCReport(report, processed);
    m_mscSuperframeSequence = m_superframeSequence;

    if (!processed)
    {
        // The MSC state cannot continue, but tracking does: retainCapture
        // below keeps this superframe's FAC as the tracking reference.
        m_mscCellDeinterleaver.reset();
        m_mscConfigurationKey.clear();
        dataFramesLost();
        m_modeEAudioAssembler.reset();
        m_xheAudioParser.reset();
        m_textMessageDecoder.reset();
        if (m_audioDecoder) {
            m_audioDecoder->reset();
        }
    }

    m_status.audioActive = m_audioSamplesWritten > 0;
    m_status.error = QString::fromStdString(error.empty() ? report.audioError : error);
    return retainCapture(fac, superframeEnd);
}

void DRMDemodSink::accumulateMSCReport(const modemdrm::ReceiverMSCReport& report, bool processed)
{
    // The receiver moved the slideshow and website decoders to another
    // service before decoding it (automatic selection can move as the SDC
    // fills, a data service first, then the audio service): what is shown of
    // the previous one goes before this report's data is taken
    if (report.serviceDataReset) {
        clearServiceDataStatus();
    }

    // And its text, which the receiver also dropped before decoding
    if (report.textMessageReset)
    {
        m_status.textMessagePresent = false;
        m_status.textMessage.clear();
    }

    // A failed decode stops before selecting or measuring: its report's
    // defaults would hide the service and its measurements for a superframe
    if (processed)
    {
        m_status.selectedServiceIdentifier = report.selectedServiceIdentifier;
        m_status.sampleClockErrorPPM = report.sampleClockErrorPPM;
        m_status.sampleClockErrorAvailable = m_sampleClock.haveDrift;
        m_status.mscMERAvailable = report.mscMERAvailable;
        m_status.mscMERDB = report.mscMERDB;
        m_status.mscMERCellCount = static_cast<quint64>(report.mscMERCellCount);
        m_status.slideshowPresent = report.slideshowPresent;
        m_status.websitePresent = report.websitePresent;
        m_status.dataService = report.dataService;
    }

    m_status.processedSuperframes += report.processedSuperframes;
    m_status.decodedMultiplexFrames += report.decodedMultiplexFrames;
    m_status.invalidMultiplexFrames += report.invalidMultiplexFrames;
    m_status.parsedAudioSuperframes += report.parsedAudioSuperframes;
    m_status.invalidAudioSuperframes += report.invalidAudioSuperframes;
    m_status.decodedPCMFrames += report.decodedPCMFrames;
    m_status.concealedPCMFrames += report.concealedPCMFrames;
    if (report.textMessageUpdated)
    {
        m_status.textMessagePresent = !report.clearTextMessage;
        m_status.textMessage = report.clearTextMessage
            ? QString() : QString::fromUtf8(
                report.textMessage.data(),
                static_cast<int>(report.textMessage.size())
            );
    }

    m_status.invalidTextSegments += report.invalidTextSegments;

    if (report.slideshowPresent)
    {
        m_status.slideshowValidPackets = static_cast<quint64>(report.slideshowValidPackets);
        m_status.slideshowInvalidPackets = static_cast<quint64>(report.slideshowInvalidPackets);
    }

    if (report.websitePresent)
    {
        m_status.websiteValidPackets = static_cast<quint64>(report.websiteValidPackets);
        m_status.websiteInvalidPackets = static_cast<quint64>(report.websiteInvalidPackets);
    }

    if (report.websiteUpdated)
    {
        m_status.website = std::make_shared<const modemdrm::MOTWebsite>(m_websiteDecoder.website());
        ++m_status.websiteUpdates;
    }

    if (report.journalineUpdated) {
        publishJournaline();
    }

    // The slideshow shows images (MOT content type 2); the latest replaces
    // the one before.
    for (const modemdrm::MOTObject& object : report.slideshowObjects)
    {
        if (object.contentType != 2 || object.body.empty()) {
            continue;
        }

        m_status.slideshowImage = QByteArray(reinterpret_cast<const char*>(object.body.data()), static_cast<int>(object.body.size()));
        m_status.slideshowName = QString::fromUtf8(object.name.data(), static_cast<int>(object.name.size()));
        m_status.slideshowMimeType = object.mimeType.empty() ? imageMimeType(object.contentSubtype)
            : QString::fromLatin1(object.mimeType.data(), static_cast<int>(object.mimeType.size()));
        ++m_status.slideshowImages;
        ++m_status.slideshowUpdates;
        m_lastSlide = Clock::now();
    }
}

// Journaline belongs to the station, not the lock: kept through
// reacquisition, cleared when the receiver forgets the station.
void DRMDemodSink::clearJournaline()
{
    m_journalineDecoder.reset();
    m_journalineSnapshots.clear();
    m_status.journaline.clear();
    ++m_status.journalineUpdates;
}

void DRMDemodSink::publishJournaline()
{
    // Only a component that changed is copied: the others keep their snapshot
    std::map<std::pair<int, int>, JournalineSnapshot> snapshots;
    m_status.journaline.clear();

    for (const auto& component : m_journalineDecoder.components())
    {
        const auto previous = m_journalineSnapshots.find(component.first);
        JournalineSnapshot snapshot;

        if ((previous != m_journalineSnapshots.end()) && (previous->second.revision == component.second.revision)) {
            snapshot = previous->second;
        } else {
            snapshot = {component.second.revision, std::make_shared<const DRMDemodStatus::JournalineObjects>(component.second.decoder.objects())};
        }

        for (int shortId : component.second.shortIds) {
            m_status.journaline[shortId] = snapshot.objects;
        }

        snapshots[component.first] = snapshot;
    }

    m_journalineSnapshots = std::move(snapshots);
    ++m_status.journalineUpdates;
}

QString DRMDemodSink::imageMimeType(int contentSubtype)
{
    // MOT image subtypes (ETSI TS 101 756 table 17).
    switch (contentSubtype)
    {
    case 0:
        return QStringLiteral("image/gif");
    case 1:
        return QStringLiteral("image/jpeg");
    case 2:
        return QStringLiteral("image/bmp");
    case 3:
        return QStringLiteral("image/png");
    default:
        return QString();
    }
}
