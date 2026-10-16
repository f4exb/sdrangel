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

#include "drmconvolutional.h"

#include "modemdr/viterbi.h"

#include <algorithm>
#include <array>
#include <limits>

namespace modemdrm
{

namespace
{

// ETSI ES 201 980 V4.3.1, clause 7.3.2. The rate-1/6 mother code uses
// generators 133, 171 and 145 (octal), repeated for output branches 3 to 5.
constexpr std::array<unsigned int, 6> Generators {{0133U, 0171U, 0145U, 0133U, 0171U, 0145U}};

std::uint8_t parity(unsigned int value)
{
    value ^= value >> 16;
    value ^= value >> 8;
    value ^= value >> 4;
    value &= 0xfU;
    return static_cast<std::uint8_t>((0x6996U >> value) & 1U);
}

bool transmitted(CodeRate rate, int branch, std::size_t inputIndex)
{
    switch (rate)
    {
    case CodeRate::R1_6:
        return true;
    case CodeRate::R1_4:
        return branch < 4;
    case CodeRate::R1_3:
        return branch < 3;
    case CodeRate::R2_5:
        if (branch < 2) {
            return true;
        }

        if (branch == 2) {
            return inputIndex % 2U == 0U;
        }

        return false;
    case CodeRate::R1_2:
        return branch < 2;
    case CodeRate::R4_7:
        if (branch == 0) {
            return true;
        }

        if (branch == 1) {
            return inputIndex % 4U == 0U || inputIndex % 4U == 2U;
        }

        if (branch == 2) {
            return inputIndex % 4U == 1U;
        }

        return false;
    case CodeRate::R3_5:
        if (branch == 0) {
            return true;
        }

        if (branch == 1) {
            return inputIndex % 3U != 1U;
        }

        return false;
    case CodeRate::R2_3:
        if (branch == 0) {
            return true;
        }

        if (branch == 1) {
            return inputIndex % 2U == 0U;
        }

        return false;
    case CodeRate::R3_4:
        if (branch == 0) {
            return true;
        }

        if (branch == 1) {
            return inputIndex % 3U == 0U;
        }

        return false;
    case CodeRate::R4_5:
        if (branch == 0) {
            return true;
        }

        if (branch == 1) {
            return inputIndex % 4U == 0U;
        }

        return false;
    case CodeRate::R7_8:
        if (branch == 0) {
            return true;
        }

        if (branch == 1) {
            return inputIndex % 7U == 0U;
        }

        return false;
    case CodeRate::R8_9:
        if (branch == 0) {
            return true;
        }

        if (branch == 1) {
            return inputIndex % 8U == 0U;
        }

        return false;
    }

    return false;
}

bool transmittedTail(int residual, int branch, std::size_t step)
{
    if (residual < 0 || residual > 11 || step >= 6U) {
        return false;
    }

    if (branch < 2) {
        return true;
    }

    if (branch == 2)
    {
        switch (residual)
        {
        case 0:
            return false;
        case 1:
            return step == 0U;
        case 2:
            return step == 0U || step == 3U;
        case 3:
            return step == 0U || step == 1U || step == 3U;
        case 4:
            return step == 0U || step == 1U || step == 3U || step == 4U;
        default:
            return residual >= 6 || step < 5U;
        }
    }

    if (branch == 3)
    {
        switch (residual)
        {
        case 7:
            return step == 0U;
        case 8:
            return step == 0U || step == 3U;
        case 9:
            return step == 0U || step == 1U || step == 3U;
        case 10:
            return step == 0U || step == 1U || step == 3U || step == 5U;
        case 11: return step == 0U || step == 1U || step == 2U
                || step == 3U || step == 5U;
        default:
            return false;
        }
    }

    return false;
}

bool transmittedPunctured(
    CodeRate rate,
    int branch,
    std::size_t step,
    std::size_t informationBitCount,
    int tailResidual
)
{
    return step < informationBitCount
        ? transmitted(rate, branch, step)
        : transmittedTail(tailResidual, branch, step - informationBitCount);
}

bool transmittedUEP(
    CodeRate highProtectionRate,
    CodeRate lowProtectionRate,
    int branch,
    std::size_t step,
    std::size_t highProtectionBitCount,
    std::size_t lowProtectionBitCount,
    int tailResidual
)
{
    if (step < highProtectionBitCount) {
        return transmitted(highProtectionRate, branch, step);
    }

    const std::size_t informationBitCount = highProtectionBitCount
        + lowProtectionBitCount;
    if (step < informationBitCount)
    {
        return transmitted(
            lowProtectionRate, branch, step - highProtectionBitCount
        );
    }

    return tailResidual >= 0
        ? transmittedTail(tailResidual, branch, step - informationBitCount)
        : transmitted(lowProtectionRate, branch, step);
}

std::array<std::uint8_t, 6> outputBits(unsigned int state, unsigned int input)
{
    // The polynomial's most-significant coefficient multiplies the current
    // input bit. The six-bit state therefore holds a(i-1)..a(i-6), with
    // a(i-1) in bit 5 (figure 25).
    const unsigned int reg = state | (input << 6U);
    std::array<std::uint8_t, 6> output {};
    for (std::size_t branch = 0; branch < output.size(); ++branch) {
        output[branch] = parity(reg & Generators[branch]);
    }

    return output;
}

// Mother-code outputs for every state and input, one bit per branch, indexed
// by 2 * state + input.
const std::array<std::uint8_t, 128>& outputWords()
{
    static const std::array<std::uint8_t, 128> words = [] {
        std::array<std::uint8_t, 128> table {};
        for (unsigned int state = 0; state < 64U; ++state)
        {
            for (unsigned int input = 0; input < 2U; ++input)
            {
                const std::array<std::uint8_t, 6> output = outputBits(state, input);
                std::uint8_t word = 0U;
                for (unsigned int branch = 0; branch < 6U; ++branch) {
                    word = static_cast<std::uint8_t>(word | (output[branch] << branch));
                }

                table[2U * state + input] = word;
            }
        }

        return table;
    }();
    return words;
}

} // namespace

std::vector<std::uint8_t> ConvolutionalCode::encodeMother(
    const std::vector<std::uint8_t>& inputBits
)
{
    std::vector<std::uint8_t> result;
    result.reserve(inputBits.size() * Generators.size());
    unsigned int state = 0;
    for (std::uint8_t inputBit : inputBits)
    {
        const unsigned int input = inputBit & 1U;
        const std::array<std::uint8_t, 6> output = outputBits(state, input);
        result.insert(result.end(), output.begin(), output.end());
        state = (state >> 1U) | (input << 5U);
    }

    return result;
}

std::vector<std::uint8_t> ConvolutionalCode::encode(
    const std::vector<std::uint8_t>& informationBits,
    CodeRate rate
)
{
    std::vector<std::uint8_t> terminated = informationBits;
    terminated.insert(terminated.end(), TailBitCount, 0U);
    const std::vector<std::uint8_t> mother = encodeMother(terminated);
    std::vector<std::uint8_t> result;
    result.reserve(puncturedBitCount(informationBits.size(), rate));
    for (std::size_t inputIndex = 0; inputIndex < terminated.size(); ++inputIndex)
    {
        for (int branch = 0; branch < 6; ++branch)
        {
            if (transmitted(rate, branch, inputIndex)) {
                result.push_back(mother[inputIndex * 6U + static_cast<std::size_t>(branch)]);
            }
        }
    }

    return result;
}

std::size_t ConvolutionalCode::puncturedBitCount(std::size_t informationBitCount, CodeRate rate)
{
    const std::size_t inputCount = informationBitCount + TailBitCount;
    std::size_t count = 0;
    for (std::size_t inputIndex = 0; inputIndex < inputCount; ++inputIndex)
    {
        for (int branch = 0; branch < 6; ++branch)
        {
            if (transmitted(rate, branch, inputIndex)) {
                ++count;
            }
        }
    }

    return count;
}

std::vector<std::uint8_t> ConvolutionalCode::encodePunctured(
    const std::vector<std::uint8_t>& informationBits,
    CodeRate rate,
    int tailResidual
)
{
    if (tailResidual < 0 || tailResidual > 11) {
        return {};
    }

    std::vector<std::uint8_t> terminated = informationBits;
    terminated.insert(terminated.end(), TailBitCount, 0U);
    const std::vector<std::uint8_t> mother = encodeMother(terminated);
    std::vector<std::uint8_t> result;
    for (std::size_t step = 0; step < terminated.size(); ++step)
    {
        for (int branch = 0; branch < 6; ++branch)
        {
            if (transmittedPunctured(
                    rate, branch, step, informationBits.size(), tailResidual)) {
                result.push_back(mother[step * 6U + static_cast<std::size_t>(branch)]);
            }
        }
    }

    return result;
}

std::vector<std::uint8_t> ConvolutionalCode::encodeUEP(
    const std::vector<std::uint8_t>& highProtectionBits,
    CodeRate highProtectionRate,
    const std::vector<std::uint8_t>& lowProtectionBits,
    CodeRate lowProtectionRate,
    int tailResidual
)
{
    if (tailResidual < 0 || tailResidual > 11) {
        return {};
    }

    std::vector<std::uint8_t> terminated = highProtectionBits;
    terminated.insert(
        terminated.end(), lowProtectionBits.begin(), lowProtectionBits.end()
    );
    terminated.insert(terminated.end(), TailBitCount, 0U);
    const std::vector<std::uint8_t> mother = encodeMother(terminated);
    std::vector<std::uint8_t> result;
    for (std::size_t step = 0; step < terminated.size(); ++step)
    {
        for (int branch = 0; branch < 6; ++branch)
        {
            if (transmittedUEP(
                    highProtectionRate, lowProtectionRate, branch, step,
                    highProtectionBits.size(), lowProtectionBits.size(),
                    tailResidual)) {
                result.push_back(mother[step * 6U + static_cast<std::size_t>(branch)]);
            }
        }
    }

    return result;
}

namespace
{

bool decodeUEPImpl(
    const std::vector<float>& softBits,
    std::size_t highProtectionBitCount,
    CodeRate highProtectionRate,
    std::size_t lowProtectionBitCount,
    CodeRate lowProtectionRate,
    int tailResidual,
    std::vector<std::uint8_t>& informationBits
)
{
    const std::size_t informationBitCount = highProtectionBitCount
        + lowProtectionBitCount;
    const std::size_t stepCount = informationBitCount + ConvolutionalCode::TailBitCount;
    // The puncturing pattern depends only on the step: resolve it once per
    // step rather than for every state and branch.
    std::vector<std::uint8_t> sentMasks(stepCount);
    std::size_t expectedSoftBits = 0;
    for (std::size_t step = 0; step < stepCount; ++step)
    {
        std::uint8_t mask = 0U;
        for (int branch = 0; branch < 6; ++branch)
        {
            if (transmittedUEP(
                    highProtectionRate, lowProtectionRate, branch, step,
                    highProtectionBitCount, lowProtectionBitCount,
                    tailResidual))
            {
                mask = static_cast<std::uint8_t>(mask | (1U << branch));
                ++expectedSoftBits;
            }
        }

        sentMasks[step] = mask;
    }

    if (softBits.size() != expectedSoftBits) {
        return false;
    }

    // The mother codeword, with zeros (erasures) where bits were punctured, for the forward
    // pass shared with DAB
    std::vector<float> mother(stepCount * 6U, 0.0f);
    const float* soft = softBits.data();
    for (std::size_t step = 0; step < stepCount; ++step)
    {
        for (unsigned int branch = 0; branch < 6U; ++branch)
        {
            if (sentMasks[step] & (1U << branch)) {
                mother[step * 6U + branch] = *soft++;
            }
        }
    }

    static const modemdr::Viterbi viterbi(modemdr::Viterbi::drmGenerators());
    std::vector<std::uint64_t> decisions(stepCount);
    viterbi.forward(mother.data(), static_cast<int>(stepCount), decisions.data());
    informationBits.assign(informationBitCount, 0U);
    modemdr::Viterbi::traceback(decisions.data(), static_cast<int>(stepCount), static_cast<int>(informationBitCount),
        informationBits.data());
    return true;
}

bool decodeListImpl(
    const std::vector<float>& softBits,
    std::size_t informationBitCount,
    CodeRate rate,
    std::size_t maximumCandidates,
    std::vector<std::vector<std::uint8_t>>& candidates
)
{
    candidates.clear();
    if (maximumCandidates == 0U) {
        return false;
    }

    constexpr int StateCount = 1 << (ConvolutionalCode::ConstraintLength - 1);
    const std::size_t stepCount = informationBitCount + ConvolutionalCode::TailBitCount;
    std::vector<std::size_t> softOffsets(stepCount + 1U, 0U);
    std::vector<std::uint8_t> sentMasks(stepCount, 0U);
    for (std::size_t step = 0; step < stepCount; ++step)
    {
        softOffsets[step + 1U] = softOffsets[step];
        for (int branch = 0; branch < 6; ++branch)
        {
            if (transmittedPunctured(rate, branch, step, informationBitCount, 0))
            {
                ++softOffsets[step + 1U];
                sentMasks[step] = static_cast<std::uint8_t>(sentMasks[step] | (1U << branch));
            }
        }
    }

    const std::array<std::uint8_t, 2 * StateCount>& outputs = outputWords();
    if (softOffsets.back() != softBits.size()) {
        return false;
    }

    constexpr float negativeInfinity = -std::numeric_limits<float>::infinity();
    const std::size_t survivorCount = static_cast<std::size_t>(StateCount) * maximumCandidates;
    std::vector<float> current(survivorCount, negativeInfinity);
    std::vector<float> next(survivorCount, negativeInfinity);
    current[0] = 0.0f;
    // Every trace entry the traceback reads is written by this call, so the
    // buffers are reused across calls without clearing.
    thread_local std::vector<std::uint8_t> predecessorState;
    thread_local std::vector<std::uint16_t> predecessorRank;
    if (predecessorState.size() < stepCount * survivorCount)
    {
        predecessorState.resize(stepCount * survivorCount);
        predecessorRank.resize(stepCount * survivorCount);
    }

    for (std::size_t step = 0; step < stepCount; ++step)
    {
        std::fill(next.begin(), next.end(), negativeInfinity);
        const unsigned int mask = sentMasks[step];
        const float* soft = softBits.data() + softOffsets[step];
        for (int nextState = 0; nextState < StateCount; ++nextState)
        {
            const unsigned int input = static_cast<unsigned int>(nextState >> 5);
            // The two predecessors' lists, each in metric order from the
            // previous step. Adding one branch's values to every rank keeps
            // that order (rounding is monotone), so merging the two lists
            // keeps the best ranks in metric order, the lower predecessor
            // first on equal metrics.
            std::array<int, 2> states {};
            std::array<std::array<float, 6>, 2> values {};
            std::array<int, 2> sent {};
            for (int lowBit = 0; lowBit < 2; ++lowBit)
            {
                const int state = ((nextState & 31) << 1) | lowBit;
                states[static_cast<std::size_t>(lowBit)] = state;
                const unsigned int output = outputs[2U * static_cast<unsigned int>(state) + input];
                for (unsigned int branch = 0, softIndex = 0; branch < 6U; ++branch)
                {
                    if (mask & (1U << branch))
                    {
                        const float value = soft[softIndex++];
                        values[static_cast<std::size_t>(lowBit)][static_cast<std::size_t>(sent[static_cast<std::size_t>(lowBit)]++)] =
                            (output & (1U << branch)) == 0U ? value : -value;
                    }
                }
            }

            // The metric of a predecessor's rank, adding the branch values one
            // at a time; -infinity once its list is exhausted.
            const auto extend = [&](int lowBit, std::size_t rank) {
                if (rank >= maximumCandidates) {
                    return negativeInfinity;
                }

                const float previous = current[
                    static_cast<std::size_t>(states[static_cast<std::size_t>(lowBit)]) * maximumCandidates + rank];
                if (previous == negativeInfinity) {
                    return negativeInfinity;
                }

                float metric = previous;
                for (int i = 0; i < sent[static_cast<std::size_t>(lowBit)]; ++i) {
                    metric += values[static_cast<std::size_t>(lowBit)][static_cast<std::size_t>(i)];
                }

                return metric;
            };
            std::array<std::size_t, 2> ranks {0U, 0U};
            std::array<float, 2> heads {extend(0, 0U), extend(1, 0U)};
            for (std::size_t rank = 0; rank < maximumCandidates; ++rank)
            {
                if (heads[0] == negativeInfinity && heads[1] == negativeInfinity) {
                    break;
                }

                const int taken = heads[0] >= heads[1] ? 0 : 1;
                const std::size_t survivor =
                    static_cast<std::size_t>(nextState) * maximumCandidates + rank;
                next[survivor] = heads[static_cast<std::size_t>(taken)];
                const std::size_t trace = step * survivorCount + survivor;
                predecessorState[trace] = static_cast<std::uint8_t>(states[static_cast<std::size_t>(taken)]);
                predecessorRank[trace] = static_cast<std::uint16_t>(ranks[static_cast<std::size_t>(taken)]);
                heads[static_cast<std::size_t>(taken)] = extend(taken, ++ranks[static_cast<std::size_t>(taken)]);
            }
        }

        current.swap(next);
    }

    for (std::size_t finalRank = 0; finalRank < maximumCandidates; ++finalRank)
    {
        if (current[finalRank] == negativeInfinity) {
            break;
        }

        std::vector<std::uint8_t> decoded(stepCount);
        unsigned int state = 0U;
        std::size_t rank = finalRank;
        for (std::size_t step = stepCount; step-- > 0;)
        {
            decoded[step] = static_cast<std::uint8_t>(state >> 5U);
            const std::size_t survivor = static_cast<std::size_t>(state) * maximumCandidates + rank;
            const std::size_t trace = step * survivorCount + survivor;
            state = predecessorState[trace];
            rank = predecessorRank[trace];
        }

        decoded.resize(informationBitCount);
        candidates.push_back(std::move(decoded));
    }

    return !candidates.empty();
}

} // namespace

bool ViterbiDecoder::decode(
    const std::vector<float>& softBits,
    std::size_t informationBitCount,
    CodeRate rate,
    std::vector<std::uint8_t>& informationBits
)
{
    return decodeUEPImpl(
        softBits, 0U, rate, informationBitCount, rate, -1, informationBits
    );
}

bool ViterbiDecoder::decodeSDC(
    const std::vector<float>& softBits,
    std::size_t informationBitCount,
    CodeRate rate,
    std::vector<std::uint8_t>& informationBits
)
{
    if (rate != CodeRate::R1_2 && rate != CodeRate::R1_4) {
        return false;
    }

    return decodeUEPImpl(
        softBits, 0U, rate, informationBitCount, rate, 0, informationBits
    );
}

bool ViterbiDecoder::decodePunctured(
    const std::vector<float>& softBits,
    std::size_t informationBitCount,
    CodeRate rate,
    int tailResidual,
    std::vector<std::uint8_t>& informationBits
)
{
    if (tailResidual < 0 || tailResidual > 11) {
        return false;
    }

    return decodeUEPImpl(
        softBits, 0U, rate, informationBitCount, rate,
        tailResidual, informationBits
    );
}

bool ViterbiDecoder::decodeUEP(
    const std::vector<float>& softBits,
    std::size_t highProtectionBitCount,
    CodeRate highProtectionRate,
    std::size_t lowProtectionBitCount,
    CodeRate lowProtectionRate,
    int tailResidual,
    std::vector<std::uint8_t>& informationBits
)
{
    if (highProtectionBitCount == 0U || lowProtectionBitCount == 0U
        || tailResidual < 0 || tailResidual > 11) {
        return false;
    }

    return decodeUEPImpl(
        softBits, highProtectionBitCount, highProtectionRate,
        lowProtectionBitCount, lowProtectionRate,
        tailResidual, informationBits
    );
}

bool ViterbiDecoder::decodeSDCList(
    const std::vector<float>& softBits,
    std::size_t informationBitCount,
    CodeRate rate,
    std::size_t maximumCandidates,
    std::vector<std::vector<std::uint8_t>>& candidates
)
{
    if (rate != CodeRate::R1_2 && rate != CodeRate::R1_4) {
        return false;
    }

    return decodeListImpl(
        softBits, informationBitCount, rate, maximumCandidates, candidates
    );
}

} // namespace modemdrm
