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

#include "drmchannelestimator.h"

#include "drmcellmap.h"
#include "sdrbase/dsp/kissfft.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>

#include <stdexcept>

namespace modemdrm
{

namespace
{

constexpr double Pi = 3.14159265358979323846;

std::complex<float> binForCarrier(const std::vector<std::complex<float>>& bins, int carrier)
{
    const int size = static_cast<int>(bins.size());
    int index = carrier % size;
    if (index < 0) {
        index += size;
    }

    return bins[static_cast<std::size_t>(index)];
}

std::complex<float> pilotValue(const PilotCell& pilot)
{
    const double phase = 2.0 * Pi * pilot.phase1024 / 1024.0;
    return pilot.amplitude * std::complex<float>(
        static_cast<float>(std::cos(phase)),
        static_cast<float>(std::sin(phase))
    );
}

// The transmitted value of each reference cell of a symbol, in
// CellMap::referenceCells order, computed once per symbol position.
const std::vector<std::complex<float>>& referenceValues(
    Mode mode, int occupancy, int symbol, int frameInSuperframe)
{
    static std::mutex mutex;
    static std::map<int, std::vector<std::complex<float>>> cache;
    const int key = ((static_cast<int>(mode) * 8 + occupancy) * 64 + symbol) * 8 + frameInSuperframe;
    std::lock_guard<std::mutex> lock(mutex);
    auto found = cache.find(key);
    if (found == cache.end())
    {
        std::vector<std::complex<float>> values;
        for (const PilotCell& pilot : CellMap::referenceCells(mode, occupancy, symbol, frameInSuperframe)) {
            values.push_back(pilotValue(pilot));
        }

        found = cache.emplace(key, std::move(values)).first;
    }

    return found->second;
}

using TimedPilot = ChannelPilotObservation;

// Regression weights by symbol distance: temporalCorrelation^|time| for the
// twelve symbols either side used below.
using TemporalWeights = std::array<double, 13>;

TemporalWeights temporalWeights(float temporalCorrelation)
{
    TemporalWeights weights {};
    for (int time = 0; time < static_cast<int>(weights.size()); ++time)
    {
        weights[static_cast<std::size_t>(time)] = std::pow(
            static_cast<double>(temporalCorrelation), time
        );
    }

    return weights;
}

// Removes the common phase advance between symbols (residual carrier offset)
// relative to a target symbol. The rate comes from successive observations of
// the same pilot carriers: per-symbol estimates interpolated across the band
// alias on a frequency-selective channel, and correlating those gives
// arbitrary phases.
class PhaseCorrections
{
public:
    PhaseCorrections(double phaseRate, int targetSequenceSymbol) :
        m_phaseRate(phaseRate),
        m_target(targetSequenceSymbol)
    {
    }

