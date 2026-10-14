///////////////////////////////////////////////////////////////////////////////////
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

#include "selcaldecoder.h"
#include "dsp/fftengine.h"
#include <algorithm>
#include <cmath>
#include <utility>

// Modified Bessel function of the first kind, order 0, for the Kaiser window.
double SELCALDecoder::besselI0(double x)
{
    double sum = 1, term = 1;
    for (int m = 1; m < 50; ++m)
    {
        term *= (x / (2 * m)) * (x / (2 * m));
        sum += term;
        if (term < sum * 1e-12) {
            break;
        }
    }
    return sum;
}

// Hz by which the measured offsets of tones of the given frequencies may differ.
double SELCALDecoder::maxOffsetDifference(double frequency1, double frequency2)
{
    return m_toneTolerance * (frequency1 + frequency2) + m_offsetMeasurementError;
}

double SELCALDecoder::median(std::vector<double> values)
{
    if (values.empty()) {
        return 0;
    }
    auto middle = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}

const std::array<SELCALDecoder::Tone, SELCALDecoder::m_toneCount>& SELCALDecoder::tones()
{
    // ASRI SELCAL Users Guide, Rev E (10 September 2024), table 2-1.
    // Symbol order is deliberately independent of frequency order (SELCAL 32
    // inserts frequencies between the original tones).
    static const std::array<Tone, m_toneCount> table{
        {{'A', 312.6}, {'B', 346.7}, {'C', 384.6}, {'D', 426.6},  {'E', 473.2},  {'F', 524.8},  {'G', 582.1},  {'H', 645.7},
         {'J', 716.1}, {'K', 794.3}, {'L', 881.0}, {'M', 977.2},  {'P', 1083.9}, {'Q', 1202.3}, {'R', 1333.5}, {'S', 1479.1},
         {'T', 329.2}, {'U', 365.2}, {'V', 405.0}, {'W', 449.3},  {'X', 498.3},  {'Y', 552.7},  {'Z', 613.1},  {'1', 680.0},
         {'2', 754.2}, {'3', 836.6}, {'4', 927.9}, {'5', 1029.2}, {'6', 1141.6}, {'7', 1266.2}, {'8', 1404.4}, {'9', 1557.8}}};
    return table;
}

SELCALDecoder::SELCALDecoder() :
    m_fft(FFTEngine::create(""))
{
    m_fft->configure(m_fftSize, false);

    // Kaiser windowed halfband lowpass for 8 kHz to 4 kHz decimation: flat to
    // 1.7 kHz and more than 65 dB down from 2.3 kHz, where aliases would reach
    // the SELCAL band. Even taps other than the centre are zero.
    constexpr int centre = m_decimatorTaps / 2;
    constexpr double beta = 6.0;
    double sum = 0.5;
    m_halfbandCentre = 0.5;
    for (int j = 0; j < m_halfbandTaps; ++j)
    {
        int k = 2 * j + 1;
        double ratio = static_cast<double>(k) / centre;
        double window = besselI0(beta * std::sqrt(std::max(0.0, 1 - ratio * ratio))) / besselI0(beta);
        m_halfband[j] = std::sin(M_PI * k / 2) / (M_PI * k) * window;
        sum += 2 * m_halfband[j];
    }
    m_halfbandCentre /= sum;
    for (double& coefficient : m_halfband) {
        coefficient /= sum;
    }

    double windowSum = 0;
    for (int i = 0; i < m_windowSize; ++i)
    {
        m_window[i] = 0.5 - 0.5 * std::cos(2 * M_PI * i / (m_windowSize - 1));
        windowSum += m_window[i];
    }
    // A tone of amplitude A measures A^2 at its centre bin.
    m_powerScale = 4 / (windowSum * windowSum);

    double margin = m_maxFrequencyOffset + m_searchRange + 10;
    double lowest = tones()[0].m_frequency, highest = tones()[0].m_frequency;
    for (int i = 0; i < m_toneCount; ++i)
    {
        m_toneBins[i] = tones()[i].m_frequency / m_binWidth;
        lowest = std::min(lowest, tones()[i].m_frequency);
        highest = std::max(highest, tones()[i].m_frequency);
    }
    m_bandStart = static_cast<int>(std::floor((lowest - margin) / m_binWidth));
    m_bandEnd = static_cast<int>(std::ceil((highest + margin) / m_binWidth)) + 1;
    m_hops.resize(m_preRollHops + 1);
    for (auto& hop : m_hops) {
        hop.m_power.resize(m_bandEnd - m_bandStart);
    }
    m_scratch.resize(m_bandEnd - m_bandStart);
    m_raw.resize(m_bandEnd - m_bandStart);
    m_background.resize(m_bandEnd - m_bandStart);
    configure(0, m_defaultThreshold);
}

