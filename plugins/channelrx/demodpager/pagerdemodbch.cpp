///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include "pagerdemodbch.h"

// XOR bits together for parity check
int PagerDemodBCH::xorBits(quint32 word, int firstBit, int lastBit)
{
    int x = 0;
    for (int i = firstBit; i <= lastBit; i++)
    {
        x ^= (word >> i) & 1;
    }
    return x;
}

// Check for even parity
bool PagerDemodBCH::evenParity(quint32 word, int firstBit, int lastBit, int parityBit)
{
    return xorBits(word, firstBit, lastBit) == parityBit;
}

// Reverse order of bits
quint32 PagerDemodBCH::reverse(quint32 x)
{
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
    return((x >> 16) | (x << 16));
}

// Calculate BCH parity bits
quint32 PagerDemodBCH::encode(const quint32 cw)
{
    quint32 bit = 0;
    quint32 localCW = cw & 0xFFFFF800;  // Mask off BCH parity and even parity bits
    quint32 cwE = localCW;

    // Calculate BCH bits
    for (bit = 1; bit <= 21; bit++)
    {
        if (cwE & 0x80000000) {
            cwE ^= 0xED200000;
        }
        cwE <<= 1;
    }
    localCW |= (cwE >> 21);

    return localCW;
}

// Use BCH decoding to try to fix any bit errors
// Returns true if able to be decode/repair successful
// See: https://www.eevblog.com/forum/microcontrollers/practical-guides-to-bch-fec/
bool PagerDemodBCH::decode(const quint32 cw, quint32& correctedCW)
{
    // Calculate syndrome
    // We do this by recalculating the BCH parity bits and XORing them against the received ones
    quint32 syndrome = ((encode(cw) ^ cw) >> 1) & 0x3FF;

    if (syndrome == 0)
    {
        // Syndrome of zero indicates no repair required
        correctedCW = cw;
        return true;
    }

    // Meggitt decoder

    quint32 result = 0;
    quint32 damagedCW = cw;

    // Calculate BCH bits
    for (quint32 xbit = 0; xbit < 31; xbit++)
    {
        // Produce the next corrected bit in the high bit of the result
        result <<= 1;
        if ((syndrome == 0x3B4) ||      // 0x3B4: Syndrome when a single error is detected in the MSB
            (syndrome == 0x26E) ||      // 0x26E: Two adjacent errors
            (syndrome == 0x359) ||      // 0x359: Two errors, one OK bit between
            (syndrome == 0x076) ||      // 0x076: Two errors, two OK bits between
            (syndrome == 0x255) ||      // 0x255: Two errors, three OK bits between
            (syndrome == 0x0F0) ||      // 0x0F0: Two errors, four OK bits between
            (syndrome == 0x216) ||
            (syndrome == 0x365) ||
            (syndrome == 0x068) ||
            (syndrome == 0x25A) ||
            (syndrome == 0x343) ||
            (syndrome == 0x07B) ||
            (syndrome == 0x1E7) ||
            (syndrome == 0x129) ||
            (syndrome == 0x14E) ||
            (syndrome == 0x2C9) ||
            (syndrome == 0x0BE) ||
            (syndrome == 0x231) ||
            (syndrome == 0x0C2) ||
            (syndrome == 0x20F) ||
            (syndrome == 0x0DD) ||
            (syndrome == 0x1B4) ||
            (syndrome == 0x2B4) ||
            (syndrome == 0x334) ||
            (syndrome == 0x3F4) ||
            (syndrome == 0x394) ||
            (syndrome == 0x3A4) ||
            (syndrome == 0x3BC) ||
            (syndrome == 0x3B0) ||
            (syndrome == 0x3B6) ||
            (syndrome == 0x3B5)
           )
        {
            // Syndrome matches an error in the MSB
            // Correct that error and adjust the syndrome to account for it
            syndrome ^= 0x3B4;
            result |= (~damagedCW & 0x80000000) >> 30;
        }
        else
        {
            // No error
            result |= (damagedCW & 0x80000000) >> 30;
        }
        damagedCW <<= 1;

        // Handle syndrome shift register feedback
        if (syndrome & 0x200)
        {
            syndrome <<= 1;
            syndrome ^= 0x769;  // 0x769 = POCSAG generator polynomial -- x^10 + x^9 + x^8 + x^6 + x^5 + x^3 + 1
        }
        else
        {
            syndrome <<= 1;
        }
        // Mask off bits which fall off the end of the syndrome shift register
        syndrome &= 0x3FF;
    }

    // Check if error correction was successful
    if (syndrome != 0)
    {
        // Syndrome nonzero at end indicates uncorrectable errors
        correctedCW = cw;
        return false;
    }

    // The BCH decoder operates on bits 31 through 1. Preserve the separate
    // overall parity bit in bit 0 so it can be checked after correction.
    correctedCW = result | (cw & 0x1);
    return true;
}
