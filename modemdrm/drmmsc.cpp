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

#include "drmmsc.h"

#include "drmcellmap.h"
#include "drmbitutils.h"
#include "drmconvolutional.h"
#include "drminterleaver.h"

#include <cmath>
#include <algorithm>
#include <limits>
#include <numeric>

namespace modemdrm
{

constexpr int AdditionalMLCIterations = 4;

bool MSCExtractor::extract(
    Mode mode,
    int occupancy,
    int frameInSuperframe,
    const std::vector<EqualizedSymbol>& symbols,
    MSCFrameCells& output,
    std::string* error
)
{
    const std::vector<CellPosition>& positions = CellMap::mscCells(
        mode, occupancy, frameInSuperframe
    );
    if (positions.empty())
    {
        if (error) {
            *error = "Invalid DRM MSC mode, occupancy or frame index";
        }

        return false;
    }

    output = MSCFrameCells();
    output.frameInSuperframe = frameInSuperframe;
    output.cells.reserve(positions.size());
    output.reliabilities.reserve(positions.size());
    for (const CellPosition& position : positions)
    {
        if (position.symbol < 0 || static_cast<std::size_t>(position.symbol) >= symbols.size())
        {
            if (error) {
                *error = "Missing equalized DRM MSC symbol";
            }

            output = MSCFrameCells();
            return false;
        }

        const EqualizedSymbol& symbol = symbols[static_cast<std::size_t>(position.symbol)];
        if (!symbol.contains(position.carrier))
        {
            if (error) {
                *error = "Missing equalized DRM MSC carrier";
            }

            output = MSCFrameCells();
            return false;
        }

        output.cells.push_back(symbol.cell(position.carrier));
        const std::size_t channelIndex = static_cast<std::size_t>(
            position.carrier - symbol.minimumCarrier
        );
        output.reliabilities.push_back(channelIndex < symbol.channel.size()
            ? std::abs(symbol.channel[channelIndex]) : 1.0f);
    }

    return true;
}

bool MSCSuperframeAssembler::assemble(
    Mode mode,
    int occupancy,
    const std::vector<MSCFrameCells>& frames,
    MSCSuperframeCells& output,
    std::string* error
)
{
    const SpectrumParameters* spectrum = Parameters::forSpectrum(mode, occupancy);
    const ModeParameters& parameters = Parameters::forMode(mode);
    if (!spectrum || static_cast<int>(frames.size()) != parameters.framesPerSuperframe)
    {
        if (error) {
            *error = "Incomplete DRM MSC superframe";
        }

        return false;
    }

    output = MSCSuperframeCells();
    std::size_t totalCells = 0;
    for (int frame = 0; frame < parameters.framesPerSuperframe; ++frame)
    {
        const MSCFrameCells& input = frames[static_cast<std::size_t>(frame)];
        const std::size_t expected = CellMap::mscCells(mode, occupancy, frame).size();
        if (input.frameInSuperframe != frame || input.cells.size() != expected
            || input.reliabilities.size() != expected)
        {
            if (error) {
                *error = "Invalid DRM MSC frame order or cell count";
            }

            output = MSCSuperframeCells();
            return false;
        }

        totalCells += expected;
    }

    output.cells.reserve(totalCells);
    output.reliabilities.reserve(totalCells);
    for (const MSCFrameCells& frame : frames)
    {
        output.cells.insert(output.cells.end(), frame.cells.begin(), frame.cells.end());
        output.reliabilities.insert(
            output.reliabilities.end(),
            frame.reliabilities.begin(),
            frame.reliabilities.end()
        );
    }

    return true;
}

bool MSCMultiplexFrameSplitter::capacity(Mode mode, int occupancy, MSCCapacity& result)
{
    // NSFA, NSFU and NMUX from ETSI ES 201 980 V4.3.1, tables 41-45.
    struct Entry { Mode mode; int occupancy; int available; int useful; int multiplex; };
    static const Entry entries[] = {
        {Mode::A, 0,  3778,  3777, 1259}, {Mode::A, 1,  4268,  4266, 1422},
        {Mode::A, 2,  7897,  7896, 2632}, {Mode::A, 3,  8877,  8877, 2959},
        {Mode::A, 4, 16394, 16392, 5464}, {Mode::A, 5, 18354, 18354, 6118},
        {Mode::B, 0,  2900,  2898,  966}, {Mode::B, 1,  3330,  3330, 1110},
        {Mode::B, 2,  6153,  6153, 2051}, {Mode::B, 3,  7013,  7011, 2337},
        {Mode::B, 4, 12747, 12747, 4249}, {Mode::B, 5, 14323, 14322, 4774},
        {Mode::C, 3,  5532,  5532, 1844}, {Mode::C, 5, 11603, 11601, 3867},
        {Mode::D, 3,  3679,  3678, 1226}, {Mode::D, 5,  7819,  7818, 2606},
        {Mode::E, 0, 29842, 29840, 7460}
    };
    for (const Entry& entry : entries)
    {
        if (entry.mode == mode && entry.occupancy == occupancy)
        {
            result.availableCellsPerSuperframe = entry.available;
            result.usefulCellsPerSuperframe = entry.useful;
            result.cellsPerMultiplexFrame = entry.multiplex;
            result.dummyCellsPerSuperframe = entry.available - entry.useful;
            return true;
        }
    }

    result = MSCCapacity();
    return false;
}

bool MSCMultiplexFrameSplitter::split(
    Mode mode,
    int occupancy,
    const MSCSuperframeCells& superframe,
    std::vector<MSCMultiplexFrameCells>& frames,
    std::string* error
)
{
    MSCCapacity values;
    const ModeParameters& parameters = Parameters::forMode(mode);
    if (!capacity(mode, occupancy, values)
        || superframe.cells.size() != static_cast<std::size_t>(values.availableCellsPerSuperframe)
        || superframe.reliabilities.size() != superframe.cells.size())
    {
        if (error) {
            *error = "Invalid DRM MSC transmission superframe size";
        }

        frames.clear();
        return false;
    }

    if (values.cellsPerMultiplexFrame * parameters.framesPerSuperframe
        != values.usefulCellsPerSuperframe)
    {
        if (error) {
            *error = "Inconsistent DRM MSC capacity table";
        }

        frames.clear();
        return false;
    }

    frames.assign(
        static_cast<std::size_t>(parameters.framesPerSuperframe),
        MSCMultiplexFrameCells()
    );
    for (int frame = 0; frame < parameters.framesPerSuperframe; ++frame)
    {
        MSCMultiplexFrameCells& output = frames[static_cast<std::size_t>(frame)];
        output.frameInSuperframe = frame;
        const std::size_t begin = static_cast<std::size_t>(
            frame * values.cellsPerMultiplexFrame
        );
        const std::size_t end = begin + static_cast<std::size_t>(values.cellsPerMultiplexFrame);
        output.cells.assign(
            superframe.cells.begin() + static_cast<std::ptrdiff_t>(begin),
            superframe.cells.begin() + static_cast<std::ptrdiff_t>(end)
        );
        output.reliabilities.assign(
            superframe.reliabilities.begin() + static_cast<std::ptrdiff_t>(begin),
            superframe.reliabilities.begin() + static_cast<std::ptrdiff_t>(end)
        );
    }

    return true;
}

bool MSCCellInterleaver::deinterleaveBlock(
    const MSCMultiplexFrameCells& input,
    MSCMultiplexFrameCells& output,
    std::string* error
)
{
    if (input.cells.empty() || input.reliabilities.size() != input.cells.size())
    {
        if (error) {
            *error = "Invalid DRM MSC multiplex frame";
        }

        output = MSCMultiplexFrameCells();
        return false;
    }

    const std::vector<std::size_t>& permutation = BitInterleaver::cachedPermutation(
        input.cells.size(), 5
    );
    if (permutation.size() != input.cells.size())
    {
        if (error) {
            *error = "Unable to construct DRM MSC cell permutation";
        }

        output = MSCMultiplexFrameCells();
        return false;
    }

    output = MSCMultiplexFrameCells();
    output.frameInSuperframe = input.frameInSuperframe;
    output.cells.resize(input.cells.size());
    output.reliabilities.resize(input.reliabilities.size());
    for (std::size_t i = 0; i < permutation.size(); ++i)
    {
        output.cells[permutation[i]] = input.cells[i];
        output.reliabilities[permutation[i]] = input.reliabilities[i];
    }

    return true;
}

MSCCellDeinterleaver::MSCCellDeinterleaver(Mode mode, bool longInterleaving) :
    m_mode(mode),
    m_longInterleaving(longInterleaving)
{
}

void MSCCellDeinterleaver::reset()
{
    m_inputFrameCount = 0;
    m_firstFramePhase = -1;
    m_cellCount = 0;
    m_pending.clear();
}

int MSCCellDeinterleaver::depth() const
{
    if (!m_longInterleaving) {
        return 1;
    }

    return m_mode == Mode::E ? 6 : 5;
}

std::size_t MSCCellDeinterleaver::bufferedFrameCount() const
{
    return m_pending.size();
}

bool MSCCellDeinterleaver::push(
    const MSCMultiplexFrameCells& input,
    MSCMultiplexFrameCells& output,
    bool& outputReady,
    std::string* error
)
{
    outputReady = false;
    const ModeParameters& parameters = Parameters::forMode(m_mode);
    if (input.frameInSuperframe < 0
        || input.frameInSuperframe >= parameters.framesPerSuperframe
        || input.cells.empty()
        || input.reliabilities.size() != input.cells.size())
    {
        if (error) {
            *error = "Invalid DRM MSC multiplex frame";
        }

        return false;
    }

    if (m_firstFramePhase < 0)
    {
        m_firstFramePhase = input.frameInSuperframe;
        m_cellCount = input.cells.size();
    }
    else
    {
        const int expectedPhase = static_cast<int>(
            (m_firstFramePhase + m_inputFrameCount)
            % static_cast<std::uint64_t>(parameters.framesPerSuperframe)
        );
        if (input.frameInSuperframe != expectedPhase || input.cells.size() != m_cellCount)
        {
            if (error) {
                *error = "Discontinuous DRM MSC multiplex-frame sequence";
            }

            return false;
        }
    }

    if (!m_longInterleaving)
    {
        if (!MSCCellInterleaver::deinterleaveBlock(input, output, error)) {
            return false;
        }

        ++m_inputFrameCount;
        outputReady = true;
        return true;
    }

    const int interleavingDepth = depth();
    const std::vector<std::size_t>& permutation = BitInterleaver::cachedPermutation(m_cellCount, 5);
    const std::int64_t transmittedFrame = static_cast<std::int64_t>(m_inputFrameCount);
    // Cell i belongs to the frame i mod D before this one: find or start
    // those D frames once rather than for every cell.
    std::vector<PendingFrame*> sources(static_cast<std::size_t>(interleavingDepth), nullptr);
    for (int delay = 0; delay < interleavingDepth; ++delay)
    {
        const std::int64_t sourceFrame = transmittedFrame - delay;
        if (sourceFrame < 0) {
            continue;
        }

        auto insertion = m_pending.try_emplace(sourceFrame);
        PendingFrame& pending = insertion.first->second;
        if (insertion.second)
        {
            pending.frame.frameInSuperframe = static_cast<int>(
                (m_firstFramePhase + sourceFrame) % parameters.framesPerSuperframe
            );
            pending.frame.cells.resize(m_cellCount);
            pending.frame.reliabilities.resize(m_cellCount);
            pending.received.assign(m_cellCount, false);
        }

        sources[static_cast<std::size_t>(delay)] = &pending;
    }

    for (std::size_t i = 0; i < m_cellCount; ++i)
    {
        PendingFrame* pending = sources[i % static_cast<std::size_t>(interleavingDepth)];
        if (!pending) {
            continue;
        }

        const std::size_t destination = permutation[i];
        if (!pending->received[destination])
        {
            pending->frame.cells[destination] = input.cells[i];
            pending->frame.reliabilities[destination] = input.reliabilities[i];
            pending->received[destination] = true;
            ++pending->filled;
        }
    }

    const std::int64_t completedFrame = transmittedFrame - (interleavingDepth - 1);
    if (completedFrame >= 0)
    {
        auto pending = m_pending.find(completedFrame);
        if (pending == m_pending.end() || pending->second.filled != m_cellCount)
        {
            if (error) {
                *error = "Incomplete DRM MSC long-interleaved frame";
            }

            return false;
        }

        output = std::move(pending->second.frame);
        m_pending.erase(pending);
        outputReady = true;
    }

    ++m_inputFrameCount;
    return true;
}

namespace
{

bool modeE4QAMRate(int protectionLevel, CodeRate& rate, int& numerator, int& denominator)
{
    switch (protectionLevel)
    {
    case 0:
        rate = CodeRate::R1_4;
        numerator = 1;
        denominator = 4;
        return true;
    case 1:
        rate = CodeRate::R1_3;
        numerator = 1;
        denominator = 3;
        return true;
    case 2:
        rate = CodeRate::R2_5;
        numerator = 2;
        denominator = 5;
        return true;
    case 3:
        rate = CodeRate::R1_2;
        numerator = 1;
        denominator = 2;
        return true;
    default:
        return false;
    }
}

struct MSCComponentProfile
{
    CodeRate rate = CodeRate::R1_6;
    int numerator = 0;
    int denominator = 0;
    int interleaverMultiplier = 0;
};

bool eepProfiles(
    Mode mode,
    int mscMode,
    int protectionLevel,
    std::vector<MSCComponentProfile>& profiles
)
{
    profiles.clear();
    if (mode == Mode::E && mscMode == 3)
    {
        CodeRate rate = CodeRate::R1_6;
        int numerator = 0;
        int denominator = 0;
        if (!modeE4QAMRate(protectionLevel, rate, numerator, denominator)) {
            return false;
        }

        profiles.push_back({rate, numerator, denominator, 21});
        return true;
    }

    if (mode != Mode::E)
    {
        // ETSI ES 201 980 V4.3.1 tables 30 and 32. Level zero is not bit
        // interleaved for 64-QAM; levels one and two use multipliers 13/21.
        if (mscMode == 3)
        {
            switch (protectionLevel)
            {
            case 0:
                profiles = {
                    {CodeRate::R1_3, 1, 3, 13},
                    {CodeRate::R2_3, 2, 3, 21}
                };
                return true;
            case 1:
                profiles = {
                    {CodeRate::R1_2, 1, 2, 13},
                    {CodeRate::R3_4, 3, 4, 21}
                };
                return true;
            default:
                return false;
            }
        }

        if (mscMode != 0) {
            return false;
        }

        switch (protectionLevel)
        {
        case 0:
            profiles = {
                {CodeRate::R1_4, 1, 4, 0},
                {CodeRate::R1_2, 1, 2, 13},
                {CodeRate::R3_4, 3, 4, 21}
            };
            return true;
        case 1:
            profiles = {
                {CodeRate::R1_3, 1, 3, 0},
                {CodeRate::R2_3, 2, 3, 13},
                {CodeRate::R4_5, 4, 5, 21}
            };
            return true;
        case 2:
            profiles = {
                {CodeRate::R1_2, 1, 2, 0},
                {CodeRate::R3_4, 3, 4, 13},
                {CodeRate::R7_8, 7, 8, 21}
            };
            return true;
        case 3:
            profiles = {
                {CodeRate::R2_3, 2, 3, 0},
                {CodeRate::R4_5, 4, 5, 13},
                {CodeRate::R8_9, 8, 9, 21}
            };
            return true;
        default:
            return false;
        }
    }

    if (mscMode != 0) {
        return false;
    }

    // ETSI ES 201 980 V4.3.1 table 31.
    switch (protectionLevel)
    {
    case 0:
        profiles = {
            {CodeRate::R1_6, 1, 6, 13},
            {CodeRate::R1_2, 1, 2, 21}
        };
        return true;
    case 1:
        profiles = {
            {CodeRate::R1_4, 1, 4, 13},
            {CodeRate::R4_7, 4, 7, 21}
        };
        return true;
    case 2:
        profiles = {
            {CodeRate::R1_3, 1, 3, 13},
            {CodeRate::R2_3, 2, 3, 21}
        };
        return true;
    case 3:
        profiles = {
            {CodeRate::R1_2, 1, 2, 13},
            {CodeRate::R3_4, 3, 4, 21}
        };
        return true;
    default:
        return false;
    }
}

std::vector<float> deinterleaveComponent(
    const std::vector<float>& bits,
    int multiplier
)
{
    return multiplier == 0 ? bits : BitInterleaver::deinterleave(bits, multiplier);
}

std::vector<std::uint8_t> interleaveComponent(
    const std::vector<std::uint8_t>& bits,
    int multiplier
)
{
    return multiplier == 0 ? bits : BitInterleaver::interleave(bits, multiplier);
}

struct MSCDecodeLayout
{
    int higherProtectedCells = 0;
    int lowerProtectedCells = 0;
    std::vector<MSCComponentProfile> higherProfiles;
    std::vector<MSCComponentProfile> lowerProfiles;
    std::vector<int> higherBitCounts;
    std::vector<int> lowerBitCounts;
    std::vector<int> tailResiduals;
};

bool makeDecodeLayout(
    Mode mode,
    int mscMode,
    int cellCount,
    const MSCProtectionParameters& protection,
    MSCDecodeLayout& layout
)
{
    layout = MSCDecodeLayout();
    if (cellCount <= 0
        || !eepProfiles(
            mode, mscMode, protection.protectionLevelB,
            layout.lowerProfiles)) {
        return false;
    }

    layout.lowerProtectedCells = cellCount;
    if (protection.partABytes > 0U)
    {
        if (protection.partABytes
                > std::numeric_limits<std::size_t>::max() / 8U) {
            return false;
        }

        if (!eepProfiles(
                mode, mscMode, protection.protectionLevelA,
                layout.higherProfiles)
            || layout.higherProfiles.size() != layout.lowerProfiles.size()) {
            return false;
        }

        int denominatorLCM = 1;
        for (const MSCComponentProfile& profile : layout.higherProfiles) {
            denominatorLCM = std::lcm(denominatorLCM, profile.denominator);
        }

        int bitsPerLCMCells = 0;
        for (const MSCComponentProfile& profile : layout.higherProfiles)
        {
            bitsPerLCMCells += 2 * profile.numerator
                * (denominatorLCM / profile.denominator);
        }

        if (bitsPerLCMCells <= 0) {
            return false;
        }

        const std::size_t requestedBits = 8U * protection.partABytes;
        const std::size_t groups = (requestedBits
            + static_cast<std::size_t>(bitsPerLCMCells) - 1U)
            / static_cast<std::size_t>(bitsPerLCMCells);
        if (groups > static_cast<std::size_t>(cellCount / denominatorLCM)) {
            return false;
        }

        layout.higherProtectedCells = static_cast<int>(groups)
            * denominatorLCM;
        layout.lowerProtectedCells = cellCount - layout.higherProtectedCells;
        if (layout.lowerProtectedCells < 20) {
            return false;
        }
    }
    else
    {
        layout.higherProfiles = layout.lowerProfiles;
    }

    const std::size_t componentCount = layout.lowerProfiles.size();
    layout.higherBitCounts.resize(componentCount, 0);
    layout.lowerBitCounts.resize(componentCount, 0);
    layout.tailResiduals.resize(componentCount, 0);
    const int lowerAvailable = 2 * layout.lowerProtectedCells - 12;
    if (lowerAvailable < 0) {
        return false;
    }

    for (std::size_t level = 0; level < componentCount; ++level)
    {
        const MSCComponentProfile& higher = layout.higherProfiles[level];
        const MSCComponentProfile& lower = layout.lowerProfiles[level];
        const int higherNumerator = 2 * layout.higherProtectedCells
            * higher.numerator;
        if (higherNumerator % higher.denominator != 0
            || higher.interleaverMultiplier != lower.interleaverMultiplier) {
            return false;
        }

        layout.higherBitCounts[level] = higherNumerator / higher.denominator;
        layout.lowerBitCounts[level] = lower.numerator
            * (lowerAvailable / lower.denominator);
        layout.tailResiduals[level] = lowerAvailable % lower.denominator;
    }

    return true;
}

int layoutInformationBitCount(const MSCDecodeLayout& layout)
{
    return std::accumulate(
        layout.higherBitCounts.begin(), layout.higherBitCounts.end(), 0
    ) + std::accumulate(
        layout.lowerBitCounts.begin(), layout.lowerBitCounts.end(), 0
    );
}

std::vector<std::uint8_t> bitsToBytes(
    const std::vector<std::uint8_t>& bits,
    std::size_t offset,
    std::size_t byteCount
)
{
    std::vector<std::uint8_t> bytes(byteCount, 0U);
    for (std::size_t byte = 0; byte < byteCount; ++byte)
    {
        for (int bit = 0; bit < 8; ++bit)
        {
            bytes[byte] = static_cast<std::uint8_t>(
                (bytes[byte] << 1U) | (bits[offset++] & 1U)
            );
        }
    }

    return bytes;
}

} // namespace

bool MSCChannelDecoder::supports(Mode mode, int mscMode, bool unequalProtection)
{
    (void) mode;
    (void) unequalProtection;
    return mscMode == 0 || mscMode == 3;
}

int MSCChannelDecoder::informationBitCount(
    Mode mode,
    int occupancy,
    int mscMode,
    int protectionLevel
)
{
    if (!supports(mode, mscMode, false)) {
        return -1;
    }

    MSCCapacity capacityValues;
    std::vector<MSCComponentProfile> profiles;
    if (!MSCMultiplexFrameSplitter::capacity(mode, occupancy, capacityValues)
        || !eepProfiles(mode, mscMode, protectionLevel, profiles)) {
        return -1;
    }

    const int available = 2 * capacityValues.cellsPerMultiplexFrame - 12;
    int informationBits = 0;
    for (const MSCComponentProfile& profile : profiles) {
        informationBits += profile.numerator * (available / profile.denominator);
    }

    return informationBits;
}

int MSCChannelDecoder::informationBitCount(
    Mode mode,
    int occupancy,
    int mscMode,
    const MSCProtectionParameters& protection
)
{
    if (protection.partABytes == 0U)
    {
        return informationBitCount(
            mode, occupancy, mscMode, protection.protectionLevelB
        );
    }

    if (!supports(mode, mscMode, true)) {
        return -1;
    }

    MSCCapacity capacityValues;
    MSCDecodeLayout layout;
    if (!MSCMultiplexFrameSplitter::capacity(mode, occupancy, capacityValues)
        || !makeDecodeLayout(
            mode, mscMode, capacityValues.cellsPerMultiplexFrame,
            protection, layout)) {
        return -1;
    }

    return layoutInformationBitCount(layout);
}

bool MSCChannelDecoder::decode(
    Mode mode,
    int occupancy,
    int mscMode,
    int protectionLevel,
    const MSCMultiplexFrameCells& input,
    MSCDecodedMultiplexFrame& output,
    std::string* error
)
{
    output = MSCDecodedMultiplexFrame();
    MSCCapacity capacityValues;
    std::vector<MSCComponentProfile> profiles;
    if (!supports(mode, mscMode, false)
        || !MSCMultiplexFrameSplitter::capacity(mode, occupancy, capacityValues)
        || !eepProfiles(mode, mscMode, protectionLevel, profiles))
    {
        if (error) {
            *error = "Unsupported DRM MSC constellation or protection profile";
        }

        return false;
    }

    if (input.cells.size()
            != static_cast<std::size_t>(capacityValues.cellsPerMultiplexFrame)
        || input.reliabilities.size() != input.cells.size())
    {
        if (error) {
            *error = "Invalid DRM MSC multiplex-frame cell count";
        }

        return false;
    }

    const int available = 2 * capacityValues.cellsPerMultiplexFrame - 12;
    if (mode == Mode::E && mscMode == 3)
    {
        const MSCComponentProfile& profile = profiles.front();
        std::vector<float> interleavedSoftBits;
        interleavedSoftBits.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            interleavedSoftBits.push_back(input.cells[i].real() * input.reliabilities[i]);
            interleavedSoftBits.push_back(input.cells[i].imag() * input.reliabilities[i]);
        }

        const std::vector<float> softBits = BitInterleaver::deinterleave(
            interleavedSoftBits, profile.interleaverMultiplier
        );
        const int informationBits = profile.numerator * (available / profile.denominator);
        const int tailResidual = available % profile.denominator;
        if (!ViterbiDecoder::decodePunctured(
                softBits, static_cast<std::size_t>(informationBits),
                profile.rate, tailResidual, output.bits))
        {
            if (error) {
                *error = "DRM 4-QAM MSC Viterbi decoding failed";
            }

            output = MSCDecodedMultiplexFrame();
            return false;
        }
    }
    else if (profiles.size() == 2U)
    {
        constexpr float AxisScale = 0.31622776601683794f; // 1/sqrt(10)
        auto axisMetric = [=](float received, int level0, int level1) {
            static constexpr int amplitudes[2][2] = {{3, -1}, {1, -3}};
            const float difference = received
                - AxisScale * static_cast<float>(amplitudes[level0][level1]);
            return std::abs(difference);
        };

        std::vector<float> level0Interleaved;
        level0Interleaved.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            for (float axis : {input.cells[i].real(), input.cells[i].imag()})
            {
                const float distance0 = std::min(
                    axisMetric(axis, 0, 0), axisMetric(axis, 0, 1)
                );
                const float distance1 = std::min(
                    axisMetric(axis, 1, 0), axisMetric(axis, 1, 1)
                );
                level0Interleaved.push_back(
                    (distance1 - distance0) * input.reliabilities[i]
                );
            }
        }

