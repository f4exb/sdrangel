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
//                                                                               //
// Source: https://mmi-comm.tripod.com/dcs.html                                  //
///////////////////////////////////////////////////////////////////////////////////

#include <bitset>

#include "nfmmoddcs.h"

const float NFMModDCS::m_codeRate = 134.3; //!< bits per second

NFMModDCS::NFMModDCS()
{
    reset();
}

NFMModDCS::~NFMModDCS()
{
}

void NFMModDCS::reset()
{
    m_step = 0;
}

void NFMModDCS::setDCS(int code)
{
    unsigned int dcsCode = code < 0 ? 0 : code > 511 ? 511 : code; // trim invalid values
    // Parity bits calculation:
    unsigned int p[11];
    p[0]  = std::bitset<32>(dcsCode & 0b10011111).count() % 2;      // P0  = C0 + C1 + C2 + C3 + C4 + C7 (modulo two addition)
    p[1]  = (std::bitset<32>(dcsCode & 0b100111110).count()+1) % 2; // P1  = NOT ( C1 + C2 + C3 + C4 + C5 + C8 )
    p[2]  = std::bitset<32>(dcsCode & 0b11100011).count() % 2;      // P2  = C0 + C1 + C5 + C6 + C7
    p[3]  = (std::bitset<32>(dcsCode & 0b111000110).count()+1) % 2; // P3  = NOT ( C1 + C2 + C6 + C7 + C8 )
    p[4]  = (std::bitset<32>(dcsCode & 0b100010011).count()+1) % 2; // P4  = NOT ( C0 + C1 + C4 + C8 )
    p[5]  = (std::bitset<32>(dcsCode & 0b10111001).count()+1) % 2;  // P5  = NOT ( C0 + C3 + C4 + C5 + C7 )
    p[6]  = std::bitset<32>(dcsCode & 0b111101101).count() % 2;     // P6  = C0 + C2 + C3 + C5 + C6 + C7 + C8
    p[7]  = std::bitset<32>(dcsCode & 0b111011010).count() % 2;     // P7  = C1 + C3 + C4 + C6 + C7 + C8
    p[8]  = std::bitset<32>(dcsCode & 0b110110100).count() % 2;     // P8  = C2 + C4 + C5 + C7 + C8
    p[9]  = (std::bitset<32>(dcsCode & 0b101101000).count()+1) % 2; // P9  = NOT ( C3 + C5 + C6 + C8 )
    p[10] = (std::bitset<32>(dcsCode & 0b1001111).count()+1) % 2;   // P10 = NOT ( C0 + C1 + C2 + C3 + C6 )
    int dcsIndex = 0;
    // code:
    for (int i = 0; i < 9; i++, dcsIndex++) {
        m_dcsWord[dcsIndex] = (dcsCode >> i) & 1;
    }
    // filler (0x04)
    m_dcsWord[dcsIndex++] = 0;
    m_dcsWord[dcsIndex++] = 0;
    m_dcsWord[dcsIndex++] = 1;
    // parity
    for (int i = 0; i < 11; i++, dcsIndex++) {
        m_dcsWord[dcsIndex] = p[i];
    }

    m_step = 0;
}

void NFMModDCS::setPositive(bool positive)
{
    m_positive = positive;
    m_step = 0;
}

void NFMModDCS::setSampleRate(int sampleRate)
{
    m_bitPerStep = m_codeRate / sampleRate;
    m_step = 0;
}

int NFMModDCS::next()
{
    int dcsIndex = (int) m_step;
    int carrier = (m_dcsWord[dcsIndex] == 1) ? (m_positive ? 1 : -1) : (m_positive ? -1 : 1);

    if (m_step + m_bitPerStep < 23) {
        m_step += m_bitPerStep;
    } else {
        m_step = m_step + m_bitPerStep - 23;
    }

    return carrier;
}
