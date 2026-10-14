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

#include "drmfacacquisition.h"

#include "drmcellmap.h"
#include "drmchannelestimator.h"
#include "drmofdm.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace modemdrm
{

namespace
{
constexpr double WindowPlacementSmoothing = 0.1;
constexpr double WindowPlacementSpanFraction = 0.8;

bool coherentTransition(Mode mode, const FACData& current, const FACData& next)
{
    if (current.occupancy != next.occupancy
        || current.modeE != next.modeE
        || current.mscMode != next.mscMode
        || current.sdcMode != next.sdcMode
        || current.legacyModeETail != next.legacyModeETail
        || current.shortInterleaving != next.shortInterleaving) {
        return false;
    }

    const bool nextIsFirst = next.identity == 0 || next.identity == 3;
    if (mode != Mode::E)
    {
        if (current.identity == 0 || current.identity == 3) {
            return next.identity == 1;
        }

        if (current.identity == 1) {
            return next.identity == 2;
        }

        if (current.identity == 2) {
            return nextIsFirst;
        }

        return false;
    }

    if (current.identity == 0 || current.identity == 3) {
        return !current.toggle && next.identity == 1 && next.toggle;
    }

    if (current.identity == 1 && current.toggle) {
        return next.identity == 1 && !next.toggle;
    }

    if (current.identity == 1 && !current.toggle) {
        return next.identity == 2 && next.toggle;
    }

    if (current.identity == 2) {
        return current.toggle && nextIsFirst && !next.toggle;
    }

    return false;
}

std::vector<int> occupanciesForMode(Mode mode)
{
    std::vector<int> result;
    for (const SpectrumParameters& spectrum : Parameters::spectra())
    {
        if (spectrum.mode == mode) {
            result.push_back(spectrum.occupancy);
        }
    }

    return result;
}

bool decodeTrackedFAC(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    const AcquisitionCandidate& coarse,
    int occupancy,
    int integerCarrierOffset,
    bool spectralInversion,
    std::size_t frameStartOffsetSamples,
    const std::vector<FACData>& previousBlocks,
    std::vector<FACData>& blocks,
    std::vector<bool>& blockValid,
    int& validatedBlockCount
)
{
    const Mode mode = coarse.mode;
    const ModeParameters& parameters = Parameters::forMode(mode);
    const int symbolsPerFrame = parameters.symbolsPerFrame;
    const int symbolSamples = parameters.symbolSamples(sampleRate);
    if (symbolSamples <= 0 || !Parameters::forSpectrum(mode, occupancy)) {
        return false;
    }

    const int blockCount = parameters.framesPerSuperframe;
    blockValid.assign(static_cast<std::size_t>(blockCount), false);
    const int symbolCount = blockCount * symbolsPerFrame;
    OFDMDemodulator demodulator(mode, sampleRate);
    // Demodulated straight into frames, which the estimation context shares.
    std::vector<std::vector<std::vector<std::complex<float>>>> framedSpectra(
        static_cast<std::size_t>(blockCount),
        std::vector<std::vector<std::complex<float>>>(
            static_cast<std::size_t>(symbolsPerFrame)
        )
    );
    for (int symbol = 0; symbol < symbolCount; ++symbol)
    {
        const std::size_t prefixStart = frameStartOffsetSamples
            + static_cast<std::size_t>(symbol * symbolSamples);
        if (!demodulator.demodulate(
                samples, sampleCount, prefixStart,
                coarse.fractionalCarrierOffsetHz,
                framedSpectra[static_cast<std::size_t>(symbol / symbolsPerFrame)][
                    static_cast<std::size_t>(symbol % symbolsPerFrame)],
                spectralInversion)) {
            return false;
        }
    }

    std::vector<int> facSymbols;
    for (const CellPosition& position : CellMap::facCells(mode))
    {
        if (std::find(facSymbols.begin(), facSymbols.end(), position.symbol)
                == facSymbols.end()) {
            facSymbols.push_back(position.symbol);
        }
    }

    blocks.assign(
        static_cast<std::size_t>(blockCount),
        FACData()
    );
    std::vector<bool> haveBlock(
        static_cast<std::size_t>(blockCount), false
    );
    validatedBlockCount = 0;
    auto matchesSlot = [mode](const FACData& fac, int frame) {
        if (mode != Mode::E)
        {
            return frame == 0
                ? fac.identity == 0 || fac.identity == 3
                : fac.identity == frame;
        }

        if (frame == 0) {
            return (fac.identity == 0 || fac.identity == 3) && !fac.toggle;
        }

        if (frame == 3) {
            return fac.identity == 2 && fac.toggle;
        }

        return fac.identity == 1
            && fac.toggle == (frame == 1);
    };
    for (int frame = 0; frame < blockCount; ++frame)
    {
        const auto previous = std::find_if(
            previousBlocks.begin(), previousBlocks.end(),
            [=](const FACData& fac) { return matchesSlot(fac, frame); }
        );
        if (previous != previousBlocks.end())
        {
            blocks[static_cast<std::size_t>(frame)] = *previous;
            haveBlock[static_cast<std::size_t>(frame)] = true;
        }
    }

    // The frames share one window of pilot estimates, prepared on first use.
    ChannelEstimationContext estimation;
    int estimationState = 0;
    const auto estimationReady = [&]() {
        if (estimationState == 0)
        {
            estimationState = estimation.prepare(
                mode, occupancy, 0, integerCarrierOffset, framedSpectra) ? 1 : -1;
        }

        return estimationState > 0;
    };
    for (int frame = 0;
        frame < blockCount;
        ++frame)
    {
        std::vector<EqualizedSymbol> equalized(
            static_cast<std::size_t>(symbolsPerFrame)
        );
        if (mode == Mode::E)
        {
            // The FAC decoder reads only the FAC symbols.
            for (int facSymbol : facSymbols)
            {
                if (!ChannelEstimator::estimate(
                        mode, occupancy, facSymbol, 1,
                        integerCarrierOffset,
                        framedSpectra[static_cast<std::size_t>(frame)][static_cast<std::size_t>(facSymbol)],
                        equalized[static_cast<std::size_t>(facSymbol)])) {
                    return false;
                }
            }
        }
        else
        {
            bool estimated = estimationReady()
                && estimation.estimateFrameWiener(frame, equalized);
            if (!estimated)
            {
                for (int facSymbol : facSymbols)
                {
                    if (!ChannelEstimator::estimate(
                            mode, occupancy, facSymbol,
                            frame % parameters.framesPerSuperframe,
                            integerCarrierOffset,
                            framedSpectra[static_cast<std::size_t>(frame)][static_cast<std::size_t>(facSymbol)],
                            equalized[static_cast<std::size_t>(facSymbol)])) {
                        return false;
                    }
                }
            }
        }

        FACData fac;
        if (!FACDecoder::decode(mode, equalized, fac, nullptr, mode == Mode::E))
        {
            // DRM+ selective channels can reject the single-symbol linear
            // FAC estimate even while the MSC's pilot-context estimate works.
            // Retry only rejected blocks, using the known superframe phase and
            // the same bounded temporal/DFT estimator used by the MSC path.
            if (mode != Mode::E || !estimationReady()
                || !estimation.estimateFrame(frame, equalized, 3)
                || !FACDecoder::decode(mode, equalized, fac, nullptr, true)) {
                continue;
            }
        }

        if (fac.occupancy != occupancy || !matchesSlot(fac, frame)) {
            continue;
        }

        const FACData& reference = blocks.front();
        if (frame > 0 && haveBlock.front()
            && (fac.modeE != reference.modeE
                || fac.mscMode != reference.mscMode
                || fac.sdcMode != reference.sdcMode
                || fac.legacyModeETail != reference.legacyModeETail
                || fac.shortInterleaving != reference.shortInterleaving)) {
            continue;
        }

        blocks[static_cast<std::size_t>(frame)] = std::move(fac);
        blockValid[static_cast<std::size_t>(frame)] = true;
        haveBlock[static_cast<std::size_t>(frame)] = true;
        ++validatedBlockCount;
    }

    // Initial acquisition still requires three consecutive coherent blocks.
    // Tracking has an established epoch: two fresh coherent observations may
    // bridge erased slots. The receiver's per-frame loss hysteresis decides
    // whether lock survives; cached slots never become fresh signalling.
    return validatedBlockCount >= 2
        && std::all_of(haveBlock.begin(), haveBlock.end(), [](bool have) {
            return have;
        });
}

bool passbandLimited(const FACSearchOptions& options)
{
    return options.passbandLowHz < options.passbandHighHz;
}

// The orientations to try, the most likely first.
std::vector<bool> orientations(const FACSearchOptions& options)
{
    if (options.spectralInversion >= 0) {
        return {options.spectralInversion != 0};
    }

    const bool first = options.haveHint && options.hintSpectralInversion;
    return {first, !first};
}

// The options with one orientation resolved.
FACSearchOptions oriented(const FACSearchOptions& options, bool inversion)
{
    FACSearchOptions result = options;
    result.spectralInversion = inversion ? 1 : 0;
    return result;
}

std::vector<int> candidateOccupancies(Mode mode, const FACSearchOptions& options)
{
    std::vector<int> result;
    for (int occupancy : occupanciesForMode(mode))
    {
        if (options.forcedOccupancy >= 0 && occupancy != options.forcedOccupancy) {
            continue;
        }

        if (passbandLimited(options) && !Parameters::spectrumWithinBand(mode, occupancy,
                options.passbandLowHz, options.passbandHighHz,
                options.configuredSearchRangeHz, options.spectralInversion == 1)) {
            continue;
        }

        result.push_back(occupancy);
    }

    return result;
}

// FAC hypothesis search for one robustness mode and orientation: the ranked
// candidates, or (exhaustive) the per-symbol search of every hypothesis. With
// a preferred total carrier offset, integer hypotheses are tried nearest to
// it first.
FACAcquisitionResult searchMode(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    const AcquisitionCandidate& coarse,
    const std::vector<int>& occupancies,
    const FACSearchOptions& options,
    const double* preferredCarrierOffsetHz,
    bool exhaustive
);

bool cancelled(const FACSearchOptions& options)
{
    return options.cancel && options.cancel->load();
}

} // namespace

FACAcquisitionResult FACAcquisition::search(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    int configuredSearchRangeHz,
    bool spectralInversion,
    int forcedMode,
    int forcedOccupancy
)
{
    FACSearchOptions options;
    options.configuredSearchRangeHz = configuredSearchRangeHz;
    options.spectralInversion = spectralInversion ? 1 : 0;
    options.forcedMode = forcedMode;
    options.forcedOccupancy = forcedOccupancy;
    return search(samples, sampleCount, sampleRate, options);
}

FACAcquisitionResult FACAcquisition::search(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    const FACSearchOptions& options
)
{
    FACAcquisitionResult result;
    const int forcedMode = options.forcedMode;
    if (forcedMode < -1 || forcedMode > static_cast<int>(Mode::E)
        || options.forcedOccupancy < -1 || options.forcedOccupancy > 5) {
        return result;
    }

    const std::size_t coarseSampleCount = sampleRate > 0
        ? std::min(
            sampleCount,
            static_cast<std::size_t>(CoarseAcquisitionSeconds)
                * static_cast<std::size_t>(sampleRate)
        ) : sampleCount;

    const std::vector<bool> tried = orientations(options);

    // Reacquisition: the lost lock's mode, occupancy and orientation first.
    if (options.haveHint && options.hintMode >= 0 && options.hintMode <= static_cast<int>(Mode::E)
        && (forcedMode < 0 || forcedMode == options.hintMode)
        && (options.spectralInversion < 0 || (options.spectralInversion == 1) == options.hintSpectralInversion))
    {
        const Mode hintMode = static_cast<Mode>(options.hintMode);
        const FACSearchOptions hinted = oriented(options, options.hintSpectralInversion);
        std::vector<int> occupancies = candidateOccupancies(hintMode, hinted);
        if (std::find(occupancies.begin(), occupancies.end(), options.hintOccupancy)
                != occupancies.end())
        {
            const AcquisitionCandidate coarse = Acquisition::evaluateMode(
                samples, coarseSampleCount, sampleRate, hintMode);
            if (coarse.valid)
            {
                // Ranked candidates only: the hint is for speed, and the full
                // search below covers the hinted occupancy exhaustively too.
                FACAcquisitionResult found = searchMode(samples, sampleCount, sampleRate,
                    coarse, {options.hintOccupancy}, hinted, &options.hintCarrierOffsetHz, false);
                if (found.valid) {
                    return found;
                }
            }
        }
    }

    if (cancelled(options)) {
        return result;
    }

    // Choose the mode by cyclic-prefix correlation, which mirroring does not
    // change, among modes the input passband can carry in some orientation:
    // a narrow capture cannot hold mode E, for example.
    const auto usable = [&](Mode mode) {
        return std::any_of(tried.begin(), tried.end(), [&](bool inversion) {
            return !candidateOccupancies(mode, oriented(options, inversion)).empty();
        });
    };
    for (const ModeParameters& parameters : Parameters::modes())
    {
        if (forcedMode >= 0 && static_cast<int>(parameters.mode) != forcedMode) {
            continue;
        }

        if (!usable(parameters.mode)) {
            continue;
        }

        const AcquisitionCandidate candidate = Acquisition::evaluateMode(
            samples, coarseSampleCount, sampleRate, parameters.mode);
        if (forcedMode >= 0 || (candidate.valid && (!result.coarse.valid
                || candidate.cyclicPrefixCorrelation > result.coarse.cyclicPrefixCorrelation))) {
            result.coarse = candidate;
        }
    }

    if (!result.coarse.valid) {
        return result;
    }

    // Each orientation's ranked candidates, then the exhaustive search: a
    // mirrored spectrum decodes no coherent FAC, so trying it costs a search
    // but cannot lock on the normal one, and a normal signal locks on its
    // ranked candidates as when only it was tried.
    for (const bool exhaustive : {false, true})
    {
        for (const bool inversion : tried)
        {
            if (cancelled(options)) {
                return result;
            }

            const FACSearchOptions attempt = oriented(options, inversion);
            FACAcquisitionResult found = searchMode(samples, sampleCount, sampleRate, result.coarse,
                candidateOccupancies(result.coarse.mode, attempt), attempt, nullptr, exhaustive);
            if (found.valid) {
                return found;
            }
        }
    }

    return result;
}

namespace
{

FACAcquisitionResult searchMode(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    const AcquisitionCandidate& coarse,
    const std::vector<int>& occupancies,
    const FACSearchOptions& options,
    const double* preferredCarrierOffsetHz,
    bool exhaustive
)
{
    constexpr int RequiredConsecutiveBlocks = FACAcquisition::RequiredConsecutiveBlocks;
    const bool spectralInversion = options.spectralInversion == 1;
    FACAcquisitionResult result;
    result.coarse = coarse;
    if (occupancies.empty()) {
        return result;
    }

    result.spectralInversion = spectralInversion;
    if (spectralInversion) {
        result.coarse.fractionalCarrierOffsetHz = -result.coarse.fractionalCarrierOffsetHz;
    }

    const Mode mode = result.coarse.mode;
    const ModeParameters& parameters = Parameters::forMode(mode);
    const int symbolsPerFrame = parameters.symbolsPerFrame;
    const int symbolSamples = parameters.symbolSamples(sampleRate);
    if (symbolSamples <= 0) {
        return result;
    }

    // Every candidate frame start needs its required blocks; one frame more,
    // where the input has it, lets the block after them be checked too.
    const int requiredSymbolCount = (RequiredConsecutiveBlocks + 1) * symbolsPerFrame;
    OFDMDemodulator demodulator(mode, sampleRate);
    std::vector<std::vector<std::complex<float>>> spectra(
        static_cast<std::size_t>(requiredSymbolCount + symbolsPerFrame)
    );
    int demodulatedSymbolCount = 0;
    for (int symbol = 0; symbol < requiredSymbolCount + symbolsPerFrame; ++symbol)
    {
        const std::size_t prefixStart = static_cast<std::size_t>(
            result.coarse.symbolStartOffsetSamples + symbol * symbolSamples
        );
        if (!demodulator.demodulate(
                samples,
                sampleCount,
                prefixStart,
                result.coarse.fractionalCarrierOffsetHz,
                spectra[static_cast<std::size_t>(symbol)],
                spectralInversion))
        {
            if (symbol < requiredSymbolCount) {
                return result;
            }

            break;
        }

        demodulatedSymbolCount = symbol + 1;
    }

    spectra.resize(static_cast<std::size_t>(demodulatedSymbolCount));

    std::vector<int> facSymbols;
    for (const CellPosition& position : CellMap::facCells(mode))
    {
        if (std::find(facSymbols.begin(), facSymbols.end(), position.symbol) == facSymbols.end()) {
            facSymbols.push_back(position.symbol);
        }
    }

    // Three coherent blocks can still straddle a discontinuity (a dropped
    // frame, say) that the next block reveals. When the input holds the next
    // block and it decodes, it must continue the sequence.
    const auto nextBlockContradicts = [&](int integerOffset, int occupancy, int frameStart, const FACData& last) {
        const int frame = RequiredConsecutiveBlocks;
        if (frameStart + (frame + 1) * symbolsPerFrame > demodulatedSymbolCount) {
            return false;
        }

        std::vector<EqualizedSymbol> equalized(static_cast<std::size_t>(symbolsPerFrame));
        for (int symbol = 0; symbol < symbolsPerFrame; ++symbol)
        {
            if (std::find(facSymbols.begin(), facSymbols.end(), symbol) == facSymbols.end()) {
                continue;
            }

            if (!ChannelEstimator::estimate(mode, occupancy, symbol,
                    mode == Mode::E ? 1 : frame % parameters.framesPerSuperframe, integerOffset,
                    spectra[static_cast<std::size_t>(frameStart + frame * symbolsPerFrame + symbol)],
                    equalized[static_cast<std::size_t>(symbol)])) {
                return false;
            }
        }

        FACData next;
        return FACDecoder::decode(mode, equalized, next, nullptr, mode == Mode::E)
            && next.occupancy == occupancy && !coherentTransition(mode, last, next);
    };

    std::vector<int> integerHypotheses = Acquisition::integerCarrierHypotheses(
        mode, result.coarse.fractionalCarrierOffsetHz, options.configuredSearchRangeHz
    );
    if (preferredCarrierOffsetHz)
    {
        const double spacing = parameters.carrierSpacingHz();
        const double preferred = (*preferredCarrierOffsetHz
            - result.coarse.fractionalCarrierOffsetHz) / spacing;
        std::stable_sort(integerHypotheses.begin(), integerHypotheses.end(),
            [preferred](int left, int right) {
                return std::abs(left - preferred) < std::abs(right - preferred);
            });
    }

    // One stage per call: the ranked candidates, or the exhaustive search.
    if (!exhaustive)
    {
        // First rank integer offset, occupancy and frame start by gain-pilot
        // coherence and decode FAC for the best few only, equalizing with the
        // time/frequency estimator over the candidate's frames. Each pilot is
        // correlated with the same carrier one pilot period later: on the
        // right hypothesis the products share a phase however frequency
        // selective the channel, and on a wrong one they do not. The
        // exhaustive per-symbol search below remains the fallback.
        const int period = mode == Mode::A ? 5 : mode == Mode::B ? 3 : mode == Mode::C ? 2
            : mode == Mode::D ? 3 : 4;
        const int bins = static_cast<int>(spectra.front().size());
        const int pairedSymbols = demodulatedSymbolCount - period;
        // Bin products of each symbol with the one a pilot period later.
        std::vector<std::vector<std::complex<float>>> products(static_cast<std::size_t>(pairedSymbols));
        for (int j = 0; j < pairedSymbols; ++j)
        {
            const auto& first = spectra[static_cast<std::size_t>(j)];
            const auto& second = spectra[static_cast<std::size_t>(j + period)];
            auto& product = products[static_cast<std::size_t>(j)];
            product.resize(static_cast<std::size_t>(bins));
            for (int bin = 0; bin < bins; ++bin)
            {
                product[static_cast<std::size_t>(bin)] = first[static_cast<std::size_t>(bin)]
                    * std::conj(second[static_cast<std::size_t>(bin)]);
            }
        }

        struct Candidate { double coherence; int integerOffset; int occupancy; int frameStart; };
        std::vector<Candidate> candidates;
        const int framePhase = mode == Mode::E ? 1 : 0;
        const auto unitPilot = [](const PilotCell& cell) {
            const double phase = 2.0 * 3.14159265358979323846 * cell.phase1024 / 1024.0;
            return std::complex<float>(static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
        };
        for (int occupancy : occupancies)
        {
            // For each symbol of the frame: gain pilot carriers present in it
            // and a pilot period later, with conj(P(s)) P(s + period).
            std::vector<std::vector<std::pair<int, std::complex<float>>>> expected(
                static_cast<std::size_t>(symbolsPerFrame));
            for (int symbol = 0; symbol < symbolsPerFrame; ++symbol)
            {
                const auto& now = CellMap::referenceCells(mode, occupancy, symbol, framePhase);
                const auto& later = CellMap::referenceCells(
                    mode, occupancy, (symbol + period) % symbolsPerFrame, framePhase);
                for (const PilotCell& cell : now)
                {
                    if (cell.kind != ReferenceKind::Gain) {
                        continue;
                    }

                    const auto match = std::find_if(later.begin(), later.end(), [&cell](const PilotCell& other) {
                        return other.carrier == cell.carrier && other.kind == ReferenceKind::Gain;
                    });
                    if (match == later.end()) {
                        continue;
                    }

                    expected[static_cast<std::size_t>(symbol)].push_back(
                        {cell.carrier, std::conj(unitPilot(cell)) * unitPilot(*match)});
                }
            }

            for (int integerOffset : integerHypotheses)
            {
                for (int frameStart = 0; frameStart < symbolsPerFrame; ++frameStart)
                {
                    std::complex<double> sum(0.0, 0.0);
                    double magnitude = 0.0;
                    for (int j = 0; j < pairedSymbols; ++j)
                    {
                        const int symbol = ((j - frameStart) % symbolsPerFrame + symbolsPerFrame) % symbolsPerFrame;
                        const auto& product = products[static_cast<std::size_t>(j)];
                        for (const auto& pilot : expected[static_cast<std::size_t>(symbol)])
                        {
                            int bin = (pilot.first + integerOffset) % bins;
                            if (bin < 0) {
                                bin += bins;
                            }

                            // Y(s) conj(Y(s+p)) = |H|^2 P(s) conj(P(s+p)) on the right hypothesis.
                            const std::complex<float> value = product[static_cast<std::size_t>(bin)];
                            sum += std::complex<double>(value * pilot.second);
                            magnitude += std::abs(value);
                        }
                    }

                    if (magnitude > 1.0e-20) {
                        candidates.push_back({std::abs(sum) / magnitude, integerOffset, occupancy, frameStart});
                    }
                }
            }
        }

        // Stable, so that equal coherences keep the hypothesis order (nearest
        // the hinted offset first) on every platform.
        std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
            return left.coherence > right.coherence;
        });
        constexpr std::size_t RankedCandidates = 6;
        // The time/frequency context needs the candidate's frames (A-D only);
        // candidates sharing a frame start share the copy.
        std::map<int, std::vector<std::vector<std::vector<std::complex<float>>>>> framedByStart;
        for (std::size_t rank = 0; rank < std::min(RankedCandidates, candidates.size()); ++rank)
        {
            if (cancelled(options)) {
                return result;
            }

            const Candidate& candidate = candidates[rank];
            auto& framed = framedByStart[candidate.frameStart];
            if (mode != Mode::E && framed.empty())
            {
                framed.assign(static_cast<std::size_t>(RequiredConsecutiveBlocks),
                    std::vector<std::vector<std::complex<float>>>(static_cast<std::size_t>(symbolsPerFrame)));
                for (int frame = 0; frame < RequiredConsecutiveBlocks; ++frame)
                {
                    for (int symbol = 0; symbol < symbolsPerFrame; ++symbol)
                    {
                        framed[static_cast<std::size_t>(frame)][static_cast<std::size_t>(symbol)] =
                            spectra[static_cast<std::size_t>(candidate.frameStart + frame * symbolsPerFrame + symbol)];
                    }
                }
            }

            ChannelEstimationContext estimation;
            const bool robust = mode != Mode::E && estimation.prepare(
                mode, candidate.occupancy, 0, candidate.integerOffset, framed);
            std::vector<FACData> blocks;
            bool candidateValid = true;
            for (int frame = 0; frame < RequiredConsecutiveBlocks && candidateValid; ++frame)
            {
                std::vector<EqualizedSymbol> equalized(static_cast<std::size_t>(symbolsPerFrame));
                const bool estimated = robust && estimation.estimateFrameWiener(frame, equalized);
                for (std::size_t index = 0; !estimated && index < facSymbols.size(); ++index)
                {
                    const int symbol = facSymbols[index];
                    if (!ChannelEstimator::estimate(mode, candidate.occupancy, symbol,
                            mode == Mode::E ? 1 : frame % parameters.framesPerSuperframe,
                            candidate.integerOffset,
                            spectra[static_cast<std::size_t>(candidate.frameStart + frame * symbolsPerFrame + symbol)],
                            equalized[static_cast<std::size_t>(symbol)]))
                    {
                        candidateValid = false;
                        break;
                    }
                }

                FACData fac;
                const bool decoded = candidateValid
                    && FACDecoder::decode(mode, equalized, fac, nullptr, mode == Mode::E);
                if (decoded && fac.occupancy == candidate.occupancy
                    && (blocks.empty() || coherentTransition(mode, blocks.back(), fac))) {
                    blocks.push_back(fac);
                } else {
                    candidateValid = false;
                }
            }

            if (candidateValid && blocks.size() == RequiredConsecutiveBlocks
                && !nextBlockContradicts(candidate.integerOffset, candidate.occupancy, candidate.frameStart, blocks.back()))
            {
                result.valid = true;
                result.occupancy = candidate.occupancy;
                result.integerCarrierOffset = candidate.integerOffset;
                result.frameStartOffsetSamples = static_cast<std::size_t>(
                    result.coarse.symbolStartOffsetSamples + candidate.frameStart * symbolSamples
                );
                result.validatedFACBlockCount = RequiredConsecutiveBlocks;
                result.facBlockValid.assign(RequiredConsecutiveBlocks, true);
                result.facBlocks = std::move(blocks);
                return result;
            }
        }
    }

    if (!exhaustive) {
        return result;
    }

    for (int integerOffset : integerHypotheses)
    {
        if (cancelled(options)) {
            return result;
        }

        for (int occupancy : occupancies)
        {
            for (int frameStart = 0; frameStart < symbolsPerFrame; ++frameStart)
            {
                std::vector<FACData> blocks;
                bool candidateValid = true;
                for (int frame = 0; frame < RequiredConsecutiveBlocks && candidateValid; ++frame)
                {
                    std::vector<EqualizedSymbol> equalized(
                        static_cast<std::size_t>(symbolsPerFrame)
                    );
                    if (mode == Mode::E)
                    {
                        for (int frameSymbol : facSymbols)
                        {
                            const int observedSymbol = frameStart + frame * symbolsPerFrame + frameSymbol;
                            if (!ChannelEstimator::estimate(
                                    mode, occupancy, frameSymbol, 1, integerOffset,
                                    spectra[static_cast<std::size_t>(observedSymbol)],
                                    equalized[static_cast<std::size_t>(frameSymbol)]))
                            {
                                candidateValid = false;
                                break;
                            }
                        }
                    }
                    else
                    {
                        for (int facSymbol : facSymbols)
                        {
                            const int observedSymbol = frameStart + frame * symbolsPerFrame + facSymbol;
                            if (!ChannelEstimator::estimate(
                                    mode,
                                    occupancy,
                                    facSymbol,
                                    frame % parameters.framesPerSuperframe,
                                    integerOffset,
                                    spectra[static_cast<std::size_t>(observedSymbol)],
                                    equalized[static_cast<std::size_t>(facSymbol)]))
                            {
                                candidateValid = false;
                                break;
                            }
                        }
                    }

                    FACData fac;
                    const bool decoded = candidateValid
                        && FACDecoder::decode(
                            mode, equalized, fac, nullptr, mode == Mode::E
                        );
                    if (decoded && fac.occupancy == occupancy)
                    {
                        if (!blocks.empty() && !coherentTransition(mode, blocks.back(), fac)) {
                            candidateValid = false;
                        } else {
                            blocks.push_back(fac);
                        }
                    }
                    else
                    {
                        candidateValid = false;
                    }
                }

                if (candidateValid && blocks.size() == RequiredConsecutiveBlocks
                    && !nextBlockContradicts(integerOffset, occupancy, frameStart, blocks.back()))
                {
                    result.valid = true;
                    result.occupancy = occupancy;
                    result.integerCarrierOffset = integerOffset;
                    result.frameStartOffsetSamples = static_cast<std::size_t>(
                        result.coarse.symbolStartOffsetSamples + frameStart * symbolSamples
                    );
                    result.validatedFACBlockCount = RequiredConsecutiveBlocks;
                    result.facBlockValid.assign(RequiredConsecutiveBlocks, true);
                    result.facBlocks = std::move(blocks);
                    return result;
                }
            }
        }
    }

    return result;
}

} // namespace

