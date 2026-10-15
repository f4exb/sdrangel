///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                    //
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

#include <algorithm>
#include <cmath>

#include "psk31demodulator.h"

PSK31Demodulator::PSK31Demodulator() :
    m_rfBandwidth(100.0f),
    m_channelSampleRate(m_sampleRate),
    m_channelFrequencyOffset(0),
    m_interpolatorDistance(1.0f),
    m_interpolatorDistanceRemain(1.0f),
    m_costasLoop(2.0f * M_PI / 300.0f, 2),
    m_maxFrequency(0.0f),
    m_trackingCutoff(0.0f),
    m_trackingNoiseBandwidth(1.0f),
    m_magsq(0.0),
    m_magsqSum(0.0),
    m_magsqPeak(0.0),
    m_magsqCount(0),
    m_afcIndex(0),
    m_afcCount(0),
    m_afcCandidate(0.0f),
    m_afcCandidateValid(false),
    m_afcFrequency(0.0f),
    m_afcPhase(0.0f),
    m_matchedFilterIndex(0),
    m_historyIndex(0),
    m_symbolPower(0.0f),
    m_clockCount(0.0f),
    m_timingIntegrator(0.0f),
    m_lockMetric(0.0f),
    m_lockCount(0),
    m_presentCount(0),
    m_unlockCount(0),
    m_lockedLevel(0.0f),
    m_locked(false),
    m_snrInPhaseSum(0.0),
    m_snrQuadratureSum(0.0),
    m_snrCount(0),
    m_snrSignalPower(0.0),
    m_snrNoisePower(0.0),
    m_snrAverageValid(false),
    m_snr(0.0f),
    m_reportSampleCount(0)
{
    // PSK31 shapes each phase reversal with a cosine envelope, so each symbol
    // is a raised cosine pulse spanning two symbol periods. Matching it
    // restricts the noise at the decision point to roughly the symbol rate.
    const int matchedFilterLength = 2 * m_samplesPerSymbol + 1;
    m_matchedFilterTaps.resize(matchedFilterLength);
    Real sum = 0.0f;
    for (int i = 0; i < matchedFilterLength; ++i)
    {
        m_matchedFilterTaps[i] = 0.5f - 0.5f * std::cos(2.0f * M_PI * i / (matchedFilterLength - 1));
        sum += m_matchedFilterTaps[i];
    }
    for (auto& tap : m_matchedFilterTaps) {
        tap /= sum;
    }
    m_matchedFilterSamples.resize(matchedFilterLength);
    m_history.resize(m_samplesPerSymbol + 1);

    m_afcBuffer.resize(m_afcLength);
    m_afcTwiddle.resize(m_afcLength);
    m_afcWindow.resize(m_afcLength);
    for (int i = 0; i < m_afcLength; ++i)
    {
        m_afcTwiddle[i] = Complex(std::cos(2.0 * M_PI * i / m_afcLength), -std::sin(2.0 * M_PI * i / m_afcLength));
        m_afcWindow[i] = 0.5f - 0.5f * std::cos(2.0f * M_PI * i / m_afcLength);
    }

    setRFBandwidth(m_rfBandwidth);
    setChannel(m_channelSampleRate, m_channelFrequencyOffset);
}

void PSK31Demodulator::setChannel(int channelSampleRate, int channelFrequencyOffset)
{
    m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    m_interpolator.create(32, channelSampleRate, m_rfBandwidth / 2.0f);
    m_interpolatorDistance = static_cast<Real>(channelSampleRate) / m_sampleRate;
    m_interpolatorDistanceRemain = m_interpolatorDistance;
    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    reset();
}