SELCALDecoder::~SELCALDecoder() = default;

void SELCALDecoder::configure(double frequencyOffset, double threshold)
{
    threshold = std::isfinite(threshold) ? std::clamp(threshold, 3.0, 30.0) : m_defaultThreshold;
    m_threshold = std::pow(10.0, threshold / 10);
    setFrequencyOffset(frequencyOffset);
    reset();
}

void SELCALDecoder::setFrequencyOffset(double frequencyOffset, double uncertainty)
{
    frequencyOffset = std::isfinite(frequencyOffset) ? std::clamp(frequencyOffset, -m_maxFrequencyOffset, m_maxFrequencyOffset) : 0;
    uncertainty = std::isfinite(uncertainty) ? std::clamp(uncertainty, 0.0, m_searchRange) : m_searchRange;
    m_baseOffset = frequencyOffset / m_binWidth;
    m_searchBins = std::max(1, static_cast<int>(std::ceil(uncertainty / m_binWidth)));
}

void SELCALDecoder::reset()
{
    m_history.fill(0);
    m_historyPosition = 0;
    m_samples.fill(0);
    m_position = 0;
    m_levels.fill(-30);
    m_inputCount = 0;
    m_analysisCount = 0;
    m_hopCount = 0;
    m_hopIndex = 0;
    m_detected = {{-1, -1}};
    m_currentOffset = m_baseOffset;
    m_segment = Segment();
    m_pending = Burst();
    m_held = Burst();
    m_holding = false;
    m_called = Burst();
    m_blankLevel = 0;
    m_blankCount = 0;
    m_blankRun = 0;
    m_blankQuiet = 0;
    std::fill(m_background.begin(), m_background.end(), 0.0);
    m_backgroundNoise = 0;
    m_backgroundCount = 0;
    m_learnAfter = 0;
}

const char *SELCALDecoder::stateName(State state)
{
    switch (state)
    {
    case State::FirstPair:
        return "First pair";
    case State::Gap:
        return "Gap";
    case State::SecondPair:
        return "Second pair";
    case State::WaitForSilence:
        return "Waiting for silence";
    default:
        return "Idle";
    }
}

SELCALDecoder::State SELCALDecoder::state() const
{
    bool waiting = m_pending.m_valid || (m_holding && !brief(m_held));
    if (m_segment.m_active) {
        return m_segment.m_overlong ? State::WaitForSilence : waiting ? State::SecondPair : State::FirstPair;
    }
    return waiting ? State::Gap : State::Idle;
}

double SELCALDecoder::frequencyOffset() const
{
    double offset = m_segment.m_active ? m_segment.m_offset : m_holding ? m_held.m_offset : m_pending.m_valid ? m_pending.m_offset : m_currentOffset;
    return offset * m_binWidth;
}

void SELCALDecoder::feed(double sample)
{
    m_history[m_historyPosition] = m_history[m_historyPosition + m_decimatorTaps] = std::isfinite(sample) ? sample : 0;
    m_historyPosition = (m_historyPosition + 1) % m_decimatorTaps;
    if (++m_inputCount % 2) {
        return;
    }
    // Oldest to newest input.
    const double *history = &m_history[m_historyPosition];
    constexpr int centre = m_decimatorTaps / 2;
    double output = m_halfbandCentre * history[centre];
    for (int j = 0; j < m_halfbandTaps; ++j)
    {
        int k = 2 * j + 1;
        output += m_halfband[j] * (history[centre - k] + history[centre + k]);
    }
    if (m_sampleHandler) {
        m_sampleHandler(static_cast<float>(output));
    }
    blank(output);
    m_samples[m_position] = m_samples[m_position + m_windowSize] = output;
    m_position = (m_position + 1) % m_windowSize;
    if (++m_analysisCount >= m_windowSize && m_analysisCount % m_hopSize == 0) {
        analyse();
    }
}

int SELCALDecoder::bin(int tone, double offset) const
{
    return std::clamp(static_cast<int>(std::lround(m_toneBins[tone] + offset)), m_bandStart, m_bandEnd - 1) - m_bandStart;
}

const SELCALDecoder::Hop& SELCALDecoder::hop(int age) const
{
    int size = static_cast<int>(m_hops.size());
    return m_hops[(m_hopIndex - age + size) % size];
}