FACAcquisitionResult FACAcquisition::searchWindow(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    int configuredSearchRangeHz,
    bool spectralInversion,
    int maximumStartOffsetSeconds,
    int forcedMode,
    int forcedOccupancy
)
{
    FACSearchOptions options;
    options.configuredSearchRangeHz = configuredSearchRangeHz;
    options.spectralInversion = spectralInversion ? 1 : 0;
    options.forcedMode = forcedMode;
    options.forcedOccupancy = forcedOccupancy;
    return searchWindow(samples, sampleCount, sampleRate, maximumStartOffsetSeconds, options);
}

FACAcquisitionResult FACAcquisition::searchWindow(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    int maximumStartOffsetSeconds,
    const FACSearchOptions& options
)
{
    FACAcquisitionResult result;
    if (!samples || sampleRate <= 0 || maximumStartOffsetSeconds < 0) {
        return result;
    }

    for (int seconds = 0; seconds <= maximumStartOffsetSeconds; ++seconds)
    {
        const std::size_t offset = static_cast<std::size_t>(seconds)
            * static_cast<std::size_t>(sampleRate);
        if (offset >= sampleCount || cancelled(options)) {
            break;
        }

        result = search(samples + offset, sampleCount - offset, sampleRate, options);
        if (result.valid)
        {
            result.coarse.symbolStartOffsetSamples += static_cast<int>(offset);
            result.frameStartOffsetSamples += offset;
            return result;
        }
    }

    return result;
}

