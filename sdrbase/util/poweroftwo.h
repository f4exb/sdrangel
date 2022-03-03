///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_POWEROFTWO_H
#define INCLUDE_POWEROFTWO_H

#include <cstdint>

// Is x a power of two
// From: https://stackoverflow.com/questions/600293/how-to-check-if-a-number-is-a-power-of-2
inline bool isPowerOfTwo(uint32_t x)
{
    return (x & (x - 1)) == 0;
}

// Calculate next power of 2 lower than x
// From: https://stackoverflow.com/questions/2679815/previous-power-of-2
inline uint32_t lowerPowerOfTwo(uint32_t x)
{
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    return x - (x >> 1);
}

#endif /* INCLUDE_POWEROFTWO_H */
