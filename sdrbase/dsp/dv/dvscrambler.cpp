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

#include <vector>

#include "dvscrambler.h"

DVScrambler::PN95::PN95(uint32_t seed)
{
    uint32_t sr = seed;

    for (int i = 0; i < 512; i++)
    {
        uint32_t bit0 = sr & 1;
        uint32_t bit4 = (sr >> 4) & 1;
        sr >>= 1;
        sr |= (bit4 ^ bit0) << 8;
        m_bits[i] = (uint8_t) bit0;
    }

    for (int i = 0; i < 64; i++)
    {
        uint8_t byte = 0;

        for (int b = 0; b < 8; b++) {
            byte |= m_bits[8 * i + b] << (7 - b);
        }

        m_bytes[i] = byte;
    }
}

void DVScrambler::PN95::apply(uint8_t *bits, int nbBits, unsigned int firstIndex) const
{
    for (int i = 0; i < nbBits; i++) {
        bits[i] ^= getBit(firstIndex + i);
    }
}

const DVScrambler::PN95& DVScrambler::PN95::nxdn()
{
    static const PN95 pn(0xE4);
    return pn;
}

const DVScrambler::PN95& DVScrambler::PN95::ysf()
{
    static const PN95 pn(0x1C9);
    return pn;
}

const uint8_t *DVScrambler::dstarTable()
{
    // 7 bit register from all ones, output and feedback are bit 6 xor bit 3 (x^7 + x^4 + 1)
    static uint8_t table[660];
    static bool ready = false;

    if (!ready)
    {
        uint32_t sr = 0x7F;

        for (int i = 0; i < 660; i++)
        {
            uint32_t fb = ((sr >> 6) ^ (sr >> 3)) & 1;
            table[i] = (uint8_t) fb;
            sr = ((sr << 1) | fb) & 0x7F;
        }

        ready = true;
    }

    return table;
}

void DVScrambler::dstarHeader(const uint8_t *in, uint8_t *out, int nbBits)
{
    const uint8_t *table = dstarTable();

    for (int i = 0; i < nbBits; i++) {
        out[i] = in[i] ^ table[i % 660];
    }
}

void DVScrambler::dpmrSequence(uint8_t *sequence, int nbBits)
{
    // x^9 + x^5 + 1 with all ones initial state: o[k] = o[k-9] ^ o[k-5]
    uint32_t sr = 0x3FF;

    for (int i = 0; i < nbBits; i++)
    {
        sr >>= 1;
        uint32_t res = sr & 1;
        uint32_t feedback = (((sr >> 4) & 1) ^ res) << 9;
        sr = (sr & 0x1FF) | feedback;
        sequence[i] = (uint8_t) res;
    }
}

void DVScrambler::dpmr(uint8_t *bits, int nbBits)
{
    std::vector<uint8_t> sequence(nbBits);
    dpmrSequence(sequence.data(), nbBits);

    for (int i = 0; i < nbBits; i++) {
        bits[i] ^= sequence[i];
    }
}

void DVScrambler::PN15::apply(uint8_t *bits, int nbBits, int counter)
{
    PN15 pn(counter);

    for (int i = 0; i < nbBits; i++) {
        bits[i] ^= pn.next();
    }
}

void DVScrambler::PN15::apply(float *llr, int nbBits, int counter)
{
    PN15 pn(counter);

    for (int i = 0; i < nbBits; i++)
    {
        if (pn.next()) {
            llr[i] = -llr[i];
        }
    }
}
