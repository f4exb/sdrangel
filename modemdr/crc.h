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

#ifndef MODEMDR_CRC_H_
#define MODEMDR_CRC_H_

#include <cstddef>
#include <cstdint>

#include "export.h"

namespace modemdr
{

class MODEMDR_API CRC
{
public:
    // CRC-16 CCITT (x^16 + x^12 + x^5 + 1, preset to ones, inverted), as DAB and DRM use for
    // FIBs, MSC data groups, packets and Journaline data units.
    static std::uint16_t crc16(const std::uint8_t *data, std::size_t length);
    // Whether the last two bytes are the CRC-16 of those before them
    static bool crc16Check(const std::uint8_t *data, std::size_t lengthWithCRC);
    // CRC-32 of gzip (RFC 1952 section 8)
    static std::uint32_t crc32(const std::uint8_t *data, std::size_t length);
};

} // namespace modemdr

#endif // MODEMDR_CRC_H_
