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

#ifndef MODEMDRM_DRMINTERLEAVER_H_
#define MODEMDRM_DRMINTERLEAVER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "export.h"

namespace modemdrm
{

class MODEMDRM_API BitInterleaver
{
public:
    static std::vector<std::size_t> permutation(std::size_t inputSize, int multiplier);
    // As permutation(), computed once per size and multiplier. The reference
    // stays valid until the calling thread's next call.
    static const std::vector<std::size_t>& cachedPermutation(std::size_t inputSize, int multiplier);
    static std::vector<std::uint8_t> interleave(
        const std::vector<std::uint8_t>& input,
        int multiplier
    );
    static std::vector<float> deinterleave(
        const std::vector<float>& input,
        int multiplier
    );
};

} // namespace modemdrm

#endif // MODEMDRM_DRMINTERLEAVER_H_
