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

#ifndef MODEMDRM_DRMCONVOLUTIONAL_H_
#define MODEMDRM_DRMCONVOLUTIONAL_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "export.h"

namespace modemdrm
{

enum class CodeRate
{
    R1_6,
    R1_4,
    R1_3,
    R2_5,
    R1_2,
    R4_7,
    R3_5,
    R2_3,
    R3_4,
    R4_5,
    R7_8,
    R8_9
};

class MODEMDRM_API ConvolutionalCode
{
public:
    static constexpr int ConstraintLength = 7;
    static constexpr int TailBitCount = ConstraintLength - 1;

    static std::vector<std::uint8_t> encodeMother(const std::vector<std::uint8_t>& inputBits);
    static std::vector<std::uint8_t> encode(
        const std::vector<std::uint8_t>& informationBits,
        CodeRate rate
    );
    static std::size_t puncturedBitCount(std::size_t informationBitCount, CodeRate rate);
    static std::vector<std::uint8_t> encodePunctured(
        const std::vector<std::uint8_t>& informationBits,
        CodeRate rate,
        int tailResidual
    );
    static std::vector<std::uint8_t> encodeUEP(
        const std::vector<std::uint8_t>& highProtectionBits,
        CodeRate highProtectionRate,
        const std::vector<std::uint8_t>& lowProtectionBits,
        CodeRate lowProtectionRate,
        int tailResidual
    );
};

class MODEMDRM_API ViterbiDecoder
{
public:
    // Positive soft values favour a transmitted zero; negative values favour one.
    static bool decode(
        const std::vector<float>& softBits,
        std::size_t informationBitCount,
        CodeRate rate,
        std::vector<std::uint8_t>& informationBits
    );
    static bool decodeSDC(
        const std::vector<float>& softBits,
        std::size_t informationBitCount,
        CodeRate rate,
        std::vector<std::uint8_t>& informationBits
    );
    static bool decodePunctured(
        const std::vector<float>& softBits,
        std::size_t informationBitCount,
        CodeRate rate,
        int tailResidual,
        std::vector<std::uint8_t>& informationBits
    );
    static bool decodeUEP(
        const std::vector<float>& softBits,
        std::size_t highProtectionBitCount,
        CodeRate highProtectionRate,
        std::size_t lowProtectionBitCount,
        CodeRate lowProtectionRate,
        int tailResidual,
        std::vector<std::uint8_t>& informationBits
    );
    static bool decodeSDCList(
        const std::vector<float>& softBits,
        std::size_t informationBitCount,
        CodeRate rate,
        std::size_t maximumCandidates,
        std::vector<std::vector<std::uint8_t>>& candidates
    );
};

} // namespace modemdrm

#endif // MODEMDRM_DRMCONVOLUTIONAL_H_
