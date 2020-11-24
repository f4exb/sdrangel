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

#ifndef INCLUDE_MORSE_H
#define INCLUDE_MORSE_H

#include <QString>

#include "export.h"

// Morse (ITU) code utils for converting between strings and Morse code and vice-versa
class SDRBASE_API Morse
{
public:
    static QString toMorse(char asciiChar);
    static QString toMorse(QString &string);
    static QString toUnicode(QString &morse);
    static QString toSpacedUnicode(QString &morse);
    static QString toUnicodeMorse(QString &string);
    static QString toSpacedUnicodeMorse(QString &string);
    static int toASCII(QString &morse);
    static QString toString(QString &morse);

private:
    struct ASCIIToMorse {
        char ascii;
        const char *morse;
    };

    const static ASCIIToMorse m_asciiToMorse[];
};

#endif // INCLUDE_MORSE_H
