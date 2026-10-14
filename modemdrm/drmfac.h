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

#ifndef MODEMDRM_DRMFAC_H_
#define MODEMDRM_DRMFAC_H_

#include <cstdint>
#include <vector>

#include "export.h"
#include "drmchannelestimator.h"

namespace modemdrm
{

struct MODEMDRM_API FACService
{
    std::uint32_t serviceIdentifier = 0;
    int shortIdentifier = 0;
    bool audioConditionalAccess = false;
    int language = 0;
    bool dataService = false;
    int descriptor = 0;
    bool dataConditionalAccess = false;
};

struct MODEMDRM_API FACData
{
    bool baseLayer = true;
    int identity = 0;
    bool modeE = false;
    int occupancy = 0;
    bool shortInterleaving = false;
    int mscMode = 0;
    int sdcMode = 0;
    int serviceCountCode = 0;
    int reconfigurationIndex = 0;
    bool toggle = false;
    // Decoded after removing a scrambled code tail (early DRM+ transmitters).
    bool legacyModeETail = false;
    std::vector<FACService> services;
    // The services of the multiplex, audio and data, from serviceCountCode.
    int serviceCount() const;
};

// FAC service parameters rotate independently of transmission-superframe
// boundaries. Retain at most the four short identifiers within one RF epoch.
class MODEMDRM_API FACServiceCache
{
public:
    void reset();
    // Call only for a fresh, validated FAC block. Returns true if a service
    // count change or a reconfiguration boundary invalidated the old mapping.
    bool observe(const FACData& block);
    const std::vector<FACService>& services() const { return m_services; }
    const FACService* findIdentifier(std::uint32_t identifier) const;
    const FACService* findShortIdentifier(int identifier) const;

private:
    std::vector<FACService> m_services;
    int m_serviceCountCode = -1;
    int m_reconfigurationIndex = -1;
};

class MODEMDRM_API FACDecoder
{
public:
    static int informationBitCount(Mode mode);
    static bool decodeBits(
        Mode mode,
        std::vector<std::uint8_t> bits,
        FACData& data
    );
    // allowLegacyModeETail: in mode E, also try the scrambled-tail removal
    // of the early DRM+ transmitters (FACData::legacyModeETail).
    static bool decode(
        Mode mode,
        const std::vector<EqualizedSymbol>& symbols,
        FACData& data,
        std::vector<std::uint8_t>* decodedBits = nullptr,
        bool allowLegacyModeETail = false
    );
};

} // namespace modemdrm

#endif // MODEMDRM_DRMFAC_H_
