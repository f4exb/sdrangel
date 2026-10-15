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

#include "wefaxdemodsink.h"

#include <algorithm>
#include <cmath>

WefaxDemodSink::WefaxDemodSink() :
    m_channelSampleRate(12000),
    m_channelFrequencyOffset(0),
    m_interpolatorDistance(1.0f),
    m_interpolatorDistanceRemain(1.0f),
    m_powerReference(0.0),
    m_powerReferenceAlpha(0.0),
    m_videoScale(1.0f / 800.0f),
    m_magnitudeSquared(0.0),
    m_magnitudeSquaredSum(0.0),
    m_magnitudeSquaredPeak(0.0),
    m_magnitudeSquaredCount(0),
    m_toneWindowSamples(0),
    m_startConfirmWindows(10),
    m_stopConfirmWindows(10),
    m_ioc576StartToneWindows(0),
    m_ioc288StartToneWindows(0),
    m_stopToneWindows(0),
    m_acquisitionSampleCount(0),
    m_lastPhasingLineSample(0),
    m_lastPhasingLineCount(0),
    m_autoPhasingPreviousWhite(false),
    m_autoPhasingHaveLevel(false),
    m_autoPhasingPulseOpen(false),
    m_autoPhasingPulseStart(0),
    m_autoPhasingLastPulseCenter(0.0),
    m_autoPhasingHaveLastPulse(false),
    m_completionReason(CompletionReason::None),
    m_signalBlockSum(0.0),
    m_signalBlockSamples(0),
    m_signalReference(0.0),
    m_lowSignalSamples(0),
    m_signalLossSampleLimit(120000),
    m_blackFrequencyHz(0.0),
    m_whiteFrequencyHz(0.0),
    m_tuningErrorHz(0.0),
    m_tuningCorrectionHz(0.0),
    m_haveBlackFrequency(false),
    m_haveWhiteFrequency(false),
    m_timingFromPhasing(false)
{
    configureDecoder();
    configureDsp();
}

void WefaxDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex resampled;

    for (auto iterator = begin; iterator != end; ++iterator)
    {
        Complex sample(iterator->real(), iterator->imag());
        sample *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f)
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, sample, &resampled))
            {
                processOneSample(resampled);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else if (m_interpolator.decimate(&m_interpolatorDistanceRemain, sample, &resampled))
        {
            processOneSample(resampled);
            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
    }
}

void WefaxDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    if (force || (channelSampleRate != m_channelSampleRate))
    {
        m_channelSampleRate = std::max(1, channelSampleRate);
        m_channelFrequencyOffset = channelFrequencyOffset;
        configureDsp();
    }
    else if (channelFrequencyOffset != m_channelFrequencyOffset)
    {
        // Retuning only moves the NCO, so an image in progress continues.
        // The black/white levels were measured at the old offset.
        m_channelFrequencyOffset = channelFrequencyOffset;
        m_nco.setFreq(-m_channelFrequencyOffset, m_channelSampleRate);
        resetAfc();
    }
}