double SELCALDecoder::pairPower(const Hop& hop, const Pair& pair, double offset) const
{
    return hop.m_power[bin(pair[0], offset)] + hop.m_power[bin(pair[1], offset)];
}

void SELCALDecoder::analyse()
{
    Complex *input = m_fft->in();
    const double *samples = &m_samples[m_position];
    for (int n = 0; n < m_windowSize; ++n) {
        input[n] = Complex(static_cast<Real>(samples[n] * m_window[n]), 0);
    }
    std::fill(input + m_windowSize, input + m_fftSize, Complex{});
    m_fft->transform();

    m_hopIndex = (m_hopIndex + 1) % static_cast<int>(m_hops.size());
    Hop& current = m_hops[m_hopIndex];
    // Window centre in input samples, allowing for the decimator's delay.
    current.m_time = 2.0 * static_cast<double>(m_analysisCount) - 431;
    const Complex *output = m_fft->out();
    for (int b = m_bandStart; b < m_bandEnd; ++b) {
        current.m_power[b - m_bandStart] = static_cast<float>(std::norm(output[b]) * m_powerScale);
    }
    ++m_hopCount;

    // The median of the band is robust to the tones themselves. For white
    // noise each bin is exponentially distributed, so mean = median / ln 2.
    std::copy(current.m_power.begin(), current.m_power.end(), m_scratch.begin());
    auto middle = m_scratch.begin() + m_scratch.size() / 2;
    std::nth_element(m_scratch.begin(), middle, m_scratch.end());
    current.m_noise = std::max(static_cast<double>(*middle) / M_LN2, m_noiseFloor);

    // Remove steady signals, such as heterodynes, that were present before
    // any burst. Otherwise they would be taken as competing tones and prevent
    // detection. Tones received on top of them are still measured.
    std::copy(current.m_power.begin(), current.m_power.end(), m_raw.begin());
    if (m_backgroundCount > 0)
    {
        for (std::size_t b = 0; b < current.m_power.size(); ++b)
        {
            if (m_background[b] > m_steadyLevel * m_backgroundNoise)
            {
                // A strong steady signal's level varies slightly between analyses,
                // so a margin is subtracted to prevent residuals being detected.
                double power = current.m_power[b] - m_steadyMargin * (m_background[b] - m_backgroundNoise);
                current.m_power[b] = static_cast<float>(std::max(power, 1e-3 * current.m_noise));
            }
        }
    }

    // Candidate common tone offsets, in bins. While a burst is in progress,
    // its own offsets are searched as well as the expected offset, so that a
    // spurious burst cannot mask the start of a stronger one elsewhere.
    std::array<double, 64> candidates;
    std::array<bool, 64> inSegment;
    int candidateCount = 0;
    auto add = [&](double centre, int range, bool segment) {
        for (int c = -range; c <= range && candidateCount < static_cast<int>(candidates.size()); ++c)
        {
            inSegment[candidateCount] = segment;
            candidates[candidateCount++] = centre + c;
        }
    };
    if (m_segment.m_active) {
        add(m_segment.m_offset, m_anchorSearch, true);
    }
    add(m_baseOffset, m_searchBins, false);
    const Burst *previous = previousPair();
    if (previous) {
        add(previous->m_offset, m_anchorSearch, false);
    }

    bool found = false;
    Pair bestPair{{-1, -1}};
    double bestOffset = m_segment.m_active ? m_segment.m_offset : previous ? previous->m_offset : m_baseOffset;
    double bestSnr = 0;
    bool continued = false; // The burst in progress was detected again
    double continuedSnr = 0;
    double scale = 1.0 / current.m_noise;
    Peaks peaks;
    findPeaks(current.m_power, peaks);
    for (int c = 0; c < candidateCount; ++c)
    {
        Pair pair;
        double snr;
        if (!evaluate(current.m_power, peaks, scale, candidates[c], pair, snr)) {
            continue;
        }
        if (inSegment[c] && pair == m_segment.m_pair && snr > continuedSnr)
        {
            continued = true;
            continuedSnr = snr;
        }
        if (snr > bestSnr)
        {
            found = true;
            bestSnr = snr;
            bestOffset = candidates[c];
            bestPair = pair;
        }
    }
    if (continued)
    {
        m_detected = m_segment.m_pair;
        m_currentOffset = m_segment.m_offset;
    }
    else
    {
        m_detected = bestPair;
        m_currentOffset = bestOffset;
    }
    for (int i = 0; i < m_toneCount; ++i) {
        m_levels[i] = 10 * std::log10(std::max(current.m_power[bin(i, m_currentOffset)] * scale, 1e-3));
    }

    if (m_segment.m_active && m_segment.m_overlong)
    {
        // Too long for a SELCAL burst, so a steady signal. Its history is no
        // longer needed, and it is learnt as background (below) until it no
        // longer masks other signals.
        if (continued) {
            m_segment.m_missed = 0;
        } else if (++m_segment.m_missed > m_holdHops) {
            closeSegment();
        }
    }
    else if (m_segment.m_active)
    {
        m_segment.m_power.push_back(pairPower(current, m_segment.m_pair, m_segment.m_offset));
        m_segment.m_times.push_back(current.m_time);
        m_segment.m_detected.push_back(continued);
        if (current.m_time - m_segment.m_times[m_segment.m_first] > (m_maxBurst + 0.125) * m_sampleRate) {
            m_segment.m_overlong = true;
        }
        if (continued)
        {
            m_segment.m_last = static_cast<int>(m_segment.m_power.size()) - 1;
            m_segment.m_missed = 0;
            m_segment.m_snr.push_back(continuedSnr);
            accumulate(current);
        }
        else if (++m_segment.m_missed > m_holdHops) {
            // Ends the burst. A different pair detected in this hop may start the next one.
            closeSegment();
        }
    }
    if (!m_segment.m_active)
    {
        // The leading edge of a burst can be found up to the pre-roll before it is detected.
        double preRoll = m_preRollHops * m_hopSize * 2.0 / m_sampleRate;
        if (m_holding && current.m_time - m_held.m_end > (m_maxFadeGap + preRoll) * m_sampleRate) {
            releaseHeld();
        }
        // While a burst is held, it may be the second pair.
        if (m_pending.m_valid && !m_holding &&
            current.m_time - m_pending.m_end > (m_maxGap + (m_nominalBurst - m_minFadedBurst) + (m_nominalBurst - m_minBurst) + preRoll) * m_sampleRate) {
            m_pending.m_valid = false;
        }
        if (found) {
            startSegment(bestPair, bestOffset, bestSnr);
        }
    }
    bool idle = !found && !m_segment.m_active && !m_pending.m_valid && !(m_holding && !brief(m_held));
    bool steady = m_segment.m_active && m_segment.m_overlong;
    if ((idle || steady) && (m_inputCount >= m_learnAfter)) {
        learnBackground(current.m_noise);
    }
}

