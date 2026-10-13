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

#include "ltegold.h"

void LTEGold::generate(uint32_t cInit, int length, std::vector<uint8_t>& c)
{
    const int Nc = 1600;
    // Bit n of each register holds x(n+i) for the current position i
    uint32_t x1 = 1;
    uint32_t x2 = cInit & 0x7fffffff;

    c.resize(length);
    for (int n = 0; n < Nc + length; n++)
    {
        if (n >= Nc) {
            c[n - Nc] = (x1 ^ x2) & 1;
        }
        // x1(n+31) = x1(n+3) + x1(n)
        uint32_t b1 = ((x1 >> 3) ^ x1) & 1;
        // x2(n+31) = x2(n+3) + x2(n+2) + x2(n+1) + x2(n)
        uint32_t b2 = ((x2 >> 3) ^ (x2 >> 2) ^ (x2 >> 1) ^ x2) & 1;
        x1 = (x1 >> 1) | (b1 << 30);
        x2 = (x2 >> 1) | (b2 << 30);
    }
}