        const MSCComponentProfile& level0Profile = profiles[0];
        const int level0InformationBits = level0Profile.numerator
            * (available / level0Profile.denominator);
        const int level0TailResidual = available % level0Profile.denominator;
        std::vector<std::uint8_t> level0Bits;
        if (!ViterbiDecoder::decodePunctured(
                deinterleaveComponent(
                    level0Interleaved, level0Profile.interleaverMultiplier
                ),
                static_cast<std::size_t>(level0InformationBits),
                level0Profile.rate, level0TailResidual, level0Bits))
        {
            if (error) {
                *error = "DRM 16-QAM MSC level-0 decoding failed";
            }

            return false;
        }

        const std::vector<std::uint8_t> level0Mapped = interleaveComponent(
            ConvolutionalCode::encodePunctured(
                level0Bits, level0Profile.rate, level0TailResidual
            ),
            level0Profile.interleaverMultiplier
        );
        if (level0Mapped.size() != 2U * input.cells.size())
        {
            if (error) {
                *error = "Invalid DRM 16-QAM MSC level-0 size";
            }

            return false;
        }

        std::vector<float> level1Interleaved;
        level1Interleaved.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
            for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
            {
                const int level0 = level0Mapped[
                    2U * i + static_cast<std::size_t>(axisIndex)
                ];
                const float distance0 = axisMetric(axes[axisIndex], level0, 0);
                const float distance1 = axisMetric(axes[axisIndex], level0, 1);
                level1Interleaved.push_back(
                    (distance1 - distance0) * input.reliabilities[i]
                );
            }
        }

        const MSCComponentProfile& level1Profile = profiles[1];
        const int level1InformationBits = level1Profile.numerator
            * (available / level1Profile.denominator);
        const int level1TailResidual = available % level1Profile.denominator;
        std::vector<std::uint8_t> level1Bits;
        if (!ViterbiDecoder::decodePunctured(
                deinterleaveComponent(
                    level1Interleaved, level1Profile.interleaverMultiplier
                ),
                static_cast<std::size_t>(level1InformationBits),
                level1Profile.rate, level1TailResidual, level1Bits))
        {
            if (error) {
                *error = "DRM 16-QAM MSC level-1 decoding failed";
            }

            return false;
        }

        output.bits = std::move(level0Bits);
        output.bits.insert(output.bits.end(), level1Bits.begin(), level1Bits.end());
    }
    else if (profiles.size() == 3U)
    {
        constexpr float AxisScale = 0.15430334996209191f; // 1/sqrt(42)
        auto axisMetric = [=](float received, int level0, int level1, int level2) {
            static constexpr int amplitudes[2][2][2] = {
                {{7, -1}, {3, -5}},
                {{5, -3}, {1, -7}}
            };
            const float difference = received - AxisScale * static_cast<float>(
                amplitudes[level0][level1][level2]
            );
            return std::abs(difference);
        };
        auto decodeLevel = [&](
            int level,
            const std::vector<float>& interleaved,
            std::vector<std::uint8_t>& decoded,
            std::vector<std::uint8_t>& mapped
        ) {
            const MSCComponentProfile& profile = profiles[static_cast<std::size_t>(level)];
            const int informationBits = profile.numerator
                * (available / profile.denominator);
            const int tailResidual = available % profile.denominator;
            if (!ViterbiDecoder::decodePunctured(
                    deinterleaveComponent(interleaved, profile.interleaverMultiplier),
                    static_cast<std::size_t>(informationBits), profile.rate,
                    tailResidual, decoded)) {
                return false;
            }

            mapped = interleaveComponent(
                ConvolutionalCode::encodePunctured(
                    decoded, profile.rate, tailResidual
                ),
                profile.interleaverMultiplier
            );
            return mapped.size() == 2U * input.cells.size();
        };

        std::vector<float> level0Interleaved;
        level0Interleaved.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            for (float axis : {input.cells[i].real(), input.cells[i].imag()})
            {
                const float distance0 = std::min({
                    axisMetric(axis, 0, 0, 0), axisMetric(axis, 0, 0, 1),
                    axisMetric(axis, 0, 1, 0), axisMetric(axis, 0, 1, 1)
                });
                const float distance1 = std::min({
                    axisMetric(axis, 1, 0, 0), axisMetric(axis, 1, 0, 1),
                    axisMetric(axis, 1, 1, 0), axisMetric(axis, 1, 1, 1)
                });
                level0Interleaved.push_back(
                    (distance1 - distance0) * input.reliabilities[i]
                );
            }
        }

        std::vector<std::uint8_t> level0Bits;
        std::vector<std::uint8_t> level0Mapped;
        if (!decodeLevel(0, level0Interleaved, level0Bits, level0Mapped))
        {
            if (error) {
                *error = "DRM 64-QAM MSC level-0 decoding failed";
            }

            return false;
        }

        std::vector<float> level1Interleaved;
        level1Interleaved.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
            for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
            {
                const std::size_t bitIndex = 2U * i + static_cast<std::size_t>(axisIndex);
                const int level0 = level0Mapped[bitIndex];
                const float distance0 = std::min(
                    axisMetric(axes[axisIndex], level0, 0, 0),
                    axisMetric(axes[axisIndex], level0, 0, 1)
                );
                const float distance1 = std::min(
                    axisMetric(axes[axisIndex], level0, 1, 0),
                    axisMetric(axes[axisIndex], level0, 1, 1)
                );
                level1Interleaved.push_back(
                    (distance1 - distance0) * input.reliabilities[i]
                );
            }
        }

        std::vector<std::uint8_t> level1Bits;
        std::vector<std::uint8_t> level1Mapped;
        if (!decodeLevel(1, level1Interleaved, level1Bits, level1Mapped))
        {
            if (error) {
                *error = "DRM 64-QAM MSC level-1 decoding failed";
            }

            return false;
        }

        std::vector<float> level2Interleaved;
        level2Interleaved.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
            for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
            {
                const std::size_t bitIndex = 2U * i + static_cast<std::size_t>(axisIndex);
                const int level0 = level0Mapped[bitIndex];
                const int level1 = level1Mapped[bitIndex];
                const float distance0 = axisMetric(
                    axes[axisIndex], level0, level1, 0
                );
                const float distance1 = axisMetric(
                    axes[axisIndex], level0, level1, 1
                );
                level2Interleaved.push_back(
                    (distance1 - distance0) * input.reliabilities[i]
                );
            }
        }

        std::vector<std::uint8_t> level2Bits;
        std::vector<std::uint8_t> level2Mapped;
        if (!decodeLevel(2, level2Interleaved, level2Bits, level2Mapped))
        {
            if (error) {
                *error = "DRM 64-QAM MSC level-2 decoding failed";
            }

            return false;
        }

        // Repeat the multilevel decisions using all decisions from the prior
        // pass. The reference schedule re-encodes levels zero and one before
        // each final level-two decision; extra passes can help a marginal
        // 64-QAM signal converge.
        for (int iteration = 0; iteration < AdditionalMLCIterations; ++iteration)
        {
            // Each pass depends only on the previous decisions, so a pass
            // that changes none of them has converged.
            const std::vector<std::uint8_t> previous0 = level0Bits;
            const std::vector<std::uint8_t> previous1 = level1Bits;
            const std::vector<std::uint8_t> previous2 = level2Bits;
            level0Interleaved.clear();
            for (std::size_t i = 0; i < input.cells.size(); ++i)
            {
                const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
                for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
                {
                    const std::size_t bitIndex = 2U * i
                        + static_cast<std::size_t>(axisIndex);
                    const int level1 = level1Mapped[bitIndex];
                    const int level2 = level2Mapped[bitIndex];
                    level0Interleaved.push_back((
                        axisMetric(axes[axisIndex], 1, level1, level2)
                        - axisMetric(axes[axisIndex], 0, level1, level2)
                    ) * input.reliabilities[i]);
                }
            }

            if (!decodeLevel(0, level0Interleaved, level0Bits, level0Mapped))
            {
                if (error) {
                    *error = "DRM 64-QAM MSC iterative level-0 decoding failed";
                }

                return false;
            }

            level1Interleaved.clear();
            for (std::size_t i = 0; i < input.cells.size(); ++i)
            {
                const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
                for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
                {
                    const std::size_t bitIndex = 2U * i
                        + static_cast<std::size_t>(axisIndex);
                    const int level0 = level0Mapped[bitIndex];
                    const int level2 = level2Mapped[bitIndex];
                    level1Interleaved.push_back((
                        axisMetric(axes[axisIndex], level0, 1, level2)
                        - axisMetric(axes[axisIndex], level0, 0, level2)
                    ) * input.reliabilities[i]);
                }
            }

            if (!decodeLevel(1, level1Interleaved, level1Bits, level1Mapped))
            {
                if (error) {
                    *error = "DRM 64-QAM MSC iterative level-1 decoding failed";
                }

                return false;
            }

            level2Interleaved.clear();
            for (std::size_t i = 0; i < input.cells.size(); ++i)
            {
                const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
                for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
                {
                    const std::size_t bitIndex = 2U * i
                        + static_cast<std::size_t>(axisIndex);
                    const int level0 = level0Mapped[bitIndex];
                    const int level1 = level1Mapped[bitIndex];
                    level2Interleaved.push_back((
                        axisMetric(axes[axisIndex], level0, level1, 1)
                        - axisMetric(axes[axisIndex], level0, level1, 0)
                    ) * input.reliabilities[i]);
                }
            }

            if (!decodeLevel(2, level2Interleaved, level2Bits, level2Mapped))
            {
                if (error) {
                    *error = "DRM 64-QAM MSC iterative level-2 decoding failed";
                }

                return false;
            }

            if (level0Bits == previous0 && level1Bits == previous1 && level2Bits == previous2) {
                break;
            }
        }

        output.bits = std::move(level0Bits);
        output.bits.insert(output.bits.end(), level1Bits.begin(), level1Bits.end());
        output.bits.insert(output.bits.end(), level2Bits.begin(), level2Bits.end());
    }
    else
    {
        if (error) {
            *error = "Invalid DRM MSC component profile";
        }

        return false;
    }

    EnergyDispersal::apply(output.bits);
    output.trailingBitCount = static_cast<int>(output.bits.size() % 8U);
    return true;
}