    std::complex<float> operator()(int sequenceSymbol) const
    {
        return std::polar(1.0f, static_cast<float>(-m_phaseRate * (sequenceSymbol - m_target)));
    }

private:
    double m_phaseRate;
    int m_target;
};

bool invertMatrix(std::vector<std::complex<double>>& matrix, std::size_t size);

// Wiener time interpolation: a Gaussian time correlation (fixed by its value
// one pilot period apart) and a noise to channel power ratio.
struct TimeWienerModel
{
    double periodCorrelation = 1.0;
    int period = 1;
    double noiseRatio = 0.01;
    double at(int lag) const
    {
        const double x = static_cast<double>(lag) / period;
        return std::pow(std::max(1.0e-6, periodCorrelation), x * x);
    }
};

// Wiener time weights by the observations' times relative to the target. For
// one model they depend on nothing else, so every pilot carrier and symbol
// with the same pattern of observations shares them (an empty vector records
// a singular system).
using TimeWienerWeights = std::map<std::vector<int>, std::vector<std::complex<double>>>;

std::complex<float> estimateTimedPilot(
    const std::vector<TimedPilot>& observations,
    int targetSequenceSymbol,
    const PhaseCorrections* phaseCorrections = nullptr,
    const TemporalWeights* weights = nullptr,
    int window = 12,
    const TimeWienerModel* timeModel = nullptr,
    TimeWienerWeights* timeWeights = nullptr
)
{
    const auto upper = std::lower_bound(
        observations.begin(), observations.end(), targetSequenceSymbol,
        [](const TimedPilot& observation, int symbol) {
            return observation.sequenceSymbol < symbol;
        }
    );
    const auto corrected = [phaseCorrections](const TimedPilot& observation) {
        return observation.channel * (phaseCorrections
            ? (*phaseCorrections)(observation.sequenceSymbol)
            : std::complex<float>(1.0f, 0.0f));
    };
    if (phaseCorrections && timeModel)
    {
        // Observations are chronological: only those within the window count.
        const auto first = std::lower_bound(
            observations.begin(), observations.end(), targetSequenceSymbol - window,
            [](const TimedPilot& observation, int symbol) {
                return observation.sequenceSymbol < symbol;
            }
        );
        std::vector<int> times;
        std::vector<std::complex<double>> values;
        for (auto observationIt = first; observationIt != observations.end(); ++observationIt)
        {
            const int time = observationIt->sequenceSymbol - targetSequenceSymbol;
            if (time > window) {
                break;
            }

            times.push_back(time);
            values.push_back(std::complex<double>(corrected(*observationIt)));
        }

        const std::size_t size = times.size();
        if (size >= 1U)
        {
            const auto solve = [&]() {
                std::vector<std::complex<double>> matrix(size * size);
                for (std::size_t i = 0; i < size; ++i)
                {
                    for (std::size_t j = 0; j < size; ++j) {
                        matrix[i * size + j] = timeModel->at(times[i] - times[j]);
                    }

                    matrix[i * size + i] += timeModel->noiseRatio;
                }

                std::vector<std::complex<double>> result;
                if (!invertMatrix(matrix, size)) {
                    return result;
                }

                result.assign(size, std::complex<double>(0.0, 0.0));
                for (std::size_t i = 0; i < size; ++i)
                {
                    for (std::size_t j = 0; j < size; ++j) {
                        result[i] += timeModel->at(times[j]) * matrix[j * size + i];
                    }
                }

                return result;
            };
            std::vector<std::complex<double>> uncached;
            const std::vector<std::complex<double>>* solved = &uncached;
            if (timeWeights)
            {
                auto found = timeWeights->find(times);
                if (found == timeWeights->end()) {
                    found = timeWeights->emplace(times, solve()).first;
                }

                solved = &found->second;
            }
            else
            {
                uncached = solve();
            }

            if (solved->size() == size)
            {
                std::complex<double> estimate(0.0, 0.0);
                for (std::size_t i = 0; i < size; ++i) {
                    estimate += (*solved)[i] * values[i];
                }

                return static_cast<std::complex<float>>(estimate);
            }
        }
    }

    if (phaseCorrections && weights)
    {
        double sumWeight = 0.0;
        double sumTime = 0.0;
        double sumTimeSquared = 0.0;
        std::complex<double> sumChannel(0.0, 0.0);
        std::complex<double> sumTimeChannel(0.0, 0.0);
        // Observations are chronological; only the twelve symbols either side
        // of the target contribute.
        const auto first = std::lower_bound(
            observations.begin(), observations.end(), targetSequenceSymbol - window,
            [](const TimedPilot& observation, int symbol) {
                return observation.sequenceSymbol < symbol;
            }
        );
        for (auto observationIt = first; observationIt != observations.end(); ++observationIt)
        {
            const TimedPilot& observation = *observationIt;
            const int time = observation.sequenceSymbol - targetSequenceSymbol;
            if (time > window) {
                break;
            }

            const double weight = (*weights)[static_cast<std::size_t>(std::abs(time))];
            const std::complex<double> channel = corrected(observation);
            sumWeight += weight;
            sumTime += weight * time;
            sumTimeSquared += weight * time * time;
            sumChannel += weight * channel;
            sumTimeChannel += weight * time * channel;
        }

        const double denominator = sumWeight * sumTimeSquared
            - sumTime * sumTime;
        if (sumWeight > 1.0e-20 && std::abs(denominator) > 1.0e-20)
        {
            return static_cast<std::complex<float>>(
                (sumTimeSquared * sumChannel - sumTime * sumTimeChannel)
                    / denominator
            );
        }

        if (sumWeight > 1.0e-20) {
            return static_cast<std::complex<float>>(sumChannel / sumWeight);
        }
    }

    if (upper == observations.begin()) {
        return corrected(*upper);
    }

    if (upper == observations.end()) {
        return corrected(observations.back());
    }

    if (upper->sequenceSymbol == targetSequenceSymbol) {
        return corrected(*upper);
    }

    const TimedPilot& right = *upper;
    const TimedPilot& left = *(upper - 1);
    const float fraction = static_cast<float>(
        targetSequenceSymbol - left.sequenceSymbol
    ) / static_cast<float>(right.sequenceSymbol - left.sequenceSymbol);
    const std::complex<float> leftChannel = corrected(left);
    const std::complex<float> rightChannel = corrected(right);
    return leftChannel + fraction * (rightChannel - leftChannel);
}

// FFT plans by size and direction, built once per thread rather than per
// symbol.
kissfft<float, std::complex<float>>& fftPlan(int size, bool inverse)
{
    thread_local std::map<std::pair<int, bool>, std::unique_ptr<kissfft<float, std::complex<float>>>> plans;
    auto& plan = plans[{size, inverse}];
    if (!plan) {
        plan = std::make_unique<kissfft<float, std::complex<float>>>(size, inverse);
    }

    return *plan;
}

bool dftInterpolate(
    const ModeParameters& parameters,
    int carrierCount,
    int pilotSpacing,
    const std::vector<std::complex<float>>& pilots,
    std::vector<std::complex<float>>& interpolated,
    float* noiseRatio = nullptr
)
{
    if (carrierCount <= 0 || pilotSpacing <= 0 || pilots.size() < 2U) {
        return false;
    }

    const int shortLength = static_cast<int>(pilots.size());
    const int longLength = carrierCount + pilotSpacing - 1;
    if (longLength < shortLength) {
        return false;
    }

    std::vector<std::complex<float>> delay(static_cast<std::size_t>(shortLength));
    fftPlan(shortLength, true).transform(pilots.data(), delay.data());
    // The guard interval in delay taps of the pilot transform.
    const int guardTaps = static_cast<int>(std::floor(
        static_cast<double>(carrierCount) * parameters.guardTimeUnits
            / parameters.usefulTimeUnits
    ));
    if (guardTaps < 1) {
        return false;
    }

    // Paths arrive from about zero delay (the FFT window backoff is phase
    // corrected) up to the guard interval. Keep delays within the guard either
    // side, which also keeps the leakage of off-grid paths. When the guard is
    // more than half the pilot transform span (modes C and D, whose pilot
    // spacing is wide for their guard), a symmetric window would fold echoes
    // beyond half the span onto negative delays: keep the whole guard as
    // positive delay and the remaining taps as early ones instead.
    const bool longGuard = guardTaps > shortLength / 2;
    const int positiveLimit = longGuard ? std::min(shortLength - 1, guardTaps) : guardTaps;
    const int negativeLimit = longGuard ? shortLength - positiveLimit - 1 : guardTaps - 1;
    const auto signedDelay = [&](int tap) {
        return longGuard
            ? (tap < positiveLimit ? tap : tap - shortLength)
            : (tap <= shortLength / 2 ? tap : tap - shortLength);
    };
    const auto kept = [&](int tap) {
        const int signedTap = signedDelay(tap);
        return signedTap < positiveLimit && -signedTap <= negativeLimit;
    };

    // A path between the taps of this transform leaks into every tap, and
    // truncating to the guard window leaves an error that grows and shrinks
    // with the path's fractional position (1.6 dB of MER on a steady BBC
    // channel as the timing moved by single samples). Shift the strongest
    // kept path onto the grid (its fractional position by Jacobsen's
    // estimator) and restore the shift on the interpolated response.
    double alignment = 0.0;
    {
        int peak = 0;
        for (int tap = 1; tap < shortLength; ++tap)
        {
            if (!kept(tap)) {
                continue;
            }

            if (std::norm(delay[static_cast<std::size_t>(tap)]) > std::norm(delay[static_cast<std::size_t>(peak)])) {
                peak = tap;
            }
        }

        const auto at = [&](int tap) {
            return std::complex<double>(delay[static_cast<std::size_t>((tap % shortLength + shortLength) % shortLength)]);
        };
        const std::complex<double> denominator = 2.0 * at(peak) - at(peak - 1) - at(peak + 1);
        if (std::abs(denominator) > 1.0e-20) {
            alignment = std::clamp(std::real((at(peak - 1) - at(peak + 1)) / denominator), -0.5, 0.5);
        }

        if (alignment != 0.0)
        {
            std::vector<std::complex<float>> aligned = pilots;
            for (int i = 0; i < shortLength; ++i) {
                aligned[static_cast<std::size_t>(i)] *= std::polar(1.0f, static_cast<float>(2.0 * Pi * alignment * i / shortLength));
            }

            fftPlan(shortLength, true).transform(aligned.data(), delay.data());
        }
    }

    std::vector<float> delayPowers(delay.size());
    std::transform(
        delay.begin(), delay.end(), delayPowers.begin(),
        [](const std::complex<float>& value) { return std::norm(value); }
    );
    std::vector<float> sortedPowers = delayPowers;
    const auto lowerQuartile = sortedPowers.begin()
        + static_cast<std::ptrdiff_t>(sortedPowers.size() / 4U);
    std::nth_element(sortedPowers.begin(), lowerQuartile, sortedPowers.end());
    constexpr float ExponentialLowerQuartile = 0.28768207245f;
    // A finite occupied band spreads off-grid physical paths across delay
    // bins. Use the lower quartile as a conservative noise observation and
    // leave most of that deterministic leakage intact.
    constexpr float DelayNoiseScale = 0.1f;
    const float noiseFloor = DelayNoiseScale
        * *lowerQuartile / ExponentialLowerQuartile;
    const float averagePower = std::accumulate(
        delayPowers.begin(), delayPowers.end(), 0.0f
    ) / static_cast<float>(delayPowers.size());
    if (noiseRatio)
    {
        *noiseRatio = averagePower > 1.0e-20f
            ? noiseFloor / averagePower : 0.0f;
    }

    std::vector<std::complex<float>> padded(
        static_cast<std::size_t>(longLength), {0.0f, 0.0f}
    );
    for (int tap = 0; tap < shortLength; ++tap)
    {
        if (!kept(tap)) {
            continue;
        }

        const int signedTap = signedDelay(tap);
        const float power = delayPowers[static_cast<std::size_t>(tap)];
        const float wienerGain = power > 1.0e-20f
            ? std::max(0.0f, 1.0f - noiseFloor / power) : 0.0f;
        const int paddedTap = signedTap >= 0 ? signedTap : longLength + signedTap;
        if (paddedTap >= 0 && paddedTap < longLength)
        {
            padded[static_cast<std::size_t>(paddedTap)] =
                wienerGain * delay[static_cast<std::size_t>(tap)];
        }
    }

    std::vector<std::complex<float>> transformed(static_cast<std::size_t>(longLength));
    fftPlan(longLength, false).transform(padded.data(), transformed.data());
    interpolated.resize(static_cast<std::size_t>(carrierCount));
    for (int i = 0; i < carrierCount; ++i)
    {
        interpolated[static_cast<std::size_t>(i)] =
            transformed[static_cast<std::size_t>(i)] / static_cast<float>(shortLength)
            * std::polar(1.0f, static_cast<float>(-2.0 * Pi * alignment * i / (static_cast<double>(pilotSpacing) * shortLength)));
    }

    return true;
}

// Mode E places each symbol's gain references 16 carriers apart and steps
// them by 4 carriers a symbol. Its Doppler (up to about 130 Hz in the annex B
// channels) is too fast to interpolate the pilots over the 4-symbol pattern,
// so each symbol is interpolated from its own reference cells, which resolve
// delays over a span of N/16 samples (140 us). Linear interpolation is exact
// only for a flat channel; a frequency-domain Wiener interpolator follows the
// echoes within that span and averages the noise over the pilots. Its
// correlation model is a uniform delay profile over a window chosen from the
// pilots (see selectFrequencyWiener). Echoes beyond the span need the
// time/frequency estimator instead (see longEchoFraction).
struct SymbolPilots
{
    std::vector<int> carriers;
    std::vector<std::complex<float>> values;
    std::vector<float> amplitudes;
};

struct FrequencyWienerModel
{
    bool valid = false;
    double centre = 0.0;     // samples of the useful period
    double width = 0.0;      // samples of the useful period
    double noiseRatio = 0.0; // noise to channel power, for a pilot of FrequencyWienerReferenceAmplitude
};

constexpr int FrequencyWienerSpacing = 16;
constexpr float FrequencyWienerReferenceAmplitude = 1.41421356f;
constexpr double FrequencyWienerNoiseBias = 0.3;
// Echo energy beyond the per-symbol span, relative to that within it, above
// which mode E interpolates in time: about 0.003-0.023 (median) on the annex B
// channels 7 to 11 and 0.05-0.074 on the SFN channel, static or moving.
constexpr double ModeELongEchoFraction = 0.035;

std::complex<double> uniformDelayCorrelation(double lag, double centre, double width, int usefulSamples)
{
    const double x = lag * width / usefulSamples;
    const double sinc = std::abs(x) < 1.0e-9 ? 1.0 : std::sin(Pi * x) / (Pi * x);
    return std::polar(sinc, -2.0 * Pi * lag * centre / usefulSamples);
}

// Interpolation from one symbol's reference cells to every carrier: the
// estimate at carrier k is cross(k) * inverse * pilots. The coefficients
// (inverse * pilots) are solved once per symbol, which costs less than
// forming the weights when a pilot pattern serves few symbols. With a narrow
// delay window at high SNR the matrix is ill-conditioned (condition numbers
// near 1e5), so the inverse, the coefficients and their sum stay in double.
struct FrequencyWienerFilter
{
    std::size_t pilotCount = 0;
    std::vector<std::complex<double>> inverse; // pilotCount x pilotCount
    std::vector<std::complex<double>> cross;   // carrier-major, carriers x pilotCount

