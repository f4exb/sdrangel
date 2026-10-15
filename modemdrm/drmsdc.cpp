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

#include "drmsdc.h"

#include "drmaudioframing.h"
#include "drmbitutils.h"
#include "drmcellmap.h"
#include "drmconvolutional.h"
#include "drminterleaver.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace modemdrm
{

namespace
{

class BitReader
{
public:
    explicit BitReader(const std::vector<std::uint8_t>& bits) : m_bits(bits) {}

    bool read(int count, std::uint32_t& value)
    {
        if (count < 0 || count > 32 || static_cast<std::size_t>(count) > remaining()) {
            return false;
        }

        value = 0;
        for (int i = 0; i < count; ++i) {
            value = (value << 1U) | (m_bits[m_offset++] & 1U);
        }

        return true;
    }

    std::size_t remaining() const { return m_bits.size() - m_offset; }

private:
    const std::vector<std::uint8_t>& m_bits;
    std::size_t m_offset = 0;
};

std::vector<std::uint8_t> bytesToBits(const std::vector<std::uint8_t>& bytes)
{
    std::vector<std::uint8_t> bits;
    bits.reserve(bytes.size() * 8U);
    for (std::uint8_t byte : bytes)
    {
        for (int bit = 7; bit >= 0; --bit) {
            bits.push_back(static_cast<std::uint8_t>((byte >> bit) & 1U));
        }
    }

    return bits;
}

bool allZeroFrom(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    return std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end(),
        [](std::uint8_t byte) { return byte == 0U; });
}

void preserveAFSEntity(const SDCEntity& entity, SDCData& data)
{
    if (entity.type != 3 && entity.type != 4 && entity.type != 7
        && entity.type != 11 && entity.type != 13 && entity.type != 15) {
        return;
    }

    SDCAlternativeFrequencyEntity afs;
    afs.type = entity.type;
    afs.listVersion = entity.version;
    afs.bodyBits = entity.bodyBits;
    if (entity.type == 3 && entity.bodyBits.size() >= 4U)
    {
        // Clause 6.4.3.4: four flags, then an 8-bit Service Restriction field
        // and an 8-bit Region/Schedule field when their flags are set, then
        // the 16-bit frequency codes.
        BitReader reader(entity.bodyBits);
        std::uint32_t value = 0;
        reader.read(1, value);
        const bool synchronous = value != 0U;
        reader.read(1, value);
        const bool baseLayer = value == 0U;
        reader.read(1, value);
        const bool serviceRestriction = value != 0U;
        reader.read(1, value);
        const bool regionSchedule = value != 0U;
        const std::size_t optionalBits = (serviceRestriction ? 8U : 0U)
            + (regionSchedule ? 8U : 0U);
        if (reader.remaining() >= optionalBits
            && (reader.remaining() - optionalBits) % 16U == 0U)
        {
            afs.synchronousMultiplex = synchronous;
            afs.baseLayer = baseLayer;
            afs.appliesToAllServices = !serviceRestriction;
            afs.regionOrScheduleRestricted = regionSchedule;
            if (serviceRestriction)
            {
                reader.read(8, value);
                afs.serviceRestrictionField = static_cast<int>(value);
            }

            if (regionSchedule)
            {
                reader.read(8, value);
                afs.regionScheduleField = static_cast<int>(value);
            }

            while (reader.remaining() >= 16U)
            {
                reader.read(16, value);
                afs.frequencyCodes.push_back(static_cast<std::uint16_t>(value));
            }

            afs.commonHeaderPresent = true;
        }
    }

    if (entity.type == 15 && entity.bodyBits.size() >= 4U)
    {
        BitReader reader(entity.bodyBits);
        std::uint32_t extension = 0;
        reader.read(4, extension);
        afs.extension = static_cast<int>(extension);
    }

    data.alternativeFrequencies.push_back(std::move(afs));
}

bool parseEntity(const SDCEntity& entity, SDCData& data, std::string* error, bool strict)
{
    BitReader reader(entity.bodyBits);
    std::uint32_t value = 0;

    switch (entity.type)
    {
    case 0:
    {
        // Stream Ids are 2 bits, so a multiplex has one to four streams of
        // 24 bits each. An empty description must not replace a live one.
        if (entity.lengthBytes % 3 != 0 || entity.lengthBytes < 3
            || entity.lengthBytes > 12) {
            break;
        }

        SDCMultiplexDescription multiplex;
        if (!reader.read(2, value)) {
            break;
        }

        multiplex.protectionLevelA = static_cast<int>(value);
        if (!reader.read(2, value)) {
            break;
        }

        multiplex.protectionLevelB = static_cast<int>(value);
        bool complete = true;
        for (int stream = 0; stream < entity.lengthBytes / 3; ++stream)
        {
            SDCStreamDescription description;
            description.streamId = stream;
            std::uint32_t partA = 0;
            std::uint32_t partB = 0;
            if (!reader.read(12, partA) || !reader.read(12, partB))
            {
                complete = false;
                break;
            }

            description.partALengthBytes = static_cast<int>(partA);
            description.partBLengthBytes = static_cast<int>(partB);
            multiplex.streams.push_back(description);
        }

        if (!complete || reader.remaining() != 0) {
            break;
        }

        multiplex.present = true;
        multiplex.nextConfiguration = entity.version;
        (entity.version ? data.nextMultiplex : data.multiplex) = std::move(multiplex);
        return true;
    }
    case 1:
    {
        SDCLabel label;
        if (!reader.read(2, value)) {
            break;
        }

        label.shortId = static_cast<int>(value);
        // rfu "reserved for future use of the remainder of the parameter
        // field": a non-zero value means the label cannot be interpreted.
        if (!reader.read(2, value) || value != 0U) {
            break;
        }

        std::vector<std::uint8_t> labelBytes;
        while (reader.remaining() >= 8U)
        {
            reader.read(8, value);
            labelBytes.push_back(static_cast<std::uint8_t>(value));
        }

        if (reader.remaining() != 0U) {
            break;
        }

        // Clause 6.4.3.2: a non-zero text control field is carried as the
        // first label byte, 0x01..0x0F. It is signalling, not a character.
        if (!labelBytes.empty() && labelBytes.front() >= 0x01U && labelBytes.front() <= 0x0FU)
        {
            label.textControl = labelBytes.front();
            labelBytes.erase(labelBytes.begin());
        }

        label.utf8.assign(labelBytes.begin(), labelBytes.end());
        data.labels.push_back(std::move(label));
        return true;
    }
    case 5:
    {
        SDCDataApplication application;
        if (!reader.read(2, value)) {
            break;
        }

        application.shortId = static_cast<int>(value);
        if (!reader.read(2, value)) {
            break;
        }

        application.streamId = static_cast<int>(value);
        if (!reader.read(1, value)) {
            break;
        }

        application.packetMode = value != 0U;
        if (application.packetMode)
        {
            if (!reader.read(1, value)) {
                break;
            }

            application.dataUnits = value != 0U;
            if (!reader.read(2, value)) {
                break;
            }

            application.packetId = static_cast<int>(value);
            if (!reader.read(1, value)) {
                break;
            }

            application.enhancement = value != 0U;
            if (!reader.read(3, value)) {
                break;
            }

            application.applicationDomain = static_cast<int>(value);
            if (!reader.read(8, value)) {
                break;
            }

            application.packetLengthBytes = static_cast<int>(value);
        }
        else
        {
            // Synchronous stream mode: 3 rfa bits, the enhancement flag and the
            // application domain.
            if (!reader.read(3, value) || !reader.read(1, value)) {
                break;
            }

            application.enhancement = value != 0U;
            if (!reader.read(3, value)) {
                break;
            }

            application.applicationDomain = static_cast<int>(value);
        }

        if (application.packetMode && application.applicationDomain == 1)
        {
            // DAB domain: 5 rfu bits and the 11-bit user application type.
            if (!reader.read(5, value) || !reader.read(11, value)) {
                break;
            }

            application.userApplicationType = static_cast<int>(value);
        }

        while (reader.remaining() >= 8U)
        {
            reader.read(8, value);
            application.applicationData.push_back(static_cast<std::uint8_t>(value));
        }

        if (reader.remaining() != 0U) {
            break;
        }

        if (!entity.version) {
            data.dataApplications.push_back(std::move(application));
        }

        return true;
    }
    case 8:
    {
        SDCTimeDate decoded;
        if (!reader.read(17, value)) {
            break;
        }

        decoded.modifiedJulianDate = static_cast<int>(value);
        if (!reader.read(5, value)) {
            break;
        }

        decoded.hour = static_cast<int>(value);
        if (!reader.read(6, value)) {
            break;
        }

        decoded.minute = static_cast<int>(value);
        if (decoded.hour > 23 || decoded.minute > 59
            || !SDCParser::modifiedJulianDateToGregorian(
                decoded.modifiedJulianDate, decoded.year, decoded.month, decoded.day)) {
            break;
        }

        if (reader.remaining() == 8U)
        {
            if (!reader.read(2, value) || value != 0U) {
                break;
            }

            std::uint32_t sense = 0;
            std::uint32_t halfHours = 0;
            if (!reader.read(1, sense) || !reader.read(5, halfHours)) {
                break;
            }

            decoded.localTimeOffsetPresent = true;
            decoded.localTimeOffsetMinutes = static_cast<int>(halfHours) * 30;
            if (sense != 0U) {
                decoded.localTimeOffsetMinutes = -decoded.localTimeOffsetMinutes;
            }
        }

        if (reader.remaining() != 0U) {
            break;
        }

        decoded.present = true;
        data.timeDate = decoded;
        return true;
    }
    case 9:
    {
        SDCAudioInformation audio;
        audio.nextConfiguration = entity.version;
        if (!reader.read(2, value)) {
            break;
        }

        audio.shortId = static_cast<int>(value);
        if (!reader.read(2, value)) {
            break;
        }

        audio.streamId = static_cast<int>(value);
        if (!reader.read(2, value)) {
            break;
        }

        audio.audioCoding = static_cast<int>(value);
        if (!reader.read(1, value)) {
            break;
        }

        audio.sbr = value != 0U;
        if (!reader.read(2, value)) {
            break;
        }

        audio.audioMode = static_cast<int>(value);
        if (!reader.read(3, value)) {
            break;
        }

        audio.samplingRateCode = static_cast<int>(value);
        if (!reader.read(1, value)) {
            break;
        }

        audio.text = value != 0U;
        if (!reader.read(1, value)) {
            break;
        }

        audio.enhancement = value != 0U;
        if (!reader.read(5, value)) {
            break;
        }

        audio.coderField = static_cast<int>(value);
        audio.surroundMode = (audio.coderField >> 2) & 0x7;
        std::uint32_t rfa = 0;
        if (!reader.read(1, rfa)) {
            break;
        }

        const bool aac = audio.audioCoding == 0;
        const bool xhe = audio.audioCoding == 3;
        if (strict)
        {
            // Candidate-selection checks for list-decoded blocks only: any
            // reserved or rfa value makes a candidate implausible. The AAC
            // sampling rate depends on the mode; SDCDecoder checks it.
            if (rfa != 0U || (audio.coderField & 0x3) != 0) {
                break;
            }

            if (aac && (audio.audioMode == 3 || reader.remaining() != 0U)) {
                break;
            }

            if (xhe && (audio.sbr || (audio.audioMode != 0 && audio.audioMode != 2))) {
                break;
            }

            if (!aac && !xhe) {
                break;
            }
        }

        // rfa fields are ignored. For xHE-AAC the SBR position is rfa. Reserved
        // codings and values are kept, so the service is listed and reported
        // as unsupported rather than hiding the rest of the SDC block.
        if (!aac) {
            audio.sbr = false;
        }

        while (reader.remaining() >= 8U)
        {
            reader.read(8, value);
            audio.codecSpecificConfig.push_back(static_cast<std::uint8_t>(value));
        }

        if (reader.remaining() != 0U) {
            break;
        }

        // Codec specific config is defined only for xHE-AAC (n = 0 otherwise).
        if (!xhe) {
            audio.codecSpecificConfig.clear();
        }

        if (entity.version) {
            data.nextAudio.push_back(std::move(audio));
        } else {
            data.audio.push_back(std::move(audio));
        }

        return true;
    }
    case 12:
    {
        SDCLanguageCountry item;
        if (!reader.read(2, value)) {
            break;
        }

        item.shortId = static_cast<int>(value);
        if (!reader.read(2, value) || value != 0U) {
            break;
        }

        for (int i = 0; i < 3; ++i)
        {
            if (!reader.read(8, value)) {
                break;
            }

            item.language.push_back(static_cast<char>(value));
        }

        for (int i = 0; i < 2; ++i)
        {
            if (!reader.read(8, value)) {
                break;
            }

            item.country.push_back(static_cast<char>(value));
        }

        if (reader.remaining() != 0U || item.language.size() != 3U || item.country.size() != 2U) {
            break;
        }

        data.languageCountries.push_back(std::move(item));
        return true;
    }
    default:
        preserveAFSEntity(entity, data);
        return true;
    }

    if (error) {
        *error = "Malformed DRM SDC entity type " + std::to_string(entity.type);
    }

    return false;
}

bool crcMatches(
    const std::vector<std::uint8_t>& decodedBits,
    std::size_t dataFieldByteCount,
    std::string* error
)
{
    std::vector<std::uint8_t> crcInput(4U, 0U);
    crcInput.insert(crcInput.end(), decodedBits.begin(),
        decodedBits.begin() + static_cast<std::ptrdiff_t>(4U + dataFieldByteCount * 8U));
    std::uint16_t transmittedCRC = 0;
    const std::size_t crcOffset = 4U + dataFieldByteCount * 8U;
    for (std::size_t i = crcOffset; i < crcOffset + 16U; ++i) {
        transmittedCRC = static_cast<std::uint16_t>((transmittedCRC << 1U) | (decodedBits[i] & 1U));
    }

    const std::uint16_t expectedCRC = CRC::crc16(
        crcInput.data(), crcInput.size()
    );
    if (expectedCRC != transmittedCRC)
    {
        if (error)
        {
            *error = "DRM SDC CRC mismatch (expected "
                + std::to_string(expectedCRC) + ", received "
                + std::to_string(transmittedCRC) + ")";
        }

        return false;
    }

    return true;
}

// A list candidate's AAC descriptors must give a superframe the mode carries
// (12 and 24 kHz in A-D, 24 and 48 kHz in E); all xHE-AAC rates are defined.
bool plausibleAudio(Mode mode, const std::vector<SDCAudioInformation>& audio)
{
    return std::all_of(audio.begin(), audio.end(), [mode](const SDCAudioInformation& item) {
        return item.audioCoding != 0 || AACSuperframeParser::frameCount(mode, item) > 0;
    });
}

} // namespace

