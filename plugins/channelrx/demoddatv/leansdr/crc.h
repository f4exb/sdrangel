// This file is part of LeanSDR Copyright (C) 2016-2018 <pabr@pabr.org>.
// See the toplevel README for more information.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef LEANSDR_CRC_H
#define LEANSDR_CRC_H

#include <stdint.h>

#include "leansdr/discrmath.h"

namespace leansdr
{

// EN 302 307-1 section 5.1.4 CRC-8 encoder

struct crc8_engine
{
    crc8_engine()
    {
        // Precompute
        // EN 302 307-1 5.1.4 Figure 2
        bitvect<uint8_t, 8> g = POLY_DVBS2_CRC8;
        for (int u = 0; u < 256; ++u)
        {
            uint8_t u8 = u;
            bitvect<uint8_t, 8> c = shiftdivmod(&u8, 1, g);
            table[u] = c.v[0];
        }
    }
    uint8_t compute(const uint8_t *buf, int len)
    {
        uint8_t c = 0;
        for (; len--; ++buf)
            c = table[c ^ *buf];
        return c;
    }

  private:
    static const uint8_t POLY_DVBS2_CRC8 = 0xd5; // (1)11010101
    uint8_t table[256];
};

// CRC-32 ITU V.42 for FINGERPRINT

uint32_t crc32(const uint8_t *buf, int len)
{
    static const uint32_t poly = 0xedb88320;
    uint32_t c = 0xffffffff;
    for (int i = 0; i < len; ++i)
    {
        c ^= buf[i];
        for (int bit = 8; bit--;)
            c = (c & 1) ? (c >> 1) ^ poly : (c >> 1);
    }
    return c ^ 0xffffffff;
}

} // namespace leansdr

#endif // LEANSDR_CRC_H