void PSK31Demodulator::setRFBandwidth(Real rfBandwidth)
{
    m_rfBandwidth = std::isfinite(rfBandwidth) ? std::clamp(rfBandwidth, 20.0f, m_sampleRate / 2.0f) : 100.0f;
    m_interpolator.create(32, m_channelSampleRate, m_rfBandwidth / 2.0f);
    m_channelFilter.create(m_channelFilterTaps, m_sampleRate, m_rfBandwidth / 2.0f);
    m_trackingCutoff = std::min(m_trackingBandwidth, m_rfBandwidth) / 2.0f;
    m_trackingFilter.create(m_channelFilterTaps, m_sampleRate, m_trackingCutoff);

    // Equivalent noise bandwidth of the tracking filter, for SNR normalization
    std::vector<Real> taps;
    FirFilterGenerators::generateLowPassFilter(m_channelFilterTaps, m_sampleRate, m_trackingCutoff, taps);
    double sum = 0.0;
    double sumSquares = 0.0;
    for (size_t i = 0; i < taps.size(); ++i)
    {
        // Only half the symmetric taps are stored. The last is the center.
        const int weight = (i == taps.size() - 1) ? 1 : 2;
        sum += weight * taps[i];
        sumSquares += weight * taps[i] * taps[i];
    }
    m_trackingNoiseBandwidth = m_sampleRate * sumSquares / (sum * sum);

    // Leave room for the signal's main lobe inside the channel filter.
    m_maxFrequency = std::max(5.0f, m_rfBandwidth / 2.0f - 15.0f);
    m_costasLoop.setMaxFreq(m_costasRange * 2.0f * M_PI / m_sampleRate);
    m_costasLoop.setMinFreq(-m_costasRange * 2.0f * M_PI / m_sampleRate);

    resetSNR();
    m_snrAverageValid = false;
    m_snr = 0.0f;
    m_reportSampleCount = 0;
}

void PSK31Demodulator::reset()
{
    m_costasLoop.reset();
    m_lockMetric = 0.0f;
    m_levelAverage.reset();
    m_signalLevelAverage.reset();
    m_lockCount = 0;
    m_presentCount = 0;
    m_unlockCount = 0;
    m_lockedLevel = 0.0f;
    m_locked = false;
    m_pendingCharacters.clear();
    m_acquisitionSymbols.clear();
    m_decoder.reset();
    resetSNR();
    m_snrAverageValid = false;
    m_snr = 0.0f;
    m_clockCount = m_samplesPerSymbol / 2;
    m_timingIntegrator = 0.0f;
    m_symbolPower = 0.0f;
    m_afcFrequency = 0.0f;
    m_afcPhase = 0.0f;
    std::fill(m_matchedFilterSamples.begin(), m_matchedFilterSamples.end(), Complex(0.0f, 0.0f));
    std::fill(m_history.begin(), m_history.end(), 0.0f);
    std::fill(m_afcBuffer.begin(), m_afcBuffer.end(), Complex(0.0f, 0.0f));
    m_afcIndex = 0;
    m_afcCount = 0;
    m_afcCandidate = 0.0f;
    m_afcCandidateValid = false;
}

void PSK31Demodulator::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex output;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex sample(it->real(), it->imag());
        sample *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f)
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, sample, &output))
            {
                processSample(output / SDR_RX_SCALEF);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else if (m_interpolator.decimate(&m_interpolatorDistanceRemain, sample, &output))
        {
            processSample(output / SDR_RX_SCALEF);
            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
    }
}

