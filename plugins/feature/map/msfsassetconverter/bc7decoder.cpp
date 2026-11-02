///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// BC7 decoding logic adapted from Basis Universal's basisu_gpu_texture.cpp.     //
// Copyright (C) 2019-2021 Binomial LLC. Licensed under Apache-2.0; see the      //
// repository's LICENSE.md. The partition constants are defined by BPTC/BC7.     //
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

#include "bc7decoder.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace {

// BC7 partition and anchor tables from the public BPTC format specification.
constexpr uint8_t Partition2[64 * 16] = {
    0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0,
    0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0,
    0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1,
    0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1,
    1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0,
    1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0,
    0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1,
    1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0,
    0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0,
    1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1,
    0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1,
    1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0,
    0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1,
    0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
    1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0,
    1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0,
    1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0,
    1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1,
    0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0,
    0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1,
    1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0,
    1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1};

constexpr uint8_t Partition3[64 * 16] = {
    0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 1, 2, 2, 2, 2, 0, 0, 0, 1, 0, 0, 1, 1, 2,
    2, 1, 1, 2, 2, 2, 1, 0, 0, 0, 0, 2, 0, 0, 1, 2, 2, 1, 1, 2, 2, 1, 1, 0, 2,
    2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2,
    2, 1, 1, 2, 2, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 2, 2,
    0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 1, 1, 2,
    2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
    2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2,
    0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 2, 2, 0, 1, 2, 2, 0, 1, 2, 2, 0, 1, 2, 2, 0,
    0, 1, 1, 0, 1, 1, 2, 1, 1, 2, 2, 1, 2, 2, 2, 0, 0, 1, 1, 2, 0, 0, 1, 2, 2,
    0, 0, 2, 2, 2, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 2, 1, 1, 2, 2, 0, 1, 1,
    1, 0, 0, 1, 1, 2, 0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2,
    1, 1, 2, 2, 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1, 0, 1, 1, 1, 0,
    1, 1, 1, 0, 2, 2, 2, 0, 2, 2, 2, 0, 0, 0, 1, 0, 0, 0, 1, 2, 2, 2, 1, 2, 2,
    2, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 0, 0, 1, 1, 0,
    0, 2, 2, 1, 0, 2, 2, 1, 0, 0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1, 0, 0, 0, 0,
    0, 0, 1, 2, 0, 0, 1, 2, 1, 1, 2, 2, 2, 2, 2, 2, 0, 1, 1, 0, 1, 2, 2, 1, 1,
    2, 2, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 2, 2, 1, 1, 2, 2, 1, 0, 0,
    2, 2, 1, 1, 0, 2, 1, 1, 0, 2, 0, 0, 2, 2, 0, 1, 1, 0, 0, 1, 1, 0, 2, 0, 0,
    2, 2, 2, 2, 2, 0, 0, 1, 1, 0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1, 0, 0, 0, 0,
    2, 0, 0, 0, 2, 2, 1, 1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 2, 1,
    2, 2, 2, 0, 2, 2, 2, 0, 0, 2, 2, 0, 0, 1, 2, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0,
    1, 2, 0, 0, 2, 2, 0, 2, 2, 2, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2,
    0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 1, 2, 0, 1, 2, 0, 1,
    2, 0, 1, 2, 0, 1, 2, 0, 0, 1, 2, 0, 2, 0, 1, 2, 1, 2, 0, 1, 0, 1, 2, 0, 0,
    0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0,
    0, 0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1, 0, 0, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2,
    1, 1, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 1, 1, 0, 2, 2, 0, 1,
    2, 2, 1, 0, 2, 2, 0, 1, 2, 2, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1,
    0, 1, 0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 2, 2, 2, 2, 0, 2, 2, 2, 0, 1, 1, 1, 0, 2, 2, 2, 0, 1, 1, 1,
    0, 0, 0, 2, 1, 1, 1, 2, 0, 0, 0, 2, 1, 1, 1, 2, 0, 0, 0, 0, 2, 1, 1, 2, 2,
    1, 1, 2, 2, 1, 1, 2, 0, 2, 2, 2, 0, 1, 1, 1, 0, 1, 1, 1, 0, 2, 2, 2, 0, 0,
    0, 2, 1, 1, 1, 2, 1, 1, 1, 2, 0, 0, 0, 2, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1,
    0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 1, 2, 0, 1, 1, 0,
    0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 1, 1, 0,
    0, 2, 2, 0, 0, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0,
    1, 0, 2, 2, 2, 1, 2, 2, 2, 0, 2, 2, 2, 1, 2, 2, 2, 0, 1, 0, 1, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 1, 1, 2, 0, 1, 1, 2, 2, 0, 1, 2, 2, 2, 0};

constexpr uint8_t Anchor2[64] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 2, 8, 2, 2, 8, 8, 15, 2, 8, 2, 2, 8, 8, 2, 2,
    15, 15, 6, 8, 2, 8, 15, 15, 2, 8, 2, 2, 2, 15, 15, 6,
    6, 2, 6, 8, 15, 15, 2, 2, 15, 15, 15, 15, 15, 2, 2, 15};