void SELCALDecoder::holdLearning(std::uint64_t samples)
{
    m_learnAfter = m_inputCount + samples;
}

void SELCALDecoder::learnBackground(double noise)
{
    // Running mean initially, then an exponential average.
    m_backgroundCount = std::min(m_backgroundCount + 1, m_backgroundHops);
    double alpha = 1.0 / m_backgroundCount;
    for (std::size_t b = 0; b < m_raw.size(); ++b) {
        m_background[b] += (m_raw[b] - m_background[b]) * alpha;
    }
    m_backgroundNoise += (noise - m_backgroundNoise) * alpha;
}

std::uint64_t SELCALDecoder::sampleIndex(double time)
{
    return static_cast<std::uint64_t>(std::max(0.0, std::round(time)));
}

void SELCALDecoder::findPeaks(const std::vector<float>& power, Peaks& peaks) const
{
    // Strongest local maxima of the band, strongest first.
    peaks.fill(-1);
    for (int b = 1; b + 1 < static_cast<int>(power.size()); ++b)
    {
        if (power[b] < power[b - 1] || power[b] < power[b + 1] || (peaks[m_peakCount - 1] >= 0 && power[b] <= power[peaks[m_peakCount - 1]])) {
            continue;
        }
        int i = m_peakCount - 1;
        while (i > 0 && (peaks[i - 1] < 0 || power[b] > power[peaks[i - 1]]))
        {
            peaks[i] = peaks[i - 1];
            --i;
        }
        peaks[i] = b;
    }
}