void PSK31Demodulator::processSample(const Complex& input)
{
    // Measure levels after the channel filter so signals outside the RF
    // bandwidth do not reduce loop gain or distort the channel power reading.
    const Complex sample = m_channelFilter.filter(input);
    const Real magsq = std::norm(sample);
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    m_magsqPeak = std::max(m_magsqPeak, static_cast<double>(magsq));
    ++m_magsqCount;

    if (m_magsqCount >= m_sampleRate / 10)
    {
        m_magSqLevelStore.m_magsq = m_magsqSum / m_magsqCount;
        m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        m_magSqLevelStore.m_nbSamples = m_magsqCount;
        m_magsqSum = 0.0;
        m_magsqPeak = 0.0;
        m_magsqCount = 0;
    }

    acquireCarrier(sample);

    // Move the acquired carrier to the center of the narrower tracking filter,
    // which keeps adjacent signals out of the carrier and timing loops.
    m_afcPhase += m_afcFrequency;
    if (m_afcPhase > M_PI) {
        m_afcPhase -= 2.0f * M_PI;
    } else if (m_afcPhase < -M_PI) {
        m_afcPhase += 2.0f * M_PI;
    }
    const Complex tracked = m_trackingFilter.filter(sample * Complex(std::cos(m_afcPhase), -std::sin(m_afcPhase)));

    // The Costas detector gain is proportional to input power. Normalize over
    // two symbols so acquisition bandwidth does not depend on signal level.
    m_levelAverage(std::norm(tracked));
    m_signalLevelAverage(std::norm(tracked));
    const Real scale = 1.0f / std::sqrt(std::max(m_levelAverage.asDouble(), 1.0e-20));
    const Complex normalized = tracked * scale;
    m_costasLoop.feed(normalized.real(), normalized.imag());

    if (m_locked)
    {
        // Slowly move the Costas loop's frequency into the AFC, so the signal
        // stays centered in the tracking filter as it drifts.
        const Real transfer = m_costasLoop.getFreq() * m_afcTransferRate;
        m_afcFrequency += transfer;
        m_costasLoop.setFreq(m_costasLoop.getFreq() - transfer);
    }

    const Complex corrected = normalized * -std::conj(m_costasLoop.getComplex());
    const Complex symbolValue = matchedFilter(corrected);
    const Real filtered = symbolValue.real();
    m_historyIndex = (m_historyIndex + 1) % m_history.size();
    m_history[m_historyIndex] = filtered;

    bool symbolSample = false;
    m_clockCount -= 1.0f;

    if (m_clockCount <= 0.0f)
    {
        // Gardner timing error: the matched filter output half way between
        // opposite symbols should be zero. Transitions provide the timing
        // information, so steady carrier leaves the clock free running.
        const int size = m_history.size();
        const Real previous = m_history[(m_historyIndex + 1) % size];
        const Real middle = m_history[(m_historyIndex + 1 + m_samplesPerSymbol / 2) % size];
        m_symbolPower += 0.05f * (filtered * filtered - m_symbolPower);
        const Real timingError = m_symbolPower > 0.0f
            ? std::clamp(middle * (previous - filtered) / m_symbolPower, -m_maxTimingError, m_maxTimingError)
            : 0.0f;
        // Proportional plus integral, so a sample clock offset between
        // transmitter and receiver does not leave a steady timing error.
        m_timingIntegrator = std::clamp(
            m_timingIntegrator + m_timingIntegralGain * timingError,
            -m_maxTimingRate,
            m_maxTimingRate);
        m_clockCount += m_samplesPerSymbol + m_timingGain * timingError + m_timingIntegrator;

        symbol(symbolValue);
        symbolSample = true;
    }

    if (m_locked && (m_unlockCount == 0))
    {
        m_snrInPhaseSum += corrected.real() * corrected.real();
        m_snrQuadratureSum += corrected.imag() * corrected.imag();
        ++m_snrCount;
    }

    if (++m_reportSampleCount >= m_sampleRate)
    {
        if (m_locked && (m_snrCount > 0))
        {
            const double inPhasePower = m_snrInPhaseSum / m_snrCount;
            const double quadraturePower = m_snrQuadratureSum / m_snrCount;
            const double signalPower = std::max(inPhasePower - quadraturePower, 1.0e-20);
            const double noisePower = std::max(2.0 * quadraturePower, 1.0e-20);

            // Average over a few seconds for a steadier reading
            if (m_snrAverageValid)
            {
                m_snrSignalPower += 0.5 * (signalPower - m_snrSignalPower);
                m_snrNoisePower += 0.5 * (noisePower - m_snrNoisePower);
            }
            else
            {
                m_snrSignalPower = signalPower;
                m_snrNoisePower = noisePower;
                m_snrAverageValid = true;
            }

            const double referenceNoisePower = m_snrNoisePower * m_snrReferenceBandwidth / m_trackingNoiseBandwidth;
            m_snr = std::clamp(
                static_cast<float>(10.0 * std::log10(m_snrSignalPower / referenceNoisePower)),
                -99.9f,
                99.9f);
            resetSNR();
        }

        if (m_reportCallback) {
            m_reportCallback();
        }

        m_reportSampleCount = 0;
    }

    if (m_scopeCallback)
    {
        const Real normalizedFiltered = m_symbolPower > 0.0f ? filtered / std::sqrt(m_symbolPower) : 0.0f;
        m_scopeCallback(Complex(normalizedFiltered, symbolSample ? 1.0f : 0.0f));
    }
}

