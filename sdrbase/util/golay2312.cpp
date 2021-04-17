///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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
#include <bitset>

#include "golay2312.h"

const unsigned int Golay2312::m_B[11] = {
    0b101001001111,
    0b111101101000,
    0b011110110100,
    0b001111011010,
    0b000111101101,
    0b101010111001,
    0b111100010011,
    0b110111000110,
    0b011011100011,
    0b100100111110,
    0b010010011111,
};

const unsigned int Golay2312::m_I11[11] = {
    0b10000000000,
    0b01000000000,
    0b00100000000,
    0b00010000000,
    0b00001000000,
    0b00000100000,
    0b00000010000,
    0b00000001000,
    0b00000000100,
    0b00000000010,
    0b00000000001,
};

const unsigned int Golay2312::m_I12[12] = {
    0b100000000000,
    0b010000000000,
    0b001000000000,
    0b000100000000,
    0b000010000000,
    0b000001000000,
    0b000000100000,
    0b000000010000,
    0b000000001000,
    0b000000000100,
    0b000000000010,
    0b000000000001,
};

Golay2312::Golay2312()
{
    initG();
    initH();
    buildCorrMatrix(m_corrPL, m_HPL);
    buildCorrMatrix(m_corrPF, m_HPF, true);
}

Golay2312::~Golay2312()
{
}

void Golay2312::initG()
{
    for (int r = 0; r < 23; r++)
    {
        // parity last
        if (r < 12) {
            m_GPL[r] = m_I12[r];
        } else {
            m_GPL[r] = m_B[r-12];
        }
        // parity first
        if (r < 11) {
            m_GPF[r] = m_B[r];
        } else {
            m_GPF[r] = m_I12[r-11];
        }
    }
}

void Golay2312::initH()
{
    for (int r = 0; r < 11; r++)
    {
        m_HPL[r] = (m_B[r] << 11) + m_I11[r];   // parity last
        m_HPF[r] = (m_I11[r] << 12) + m_B[r]; // parity first
    }
}

void Golay2312::encodeParityLast(unsigned int msg, unsigned int *tx)
{
    *tx = 0;

    for (int r = 0; r < 23; r++) {
        *tx += (std::bitset<32>(m_GPL[r] & msg).count() % 2) << (22-r);
    }
}

void Golay2312::encodeParityFirst(unsigned int msg, unsigned int *tx)
{
    *tx = 0;

    for (int r = 0; r < 23; r++) {
        *tx += (std::bitset<32>(m_GPF[r] & msg).count() % 2) << (22-r);
    }
}

bool Golay2312::decodeParityLast(unsigned int *rx)
{
    unsigned int s = syn(m_HPL, *rx);
    return lut(m_corrPL, s, rx);
}

bool Golay2312::decodeParityFirst(unsigned int *rx)
{
    unsigned int s = syn(m_HPF, *rx);
    return lut(m_corrPF, s, rx);
}

unsigned int Golay2312::syn(unsigned int *H, unsigned int rx)
{
    unsigned int s = 0;

    for (int r = 0; r < 11; r++) {
        s += (std::bitset<32>(H[r] & rx).count() % 2) << (10-r);
    }

    return s;
}

bool Golay2312::lut(unsigned char *corr, unsigned int syndrome, unsigned int *rx)
{
    if (syndrome == 0) {
        return true;
    }

    int i = 0;

    for (; i < 3; i++)
    {
        if (corr[3*syndrome + i] == 0xFF) {
            break;
        } else {
            *rx ^= (1 << corr[3*syndrome + i]); // flip bit
        }
    }

    if (i == 0) {
        return false;
    }

    return true;
}

void Golay2312::buildCorrMatrix(unsigned char *corr, unsigned int *H, bool pf)
{
    int shiftP = pf ? 12 : 0; // shift in position value for parity bits
    int shiftW = pf ? 0 : 11; // shift in position value for message word bits
    std::fill(corr, corr + 3*2048, 0xFF);
    int syndromeI;
    unsigned int cw;

    for (int i1 = 0; i1 < 12; i1++)
    {
        for (int i2 = i1+1; i2 < 12; i2++)
        {
            for (int i3 = i2+1; i3 < 12; i3++)
            {
                // 3 bit patterns (in message)
                cw = (1 << (i1+shiftW)) + (1 << (i2+shiftW)) + (1 << (i3+shiftW));
                syndromeI = syn(H, cw);
                corr[3*syndromeI + 0] = i1 + shiftW;
                corr[3*syndromeI + 1] = i2 + shiftW;
                corr[3*syndromeI + 2] = i3 + shiftW;
            }

            // 2 bit patterns (in message)
            cw = (1 << (i1+shiftW)) + (1 << (i2+shiftW));
            syndromeI = syn(H, cw);
            corr[3*syndromeI + 0] = i1 + shiftW;
            corr[3*syndromeI + 1] = i2 + shiftW;

            // 1 possible bit flip left in the parity part
            for (int ip = 0; ip < 11; ip++)
            {
                int syndromeIP = syndromeI ^ (1 << (10-ip));
                corr[3*syndromeIP + 0] = i1 + shiftW;
                corr[3*syndromeIP + 1] = i2 + shiftW;
                corr[3*syndromeIP + 2] = 10 - ip + shiftP;
            }
        }

        // single bit patterns (in message)
        cw = (1 << (i1+shiftW));
        syndromeI = syn(H, cw);
        corr[3*syndromeI + 0] = i1 + shiftW;

        for (int ip1 = 0; ip1 < 11; ip1++) // 1 more bit flip in parity
        {
            int syndromeIP1 = syndromeI ^ (1 << (10-ip1));
            corr[3*syndromeIP1 + 0] = i1 + shiftW;
            corr[3*syndromeIP1 + 1] = 10 - ip1 + shiftP;

            for (int ip2 = ip1+1; ip2 < 11; ip2++) // 1 more bit flip in parity
            {
                int syndromeIP2 = syndromeIP1 ^ (1 << (10-ip2));
                corr[3*syndromeIP2 + 0] = i1 + shiftW;
                corr[3*syndromeIP2 + 1] = 10 - ip1 + shiftP;
                corr[3*syndromeIP2 + 2] = 10 - ip2 + shiftP;
            }
        }
    }

    // no bit patterns (in message) -> all in parity

    for (int ip1 = 0; ip1 < 11; ip1++) // 1 bit flip in parity
    {
        int syndromeIP1 =  (1 << (10-ip1));
        corr[3*syndromeIP1 + 0] = 10 - ip1 + shiftP;

        for (int ip2 = ip1+1; ip2 < 11; ip2++) // 1 more bit flip in parity
        {
            int syndromeIP2 = syndromeIP1 ^ (1 << (10-ip2));
            corr[3*syndromeIP2 + 0] = 10 - ip1 + shiftP;
            corr[3*syndromeIP2 + 1] = 10 - ip2 + shiftP;

            for (int ip3 = ip2+1; ip3 < 11; ip3++) // 1 more bit flip in parity
            {
                int syndromeIP3 = syndromeIP2 ^ (1 << (10-ip3));
                corr[3*syndromeIP3 + 0] = 10 - ip1 + shiftP;
                corr[3*syndromeIP3 + 1] = 10 - ip2 + shiftP;
                corr[3*syndromeIP3 + 2] = 10 - ip3 + shiftP;
            }
        }
    }
}