bool SELCALDecoder::evaluate(const std::vector<float>& power, const Peaks& peaks, double scale, double offset, Pair& pair, double& snr) const
{
    int first = -1, second = -1;
    double p1 = -1, p2 = -1, p3 = -1;
    for (int i = 0; i < m_toneCount; ++i)
    {
        double level = power[bin(i, offset)];
        if (level > p1)
        {
            p3 = p2;
            second = first;
            p2 = p1;
            first = i;
            p1 = level;
        }
        else if (level > p2)
        {
            p3 = p2;
            second = i;
            p2 = level;
        }
        else if (level > p3)
        {
            p3 = level;
        }
    }
    p1 *= scale;
    p2 *= scale;
    p3 *= scale;
    pair = {{std::min(first, second), std::max(first, second)}};
    snr = p2;
    // The strongest other signal in the band, away from the pair's main lobes.
    // Unlike the third tone, this does not depend on the offset: at another
    // offset, a three-tone chord could otherwise look like a different pair.
    double other = 0;
    int firstBin = bin(pair[0], offset), secondBin = bin(pair[1], offset);
    for (int peak : peaks)
    {
        if (peak >= 0 && std::abs(peak - firstBin) > m_mainLobeBins && std::abs(peak - secondBin) > m_mainLobeBins)
        {
            other = power[peak] * scale;
            break;
        }
    }
    // Both tones must stand out from the noise, and not differ too greatly in level.
    // A third tone or other signal that is itself significant makes the pair
    // ambiguous. The noise maximum across the band is allowed for.
    if (!(p2 >= m_threshold && p2 >= p1 * m_minBalance && p3 < std::max(p2 * 0.30, m_threshold) &&
          other < std::max(p2 * 0.30, 2 * m_threshold))) {
        return false;
    }
    // Each tone must be a spectral peak. The main lobe is wider than the
    // spacing of adjacent tones, so a single signal between two of them could
    // otherwise look like a pair.
    return peaked(power, pair[0], pair[1], offset) && peaked(power, pair[1], pair[0], offset);
}

int SELCALDecoder::peakRange(int tone, int partner) const
{
    // Bins either side of a tone to search for its peak, keeping clear of the
    // other tone of the pair.
    return std::min(m_anchorSearch + 2, static_cast<int>((std::abs(m_toneBins[partner] - m_toneBins[tone]) - 1) / 2));
}

bool SELCALDecoder::peaked(const std::vector<float>& power, int tone, int partner, double offset) const
{
    int range = peakRange(tone, partner);
    int centre = bin(tone, offset);
    int low = std::max(0, centre - range), high = std::min(static_cast<int>(power.size()) - 1, centre + range);
    int peak = low;
    for (int b = low; b <= high; ++b) {
        if (power[b] > power[peak]) {
            peak = b;
        }
    }
    return peak > low && peak < high;
}

void SELCALDecoder::startSegment(const Pair& pair, double offset, double snr)
{
    m_segment = Segment();
    m_segment.m_active = true;
    m_segment.m_pair = pair;
    m_segment.m_offset = offset;
    m_segment.m_acquired = offset;
    // Acquired around the expected offset, or that of the previous pair.
    bool expected = std::abs(offset - m_baseOffset) <= m_searchBins + 0.01;
    const Burst *previous = previousPair();
    m_segment.m_searchCentre = expected || !previous ? m_baseOffset : previous->m_offset;
    m_segment.m_searchBins = expected || !previous ? m_searchBins : m_anchorSearch;
    m_segment.m_spectrum.assign(m_bandEnd - m_bandStart, 0.0f);
    int history = static_cast<int>(std::min<std::uint64_t>(m_preRollHops, m_hopCount - 1));
    for (int age = history; age >= 0; --age)
    {
        const Hop& previous = hop(age);
        m_segment.m_power.push_back(pairPower(previous, pair, offset));
        m_segment.m_times.push_back(previous.m_time);
        m_segment.m_detected.push_back(age == 0);
    }
    m_segment.m_first = m_segment.m_last = history;
    m_segment.m_snr.push_back(snr);
    accumulate(hop(0));
}

void SELCALDecoder::accumulate(const Hop& current)
{
    double scale = 1.0 / current.m_noise;
    for (std::size_t b = 0; b < current.m_power.size(); ++b) {
        m_segment.m_spectrum[b] += static_cast<float>(current.m_power[b] * scale);
    }
    ++m_segment.m_spectrumHops;

    // The acquisition offset is measured on the burst's leading edge and is
    // coarse. Follow the tones' measured offset so they stay centred in the search.
    double sum = 0, weight = 0;
    for (int k = 0; k < 2; ++k) {
        measurePeak(current.m_power, m_segment.m_pair[k], m_segment.m_pair[1 - k], m_segment.m_offset, sum, weight);
    }
    m_segment.m_offsetSum += sum;
    m_segment.m_offsetWeight += weight;
    if (m_segment.m_offsetWeight > 0)
    {
        // Stay within the search, so the tones cannot be followed towards another pair.
        double range = m_segment.m_searchBins + 1;
        m_segment.m_offset = std::clamp(m_segment.m_offsetSum / m_segment.m_offsetWeight, m_segment.m_searchCentre - range, m_segment.m_searchCentre + range);
    }
}