void PSK31Demodulator::acquireCarrier(const Complex& sample)
{
    // Squaring BPSK removes the modulation, leaving a tone at twice the
    // carrier offset. Search for it and tune there, as the Costas loop itself
    // only pulls in over a few Hz.
    m_afcBuffer[m_afcIndex] = sample * sample;
    m_afcIndex = (m_afcIndex + 1) % m_afcLength;

    if (m_afcCount < m_afcLength)
    {
        ++m_afcCount;
        return;
    }

    if ((m_afcIndex % m_afcInterval) != 0) {
        return;
    }

    const Real current = getFrequencyOffset();

    if (m_locked)
    {
        checkIdleToneLock(current);
        return;
    }

    const int maxBin = static_cast<int>(2.0f * m_maxFrequency * m_afcLength / m_sampleRate);
    int peakBin = -maxBin;
    Real peakPower = 0.0f;
    m_afcPowers.resize(2 * maxBin + 1);

    for (int bin = -maxBin; bin <= maxBin; ++bin)
    {
        const Real power = afcPower(bin);
        m_afcPowers[bin + maxBin] = power;

        if (power > peakPower)
        {
            peakPower = power;
            peakBin = bin;
        }
    }

    // Compare against the median, as the signal's own tones would inflate a mean.
    m_afcSorted = m_afcPowers;
    std::nth_element(m_afcSorted.begin(), m_afcSorted.begin() + m_afcSorted.size() / 2, m_afcSorted.end());
    const Real noisePower = m_afcSorted[m_afcSorted.size() / 2];

    if ((noisePower <= 0.0f) || (peakPower < m_afcThreshold * noisePower)) {
        return;
    }

    // Parabolic interpolation between bins
    Real delta = 0.0f;
    if ((peakBin > -maxBin) && (peakBin < maxBin))
    {
        const Real left = m_afcPowers[peakBin + maxBin - 1];
        const Real right = m_afcPowers[peakBin + maxBin + 1];
        const Real denominator = left - 2.0f * peakPower + right;
        if (denominator != 0.0f) {
            delta = std::clamp(0.5f * (left - right) / denominator, -0.5f, 0.5f);
        }
    }

    const Real frequency = std::clamp(
        (peakBin + delta) * m_sampleRate / (2.0f * m_afcLength),
        -m_maxFrequency,
        m_maxFrequency);

    if (std::abs(frequency - current) > m_afcRetuneHz) {
        retune(frequency);
    }
}

