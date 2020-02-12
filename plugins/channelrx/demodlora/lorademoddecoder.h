///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_LORADEMODDECODER_H
#define INCLUDE_LORADEMODDECODER_H

#include <vector>
#include "lorademodsettings.h"

class LoRaDemodDecoder
{
public:
    LoRaDemodDecoder();
    ~LoRaDemodDecoder();

    void setCodingScheme(LoRaDemodSettings::CodingScheme codingScheme) { m_codingScheme = codingScheme; }
    void setNbSymbolBits(unsigned int symbolBits) { m_nbSymbolBits = symbolBits; }
    void decodeSymbols(const std::vector<unsigned int>& symbols, QString& str);      //!< For ASCII and TTY
    void decodeSymbols(const std::vector<unsigned int>& symbols, QByteArray& bytes); //!< For raw bytes (original LoRa)

private:
    enum TTYState
    {
        TTYLetters,
        TTYFigures
    };

    void decodeSymbolsASCII(const std::vector<unsigned int>& symbols, QString& str);
    void decodeSymbolsTTY(const std::vector<unsigned int>& symbols, QString& str);

    LoRaDemodSettings::CodingScheme m_codingScheme;
    unsigned int m_nbSymbolBits;

    static const char ttyLetters[32];
    static const char ttyFigures[32];
    static const char lettersTag = 0x1f;
    static const char figuresTag = 0x1b;
};

#endif // INCLUDE_LORADEMODDECODER_H
