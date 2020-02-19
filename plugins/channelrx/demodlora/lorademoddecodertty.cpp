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

#include "lorademoddecodertty.h"

const char LoRaDemodDecoderTTY::ttyLetters[32] = {
    '\0',   'E',    '\n',   'A',    ' ',    'S',    'I',    'U',
    '\r',   'D',    'R',    'J',    'N',    'F',    'C',    'K',
    'T',    'Z',    'L',    'W',    'H',    'Y',    'P',    'Q',
    'O',    'B',    'G',    ' ',    'M',    'X',    'V',    ' '
};

const char LoRaDemodDecoderTTY::ttyFigures[32] = { // U.S. standard
    '\0',   '3',    '\n',   '-',    ' ',    '\a',   '8',    '7',
    '\r',   '$',    '4',    '\'',   ',',    '!',    ':',    '(',
    '5',    '"',    ')',    '2',    '#',    '6',    '0',    '1',
    '9',    '?',    '&',    ' ',    '.',    '/',    ';',    ' '
};

void LoRaDemodDecoderTTY::decodeSymbols(const std::vector<unsigned short>& symbols, QString& str)
{
    std::vector<unsigned short>::const_iterator it = symbols.begin();
    QByteArray bytes;
    TTYState ttyState = TTYLetters;

    for (; it != symbols.end(); ++it)
    {
        char ttyChar = *it & 0x1F;

        if (ttyChar == lettersTag) {
            ttyState = TTYLetters;
        } else if (ttyChar == figuresTag) {
            ttyState = TTYFigures;
        }
        else
        {
            char asciiChar = -1;

            if (ttyState == TTYLetters) {
                asciiChar = ttyLetters[ttyChar];
            } else if (ttyState == TTYFigures) {
                asciiChar = ttyFigures[ttyChar];
            }

            if (asciiChar >= 0) {
                bytes.push_back(asciiChar);
            }
        }
    }

    str = QString(bytes.toStdString().c_str());
}

