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

#include "drmreceiver.h"
#include "drmcellmap.h"

#include "drmaudioframing.h"
#include "drmchannelestimator.h"
#include "drmmsc.h"
#include "drmofdm.h"
#include "drmparameters.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

namespace modemdrm
{

namespace
{

int firstTransmissionFrame(const FACAcquisitionResult& fac)
{
    for (std::size_t i = 0; i < fac.facBlocks.size(); ++i)
    {
        if (fac.facBlocks[i].identity == 0 || fac.facBlocks[i].identity == 3) {
            return static_cast<int>(i);
        }
    }

    return fac.coarse.mode == Mode::E ? 3 : -1;
}

// The raw gain-pilot channel observations of one symbol, zero elsewhere, in
// the layout of an equalized symbol.
EqualizedSymbol rawGainPilots(
    Mode mode,
    int occupancy,
    int symbol,
    int framePhase,
    int integerCarrierOffset,
    const std::vector<std::complex<float>>& bins
)
{
    EqualizedSymbol result;
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    if (!spectrum || bins.empty()) {
        return result;
    }

    result.minimumCarrier = spectrum->minimumCarrier;
    result.maximumCarrier = spectrum->maximumCarrier;
    result.channel.assign(static_cast<std::size_t>(spectrum->maximumCarrier - spectrum->minimumCarrier + 1), {});
    const std::vector<PilotCell>& references = CellMap::referenceCells(mode, occupancy, symbol, framePhase);
    const int size = static_cast<int>(bins.size());
    for (const PilotCell& pilot : references)
    {
        if (pilot.kind != ReferenceKind::Gain) {
            continue;
        }

        int index = (pilot.carrier + integerCarrierOffset) % size;
        if (index < 0) {
            index += size;
        }

        const double phase = 2.0 * 3.14159265358979323846 * pilot.phase1024 / 1024.0;
        const std::complex<float> expected = pilot.amplitude * std::complex<float>(
            static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
        result.channel[static_cast<std::size_t>(pilot.carrier - spectrum->minimumCarrier)] =
            bins[static_cast<std::size_t>(index)] / expected;
    }

    return result;
}

// An edge frame's continuity into the superframe, against the median of the
// superframe's own frame-to-frame continuity (see decodeMSC). Sample counts
// here and below are at the mode's reference rate (48 kHz for A-D, 192 kHz
// for E): a timing estimate's spread in samples scales with the rate.
constexpr double EdgeFrameCoherence = 0.9;
constexpr double EdgeFrameShiftSamples = 2.0;
constexpr double EdgeFramePhaseAgreement = 0.5;

double referenceRateScale(Mode mode, int sampleRate)
{
    const int reference = Parameters::forMode(mode).internalSampleRate;
    return reference > 0 && sampleRate > 0 ? static_cast<double>(sampleRate) / reference : 1.0;
}

// How the raw pilots of one frame continue into the next: the median timing
// shift of the valid PilotTimingTracker::compare estimates and the mean
// coherence over the symbols (each symbol index has the same gain-reference
// carriers in every frame). A slip of one gain-pilot comb period (N/6 samples
// in mode B, for example) aliases to no shift with full per-symbol coherence,
// but turns each symbol's common phase by a different amount (2 pi k0 / 6 for
// a comb from carrier k0), so the symbols must also agree on it: the
// coherence-weighted mean of their phasors at least EdgeFramePhaseAgreement
// of the mean coherence (an aliased slip of a three-comb mode gives near 0).
// Requiring half the symbols valid, or scaling the coherence by the phase
// agreement, cost Kuwait 10:02 44 frames.
struct FrameContinuity
{
    bool valid = false;
    double shiftSamples = 0.0;
    double coherence = 0.0;
};

FrameContinuity frameContinuity(
    Mode mode,
    int occupancy,
    int integerCarrierOffset,
    int sampleRate,
    const std::vector<std::vector<std::complex<float>>>& first,
    int firstPhase,
    const std::vector<std::vector<std::complex<float>>>& second,
    int secondPhase
)
{
    FrameContinuity result;
    const ModeParameters& parameters = Parameters::forMode(mode);
    std::vector<double> validShifts;
    double coherence = 0.0;
    std::complex<double> commonPhase(0.0, 0.0);
    for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
    {
        const EqualizedSymbol a = rawGainPilots(mode, occupancy, symbol, firstPhase, integerCarrierOffset,
            first[static_cast<std::size_t>(symbol)]);
        const EqualizedSymbol b = rawGainPilots(mode, occupancy, symbol, secondPhase, integerCarrierOffset,
            second[static_cast<std::size_t>(symbol)]);
        const PilotTimingEstimate estimate = PilotTimingTracker::compare(a, b, parameters.usefulSamples(sampleRate));
        coherence += estimate.coherence;
        commonPhase += static_cast<double>(estimate.coherence) * std::polar(1.0, estimate.commonPhaseRadians);
        if (estimate.valid) {
            validShifts.push_back(estimate.timingShiftSamples);
        }
    }

    if (validShifts.empty() || !(coherence > 0.0)
        || std::abs(commonPhase) < EdgeFramePhaseAgreement * coherence) {
        return result;
    }

    const auto middle = validShifts.begin() + static_cast<std::ptrdiff_t>(validShifts.size() / 2U);
    std::nth_element(validShifts.begin(), middle, validShifts.end());
    result.valid = true;
    result.shiftSamples = *middle;
    result.coherence = coherence / static_cast<double>(parameters.symbolsPerFrame);
    return result;
}

bool demodulateFrames(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    Mode mode,
    std::size_t firstFrameStart,
    int frameCount,
    int occupancy,
    int firstFrameInSuperframe,
    int integerCarrierOffset,
    double fractionalCarrierOffsetHz,
    bool spectralInversion,
    std::vector<std::vector<std::vector<std::complex<float>>>>& spectra,
    double* sampleClockErrorPPM = nullptr,
    SampleClockState* sampleClock = nullptr
)
{
    const ModeParameters& parameters = Parameters::forMode(mode);
    const int symbolSamples = parameters.symbolSamples(sampleRate);
    spectra.assign(
        static_cast<std::size_t>(frameCount),
        std::vector<std::vector<std::complex<float>>>(
            static_cast<std::size_t>(parameters.symbolsPerFrame)
        )
    );
    OFDMDemodulator demodulator(mode, sampleRate);
    std::vector<EqualizedSymbol> previousPilots;
    SampleClockState localClock;
    SampleClockState& clock = sampleClock ? *sampleClock : localClock;
    // The caller re-anchors the first frame from fresh timing each call, so
    // prediction restarts at zero; only the drift rate is carried over. Keep
    // the steering well inside the guard interval.
    const double maximumPrediction = demodulator.guardSamples() / 4.0;
    const double rateScale = referenceRateScale(mode, sampleRate);
    double predictedCorrection = 0.0;
    long long previousCorrection = 0;
    const long long frameSamples = static_cast<long long>(
        parameters.symbolsPerFrame * symbolSamples
    );
    for (int frame = 0; frame < frameCount; ++frame)
    {
        const long long nominalFrameStart = static_cast<long long>(firstFrameStart)
            + static_cast<long long>(frame) * frameSamples;
        long long frameCorrection = std::llround(predictedCorrection);
        const long long minimumCorrection = -nominalFrameStart;
        const long long finalNominalPrefix = nominalFrameStart
            + static_cast<long long>(parameters.symbolsPerFrame - 1) * symbolSamples;
        const long long maximumCorrection = static_cast<long long>(sampleCount)
            - finalNominalPrefix - demodulator.guardSamples()
            - demodulator.usefulSamples();
        if (maximumCorrection < minimumCorrection) {
            return false;
        }

        frameCorrection = std::clamp(
            frameCorrection, minimumCorrection, maximumCorrection
        );
        for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
        {
            const std::size_t prefixStart = static_cast<std::size_t>(
                nominalFrameStart + frameCorrection
                    + static_cast<long long>(symbol) * symbolSamples
            );
            if (!demodulator.demodulate(
                    samples, sampleCount, prefixStart, fractionalCarrierOffsetHz,
                    spectra[static_cast<std::size_t>(frame)][static_cast<std::size_t>(symbol)],
                    spectralInversion)) {
                return false;
            }
        }

        std::vector<EqualizedSymbol> currentPilots(
            static_cast<std::size_t>(parameters.symbolsPerFrame)
        );
        const int framePhase = (firstFrameInSuperframe + frame)
            % parameters.framesPerSuperframe;
        bool pilotsValid = true;
        // Timing drift from the raw pilots only: estimates interpolated
        // between pilots alias on a frequency-selective channel, change from
        // frame to frame under fading, and can still look coherent.
        for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
        {
            const auto& bins = spectra[static_cast<std::size_t>(frame)][static_cast<std::size_t>(symbol)];
            currentPilots[static_cast<std::size_t>(symbol)] = rawGainPilots(
                mode, occupancy, symbol, framePhase, integerCarrierOffset, bins);
            pilotsValid = pilotsValid && !currentPilots[static_cast<std::size_t>(symbol)].channel.empty();
        }

        if (pilotsValid && !previousPilots.empty())
        {
            std::vector<double> shifts;
            shifts.reserve(static_cast<std::size_t>(parameters.symbolsPerFrame));
            for (int symbol = 0; symbol < parameters.symbolsPerFrame; ++symbol)
            {
                const PilotTimingEstimate estimate = PilotTimingTracker::compare(
                    previousPilots[static_cast<std::size_t>(symbol)],
                    currentPilots[static_cast<std::size_t>(symbol)],
                    demodulator.usefulSamples()
                );
                if (estimate.valid && estimate.coherence >= 0.8f
                    && std::abs(estimate.timingShiftSamples) <= 4.0 * rateScale) {
                    shifts.push_back(estimate.timingShiftSamples);
                }
            }

            if (shifts.size() >= static_cast<std::size_t>(
                    std::max(3, parameters.symbolsPerFrame / 3)))
            {
                const auto middle = shifts.begin()
                    + static_cast<std::ptrdiff_t>(shifts.size() / 2U);
                std::nth_element(shifts.begin(), middle, shifts.end());
                const double measuredErrorChange = *middle;
                const double appliedChange = static_cast<double>(
                    frameCorrection - previousCorrection
                );
                const double observedDrift = std::clamp(
                    appliedChange - measuredErrorChange, -4.0 * rateScale, 4.0 * rateScale
                );
                clock.driftPerFrame = clock.haveDrift
                    ? 0.8 * clock.driftPerFrame + 0.2 * observedDrift
                    : observedDrift;
                clock.haveDrift = true;
                if (clock.observations < 1000000) {
                    ++clock.observations;
                }
            }
        }

        previousPilots = std::move(currentPilots);
        previousCorrection = frameCorrection;
        if (clock.observations >= 2)
        {
            predictedCorrection = std::clamp(predictedCorrection + clock.driftPerFrame,
                -maximumPrediction, maximumPrediction);
        }

        // Moving an FFT window by an integer sample rotates every carrier by
        // a different phase. Keep the raw pilots above for drift measurement,
        // but return spectra in the nominal window's phase reference so the
        // temporal channel estimator does not interpolate across a phase jump.
        if (frameCorrection != 0)
        {
            const int usefulSamples = demodulator.usefulSamples();
            std::vector<std::complex<float>> phase(static_cast<std::size_t>(usefulSamples));
            for (int bin = 0; bin < usefulSamples; ++bin)
            {
                phase[static_cast<std::size_t>(bin)] = std::polar(1.0f,
                    static_cast<float>(-2.0 * std::acos(-1.0) * frameCorrection * bin / usefulSamples));
            }

            for (auto& symbol : spectra[static_cast<std::size_t>(frame)])
            {
                for (int bin = 0; bin < usefulSamples; ++bin) {
                    symbol[static_cast<std::size_t>(bin)] *= phase[static_cast<std::size_t>(bin)];
                }
            }
        }
    }

    if (sampleClockErrorPPM)
    {
        *sampleClockErrorPPM = clock.haveDrift
            ? clock.driftPerFrame * 1.0e6 / static_cast<double>(frameSamples)
            : 0.0;
    }

    return true;
}

bool audioCodingSupported(
    const AudioDecoderCapabilities& capabilities,
    const SDCAudioInformation& audio
)
{
    return (audio.audioCoding == 0 && capabilities.aac)
        || (audio.audioCoding == 3 && capabilities.xheAAC);
}

// The nearest level on one axis of the MSC constellation: an odd multiple of
// the axis scale, up to the constellation's largest.
struct AxisDecision
{
    AxisDecision(Mode mode, int mscMode)
    {
        if (mode == Mode::E && mscMode == 3)
        {
            maximum = 1.0f;
            scale = 0.70710678118654752f; // 4-QAM, 1/sqrt(2)
        }
        else if (mscMode == 3 || mode == Mode::E)
        {
            maximum = 3.0f;
            scale = 0.31622776601683794f; // 16-QAM, 1/sqrt(10)
        }
        else
        {
            maximum = 7.0f;
            scale = 0.15430334996209191f; // 64-QAM, 1/sqrt(42)
        }
    }

    float operator()(float received) const
    {
        const float level = std::clamp(received / scale, -maximum, maximum);
        return scale * std::clamp(2.0f * std::floor(0.5f * level) + 1.0f, -maximum, maximum);
    }

    float scale = 1.0f;
    float maximum = 1.0f;
};

void accumulateDecisionDirectedMER(
    Mode mode,
    int mscMode,
    const MSCMultiplexFrameCells& frame,
    double& referencePower,
    double& errorPower,
    std::size_t& cellCount
)
{
    const AxisDecision nearestAxis(mode, mscMode);
    for (std::size_t i = 0; i < frame.cells.size(); ++i)
    {
        if (i < frame.reliabilities.size() && frame.reliabilities[i] <= 0.0f) {
            continue;
        }

        const auto& received = frame.cells[i];
        const std::complex<float> decided(
            nearestAxis(received.real()), nearestAxis(received.imag())
        );
        referencePower += std::norm(decided);
        errorPower += std::norm(received - decided);
        ++cellCount;
    }
}

// MSC soft bits are weighted by |H|, which is right when every cell's error is
// receiver noise: N0/|H|^2 after equalization. Error that scales with the
// channel, a transmitter's own modulation error above all, is the same on
// every equalized cell instead, so a strong carrier is not as reliable as |H|
// says. The superframe's pilots measure both terms (error power N0 + F |H|^2,
// see EqualizedSymbol::pilotErrors), without decisions, which saturate at low
// SNR. Each cell is then weighted by |H| / sqrt(1 + s (F/N0) |H|^2), rescaled
// to the superframe's mean |H| so that superframes mixed by the deinterleaver
// keep their relative scale. At F/N0 near zero (the annex B channels, BBC)
// this is |H|. Kuwait 10:02 (F/N0 about 3 at the mean channel power, a floor
// near 17 dB) decodes 3,646 frames instead of 1,976; the A-D sweep keeps
// 14,829 of 14,834 frames and the mode E sweep gains (7,221 against 7,219). A scale s of
// 0.5 was best: 1 and 2 cost the sweeps more for little more on Kuwait.
constexpr double ErrorFloorWeightScale = 0.5;
constexpr double MinimumPilotErrors = 20.0;

void weightForErrorFloor(std::vector<MSCFrameCells>& frames,
    const std::vector<EqualizedSymbol::PilotError>& errors, const std::vector<bool>& excludedCarriers,
    int minimumCarrier)
{
    // Carriers an interferer raises (see weightForCarrierNoise, as measured
    // up to the previous superframe) would inflate the slope; leaving them
    // out gained Kuwait 10:02 8 frames and 11:19 2.
    EqualizedSymbol::PilotErrorSums sums;
    for (const EqualizedSymbol::PilotError& error : errors)
    {
        const int index = error.carrier - minimumCarrier;
        if (index >= 0 && index < static_cast<int>(excludedCarriers.size())
            && excludedCarriers[static_cast<std::size_t>(index)]) {
            continue;
        }

        sums.add(error);
    }

    if (sums.count < MinimumPilotErrors) {
        return;
    }

    const double denominator = sums.count * sums.xx - sums.x * sums.x;
    if (!(denominator > 0.0)) {
        return;
    }

    const double slope = std::max(0.0, (sums.count * sums.xy - sums.x * sums.y) / denominator);
    // The intercept can fit negative when the floor dominates; bound it by a
    // small share of the mean error.
    const double intercept = std::max((sums.y - slope * sums.x) / sums.count, 0.01 * sums.y / sums.count);
    const double ratio = ErrorFloorWeightScale * slope / intercept;
    if (!(ratio > 0.0)) {
        return;
    }

    double before = 0.0;
    double after = 0.0;
    for (const MSCFrameCells& frame : frames)
    {
        for (float h : frame.reliabilities)
        {
            if (h <= 0.0f) {
                continue;
            }

            before += h;
            after += h / std::sqrt(1.0 + ratio * static_cast<double>(h) * h);
        }
    }

    if (!(after > 0.0)) {
        return;
    }

    const double scale = before / after;
    for (MSCFrameCells& frame : frames)
    {
        for (float& h : frame.reliabilities)
        {
            if (h > 0.0f) {
                h = static_cast<float>(scale * h / std::sqrt(1.0 + ratio * static_cast<double>(h) * h));
            }
        }
    }
}

// A narrowband interferer raises the noise of a few carriers (RFmondial
// 16-QAM: carriers 34-38 at up to 7.7 times the median; BBC: a tone on carrier
// -64), which |H| does not see. Each carrier's decision error, scaled by the
// cells' reliabilities, over the superframe's stronger cells (whose decisions
// saturate least) is
// compared with the median over carriers, and a carrier above twice the median
// is weighted down by the square root of the excess. Where the noise is
// flat (the annex B channels) no carrier is above the threshold. With the
// error-floor weighting: Kuwait 10:02 3,658 frames (3,646), Kuwait 11:19 736
// (726), RFmondial 4-QAM 5,500 (5,493, now nothing concealed), the A-D sweep
// 14,833 (14,829). Keeping each carrier's latest measurement (CarrierNoiseHistory)
// mattered (without it Kuwait 10:02 decoded 3,621); averaging over superframes
// did no better.
constexpr double CarrierNoiseThreshold = 2.0;
constexpr double CarrierNoiseExponent = 0.5;
constexpr double CarrierNoiseStrongCells = 0.5; // of the mean channel power
constexpr double CarrierNoiseMinimumCells = 4.0;
constexpr int CarrierNoiseMaximumAge = 10; // superframes without a measurement

// Carriers whose kept noise measurement is above the threshold, from the lowest.
std::vector<bool> noisyCarriers(Mode mode, int occupancy, int mscMode, const CarrierNoiseHistory& history)
{
    if (history.mode != static_cast<int>(mode) || history.occupancy != occupancy || history.mscMode != mscMode) {
        return {};
    }

    std::vector<double> measured;
    for (double n : history.noise)
    {
        if (n >= 0.0) {
            measured.push_back(n);
        }
    }

    if (measured.size() < 16U) {
        return {};
    }

    std::nth_element(measured.begin(), measured.begin() + static_cast<std::ptrdiff_t>(measured.size() / 2U), measured.end());
    const double limit = CarrierNoiseThreshold * measured[measured.size() / 2U];
    if (!(limit > 0.0)) {
        return {};
    }

    std::vector<bool> noisy(history.noise.size(), false);
    for (std::size_t carrier = 0; carrier < noisy.size(); ++carrier) {
        noisy[carrier] = history.noise[carrier] > limit;
    }

    return noisy;
}

// An erased superframe measures nothing: its carriers' measurements age.
void ageCarrierNoise(CarrierNoiseHistory& history)
{
    for (std::size_t carrier = 0; carrier < history.noise.size() && carrier < history.age.size(); ++carrier)
    {
        if (history.noise[carrier] >= 0.0 && ++history.age[carrier] > CarrierNoiseMaximumAge) {
            history.noise[carrier] = -1.0;
        }
    }
}

void weightForCarrierNoise(Mode mode, int occupancy, int mscMode, std::vector<MSCFrameCells>& frames,
    CarrierNoiseHistory& history)
{
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    if (!spectrum) {
        return;
    }

    const int carriers = spectrum->maximumCarrier - spectrum->minimumCarrier + 1;
    // Decision errors compare only within one constellation.
    if (history.mode != static_cast<int>(mode) || history.occupancy != occupancy
        || history.mscMode != mscMode || static_cast<int>(history.noise.size()) != carriers)
    {
        history.mode = static_cast<int>(mode);
        history.occupancy = occupancy;
        history.mscMode = mscMode;
        history.noise.assign(static_cast<std::size_t>(carriers), -1.0);
        history.age.assign(static_cast<std::size_t>(carriers), 0);
    }

    const AxisDecision nearestAxis(mode, mscMode);
    double meanPower = 0.0;
    double cells = 0.0;
    for (const MSCFrameCells& frame : frames)
    {
        for (float h : frame.reliabilities)
        {
            if (h > 0.0f)
            {
                meanPower += static_cast<double>(h) * h;
                cells += 1.0;
            }
        }
    }

    if (!(cells > 0.0)) {
        return;
    }

    meanPower /= cells;
    std::vector<double> errorSum(static_cast<std::size_t>(carriers), 0.0);
    std::vector<double> errorCount(static_cast<std::size_t>(carriers), 0.0);
    for (std::size_t phase = 0; phase < frames.size(); ++phase)
    {
        const auto& positions = CellMap::mscCells(mode, occupancy, static_cast<int>(phase));
        const MSCFrameCells& frame = frames[phase];
        for (std::size_t i = 0; i < positions.size() && i < frame.cells.size(); ++i)
        {
            const double h2 = static_cast<double>(frame.reliabilities[i]) * frame.reliabilities[i];
            if (h2 < CarrierNoiseStrongCells * meanPower) {
                continue;
            }

            const std::complex<float> y = frame.cells[i];
            const std::complex<float> decided(nearestAxis(y.real()), nearestAxis(y.imag()));
            const std::size_t carrier = static_cast<std::size_t>(positions[i].carrier - spectrum->minimumCarrier);
            // Relative to the superframe's channel power, so that measurements
            // kept from earlier superframes compare.
            errorSum[carrier] += h2 * std::norm(y - decided) / meanPower;
            errorCount[carrier] += 1.0;
        }
    }

    // A carrier with too few strong cells this superframe (deep in a fade)
    // keeps its last measurement, for a while: an interferer can leave.
    std::vector<double>& noise = history.noise;
    std::vector<double> measured;
    for (std::size_t carrier = 0; carrier < noise.size(); ++carrier)
    {
        if (errorCount[carrier] >= CarrierNoiseMinimumCells)
        {
            noise[carrier] = errorSum[carrier] / errorCount[carrier];
            history.age[carrier] = 0;
        }
        else if (noise[carrier] >= 0.0 && ++history.age[carrier] > CarrierNoiseMaximumAge) {
            noise[carrier] = -1.0;
        }

        if (noise[carrier] >= 0.0) {
            measured.push_back(noise[carrier]);
        }
    }

    if (measured.size() < 16U) {
        return;
    }

    std::nth_element(measured.begin(), measured.begin() + static_cast<std::ptrdiff_t>(measured.size() / 2U), measured.end());
    const double limit = CarrierNoiseThreshold * measured[measured.size() / 2U];
    if (!(limit > 0.0)) {
        return;
    }

    for (std::size_t phase = 0; phase < frames.size(); ++phase)
    {
        const auto& positions = CellMap::mscCells(mode, occupancy, static_cast<int>(phase));
        MSCFrameCells& frame = frames[phase];
        for (std::size_t i = 0; i < positions.size() && i < frame.reliabilities.size(); ++i)
        {
            const double n = noise[static_cast<std::size_t>(positions[i].carrier - spectrum->minimumCarrier)];
            if (n > limit) {
                frame.reliabilities[i] *= static_cast<float>(std::pow(limit / n, CarrierNoiseExponent));
            }
        }
    }
}

const FACService* findFACService(
    const FACAcquisitionResult& fac,
    std::int64_t serviceIdentifier
)
{
    for (const FACData& block : fac.facBlocks)
    {
        for (const FACService& service : block.services)
        {
            if (serviceIdentifier < 0
                || service.serviceIdentifier == static_cast<std::uint32_t>(serviceIdentifier)) {
                return &service;
            }
        }
    }

    return nullptr;
}

bool sameMultiplex(const SDCMultiplexDescription& lhs, const SDCMultiplexDescription& rhs)
{
    return lhs.present == rhs.present
        && lhs.nextConfiguration == rhs.nextConfiguration
        && lhs.protectionLevelA == rhs.protectionLevelA
        && lhs.protectionLevelB == rhs.protectionLevelB
        && std::equal(lhs.streams.begin(), lhs.streams.end(), rhs.streams.begin(), rhs.streams.end(),
            [](const SDCStreamDescription& left, const SDCStreamDescription& right) {
                return left.streamId == right.streamId
                    && left.partALengthBytes == right.partALengthBytes
                    && left.partBLengthBytes == right.partBLengthBytes;
            });
}

bool sameAudio(const SDCAudioInformation& lhs, const SDCAudioInformation& rhs)
{
    return lhs.nextConfiguration == rhs.nextConfiguration
        && lhs.shortId == rhs.shortId
        && lhs.streamId == rhs.streamId
        && lhs.audioCoding == rhs.audioCoding
        && lhs.sbr == rhs.sbr
        && lhs.audioMode == rhs.audioMode
        && lhs.samplingRateCode == rhs.samplingRateCode
        && lhs.text == rhs.text
        && lhs.enhancement == rhs.enhancement
        && lhs.coderField == rhs.coderField
        && lhs.codecSpecificConfig == rhs.codecSpecificConfig;
}

bool sameSDCEntity(const SDCEntity& lhs, const SDCEntity& rhs)
{
    return lhs.type == rhs.type
        && lhs.version == rhs.version
        && lhs.lengthBytes == rhs.lengthBytes
        && lhs.bodyBits == rhs.bodyBits;
}

bool replacesSDCEntity(const SDCEntity& current, const SDCEntity& received)
{
    if (current.type != received.type) {
        return false;
    }

    if ((received.type == 0 || received.type == 9) && current.version != received.version) {
        return false;
    }

    switch (received.type) {
    case 0:
        case 8: return true;
    case 1: case 9: case 12:
        return current.bodyBits.size() >= 2U && received.bodyBits.size() >= 2U
            && current.bodyBits[0] == received.bodyBits[0]
            && current.bodyBits[1] == received.bodyBits[1];
    default:
        return false;
    }
}

bool mergeSDCDataInternal(SDCData& aggregate, const SDCData& block)
{
    if (!block.valid) {
        return false;
    }

    // A list-decoded block can match the CRC by coincidence. When it
    // contradicts a live database (another AFS index, another multiplex, or
    // another audio descriptor for a known service, which would reconfigure
    // the codec), take nothing from it: hold it until a second block agrees.
    const bool liveMultiplex = block.multiplex.present && !block.multiplex.nextConfiguration;
    if (block.listDecoded && aggregate.valid && aggregate.multiplex.present
        && (aggregate.afsIndex != block.afsIndex
            || (liveMultiplex && !sameMultiplex(aggregate.multiplex, block.multiplex))))
    {
        const bool confirmed = aggregate.unconfirmedAFSIndex == block.afsIndex
            && liveMultiplex && aggregate.unconfirmedMultiplex.present
            && sameMultiplex(aggregate.unconfirmedMultiplex, block.multiplex);
        if (!confirmed)
        {
            aggregate.unconfirmedMultiplex = liveMultiplex ? block.multiplex : SDCMultiplexDescription();
            aggregate.unconfirmedAFSIndex = block.afsIndex;
            return false;
        }
    }
    else if (block.listDecoded && aggregate.valid && aggregate.afsIndex == block.afsIndex)
    {
        // Per service: a change is confirmed by a second block with the same
        // descriptor, otherwise held (replacing an earlier held one).
        bool hold = false;
        for (const SDCAudioInformation& audio : block.audio)
        {
            const auto existing = std::find_if(aggregate.audio.begin(), aggregate.audio.end(),
                [&audio](const SDCAudioInformation& item) { return item.shortId == audio.shortId; });
            if (existing == aggregate.audio.end() || sameAudio(*existing, audio)) {
                continue;
            }

            const auto held = std::find_if(aggregate.unconfirmedAudio.begin(), aggregate.unconfirmedAudio.end(),
                [&audio](const SDCAudioInformation& item) { return item.shortId == audio.shortId; });
            if (held != aggregate.unconfirmedAudio.end() && sameAudio(*held, audio)) {
                continue;
            }

            if (held != aggregate.unconfirmedAudio.end()) {
                *held = audio;
            } else {
                aggregate.unconfirmedAudio.push_back(audio);
            }

            hold = true;
        }

        if (hold) {
            return false;
        }
    }

    // A merged descriptor settles any change held for its service.
    for (const SDCAudioInformation& audio : block.audio)
    {
        aggregate.unconfirmedAudio.erase(std::remove_if(aggregate.unconfirmedAudio.begin(),
            aggregate.unconfirmedAudio.end(), [&audio](const SDCAudioInformation& held) {
                return held.shortId == audio.shortId;
            }), aggregate.unconfirmedAudio.end());
    }

    if (!aggregate.valid || aggregate.afsIndex != block.afsIndex)
    {
        aggregate = SDCData();
        aggregate.valid = true;
        aggregate.afsIndex = block.afsIndex;
    }

    aggregate.listDecoded = aggregate.listDecoded || block.listDecoded;

    // Version 0 describes the configuration currently carried by the MSC.
    // Keep version-1 entities in the raw entity list for later atomic
    // reconfiguration handling, but never apply them to the live decoder.
    if (liveMultiplex)
    {
        aggregate.multiplex = block.multiplex;
        aggregate.unconfirmedMultiplex = SDCMultiplexDescription();
        aggregate.unconfirmedAFSIndex = -1;
    }

    if (block.nextMultiplex.present && block.nextMultiplex.nextConfiguration) {
        aggregate.nextMultiplex = block.nextMultiplex;
    }

    // A service can carry several data components, one per packet Id.
    for (const SDCDataApplication& application : block.dataApplications)
    {
        const auto existing = std::find_if(
            aggregate.dataApplications.begin(), aggregate.dataApplications.end(),
            [&application](const SDCDataApplication& item) {
                return item.shortId == application.shortId
                    && item.streamId == application.streamId
                    && item.packetId == application.packetId;
            }
        );
        if (existing == aggregate.dataApplications.end()) {
            aggregate.dataApplications.push_back(application);
        } else {
            *existing = application;
        }
    }

    for (const SDCLabel& label : block.labels)
    {
        const auto existing = std::find_if(
            aggregate.labels.begin(), aggregate.labels.end(),
            [&label](const SDCLabel& item) {
                return item.shortId == label.shortId;
            }
        );
        if (existing == aggregate.labels.end()) {
            aggregate.labels.push_back(label);
        } else {
            *existing = label;
        }
    }

    if (block.timeDate.present) {
        aggregate.timeDate = block.timeDate;
    }

    for (const SDCAudioInformation& audio : block.audio)
    {
        const auto existing = std::find_if(
            aggregate.audio.begin(), aggregate.audio.end(),
            [&audio](const SDCAudioInformation& item) {
                return !item.nextConfiguration && item.shortId == audio.shortId;
            }
        );
        if (existing == aggregate.audio.end()) {
            aggregate.audio.push_back(audio);
        } else {
            *existing = audio;
        }
    }

    for (const SDCAudioInformation& audio : block.nextAudio)
    {
        const auto existing = std::find_if(
            aggregate.nextAudio.begin(), aggregate.nextAudio.end(),
            [&audio](const SDCAudioInformation& item) {
                return item.nextConfiguration && item.shortId == audio.shortId;
            }
        );
        if (existing == aggregate.nextAudio.end()) {
            aggregate.nextAudio.push_back(audio);
        } else {
            *existing = audio;
        }
    }

    for (const SDCLanguageCountry& language : block.languageCountries)
    {
        const auto existing = std::find_if(
            aggregate.languageCountries.begin(), aggregate.languageCountries.end(),
            [&language](const SDCLanguageCountry& item) {
                return item.shortId == language.shortId;
            }
        );
        if (existing == aggregate.languageCountries.end()) {
            aggregate.languageCountries.push_back(language);
        } else {
            *existing = language;
        }
    }

    for (const SDCAlternativeFrequencyEntity& afs : block.alternativeFrequencies)
    {
        const auto sameType = [&afs](const SDCAlternativeFrequencyEntity& item) {
            return item.type == afs.type;
        };
        const auto first = std::find_if(
            aggregate.alternativeFrequencies.begin(),
            aggregate.alternativeFrequencies.end(), sameType
        );
        if (first != aggregate.alternativeFrequencies.end()
            && first->listVersion != afs.listVersion)
        {
            aggregate.alternativeFrequencies.erase(
                std::remove_if(
                    aggregate.alternativeFrequencies.begin(),
                    aggregate.alternativeFrequencies.end(), sameType
                ),
                aggregate.alternativeFrequencies.end()
            );
        }

        const auto duplicate = std::find_if(
            aggregate.alternativeFrequencies.begin(),
            aggregate.alternativeFrequencies.end(),
            [&afs](const SDCAlternativeFrequencyEntity& item) {
                return item.type == afs.type
                    && item.listVersion == afs.listVersion
                    && item.bodyBits == afs.bodyBits;
            }
        );
        if (duplicate == aggregate.alternativeFrequencies.end()
            && aggregate.alternativeFrequencies.size() < SDCData::MaximumAFSEntities) {
            aggregate.alternativeFrequencies.push_back(afs);
        }
    }

    for (const SDCEntity& entity : block.entities)
    {
        const auto replacement = std::find_if(aggregate.entities.begin(), aggregate.entities.end(),
            [&entity](const SDCEntity& item) { return replacesSDCEntity(item, entity); });
        if (replacement != aggregate.entities.end())
        {
            *replacement = entity;
            continue;
        }

        // An AFS version toggle replaces its old raw list as well as the parsed
        // list. Other optional/unknown entities remain bounded diagnostics.
        if (entity.type == 3 || entity.type == 4 || entity.type == 7
            || entity.type == 11 || entity.type == 13 || entity.type == 15)
        {
            aggregate.entities.erase(std::remove_if(aggregate.entities.begin(), aggregate.entities.end(),
                [&entity](const SDCEntity& item) {
                    return item.type == entity.type && item.version != entity.version;
                }), aggregate.entities.end());
        }

        if (std::none_of(aggregate.entities.begin(), aggregate.entities.end(),
                [&entity](const SDCEntity& item) { return sameSDCEntity(item, entity); }))
        {
            if (aggregate.entities.size() == SDCData::MaximumRawEntities)
            {
                // Prefer evicting optional diagnostics, preserving current and
                // pending multiplex/audio entries even when the list is full.
                const auto optional = std::find_if(aggregate.entities.begin(), aggregate.entities.end(),
                    [](const SDCEntity& item) { return item.type != 0 && item.type != 9; });
                if (optional != aggregate.entities.end()) {
                    aggregate.entities.erase(optional);
                }
            }

            if (aggregate.entities.size() < SDCData::MaximumRawEntities) {
                aggregate.entities.push_back(entity);
            }
        }
    }

    return true;
}

} // namespace

bool SDCParser::mergeBlock(SDCData& aggregate, const SDCData& block)
{
    return mergeSDCDataInternal(aggregate, block);
}

bool SDCParser::promoteNextConfiguration(SDCData& data)
{
    if (!data.nextMultiplex.present) {
        return false;
    }

    data.multiplex = std::move(data.nextMultiplex);
    data.multiplex.nextConfiguration = false;
    data.nextMultiplex = SDCMultiplexDescription();
    if (!data.nextAudio.empty()) {
        data.audio = std::move(data.nextAudio);
    }
    else
    {
        // No version-1 audio entity was received before the boundary. Keep
        // the current descriptors whose streams survive, rather than leaving
        // no audio until the next SDC cycle; version-0 entities after the
        // boundary replace them.
        const auto& streams = data.multiplex.streams;
        data.audio.erase(std::remove_if(data.audio.begin(), data.audio.end(),
            [&streams](const SDCAudioInformation& audio) {
                return std::none_of(streams.begin(), streams.end(),
                    [&audio](const SDCStreamDescription& stream) {
                        return stream.streamId == audio.streamId;
                    });
            }), data.audio.end());
    }

    for (SDCAudioInformation& audio : data.audio) {
        audio.nextConfiguration = false;
    }

    // Version-1 data entities are not kept (a component may move to another
    // stream or packet Id): the new configuration's arrive as version 0.
    data.dataApplications.clear();
    data.nextAudio.clear();
    return true;
}

int DRMReceiver::maximumCompleteSuperframes(
    std::size_t sampleCount,
    int sampleRate,
    const FACAcquisitionResult& fac
)
{
    if (sampleRate <= 0 || !fac.valid || fac.facBlocks.empty()) {
        return 0;
    }

    const int firstFrame = firstTransmissionFrame(fac);
    if (firstFrame < 0) {
        return 0;
    }

    const ModeParameters& parameters = Parameters::forMode(fac.coarse.mode);
    const std::size_t frameSamples = static_cast<std::size_t>(
        parameters.symbolsPerFrame * parameters.symbolSamples(sampleRate)
    );
    const std::size_t firstFrameStart = fac.frameStartOffsetSamples
        + static_cast<std::size_t>(firstFrame) * frameSamples;
    if (frameSamples == 0 || firstFrameStart >= sampleCount) {
        return 0;
    }

    const std::size_t availableFrames = (sampleCount - firstFrameStart)
        / frameSamples;
    return static_cast<int>(std::min<std::size_t>(
        availableFrames / static_cast<std::size_t>(parameters.framesPerSuperframe),
        static_cast<std::size_t>(std::numeric_limits<int>::max())
    ));
}

bool DRMReceiver::decodeSDC(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    const FACAcquisitionResult& fac,
    SDCData& sdc,
    int maximumAttempts,
    std::string* error,
    std::vector<std::uint8_t>* resultBits
)
{
    sdc = SDCData();
    if (error) {
        error->clear();
    }

    if (resultBits) {
        resultBits->clear();
    }

    if (!samples || sampleRate <= 0 || maximumAttempts <= 0
        || !fac.valid || fac.facBlocks.empty())
    {
        if (error) {
            *error = "Invalid DRM SDC receiver input";
        }

        return false;
    }

    const Mode mode = fac.coarse.mode;
    const int sdcMode = fac.facBlocks.front().sdcMode;
    if (!SDCDecoder::supports(mode, sdcMode))
    {
        if (error) {
            *error = "Unsupported DRM SDC parameters";
        }

        return false;
    }

    const int firstFrame = firstTransmissionFrame(fac);
    if (firstFrame < 0)
    {
        if (error) {
            *error = "No first transmission frame in FAC sequence";
        }

        return false;
    }

    const ModeParameters& parameters = Parameters::forMode(mode);
    const int symbolSamples = parameters.symbolSamples(sampleRate);
    const std::size_t frameSamples = static_cast<std::size_t>(
        parameters.symbolsPerFrame * symbolSamples
    );
    const std::size_t frameStart = fac.frameStartOffsetSamples
        + static_cast<std::size_t>(firstFrame) * frameSamples;
    const std::size_t superframeSamples = static_cast<std::size_t>(
        parameters.framesPerSuperframe
    ) * frameSamples;
    std::vector<EqualizedSymbol> combined;
    std::vector<std::uint8_t> previousBits;
    int minimumHammingDistance = -1;
    // The AFS cycle is complete after that many valid blocks, not superframes:
    // a lost block leaves its part of the cycle still to be received.
    int validBlocks = 0;

    for (int superframe = 0; superframe < maximumAttempts; ++superframe)
    {
        const std::size_t candidateStart = frameStart
            + superframeSamples * static_cast<std::size_t>(superframe);
        bool hasPreviousFrame = candidateStart >= frameSamples;
        const std::size_t contextStart = hasPreviousFrame
            ? candidateStart - frameSamples : candidateStart;
        const int contextFrameCount = hasPreviousFrame ? 3 : 2;
        std::vector<std::vector<std::vector<std::complex<float>>>> spectra;
        if (!demodulateFrames(
                samples, sampleCount, sampleRate, mode, contextStart,
                contextFrameCount, fac.occupancy,
                hasPreviousFrame ? parameters.framesPerSuperframe - 1 : 0,
                fac.integerCarrierOffset, fac.coarse.fractionalCarrierOffsetHz,
                fac.spectralInversion, spectra)) {
            continue;
        }

        // The previous frame is time-interpolation context for the SDC symbols
        // at the start of frame 0 only if its pilots continue into frame 0 as
        // frame 0's do into frame 1 (a recording can slip between superframes;
        // see decodeMSC). Otherwise decode from the superframe's frames alone.
        if (hasPreviousFrame && spectra.size() == 3U)
        {
            const FrameContinuity reference = frameContinuity(mode, fac.occupancy, fac.integerCarrierOffset,
                sampleRate, spectra[1], 0, spectra[2], 1);
            const FrameContinuity edge = frameContinuity(mode, fac.occupancy, fac.integerCarrierOffset,
                sampleRate, spectra[0], parameters.framesPerSuperframe - 1, spectra[1], 0);
            // Without a valid reference continuity is not shown, as in decodeMSC.
            if (!reference.valid || !edge.valid
                    || edge.coherence < EdgeFrameCoherence * reference.coherence
                    || std::abs(edge.shiftSamples - reference.shiftSamples)
                        > EdgeFrameShiftSamples * referenceRateScale(mode, sampleRate))
            {
                spectra.erase(spectra.begin());
                hasPreviousFrame = false;
            }
        }

        const int targetFrame = hasPreviousFrame ? 1 : 0;
        const int firstFramePhase = hasPreviousFrame
            ? parameters.framesPerSuperframe - 1 : 0;
        std::vector<EqualizedSymbol> equalized;
        // The time/frequency estimator resolves echoes the per-symbol pilot
        // spacing cannot; the per-symbol one remains for mode E without long
        // echoes (the context decides) and as fallback.
        ChannelEstimationContext estimation;
        if (!estimation.prepare(mode, fac.occupancy, firstFramePhase, fac.integerCarrierOffset, spectra)) {
            continue;
        }

        if (!estimation.estimateFrameWiener(targetFrame, equalized)
            && !estimation.estimateFrame(targetFrame, equalized)) {
            continue;
        }

        std::vector<std::uint8_t> decodedBits;
        SDCData block;
        if (SDCDecoder::decode(
                mode, fac.occupancy, sdcMode, equalized, block,
                &decodedBits, error))
        {
            if (resultBits) {
                *resultBits = decodedBits;
            }

            if (error) {
                error->clear();
            }

            const int previousAFSIndex = sdc.valid ? sdc.afsIndex : -1;
            // A held (unconfirmed) block does not count towards the cycle.
            if (SDCParser::mergeBlock(sdc, block)) {
                validBlocks = sdc.afsIndex == previousAFSIndex ? validBlocks + 1 : 1;
            }

            const int cycleLength = std::max(1, sdc.afsIndex + 1);
            if (sdc.multiplex.present && validBlocks >= cycleLength) {
                return true;
            }

            combined.clear();
            previousBits.clear();
            continue;
        }

        if (resultBits) {
            *resultBits = decodedBits;
        }

        if (!previousBits.empty() && previousBits.size() == decodedBits.size())
        {
            const int distance = static_cast<int>(std::inner_product(
                previousBits.begin(), previousBits.end(), decodedBits.begin(),
                0, std::plus<int>(), std::not_equal_to<std::uint8_t>()
            ));
            minimumHammingDistance = minimumHammingDistance < 0
                ? distance : std::min(minimumHammingDistance, distance);
        }

        previousBits = std::move(decodedBits);

        if (combined.empty())
        {
            combined = equalized;
            for (EqualizedSymbol& symbol : combined)
            {
                for (std::size_t i = 0; i < symbol.channel.size(); ++i)
                {
                    symbol.channel[i] = {
                        std::sqrt(std::norm(symbol.channel[i])), 0.0f
                    };
                }
            }
        }
        else if (combined.size() == equalized.size())
        {
            bool compatible = true;
            for (std::size_t symbolIndex = 0;
                compatible && symbolIndex < combined.size();
                ++symbolIndex)
            {
                EqualizedSymbol& destination = combined[symbolIndex];
                const EqualizedSymbol& source = equalized[symbolIndex];
                compatible = destination.minimumCarrier == source.minimumCarrier
                    && destination.maximumCarrier == source.maximumCarrier
                    && destination.cells.size() == source.cells.size()
                    && destination.channel.size() == source.channel.size();
                for (std::size_t i = 0;
                    compatible && i < destination.cells.size();
                    ++i)
                {
                    const float accumulatedWeight = std::norm(
                        destination.channel[i]
                    );
                    const float sourceWeight = std::norm(source.channel[i]);
                    const float totalWeight = accumulatedWeight + sourceWeight;
                    if (totalWeight <= 1.0e-20f) {
                        continue;
                    }

                    destination.cells[i] = (
                        accumulatedWeight * destination.cells[i]
                        + sourceWeight * source.cells[i]
                    ) / totalWeight;
                    destination.channel[i] = {
                        std::sqrt(totalWeight), 0.0f
                    };
                }
            }

            if (!compatible) {
                combined.clear();
            }
        }

        if (superframe > 0 && !combined.empty())
        {
            std::vector<std::uint8_t> combinedBits;
            SDCData combinedBlock;
            if (SDCDecoder::decode(
                    mode, fac.occupancy, sdcMode, combined,
                    combinedBlock, &combinedBits, error))
            {
                if (resultBits) {
                    *resultBits = combinedBits;
                }

                if (error) {
                    error->clear();
                }

                const int previousAFSIndex = sdc.valid ? sdc.afsIndex : -1;
                // A held (unconfirmed) block does not count towards the cycle.
                if (SDCParser::mergeBlock(sdc, combinedBlock)) {
                    validBlocks = sdc.afsIndex == previousAFSIndex ? validBlocks + 1 : 1;
                }

                const int cycleLength = std::max(1, sdc.afsIndex + 1);
                if (sdc.multiplex.present && validBlocks >= cycleLength) {
                    return true;
                }

                combined.clear();
                previousBits.clear();
            }
        }
    }

    if (sdc.valid)
    {
        if (error) {
            error->clear();
        }

        return true;
    }

    if (error)
    {
        if (error->empty()) {
            *error = "No CRC-valid DRM SDC block found";
        }

        if (minimumHammingDistance >= 0)
        {
            *error += " (minimum repeated-block Hamming distance "
                + std::to_string(minimumHammingDistance) + ")";
        }
    }

    return false;
}

bool DRMReceiver::decodeMSC(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    const FACAcquisitionResult& fac,
    const SDCData& sdc,
    ReceiverMSCReport& report,
    std::int64_t selectedServiceIdentifier,
    int superframeCount,
    const PCMCallback& pcmCallback,
    std::string* error,
    DRMTextMessageDecoder* textMessageDecoder,
    DRMAudioDecoder* persistentAudioDecoder,
    MSCCellDeinterleaver* persistentCellDeinterleaver,
    ModeEAudioSuperframeAssembler* persistentModeEAudioAssembler,
    std::vector<std::vector<std::uint8_t>>* decodedFrames,
    XHEAACSuperframeParser* persistentXHEAudioParser,
    bool erasedTransmission,
    const FACServiceCache* serviceCache,
    SampleClockState* sampleClock,
    CarrierNoiseHistory* carrierNoise,
    DRMSlideshowDecoder* slideshowDecoder,
    DRMJournalineDecoder* journalineDecoder,
    DRMWebsiteDecoder* websiteDecoder
)
{
    report = ReceiverMSCReport();
    if (error) {
        error->clear();
    }

    if (decodedFrames) {
        decodedFrames->clear();
    }

    if (!samples || sampleRate <= 0 || superframeCount < 0
        || !fac.valid || fac.facBlocks.empty() || !sdc.multiplex.present)
    {
        if (error) {
            *error = "Missing FAC or SDC configuration for DRM MSC";
        }

        return false;
    }

    const Mode mode = fac.coarse.mode;
    const FACData& channel = fac.facBlocks.front();
    const bool unequalProtection = std::any_of(
        sdc.multiplex.streams.begin(), sdc.multiplex.streams.end(),
        [](const SDCStreamDescription& stream) { return stream.partALengthBytes != 0; }
    );
    if (!MSCChannelDecoder::supports(mode, channel.mscMode, unequalProtection))
    {
        if (error) {
            *error = "Unsupported DRM MSC profile";
        }

        return false;
    }

    const int firstFrame = firstTransmissionFrame(fac);
    if (firstFrame < 0)
    {
        if (error) {
            *error = "No first transmission frame in FAC sequence";
        }

        return false;
    }

    const ModeParameters& parameters = Parameters::forMode(mode);
    const int symbolSamples = parameters.symbolSamples(sampleRate);
    const std::size_t frameSamples = static_cast<std::size_t>(parameters.symbolsPerFrame)
        * static_cast<std::size_t>(symbolSamples);
    const std::size_t firstFrameStart = fac.frameStartOffsetSamples
        + static_cast<std::size_t>(firstFrame) * frameSamples;
    const int availableSuperframes = maximumCompleteSuperframes(
        sampleCount, sampleRate, fac
    );
    const int effectiveSuperframeCount = superframeCount == 0
        ? availableSuperframes : superframeCount;
    if (effectiveSuperframeCount <= 0)
    {
        if (error) {
            *error = "No complete DRM transmission superframe available";
        }

        return false;
    }

    // Decoding holds every frame's spectra at once; bound a single call so a
    // whole-file span cannot overflow the frame count or exhaust memory.
    if (effectiveSuperframeCount > MaximumSuperframesPerCall)
    {
        if (error) {
            *error = "Too many DRM superframes requested in one MSC decode";
        }

        return false;
    }

    const int totalFrames = effectiveSuperframeCount
        * parameters.framesPerSuperframe;
    const std::size_t requiredSamples = firstFrameStart
        + static_cast<std::size_t>(totalFrames) * frameSamples;
    if (requiredSamples > sampleCount)
    {
        if (error) {
            *error = "Insufficient samples for DRM MSC interleaver fill";
        }

        return false;
    }

    std::vector<std::vector<std::vector<std::complex<float>>>> spectra;
    double sampleClockErrorPPM = 0.0;
    if (!erasedTransmission && !demodulateFrames(
            samples, sampleCount, sampleRate, mode, firstFrameStart, totalFrames,
            fac.occupancy, 0, fac.integerCarrierOffset,
            fac.coarse.fractionalCarrierOffsetHz, fac.spectralInversion, spectra,
            &sampleClockErrorPPM, sampleClock))
    {
        if (error) {
            *error = "DRM MSC OFDM demodulation failed";
        }

        return false;
    }

    report.sampleClockErrorPPM = erasedTransmission && sampleClock && sampleClock->haveDrift
        ? sampleClock->driftPerFrame * 1.0e6 / static_cast<double>(frameSamples)
        : sampleClockErrorPPM;

    std::unique_ptr<DRMAudioDecoder> ownedAudioDecoder;
    if (!persistentAudioDecoder)
    {
        ownedAudioDecoder = DRMAudioDecoder::create();
        persistentAudioDecoder = ownedAudioDecoder.get();
    }

    const AudioDecoderCapabilities capabilities =
        persistentAudioDecoder->capabilities();
    const SDCAudioInformation* selectedAudio = nullptr;
    const FACService* selectedDataService = nullptr;
    const auto serviceForShortId = [&fac, serviceCache](int shortId) -> const FACService* {
        if (serviceCache) {
            return serviceCache->findShortIdentifier(shortId);
        }

        for (const FACData& block : fac.facBlocks)
        {
            for (const FACService& service : block.services)
            {
                if (service.shortIdentifier == shortId) {
                    return &service;
                }
            }
        }

        return nullptr;
    };
    const auto serviceForAudio = [&serviceForShortId](const SDCAudioInformation& audio) {
        return serviceForShortId(audio.shortId);
    };
    if (selectedServiceIdentifier >= 0)
    {
        const FACService* facService = serviceCache
            ? serviceCache->findIdentifier(static_cast<std::uint32_t>(selectedServiceIdentifier))
            : findFACService(fac, selectedServiceIdentifier);
        if (!facService) {
            report.audioError = "Selected DRM service identifier is absent";
        }
        else
        {
            const auto found = std::find_if(
                sdc.audio.begin(), sdc.audio.end(), [facService](
                    const SDCAudioInformation& audio) {
                    return audio.shortId == facService->shortIdentifier;
                }
            );
            // The FAC says whether it is a data service: an audio descriptor
            // left from an earlier configuration does not make it audio.
            if (facService->dataService) {
                selectedDataService = facService;
            } else if (found != sdc.audio.end()) {
                selectedAudio = &*found;
            } else {
                report.audioError = "Selected DRM audio descriptor is unavailable";
            }
        }
    }
    else
    {
        const auto audioService = [&serviceForAudio](const SDCAudioInformation& audio) {
            const FACService* service = serviceForAudio(audio);
            return service && !service->dataService;
        };
        const auto supported = std::find_if(
            sdc.audio.begin(), sdc.audio.end(), [&capabilities, &serviceForAudio, &audioService](
                const SDCAudioInformation& audio) {
                return audioService(audio) && !serviceForAudio(audio)->audioConditionalAccess
                    && audioCodingSupported(capabilities, audio);
            }
        );
        // Audio that this receiver could decode with the codec: AAC or
        // xHE-AAC without conditional access. Selecting it without its codec
        // reports the codec as unavailable.
        const auto decodable = std::find_if(
            sdc.audio.begin(), sdc.audio.end(), [&serviceForAudio, &audioService](const SDCAudioInformation& audio) {
                return audioService(audio) && !serviceForAudio(audio)->audioConditionalAccess
                    && ((audio.audioCoding == 0) || (audio.audioCoding == 3));
            }
        );
        if (supported != sdc.audio.end()) {
            selectedAudio = &*supported;
        } else if (decodable != sdc.audio.end()) {
            selectedAudio = &*decodable;
        }
        else
        {
            // Without audio that could be decoded, the first data service with
            // a slideshow or website
            for (const SDCDataApplication& application : sdc.dataApplications)
            {
                const FACService* service = serviceForShortId(application.shortId);

                if (service && service->dataService && !service->dataConditionalAccess
                    && (DRMSlideshowDecoder::find(sdc, application.shortId) || DRMWebsiteDecoder::find(sdc, application.shortId)))
                {
                    selectedDataService = service;
                    break;
                }
            }

            // Else the first audio service, to report why it cannot be played:
            // one the FAC has not announced yet, never a data service's
            // descriptor left from an earlier configuration
            const auto first = std::find_if(sdc.audio.begin(), sdc.audio.end(), audioService);
            const auto unannounced = std::find_if(sdc.audio.begin(), sdc.audio.end(),
                [&serviceForAudio](const SDCAudioInformation& audio) { return !serviceForAudio(audio); });

            if (!selectedDataService) {
                selectedAudio = first != sdc.audio.end() ? &*first : (unannounced != sdc.audio.end() ? &*unannounced : nullptr);
            }
        }
    }

    bool decodeAudio = false;
    bool parseAudio = false;
    if (selectedAudio)
    {
        const FACService* facService = serviceForAudio(*selectedAudio);
        if (facService) {
            report.selectedServiceIdentifier = facService->serviceIdentifier;
        }

        report.selectedShortId = selectedAudio->shortId;
        report.selectedStreamId = selectedAudio->streamId;
        report.audioCodecAvailable = audioCodingSupported(capabilities, *selectedAudio);
        parseAudio = selectedAudio->audioCoding == 0 || selectedAudio->audioCoding == 3;
        if (!facService)
        {
            parseAudio = false;
            report.audioError = "Selected DRM audio descriptor has no current FAC service";
        }
        else if (facService->audioConditionalAccess)
        {
            parseAudio = false;
            report.audioError = "DRM conditional-access audio is not supported";
        }
        else if (!parseAudio) {
            report.audioError = "DRM audio coding is not supported (AAC and xHE-AAC only)";
        }
        else if (!report.audioCodecAvailable)
        {
            report.audioError = selectedAudio->audioCoding == 3
                ? "DRM xHE-AAC codec is unavailable" : "DRM AAC codec is unavailable";
        }
        else
        {
            decodeAudio = persistentAudioDecoder->configure(*selectedAudio, &report.audioError);
            if (!decodeAudio && report.audioError.empty()) {
                report.audioError = "DRM audio codec rejected the configuration";
            }
        }
    }
    else if (selectedDataService)
    {
        report.selectedServiceIdentifier = selectedDataService->serviceIdentifier;
        report.selectedShortId = selectedDataService->shortIdentifier;
        report.dataService = true;
    }
    else if (report.audioError.empty()) {
        report.audioError = "No DRM audio descriptor is available";
    }

    // The selected service's slideshow and website, decoded from the same
    // multiplex frames as its audio (not for conditional-access services).
    const SDCDataApplication* slideshow = nullptr;
    const SDCDataApplication* website = nullptr;
    const FACService* dataFACService = selectedAudio ? serviceForAudio(*selectedAudio) : selectedDataService;

    // Another service's data and text go before this one's are decoded (not
    // while the selection is unknown, as when the SDC is rebuilt after a
    // lost lock)
    if (dataFACService)
    {
        const bool slideshowMoved = slideshowDecoder && slideshowDecoder->setService(dataFACService->serviceIdentifier);
        const bool websiteMoved = websiteDecoder && websiteDecoder->setService(dataFACService->serviceIdentifier);
        report.serviceDataReset = slideshowMoved || websiteMoved;
        report.textMessageReset = textMessageDecoder && textMessageDecoder->setService(dataFACService->serviceIdentifier);
    }

    if (dataFACService && !dataFACService->dataConditionalAccess)
    {
        if (slideshowDecoder) {
            slideshow = DRMSlideshowDecoder::find(sdc, dataFACService->shortIdentifier);
        }

        if (websiteDecoder) {
            website = DRMWebsiteDecoder::find(sdc, dataFACService->shortIdentifier);
        }
    }

    report.slideshowPresent = slideshow != nullptr;
    report.websitePresent = website != nullptr;

    const auto deliverPCM = [&](const PCMFrame& pcm) {
        if (pcm.samples.empty()) {
            return;
        }

        if (pcm.concealed) {
            ++report.concealedPCMFrames;
        } else {
            ++report.decodedPCMFrames;
        }

        if (pcmCallback) {
            pcmCallback(pcm);
        }
    };
    const auto concealFrame = [&]() {
        if (!decodeAudio) {
            return;
        }

        PCMFrame pcm;
        std::string concealError;
        if (persistentAudioDecoder->conceal(pcm, &concealError))
        {
            pcm.concealed = true;
            deliverPCM(pcm);
        }
    };
    // What xHE concealment gave beyond the superframes lost, carried by the
    // caller's parser between calls
    double localConcealedSurplus = 0.0;
    double& concealedSurplus = persistentXHEAudioParser
        ? persistentXHEAudioParser->concealedSurplusSeconds : localConcealedSurplus;
    const auto concealAACSuperframe = [&]() {
        if (!selectedAudio || !decodeAudio) {
            return;
        }

        const int frames = AACSuperframeParser::frameCount(mode, *selectedAudio);
        if (frames >= 0)
        {
            for (int i = 0; i < frames; ++i) {
                concealFrame();
            }

            return;
        }

        // xHE-AAC: the number of access units varies, so conceal the audio
        // superframe's duration (400 ms, 200 ms in mode E), less what earlier
        // losses gave beyond theirs
        const double duration = (mode == Mode::E ? 0.2 : 0.4) - concealedSurplus;
        double concealed = 0.0;
        for (int i = 0; (i < 64) && (concealed < duration - 1.0e-6); ++i)
        {
            PCMFrame pcm;
            std::string concealError;
            if (!persistentAudioDecoder->conceal(pcm, &concealError) || pcm.samples.empty()
                || (pcm.channels <= 0) || (pcm.sampleRate <= 0)) {
                break;
            }

            pcm.concealed = true;
            concealed += static_cast<double>(pcm.samples.size()) / (pcm.channels * pcm.sampleRate);
            deliverPCM(pcm);
        }

        concealedSurplus = std::max(0.0, concealed - duration);
    };

    std::size_t signalledBytes = 0;
    std::size_t partABytes = 0;
    for (const SDCStreamDescription& stream : sdc.multiplex.streams)
    {
        partABytes += static_cast<std::size_t>(stream.partALengthBytes);
        signalledBytes += static_cast<std::size_t>(
            stream.partALengthBytes + stream.partBLengthBytes
        );
    }

    const MSCProtectionParameters protection {
        sdc.multiplex.protectionLevelA,
        sdc.multiplex.protectionLevelB,
        partABytes
    };
    std::vector<MSCFrameCells> transmissionFrames(
        static_cast<std::size_t>(parameters.framesPerSuperframe)
    );
    std::vector<EqualizedSymbol::PilotError> pilotErrors; // this superframe's, for weightForErrorFloor
    CarrierNoiseHistory localCarrierNoise; // kept across this call's superframes without a caller's
    CarrierNoiseHistory& noiseHistory = carrierNoise ? *carrierNoise : localCarrierNoise;
    std::unique_ptr<MSCCellDeinterleaver> ownedCellDeinterleaver;
    if (!persistentCellDeinterleaver)
    {
        ownedCellDeinterleaver = std::make_unique<MSCCellDeinterleaver>(
            mode, !channel.shortInterleaving
        );
        persistentCellDeinterleaver = ownedCellDeinterleaver.get();
    }

    ModeEAudioSuperframeAssembler localModeEAudioAssembler;
    ModeEAudioSuperframeAssembler& modeEAudioAssembler =
        persistentModeEAudioAssembler
            ? *persistentModeEAudioAssembler : localModeEAudioAssembler;
    XHEAACSuperframeParser localXHEAudioParser;
    XHEAACSuperframeParser& xheAudioParser = persistentXHEAudioParser
        ? *persistentXHEAudioParser : localXHEAudioParser;
    DRMTextMessageDecoder localTextDecoder;
    DRMTextMessageDecoder& textDecoder = textMessageDecoder
        ? *textMessageDecoder : localTextDecoder;
    double merReferencePower = 0.0;
    double merErrorPower = 0.0;
    std::size_t merCellCount = 0;

    // Every frame is equalized from the same window of pilot estimates. One
    // frame either side, where the input holds it, gives the time
    // interpolation pilots beyond the superframe's edges, where it would
    // otherwise extrapolate from one side. Those frames are demodulated
    // without steering the sample clock (it stays measured on the decoded
    // frames); every frame's spectra share the nominal window's phase
    // reference. A frame is used only if its pilots continue into the
    // superframe's as the superframe's own frames do into each other: a
    // recording can slip samples between superframes, which each
    // superframe's fresh timing absorbs but interpolation across would not.
    int leadingFrames = 0;
    // Not in mode E: its per-symbol interpolation smooths over three symbols
    // either side, and the mode E sweeps showed no gain.
    if (!erasedTransmission && mode != Mode::E && totalFrames >= 2)
    {
        const int lastPhase = parameters.framesPerSuperframe - 1;
        const double shiftTolerance = EdgeFrameShiftSamples * referenceRateScale(mode, sampleRate);
        std::vector<double> coherences;
        std::vector<double> shifts;
        for (int frame = 0; frame + 1 < totalFrames; ++frame)
        {
            const FrameContinuity continuity = frameContinuity(mode, fac.occupancy, fac.integerCarrierOffset,
                sampleRate, spectra[static_cast<std::size_t>(frame)], frame % parameters.framesPerSuperframe,
                spectra[static_cast<std::size_t>(frame + 1)], (frame + 1) % parameters.framesPerSuperframe);
            if (!continuity.valid) {
                continue;
            }

            coherences.push_back(continuity.coherence);
            shifts.push_back(continuity.shiftSamples);
        }

        // A superframe whose own frames slip is not extended either. With one
        // superframe (two frame pairs in A-D) the "median" is the larger value.
        if (!coherences.empty())
        {
            const auto median = [](std::vector<double> values) {
                const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2U);
                std::nth_element(values.begin(), middle, values.end());
                return *middle;
            };
            const double referenceCoherence = median(coherences);
            const double referenceShift = median(shifts);
            const auto continuous = [&](const FrameContinuity& continuity) {
                return continuity.valid && continuity.coherence >= EdgeFrameCoherence * referenceCoherence
                    && std::abs(continuity.shiftSamples - referenceShift) <= shiftTolerance;
            };
            std::vector<std::vector<std::vector<std::complex<float>>>> edge;
            const bool steady = std::all_of(shifts.begin(), shifts.end(), [referenceShift, shiftTolerance](double shift) {
                return std::abs(shift - referenceShift) <= shiftTolerance;
            });
            if (steady && firstFrameStart >= frameSamples && demodulateFrames(
                    samples, sampleCount, sampleRate, mode, firstFrameStart - frameSamples, 1,
                    fac.occupancy, lastPhase, fac.integerCarrierOffset,
                    fac.coarse.fractionalCarrierOffsetHz, fac.spectralInversion, edge)
                && continuous(frameContinuity(mode, fac.occupancy, fac.integerCarrierOffset, sampleRate,
                    edge.front(), lastPhase, spectra.front(), 0)))
            {
                spectra.insert(spectra.begin(), std::move(edge.front()));
                leadingFrames = 1;
            }

            if (steady && demodulateFrames(
                    samples, sampleCount, sampleRate, mode, requiredSamples, 1,
                    fac.occupancy, 0, fac.integerCarrierOffset,
                    fac.coarse.fractionalCarrierOffsetHz, fac.spectralInversion, edge)
                && continuous(frameContinuity(mode, fac.occupancy, fac.integerCarrierOffset, sampleRate,
                    spectra.back(), lastPhase, edge.front(), 0))) {
                spectra.push_back(std::move(edge.front()));
            }
        }
    }

