///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include "psk31.h"

// ASCII varicode encoding
// From http://www.aintel.bi.ehu.es/psk31.html
const QStringList PSK31Varicode::m_varicode = {
    "1010101011",
    "1011011011",
    "1011101101",
    "1101110111",
    "1011101011",
    "1101011111",
    "1011101111",
    "1011111101",
    "1011111111",
    "11101111",
    "11101",
    "1101101111",
    "1011011101",
    "11111",
    "1101110101",
    "1110101011",
    "1011110111",
    "1011110101",
    "1110101101",
    "1110101111",
    "1101011011",
    "1101101011",
    "1101101101",
    "1101010111",
    "1101111011",
    "1101111101",
    "1110110111",
    "1101010101",
    "1101011101",
    "1110111011",
    "1011111011",
    "1101111111",
    "1",
    "111111111",
    "101011111",
    "111110101",
    "111011011",
    "1011010101",
    "1010111011",
    "101111111",
    "11111011",
    "11110111",
    "101101111",
    "111011111",
    "1110101",
    "110101",
    "1010111",
    "110101111",
    "10110111",
    "10111101",
    "11101101",
    "11111111",
    "101110111",
    "101011011",
    "101101011",
    "110101101",
    "110101011",
    "110110111",
    "11110101",
    "110111101",
    "111101101",
    "1010101",
    "111010111",
    "1010101111",
    "1010111101",
    "1111101",
    "11101011",
    "10101101",
    "10110101",
    "1110111",
    "11011011",
    "11111101",
    "101010101",
    "1111111",
    "111111101",
    "101111101",
    "11010111",
    "10111011",
    "11011101",
    "10101011",
    "11010101",
    "111011101",
    "10101111",
    "1101111",
    "1101101",
    "101010111",
    "110110101",
    "101011101",
    "101110101",
    "101111011",
    "1010101101",
    "111110111",
    "111101111",
    "111111011",
    "1010111111",
    "101101101",
    "1011011111",
    "1011",
    "1011111",
    "101111",
    "101101",
    "11",
    "111101",
    "1011011",
    "101011",
    "1101",
    "111101011",
    "10111111",
    "11011",
    "111011",
    "1111",
    "111",
    "111111",
    "110111111",
    "10101",
    "10111",
    "101",
    "110111",
    "1111011",
    "1101011",
    "11011111",
    "1011101",
    "111010101",
    "1010110111",
    "110111011",
    "1010110101",
    "1011010111",
    "1110110101",
};

PSK31Encoder::PSK31Encoder()
{
}


bool PSK31Encoder::encode(QChar c, unsigned &bits, unsigned int &bitCount)
{
    bits = 0;
    bitCount = 0;

    char ch = c.toLatin1() & 0x7f;
    QString code = PSK31Varicode::m_varicode[ch];

    // FIXME: http://det.bi.ehu.es/~jtpjatae/pdf/p31g3plx.pdf > 128

    addCode(bits, bitCount, code);
    qDebug() << "Encoding " << c << "as" << code << bits << bitCount;
    return true;
}

void PSK31Encoder::addCode(unsigned& bits, unsigned int& bitCount, const QString& code) const
{
    int codeBits = 0;
    unsigned int codeLen = code.size();

    for (unsigned int i = 0; i < codeLen; i++) {
        codeBits |= (code[i] == "1" ? 1 : 0) << i;
    }

    addStartBits(bits, bitCount);
    addBits(bits, bitCount, codeBits, codeLen);
    addStopBits(bits, bitCount);
}

void PSK31Encoder::addStartBits(unsigned& bits, unsigned int& bitCount) const
{
    // Start bit is 0
    addBits(bits, bitCount, 0, 1);
}

void PSK31Encoder::addStopBits(unsigned& bits, unsigned int& bitCount) const
{
    // Stop bit is 1
    addBits(bits, bitCount, 0, 1);
}

void PSK31Encoder::addBits(unsigned& bits, unsigned int& bitCount, int data, int count) const
{
    bits |= data << bitCount;
    bitCount += count;
}