bool SDCParser::parseBlock(
    const std::vector<std::uint8_t>& decodedBits,
    std::size_t dataFieldByteCount,
    SDCData& data,
    std::string* error,
    bool strict,
    bool* crcValid
)
{
    if (crcValid) {
        *crcValid = false;
    }

    const std::size_t requiredBits = 4U + dataFieldByteCount * 8U + 16U;
    if (decodedBits.size() < requiredBits)
    {
        if (error) {
            *error = "Truncated DRM SDC block";
        }

        return false;
    }

    int afsIndex = 0;
    for (std::size_t i = 0; i < 4U; ++i) {
        afsIndex = (afsIndex << 1) | (decodedBits[i] & 1U);
    }

    if (!crcMatches(decodedBits, dataFieldByteCount, error)) {
        return false;
    }

    if (crcValid) {
        *crcValid = true;
    }

    std::vector<std::uint8_t> bytes(dataFieldByteCount);
    for (std::size_t byte = 0; byte < dataFieldByteCount; ++byte)
    {
        for (std::size_t bit = 0; bit < 8U; ++bit)
        {
            bytes[byte] = static_cast<std::uint8_t>(
                (bytes[byte] << 1U) | decodedBits[4U + byte * 8U + bit]
            );
        }
    }

    return parseDataField(afsIndex, bytes, data, error, strict);
}