constexpr uint8_t Anchor31[64] = {
    3, 3, 15, 15, 8, 3, 15, 15, 8, 8, 6, 6, 6, 5, 3, 3,
    3, 3, 8, 15, 3, 3, 6, 10, 5, 8, 8, 6, 8, 5, 15, 15,
    8, 15, 3, 5, 6, 10, 8, 15, 15, 3, 15, 5, 15, 15, 15, 15,
    3, 15, 5, 5, 5, 8, 5, 10, 5, 10, 8, 13, 15, 12, 3, 3};
constexpr uint8_t Anchor32[64] = {
    15, 8, 8, 3, 15, 15, 3, 8, 15, 15, 15, 15, 15, 15, 15, 8,
    15, 8, 15, 3, 15, 8, 15, 8, 3, 15, 6, 10, 15, 15, 10, 8,
    15, 3, 15, 10, 10, 8, 9, 10, 6, 15, 8, 15, 3, 6, 6, 8,
    15, 3, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 3, 15, 15, 8};
constexpr int Weights2[4] = {0, 21, 43, 64};
constexpr int Weights3[8] = {0, 9, 18, 27, 37, 46, 55, 64};
constexpr int Weights4[16] = {0, 4, 9, 13, 17, 21, 26, 30,
                              34, 38, 43, 47, 51, 55, 60, 64};

uint32_t readBits(const uchar *block, int &offset, int count)
{
    uint32_t value = 0;
    for (int bit = 0; bit < count; ++bit) {
        value |= ((block[(offset + bit) / 8] >> ((offset + bit) & 7)) & 1U) << bit;
    }
    offset += count;
    return value;
}

int dequant(int value, int bits)
{
    value <<= 8 - bits;
    return value | (value >> bits);
}
int dequant(int value, int pbit, int bits)
{
    return dequant((value << 1) | pbit, bits + 1);
}
int interpolate(int low, int high, int index, int bits)
{
    const int *weights = bits == 2 ? Weights2 : (bits == 3 ? Weights3 : Weights4);
    return (low * (64 - weights[index]) + high * weights[index] + 32) >> 6;
}

bool decodeMode02(int mode, const uchar *block, Bc7Rgba *pixels)
{
    const int weightBits = mode == 0 ? 3 : 2;
    const int endpointBits = mode == 0 ? 4 : 5;
    int offset = 0;
    if (readBits(block, offset, mode + 1) != (1U << mode))
        return false;
    const int partition = readBits(block, offset, mode == 0 ? 4 : 6);
    int endpoints[6][4]{};
    for (int component = 0; component < 3; ++component)
        for (auto &endpoint : endpoints)
            endpoint[component] = readBits(block, offset, endpointBits);
    int pbits[6]{};
    if (mode == 0)
        for (int &pbit : pbits)
            pbit = readBits(block, offset, 1);
    int weights[16];
    for (int i = 0; i < 16; ++i)
    {
        const bool anchor =
            i == 0 || i == Anchor31[partition] || i == Anchor32[partition];
        weights[i] = readBits(block, offset, weightBits - (anchor ? 1 : 0));
    }
    if (offset != 128)
        return false;
    for (int e = 0; e < 6; ++e)
        for (int c = 0; c < 3; ++c)
            endpoints[e][c] = mode == 0
                                  ? dequant(endpoints[e][c], pbits[e], endpointBits)
                                  : dequant(endpoints[e][c], endpointBits);
    for (int i = 0; i < 16; ++i)
    {
        const int subset = Partition3[partition * 16 + i];
        pixels[i] = {
            interpolate(endpoints[subset * 2][0], endpoints[subset * 2 + 1][0],
                        weights[i], weightBits),
            interpolate(endpoints[subset * 2][1], endpoints[subset * 2 + 1][1],
                        weights[i], weightBits),
            interpolate(endpoints[subset * 2][2], endpoints[subset * 2 + 1][2],
                        weights[i], weightBits),
            255};
    }
    return true;
}

