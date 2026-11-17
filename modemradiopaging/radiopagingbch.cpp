///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#include "radiopagingbch.h"

#include <array>

namespace {

// Remainder of the 31-bit polynomial in bits 30-0 divided by the generator
uint32_t remainder31(uint32_t poly)
{
    for (int bit = 30; bit >= 10; bit--)
    {
        if (poly & (1u << bit)) {
            poly ^= RadioPagingBCH::m_generator << (bit - 10);
        }
    }

    return poly & 0x3ff;
}

// Error pattern (in codeword bit positions 31-1) for each syndrome, for all
// patterns of weight 1 and 2. Entry 0 is unused.
struct ErrorTable
{
    std::array<uint32_t, 1024> m_pattern;
    std::array<int8_t, 1024> m_weight;

    ErrorTable()
    {
        m_pattern.fill(0);
        m_weight.fill(-1);
        m_weight[0] = 0;

        for (int i = 1; i < 32; i++)
        {
            uint32_t e1 = 1u << i;
            uint32_t s1 = RadioPagingBCH::syndrome(e1);
            m_pattern[s1] = e1;
            m_weight[s1] = 1;

            for (int j = i + 1; j < 32; j++)
            {
                uint32_t e2 = e1 | (1u << j);
                uint32_t s2 = RadioPagingBCH::syndrome(e2);
                m_pattern[s2] = e2;
                m_weight[s2] = 2;
            }
        }
    }
};

const ErrorTable& errorTable()
{
    static const ErrorTable table;
    return table;
}

} // namespace

uint32_t RadioPagingBCH::syndrome(uint32_t word)
{
    return remainder31(word >> 1);
}

bool RadioPagingBCH::evenParity(uint32_t word)
{
    word ^= word >> 16;
    word ^= word >> 8;
    word ^= word >> 4;
    word ^= word >> 2;
    word ^= word >> 1;
    return (word & 1) == 0;
}

uint32_t RadioPagingBCH::encode(uint32_t data)
{
    uint32_t code31 = (data & 0x1fffff) << 10;
    code31 |= remainder31(code31);
    uint32_t word = code31 << 1;

    if (!evenParity(word)) {
        word |= 1;
    }

    return word;
}

RadioPagingBCH::Result RadioPagingBCH::decode(uint32_t word)
{
    Result result;
    const ErrorTable& table = errorTable();
    uint32_t s = syndrome(word);
    int weight = table.m_weight[s];

    if (weight < 0)
    {
        result.m_word = word;
        result.m_bitErrors = -1;
        result.m_parityError = !evenParity(word);
    }
    else
    {
        result.m_word = word ^ table.m_pattern[s];
        result.m_bitErrors = weight;
        result.m_parityError = !evenParity(result.m_word);
    }

    // The code with the parity bit has a minimum distance of 6: it corrects 2 errors and detects 3.
    // A wrong parity after correcting 2 bits means 3 or more errors: uncorrectable, the word is kept.
    // With 0 or 1 bit corrected the parity bit is the other error: it is repaired, so the word is a
    // valid codeword
    if ((result.m_bitErrors == 2) && result.m_parityError)
    {
        result.m_word = word;
        result.m_bitErrors = -1;
        result.m_parityError = !evenParity(word);
    }
    else if ((result.m_bitErrors >= 0) && result.m_parityError)
    {
        result.m_word ^= 1u;
    }

    return result;
}

uint32_t RadioPagingBCH::reverse(uint32_t word)
{
    word = ((word >> 1) & 0x55555555u) | ((word & 0x55555555u) << 1);
    word = ((word >> 2) & 0x33333333u) | ((word & 0x33333333u) << 2);
    word = ((word >> 4) & 0x0f0f0f0fu) | ((word & 0x0f0f0f0fu) << 4);
    word = ((word >> 8) & 0x00ff00ffu) | ((word & 0x00ff00ffu) << 8);
    return (word >> 16) | (word << 16);
}