bool SDCParser::parseDataField(
    int afsIndex,
    const std::vector<std::uint8_t>& bytes,
    SDCData& data,
    std::string* error,
    bool strict
)
{
    if (afsIndex < 0 || afsIndex > 15)
    {
        if (error) {
            *error = "Invalid DRM SDC AFS index";
        }

        return false;
    }

    data = SDCData();
    data.afsIndex = afsIndex;

    std::size_t offset = 0;
    while (offset < bytes.size())
    {
        if (allZeroFrom(bytes, offset)) {
            break;
        }

        if (bytes.size() - offset < 2U)
        {
            if (error) {
                *error = "Truncated DRM SDC entity header";
            }

            return false;
        }

        SDCEntity entity;
        entity.lengthBytes = bytes[offset] >> 1U;
        entity.version = (bytes[offset] & 1U) != 0U;
        entity.type = bytes[offset + 1U] >> 4U;
        const std::size_t entityBytes = 2U + static_cast<std::size_t>(entity.lengthBytes);
        if (entityBytes > bytes.size() - offset)
        {
            if (error) {
                *error = "DRM SDC entity exceeds data field";
            }

            return false;
        }

        entity.bodyBits.reserve(4U + static_cast<std::size_t>(entity.lengthBytes) * 8U);
        for (int bit = 3; bit >= 0; --bit) {
            entity.bodyBits.push_back(static_cast<std::uint8_t>((bytes[offset + 1U] >> bit) & 1U));
        }

        std::vector<std::uint8_t> continuation(
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + 2U),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + entityBytes)
        );
        const std::vector<std::uint8_t> continuationBits = bytesToBits(continuation);
        entity.bodyBits.insert(entity.bodyBits.end(), continuationBits.begin(), continuationBits.end());
        data.entities.push_back(entity);
        std::string entityError;
        if (!parseEntity(data.entities.back(), data, &entityError, strict))
        {
            // The length chain is intact, so the following entities are still
            // located correctly. Skip only this entity unless strict.
            if (strict)
            {
                if (error) {
                    *error = entityError;
                }

                return false;
            }

            ++data.invalidEntityCount;
        }

        offset += entityBytes;
    }

    data.valid = true;
    if (error) {
        error->clear();
    }

    return true;
}

