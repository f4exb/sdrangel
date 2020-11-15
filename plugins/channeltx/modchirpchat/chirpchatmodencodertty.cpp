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

#include "chirpchatmodencodertty.h"

const signed char ChirpChatModEncoderTTY::asciiToTTYLetters[128] = {
//    '\x00'   '\x01'   '\x02'   '\x03'   '\x04'   '\x05'   '\x06'   '\x07'
      0x00,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//    '\x08'   '\t'     '\n'     '\x0b'   '\x0c'   '\r'     '\x0e'   '\x0f'
      -1  ,    -1  ,    0x02,    -1  ,    -1  ,    0x08,    -1  ,    -1  ,
//    '\x10'   '\x11'   '\x12'   '\x13'   '\x14'   '\x15'   '\x16'   '\x17'
      -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//    '\x18'   '\x19'   '\x1a'   '\x1b'   '\x1c'   '\x1d'   '\x1e'   '\x1f'
      -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//    ' '      '!'      '"'      '#'      '$'      '%'      '&'      "'"
      0x04,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//    '('      ')'      '*'      '+'      ','      '-'      '.'      '/'
      -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//    '0'      '1'      '2'      '3'      '4'      '5'      '6'      '7'
      -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//    '8'      '9'      ':'      ';'      '<'      '='      '>'      '?'
      -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//    '@'      'A'      'B'      'C'      'D'      'E'      'F'      'G'
      -1  ,    0x03,    0x19,    0x0e,    0x09,    0x01,    0x0d,    0x1a,
//    'H'      'I'      'J'      'K'      'L'      'M'      'N'      'O'
      0x14,    0x06,    0x0b,    0x0f,    0x12,    0x1c,    0x0c,    0x18,
//    'P'      'Q'      'R'      'S'      'T'      'U'      'V'      'W'
      0x16,    0x17,    0x0a,    0x05,    0x10,    0x07,    0x1e,    0x13,
//    'X'      'Y'      'Z'      '['      '\\'     ']'      '^'      '_'
      0x1d,    0x15,    0x11,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//    '`'      'a'      'b'      'c'      'd'      'e'      'f'      'g'
      -1  ,    0x03,    0x19,    0x0e,    0x09,    0x01,    0x0d,    0x1a,
//    'h'      'i'      'j'      'k'      'l'      'm'      'n'      'o'
      0x14,    0x06,    0x0b,    0x0f,    0x12,    0x1c,    0x0c,    0x18,
//    'p'      'q'      'r'      's'      't'      'u'      'v'      'w'
      0x16,    0x17,    0x0a,    0x05,    0x10,    0x07,    0x1e,    0x13,
//    'x'      'y'      'z'      '{'      '|'      '}'      '~'      '\x7f'
      0x1d,    0x15,    0x11,    -1  ,    -1  ,    -1  ,    -1  ,    -1
    };

const signed char ChirpChatModEncoderTTY::asciiToTTYFigures[128] = {
//   '\x00'   '\x01'   '\x02'   '\x03'   '\x04'   '\x05'   '\x06'   '\x07'
     0x00,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    0x05,
//   '\x08'   '\t'     '\n'     '\x0b'   '\x0c'   '\r'     '\x0e'   '\x0f'
     -1  ,    -1  ,    0x02,    -1  ,    -1  ,    0x08,    -1  ,    -1  ,
//   '\x10'   '\x11'   '\x12'   '\x13'   '\x14'   '\x15'   '\x16'   '\x17'
     -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//   '\x18'   '\x19'   '\x1a'   '\x1b'   '\x1c'   '\x1d'   '\x1e'   '\x1f'
     -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//   ' '      '!'      '"'      '#'      '$'      '%'      '&'      "'"
     0x04,    0x0d,    0x11,    0x14,    0x09,    -1  ,    0x1a,    -1  ,
//   '('      ')'      '*'      '+'      ','      '-'      '.'      '/'
     0x0f,    0x12,    -1  ,    -1  ,    0x0c,    0x03,    0x1c,    0x1d,
//   '0'      '1'      '2'      '3'      '4'      '5'      '6'      '7'
     0x16,    0x17,    0x13,    0x01,    0x0a,    0x10,    0x15,    0x07,
//   '8'      '9'      ':'      ';'      '<'      '='      '>'      '?'
     0x06,    0x18,    0x0e,    0x1e,    -1  ,    -1  ,    -1  ,    0x19,
//   '@'      'A'      'B'      'C'      'D'      'E'      'F'      'G'
     -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//   'H'      'I'      'J'      'K'      'L'      'M'      'N'      'O'
     -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//   'P'      'Q'      'R'      'S'      'T'      'U'      'V'      'W'
     -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//   'X'      'Y'      'Z'      '['      '\\'     ']'      '^'      '_'
     -1  ,    -1  ,    -1  ,    -1  ,    0x0b,    -1  ,    -1  ,    -1  ,
//   '`'      'a'      'b'      'c'      'd'      'e'      'f'      'g'
     -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//   'h'      'i'      'j'      'k'      'l'      'm'      'n'      'o'
     -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//   'p'      'q'      'r'      's'      't'      'u'      'v'      'w'
     -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,
//   'x'      'y'      'z'      '{'      '|'      '}'      '~'      '\x7f'
     -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1  ,    -1
    };

void ChirpChatModEncoderTTY::encodeString(const QString& str, std::vector<unsigned short>& symbols)
{
    TTYState ttyState = TTYLetters;
    QByteArray asciiStr = str.toUtf8();
    QByteArray::const_iterator it = asciiStr.begin();

    for (; it != asciiStr.end(); ++it)
    {
        char asciiChar = *it & 0x7F;
        int ttyLetter = asciiToTTYLetters[(int) asciiChar];
        int ttyFigure = asciiToTTYFigures[(int) asciiChar];

        if (ttyLetter < 0)
        {
            if (ttyFigure >= 0)
            {
                if (ttyState != TTYFigures)
                {
                    symbols.push_back(ttyFigures);
                    ttyState = TTYFigures;
                }

                symbols.push_back(ttyFigure);
            } // else skip
        }
        else
        {
            if (ttyFigure >= 0)
            {
                symbols.push_back(ttyFigure); // same TTY character no state change
            }
            else
            {
                if (ttyState != TTYLetters)
                {
                    symbols.push_back(ttyLetters);
                    ttyState = TTYLetters;
                }

                symbols.push_back(ttyLetter);
            }
        }
    }
}