bool decodeMode137(int mode, const uchar *block, Bc7Rgba *pixels)
{
    const int components = mode == 7 ? 4 : 3;
    const int weightBits = mode == 1 ? 3 : 2;
    const int endpointBits = mode == 7 ? 5 : (mode == 1 ? 6 : 7);
    int offset = 0;
    if (readBits(block, offset, mode + 1) != (1U << mode))
        return false;
    const int partition = readBits(block, offset, 6);
    int endpoints[4][4]{};
    for (int c = 0; c < components; ++c)
        for (auto &endpoint : endpoints)
            endpoint[c] = readBits(block, offset, endpointBits);
    int pbits[4]{};
    const int pbitCount = mode == 1 ? 2 : 4;
    for (int i = 0; i < pbitCount; ++i)
        pbits[i] = readBits(block, offset, 1);
    int weights[16];
    for (int i = 0; i < 16; ++i)
        weights[i] =
            readBits(block, offset,
                     weightBits - ((i == 0 || i == Anchor2[partition]) ? 1 : 0));
    if (offset != 128)
        return false;
    for (int e = 0; e < 4; ++e)
    {
        for (int c = 0; c < components; ++c)
            endpoints[e][c] =
                dequant(endpoints[e][c], pbits[mode == 1 ? e / 2 : e], endpointBits);
        if (components == 3)
            endpoints[e][3] = 255;
    }
    for (int i = 0; i < 16; ++i)
    {
        const int subset = Partition2[partition * 16 + i];
        int value[4];
        for (int c = 0; c < 4; ++c)
            value[c] =
                interpolate(endpoints[subset * 2][c], endpoints[subset * 2 + 1][c],
                            weights[i], weightBits);
        pixels[i] = {value[0], value[1], value[2], value[3]};
    }
    return true;
}

bool decodeMode45(int mode, const uchar *block, Bc7Rgba *pixels)
{
    const int alphaWeightBits = mode == 4 ? 3 : 2;
    const int endpointBits = mode == 4 ? 5 : 7;
    const int alphaEndpointBits = mode == 4 ? 6 : 8;
    int offset = 0;
    if (readBits(block, offset, mode + 1) != (1U << mode))
        return false;
    const int rotation = readBits(block, offset, 2);
    const int indexMode = mode == 4 ? readBits(block, offset, 1) : 0;
    int endpoints[2][4]{};
    for (int c = 0; c < 4; ++c)
        for (auto &endpoint : endpoints)
            endpoint[c] =
                readBits(block, offset, c == 3 ? alphaEndpointBits : endpointBits);
    const int bits[2] = {indexMode ? alphaWeightBits : 2,
                         indexMode ? 2 : alphaWeightBits};
    int colorWeights[16]{}, alphaWeights[16]{};
    int *first = indexMode ? alphaWeights : colorWeights;
    int *second = indexMode ? colorWeights : alphaWeights;
    for (int i = 0; i < 16; ++i)
        first[i] = readBits(block, offset, bits[indexMode] - (i == 0 ? 1 : 0));
    for (int i = 0; i < 16; ++i)
        second[i] = readBits(block, offset, bits[1 - indexMode] - (i == 0 ? 1 : 0));
    if (offset != 128)
        return false;
    for (auto &endpoint : endpoints)
        for (int c = 0; c < 4; ++c)
            endpoint[c] =
                dequant(endpoint[c], c == 3 ? alphaEndpointBits : endpointBits);
    for (int i = 0; i < 16; ++i)
    {
        int values[4];
        for (int c = 0; c < 3; ++c)
            values[c] = interpolate(endpoints[0][c], endpoints[1][c], colorWeights[i],
                                    bits[0]);
        values[3] =
            interpolate(endpoints[0][3], endpoints[1][3], alphaWeights[i], bits[1]);
        if (rotation >= 1)
            std::swap(values[3], values[rotation - 1]);
        pixels[i] = {values[0], values[1], values[2], values[3]};
    }
    return true;
}

bool decodeMode6(const uchar *block, Bc7Rgba *pixels)
{
    int offset = 0;
    if (readBits(block, offset, 7) != 64)
        return false;
    int endpoints[2][4]{};
    for (int c = 0; c < 4; ++c)
        for (auto &endpoint : endpoints)
            endpoint[c] = readBits(block, offset, 7);
    const int p0 = readBits(block, offset, 1), p1 = readBits(block, offset, 1);
    for (int c = 0; c < 4; ++c)
    {
        endpoints[0][c] = endpoints[0][c] * 2 + p0;
        endpoints[1][c] = endpoints[1][c] * 2 + p1;
    }
    for (int i = 0; i < 16; ++i)
    {
        const int weight = readBits(block, offset, i == 0 ? 3 : 4);
        pixels[i] = {interpolate(endpoints[0][0], endpoints[1][0], weight, 4),
                     interpolate(endpoints[0][1], endpoints[1][1], weight, 4),
                     interpolate(endpoints[0][2], endpoints[1][2], weight, 4),
                     interpolate(endpoints[0][3], endpoints[1][3], weight, 4)};
    }
    return offset == 128;
}

} // namespace

bool decodeBc7Block(const uchar *block, Bc7Rgba *pixels)
{
    if (!block || !pixels || block[0] == 0)
        return false;
    int mode = 0;
    while (mode < 8 && (block[0] & (1U << mode)) == 0)
        ++mode;
    switch (mode)
    {
    case 0:
    case 2:
        return decodeMode02(mode, block, pixels);
    case 1:
    case 3:
    case 7:
        return decodeMode137(mode, block, pixels);
    case 4:
    case 5:
        return decodeMode45(mode, block, pixels);
    case 6:
        return decodeMode6(block, pixels);
    default:
        return false;
    }
}