void PSK31Demodulator::checkIdleToneLock(Real current)
{
    // An idle signal is two tones, one symbol rate apart. To the Costas loop,
    // either looks like an unmodulated carrier, so it can lock 15.6 Hz from the
    // true carrier. Squared, the true carrier's tone is then 6 dB stronger than
    // the one at our frequency. Only these bins are checked, so strong signals
    // elsewhere in the RF bandwidth can't pull us away once locked.
    const int centerBin = static_cast<int>(std::lround(2.0f * current * m_afcLength / m_sampleRate));
    const int offsetBins = static_cast<int>(std::lround(m_sampleRate / static_cast<Real>(m_samplesPerSymbol) * m_afcLength / m_sampleRate));
    const int maxBin = static_cast<int>(2.0f * m_maxFrequency * m_afcLength / m_sampleRate);
    Real centerPower = 0.0f;
    Real sidePower = 0.0f;
    int sideBin = 0;

    for (int bin = centerBin - 1; bin <= centerBin + 1; ++bin) {
        centerPower = std::max(centerPower, afcPower(bin));
    }

    for (int side : {centerBin - offsetBins, centerBin + offsetBins})
    {
        for (int bin = side - 1; bin <= side + 1; ++bin)
        {
            if (std::abs(bin) > maxBin) {
                continue;
            }

            const Real power = afcPower(bin);
            if (power > sidePower)
            {
                sidePower = power;
                sideBin = bin;
            }
        }
    }

    if (sidePower <= m_afcIdleToneRatio * centerPower)
    {
        m_afcCandidateValid = false;
        return;
    }

    // Require consecutive estimates to agree before moving.
    const Real frequency = sideBin * m_sampleRate / (2.0f * m_afcLength);

    if (m_afcCandidateValid && (std::abs(frequency - m_afcCandidate) < m_afcRetuneHz)) {
        retune(frequency);
    }
    else
    {
        m_afcCandidate = frequency;
        m_afcCandidateValid = true;
    }
}

Real PSK31Demodulator::afcPower(int bin) const
{
    Complex sum(0.0f, 0.0f);
    const int step = ((bin % m_afcLength) + m_afcLength) % m_afcLength;
    int twiddleIndex = 0;

    for (int i = 0; i < m_afcLength; ++i)
    {
        sum += m_afcWindow[i] * m_afcBuffer[(m_afcIndex + i) % m_afcLength] * m_afcTwiddle[twiddleIndex];
        twiddleIndex = (twiddleIndex + step) % m_afcLength;
    }

    return std::norm(sum);
}

void PSK31Demodulator::retune(Real frequency)
{
    m_afcFrequency = frequency * 2.0f * M_PI / m_sampleRate;
    m_costasLoop.setFreq(0.0f);
    m_afcCandidateValid = false;

    if (m_locked)
    {
        // Symbols during the retuning transient are unreliable, and before it
        // the loop was on an idle tone, so there's nothing worth keeping.
        m_pendingCharacters.clear();
        m_decoder.reset();
    }
}

Complex PSK31Demodulator::matchedFilter(const Complex& sample)
{
    const int size = m_matchedFilterSamples.size();
    m_matchedFilterSamples[m_matchedFilterIndex] = sample;
    Complex sum(0.0f, 0.0f);
    int index = m_matchedFilterIndex;

    for (int i = 0; i < size; ++i)
    {
        sum += m_matchedFilterTaps[i] * m_matchedFilterSamples[index];
        index = (index == 0) ? size - 1 : index - 1;
    }

    m_matchedFilterIndex = (m_matchedFilterIndex + 1) % size;
    return sum;
}

