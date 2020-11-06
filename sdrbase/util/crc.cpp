///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "crc.h"

// Reverse bit ordering
uint32_t crc::reverse(uint32_t val, int bits)
{
    uint32_t temp;
    int i;

    temp = 0;
    for (i = 0; i < bits; i++)
        temp |= ((val >> i) & 1) << (bits - 1 - i);
    return temp;
}

// Calculate CRC for specified number of bits
void crc::calculate(uint32_t data, int data_bits)
{
    uint32_t tmp;
    uint32_t mask;
    uint32_t msb;
    int bit, i;

    if (m_msb_first)
    {
        mask = (1 << m_poly_bits) - 1;
        msb = 1 << (m_poly_bits - 1);
        tmp = m_crc ^ (data << (m_poly_bits - 8));
        for (i = 0; i < data_bits; i++)
        {
            if (tmp & msb)
                tmp = (tmp << 1) ^ m_polynomial;
            else
                tmp = tmp << 1;
            tmp = tmp & mask;
        }
        m_crc = tmp;
    }
    else
    {
        tmp = m_crc;
        for (i = 0; i < data_bits; i++) {
            bit = ((data >> i) & 1) ^ (tmp & 1);
            if (bit)
                tmp = (tmp >> 1) ^ m_polynomial_rev;
            else
                tmp = tmp >> 1;
        }
        m_crc = tmp;
    }
}

// Calculate CRC for specified array
void crc::calculate(const uint8_t *data, int length)
{
    int i;
    uint32_t mask1;
    uint32_t mask2;

    if (m_msb_first)
    {
        mask1 = (1 << m_poly_bits) - 1;
        mask2 = 0xff << (m_poly_bits - 8);
        for (i = 0; i < length; i++)
            m_crc = mask1 & ((m_crc << 8) ^ m_lut[data[i] ^ ((m_crc & mask2) >> (m_poly_bits-8))]);
    }
    else
    {
        for (i = 0; i < length; i++)
            m_crc = (m_crc >> 8) ^ m_lut[data[i] ^ (m_crc & 0xff)];
    }
}