void SELCALDecoder::measurePeak(const std::vector<float>& power, int tone, int partner, double offset, double& sum, double& weight) const
{
    // Interpolated spectral peak near the expected tone frequency, keeping
    // clear of the other tone of the pair.
    int size = static_cast<int>(power.size());
    int range = peakRange(tone, partner);
    int centre = bin(tone, offset);
    int low = std::max(1, centre - range), high = std::min(size - 2, centre + range);
    if (low > high) {
        return;
    }
    int peak = low;
    for (int b = low; b <= high; ++b) {
        if (power[b] > power[peak]) {
            peak = b;
        }
    }
    double left = std::log(std::max(static_cast<double>(power[peak - 1]), 1e-30));
    double centrePower = std::log(std::max(static_cast<double>(power[peak]), 1e-30));
    double right = std::log(std::max(static_cast<double>(power[peak + 1]), 1e-30));
    double curvature = left - 2 * centrePower + right;
    double fraction = curvature < -1e-12 ? std::clamp(0.5 * (left - right) / curvature, -0.5, 0.5) : 0;
    sum += power[peak] * (peak + m_bandStart + fraction - m_toneBins[tone]);
    weight += power[peak];
}

bool SELCALDecoder::identify(Pair& pair, double& offset) const
{
    // Identify the pair again from the spectrum averaged over the burst. Its
    // SNR is much higher than any single analysis, so it reliably distinguishes
    // the pair from a similar pair at a different offset.
    const Segment& s = m_segment;
    pair = s.m_pair;
    offset = s.m_offset;
    if (s.m_spectrumHops == 0) {
        return false;
    }
    double scale = 1.0 / s.m_spectrumHops;
    double best = 0;
    Peaks peaks;
    findPeaks(s.m_spectrum, peaks);
    auto consider = [&](double centre, int range) {
        for (int c = -range; c <= range; ++c)
        {
            Pair candidate;
            double snr;
            if (evaluate(s.m_spectrum, peaks, scale, centre + c, candidate, snr) && snr > best)
            {
                best = snr;
                pair = candidate;
                offset = centre + c;
            }
        }
    };
    consider(s.m_searchCentre, s.m_searchBins);
    // Both tones of a genuine pair share the offset, to within the tone
    // frequency tolerance. When tones are further from the expected offset
    // than the search, they can resemble other tones, but rarely with a
    // consistent offset.
    std::array<double, 2> sums{}, weights{};
    for (int k = 0; k < 2; ++k) {
        measurePeak(s.m_spectrum, pair[k], pair[1 - k], offset, sums[k], weights[k]);
    }
    if (weights[0] <= 0 || weights[1] <= 0) {
        return false;
    }
    double first = sums[0] / weights[0], second = sums[1] / weights[1];
    offset = std::clamp((sums[0] + sums[1]) / (weights[0] + weights[1]), offset - 1, offset + 1);
    double allowed = maxOffsetDifference(tones()[pair[0]].m_frequency, tones()[pair[1]].m_frequency);
    return std::abs(first - second) <= allowed / m_binWidth;
}

void SELCALDecoder::closeSegment()
{
    Segment& s = m_segment;
    s.m_active = false;

    // Burst edges are where the pair's amplitude is half its typical value:
    // the analysis window is then centred on the edge. This is independent of
    // signal level, unlike the first and last hops on which a pair is detected.
    std::vector<double> detectedPower;
    for (int i = s.m_first; i <= s.m_last; ++i) {
        if (s.m_detected[i]) {
            detectedPower.push_back(s.m_power[i]);
        }
    }
    double threshold = std::sqrt(median(detectedPower)) / 2;
    int count = static_cast<int>(s.m_power.size());
    auto amplitude = [&s](int i) {
        return std::sqrt(s.m_power[i]);
    };
    auto crossing = [&](int a, int b) {
        double difference = amplitude(b) - amplitude(a);
        double fraction = std::abs(difference) > 1e-30 ? (threshold - amplitude(a)) / difference : 0.5;
        return s.m_times[a] + (s.m_times[b] - s.m_times[a]) * std::clamp(fraction, 0.0, 1.0);
    };

    int i = s.m_first;
    if (amplitude(i) >= threshold) {
        while (i > 0 && amplitude(i - 1) >= threshold) {
            --i;
        }
    }
    else
    {
        while (i < s.m_last && amplitude(i) < threshold) {
            ++i;
        }
    }
    double start = i > 0 ? crossing(i - 1, i) : s.m_times[0];

    int j = s.m_last;
    if (amplitude(j) >= threshold) {
        while (j + 1 < count && amplitude(j + 1) >= threshold) {
            ++j;
        }
    }
    else
    {
        while (j > s.m_first && amplitude(j) < threshold) {
            --j;
        }
    }
    double end = j + 1 < count ? crossing(j, j + 1) : s.m_times[j];

    Burst burst;
    burst.m_consistent = identify(burst.m_pair, burst.m_offset);
    burst.m_overlong = s.m_overlong;
    burst.m_start = start;
    burst.m_end = end;
    burst.m_valid = validBurst(burst);
    burst.m_snr = 10 * std::log10(std::max(median(s.m_snr), 1e-3));
    burst.m_startSample = sampleIndex(start);
    segmentClosed(burst);
}