void WefaxDemodSink::applySettings(
    const QStringList& settingsKeys,
    const WefaxDemodSettings& settings,
    bool force)
{
    const int previousInternalRate = m_settings.internalSampleRate();
    const bool decoderChanged = force
        || settingsKeys.contains("ioc")
        || settingsKeys.contains("linesPerMinute")
        || settingsKeys.contains("autoMode")
        || settingsKeys.contains("inverted")
        || settingsKeys.contains("minimumPhasingLines");
    const bool captureReset = decoderChanged
        || WefaxDemodSettings::endsCapture(settingsKeys);
    const bool dspChanged = force
        || settingsKeys.contains("rfBandwidth")
        || settingsKeys.contains("fmDeviation")
        || decoderChanged;
    const bool acquisitionChanged = force
        || settingsKeys.contains("autoMode")
        || settingsKeys.contains("startConfirmSeconds")
        || settingsKeys.contains("stopConfirmSeconds")
        || decoderChanged;

    if (force) {
        m_settings = settings;
        m_settings.validate();
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (captureReset && (m_decoder.state() != WefaxDecoder::State::Idle)) {
        m_completionReason = CompletionReason::ModeChanged;
    }
    if (decoderChanged) {
        configureDecoder();
    } else if (captureReset) {
        m_decoder.reset();
    }
    if (dspChanged || (previousInternalRate != m_settings.internalSampleRate())) {
        configureDsp();
    } else if (acquisitionChanged) {
        resetAcquisition();
    }
}

void WefaxDemodSink::startPhasing()
{
    if (m_decoder.state() == WefaxDecoder::State::Receiving) {
        m_completionReason = CompletionReason::Rephase;
    } else {
        m_completionReason = CompletionReason::None;
    }
    m_timingFromPhasing = false;
    m_decoder.startPhasing();
    m_lastPhasingLineCount = 0;
    m_lastPhasingLineSample = m_acquisitionSampleCount;
    resetAutomaticPhasingDetector();
    resetAfc();
}

bool WefaxDemodSink::finishPhasing()
{
    const bool success = m_decoder.finishPhasingAligned();
    if (success) {
        m_timingFromPhasing = true;
        m_completionReason = CompletionReason::None;
    }
    return success;
}

bool WefaxDemodSink::startReceiving(bool keepTuning)
{
    if (!keepTuning) {
        resetAfc();
    }

    const double nominalSamplesPerLine = m_settings.internalSampleRate()
        * 60.0 / static_cast<double>(m_settings.m_linesPerMinute);
    const double correctedSamplesPerLine = nominalSamplesPerLine
        * (1.0 + m_settings.m_manualClockCorrectionPpm / 1.0e6);
    const bool success = m_decoder.startReceiving(correctedSamplesPerLine);
    if (success)
    {
        m_completionReason = CompletionReason::None;
        m_timingFromPhasing = false;
        m_pendingRows.clear();
    }
    return success;
}

void WefaxDemodSink::stop(CompletionReason reason)
{
    if (m_decoder.state() != WefaxDecoder::State::Idle) {
        m_completionReason = reason;
    }
    m_decoder.stop();

    // A new device frequency or stream is a different signal, so its tuning
    // error must be measured afresh.
    if (reason == CompletionReason::SourceChanged) {
        resetAfc();
    }
}

const char *WefaxDemodSink::completionReasonText(CompletionReason reason)
{
    switch (reason)
    {
    case CompletionReason::StopTone: return "stop-tone";
    case CompletionReason::ManualStop: return "manual-stop";
    case CompletionReason::Rephase: return "rephase";
    case CompletionReason::ModeChanged: return "mode-change";
    case CompletionReason::RowLimit: return "row-limit";
    case CompletionReason::NewStart: return "new-start";
    case CompletionReason::SignalLoss: return "signal-loss";
    case CompletionReason::SourceChanged: return "source-change";
    case CompletionReason::Shutdown: return "shutdown";
    case CompletionReason::None: default: return "none";
    }
}

double WefaxDemodSink::effectiveLinesPerMinute() const
{
    return m_decoder.appliedSamplesPerLine() > 0.0
        ? 60.0 * m_settings.internalSampleRate() / m_decoder.appliedSamplesPerLine()
        : 0.0;
}

std::vector<std::vector<std::uint8_t>> WefaxDemodSink::takeCompletedLines()
{
    auto lines = m_decoder.takeCompletedLines();
    if (!m_settings.m_autoMode) {
        return lines;
    }

    for (auto& line : lines) {
        m_pendingRows.emplace_back(std::move(line));
    }
    lines.clear();

    // Samples are rasterised before a control tone has accumulated enough
    // evidence to stop the decoder. Retain enough rows to cover the configured
    // confirmation interval and one detector window, at every supported LPM.
    const double confirmationSeconds = std::max(
        m_settings.m_startConfirmSeconds,
        m_settings.m_stopConfirmSeconds) + 0.1;
    const std::size_t controlGuardRows = static_cast<std::size_t>(std::max(
        1.0,
        std::ceil(confirmationSeconds * m_settings.m_linesPerMinute / 60.0) + 1.0));
    while (m_pendingRows.size() > controlGuardRows)
    {
        lines.emplace_back(std::move(m_pendingRows.front()));
        m_pendingRows.pop_front();
    }
    if (m_decoder.state() != WefaxDecoder::State::Receiving)
    {
        const bool discardControlRows = m_completionReason == CompletionReason::StopTone
            || m_completionReason == CompletionReason::NewStart;
        if (!discardControlRows) {
            while (!m_pendingRows.empty()) {
                lines.emplace_back(std::move(m_pendingRows.front()));
                m_pendingRows.pop_front();
            }
        } else {
            m_pendingRows.clear();
        }
    }
    return lines;
}

void WefaxDemodSink::getMagSqLevels(double& average, double& peak, int& sampleCount)
{
    if (m_magnitudeSquaredCount > 0) {
        m_magnitudeSquared = m_magnitudeSquaredSum / m_magnitudeSquaredCount;
    }

    average = m_magnitudeSquared;
    peak = m_magnitudeSquaredPeak;
    sampleCount = std::max(1, m_magnitudeSquaredCount);
    m_magnitudeSquaredSum = 0.0;
    m_magnitudeSquaredPeak = 0.0;
    m_magnitudeSquaredCount = 0;
}

void WefaxDemodSink::processOneSample(const Complex& sample)
{
    double magnitudeSquaredRaw;
    Real deviation;
    const Complex channelSample = m_channelFilter.filter(sample);
    const Real frequencyHz = m_phaseDiscriminator.phaseDiscriminatorDelta(
        channelSample,
        magnitudeSquaredRaw,
        deviation);

    // AFC measures the tones during phasing, which in automatic mode begins
    // during the start tone, and keeps the correction for the image. Between
    // transmissions there is only noise to measure.
    if ((m_decoder.state() == WefaxDecoder::State::Phasing) && m_toneMeter.feed(channelSample)) {
        updateAfc();
    }
    // Below the FM threshold the discriminator produces clicks: brief
    // excursions of several kHz that the video filter would otherwise smear
    // into black or white streaks. Black and white are at +/-shift/2, so this
    // leaves room for mistuning while bounding the energy of each click.
    // AFC: remove the measured midpoint error so black and white land at
    // their nominal levels.
    const Real clippedFrequencyHz = std::clamp(
        frequencyHz - static_cast<Real>(m_tuningCorrectionHz), -m_settings.m_fmDeviation, m_settings.m_fmDeviation);
    const Real filteredFrequencyHz = m_videoFilter.filter(clippedFrequencyHz);
    const Real controlFrequencyHz = m_controlFilter.filter(clippedFrequencyHz);
    const float video = 0.5f + filteredFrequencyHz * m_videoScale;
    const float controlVideo = 0.5f + controlFrequencyHz * m_videoScale;

    // Signal confidence for the decoder's fade fill: channel power through a
    // copy of the video filter, so it lines up with the video, relative to
    // its slow average.
    const Real normalizedMagnitudeSquared = magnitudeSquaredRaw / (SDR_RX_SCALED * SDR_RX_SCALED);
    const Real filteredPower = m_powerFilter.filter(normalizedMagnitudeSquared);
    m_powerReference = m_powerReference > 0.0
        ? m_powerReference + m_powerReferenceAlpha * (normalizedMagnitudeSquared - m_powerReference)
        : normalizedMagnitudeSquared;
    const float confidence = m_powerReference > 1.0e-30
        ? static_cast<float>(filteredPower / m_powerReference) : 1.0f;
    m_decoder.feedSample(video, confidence);

    // Measure tuning on the uncorrected frequency.
    processAcquisitionSample(controlVideo, controlFrequencyHz + m_tuningCorrectionHz, normalizedMagnitudeSquared);
    m_movingAverage(normalizedMagnitudeSquared);
    m_magnitudeSquared = m_movingAverage.asDouble();
    m_magnitudeSquaredSum += normalizedMagnitudeSquared;
    m_magnitudeSquaredPeak = std::max(m_magnitudeSquaredPeak, static_cast<double>(normalizedMagnitudeSquared));
    ++m_magnitudeSquaredCount;
}

void WefaxDemodSink::processAcquisitionSample(float video, float frequencyHz, double magnitudeSquared)
{
    const WefaxDecoder::State state = m_decoder.state();

    ++m_acquisitionSampleCount;
    const bool detectorsAvailable = m_ioc576StartToneDetector && m_ioc288StartToneDetector && m_stopToneDetector;

    if (m_settings.m_autoMode && (state == WefaxDecoder::State::Receiving))
    {
        // Receiver noise never reaches zero, so loss is judged relative to
        // the capture's own level: detector-window power averages more than
        // 10 dB below a slow (about 10 s) average of the normal blocks.
        m_signalBlockSum += magnitudeSquared;
        if (++m_signalBlockSamples >= m_toneWindowSamples)
        {
            const double blockPower = m_signalBlockSum / m_signalBlockSamples;
            const bool low = (blockPower < 1.0e-18)
                || ((m_signalReference > 0.0) && (blockPower < 0.1 * m_signalReference));
            if (!low)
            {
                m_signalReference = m_signalReference > 0.0
                    ? 0.99 * m_signalReference + 0.01 * blockPower
                    : blockPower;
            }
            m_lowSignalSamples = low ? m_lowSignalSamples + m_signalBlockSamples : 0;
            m_signalBlockSum = 0.0;
            m_signalBlockSamples = 0;

            if (m_lowSignalSamples >= m_signalLossSampleLimit)
            {
                stop(CompletionReason::SignalLoss);
                resetSignalLossDetector();
                return;
            }
        }
    }
    else 
    {
        resetSignalLossDetector();
    }

    if (state == WefaxDecoder::State::Phasing)
    {
        if (m_settings.m_autoMode) {
            processAutomaticPhasingSample(video);
        }
        const int phasingLines = m_decoder.phasingLineCount();
        if (phasingLines != m_lastPhasingLineCount)
        {
            m_lastPhasingLineCount = phasingLines;
            m_lastPhasingLineSample = m_acquisitionSampleCount;
        }

        const double nominalSamplesPerLine = m_settings.internalSampleRate()
            * 60.0 / static_cast<double>(m_settings.m_linesPerMinute);
        if ((phasingLines >= m_settings.m_minimumPhasingLines)
            && (m_acquisitionSampleCount - m_lastPhasingLineSample
                > static_cast<std::uint64_t>(std::ceil(1.05 * nominalSamplesPerLine))))
        {
            // Phasing has ended. If its timing was not accepted, receive with
            // nominal/manual timing rather than re-evaluating the estimate on
            // every sample while the tone detectors are held in reset.
            if (!finishPhasing() && !startReceiving(true))
            {
                m_decoder.startPhasing();
                m_lastPhasingLineCount = 0;
                m_lastPhasingLineSample = m_acquisitionSampleCount;
            }
            if (detectorsAvailable)
            {
                m_ioc576StartToneDetector->reset();
                m_ioc288StartToneDetector->reset();
                m_stopToneDetector->reset();
            }
            m_ioc576StartToneWindows = 0;
            m_ioc288StartToneWindows = 0;
            m_stopToneWindows = 0;
            return;
        }

        // Keep evaluating the start-tone detectors while manually armed for
        // phasing. The operator can press Start before the transmitter's
        // 300/675 Hz start tone; detecting it here selects the correct IOC and
        // restarts phasing at the proper beginning of the first image.
        if (!m_settings.m_autoMode) {
            return;
        }
    }

    if (!m_settings.m_autoMode || !detectorsAvailable) {
        return;
    }

    const double centeredVideo = std::clamp(static_cast<double>(video), 0.0, 1.0) - 0.5;
    if (m_ioc576StartToneDetector->size() < m_toneWindowSamples - 1)
    {
        m_ioc576StartToneDetector->filter(centeredVideo);
        m_ioc288StartToneDetector->filter(centeredVideo);
        m_stopToneDetector->filter(centeredVideo);
        return;
    }

    m_ioc576StartToneDetector->goertzel(centeredVideo);
    m_ioc288StartToneDetector->goertzel(centeredVideo);
    m_stopToneDetector->goertzel(centeredVideo);
    const double ioc576Magnitude = m_ioc576StartToneDetector->mag();
    const double ioc288Magnitude = m_ioc288StartToneDetector->mag();
    const double stopMagnitude = m_stopToneDetector->mag();
    m_ioc576StartToneDetector->reset();
    m_ioc288StartToneDetector->reset();
    m_stopToneDetector->reset();

    // A control tone must be strong and dominate the other two detectors.
    static constexpr double toneThreshold = 0.18;
    static constexpr double toneDominance = 1.8;
    const auto tonePresent = [](double magnitude, double otherA, double otherB) {
        return (magnitude > toneThreshold) && (magnitude > toneDominance * std::max(otherA, otherB));
    };
    const bool ioc576Present = tonePresent(ioc576Magnitude, ioc288Magnitude, stopMagnitude);
    const bool ioc288Present = tonePresent(ioc288Magnitude, ioc576Magnitude, stopMagnitude);

    if ((state == WefaxDecoder::State::Idle)
        || (state == WefaxDecoder::State::Phasing))
    {
        m_ioc576StartToneWindows = ioc576Present ? (m_ioc576StartToneWindows + 1) : 0;
        m_ioc288StartToneWindows = ioc288Present ? (m_ioc288StartToneWindows + 1) : 0;
        m_stopToneWindows = 0;

        if (m_ioc576StartToneWindows >= m_startConfirmWindows)
        {
            selectAutomaticIOC(576);
        }
        else if (m_ioc288StartToneWindows >= m_startConfirmWindows)
        {
            selectAutomaticIOC(288);
        }
    }
    else if (state == WefaxDecoder::State::Receiving)
    {
        const bool stopPresent = tonePresent(stopMagnitude, ioc576Magnitude, ioc288Magnitude);
        m_stopToneWindows = stopPresent ? (m_stopToneWindows + 1) : 0;
        m_ioc576StartToneWindows = ioc576Present ? (m_ioc576StartToneWindows + 1) : 0;
        m_ioc288StartToneWindows = ioc288Present ? (m_ioc288StartToneWindows + 1) : 0;

        if (m_stopToneWindows >= m_stopConfirmWindows)
        {
            stop(CompletionReason::StopTone);
            m_stopToneWindows = 0;
        }
        else if ((m_ioc576StartToneWindows >= m_startConfirmWindows)
            || (m_ioc288StartToneWindows >= m_startConfirmWindows))
        {
            const int ioc = m_ioc576StartToneWindows >= m_startConfirmWindows ? 576 : 288;
            m_completionReason = CompletionReason::NewStart;
            selectAutomaticIOC(ioc);
        }
    }
}

void WefaxDemodSink::processAutomaticPhasingSample(float video)
{
    const bool white = (m_settings.m_inverted ? 1.0f - video : video) >= 0.5f;

    if (!m_autoPhasingHaveLevel)
    {
        m_autoPhasingPreviousWhite = white;
        m_autoPhasingHaveLevel = true;
        return;
    }

    if (white && !m_autoPhasingPreviousWhite)
    {
        m_autoPhasingPulseStart = m_acquisitionSampleCount;
        m_autoPhasingPulseOpen = true;
    }
    else if (!white && m_autoPhasingPreviousWhite && m_autoPhasingPulseOpen)
    {
        const std::uint64_t pulseWidth = m_acquisitionSampleCount - m_autoPhasingPulseStart;
        const double sampleRate = m_settings.internalSampleRate();
        if ((pulseWidth >= static_cast<std::uint64_t>(std::floor(0.005 * sampleRate)))
            && (pulseWidth <= static_cast<std::uint64_t>(std::ceil(0.10 * sampleRate))))
        {
            const double center = 0.5 * (m_autoPhasingPulseStart + m_acquisitionSampleCount);
            if (m_autoPhasingHaveLastPulse)
            {
                static constexpr int candidates[] = {60, 90, 100, 120, 180, 240};
                const double interval = center - m_autoPhasingLastPulseCenter;
                // Missed pulses make an interval span up to four lines.
                const auto fits = [sampleRate](int lpm, double lineInterval) {
                    const double nominal = sampleRate * 60.0 / lpm;
                    for (int lines = 1; lines <= 4; ++lines)
                    {
                        const double expected = nominal * lines;
                        if (std::abs(lineInterval - expected) < 0.05 * expected) {
                            return true;
                        }
                    }
                    return false;
                };
                const bool plausible = std::any_of(std::begin(candidates), std::end(candidates),
                    [&fits, interval](int lpm) { return fits(lpm, interval); });

                if (plausible)
                {
                    m_autoPhasingIntervals.push_back(interval);
                    if (m_autoPhasingIntervals.size() > 6) {
                        m_autoPhasingIntervals.pop_front();
                    }
                }
                else
                {
                    m_autoPhasingIntervals.clear();
                }

                if (m_autoPhasingIntervals.size() >= 3)
                {
                    // Every multiple of the true rate also fits an interval, so
                    // one interval alone is ambiguous (1 s is 60x1, 120x2, 180x3
                    // and 240x4). Choose the lowest rate that fits all recent
                    // intervals: any single-line interval excludes lower rates.
                    for (const int lpm : candidates)
                    {
                        const bool fitsAll = std::all_of(
                            m_autoPhasingIntervals.begin(), m_autoPhasingIntervals.end(),
                            [&fits, lpm](double recent) { return fits(lpm, recent); });
                        if (fitsAll)
                        {
                            if (lpm != m_settings.m_linesPerMinute) {
                                selectAutomaticLpm(lpm);
                            }
                            break;
                        }
                    }
                }
            }

            m_autoPhasingLastPulseCenter = center;
            m_autoPhasingHaveLastPulse = true;
        }
        m_autoPhasingPulseOpen = false;
    }

    m_autoPhasingPreviousWhite = white;
}

void WefaxDemodSink::selectAutomaticIOC(int ioc)
{
    // A start tone re-confirms every confirmation period while it lasts.
    // Only the first confirmation is a new transmission; keep the AFC
    // measurement of the start tone through the rest.
    const bool newTransmission = m_decoder.state() != WefaxDecoder::State::Phasing;

    if (m_settings.m_ioc != ioc)
    {
        m_settings.m_ioc = ioc;
        configureDecoder();
        configureVideoFilter();
    }

    m_pendingRows.clear();
    if (newTransmission) {
        resetAfc();
    }
    m_timingFromPhasing = false;
    m_decoder.startPhasing();
    m_ioc576StartToneWindows = 0;
    m_ioc288StartToneWindows = 0;
    m_stopToneWindows = 0;
    m_lastPhasingLineCount = 0;
    m_lastPhasingLineSample = m_acquisitionSampleCount;
    resetAutomaticPhasingDetector();
}

void WefaxDemodSink::selectAutomaticLpm(int linesPerMinute)
{
    m_settings.m_linesPerMinute = linesPerMinute;
    configureDecoder();
    configureVideoFilter();
    m_decoder.startPhasing();
    m_lastPhasingLineCount = 0;
    m_lastPhasingLineSample = m_acquisitionSampleCount;
}

void WefaxDemodSink::resetAfc()
{
    m_toneMeter.reset();
    m_haveBlackFrequency = false;
    m_haveWhiteFrequency = false;
    m_tuningErrorHz = 0.0;
    m_tuningCorrectionHz = 0.0;
}

void WefaxDemodSink::updateAfc()
{
    const WefaxToneMeter::Result& tones = m_toneMeter.result();
    if (!tones.valid) {
        return;
    }
    m_blackFrequencyHz = tones.lowHz;
    m_whiteFrequencyHz = tones.highHz;
    m_haveBlackFrequency = true;
    m_haveWhiteFrequency = true;
    m_tuningErrorHz = 0.5 * (tones.lowHz + tones.highHz);
    m_tuningCorrectionHz = std::clamp(
        m_tuningErrorHz, -0.25 * m_settings.m_fmDeviation, 0.25 * m_settings.m_fmDeviation);
}

void WefaxDemodSink::resetAcquisition()
{
    const int sampleRate = m_settings.internalSampleRate();
    m_ioc576StartToneDetector = std::make_unique<Goertzel>(300.0, sampleRate);
    m_ioc288StartToneDetector = std::make_unique<Goertzel>(675.0, sampleRate);
    m_stopToneDetector = std::make_unique<Goertzel>(450.0, sampleRate);
    m_toneWindowSamples = std::max(1, sampleRate / 10);
    m_startConfirmWindows = std::max(1, static_cast<int>(std::ceil(
        m_settings.m_startConfirmSeconds * sampleRate / m_toneWindowSamples)));
    m_stopConfirmWindows = std::max(1, static_cast<int>(std::ceil(
        m_settings.m_stopConfirmSeconds * sampleRate / m_toneWindowSamples)));
    m_signalLossSampleLimit = static_cast<std::uint64_t>(10) * sampleRate;
    m_ioc576StartToneWindows = 0;
    m_ioc288StartToneWindows = 0;
    m_stopToneWindows = 0;
    m_acquisitionSampleCount = 0;
    m_lastPhasingLineSample = 0;
    m_lastPhasingLineCount = 0;
    resetAutomaticPhasingDetector();
    resetSignalLossDetector();
}

void WefaxDemodSink::resetAutomaticPhasingDetector()
{
    m_autoPhasingPreviousWhite = false;
    m_autoPhasingHaveLevel = false;
    m_autoPhasingPulseOpen = false;
    m_autoPhasingPulseStart = 0;
    m_autoPhasingLastPulseCenter = 0.0;
    m_autoPhasingHaveLastPulse = false;
    m_autoPhasingIntervals.clear();
}

void WefaxDemodSink::resetSignalLossDetector()
{
    m_signalBlockSum = 0.0;
    m_signalBlockSamples = 0;
    m_signalReference = 0.0;
    m_lowSignalSamples = 0;
}

void WefaxDemodSink::configureDsp()
{
    const int outputSampleRate = m_settings.internalSampleRate();
    // The resampler only prevents aliasing. Its short filter has a transition
    // band of about 2 kHz, far too gradual to reject an adjacent signal, so
    // channel selectivity comes from the steep FIR filter at the output rate.
    const Real antiAliasCutoff = 0.45f * std::min(m_channelSampleRate, outputSampleRate);
    m_nco.setFreq(-m_channelFrequencyOffset, m_channelSampleRate);
    m_interpolator.create(16, m_channelSampleRate, antiAliasCutoff, 2.2);
    // About 330 Hz transition band at any output rate.
    m_channelFilter.create(2 * (outputSampleRate / 200) + 1, outputSampleRate,
        std::min(m_settings.m_rfBandwidth / 2.0f, antiAliasCutoff));
    m_interpolatorDistance = static_cast<Real>(m_channelSampleRate)
        / static_cast<Real>(outputSampleRate);
    m_interpolatorDistanceRemain = m_interpolatorDistance;
    m_phaseDiscriminator.reset();
    // About 0.25 s: slower than a fade, fast enough to follow slow QSB.
    m_powerReferenceAlpha = 1.0 / (0.25 * outputSampleRate);
    m_powerReference = 0.0;
    // Restarts the tone measurement, but keeps the applied correction:
    // bandwidth and rate changes do not change the tuning.
    m_toneMeter.configure(outputSampleRate, m_settings.m_fmDeviation);
    m_phaseDiscriminator.setFMScaling(static_cast<Real>(outputSampleRate) / 2.0f);

    configureVideoFilter();
    resetAcquisition();
}

void WefaxDemodSink::configureVideoFilter()
{
    const int outputSampleRate = m_settings.internalSampleRate();
    const double pixelRate = static_cast<double>(WefaxDecoder::rasterWidth(m_settings.m_ioc))
        * m_settings.m_linesPerMinute / 60.0;
    // The discriminator's noise rises with frequency. Follow the RF bandwidth
    // so that it is a single noise versus sharpness control: a narrow weak
    // signal setting also narrows the video, while a wide one keeps the full
    // Nyquist resolution of half the pixel rate.
    const double videoCutoff = std::min(
        std::clamp(m_settings.m_rfBandwidth / 2.0, pixelRate / 4.0, pixelRate / 2.0),
        0.45 * outputSampleRate);
    m_videoFilter.create(63, outputSampleRate, videoCutoff);
    m_powerFilter.create(63, outputSampleRate, videoCutoff);
    m_controlFilter.create(63, outputSampleRate, std::min(1200.0, 0.45 * outputSampleRate));
    m_videoScale = 1.0f / m_settings.m_fmDeviation;
}

void WefaxDemodSink::configureDecoder()
{
    WefaxDecoder::Config config;
    config.ioc = m_settings.m_ioc;
    config.linesPerMinute = m_settings.m_linesPerMinute;
    config.nominalSampleRate = m_settings.internalSampleRate();
    config.minimumPhasingLines = m_settings.m_minimumPhasingLines;
    config.inverted = m_settings.m_inverted;
    m_decoder.configure(config);
}
