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

#ifndef MODEMDRM_DRMBITUTILS_H_
#define MODEMDRM_DRMBITUTILS_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "export.h"

namespace modemdrm
{

class MODEMDRM_API EnergyDispersal
{
public:
    // Generates the degree-9 PRBS from ETSI ES 201 980 clause 7.2.2.
    // Bits are stored as bytes containing exactly 0 or 1.
    static std::vector<std::uint8_t> sequence(std::size_t length);

    // Scrambling and descrambling are the same modulo-2 operation. Each call
    // is one FAC/SDC/MSC block: the PRBS starts from the all-ones state.
    static void apply(std::vector<std::uint8_t>& bits);
};

class MODEMDRM_API CRC
{
public:
    // DRM CRCs use an all-ones initial register, MSb-first input, and transmit
    // the one's complement of the final register (normative annex D).
    static std::uint32_t compute(
        const std::uint8_t* bits,
        std::size_t bitCount,
        int degree,
        std::uint32_t polynomialWithoutLeadingTerm
    );

    static std::uint8_t crc8(const std::uint8_t* bits, std::size_t bitCount);
    static std::uint16_t crc16(const std::uint8_t* bits, std::size_t bitCount);
    static std::uint8_t crc8Bytes(const std::uint8_t* bytes, std::size_t byteCount);
    static std::uint16_t crc16Bytes(const std::uint8_t* bytes, std::size_t byteCount);
};

} // namespace modemdrm

#endif // MODEMDRM_DRMBITUTILS_H_
