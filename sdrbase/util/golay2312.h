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

#ifndef INCLUDE_GOLAY2312_H_
#define INCLUDE_GOLAY2312_H_

#include "export.h"

class SDRBASE_API Golay2312
{
public:
    Golay2312();
    ~Golay2312();

    void encodeParityLast(unsigned int msg, unsigned int *tx);
    void encodeParityFirst(unsigned int msg, unsigned int *tx);
    bool decodeParityLast(unsigned int *rx);
    bool decodeParityFirst(unsigned int *rx);

private:
    inline int bitAt(int i, unsigned int v) {
        return (v>>i) & 0x01;
    }
    void initG();
    void initH();
    void buildCorrMatrix(unsigned char *corr, unsigned int *H, bool pf = false);
    unsigned int syn(unsigned int *H, unsigned int rx);
    bool lut(unsigned char *corr, unsigned int syndrome, unsigned int *rx);

    unsigned char m_corrPL[2048*3];      //!< up to 3 bit error correction by syndrome index - parity last
    unsigned char m_corrPF[2048*3];      //!< up to 3 bit error correction by syndrome index - parity first
    unsigned int  m_GPL[23];             //!< Generator matrix of 23x12 bits - parity first (MSB)
    unsigned int  m_GPF[23];             //!< Generator matrix of 23x12 bits - parity last (LSB)
    unsigned int  m_HPL[11];             //!< Parity check matrix of 11x23 bits - parity last (LSB)
    unsigned int  m_HPF[11];             //!< Parity check matrix of 11x23 bits - parity first (MSB)
    // building arrays
    static const unsigned int m_B[11];   //!< Coding matrix (11x12 bits)
    static const unsigned int m_I12[12]; //!< 12x12 identity matrix (12x12 bits)
    static const unsigned int m_I11[11]; //!< 11x11 identity matrix (11x11 bits)

};

#endif // INCLUDE_GOLAY2312_H_