// Impulse blanker. Static crashes spread energy across the whole spectrum of an
// analysis window, masking the tones. Samples much larger than the recent mean
// magnitude are removed. An increase lasting longer than an impulse is a new
// signal, such as the start of a call, so is restored. The beats between two
// tones briefly reduce their level, so short gaps between impulses are part of
// the same increase.
void SELCALDecoder::blank(double& sample)
{
    double magnitude = std::abs(sample);
    if (m_blankLevel <= 0)
    {
        m_blankLevel = magnitude;
        return;
    }
    bool impulse = magnitude > m_impulseLevel * m_blankLevel;
    if (impulse)
    {
        m_blankCount = m_impulseHold;
        m_blankQuiet = 0;
    }
    else if ((m_blankRun > 0) && (++m_blankQuiet > m_impulseGap))
    {
        m_blankRun = 0;
    }
    if (impulse || (m_blankRun > 0))
    {
        if (m_blankRun == m_maxImpulse)
        {
            // Restore the samples removed, which haven't yet left the analysis
            // window, and follow the new level.
            double sum = 0;
            for (int i = 0; i < m_blankRun; ++i)
            {
                int position = (m_position - m_blankRun + i + m_windowSize) % m_windowSize;
                m_samples[position] = m_samples[position + m_windowSize] = m_blanked[i];
                sum += std::abs(m_blanked[i]);
            }
            m_blankLevel = std::max(m_blankLevel, sum / m_blankRun);
            m_blankRun = 0;
            m_blankCount = 0;
            return;
        }
        m_blanked[m_blankRun++] = sample;
    }
    else
    {
        // The mean is held while blanking, so that a new signal continues to be
        // blanked until it is recognised, and limited otherwise, so that
        // impulses don't raise it much.
        m_blankLevel += (std::min(magnitude, m_impulseLevel * m_blankLevel) - m_blankLevel) * m_impulseAveraging;
    }
    if (m_blankCount > 0)
    {
        --m_blankCount;
        sample = 0;
    }
}

bool SELCALDecoder::validBurst(const Burst& burst) const
{
    double duration = (burst.m_end - burst.m_start) / m_sampleRate;
    return burst.m_consistent && !burst.m_overlong && duration >= m_minFadedBurst && duration <= m_maxBurst;
}

bool SELCALDecoder::brief(const Burst& burst) const
{
    return burst.m_end - burst.m_start < m_minBurst / 2 * m_sampleRate;
}

// Whether a burst is part of the same burst as an earlier one, split by a fade.
bool SELCALDecoder::continues(const Burst& burst, const Burst& previous) const
{
    return burst.m_pair == previous.m_pair && std::abs(burst.m_offset - previous.m_offset) <= m_anchorSearch + 0.5 &&
           burst.m_start - previous.m_end <= m_maxFadeGap * m_sampleRate;
}

// The latest burst that a following pair may complete a call with.
const SELCALDecoder::Burst *SELCALDecoder::previousPair() const
{
    if (m_holding && m_held.m_consistent && !brief(m_held)) {
        return &m_held;
    }
    return m_pending.m_valid ? &m_pending : nullptr;
}