bool SDCParser::modifiedJulianDateToGregorian(
    int modifiedJulianDate,
    int& year,
    int& month,
    int& day
)
{
    if (modifiedJulianDate < 0 || modifiedJulianDate > 131071) {
        return false;
    }

    std::int64_t l = static_cast<std::int64_t>(modifiedJulianDate) + 2400001LL + 68569LL;
    const std::int64_t n = 4LL * l / 146097LL;
    l -= (146097LL * n + 3LL) / 4LL;
    const std::int64_t i = 4000LL * (l + 1LL) / 1461001LL;
    l = l - 1461LL * i / 4LL + 31LL;
    const std::int64_t j = 80LL * l / 2447LL;
    day = static_cast<int>(l - 2447LL * j / 80LL);
    l = j / 11LL;
    month = static_cast<int>(j + 2LL - 12LL * l);
    year = static_cast<int>(100LL * (n - 49LL) + i + l);
    return year >= 1858 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

bool SDCDecoder::supports(Mode mode, int sdcMode)
{
    return (mode == Mode::E && (sdcMode == 0 || sdcMode == 1))
        || (mode != Mode::E && (sdcMode == 0 || sdcMode == 1));
}

int SDCDecoder::informationBitCount(Mode mode, int occupancy, int sdcMode)
{
    if (!supports(mode, sdcMode)) {
        return -1;
    }

    const int cells = static_cast<int>(CellMap::sdcCells(mode, occupancy).size());
    if (mode != Mode::E && sdcMode == 0)
    {
        const int componentUnits = (2 * cells - 12) / 3;
        return cells > 0 && componentUnits >= 0 ? 3 * componentUnits : -1;
    }

    const int denominator = sdcMode == 1 && mode == Mode::E ? 4 : 2;
    const int availableWithoutTail = 2 * cells - 12;
    if (cells <= 0 || availableWithoutTail < 0
        || availableWithoutTail % denominator != 0) {
        return -1;
    }

    return availableWithoutTail / denominator;
}

bool SDCDecoder::decode(
    Mode mode,
    int occupancy,
    int sdcMode,
    const std::vector<EqualizedSymbol>& symbols,
    SDCData& data,
    std::vector<std::uint8_t>* decodedBits,
    std::string* error
)
{
    const int informationBits = informationBitCount(mode, occupancy, sdcMode);
    const int dataBytes = Parameters::sdcDataFieldBytes(mode, occupancy, sdcMode);
    if (informationBits < 0 || dataBytes < 0)
    {
        if (error) {
            *error = "Unsupported DRM SDC modulation or parameters";
        }

        return false;
    }

    const std::vector<CellPosition>& positions = CellMap::sdcCells(mode, occupancy);
    std::vector<float> interleavedSoftBits;
    std::vector<std::complex<float>> receivedCells;
    std::vector<float> reliabilities;
    interleavedSoftBits.reserve(positions.size() * 2U);
    receivedCells.reserve(positions.size());
    reliabilities.reserve(positions.size());
    for (const CellPosition& position : positions)
    {
        if (position.symbol < 0 || static_cast<std::size_t>(position.symbol) >= symbols.size())
        {
            if (error) {
                *error = "Missing equalized DRM SDC symbol";
            }

            return false;
        }

        const EqualizedSymbol& symbol = symbols[static_cast<std::size_t>(position.symbol)];
        if (!symbol.contains(position.carrier))
        {
            if (error) {
                *error = "Missing equalized DRM SDC carrier";
            }

            return false;
        }

        const std::size_t channelIndex = static_cast<std::size_t>(
            position.carrier - symbol.minimumCarrier
        );
        // |H|, as the MSC: |H|^2 lets interference on strong carriers
        // outweigh the rest (see FACDecoder::decode).
        const float reliability = channelIndex < symbol.channel.size()
            ? std::abs(symbol.channel[channelIndex]) : 1.0f;
        const std::complex<float>& cell = symbol.cell(position.carrier);
        receivedCells.push_back(cell);
        reliabilities.push_back(reliability);
        interleavedSoftBits.push_back(cell.real() * reliability);
        interleavedSoftBits.push_back(cell.imag() * reliability);
    }

    if (mode != Mode::E && sdcMode == 0)
    {
        // ETSI ES 201 980 V4.3.1 tables 36 and figure 29. Each component
        // code produces 2*NSDC bits; level 0 uses rate 1/3 and multiplier
        // 13, while level 1 uses rate 2/3 and multiplier 21.
        const int componentUnits = (2 * static_cast<int>(positions.size()) - 12) / 3;
        const int tailResidual = (2 * static_cast<int>(positions.size()) - 12) % 3;
        constexpr float AxisScale = 0.31622776601683794f; // 1/sqrt(10)
        auto axisMetric = [=](float received, int level0, int level1) {
            static constexpr int amplitudes[2][2] = {{3, -1}, {1, -3}};
            const float difference = received
                - AxisScale * static_cast<float>(amplitudes[level0][level1]);
            return difference * difference;
        };

        std::vector<float> level0Interleaved;
        level0Interleaved.reserve(2U * positions.size());
        for (std::size_t i = 0; i < receivedCells.size(); ++i)
        {
            for (float axis : {receivedCells[i].real(), receivedCells[i].imag()})
            {
                const float distance0 = std::min(
                    axisMetric(axis, 0, 0), axisMetric(axis, 0, 1)
                );
                const float distance1 = std::min(
                    axisMetric(axis, 1, 0), axisMetric(axis, 1, 1)
                );
                level0Interleaved.push_back((distance1 - distance0) * reliabilities[i]);
            }
        }

        const std::vector<float> level0Soft = BitInterleaver::deinterleave(
            level0Interleaved, 13
        );
        std::vector<std::uint8_t> level0Bits;
        if (!ViterbiDecoder::decodePunctured(
                level0Soft, static_cast<std::size_t>(componentUnits),
                CodeRate::R1_3, tailResidual, level0Bits))
        {
            if (error) {
                *error = "DRM 16-QAM SDC level-0 decoding failed";
            }

            return false;
        }

        const std::vector<std::uint8_t> level0Coded =
            ConvolutionalCode::encodePunctured(
                level0Bits, CodeRate::R1_3, tailResidual
            );
        const std::vector<std::uint8_t> level0Mapped = BitInterleaver::interleave(
            level0Coded, 13
        );
        if (level0Mapped.size() != 2U * positions.size())
        {
            if (error) {
                *error = "Invalid DRM 16-QAM SDC level-0 size";
            }

            return false;
        }

        std::vector<float> level1Interleaved;
        level1Interleaved.reserve(2U * positions.size());
        for (std::size_t i = 0; i < receivedCells.size(); ++i)
        {
            const float axes[2] = {receivedCells[i].real(), receivedCells[i].imag()};
            for (int axisIndex = 0; axisIndex < 2; ++axisIndex)
            {
                const int level0 = level0Mapped[2U * i + static_cast<std::size_t>(axisIndex)];
                const float distance0 = axisMetric(axes[axisIndex], level0, 0);
                const float distance1 = axisMetric(axes[axisIndex], level0, 1);
                level1Interleaved.push_back(
                    (distance1 - distance0) * reliabilities[i]
                );
            }
        }

        const std::vector<float> level1Soft = BitInterleaver::deinterleave(
            level1Interleaved, 21
        );
        std::vector<std::uint8_t> level1Bits;
        if (!ViterbiDecoder::decodePunctured(
                level1Soft, static_cast<std::size_t>(2 * componentUnits),
                CodeRate::R2_3, tailResidual, level1Bits))
        {
            if (error) {
                *error = "DRM 16-QAM SDC level-1 decoding failed";
            }

            return false;
        }

        std::vector<std::uint8_t> bits = level0Bits;
        bits.insert(bits.end(), level1Bits.begin(), level1Bits.end());
        EnergyDispersal::apply(bits);
        if (!SDCParser::parseBlock(
                bits, static_cast<std::size_t>(dataBytes), data, error))
        {
            if (decodedBits) {
                *decodedBits = bits;
            }

            return false;
        }

        if (decodedBits) {
            *decodedBits = std::move(bits);
        }

        return true;
    }

    const CodeRate rate = sdcMode == 1 && mode == Mode::E
        ? CodeRate::R1_4 : CodeRate::R1_2;
    const std::vector<float> finalSoftBits = BitInterleaver::deinterleave(interleavedSoftBits, 21);
    std::vector<std::uint8_t> bits;
    if (!ViterbiDecoder::decodeSDC(finalSoftBits, static_cast<std::size_t>(informationBits), rate, bits))
    {
        if (error) {
            *error = "DRM SDC Viterbi decoding failed";
        }

        return false;
    }

    std::vector<std::uint8_t> firstPassBits = bits;
    EnergyDispersal::apply(firstPassBits);
    std::string firstPassError;
    bool firstPassCRCValid = false;
    if (SDCParser::parseBlock(
            firstPassBits, static_cast<std::size_t>(dataBytes), data,
            &firstPassError, false, &firstPassCRCValid))
    {
        if (decodedBits) {
            *decodedBits = std::move(firstPassBits);
        }

        return true;
    }

    if (firstPassCRCValid)
    {
        // The maximum-likelihood path already matched the CRC; its data field
        // is structurally malformed. Other list paths would only be accepted
        // by coincidence of the CRC, so do not search them.
        if (decodedBits) {
            *decodedBits = std::move(firstPassBits);
        }

        if (error) {
            *error = firstPassError;
        }

        return false;
    }

    std::vector<std::vector<std::uint8_t>> candidates;
    if (!ViterbiDecoder::decodeSDCList(
            finalSoftBits,
            static_cast<std::size_t>(informationBits),
            rate,
            32U,
            candidates))
    {
        if (error) {
            *error = "DRM SDC list Viterbi decoding failed";
        }

        return false;
    }

    for (std::vector<std::uint8_t>& candidate : candidates)
    {
        EnergyDispersal::apply(candidate);
        SDCData candidateData;
        std::string candidateError;
        // List candidates must also be fully plausible: reserved values, rfu
        // bits or unusable entities reject a candidate that matched the CRC.
        if (SDCParser::parseBlock(
                candidate,
                static_cast<std::size_t>(dataBytes),
                candidateData,
                &candidateError, true)
            && plausibleAudio(mode, candidateData.audio)
            && plausibleAudio(mode, candidateData.nextAudio))
        {
            data = std::move(candidateData);
            data.listDecoded = true;
            if (decodedBits) {
                *decodedBits = candidate;
            }

            return true;
        }
    }

    if (decodedBits && !candidates.empty()) {
        *decodedBits = candidates.front();
    }

    if (error)
    {
        double errorPower = 0.0;
        double totalWeight = 0.0;
        constexpr float QPSKAxis = 0.7071067811865475f;
        for (std::size_t i = 0; i < receivedCells.size(); ++i)
        {
            const float magnitude = std::abs(receivedCells[i]);
            if (magnitude <= 1.0e-12f) {
                continue;
            }

            const std::complex<float> normalized = receivedCells[i] / magnitude;
            const std::complex<float> decision(
                normalized.real() >= 0.0f ? QPSKAxis : -QPSKAxis,
                normalized.imag() >= 0.0f ? QPSKAxis : -QPSKAxis
            );
            errorPower += reliabilities[i] * std::norm(normalized - decision);
            totalWeight += reliabilities[i];
        }

        const double evm = totalWeight > 0.0
            ? std::sqrt(errorPower / totalWeight) : 0.0;
        *error = firstPassError.empty()
            ? "DRM SDC CRC mismatch" : firstPassError;
        *error += " (QPSK decision EVM " + std::to_string(evm) + ")";
    }

    return false;
}

} // namespace modemdrm