bool MSCChannelDecoder::decode(
    Mode mode,
    int occupancy,
    int mscMode,
    const MSCProtectionParameters& protection,
    const MSCMultiplexFrameCells& input,
    MSCDecodedMultiplexFrame& output,
    std::string* error
)
{
    if (protection.partABytes == 0U)
    {
        return decode(
            mode, occupancy, mscMode, protection.protectionLevelB,
            input, output, error
        );
    }

    output = MSCDecodedMultiplexFrame();
    MSCCapacity capacityValues;
    MSCDecodeLayout layout;
    if (!supports(mode, mscMode, true)
        || !MSCMultiplexFrameSplitter::capacity(mode, occupancy, capacityValues)
        || !makeDecodeLayout(
            mode, mscMode, capacityValues.cellsPerMultiplexFrame,
            protection, layout))
    {
        if (error) {
            *error = "Unsupported DRM MSC unequal-protection profile";
        }

        return false;
    }

    if (input.cells.size()
            != static_cast<std::size_t>(capacityValues.cellsPerMultiplexFrame)
        || input.reliabilities.size() != input.cells.size())
    {
        if (error) {
            *error = "Invalid DRM MSC multiplex-frame cell count";
        }

        return false;
    }

    const std::size_t componentCount = layout.lowerProfiles.size();
    std::vector<std::vector<std::uint8_t>> decodedLevels(componentCount);
    std::vector<std::vector<std::uint8_t>> mappedLevels(componentCount);
    auto decodeLevel = [&](std::size_t level, const std::vector<float>& interleaved) {
        const MSCComponentProfile& higher = layout.higherProfiles[level];
        const MSCComponentProfile& lower = layout.lowerProfiles[level];
        const std::size_t higherCodedCount = 2U
            * static_cast<std::size_t>(layout.higherProtectedCells);
        const std::size_t lowerCodedCount = 2U
            * static_cast<std::size_t>(layout.lowerProtectedCells);
        if (interleaved.size() != higherCodedCount + lowerCodedCount) {
            return false;
        }

        const std::vector<float> higherInterleaved(
            interleaved.begin(),
            interleaved.begin() + static_cast<std::ptrdiff_t>(higherCodedCount)
        );
        const std::vector<float> lowerInterleaved(
            interleaved.begin() + static_cast<std::ptrdiff_t>(higherCodedCount),
            interleaved.end()
        );
        std::vector<float> softBits = deinterleaveComponent(
            higherInterleaved, higher.interleaverMultiplier
        );
        const std::vector<float> lowerSoft = deinterleaveComponent(
            lowerInterleaved, lower.interleaverMultiplier
        );
        softBits.insert(softBits.end(), lowerSoft.begin(), lowerSoft.end());
        if (!ViterbiDecoder::decodeUEP(
                softBits,
                static_cast<std::size_t>(layout.higherBitCounts[level]),
                higher.rate,
                static_cast<std::size_t>(layout.lowerBitCounts[level]),
                lower.rate, layout.tailResiduals[level], decodedLevels[level])) {
            return false;
        }

        const auto boundary = decodedLevels[level].begin()
            + layout.higherBitCounts[level];
        const std::vector<std::uint8_t> higherBits(
            decodedLevels[level].begin(), boundary
        );
        const std::vector<std::uint8_t> lowerBits(
            boundary, decodedLevels[level].end()
        );
        const std::vector<std::uint8_t> coded = ConvolutionalCode::encodeUEP(
            higherBits, higher.rate, lowerBits, lower.rate,
            layout.tailResiduals[level]
        );
        if (coded.size() != interleaved.size()) {
            return false;
        }

        const std::vector<std::uint8_t> higherCoded(
            coded.begin(),
            coded.begin() + static_cast<std::ptrdiff_t>(higherCodedCount)
        );
        const std::vector<std::uint8_t> lowerCoded(
            coded.begin() + static_cast<std::ptrdiff_t>(higherCodedCount), coded.end()
        );
        mappedLevels[level] = interleaveComponent(
            higherCoded, higher.interleaverMultiplier
        );
        const std::vector<std::uint8_t> lowerMapped = interleaveComponent(
            lowerCoded, lower.interleaverMultiplier
        );
        mappedLevels[level].insert(
            mappedLevels[level].end(), lowerMapped.begin(), lowerMapped.end()
        );
        return mappedLevels[level].size() == 2U * input.cells.size();
    };

    if (componentCount == 1U)
    {
        std::vector<float> soft;
        soft.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            soft.push_back(input.cells[i].real() * input.reliabilities[i]);
            soft.push_back(input.cells[i].imag() * input.reliabilities[i]);
        }

        if (!decodeLevel(0U, soft))
        {
            if (error) {
                *error = "DRM 4-QAM UEP MSC decoding failed";
            }

            return false;
        }
    }
    else if (componentCount == 2U)
    {
        constexpr float AxisScale = 0.31622776601683794f;
        auto metric = [=](float received, int level0, int level1) {
            static constexpr int amplitudes[2][2] = {{3, -1}, {1, -3}};
            const float difference = received
                - AxisScale * static_cast<float>(amplitudes[level0][level1]);
            return std::abs(difference);
        };
        std::vector<float> level0Soft;
        level0Soft.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            for (float axis : {input.cells[i].real(), input.cells[i].imag()})
            {
                const float distance0 = std::min(
                    metric(axis, 0, 0), metric(axis, 0, 1)
                );
                const float distance1 = std::min(
                    metric(axis, 1, 0), metric(axis, 1, 1)
                );
                level0Soft.push_back(
                    (distance1 - distance0) * input.reliabilities[i]
                );
            }
        }

        if (!decodeLevel(0U, level0Soft))
        {
            if (error) {
                *error = "DRM 16-QAM UEP MSC level-0 decoding failed";
            }

            return false;
        }

        std::vector<float> level1Soft;
        level1Soft.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
            for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
            {
                const std::size_t bit = 2U * i + static_cast<std::size_t>(axisIndex);
                const int level0 = mappedLevels[0][bit];
                level1Soft.push_back((
                    metric(axes[axisIndex], level0, 1)
                    - metric(axes[axisIndex], level0, 0)
                ) * input.reliabilities[i]);
            }
        }

        if (!decodeLevel(1U, level1Soft))
        {
            if (error) {
                *error = "DRM 16-QAM UEP MSC level-1 decoding failed";
            }

            return false;
        }
    }
    else if (componentCount == 3U)
    {
        constexpr float AxisScale = 0.15430334996209191f;
        auto metric = [=](float received, int level0, int level1, int level2) {
            static constexpr int amplitudes[2][2][2] = {
                {{7, -1}, {3, -5}},
                {{5, -3}, {1, -7}}
            };
            const float difference = received - AxisScale * static_cast<float>(
                amplitudes[level0][level1][level2]
            );
            return std::abs(difference);
        };
        std::vector<float> level0Soft;
        level0Soft.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            for (float axis : {input.cells[i].real(), input.cells[i].imag()})
            {
                const float distance0 = std::min({
                    metric(axis, 0, 0, 0), metric(axis, 0, 0, 1),
                    metric(axis, 0, 1, 0), metric(axis, 0, 1, 1)
                });
                const float distance1 = std::min({
                    metric(axis, 1, 0, 0), metric(axis, 1, 0, 1),
                    metric(axis, 1, 1, 0), metric(axis, 1, 1, 1)
                });
                level0Soft.push_back(
                    (distance1 - distance0) * input.reliabilities[i]
                );
            }
        }

        if (!decodeLevel(0U, level0Soft))
        {
            if (error) {
                *error = "DRM 64-QAM UEP MSC level-0 decoding failed";
            }

            return false;
        }

        std::vector<float> level1Soft;
        level1Soft.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
            for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
            {
                const std::size_t bit = 2U * i + static_cast<std::size_t>(axisIndex);
                const int level0 = mappedLevels[0][bit];
                const float distance0 = std::min(
                    metric(axes[axisIndex], level0, 0, 0),
                    metric(axes[axisIndex], level0, 0, 1)
                );
                const float distance1 = std::min(
                    metric(axes[axisIndex], level0, 1, 0),
                    metric(axes[axisIndex], level0, 1, 1)
                );
                level1Soft.push_back(
                    (distance1 - distance0) * input.reliabilities[i]
                );
            }
        }

        if (!decodeLevel(1U, level1Soft))
        {
            if (error) {
                *error = "DRM 64-QAM UEP MSC level-1 decoding failed";
            }

            return false;
        }

        std::vector<float> level2Soft;
        level2Soft.reserve(2U * input.cells.size());
        for (std::size_t i = 0; i < input.cells.size(); ++i)
        {
            const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
            for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
            {
                const std::size_t bit = 2U * i + static_cast<std::size_t>(axisIndex);
                const int level0 = mappedLevels[0][bit];
                const int level1 = mappedLevels[1][bit];
                level2Soft.push_back((
                    metric(axes[axisIndex], level0, level1, 1)
                    - metric(axes[axisIndex], level0, level1, 0)
                ) * input.reliabilities[i]);
            }
        }

        if (!decodeLevel(2U, level2Soft))
        {
            if (error) {
                *error = "DRM 64-QAM UEP MSC level-2 decoding failed";
            }

            return false;
        }

        // Apply the same iterative multilevel-decision schedule as the EEP
        // path. The UEP code changes puncturing at the part-A/part-B boundary,
        // but its re-encoded constellation decisions have the same axis
        // mapping.
        for (int iteration = 0; iteration < AdditionalMLCIterations; ++iteration)
        {
            const std::vector<std::vector<std::uint8_t>> previous(
                decodedLevels.begin(), decodedLevels.begin() + 3
            );
            level0Soft.clear();
            for (std::size_t i = 0; i < input.cells.size(); ++i)
            {
                const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
                for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
                {
                    const std::size_t bit = 2U * i
                        + static_cast<std::size_t>(axisIndex);
                    const int level1 = mappedLevels[1][bit];
                    const int level2 = mappedLevels[2][bit];
                    level0Soft.push_back((
                        metric(axes[axisIndex], 1, level1, level2)
                        - metric(axes[axisIndex], 0, level1, level2)
                    ) * input.reliabilities[i]);
                }
            }

            if (!decodeLevel(0U, level0Soft))
            {
                if (error) {
                    *error = "DRM 64-QAM UEP iterative level-0 decoding failed";
                }

                return false;
            }

            level1Soft.clear();
            for (std::size_t i = 0; i < input.cells.size(); ++i)
            {
                const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
                for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
                {
                    const std::size_t bit = 2U * i
                        + static_cast<std::size_t>(axisIndex);
                    const int level0 = mappedLevels[0][bit];
                    const int level2 = mappedLevels[2][bit];
                    level1Soft.push_back((
                        metric(axes[axisIndex], level0, 1, level2)
                        - metric(axes[axisIndex], level0, 0, level2)
                    ) * input.reliabilities[i]);
                }
            }

            if (!decodeLevel(1U, level1Soft))
            {
                if (error) {
                    *error = "DRM 64-QAM UEP iterative level-1 decoding failed";
                }

                return false;
            }

            level2Soft.clear();
            for (std::size_t i = 0; i < input.cells.size(); ++i)
            {
                const float axes[2] = {input.cells[i].real(), input.cells[i].imag()};
                for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
                {
                    const std::size_t bit = 2U * i
                        + static_cast<std::size_t>(axisIndex);
                    const int level0 = mappedLevels[0][bit];
                    const int level1 = mappedLevels[1][bit];
                    level2Soft.push_back((
                        metric(axes[axisIndex], level0, level1, 1)
                        - metric(axes[axisIndex], level0, level1, 0)
                    ) * input.reliabilities[i]);
                }
            }

            if (!decodeLevel(2U, level2Soft))
            {
                if (error) {
                    *error = "DRM 64-QAM UEP iterative level-2 decoding failed";
                }

                return false;
            }

            if (std::equal(previous.begin(), previous.end(), decodedLevels.begin())) {
                break;
            }
        }
    }
    else
    {
        if (error) {
            *error = "Invalid DRM MSC UEP component profile";
        }

        return false;
    }

    output.bits.reserve(static_cast<std::size_t>(layoutInformationBitCount(layout)));
    for (std::size_t level = 0; level < componentCount; ++level)
    {
        output.bits.insert(
            output.bits.end(), decodedLevels[level].begin(),
            decodedLevels[level].begin() + layout.higherBitCounts[level]
        );
    }

    for (std::size_t level = 0; level < componentCount; ++level)
    {
        output.bits.insert(
            output.bits.end(),
            decodedLevels[level].begin() + layout.higherBitCounts[level],
            decodedLevels[level].end()
        );
    }

    EnergyDispersal::apply(output.bits);
    output.trailingBitCount = static_cast<int>(output.bits.size() % 8U);
    return true;
}