    void coefficients(const std::vector<std::complex<float>>& pilots, std::vector<std::complex<double>>& result) const
    {
        result.assign(pilotCount, {0.0, 0.0});
        for (std::size_t i = 0; i < pilotCount; ++i)
        {
            const std::complex<double>* row = inverse.data() + i * pilotCount;
            std::complex<double> sum(0.0, 0.0);
            for (std::size_t j = 0; j < pilotCount; ++j) {
                sum += row[j] * std::complex<double>(pilots[j]);
            }

            result[i] = sum;
        }
    }

    std::complex<float> value(int carrierIndex, const std::vector<std::complex<double>>& coefficients) const
    {
        const std::complex<double>* row = cross.data() + static_cast<std::size_t>(carrierIndex) * pilotCount;
        std::complex<double> sum(0.0, 0.0);
        for (std::size_t j = 0; j < pilotCount; ++j) {
            sum += row[j] * coefficients[j];
        }

        return static_cast<std::complex<float>>(sum);
    }
};

bool invertMatrix(std::vector<std::complex<double>>& matrix, std::size_t size)
{
    std::vector<std::complex<double>> inverse(size * size, {0.0, 0.0});
    for (std::size_t i = 0; i < size; ++i) {
        inverse[i * size + i] = 1.0;
    }

    for (std::size_t column = 0; column < size; ++column)
    {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < size; ++row)
        {
            if (std::abs(matrix[row * size + column]) > std::abs(matrix[pivot * size + column])) {
                pivot = row;
            }
        }

        if (std::abs(matrix[pivot * size + column]) < 1.0e-12) {
            return false;
        }

        if (pivot != column)
        {
            for (std::size_t k = 0; k < size; ++k)
            {
                std::swap(matrix[pivot * size + k], matrix[column * size + k]);
                std::swap(inverse[pivot * size + k], inverse[column * size + k]);
            }
        }

        const std::complex<double> scale = 1.0 / matrix[column * size + column];
        for (std::size_t k = 0; k < size; ++k)
        {
            matrix[column * size + k] *= scale;
            inverse[column * size + k] *= scale;
        }

        for (std::size_t row = 0; row < size; ++row)
        {
            if (row == column) {
                continue;
            }

            const std::complex<double> factor = matrix[row * size + column];
            if (factor == std::complex<double>(0.0, 0.0)) {
                continue;
            }

            for (std::size_t k = 0; k < size; ++k)
            {
                matrix[row * size + k] -= factor * matrix[column * size + k];
                inverse[row * size + k] -= factor * inverse[column * size + k];
            }
        }
    }

    matrix.swap(inverse);
    return true;
}

bool buildFrequencyWiener(
    const SymbolPilots& pilots,
    const FrequencyWienerModel& model,
    int usefulSamples,
    int minimumCarrier,
    int carrierCount,
    FrequencyWienerFilter& filter
)
{
    const std::size_t size = pilots.carriers.size();
    if (size < 2U || carrierCount < 1) {
        return false;
    }

    // The model depends only on the carrier lag.
    std::vector<std::complex<double>> correlation(static_cast<std::size_t>(2 * carrierCount - 1));
    for (int lag = -(carrierCount - 1); lag < carrierCount; ++lag)
    {
        correlation[static_cast<std::size_t>(lag + carrierCount - 1)] =
            uniformDelayCorrelation(lag, model.centre, model.width, usefulSamples);
    }

    const auto at = [&](int lag) { return correlation[static_cast<std::size_t>(lag + carrierCount - 1)]; };
    std::vector<std::complex<double>> matrix(size * size);
    for (std::size_t i = 0; i < size; ++i)
    {
        for (std::size_t j = 0; j < size; ++j) {
            matrix[i * size + j] = at(pilots.carriers[i] - pilots.carriers[j]);
        }

        const double amplitude = pilots.amplitudes[i];
        matrix[i * size + i] += model.noiseRatio
            * FrequencyWienerReferenceAmplitude * FrequencyWienerReferenceAmplitude
            / std::max(1.0e-6, amplitude * amplitude);
    }

    if (!invertMatrix(matrix, size)) {
        return false;
    }

    filter.pilotCount = size;
    filter.inverse = matrix;
    filter.cross.resize(static_cast<std::size_t>(carrierCount) * size);
    for (int index = 0; index < carrierCount; ++index)
    {
        for (std::size_t j = 0; j < size; ++j) {
            filter.cross[static_cast<std::size_t>(index) * size + j] = at(minimumCarrier + index - pilots.carriers[j]);
        }
    }

    return true;
}

// Whether a symbol's reference cells are only its gain references, evenly
// spaced (the symbols without time or AFS references).
bool regularPilots(const SymbolPilots& pilots)
{
    if (pilots.carriers.size() < 4U) {
        return false;
    }

    for (std::size_t i = 1; i < pilots.carriers.size(); ++i)
    {
        if (pilots.carriers[i] - pilots.carriers[i - 1] != FrequencyWienerSpacing) {
            return false;
        }
    }

    return true;
}

// Chooses the delay window and noise term. A tapered periodogram of each
// regular symbol's pilots gives the delay profile over the span the pilot
// spacing resolves; for each candidate width the window sits where it
// captures most of the profile's energy above its floor. The width and noise
// term are those whose interpolation of each symbol best predicts the pilots
// of the symbols one and two either side, which lie 4, 8 and 12 carriers from
// its own: the positions interpolation fills. A common phase is fitted to each
// prediction (residual frequency offset), and the channel's change over those
// symbols adds the same error to every candidate.
FrequencyWienerModel selectFrequencyWiener(
    const std::vector<SymbolPilots>& symbols,
    int usefulSamples,
    int minimumCarrier,
    int carrierCount
)
{
    FrequencyWienerModel best;
    const double span = static_cast<double>(usefulSamples) / FrequencyWienerSpacing;
    constexpr double DelayStep = 0.25;
    const int delayBins = static_cast<int>(std::lround(span / DelayStep));
    std::vector<double> profile(static_cast<std::size_t>(delayBins), 0.0);
    std::vector<std::size_t> regular;
    for (std::size_t index = 0; index < symbols.size(); ++index)
    {
        if (regularPilots(symbols[index])) {
            regular.push_back(index);
        }
    }

    if (regular.size() < 4U) {
        return best;
    }

    // Periodogram phasors by carrier and delay bin.
    std::vector<std::complex<double>> phasors(static_cast<std::size_t>(carrierCount) * static_cast<std::size_t>(delayBins));
    for (int index = 0; index < carrierCount; ++index)
    {
        for (int bin = 0; bin < delayBins; ++bin)
        {
            const double delay = -span / 2.0 + bin * DelayStep;
            phasors[static_cast<std::size_t>(index) * static_cast<std::size_t>(delayBins) + static_cast<std::size_t>(bin)] =
                std::polar(1.0, 2.0 * Pi * (minimumCarrier + index) * delay / usefulSamples);
        }
    }

    for (std::size_t index : regular)
    {
        const SymbolPilots& pilots = symbols[index];
        const std::size_t count = pilots.carriers.size();
        double taperPower = 0.0;
        std::vector<double> taper(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const double s = std::sin(Pi * (static_cast<double>(i) + 0.5) / static_cast<double>(count));
            taper[i] = s * s;
            taperPower += taper[i] * taper[i];
        }

        std::vector<std::complex<double>> sums(static_cast<std::size_t>(delayBins), {0.0, 0.0});
        for (std::size_t i = 0; i < count; ++i)
        {
            const int carrierIndex = pilots.carriers[i] - minimumCarrier;
            if (carrierIndex < 0 || carrierIndex >= carrierCount) {
                continue;
            }

            const std::complex<double> value = taper[i] * std::complex<double>(pilots.values[i]);
            const std::complex<double>* row = phasors.data()
                + static_cast<std::size_t>(carrierIndex) * static_cast<std::size_t>(delayBins);
            for (int bin = 0; bin < delayBins; ++bin) {
                sums[static_cast<std::size_t>(bin)] += value * row[bin];
            }
        }

        for (int bin = 0; bin < delayBins; ++bin) {
            profile[static_cast<std::size_t>(bin)] += std::norm(sums[static_cast<std::size_t>(bin)]) / taperPower;
        }
    }

    std::vector<double> sorted = profile;
    const auto floorPosition = sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 5U);
    std::nth_element(sorted.begin(), floorPosition, sorted.end());
    const double floor = *floorPosition;
    std::vector<double> excess(profile.size());
    for (std::size_t bin = 0; bin < profile.size(); ++bin) {
        excess[bin] = std::max(0.0, profile[bin] - floor);
    }

