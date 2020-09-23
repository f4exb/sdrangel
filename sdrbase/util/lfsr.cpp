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

#include "lfsr.h"
#include "popcount.h"

// Shift LFSR one bit
int LFSR::shift()
{
    int bit;

    bit = (popcount(m_sr & m_polynomial) & 1) ^ 1;
    m_sr = (m_sr << 1) | bit;
    return bit;
}

// Scramble a single bit
int LFSR::scramble(int bit_in)
{
    int bit_out;

    bit_out = (popcount(m_sr & m_polynomial) & 1) ^ bit_in;
    m_sr = (m_sr << 1) | bit_out;
    return bit_out;
}
 #include <stdio.h>

// Scramble data using LFSR - LSB first
void LFSR::scramble(uint8_t *data, int length)
{
    uint8_t byte_in, byte_out;
    int bit_in, bit_out;

    for(int i = 0; i < length; i++)
    {
        byte_in = data[i];
        byte_out = 0;
        for (int j = 0; j < 8; j++)
        {
            bit_in = (byte_in >> j) & 1;
            bit_out = (popcount(m_sr & m_polynomial) & 1) ^ bit_in;
            m_sr = (m_sr << 1) | bit_out;
            byte_out = byte_out | (bit_out << j);
        }
        data[i] = byte_out;
    }
}

// Descramble data using LFSR - LSB first
void LFSR::descramble(uint8_t *data, int length)
{
    uint8_t byte_in, byte_out;
    int bit_in, bit_out;

    for(int i = 0; i < length; i++)
    {
        byte_in = data[i];
        byte_out = 0;
        for (int j = 0; j < 8; j++)
        {
            bit_in = (byte_in >> j) & 1;
            bit_out = (popcount(m_sr & m_polynomial) & 1) ^ bit_in;
            m_sr = (m_sr << 1) | bit_in;
            byte_out = byte_out | (bit_out << j);
        }
        data[i] = byte_out;
    }
}

// XOR data with rand_bit of LFSR - LSB first
void LFSR::randomize(uint8_t *data, int length)
{
    uint8_t byte_in, byte_out;
    int bit_in, bit_out, bit;

    for(int i = 0; i < length; i++)
    {
        byte_in = data[i];
        byte_out = 0;
        for (int j = 0; j < 8; j++)
        {
            // XOR input bit with specified bit from SR
            bit_in = (byte_in >> j) & 1;
            bit_out = ((m_sr >> m_rand_bit) & 1) ^ bit_in;
            byte_out = byte_out | (bit_out << j);
            // Update LFSR
            bit = popcount(m_sr & m_polynomial) & 1;
            m_sr = (m_sr << 1) | bit;
        }
        data[i] = byte_out;
    }
}