FACAcquisitionResult FACAcquisition::track(
    const std::complex<float>* samples,
    std::size_t sampleCount,
    int sampleRate,
    const FACAcquisitionResult& previous,
    std::size_t expectedFrameStartOffsetSamples
)
{
    FACAcquisitionResult result;
    if (!samples || sampleRate <= 0 || !previous.valid
        || previous.facBlocks.empty()) {
        return result;
    }

    const Mode mode = previous.coarse.mode;
    const ModeParameters& parameters = Parameters::forMode(mode);
    const int symbolSamples = parameters.symbolSamples(sampleRate);
    if (symbolSamples <= 0 || expectedFrameStartOffsetSamples >= sampleCount) {
        return result;
    }

    // Measure timing over up to a second from a guard before the expected
    // frame, not from older history the caller keeps before it (A-D captures
    // hold about a second there; a steady mode E capture about 0.4 s).
    const int guardSamples = parameters.guardSamples(sampleRate);
    const std::size_t windowStart = expectedFrameStartOffsetSamples > static_cast<std::size_t>(guardSamples)
        ? expectedFrameStartOffsetSamples - static_cast<std::size_t>(guardSamples) : 0U;
    const std::size_t trackingSampleCount = std::min(
        sampleCount - windowStart,
        static_cast<std::size_t>(sampleRate)
    );
    // Track on the guard window of greatest correlation, not the first path
    // (acquisition aligns to that): per-superframe first-path estimates wander
    // with the noise on the profile's leading edge, and timing on the first
    // path cost superframes on the mode E sweeps and Kuwait 10:02 for a small
    // gain on simulated HF channels.
    result.coarse = Acquisition::evaluateMode(
        samples + windowStart, trackingSampleCount, sampleRate, mode, false
    );
    if (!result.coarse.valid) {
        return result;
    }

    result.spectralInversion = previous.spectralInversion;
    if (result.spectralInversion)
    {
        result.coarse.fractionalCarrierOffsetHz =
            -result.coarse.fractionalCarrierOffsetHz;
    }

    const int expectedPhase = static_cast<int>(
        (expectedFrameStartOffsetSamples - windowStart)
            % static_cast<std::size_t>(symbolSamples)
    );
    // With echoes that narrow the span of FFT-window starts holding every
    // significant path, place the window a margin before that span's end
    // (half the span when it is short) instead of at the guard-window
    // maximum, which can leave the window a few samples from the first path's
    // next symbol (BBC 3955 kHz with a 2.5 ms echo: 1 to 2 dB of MSC MER).
    // Move a tenth of the way to each superframe's measurement: jumping to it
    // cost Kuwait 10:02 and RFmondial frames. A span over 80% of the guard is
    // a single path, where the guard-window maximum is kept (moving earlier
    // there cost Kuwait 10:02 3%). Not mode E: no gain on its sweeps.
    double placement = previous.windowPlacementSamples;
    if (mode != Mode::E && result.coarse.windowSpanSamples > 0)
    {
        const int guard = parameters.guardSamples(sampleRate);
        const int margin = std::min(guard / 8, result.coarse.windowSpanSamples / 2);
        int target = 0;
        if (result.coarse.windowSpanSamples < WindowPlacementSpanFraction * guard)
        {
            target = result.coarse.windowEndOffsetSamples - margin
                - (guard - OFDMDemodulator::backoffSamplesFor(guard)) - result.coarse.symbolStartOffsetSamples;
            target = ((target % symbolSamples) + symbolSamples) % symbolSamples;
            if (target > symbolSamples / 2) {
                target -= symbolSamples;
            }
        }

        placement += WindowPlacementSmoothing * (target - placement);
        placement = std::clamp(placement, -0.5 * guard, 0.25 * guard);
    }

    result.windowPlacementSamples = placement;
    int timingCorrection = result.coarse.symbolStartOffsetSamples
        + static_cast<int>(std::lround(placement)) - expectedPhase;
    if (timingCorrection > symbolSamples / 2) {
        timingCorrection -= symbolSamples;
    }

    if (timingCorrection < -symbolSamples / 2) {
        timingCorrection += symbolSamples;
    }

    const std::int64_t correctedStart = static_cast<std::int64_t>(
        expectedFrameStartOffsetSamples
    ) + timingCorrection;
    // Callers retain a guard interval of history before the frame (A-D also
    // keep the preceding frame), so a correction earlier than that leaves a
    // deficit. One within the FFT-window backoff is absorbed by clamping (the
    // window still starts inside the guard); a larger one would misplace every
    // FFT window, so reject it.
    if (correctedStart < 0)
    {
        const std::int64_t tolerance = OFDMDemodulator::backoffSamplesFor(parameters.guardSamples(sampleRate));
        if (-correctedStart > tolerance)
        {
            result.coarse.valid = false;
            return result;
        }
    }

    result.frameStartOffsetSamples = correctedStart > 0
        ? static_cast<std::size_t>(correctedStart) : 0U;
    result.coarse.symbolStartOffsetSamples = static_cast<int>(
        result.frameStartOffsetSamples
    );

    const double carrierSpacing = parameters.carrierSpacingHz();
    const double previousCarrierOffset =
        previous.coarse.fractionalCarrierOffsetHz
        + previous.integerCarrierOffset * carrierSpacing;
    result.integerCarrierOffset = static_cast<int>(std::llround(
        (previousCarrierOffset - result.coarse.fractionalCarrierOffsetHz)
            / carrierSpacing
    ));
    result.occupancy = previous.occupancy;
    int validatedBlockCount = 0;
    if (!decodeTrackedFAC(
            samples, sampleCount, sampleRate, result.coarse,
            result.occupancy, result.integerCarrierOffset,
            result.spectralInversion, result.frameStartOffsetSamples,
            previous.facBlocks, result.facBlocks, result.facBlockValid, validatedBlockCount))
    {
        result.validatedFACBlockCount = validatedBlockCount;
        result.facBlocks.clear();
        return result;
    }

    const FACData& first = result.facBlocks.front();
    if ((first.identity != 0 && first.identity != 3)
        || (mode == Mode::E && first.toggle))
    {
        result.facBlocks.clear();
        return result;
    }

    result.validatedFACBlockCount = validatedBlockCount;
    result.valid = true;
    return result;
}

} // namespace modemdrm