    const double regularCount = static_cast<double>(regular.size());
    double zeroLag = 0.0;
    double zeroCount = 0.0;
    for (std::size_t index : regular)
    {
        for (std::size_t i = 0; i < symbols[index].values.size(); ++i)
        {
            if (std::abs(symbols[index].amplitudes[i] - FrequencyWienerReferenceAmplitude) >= 0.01f) {
                continue;
            }

            zeroLag += std::norm(std::complex<double>(symbols[index].values[i]));
            zeroCount += 1.0;
        }
    }

    if (zeroCount < 1.0) {
        return best;
    }

    zeroLag /= zeroCount;
    // The periodogram's floor is the noise per pilot (the taper is
    // normalized); an estimate only, refined by the validation below.
    const double floorNoise = floor / regularCount;
    const double floorRatio = zeroLag > floorNoise ? floorNoise / (zeroLag - floorNoise) : 1.0;

    static constexpr double Widths[] = {1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 11.0, 14.0, 18.0, 22.0};
    static constexpr double NoiseScales[] = {0.1, 0.3, 1.0, 3.0};
    // Every second symbol as a target is enough to rank the candidates.
    constexpr std::size_t ValidationStride = 2;
    // The window position capturing most excess energy for a width.
    const auto centreFor = [&](double width) {
        const int widthBins = std::max(1, static_cast<int>(std::lround(width / DelayStep)));
        double captured = 0.0;
        for (int k = 0; k < widthBins; ++k) {
            captured += excess[static_cast<std::size_t>(k % delayBins)];
        }

        double bestCaptured = captured;
        int bestStart = 0;
        for (int start = 1; start < delayBins; ++start)
        {
            captured += excess[static_cast<std::size_t>((start + widthBins - 1) % delayBins)]
                - excess[static_cast<std::size_t>(start - 1)];
            if (captured > bestCaptured)
            {
                bestCaptured = captured;
                bestStart = start;
            }
        }

        return std::remainder(-span / 2.0 + (bestStart + (widthBins - 1) / 2.0) * DelayStep, span);
    };
    // Squared error of the predicted neighbour pilots, after a common phase.
    const auto validationError = [&](const FrequencyWienerModel& candidate) {
        std::map<std::vector<int>, FrequencyWienerFilter> filters;
        std::vector<std::complex<double>> coefficients;
        double score = 0.0;
        for (std::size_t r = 0; r < regular.size(); r += ValidationStride)
        {
            const std::size_t index = regular[r];
            const SymbolPilots& pilots = symbols[index];
            auto found = filters.find(pilots.carriers);
            if (found == filters.end())
            {
                FrequencyWienerFilter filter;
                if (!buildFrequencyWiener(pilots, candidate, usefulSamples, minimumCarrier, carrierCount, filter)) {
                    return std::numeric_limits<double>::max();
                }

                found = filters.emplace(pilots.carriers, std::move(filter)).first;
            }

            const FrequencyWienerFilter& filter = found->second;
            filter.coefficients(pilots.values, coefficients);
            for (int offset : {-2, -1, 1, 2})
            {
                const long neighbour = static_cast<long>(index) + offset;
                if (neighbour < 0 || neighbour >= static_cast<long>(symbols.size())) {
                    continue;
                }

                const SymbolPilots& other = symbols[static_cast<std::size_t>(neighbour)];
                if (!regularPilots(other)) {
                    continue;
                }

                std::complex<double> cross(0.0, 0.0);
                double predictedPower = 0.0;
                double measuredPower = 0.0;
                for (std::size_t j = 0; j < other.carriers.size(); ++j)
                {
                    const int carrierIndex = other.carriers[j] - minimumCarrier;
                    if (carrierIndex < 0 || carrierIndex >= carrierCount) {
                        continue;
                    }

                    const std::complex<double> predicted(filter.value(carrierIndex, coefficients));
                    const std::complex<double> measured(other.values[j]);
                    cross += measured * std::conj(predicted);
                    predictedPower += std::norm(predicted);
                    measuredPower += std::norm(measured);
                }

                score += measuredPower + predictedPower - 2.0 * std::abs(cross);
            }
        }

        return score;
    };
    const auto candidateFor = [&](double width, double centre, double noiseScale) {
        FrequencyWienerModel candidate;
        candidate.valid = true;
        candidate.centre = centre;
        candidate.width = width;
        candidate.noiseRatio = std::clamp(floorRatio * noiseScale, 1.0e-4, 10.0);
        return candidate;
    };
    // The width at the periodogram's noise estimate, then the noise term at
    // that width.
    double bestScore = std::numeric_limits<double>::max();
    for (double width : Widths)
    {
        const FrequencyWienerModel candidate = candidateFor(width, centreFor(width), 1.0);
        const double score = validationError(candidate);
        if (score < bestScore)
        {
            bestScore = score;
            best = candidate;
        }
    }

    if (!best.valid) {
        return best;
    }

