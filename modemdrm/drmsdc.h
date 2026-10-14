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

#ifndef MODEMDRM_DRMSDC_H_
#define MODEMDRM_DRMSDC_H_

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "export.h"
#include "drmchannelestimator.h"
#include "drmparameters.h"

namespace modemdrm
{

struct MODEMDRM_API SDCEntity
{
    int type = -1;
    bool version = false;
    int lengthBytes = 0;
    std::vector<std::uint8_t> bodyBits;
};

struct MODEMDRM_API SDCStreamDescription
{
    int streamId = 0;
    int partALengthBytes = 0;
    int partBLengthBytes = 0;
};

struct MODEMDRM_API SDCMultiplexDescription
{
    bool present = false;
    bool nextConfiguration = false;
    int protectionLevelA = 0;
    int protectionLevelB = 0;
    std::vector<SDCStreamDescription> streams;
};

struct MODEMDRM_API SDCLabel
{
    int shortId = 0;
    // Clause 6.4.3.2 text control field (0 when absent: LTR, no contextual
    // or combining characters). It is carried as a leading 0x01..0x0F byte.
    int textControl = 0;
    std::string utf8;
};

struct MODEMDRM_API SDCTimeDate
{
    bool present = false;
    int modifiedJulianDate = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    bool localTimeOffsetPresent = false;
    int localTimeOffsetMinutes = 0;
};

struct MODEMDRM_API SDCAudioInformation
{
    bool nextConfiguration = false;
    int shortId = 0;
    int streamId = 0;
    int audioCoding = 0;
    bool sbr = false;
    int audioMode = 0;
    int samplingRateCode = 0;
    bool text = false;
    bool enhancement = false;
    int coderField = 0;
    int surroundMode = 0;
    std::vector<std::uint8_t> codecSpecificConfig;
};

struct MODEMDRM_API SDCLanguageCountry
{
    int shortId = 0;
    std::string language;
    std::string country;
};

struct MODEMDRM_API SDCAlternativeFrequencyEntity
{
    int type = -1;
    bool listVersion = false;
    int extension = -1;
    bool commonHeaderPresent = false;
    bool synchronousMultiplex = false;
    bool baseLayer = false;
    bool appliesToAllServices = false;
    bool regionOrScheduleRestricted = false;
    // Entity 3 optional 8-bit fields (clause 6.4.3.4); -1 when absent.
    int serviceRestrictionField = -1;
    int regionScheduleField = -1;
    std::vector<std::uint16_t> frequencyCodes;
    std::vector<std::uint8_t> bodyBits;
};

// Entity 5 (clause 6.4.3.6): how a service's data component is carried.
struct MODEMDRM_API SDCDataApplication
{
    // DAB-domain user application types (ETSI TS 101 756 table 16).
    static constexpr int MOTSlideshow = 0x002;
    static constexpr int BroadcastWebsite = 0x003;
    static constexpr int Journaline = 0x44a;

    int shortId = 0;
    int streamId = 0;
    bool packetMode = false;
    bool dataUnits = false;        // packet mode: data units span several packets
    int packetId = 0;
    bool enhancement = false;      // an enhancement layer carries more of the application
    int applicationDomain = -1;    // 0 DRM, 1 DAB (ES 201 980 clause 6.4.3.6)
    int packetLengthBytes = 0;     // packet data field, without header and CRC
    int userApplicationType = -1;  // DAB domain only
    std::vector<std::uint8_t> applicationData; // after the user application type
};

struct MODEMDRM_API SDCData
{
    // Raw entities are diagnostics, not a history of every received version.
    // Keep current/next known entities and bounded optional signalling.
    static constexpr std::size_t MaximumRawEntities = 512U;
    static constexpr std::size_t MaximumAFSEntities = 512U;
    bool valid = false;
    int afsIndex = 0;
    // Entities in a CRC-valid block whose fields could not be used (reserved
    // or rfu values, bad lengths). They are skipped; the rest of the block is kept.
    int invalidEntityCount = 0;
    SDCMultiplexDescription multiplex;
    SDCMultiplexDescription nextMultiplex;
    // A block was accepted from the SDC list decoder, not the
    // maximum-likelihood path, so its CRC match may be a coincidence. Set on
    // the decoded block and carried by an aggregate merged from it.
    bool listDecoded = false;
    // A list-decoded block that contradicted the live database (its multiplex
    // and AFS index, or a service's audio descriptor), held until another
    // block confirms it.
    SDCMultiplexDescription unconfirmedMultiplex;
    int unconfirmedAFSIndex = -1;
    std::vector<SDCAudioInformation> unconfirmedAudio;
    std::vector<SDCLabel> labels;
    SDCTimeDate timeDate;
    std::vector<SDCAudioInformation> audio;
    std::vector<SDCAudioInformation> nextAudio;
    std::vector<SDCLanguageCountry> languageCountries;
    std::vector<SDCAlternativeFrequencyEntity> alternativeFrequencies;
    std::vector<SDCDataApplication> dataApplications; // version-0 entity 5 only
    std::vector<SDCEntity> entities;
};

class MODEMDRM_API SDCParser
{
public:
    // Merge one CRC-valid SDC block into the database accumulated for the
    // current AFS-index cycle. Version-1 reconfiguration entities are
    // retained raw but do not replace the live version-0 configuration.
    // Returns false when the block was not applied: invalid, or a list-decoded
    // block held until another confirms it.
    static bool mergeBlock(SDCData& aggregate, const SDCData& block);

    // Atomically promotes a complete pending multiplex and its audio
    // descriptors at the FAC-signalled reconfiguration boundary.
    static bool promoteNextConfiguration(SDCData& data);

    // A CRC-valid block fails only for structural errors (a truncated entity
    // header or an entity exceeding the data field). An entity with unusable
    // field values is skipped and counted in SDCData::invalidEntityCount; with
    // strict set, such an entity rejects the block instead. crcValid, when
    // given, reports whether the CRC matched, whatever the parse outcome.
    static bool parseBlock(
        const std::vector<std::uint8_t>& decodedBits,
        std::size_t dataFieldByteCount,
        SDCData& data,
        std::string* error = nullptr,
        bool strict = false,
        bool* crcValid = nullptr
    );

    static bool parseDataField(
        int afsIndex,
        const std::vector<std::uint8_t>& bytes,
        SDCData& data,
        std::string* error = nullptr,
        bool strict = false
    );

    static bool modifiedJulianDateToGregorian(
        int modifiedJulianDate,
        int& year,
        int& month,
        int& day
    );
};

class MODEMDRM_API SDCDecoder
{
public:
    static bool supports(Mode mode, int sdcMode);
    static int informationBitCount(Mode mode, int occupancy, int sdcMode);
    static bool decode(
        Mode mode,
        int occupancy,
        int sdcMode,
        const std::vector<EqualizedSymbol>& symbols,
        SDCData& data,
        std::vector<std::uint8_t>* decodedBits = nullptr,
        std::string* error = nullptr
    );
};

} // namespace modemdrm

#endif // MODEMDRM_DRMSDC_H_