    ChannelEstimationContext estimation;
    const bool estimationPrepared = !erasedTransmission && estimation.prepare(
        mode, fac.occupancy, leadingFrames > 0 ? parameters.framesPerSuperframe - 1 : 0,
        fac.integerCarrierOffset, spectra
    );
    for (int frame = 0; frame < totalFrames; ++frame)
    {
        const int framePhase = frame % parameters.framesPerSuperframe;
        if (erasedTransmission)
        {
            auto& erased = transmissionFrames[static_cast<std::size_t>(framePhase)];
            erased.frameInSuperframe = framePhase;
            const auto count = CellMap::mscCells(mode, fac.occupancy, framePhase).size();
            erased.cells.assign(count, std::complex<float>());
            erased.reliabilities.assign(count, 0.0f);
        }
        else
        {
            std::vector<EqualizedSymbol> equalized;
            bool estimated = estimationPrepared
                && estimation.estimateFrameWiener(frame + leadingFrames, equalized);
            if (!estimated)
            {
                estimated = estimationPrepared
                    && estimation.estimateFrame(frame + leadingFrames, equalized, 3);
            }

            if (!estimated)
            {
                if (error) {
                    *error = "DRM MSC channel estimation failed";
                }

                return false;
            }

            if (!MSCExtractor::extract(
                    mode, fac.occupancy, framePhase, equalized,
                    transmissionFrames[static_cast<std::size_t>(framePhase)], error)) {
                return false;
            }

            for (const EqualizedSymbol& symbol : equalized) {
                pilotErrors.insert(pilotErrors.end(), symbol.pilotErrors.begin(), symbol.pilotErrors.end());
            }
        }

        if (framePhase != parameters.framesPerSuperframe - 1) {
            continue;
        }

        if (!erasedTransmission)
        {
            // The carrier noise is measured on the floor-weighted reliabilities:
            // each carrier is judged by the error it adds to the metrics the
            // decoder uses (measured on |H| itself, Kuwait 10:02 decoded 11
            // frames fewer and Kuwait 11:19 9 fewer).
            const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, fac.occupancy);
            weightForErrorFloor(transmissionFrames, pilotErrors,
                noisyCarriers(mode, fac.occupancy, channel.mscMode, noiseHistory),
                spectrum ? spectrum->minimumCarrier : 0);
            weightForCarrierNoise(mode, fac.occupancy, channel.mscMode, transmissionFrames, noiseHistory);
        }
        else
        {
            ageCarrierNoise(noiseHistory);
        }

