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

#ifndef INCLUDE_LTEGOLD_H
#define INCLUDE_LTEGOLD_H

#include <cstdint>
#include <vector>

#include "export.h"

// Length-31 Gold sequence pseudo-random generator, as used for scrambling in LTE.
// See 3GPP TS 36.211 section 7.2.
class SDRBASE_API LTEGold
{
public:
    // Generate length bits c(0)..c(length-1) (one bit per byte) for initialisation value cInit
    static void generate(uint32_t cInit, int length, std::vector<uint8_t>& c);
};

#endif // INCLUDE_LTEGOLD_H