    const FrequencyWienerModel widthChoice = best;
    for (double noiseScale : NoiseScales)
    {
        if (noiseScale == 1.0) {
            continue;
        }

        const FrequencyWienerModel candidate = candidateFor(widthChoice.width, widthChoice.centre, noiseScale);
        const double score = validationError(candidate);
        if (score < bestScore)
        {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

// Replaces the per-symbol linear estimates of a mode E context with frequency
// Wiener interpolation from the same reference cells.
bool refineFrequencyWiener(
    Mode mode,
    int occupancy,
    int firstFrameInSuperframe,
    int integerCarrierOffset,
    const std::vector<std::vector<std::vector<std::complex<float>>>>& fftFrames,
    std::vector<EqualizedSymbol>& estimates,
    FrequencyWienerModel& model
)
{
    const ModeParameters& parameters = Parameters::forMode(mode);
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    if (!spectrum) {
        return false;
    }

    const int usefulSamples = parameters.usefulSamples(parameters.internalSampleRate);
    const int carrierCount = spectrum->maximumCarrier - spectrum->minimumCarrier + 1;
    std::vector<SymbolPilots> symbols(estimates.size());
    std::vector<const std::vector<std::complex<float>>*> bins(estimates.size(), nullptr);
    for (std::size_t frame = 0; frame < fftFrames.size(); ++frame)
    {
        const int framePhase = (firstFrameInSuperframe + static_cast<int>(frame)) % parameters.framesPerSuperframe;
        for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
        {
            const std::size_t index = frame * static_cast<std::size_t>(parameters.symbolsPerFrame)
                + static_cast<std::size_t>(symbol);
            const std::vector<PilotCell>& references = CellMap::referenceCells(mode, occupancy, symbol, framePhase);
            const std::vector<std::complex<float>>& expected = referenceValues(mode, occupancy, symbol, framePhase);
            const auto& symbolBins = fftFrames[frame][static_cast<std::size_t>(symbol)];
            SymbolPilots& pilots = symbols[index];
            for (std::size_t i = 0; i < references.size(); ++i)
            {
                pilots.carriers.push_back(references[i].carrier);
                pilots.values.push_back(binForCarrier(symbolBins, references[i].carrier + integerCarrierOffset) / expected[i]);
                pilots.amplitudes.push_back(references[i].amplitude);
            }

            bins[index] = &symbolBins;
        }
    }

    model = selectFrequencyWiener(symbols, usefulSamples, spectrum->minimumCarrier, carrierCount);
    // The time smoothing that follows (estimateFrame) averages noise as
    // well, so interpolate in frequency with less smoothing than the
    // per-symbol validation chooses: on the mode E sweeps a third of its
    // noise term gave 2% more superframes, a tenth or a wider window no more.
    model.noiseRatio *= FrequencyWienerNoiseBias;
    if (!model.valid) {
        return false;
    }

    std::map<std::vector<int>, FrequencyWienerFilter> filters;
    std::vector<std::complex<double>> coefficients;
    for (std::size_t index = 0; index < estimates.size(); ++index)
    {
        const SymbolPilots& pilots = symbols[index];
        std::vector<int> key = pilots.carriers;
        for (float amplitude : pilots.amplitudes) {
            key.push_back(static_cast<int>(std::lround(amplitude * 1000.0f)));
        }

        auto found = filters.find(key);
        if (found == filters.end())
        {
            FrequencyWienerFilter filter;
            if (!buildFrequencyWiener(pilots, model, usefulSamples, spectrum->minimumCarrier, carrierCount, filter)) {
                return false;
            }

            found = filters.emplace(std::move(key), std::move(filter)).first;
        }

        const FrequencyWienerFilter& filter = found->second;
        filter.coefficients(pilots.values, coefficients);
        EqualizedSymbol& output = estimates[index];
        output.minimumCarrier = spectrum->minimumCarrier;
        output.maximumCarrier = spectrum->maximumCarrier;
        output.channel.assign(static_cast<std::size_t>(carrierCount), {0.0f, 0.0f});
        output.cells.assign(static_cast<std::size_t>(carrierCount), {0.0f, 0.0f});
        for (int carrierIndex = 0; carrierIndex < carrierCount; ++carrierIndex)
        {
            const std::complex<float> channel = filter.value(carrierIndex, coefficients);
            output.channel[static_cast<std::size_t>(carrierIndex)] = channel;
            const float power = std::norm(channel);
            output.cells[static_cast<std::size_t>(carrierIndex)] = power > 1.0e-12f
                ? binForCarrier(*bins[index], spectrum->minimumCarrier + carrierIndex + integerCarrierOffset)
                    * std::conj(channel) / power
                : std::complex<float>(0.0f, 0.0f);
        }

        double residualPower = 0.0;
        double referencePower = 0.0;
        for (std::size_t j = 0; j < pilots.carriers.size(); ++j)
        {
            const std::complex<float> predicted = output.channel[
                static_cast<std::size_t>(pilots.carriers[j] - spectrum->minimumCarrier)];
            residualPower += std::norm(pilots.values[j] - predicted);
            referencePower += std::norm(pilots.values[j]);
        }

        output.pilotResidualPower = referencePower > 1.0e-20
            ? static_cast<float>(residualPower / referencePower) : 0.0f;
    }

    return true;
}

} // namespace

bool EqualizedSymbol::contains(int carrier) const
{
    return carrier >= minimumCarrier && carrier <= maximumCarrier
        && cells.size() == static_cast<std::size_t>(maximumCarrier - minimumCarrier + 1);
}

const std::complex<float>& EqualizedSymbol::cell(int carrier) const
{
    if (!contains(carrier)) {
        throw std::out_of_range("DRM carrier is outside equalized symbol");
    }

    return cells[static_cast<std::size_t>(carrier - minimumCarrier)];
}

bool ChannelEstimator::estimate(
    Mode mode,
    int occupancy,
    int symbol,
    int frameInSuperframe,
    int integerCarrierOffset,
    const std::vector<std::complex<float>>& fftBins,
    EqualizedSymbol& result
)
{
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    const int expectedFFTSize = Parameters::forMode(mode).usefulSamples(
        Parameters::forMode(mode).internalSampleRate
    );
    if (!spectrum || static_cast<int>(fftBins.size()) < expectedFFTSize) {
        return false;
    }

    const std::vector<PilotCell>& pilots = CellMap::referenceCells(
        mode, occupancy, symbol, frameInSuperframe
    );
    if (pilots.size() < 2) {
        return false;
    }

    // Mode, occupancy, symbol and frame are within the cell-map ranges here.
    const std::vector<std::complex<float>>& expectedValues = referenceValues(
        mode, occupancy, symbol, frameInSuperframe
    );

    struct Estimate { int carrier; std::complex<float> value; };
    std::vector<Estimate> pilotEstimates;
    pilotEstimates.reserve(pilots.size());
    for (std::size_t index = 0; index < pilots.size(); ++index)
    {
        const PilotCell& pilot = pilots[index];
        const std::complex<float>& expected = expectedValues[index];
        pilotEstimates.push_back({
            pilot.carrier,
            binForCarrier(fftBins, pilot.carrier + integerCarrierOffset) / expected
        });
    }

    result.minimumCarrier = spectrum->minimumCarrier;
    result.maximumCarrier = spectrum->maximumCarrier;
    const std::size_t cellCount = static_cast<std::size_t>(
        spectrum->maximumCarrier - spectrum->minimumCarrier + 1
    );
    result.channel.resize(cellCount);
    result.cells.resize(cellCount);

    std::size_t upper = 0;
    for (int carrier = spectrum->minimumCarrier; carrier <= spectrum->maximumCarrier; ++carrier)
    {
        while (upper < pilotEstimates.size() && pilotEstimates[upper].carrier < carrier) {
            ++upper;
        }

        std::complex<float> channel;
        if (upper == 0) {
            channel = pilotEstimates.front().value;
        } else if (upper == pilotEstimates.size()) {
            channel = pilotEstimates.back().value;
        }
        else
        {
            const Estimate& left = pilotEstimates[upper - 1];
            const Estimate& right = pilotEstimates[upper];
            const float fraction = static_cast<float>(carrier - left.carrier)
                / static_cast<float>(right.carrier - left.carrier);
            channel = left.value + fraction * (right.value - left.value);
        }

        const std::size_t outputIndex = static_cast<std::size_t>(carrier - spectrum->minimumCarrier);
        result.channel[outputIndex] = channel;
        const float power = std::norm(channel);
        result.cells[outputIndex] = power > 1.0e-12f
            ? binForCarrier(fftBins, carrier + integerCarrierOffset) * std::conj(channel) / power
            : std::complex<float>(0.0f, 0.0f);
    }

    double residualPower = 0.0;
    double referencePower = 0.0;
    std::size_t residualCount = 0;
    for (std::size_t i = 1; i + 1 < pilotEstimates.size(); ++i)
    {
        const Estimate& left = pilotEstimates[i - 1];
        const Estimate& middle = pilotEstimates[i];
        const Estimate& right = pilotEstimates[i + 1];
        const float fraction = static_cast<float>(middle.carrier - left.carrier)
            / static_cast<float>(right.carrier - left.carrier);
        const std::complex<float> predicted = left.value + fraction * (right.value - left.value);
        residualPower += std::norm(middle.value - predicted);
        referencePower += std::norm(middle.value);
        ++residualCount;
    }

    result.pilotResidualPower = residualCount == 0 || referencePower <= 1.0e-20
        ? 0.0f : static_cast<float>(residualPower / referencePower);
    return true;
}

bool ChannelEstimationContext::prepare(
    Mode mode,
    int occupancy,
    int firstFrameInSuperframe,
    int integerCarrierOffset,
    const std::vector<std::vector<std::vector<std::complex<float>>>>& fftFrames
)
{
    m_mode = mode;
    m_occupancy = occupancy;
    m_firstFrameInSuperframe = firstFrameInSuperframe;
    m_integerCarrierOffset = integerCarrierOffset;
    m_frames = &fftFrames;
    m_valid = false;
    m_initial.clear();
    m_wienerState = 0;
    m_pilotGrid.clear();
    m_pilotSpacing = 0;
    m_histories.clear();
    m_timeWienerWeights.clear();
    m_temporalCorrelation = 0.8f;
    m_pilotPeriod = 0;
    m_phaseRate = 0.0;
    m_pilotCorrelation = 1.0;
    m_pilotLag1 = 1.0;
    m_haveDelayWindow = false;
    m_timeInterpolation = 0;
    m_delayCentre = 0.0;
    m_delayWidth = 0.0;

    const ModeParameters& parameters = Parameters::forMode(mode);
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    if (!spectrum || fftFrames.empty()
        || firstFrameInSuperframe < 0
        || firstFrameInSuperframe >= parameters.framesPerSuperframe) {
        return false;
    }

    for (const auto& frame : fftFrames)
    {
        if (static_cast<int>(frame.size()) != parameters.symbolsPerFrame) {
            return false;
        }
    }

    const std::size_t frameSymbols = static_cast<std::size_t>(parameters.symbolsPerFrame);
    m_initial.resize(fftFrames.size() * frameSymbols);
    for (std::size_t frame = 0; frame < fftFrames.size(); ++frame)
    {
        const int framePhase = (firstFrameInSuperframe + static_cast<int>(frame))
            % parameters.framesPerSuperframe;
        for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
        {
            const std::size_t sequenceIndex = frame * frameSymbols
                + static_cast<std::size_t>(symbol);
            if (!ChannelEstimator::estimate(
                    mode, occupancy, symbol, framePhase, integerCarrierOffset,
                    fftFrames[frame][static_cast<std::size_t>(symbol)], m_initial[sequenceIndex]))
            {
                m_initial.clear();
                return false;
            }
        }
    }

    if (mode == Mode::E)
    {
        FrequencyWienerModel model;
        if (refineFrequencyWiener(mode, occupancy, firstFrameInSuperframe, integerCarrierOffset, fftFrames, m_initial, model))
        {
            m_haveDelayWindow = true;
            m_delayCentre = model.centre;
            m_delayWidth = model.width;
        }
    }

    m_valid = true;
    return true;
}

bool ChannelEstimationContext::estimateFrame(
    int targetFrame,
    std::vector<EqualizedSymbol>& result,
    int temporalRadius
) const
{
    if (!m_valid || targetFrame < 0
        || static_cast<std::size_t>(targetFrame) >= m_frames->size()
        || temporalRadius < 0) {
        return false;
    }

    const auto& fftFrames = *m_frames;
    const std::vector<EqualizedSymbol>& initial = m_initial;
    const ModeParameters& parameters = Parameters::forMode(m_mode);
    const SpectrumParameters* spectrum = Parameters::forSpectrum(m_mode, m_occupancy);
    const std::size_t sequenceSymbols = initial.size();

    result.assign(
        initial.begin() + static_cast<std::ptrdiff_t>(targetFrame * parameters.symbolsPerFrame),
        initial.begin() + static_cast<std::ptrdiff_t>((targetFrame + 1) * parameters.symbolsPerFrame)
    );
    const std::size_t cellCount = initial.front().channel.size();
    for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
    {
        EqualizedSymbol& output = result[static_cast<std::size_t>(symbol)];
        std::vector<std::complex<float>> phaseCorrections(
            sequenceSymbols, {1.0f, 0.0f}
        );
        const int targetSequenceSymbol = targetFrame * parameters.symbolsPerFrame + symbol;
        const int first = std::max(0, targetSequenceSymbol - temporalRadius);
        const int last = std::min(
            static_cast<int>(sequenceSymbols) - 1,
            targetSequenceSymbol + temporalRadius
        );
        for (int adjacent = first; adjacent <= last; ++adjacent)
        {
            if (adjacent == targetSequenceSymbol) {
                continue;
            }

            std::complex<double> correlation(0.0, 0.0);
            for (std::size_t index = 0; index < cellCount; ++index)
            {
                correlation += static_cast<std::complex<double>>(
                    initial[static_cast<std::size_t>(adjacent)].channel[index]
                    * std::conj(initial[static_cast<std::size_t>(targetSequenceSymbol)].channel[index])
                );
            }

            if (std::abs(correlation) > 1.0e-20)
            {
                phaseCorrections[static_cast<std::size_t>(adjacent)] =
                    std::polar(1.0f, static_cast<float>(-std::arg(correlation)));
            }
        }

        for (int carrier = spectrum->minimumCarrier; carrier <= spectrum->maximumCarrier; ++carrier)
        {
            std::complex<float> channel(0.0f, 0.0f);
            float totalWeight = 0.0f;
            for (int adjacent = first; adjacent <= last; ++adjacent)
            {
                const float weight = 1.0f / (1.0f + std::abs(adjacent - targetSequenceSymbol));
                channel += weight * phaseCorrections[static_cast<std::size_t>(adjacent)]
                    * initial[static_cast<std::size_t>(adjacent)].channel[
                        static_cast<std::size_t>(carrier - spectrum->minimumCarrier)
                    ];
                totalWeight += weight;
            }

            channel /= totalWeight;
            const std::size_t index = static_cast<std::size_t>(carrier - spectrum->minimumCarrier);
            output.channel[index] = channel;
            const float power = std::norm(channel);
            output.cells[index] = power > 1.0e-12f
                ? binForCarrier(
                    fftFrames[static_cast<std::size_t>(targetFrame)][static_cast<std::size_t>(symbol)],
                    carrier + m_integerCarrierOffset
                ) * std::conj(channel) / power
                : std::complex<float>(0.0f, 0.0f);
        }
    }

    return true;
}

bool ChannelEstimationContext::prepareWiener()
{
    if (m_wienerState != 0) {
        return m_wienerState > 0;
    }

    m_wienerState = -1;
    const Mode mode = m_mode;
    const int occupancy = m_occupancy;
    const auto& fftFrames = *m_frames;
    const ModeParameters& parameters = Parameters::forMode(mode);
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    for (const auto& frame : fftFrames)
    {
        for (const auto& bins : frame)
        {
            if (static_cast<int>(bins.size()) < parameters.usefulSamples(
                    parameters.internalSampleRate)) {
                return false;
            }
        }
    }

    std::vector<int> pilotGrid;
    for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
    {
        const std::vector<int>& carriers = CellMap::gainReferenceCarriers(
            mode, occupancy, symbol
        );
        pilotGrid.insert(pilotGrid.end(), carriers.begin(), carriers.end());
    }

    std::sort(pilotGrid.begin(), pilotGrid.end());
    pilotGrid.erase(std::unique(pilotGrid.begin(), pilotGrid.end()), pilotGrid.end());
    if (pilotGrid.size() < 2U || pilotGrid.front() != spectrum->minimumCarrier) {
        return false;
    }

    const int pilotSpacing = pilotGrid[1] - pilotGrid[0];
    if (pilotSpacing <= 0) {
        return false;
    }

    // The time-interpolated grid is regular except where an unused carrier
    // (mode D's DC) displaces a gain reference; fill those positions later.
    const std::vector<int> unused = CellMap::unusedCarriers(mode);
    std::vector<int> regularGrid;
    for (int carrier = pilotGrid.front(); carrier <= pilotGrid.back(); carrier += pilotSpacing) {
        regularGrid.push_back(carrier);
    }

    for (int carrier : pilotGrid)
    {
        if ((carrier - pilotGrid.front()) % pilotSpacing != 0) {
            return false;
        }
    }

    for (int carrier : regularGrid)
    {
        const bool present = std::binary_search(pilotGrid.begin(), pilotGrid.end(), carrier);
        if (!present && std::find(unused.begin(), unused.end(), carrier) == unused.end()) {
            return false;
        }
    }

    std::vector<std::vector<ChannelPilotObservation>> histories(regularGrid.size());
    for (std::size_t frame = 0; frame < fftFrames.size(); ++frame)
    {
        const int framePhase = (m_firstFrameInSuperframe + static_cast<int>(frame))
            % parameters.framesPerSuperframe;
        for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
        {
            const int sequenceSymbol = static_cast<int>(frame) * parameters.symbolsPerFrame
                + symbol;
            const std::vector<int>& carriers = CellMap::gainReferenceCarriers(
                mode, occupancy, symbol
            );
            const std::vector<PilotCell>& references = CellMap::referenceCells(
                mode, occupancy, symbol, framePhase
            );
            for (int carrier : carriers)
            {
                const auto reference = std::find_if(
                    references.begin(), references.end(), [carrier](const PilotCell& pilot) {
                        return pilot.carrier == carrier;
                    }
                );
                if (reference == references.end()) {
                    return false;
                }

                histories[static_cast<std::size_t>((carrier - regularGrid.front()) / pilotSpacing)].push_back({
                    sequenceSymbol,
                    binForCarrier(
                        fftFrames[frame][static_cast<std::size_t>(symbol)],
                        carrier + m_integerCarrierOffset
                    ) / pilotValue(*reference)
                });
            }
        }
    }

    const std::vector<EqualizedSymbol>& initial = m_initial;
    const std::size_t sequenceSymbols = initial.size();
    std::vector<float> temporalCorrelations;
    temporalCorrelations.reserve(sequenceSymbols > 0 ? sequenceSymbols - 1U : 0U);
    for (std::size_t adjacent = 1; adjacent < sequenceSymbols; ++adjacent)
    {
        std::complex<double> correlation(0.0, 0.0);
        double leftPower = 0.0;
        double rightPower = 0.0;
        for (std::size_t index = 0; index < initial[adjacent].channel.size(); ++index)
        {
            const std::complex<double> left = initial[adjacent - 1U].channel[index];
            const std::complex<double> right = initial[adjacent].channel[index];
            correlation += right * std::conj(left);
            leftPower += std::norm(left);
            rightPower += std::norm(right);
        }

        if (leftPower > 1.0e-20 && rightPower > 1.0e-20)
        {
            temporalCorrelations.push_back(static_cast<float>(
                std::abs(correlation) / std::sqrt(leftPower * rightPower)
            ));
        }
    }

    float temporalCorrelation = 0.8f;
    if (!temporalCorrelations.empty())
    {
        const auto middle = temporalCorrelations.begin()
            + static_cast<std::ptrdiff_t>(temporalCorrelations.size() / 2U);
        std::nth_element(
            temporalCorrelations.begin(), middle, temporalCorrelations.end()
        );
        temporalCorrelation = std::clamp(*middle, 0.35f, 0.95f);
    }

    m_pilotGrid = std::move(regularGrid);
    m_pilotSpacing = pilotSpacing;
    m_histories = std::move(histories);
    m_temporalCorrelation = temporalCorrelation;
    // The common phase advance per symbol (residual carrier offset), from
    // successive observations of the same pilot carriers a pilot period
    // apart.
    {
        int gap = 0;
        for (const auto& history : m_histories)
        {
            for (std::size_t k = 1; k < history.size(); ++k)
            {
                const int step = history[k].sequenceSymbol - history[k - 1].sequenceSymbol;
                if (step > 0 && (gap == 0 || step < gap)) {
                    gap = step;
                }
            }
        }

        // A carrier displaced by k whole bins advances by 2 pi k G / N per
        // symbol (G guard, N useful samples): the NCO removes only the
        // fractional offset. Remove that known rotation first, so that only
        // the small residual is measured and cannot wrap over the gap.
        const double integerRotation = std::remainder(
            2.0 * Pi * m_integerCarrierOffset * parameters.guardTimeUnits / parameters.usefulTimeUnits,
            2.0 * Pi);
        std::complex<double> advance(0.0, 0.0);
        for (const auto& history : m_histories)
        {
            for (std::size_t k = 1; k < history.size(); ++k)
            {
                if (history[k].sequenceSymbol - history[k - 1].sequenceSymbol != gap) {
                    continue;
                }

                advance += std::complex<double>(history[k].channel) * std::conj(std::complex<double>(history[k - 1].channel))
                    * std::polar(1.0, -integerRotation * gap);
            }
        }

        m_pilotPeriod = gap;
        m_phaseRate = integerRotation
            + (gap > 0 && std::abs(advance) > 1.0e-20 ? std::arg(advance) / gap : 0.0);
        // Correlation of each pilot carrier's observations at one and two
        // observation lags, with the common rotation removed. Noise only
        // lowers lag zero, so their ratio estimates the channel's own
        // correlation at one lag.
        std::complex<double> lag1(0.0, 0.0), lag2(0.0, 0.0);
        double power1 = 0.0, power2 = 0.0;
        for (const auto& history : m_histories)
        {
            for (std::size_t k = 1; k < history.size(); ++k)
            {
                if (history[k].sequenceSymbol - history[k - 1].sequenceSymbol != gap) {
                    continue;
                }

                const std::complex<double> a(history[k - 1].channel), b(history[k].channel);
                lag1 += b * std::conj(a) * std::polar(1.0, -m_phaseRate * gap);
                power1 += std::abs(a) * std::abs(b);
                if (k >= 2 && history[k - 1].sequenceSymbol - history[k - 2].sequenceSymbol == gap)
                {
                    const std::complex<double> c(history[k - 2].channel);
                    lag2 += b * std::conj(c) * std::polar(1.0, -m_phaseRate * 2 * gap);
                    power2 += std::abs(c) * std::abs(b);
                }
            }
        }

        const double rho1 = power1 > 1.0e-20 ? std::abs(lag1) / power1 : 1.0;
        const double rho2 = power2 > 1.0e-20 ? std::abs(lag2) / power2 : 1.0;
        m_pilotCorrelation = rho1 > 1.0e-6 ? std::clamp(rho2 / rho1, 0.0, 1.0) : 0.0;
        m_pilotLag1 = rho1;
    }

    m_wienerState = 1;
    return true;
}

bool ChannelEstimationContext::estimateFrameWiener(
    int targetFrame,
    std::vector<EqualizedSymbol>& result,
    ChannelWienerStatistics* statistics
)
{
    if (!m_valid || targetFrame < 0
        || static_cast<std::size_t>(targetFrame) >= m_frames->size()
        || !prepareWiener()) {
        return false;
    }

    // Mode E interpolates each symbol from its own pilots unless the channel
    // holds echoes beyond the delay span they resolve (see longEchoFraction).
    if (m_mode == Mode::E)
    {
        if (m_timeInterpolation == 0) {
            m_timeInterpolation = longEchoFraction() >= ModeELongEchoFraction ? 1 : -1;
        }

        if (m_timeInterpolation < 0) {
            return false;
        }
    }

    const Mode mode = m_mode;
    const int occupancy = m_occupancy;
    const auto& fftFrames = *m_frames;
    const ModeParameters& parameters = Parameters::forMode(mode);
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    const int carrierCount = spectrum->maximumCarrier - spectrum->minimumCarrier + 1;
    const float temporalCorrelation = m_temporalCorrelation;
    const TemporalWeights weights = temporalWeights(temporalCorrelation);
    // Mode E's linear time regression spans as much of each pilot's history
    // as the channel stays correlated over: twelve symbols either side when
    // it is nearly static, down to one pilot period when it fades quickly.
    // The A-D Wiener filter weights the twelve symbols either side itself.
    const int pilotPeriod = std::max(1, m_pilotPeriod);
    const int window = mode != Mode::E || m_pilotCorrelation >= 0.995 ? 12
        : m_pilotCorrelation >= 0.95 ? std::min(12, 2 * pilotPeriod) : std::min(12, pilotPeriod);
    if (statistics)
    {
        statistics->temporalCorrelation = temporalCorrelation;
        statistics->delayNoiseRatio = 0.0f;
    }

    double totalNoiseRatio = 0.0;
    int noiseRatioCount = 0;
    result.assign(
        static_cast<std::size_t>(parameters.symbolsPerFrame), EqualizedSymbol()
    );
    // A-D regress each pilot carrier's observations by Wiener interpolation in
    // time: a Gaussian Doppler spectrum fitted to the pilots' noise-free
    // correlation one pilot period apart, and the noise ratio from that against
    // the measured correlation (noise lowers only the latter). It weights the
    // twelve symbols either side itself. Mode E (its SFN channel is the one
    // that interpolates in time) lost 0.5% of the sweep and keeps the linear
    // fit.
    //
    // The lag-two to lag-one ratio is rho(P) for an exponential correlation
    // but rho(P)^3 for a Gaussian one. In fast fading the noise ratio then
    // came out at its floor and the filter followed the pilots' noise; a floor
    // of 0.003 instead of 0.001 lifted the BBC recording's worst 1% of
    // superframes from 7.7 to 8.4 dB MER and the A-D sweep by 16 frames.
    // Consistent models were worse where it counts, in decoded frames: the
    // cube root (or 2/3) with the noise from the pilots' second differences
    // gave 10.3 dB there but cost 0.6% of the A-D sweep and 5% of Kuwait
    // 10:02's frames; higher floors cost Kuwait 3-5%.
    TimeWienerModel timeModel;
    timeModel.period = pilotPeriod;
    timeModel.periodCorrelation = std::clamp(m_pilotCorrelation, 0.05, 0.9999);
    timeModel.noiseRatio = std::clamp(
        timeModel.periodCorrelation / std::max(1.0e-6, m_pilotLag1) - 1.0, 3.0e-3, 10.0
    );
    const TimeWienerModel* timeWiener = mode == Mode::E ? nullptr : &timeModel;
    TimeWienerWeights& timeWeights = m_timeWienerWeights;
    for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
    {
        const int targetSequenceSymbol = targetFrame * parameters.symbolsPerFrame + symbol;
        const PhaseCorrections phaseCorrections(m_phaseRate, targetSequenceSymbol);
        std::vector<std::complex<float>> timeInterpolated;
        std::vector<bool> haveEstimate;
        timeInterpolated.reserve(m_pilotGrid.size());
        haveEstimate.reserve(m_pilotGrid.size());
        for (const std::vector<ChannelPilotObservation>& history : m_histories)
        {
            if (history.empty())
            {
                // Only an unused carrier can be absent (checked when prepared).
                timeInterpolated.emplace_back(0.0f, 0.0f);
                haveEstimate.push_back(false);
                continue;
            }

            timeInterpolated.push_back(estimateTimedPilot(
                history, targetSequenceSymbol, &phaseCorrections, &weights,
                window, timeWiener, &timeWeights
            ));
            haveEstimate.push_back(true);
        }

        for (std::size_t index = 0; index < timeInterpolated.size(); ++index)
        {
            if (haveEstimate[index]) {
                continue;
            }

            const bool left = index > 0 && haveEstimate[index - 1];
            const bool right = index + 1 < timeInterpolated.size() && haveEstimate[index + 1];
            if (!left && !right) {
                return false;
            }

            timeInterpolated[index] = left && right
                ? 0.5f * (timeInterpolated[index - 1] + timeInterpolated[index + 1])
                : left ? timeInterpolated[index - 1] : timeInterpolated[index + 1];
        }

        std::vector<std::complex<float>> channel;
        float noiseRatio = 0.0f;
        if (!dftInterpolate(
                parameters, carrierCount, m_pilotSpacing, timeInterpolated, channel,
                &noiseRatio)) {
            return false;
        }

        totalNoiseRatio += noiseRatio;
        ++noiseRatioCount;
        EqualizedSymbol& output = result[static_cast<std::size_t>(symbol)];
        output.minimumCarrier = spectrum->minimumCarrier;
        output.maximumCarrier = spectrum->maximumCarrier;
        output.channel = std::move(channel);
        output.cells.resize(static_cast<std::size_t>(carrierCount));
        for (int carrier = spectrum->minimumCarrier; carrier <= spectrum->maximumCarrier; ++carrier)
        {
            const std::size_t index = static_cast<std::size_t>(carrier - spectrum->minimumCarrier);
            const float power = std::norm(output.channel[index]);
            output.cells[index] = power > 1.0e-12f
                ? binForCarrier(
                    fftFrames[static_cast<std::size_t>(targetFrame)][static_cast<std::size_t>(symbol)],
                    carrier + m_integerCarrierOffset
                ) * std::conj(output.channel[index]) / power
                : std::complex<float>(0.0f, 0.0f);
        }

        double residualPower = 0.0;
        double referencePower = 0.0;
        const std::vector<int>& targetPilots = CellMap::gainReferenceCarriers(
            mode, occupancy, symbol
        );
        // The frequency interpolation's residual at the symbol's own pilot
        // carriers, against the time-interpolated grid it was given.
        for (int carrier : targetPilots)
        {
            const std::complex<float> measured = timeInterpolated[
                static_cast<std::size_t>((carrier - m_pilotGrid.front()) / m_pilotSpacing)
            ];
            const std::complex<float> predicted = output.channel[
                static_cast<std::size_t>(carrier - spectrum->minimumCarrier)
            ];
            residualPower += std::norm(measured - predicted);
            referencePower += std::norm(measured);
        }

        output.pilotResidualPower = referencePower > 1.0e-20
            ? static_cast<float>(residualPower / referencePower) : 0.0f;
        // The raw pilot observations of this symbol against the final
        // estimate: their error power is N0 + F |H|^2, receiver noise plus
        // error that scales with the channel (a transmitter's own modulation
        // error, estimation error). The estimate includes each pilot with some
        // weight, which shrinks both terms alike, so their ratio is unbiased.
        // A boosted pilot (amplitude 2, not sqrt(2)) carries half a normal
        // one's noise but, at the band edge, the most interpolation error;
        // scaling its error to a normal pilot's noise or dropping it cost
        // Kuwait 10:02 46 frames, so all gain pilots count alike.
        for (int carrier : targetPilots)
        {
            const auto& history = m_histories[static_cast<std::size_t>((carrier - m_pilotGrid.front()) / m_pilotSpacing)];
            const auto found = std::lower_bound(history.begin(), history.end(), targetSequenceSymbol,
                [](const ChannelPilotObservation& observation, int value) { return observation.sequenceSymbol < value; });
            if (found == history.end() || found->sequenceSymbol != targetSequenceSymbol) {
                continue;
            }

            const std::complex<float> predicted = output.channel[static_cast<std::size_t>(carrier - spectrum->minimumCarrier)];
            output.pilotErrors.push_back({carrier, std::norm(predicted), std::norm(found->channel - predicted)});
        }
    }

    if (statistics && noiseRatioCount > 0)
    {
        statistics->delayNoiseRatio = static_cast<float>(
            totalNoiseRatio / noiseRatioCount
        );
    }

    return true;
}

double ChannelEstimationContext::longEchoFraction()
{
    if (!m_valid || !m_haveDelayWindow || !prepareWiener()) {
        return -1.0;
    }

    const ModeParameters& parameters = Parameters::forMode(m_mode);
    const int usefulSamples = parameters.usefulSamples(parameters.internalSampleRate);
    const int length = static_cast<int>(m_pilotGrid.size());
    if (length < 8 || m_pilotSpacing <= 0) {
        return -1.0;
    }

    const TemporalWeights weights = temporalWeights(m_temporalCorrelation);
    const int pilotPeriod = std::max(1, m_pilotPeriod);
    const int window = m_pilotCorrelation >= 0.995 ? 12
        : m_pilotCorrelation >= 0.95 ? std::min(12, 2 * pilotPeriod) : std::min(12, pilotPeriod);
    std::vector<double> profile(static_cast<std::size_t>(length), 0.0);
    std::vector<std::complex<float>> grid(static_cast<std::size_t>(length));
    std::vector<std::complex<float>> delay(static_cast<std::size_t>(length));
    const int sequenceSymbols = static_cast<int>(m_initial.size());
    for (int target = 0; target < sequenceSymbols; ++target)
    {
        const PhaseCorrections phaseCorrections(m_phaseRate, target);
        for (std::size_t index = 0; index < m_histories.size(); ++index)
        {
            grid[index] = m_histories[index].empty() ? std::complex<float>(0.0f, 0.0f)
                : estimateTimedPilot(m_histories[index], target, &phaseCorrections, &weights, window);
        }

        fftPlan(length, true).transform(grid.data(), delay.data());
        for (int tap = 0; tap < length; ++tap) {
            profile[static_cast<std::size_t>(tap)] += std::norm(delay[static_cast<std::size_t>(tap)]);
        }
    }

    std::vector<double> sorted = profile;
    const auto floorPosition = sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 4U);
    std::nth_element(sorted.begin(), floorPosition, sorted.end());
    const double floor = *floorPosition;
    // The grid spans N / spacing samples of delay; time-interpolation errors
    // repeat with the pilot pattern across the grid, so they image the
    // channel at multiples of N / 16 samples (the per-symbol span).
    const double tapDelay = static_cast<double>(usefulSamples) / (m_pilotSpacing * length);
    const double span = static_cast<double>(usefulSamples) / m_pilotSpacing;
    const double imagePeriod = static_cast<double>(usefulSamples) / FrequencyWienerSpacing;
    const double halfWindow = m_delayWidth / 2.0 + 1.5 * tapDelay;
    double inWindow = 0.0;
    double outside = 0.0;
    for (int tap = 0; tap < length; ++tap)
    {
        const double excess = std::max(0.0, profile[static_cast<std::size_t>(tap)] - floor);
        const double delaySamples = tap * tapDelay;
        bool image = false;
        bool main = false;
        for (double shift = 0.0; shift < span - 1.0e-9; shift += imagePeriod)
        {
            if (std::abs(std::remainder(delaySamples - m_delayCentre - shift, span)) <= halfWindow) {
                (shift == 0.0 ? main : image) = true;
            }
        }

        if (main) {
            inWindow += excess;
        } else if (!image) {
            outside += excess;
        }
    }

    return inWindow > 1.0e-20 ? outside / inWindow : -1.0;
}

PilotTimingEstimate PilotTimingTracker::compare(
    const EqualizedSymbol& previous,
    const EqualizedSymbol& current,
    int usefulSamples
)
{
    PilotTimingEstimate result;
    if (usefulSamples <= 0
        || previous.minimumCarrier != current.minimumCarrier
        || previous.maximumCarrier != current.maximumCarrier
        || previous.channel.size() != current.channel.size()
        || previous.channel.size() < 3U) {
        return result;
    }

    double sumWeight = 0.0;
    double sumX = 0.0;
    double sumY = 0.0;
    double sumXX = 0.0;
    double sumXY = 0.0;
    double previousPhase = 0.0;
    bool havePhase = false;
    struct Observation { double carrier; double phase; double weight; };
    std::vector<Observation> observations;
    observations.reserve(previous.channel.size());

    for (std::size_t index = 0; index < previous.channel.size(); ++index)
    {
        const std::complex<double> left = previous.channel[index];
        const std::complex<double> right = current.channel[index];
        const double weight = std::sqrt(std::norm(left) * std::norm(right));
        if (weight <= 1.0e-12) {
            continue;
        }

        double phase = std::arg(right * std::conj(left));
        if (havePhase)
        {
            while (phase - previousPhase > Pi) {
                phase -= 2.0 * Pi;
            }

            while (phase - previousPhase < -Pi) {
                phase += 2.0 * Pi;
            }
        }

        previousPhase = phase;
        havePhase = true;
        const double carrier = previous.minimumCarrier
            + static_cast<double>(index);
        observations.push_back({carrier, phase, weight});
        sumWeight += weight;
        sumX += weight * carrier;
        sumY += weight * phase;
        sumXX += weight * carrier * carrier;
        sumXY += weight * carrier * phase;
    }

    const double denominator = sumWeight * sumXX - sumX * sumX;
    if (observations.size() < 3U || sumWeight <= 1.0e-20
        || std::abs(denominator) <= 1.0e-20) {
        return result;
    }

    const double slope = (sumWeight * sumXY - sumX * sumY) / denominator;
    const double intercept = (sumY - slope * sumX) / sumWeight;
    std::complex<double> residualCorrelation(0.0, 0.0);
    for (const Observation& observation : observations)
    {
        residualCorrelation += observation.weight * std::polar(
            1.0, observation.phase
                - intercept - slope * observation.carrier
        );
    }

    result.timingShiftSamples = slope * usefulSamples / (2.0 * Pi);
    result.commonPhaseRadians = std::remainder(intercept, 2.0 * Pi);
    result.coherence = static_cast<float>(
        std::abs(residualCorrelation) / sumWeight
    );
    result.valid = std::isfinite(result.timingShiftSamples)
        && std::isfinite(result.commonPhaseRadians)
        && result.coherence >= 0.5f
        && std::abs(result.timingShiftSamples) <= usefulSamples / 8.0;
    return result;
}

} // namespace modemdrm