        pilotErrors.clear();

        MSCSuperframeCells transmissionSuperframe;
        if (!MSCSuperframeAssembler::assemble(
                mode, fac.occupancy, transmissionFrames, transmissionSuperframe, error)) {
            return false;
        }

        std::vector<MSCMultiplexFrameCells> transmittedMultiplexFrames;
        if (!MSCMultiplexFrameSplitter::split(
                mode, fac.occupancy, transmissionSuperframe,
                transmittedMultiplexFrames, error)) {
            return false;
        }

        for (const MSCMultiplexFrameCells& transmitted : transmittedMultiplexFrames)
        {
            MSCMultiplexFrameCells deinterleaved;
            bool ready = false;
            if (!persistentCellDeinterleaver->push(
                    transmitted, deinterleaved, ready, error)) {
                return false;
            }

            if (!ready) {
                continue;
            }

            report.mscReady = true;

            accumulateDecisionDirectedMER(
                mode, channel.mscMode, deinterleaved,
                merReferencePower, merErrorPower, merCellCount
            );

            // A lost frame breaks the data components' packet sequences:
            // their partial data units go, completed objects stay
            const auto lostDataFrame = [&]() {
                if (journalineDecoder) {
                    journalineDecoder->frameLost();
                }

                if (slideshowDecoder) {
                    slideshowDecoder->frameLost();
                }

                if (websiteDecoder) {
                    websiteDecoder->frameLost();
                }
            };
            const auto lostMultiplexFrame = [&]() {
                modeEAudioAssembler.reset();
                xheAudioParser.reset();
                if (mode != Mode::E || (deinterleaved.frameInSuperframe & 1))
                {
                    ++report.invalidAudioSuperframes;
                    concealAACSuperframe();
                }
            };
            if (std::none_of(deinterleaved.reliabilities.begin(), deinterleaved.reliabilities.end(),
                    [](float reliability) { return reliability > 0.0f; }))
            {
                ++report.invalidMultiplexFrames;
                lostDataFrame();
                if (parseAudio) {
                    lostMultiplexFrame();
                }

                continue;
            }

            MSCDecodedMultiplexFrame decoded;
            if (!MSCChannelDecoder::decode(
                    mode, fac.occupancy, channel.mscMode,
                    protection, deinterleaved, decoded, error)) {
                return false;
            }

            if (decodedFrames) {
                decodedFrames->push_back(decoded.bits);
            }

            // The padding after the signalled streams is zero when sent. It ends
            // the weakest MLC level, so a residual error confined to it would,
            // if it rejected the frame, drop audio whose own CRCs pass: reject
            // only a gross failure (over an eighth of the padding bits set).
            const std::size_t payloadBits = 8U * signalledBytes;
            const auto paddingErrors = payloadBits > decoded.bits.size() ? 0
                : std::count_if(decoded.bits.begin() + static_cast<std::ptrdiff_t>(payloadBits),
                    decoded.bits.end(), [](std::uint8_t bit) { return bit != 0U; });
            const std::size_t paddingBits = payloadBits > decoded.bits.size() ? 0U
                : decoded.bits.size() - payloadBits;
            if (payloadBits > decoded.bits.size()
                || static_cast<std::size_t>(paddingErrors) * 8U > std::max<std::size_t>(paddingBits, 8U))
            {
                ++report.invalidMultiplexFrames;
                lostDataFrame();
                if (parseAudio) {
                    lostMultiplexFrame();
                }

                continue;
            }

            std::vector<MSCLogicalStreamFrame> streams;
            if (!MSCMultiplexParser::extractStreams(
                    decoded, sdc.multiplex, streams, error))
            {
                ++report.invalidMultiplexFrames;
                lostDataFrame();
                if (parseAudio) {
                    lostMultiplexFrame();
                }

                continue;
            }

            ++report.decodedMultiplexFrames;

            if (journalineDecoder && journalineDecoder->push(sdc, streams)) {
                report.journalineUpdated = true;
            }

            if (slideshow)
            {
                const auto dataStream = std::find_if(streams.begin(), streams.end(),
                    [slideshow](const MSCLogicalStreamFrame& stream) { return stream.streamId == slideshow->streamId; });

                if (dataStream != streams.end())
                {
                    std::vector<std::uint8_t> bytes = dataStream->partA;
                    bytes.insert(bytes.end(), dataStream->partB.begin(), dataStream->partB.end());
                    slideshowDecoder->push(*slideshow, bytes, report.slideshowObjects);
                }

                report.slideshowValidPackets = slideshowDecoder->validPackets();
                report.slideshowInvalidPackets = slideshowDecoder->invalidPackets();
            }

            if (website)
            {
                const auto dataStream = std::find_if(streams.begin(), streams.end(),
                    [website](const MSCLogicalStreamFrame& stream) { return stream.streamId == website->streamId; });

                if (dataStream != streams.end())
                {
                    std::vector<std::uint8_t> bytes = dataStream->partA;
                    bytes.insert(bytes.end(), dataStream->partB.begin(), dataStream->partB.end());
                    report.websiteUpdated = websiteDecoder->push(*website, bytes) || report.websiteUpdated;
                }

                report.websiteValidPackets = websiteDecoder->validPackets();
                report.websiteInvalidPackets = websiteDecoder->invalidPackets();
            }

            if (!parseAudio) {
                continue;
            }

            const auto selectedStream = std::find_if(
                streams.begin(), streams.end(), [selectedAudio](
                    const MSCLogicalStreamFrame& stream) {
                    return stream.streamId == selectedAudio->streamId;
                }
            );
            // The following are service/data errors, not RF errors: report
            // them, conceal the audio, and keep RF, MSC and interleaver state.
            if (selectedStream == streams.end())
            {
                report.audioError = "DRM audio stream is absent from multiplex frame";
                lostMultiplexFrame();
                continue;
            }

            std::vector<std::uint8_t> logicalAudio = selectedStream->partA;
            logicalAudio.insert(
                logicalAudio.end(), selectedStream->partB.begin(), selectedStream->partB.end()
            );
            if (selectedAudio->text)
            {
                if (logicalAudio.size() < 4U)
                {
                    report.audioError = "DRM audio logical frame is shorter than its text field";
                    lostMultiplexFrame();
                    continue;
                }

                const std::size_t textOffset = logicalAudio.size() - 4U;
                const std::array<std::uint8_t, 4> textPiece {
                    logicalAudio[textOffset], logicalAudio[textOffset + 1U],
                    logicalAudio[textOffset + 2U], logicalAudio[textOffset + 3U]
                };
                DRMTextMessage textMessage;
                bool textReady = false;
                std::string textError;
                if (!textDecoder.push(
                        textPiece, textMessage, textReady, &textError)) {
                    ++report.invalidTextSegments;
                }
                else if (textReady)
                {
                    report.textMessageUpdated = true;
                    report.clearTextMessage = textMessage.clearDisplay;
                    report.textMessage = std::move(textMessage.utf8);
                }

                logicalAudio.resize(logicalAudio.size() - 4U);
            }

            // Part A leads the logical frame; the text field, if any, ends it.
            const std::size_t logicalPartABytes = std::min(selectedStream->partA.size(), logicalAudio.size());
            std::vector<std::uint8_t> audioSuperframeBytes;
            std::size_t audioPartABytes = logicalPartABytes;
            bool audioReady = false;
            if (mode == Mode::E)
            {
                std::string assemblyError;
                audioPartABytes = 2U * logicalPartABytes;
                if (!modeEAudioAssembler.push(
                        deinterleaved.frameInSuperframe, logicalAudio,
                        audioSuperframeBytes, audioReady, &assemblyError, logicalPartABytes))
                {
                    report.audioError = assemblyError;
                    lostMultiplexFrame();
                    continue;
                }
            }
            else
            {
                audioSuperframeBytes = std::move(logicalAudio);
                audioReady = true;
            }

            if (!audioReady)
            {
                if (mode == Mode::E && (deinterleaved.frameInSuperframe & 1))
                {
                    ++report.invalidAudioSuperframes;
                    xheAudioParser.reset();
                    concealAACSuperframe();
                }

                continue;
            }

            AACSuperframe audioSuperframe;
            std::string audioFrameError;
            const bool parsedAudio = selectedAudio->audioCoding == 0
                ? AACSuperframeParser::parse(
                    mode, *selectedAudio, audioSuperframeBytes,
                    audioSuperframe, &audioFrameError, audioPartABytes)
                : selectedAudio->audioCoding == 3
                    ? xheAudioParser.push(
                        audioSuperframeBytes, audioSuperframe, &audioFrameError)
                    : false;
            if (!parsedAudio)
            {
                ++report.invalidAudioSuperframes;
                xheAudioParser.reset();
                concealAACSuperframe();
                continue;
            }

            if (audioSuperframe.invalidFrameCount > 0) {
                ++report.invalidAudioSuperframes;
            }

            ++report.parsedAudioSuperframes;
            if (!decodeAudio) {
                continue;
            }

            for (const int slot : audioSuperframe.codecFrameSequence)
            {
                if (slot < 0)
                {
                    concealFrame();
                    continue;
                }

                const auto& codecFrame = audioSuperframe.codecFrames[static_cast<std::size_t>(slot)];
                PCMFrame pcm;
                std::string codecError;
                if (!persistentAudioDecoder->decode(codecFrame, pcm, &codecError))
                {
                    concealFrame();
                    continue;
                }

                deliverPCM(pcm);
            }
        }
    }

    report.processedSuperframes = effectiveSuperframeCount;
    report.consumedSamples = requiredSamples;
    if (merCellCount > 0 && merReferencePower > 1.0e-20)
    {
        report.mscMERAvailable = true;
        report.mscMERDB = 10.0 * std::log10(
            merReferencePower / std::max(merErrorPower, 1.0e-20)
        );
        report.mscMERCellCount = merCellCount;
    }

    if (error) {
        error->clear();
    }

    return true;
}

} // namespace modemdrm