void PSK31Demodulator::symbol(const Complex& value)
{
    // When the carrier is tracked, symbols lie on the real axis. Noise has no
    // preferred phase, so this averages to zero without a signal.
    const Real power = std::norm(value);
    const Real lockValue = power > 0.0f ? (value.real() * value.real() - value.imag() * value.imag()) / power : 0.0f;
    m_lockMetric += m_lockMetricAlpha * (lockValue - m_lockMetric);
    const bool lockDetected = m_lockMetric > m_lockThreshold;

    // The lock metric responds slowly when a signal ends, particularly as the
    // Costas loop partly tracks noise. A drop in power from the level seen
    // while locked shows that the signal has gone (or faded) much sooner.
    // Average over four symbols, as power over shorter periods varies with
    // the data: phase reversals have half the power of steady carrier.
    const Real level = m_signalLevelAverage.asDouble();
    const bool signalPresent = lockDetected && (level > m_signalLossRatio * m_lockedLevel);
    // With hysteresis, so noise at low SNR doesn't look like the signal returning
    const bool signalReturned = lockDetected && (level > m_signalReturnRatio * m_lockedLevel);

    // Qualify lock at symbol rate. Noise can briefly cross the lock threshold,
    // so require sustained acquisition before accepting bits. Once acquired,
    // ride through short fades: characters decoded while the signal is absent
    // are held back, then released if it returns or dropped if it doesn't.
    m_lockCount = lockDetected ? std::min(m_lockCount + 1, m_lockConfirmSymbols) : 0;
    m_presentCount = signalReturned ? std::min(m_presentCount + 1, m_relockConfirmSymbols) : 0;
    const bool decision = value.real() >= 0.0f;

    if (!m_locked)
    {
        if (m_lockCount >= m_lockConfirmSymbols)
        {
            m_locked = true;
            m_lockedLevel = level;
            // Start a fresh one-second measurement at acquisition. Otherwise
            // the first locked report includes pre-lock samples and reads low.
            resetSNR();
            m_snrAverageValid = false;
            m_reportSampleCount = 0;

            // Lock takes a while to confirm, and a transmission may not start
            // with much idle, so decode the symbols received while confirming.
            m_decoder.reset();
            for (bool previous : m_acquisitionSymbols) {
                decode(previous);
            }
        }
        else
        {
            m_acquisitionSymbols.push_back(decision);
            if ((int) m_acquisitionSymbols.size() >= m_lockConfirmSymbols) {
                m_acquisitionSymbols.pop_front();
            }
        }
    }
    else if (m_unlockCount > 0)
    {
        // Noise alone can briefly raise the lock detector, so require a few
        // consecutive symbols before accepting that the signal is back.
        if (m_presentCount >= m_relockConfirmSymbols) {
            m_unlockCount = 0;
        }
        else if (++m_unlockCount >= m_unlockConfirmSymbols)
        {
            unlock();
        }
    }
    else if (!signalPresent)
    {
        m_unlockCount = 1;
    }

    if (m_locked && (m_unlockCount == 0)) {
        m_lockedLevel += m_lockedLevelAlpha * (level - m_lockedLevel);
    }

    if (m_locked)
    {
        decode(decision);

        // The lock detector takes a few symbols to respond when a signal ends,
        // so only output characters once lock has been held for that long
        // after them. Otherwise noise at the end of every transmission would
        // be decoded. Output is paused during a fade.
        if (m_unlockCount == 0)
        {
            for (auto& pending : m_pendingCharacters) {
                --pending.second;
            }
            while (!m_pendingCharacters.empty() && (m_pendingCharacters.front().second <= 0))
            {
                emitCharacter(m_pendingCharacters.front().first);
                m_pendingCharacters.pop_front();
            }
        }
    }
    else
    {
        m_decoder.reset();
    }
}

void PSK31Demodulator::decode(bool symbol)
{
    QChar character;
    if (m_decoder.decodeSymbol(symbol, character)) {
        m_pendingCharacters.push_back({character, m_characterDelaySymbols});
    }
}

void PSK31Demodulator::emitCharacter(QChar character)
{
    if (m_characterCallback) {
        m_characterCallback(character);
    }
}

void PSK31Demodulator::unlock()
{
    m_locked = false;
    m_unlockCount = 0;
    // The lock metric decays slowly, so it may still be above threshold.
    // Require fresh evidence of a signal, rather than relocking on noise.
    m_lockMetric = 0.0f;
    m_lockCount = 0;
    m_presentCount = 0;
    m_pendingCharacters.clear();
    m_acquisitionSymbols.clear();
    m_decoder.reset();
    resetSNR();
    m_snr = 0.0f;
}

void PSK31Demodulator::resetSNR()
{
    m_snrInPhaseSum = 0.0;
    m_snrQuadratureSum = 0.0;
    m_snrCount = 0;
}

float PSK31Demodulator::getFrequencyOffset() const
{
    return (m_afcFrequency + m_costasLoop.getFreq()) * m_sampleRate / (2.0f * M_PI);
}
