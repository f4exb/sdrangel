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

#include "drmacquisition.h"
#include "drmofdm.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace modemdrm
{

namespace
{

constexpr double Pi = 3.14159265358979323846;

} // namespace

AcquisitionCandidate Acquisition::evaluateMode(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    Mode mode,
    bool alignFirstPath
)
{
    AcquisitionCandidate candidate;
    candidate.mode = mode;

    if (samples == nullptr || sampleRate <= 0) {
        return candidate;
    }

    const ModeParameters& parameters = Parameters::forMode(mode);
    const int useful = parameters.usefulSamples(sampleRate);
    const int guard = parameters.guardSamples(sampleRate);
    const int symbol = parameters.symbolSamples(sampleRate);

    candidate.usefulSamples = useful;
    candidate.guardSamples = guard;
    candidate.symbolSamples = symbol;

    if (useful <= 0 || guard <= 0 || symbol <= 0 || sampleCount < static_cast<std::size_t>(useful + 4 * symbol)) {
        return candidate;
    }

    const std::size_t pairCount = sampleCount - static_cast<std::size_t>(useful);
    std::vector<std::complex<double>> phaseCorrelation(symbol, {0.0, 0.0});
    std::vector<double> phaseEnergy(symbol, 0.0);
    std::vector<int> phaseCount(symbol, 0);

    // A DC component (a device's DC spur, which in A-D falls on the unused
    // carrier 0 when the channel offset is zero) correlates at phase zero at every lag and
    // would pull the fractional frequency estimate towards 0 Hz: remove the
    // block mean first.
    std::complex<double> sum(0.0, 0.0);
    for (std::size_t i = 0; i < sampleCount; ++i) {
        sum += static_cast<std::complex<double>>(samples[i]);
    }

    const std::complex<float> mean = static_cast<std::complex<float>>(sum / static_cast<double>(sampleCount));
    int phase = 0;
    for (std::size_t i = 0; i < pairCount; ++i)
    {
        const std::complex<float> first = samples[i] - mean;
        const std::complex<float> second = samples[i + useful] - mean;
        phaseCorrelation[phase] += static_cast<std::complex<double>>(first * std::conj(second));
        phaseEnergy[phase] += 0.5 * (std::norm(first) + std::norm(second));
        phaseCount[phase]++;
        if (++phase == symbol) {
            phase = 0;
        }
    }

    std::complex<double> windowCorrelation(0.0, 0.0);
    double windowEnergy = 0.0;
    int windowCount = 0;

    for (int i = 0; i < guard; ++i)
    {
        windowCorrelation += phaseCorrelation[i];
        windowEnergy += phaseEnergy[i];
        windowCount += phaseCount[i];
    }

    double bestScore = -1.0;
    std::complex<double> bestCorrelation(0.0, 0.0);
    int bestStart = 0;
    int bestCount = 0;
    std::vector<double> scores(static_cast<std::size_t>(symbol), 0.0);

    for (int start = 0; start < symbol; ++start)
    {
        const double score = windowEnergy > std::numeric_limits<double>::epsilon()
            ? std::abs(windowCorrelation) / windowEnergy
            : 0.0;

        scores[static_cast<std::size_t>(start)] = score;
        if (score > bestScore)
        {
            bestScore = score;
            bestCorrelation = windowCorrelation;
            bestStart = start;
            bestCount = windowCount;
        }

        const int leaving = start;
        const int entering = (start + guard) % symbol;
        windowCorrelation += phaseCorrelation[entering] - phaseCorrelation[leaving];
        windowEnergy += phaseEnergy[entering] - phaseEnergy[leaving];
        windowCount += phaseCount[entering] - phaseCount[leaving];
    }

    // Each path adds its power to the cyclic-prefix correlation over its own
    // guard-length span, so the guard window's maximum falls wherever the
    // stronger paths put it. Align to the first significant path instead, so
    // that the FFT window (at the end of the guard) holds every echo the guard
    // can; aligning to a later path lets the first path's next symbol into
    // the window. Walk back from the maximum along the (lightly smoothed)
    // per-offset profile while it stays above the noise floor plus a fraction
    // of its peak: a first path too weak to pass the threshold contributes
    // little interference. The centred smoothing spreads a lone path's
    // leading edge earlier, so its start comes out up to about half the box
    // early (inside the guard, and only until tracking takes over); undoing
    // that overshoots behind a weaker first path, where the profile rises
    // gradually.
    if (alignFirstPath)
    {
        std::vector<double> profile(static_cast<std::size_t>(symbol), 0.0);
        for (int phase = 0; phase < symbol; ++phase)
        {
            const double energy = phaseEnergy[static_cast<std::size_t>(phase)];
            profile[static_cast<std::size_t>(phase)] = energy > std::numeric_limits<double>::epsilon()
                ? std::abs(phaseCorrelation[static_cast<std::size_t>(phase)]) / energy : 0.0;
        }

        const int half = std::max(2, guard / 32);
        std::vector<double> smoothed(static_cast<std::size_t>(symbol), 0.0);
        for (int phase = 0; phase < symbol; ++phase)
        {
            double sum = 0.0;
            for (int k = -half; k < half; ++k) {
                sum += profile[static_cast<std::size_t>((phase + k + symbol) % symbol)];
            }

            smoothed[static_cast<std::size_t>(phase)] = sum / (2 * half);
        }

        // Noise floor from a low percentile: with a long guard and long echoes
        // most offsets carry correlation, so the median would be signal.
        std::vector<double> sorted = smoothed;
        const auto floorIndex = static_cast<std::size_t>(symbol / 10);
        std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(floorIndex), sorted.end());
        const double noise = sorted[floorIndex];
        double peak = 0.0;
        for (int k = 0; k < guard; ++k) {
            peak = std::max(peak, smoothed[static_cast<std::size_t>((bestStart + k) % symbol)]);
        }

        // Above the noise's spread as well as its floor: the averaged noise
        // magnitude stays within about twice its 10th percentile.
        const double threshold = std::max(NoiseMargin * noise, noise + FirstPathFraction * (peak - noise));
        // Tolerate brief dips: stop only after a run below the threshold.
        const int allowedGap = std::max(2, guard / 16);
        int earliestStart = bestStart;
        int gap = 0;
        for (int step = 1; step < guard && gap < allowedGap; ++step)
        {
            const int start = (bestStart - step + symbol) % symbol;
            if (smoothed[static_cast<std::size_t>(start)] >= threshold)
            {
                earliestStart = start;
                gap = 0;
            }
            else
            {
                ++gap;
            }
        }

        // The FFT window starts a backoff before the end of the guard. The
        // profile there is the power of the paths whose cyclic prefix covers
        // that start, i.e. that reach the window free of interference. When
        // the echoes span more than the guard allows, timing on the first
        // path can push a stronger later path out: move later until the
        // window start holds most of the best achievable power.
        const int windowOffset = guard - OFDMDemodulator::backoffSamplesFor(guard);
        double best = 0.0;
        for (double value : smoothed) {
            best = std::max(best, value);
        }

        for (int step = 0; step < guard; ++step)
        {
            const int start = (earliestStart + step) % symbol;
            if (smoothed[static_cast<std::size_t>((start + windowOffset) % symbol)] >= WindowPowerFraction * best)
            {
                earliestStart = start;
                break;
            }
        }

        bestStart = earliestStart;
    }
    else
    {
        // Where the FFT window may start: the span around the per-offset
        // profile's maximum within WindowPowerFraction of it, where every
        // significant path's cyclic prefix covers the window start.
        const int half = std::max(2, guard / 32);
        std::vector<double> smoothed(static_cast<std::size_t>(symbol), 0.0);
        for (int phase = 0; phase < symbol; ++phase)
        {
            double sum = 0.0;
            for (int k = -half; k < half; ++k)
            {
                const auto index = static_cast<std::size_t>((phase + k + symbol) % symbol);
                const double energy = phaseEnergy[index];
                if (energy > std::numeric_limits<double>::epsilon()) {
                    sum += std::abs(phaseCorrelation[index]) / energy;
                }
            }

            smoothed[static_cast<std::size_t>(phase)] = sum / (2 * half);
        }

        int peakPhase = 0;
        for (int phase = 1; phase < symbol; ++phase)
        {
            if (smoothed[static_cast<std::size_t>(phase)] > smoothed[static_cast<std::size_t>(peakPhase)]) {
                peakPhase = phase;
            }
        }

        const double limit = WindowPowerFraction * smoothed[static_cast<std::size_t>(peakPhase)];
        int before = 0;
        while (before < guard && smoothed[static_cast<std::size_t>((peakPhase - before - 1 + symbol) % symbol)] >= limit) {
            ++before;
        }

        int after = 0;
        while (after < guard && smoothed[static_cast<std::size_t>((peakPhase + after + 1) % symbol)] >= limit) {
            ++after;
        }

        candidate.windowEndOffsetSamples = (peakPhase + after) % symbol;
        candidate.windowSpanSamples = before + after + 1;
    }

    candidate.cyclicPrefixCorrelation = static_cast<float>(
        std::max(0.0, std::min(1.0, bestScore))
    );
    {
        // A low percentile: strong echoes raise the correlation over much of
        // the symbol, so the median can lie within the cyclic prefix's span.
        const auto floor = scores.begin() + static_cast<std::ptrdiff_t>(scores.size() / 10U);
        std::nth_element(scores.begin(), floor, scores.end());
        candidate.cyclicPrefixFloor = static_cast<float>(std::max(0.0, *floor));
    }

    candidate.valid = bestCount >= 4 * guard
        && candidate.cyclicPrefixCorrelation >= MinimumCyclicPrefixCorrelation
        && candidate.cyclicPrefixCorrelation - candidate.cyclicPrefixFloor
            >= MinimumCyclicPrefixExcess * (1.0f - candidate.cyclicPrefixFloor);
    candidate.symbolStartOffsetSamples = bestStart;
    candidate.symbolsObserved = guard > 0 ? bestCount / guard : 0;
    candidate.fractionalCarrierOffsetHz = -std::arg(bestCorrelation) / (2.0 * Pi * parameters.usefulDurationSeconds());

    return candidate;
}

