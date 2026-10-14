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

#include "drmfac.h"

#include "drmbitutils.h"
#include "drmcellmap.h"
#include "drmconvolutional.h"
#include "drminterleaver.h"

#include <cstddef>
#include <algorithm>

namespace modemdrm
{

namespace
{

// The two supplied 2012 RFmondial DRM+ vectors scramble the FAC tail bits
// (see decode()). decode() removes the scrambled tail exactly, after the
// standard decode fails, when its caller allows it (the receiver does for
// mode E).

std::uint32_t readBits(const std::vector<std::uint8_t>& bits, std::size_t& offset, int count)
{
    std::uint32_t value = 0;
    for (int i = 0; i < count; ++i) {
        value = (value << 1U) | (bits[offset++] & 1U);
    }

    return value;
}

FACService readService(const std::vector<std::uint8_t>& bits, std::size_t& offset)
{
    FACService service;
    service.serviceIdentifier = readBits(bits, offset, 24);
    service.shortIdentifier = static_cast<int>(readBits(bits, offset, 2));
    service.audioConditionalAccess = readBits(bits, offset, 1) != 0;
    service.language = static_cast<int>(readBits(bits, offset, 4));
    service.dataService = readBits(bits, offset, 1) != 0;
    service.descriptor = static_cast<int>(readBits(bits, offset, 5));
    service.dataConditionalAccess = readBits(bits, offset, 1) != 0;
    readBits(bits, offset, 6); // rfa
    return service;
}

} // namespace

int FACData::serviceCount() const
{
    // ES 201 980 6.3.4: the audio services in the upper two bits (0 with
    // no data services is 4), the data services in the lower two (3 with 3
    // audio services is 4 data services)
    if (serviceCountCode == 0) {
        return 4;
    }

    if (serviceCountCode == 15) {
        return 4;
    }

    return std::min(4, (serviceCountCode >> 2) + (serviceCountCode & 3));
}

void FACServiceCache::reset()
{
    m_services.clear();
    m_serviceCountCode = m_reconfigurationIndex = -1;
}

bool FACServiceCache::observe(const FACData& block)
{
    const bool firstFrame = block.identity == 0 || block.identity == 3;
    const bool invalidated = (m_serviceCountCode >= 0
            && m_serviceCountCode != block.serviceCountCode)
        || (firstFrame && m_reconfigurationIndex > 0 && block.reconfigurationIndex == 0);
    if (invalidated) {
        m_services.clear();
    }

    m_serviceCountCode = block.serviceCountCode;
    if (firstFrame) {
        m_reconfigurationIndex = block.reconfigurationIndex;
    }

    for (const auto& service : block.services)
    {
        if (service.shortIdentifier < 0 || service.shortIdentifier > 3) {
            continue;
        }

        auto found = std::find_if(m_services.begin(), m_services.end(),
            [&service](const FACService& item) { return item.shortIdentifier == service.shortIdentifier; });
        if (found == m_services.end()) {
            m_services.push_back(service);
        } else {
            *found = service;
        }
    }

    return invalidated;
}

const FACService* FACServiceCache::findIdentifier(std::uint32_t identifier) const
{
    const auto found = std::find_if(m_services.begin(), m_services.end(),
        [identifier](const FACService& item) { return item.serviceIdentifier == identifier; });
    return found == m_services.end() ? nullptr : &*found;
}

const FACService* FACServiceCache::findShortIdentifier(int identifier) const
{
    const auto found = std::find_if(m_services.begin(), m_services.end(),
        [identifier](const FACService& item) { return item.shortIdentifier == identifier; });
    return found == m_services.end() ? nullptr : &*found;
}

int FACDecoder::informationBitCount(Mode mode)
{
    return mode == Mode::E ? 116 : 72;
}

namespace
{

// The rate-1/4 coded bits contributed by a dispersed tail after the mode E
// FAC's zero information bits: the encoder starts the tail in the zero state.
const std::vector<std::uint8_t>& legacyTailPattern()
{
    static const std::vector<std::uint8_t> pattern = [] {
        const std::size_t informationBits = static_cast<std::size_t>(FACDecoder::informationBitCount(Mode::E));
        std::vector<std::uint8_t> prbs(informationBits + ConvolutionalCode::TailBitCount, 0U);
        EnergyDispersal::apply(prbs);
        std::vector<std::uint8_t> inputs(informationBits + ConvolutionalCode::TailBitCount, 0U);
        std::copy(prbs.begin() + static_cast<std::ptrdiff_t>(informationBits), prbs.end(),
            inputs.begin() + static_cast<std::ptrdiff_t>(informationBits));
        const std::vector<std::uint8_t> mother = ConvolutionalCode::encodeMother(inputs);
        // Rate 1/4 transmits mother-code branches 0 to 3 of every step.
        std::vector<std::uint8_t> coded;
        for (std::size_t step = 0; step < inputs.size(); ++step)
        {
            for (std::size_t branch = 0; branch < 4U; ++branch) {
                coded.push_back(mother[step * 6U + branch]);
            }
        }

        return coded;
    }();
    return pattern;
}

} // namespace

bool FACDecoder::decodeBits(
    Mode mode,
    std::vector<std::uint8_t> bits,
    FACData& data
)
{
    const int bitCount = informationBitCount(mode);
    if (static_cast<int>(bits.size()) != bitCount) {
        return false;
    }

    EnergyDispersal::apply(bits);

    const std::size_t payloadBits = mode == Mode::E ? 108U : 64U;
    std::vector<std::uint8_t> crcInput(bits.begin(), bits.begin() + static_cast<std::ptrdiff_t>(payloadBits));
    if (mode == Mode::E) {
        crcInput.insert(crcInput.end(), 4U, 0U);
    }

    std::size_t crcOffset = payloadBits;
    const std::uint8_t transmittedCRC = static_cast<std::uint8_t>(readBits(bits, crcOffset, 8));
    const std::uint8_t expectedCRC = CRC::crc8(crcInput.data(), crcInput.size());
    if (expectedCRC != transmittedCRC) {
        return false;
    }

    std::size_t offset = 0;
    FACData decoded;
    decoded.baseLayer = readBits(bits, offset, 1) == 0;
    decoded.identity = static_cast<int>(readBits(bits, offset, 2));
    decoded.modeE = readBits(bits, offset, 1) != 0;
    decoded.occupancy = static_cast<int>(readBits(bits, offset, 3));
    decoded.shortInterleaving = readBits(bits, offset, 1) != 0;
    decoded.mscMode = static_cast<int>(readBits(bits, offset, 2));
    decoded.sdcMode = static_cast<int>(readBits(bits, offset, 1));
    decoded.serviceCountCode = static_cast<int>(readBits(bits, offset, 4));
    decoded.reconfigurationIndex = static_cast<int>(readBits(bits, offset, 3));
    decoded.toggle = readBits(bits, offset, 1) != 0;
    // rfu: receivers ignore it (as Dream does), so a later use of it does not
    // stop them locking
    readBits(bits, offset, 1);
    // Validate the signalled mode before it controls the service count. A CRC
    // match does not guarantee legal signalling: an A-D block has only one
    // service field and must never be parsed as the longer mode-E layout.
    if (decoded.modeE != (mode == Mode::E)
        || Parameters::forSpectrum(mode, decoded.occupancy) == nullptr) {
        return false;
    }

    const int serviceParameterCount = mode == Mode::E ? 2 : 1;
    decoded.services.reserve(static_cast<std::size_t>(serviceParameterCount));
    for (int i = 0; i < serviceParameterCount; ++i) {
        decoded.services.push_back(readService(bits, offset));
    }

    // Only a block that passes every check replaces data.
    data = std::move(decoded);
    return true;
}

bool FACDecoder::decode(
    Mode mode,
    const std::vector<EqualizedSymbol>& symbols,
    FACData& data,
    std::vector<std::uint8_t>* decodedBits,
    bool allowLegacyModeETail
)
{
    const std::vector<CellPosition>& positions = CellMap::facCells(mode);
    std::vector<float> interleavedSoftBits;
    interleavedSoftBits.reserve(positions.size() * 2U);
    for (const CellPosition& position : positions)
    {
        if (position.symbol < 0 || static_cast<std::size_t>(position.symbol) >= symbols.size()
            || !symbols[static_cast<std::size_t>(position.symbol)].contains(position.carrier)) {
            return false;
        }

        const EqualizedSymbol& symbol = symbols[static_cast<std::size_t>(position.symbol)];
        const std::complex<float>& cell = symbol.cell(position.carrier);
        const std::size_t channelIndex = static_cast<std::size_t>(
            position.carrier - symbol.minimumCarrier
        );
        // The equalized constellation has been divided by |H|^2. Restore |H|
        // (as the MSC does) in A-D: on real HF, interference and estimation
        // errors on strong carriers made |H|^2 trust a few wrong cells over
        // the rest (BBC 3955 kHz: 36 failed blocks, none with |H|; Kuwait
        // 10:02 39 to 2). The mode E sweeps (deep fades, no interference)
        // lose 2% of superframes with |H|; keep |H|^2 there.
        const float reliability = channelIndex >= symbol.channel.size() ? 1.0f
            : mode == Mode::E ? std::norm(symbol.channel[channelIndex])
            : std::abs(symbol.channel[channelIndex]);
        interleavedSoftBits.push_back(cell.real() * reliability);
        interleavedSoftBits.push_back(cell.imag() * reliability);
    }

    std::vector<float> softBits = BitInterleaver::deinterleave(interleavedSoftBits, 21);
    const std::size_t information = static_cast<std::size_t>(informationBitCount(mode));
    const CodeRate rate = mode == Mode::E ? CodeRate::R1_4 : CodeRate::R3_5;
    // On failure decodedBits keeps the standard attempt's bits.
    const auto attempt = [&](const std::vector<float>& soft, bool first) {
        std::vector<std::uint8_t> bits;
        if (!ViterbiDecoder::decode(soft, information, rate, bits)) {
            return false;
        }

        std::vector<std::uint8_t> descrambled;
        if (decodedBits)
        {
            descrambled = bits;
            EnergyDispersal::apply(descrambled);
        }

        const bool decoded = decodeBits(mode, std::move(bits), data);
        if (decodedBits && (decoded || first)) {
            *decodedBits = std::move(descrambled);
        }

        return decoded;
    };
    if (attempt(softBits, true)) {
        return true;
    }

    if (mode != Mode::E || !allowLegacyModeETail) {
        return false;
    }

    // Early DRM+ transmitters (the 2012 RFmondial recordings) applied energy
    // dispersal to the six tail bits as well, so the code does not end in the
    // zero state. The code is linear: remove the scrambled tail's contribution
    // by flipping the soft bits it sets, then decode as usual. The CRC is then
    // the standard one.
    const std::vector<std::uint8_t>& tailPattern = legacyTailPattern();
    std::vector<float> legacySoft = softBits;
    for (std::size_t i = 0; i < legacySoft.size() && i < tailPattern.size(); ++i)
    {
        if (tailPattern[i]) {
            legacySoft[i] = -legacySoft[i];
        }
    }

    if (attempt(legacySoft, false))
    {
        data.legacyModeETail = true;
        return true;
    }

    // Such blocks decoded with a zero-terminated trellis instead show a fixed
    // CRC syndrome (0x05); accepting it would add a CRC hypothesis per failed
    // block for little gain, since the exact tail removal decodes them.
    return false;
}

} // namespace modemdrm
