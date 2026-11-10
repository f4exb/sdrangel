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

#include <algorithm>

#include "dvbits.h"

void DVBits::bytesToBitsMSB(const uint8_t *bytes, int nbBytes, uint8_t *bits)
{
    for (int i = 0; i < nbBytes; i++)
    {
        for (int b = 0; b < 8; b++) {
            bits[8*i + b] = (bytes[i] >> (7 - b)) & 1;
        }
    }
}

void DVBits::bytesToBitsLSB(const uint8_t *bytes, int nbBytes, uint8_t *bits)
{
    for (int i = 0; i < nbBytes; i++)
    {
        for (int b = 0; b < 8; b++) {
            bits[8*i + b] = (bytes[i] >> b) & 1;
        }
    }
}

void DVBits::bitsToBytesMSB(const uint8_t *bits, int nbBits, uint8_t *bytes)
{
    std::fill(bytes, bytes + (nbBits + 7) / 8, 0);

    for (int i = 0; i < nbBits; i++)
    {
        if (bits[i] & 1) {
            bytes[i / 8] |= 1 << (7 - (i % 8));
        }
    }
}

void DVBits::bitsToBytesLSB(const uint8_t *bits, int nbBits, uint8_t *bytes)
{
    std::fill(bytes, bytes + (nbBits + 7) / 8, 0);

    for (int i = 0; i < nbBits; i++)
    {
        if (bits[i] & 1) {
            bytes[i / 8] |= 1 << (i % 8);
        }
    }
}

int DVBits::putBits(uint8_t *bits, int pos, uint64_t value, int nbBits)
{
    for (int i = nbBits - 1; i >= 0; i--) {
        bits[pos++] = (value >> i) & 1;
    }

    return pos;
}

uint64_t DVBits::getBits(const uint8_t *bits, int pos, int nbBits)
{
    uint64_t value = 0;

    for (int i = 0; i < nbBits; i++) {
        value = (value << 1) | (bits[pos + i] & 1);
    }

    return value;
}

void DVBits::bitsToHexbits(const uint8_t *bits, int nbHexbits, uint8_t *hexbits)
{
    for (int i = 0; i < nbHexbits; i++) {
        hexbits[i] = (uint8_t) getBits(bits, 6 * i, 6);
    }
}

void DVBits::hexbitsToBits(const uint8_t *hexbits, int nbHexbits, uint8_t *bits)
{
    for (int i = 0; i < nbHexbits; i++) {
        putBits(bits, 6 * i, hexbits[i] & 0x3F, 6);
    }
}

void DVBits::bitsToDibits(const uint8_t *bits, int nbDibits, uint8_t *dibits)
{
    for (int i = 0; i < nbDibits; i++) {
        dibits[i] = (uint8_t) (((bits[2*i] & 1) << 1) | (bits[2*i + 1] & 1));
    }
}

void DVBits::dibitsToBits(const uint8_t *dibits, int nbDibits, uint8_t *bits)
{
    for (int i = 0; i < nbDibits; i++)
    {
        bits[2*i] = (dibits[i] >> 1) & 1;
        bits[2*i + 1] = dibits[i] & 1;
    }
}

void DVBits::llrToBits(const float *llr, int nbBits, uint8_t *bits)
{
    for (int i = 0; i < nbBits; i++) {
        bits[i] = llr[i] < 0.0f ? 1 : 0;
    }
}

void DVBits::bitsToLlr(const uint8_t *bits, int nbBits, float *llr, float magnitude)
{
    for (int i = 0; i < nbBits; i++) {
        llr[i] = (bits[i] & 1) ? -magnitude : magnitude;
    }
}

int DVBits::popcount(uint32_t v)
{
    int n = 0;

    for (; v; v >>= 1) {
        n += v & 1;
    }

    return n;
}

int DVBits::popcount64(uint64_t v)
{
    int n = 0;

    for (; v; v >>= 1) {
        n += (int) (v & 1);
    }

    return n;
}