bool MSCMultiplexParser::extractStreams(
    const MSCDecodedMultiplexFrame& multiplex,
    const SDCMultiplexDescription& description,
    std::vector<MSCLogicalStreamFrame>& streams,
    std::string* error
)
{
    streams.clear();
    if (!description.present || description.streams.empty())
    {
        if (error) {
            *error = "Missing DRM MSC multiplex description";
        }

        return false;
    }

    std::size_t partABytes = 0;
    std::size_t partBBytes = 0;
    for (const SDCStreamDescription& stream : description.streams)
    {
        if (stream.partALengthBytes < 0 || stream.partBLengthBytes < 0)
        {
            if (error) {
                *error = "Invalid DRM MSC stream length";
            }

            return false;
        }

        partABytes += static_cast<std::size_t>(stream.partALengthBytes);
        partBBytes += static_cast<std::size_t>(stream.partBLengthBytes);
    }

    if (8U * (partABytes + partBBytes) > multiplex.bits.size())
    {
        if (error) {
            *error = "DRM MSC stream lengths exceed decoded capacity";
        }

        return false;
    }

    streams.resize(description.streams.size());
    std::size_t partAOffset = 0;
    std::size_t partBOffset = 8U * partABytes;
    for (std::size_t i = 0; i < description.streams.size(); ++i)
    {
        const SDCStreamDescription& source = description.streams[i];
        MSCLogicalStreamFrame& destination = streams[i];
        destination.streamId = source.streamId;
        destination.partA = bitsToBytes(
            multiplex.bits, partAOffset,
            static_cast<std::size_t>(source.partALengthBytes)
        );
        destination.partB = bitsToBytes(
            multiplex.bits, partBOffset,
            static_cast<std::size_t>(source.partBLengthBytes)
        );
        partAOffset += 8U * static_cast<std::size_t>(source.partALengthBytes);
        partBOffset += 8U * static_cast<std::size_t>(source.partBLengthBytes);
    }

    return true;
}

} // namespace modemdrm