std::vector<int> Acquisition::integerCarrierHypotheses(
    Mode mode,
    double fractionalCarrierOffsetHz,
    int configuredSearchRangeHz
)
{
    const int range = Parameters::effectiveSearchRangeHz(mode, configuredSearchRangeHz);

    if (range < 0) {
        return {};
    }

    const double spacing = Parameters::forMode(mode).carrierSpacingHz();
    // Floating-point IQ rotation/CP estimation can put an exact endpoint a few
    // microhertz outside the inclusive bound. Keep a 10-millihertz numerical
    // allowance so the endpoint's carrier hypothesis is still examined.
    constexpr double EndpointRoundingHz = 0.01;
    const int minimum = static_cast<int>(std::ceil((-range - EndpointRoundingHz - fractionalCarrierOffsetHz) / spacing));
    const int maximum = static_cast<int>(std::floor((range + EndpointRoundingHz - fractionalCarrierOffsetHz) / spacing));
    std::vector<int> hypotheses;

    if (minimum > maximum) {
        return hypotheses;
    }

    hypotheses.reserve(static_cast<std::size_t>(maximum - minimum + 1));
    const int nearest = std::max(minimum, std::min(maximum,
        static_cast<int>(std::llround(-fractionalCarrierOffsetHz / spacing))));
    hypotheses.push_back(nearest);

    for (int distance = 1; nearest - distance >= minimum || nearest + distance <= maximum; ++distance)
    {
        if (nearest - distance >= minimum) {
            hypotheses.push_back(nearest - distance);
        }

        if (nearest + distance <= maximum) {
            hypotheses.push_back(nearest + distance);
        }
    }

    return hypotheses;
}

} // namespace modemdrm