void SELCALDecoder::segmentClosed(const Burst& burst)
{
    // The rest of a call's second burst, after a fade, was already decoded.
    if (m_called.m_valid && continues(burst, m_called))
    {
        m_called.m_end = std::max(m_called.m_end, burst.m_end);
        return;
    }

    if (m_holding)
    {
        // A fade split the burst: join the parts.
        if (continues(burst, m_held))
        {
            // The longer part's measurements are the more reliable
            bool heldLonger = m_held.m_end - m_held.m_start >= burst.m_end - burst.m_start;
            const Burst& longer = heldLonger ? m_held : burst;
            Burst joined = burst;
            joined.m_start = m_held.m_start;
            joined.m_startSample = m_held.m_startSample;
            joined.m_offset = longer.m_offset;
            joined.m_consistent = longer.m_consistent;
            joined.m_overlong = m_held.m_overlong || burst.m_overlong;
            joined.m_snr = std::max(m_held.m_snr, burst.m_snr);
            joined.m_valid = validBurst(joined);
            m_held = joined;
            if (completesCall(m_held)) {
                releaseHeld();
            }
            return;
        }
        // Brief detections, from noise or the overlap of two pairs within the
        // analysis window, don't end a burst that may continue. Of brief
        // detections, the longest is the most likely to be part of a burst.
        if (brief(burst))
        {
            if (brief(m_held) && (burst.m_end - burst.m_start > m_held.m_end - m_held.m_start)) {
                m_held = burst;
            }
            return;
        }
        releaseHeld();
    }
    // A burst that completes a call is decoded without waiting for it to
    // continue. Others, including brief ones, are held in case they do.
    if (completesCall(burst))
    {
        burstEnded(burst);
        return;
    }
    m_held = burst;
    m_holding = true;
}

void SELCALDecoder::releaseHeld()
{
    Burst burst = m_held;
    m_holding = false;
    m_held = Burst();
    burstEnded(burst);
}

void SELCALDecoder::burstEnded(const Burst& burst)
{
    if (!burst.m_valid)
    {
        // Brief detections, from noise or the overlap of two pairs within the
        // analysis window, do not interrupt a sequence.
        if (burst.m_end - burst.m_start >= m_minBurst / 2 * m_sampleRate) {
            m_pending.m_valid = false;
        }
        return;
    }
    if (completesCall(burst))
    {
        Burst firstBurst = m_pending;
        m_pending.m_valid = false;
        m_called = burst;
        emitCall(firstBurst, burst);
        return;
    }
    m_pending = burst;
}

bool SELCALDecoder::completesCall(const Burst& burst) const
{
    if (!burst.m_valid || !m_pending.m_valid) {
        return false;
    }
    double gap = (burst.m_start - m_pending.m_end) / m_sampleRate;
    double firstDuration = (m_pending.m_end - m_pending.m_start) / m_sampleRate;
    double secondDuration = (burst.m_end - burst.m_start) / m_sampleRate;
    double shortfall = std::max(0.0, m_nominalBurst - firstDuration) + std::max(0.0, m_nominalBurst - secondDuration);
    // A fade may shorten one burst, but not both.
    bool durations = std::max(firstDuration, secondDuration) >= m_minBurst;
    const Pair& first = m_pending.m_pair;
    const Pair& second = burst.m_pair;
    bool distinct = second[0] != first[0] && second[0] != first[1] && second[1] != first[0] && second[1] != first[1];
    // Both pairs come from the same transmitter, so share a tone offset. A pair's
    // offset is a weighted mean of its tones' offsets, so may differ from the
    // transmitter's by up to the tolerance of its higher tone.
    double allowed = maxOffsetDifference(std::max(tones()[first[0]].m_frequency, tones()[first[1]].m_frequency),
                                         std::max(tones()[second[0]].m_frequency, tones()[second[1]].m_frequency));
    bool sameOffset = std::abs(burst.m_offset - m_pending.m_offset) <= allowed / m_binWidth;
    return gap >= m_minGap && gap <= m_maxGap + shortfall && durations && distinct && sameOffset;
}

void SELCALDecoder::emitCall(const Burst& first, const Burst& second)
{
    if (!m_handler) {
        return;
    }
    Call call;
    call.m_code = {tones()[first.m_pair[0]].m_symbol, tones()[first.m_pair[1]].m_symbol, '-', tones()[second.m_pair[0]].m_symbol,
                 tones()[second.m_pair[1]].m_symbol};
    call.m_startSample = first.m_startSample;
    call.m_firstEndSample = sampleIndex(first.m_end);
    call.m_secondStartSample = sampleIndex(second.m_start);
    call.m_endSample = sampleIndex(second.m_end);
    call.m_snr = std::min(first.m_snr, second.m_snr);
    call.m_frequencyOffset = (first.m_offset + second.m_offset) / 2 * m_binWidth;
    m_handler(call);
}
