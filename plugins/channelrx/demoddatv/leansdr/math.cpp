///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019, 2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>             //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include "math.h"
#include <bitset>

namespace leansdr
{

int hamming_weight(uint8_t x)
{
    static const int lut[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
    return lut[x & 15] + lut[x >> 4];
}

int hamming_weight(uint16_t x)
{
    return hamming_weight((uint8_t)x) + hamming_weight((uint8_t)(x >> 8));
}

int hamming_weight(uint32_t x)
{
    return std::bitset<32>(x).count();
    // x = x - ((x >> 1) & 0x55555555);
    // x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    // return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
//    return hamming_weight((uint16_t)x) + hamming_weight((uint16_t)(x >> 16));
}

int hamming_weight(uint64_t x)
{
    return std::bitset<64>(x).count();
    // x = x - ((x >> 1) & 0x5555555555555555);
    // x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
    // return (((x + (x >> 4)) & 0x0F0F0F0F0F0F0F0F) * 0x0101010101010101) >> 56;
//    return hamming_weight((uint32_t)x) + hamming_weight((uint32_t)(x >> 32));
}

unsigned char parity(uint8_t x)
{
    x ^= x >> 4;
    return (0x6996 >> (x & 15)) & 1; // 16-entry look-up table
}

unsigned char parity(uint16_t x)
{
    return parity((uint8_t)(x ^ (x >> 8)));
}

unsigned char parity(uint32_t x)
{
    return parity((uint16_t)(x ^ (x >> 16)));
}

unsigned char parity(uint64_t x)
{
    return parity((uint32_t)(x ^ (x >> 32)));
}

int log2i(uint64_t x)
{
    int n = -1;
    for (; x; ++n, x >>= 1);
    return n;
}

} // leansdr
