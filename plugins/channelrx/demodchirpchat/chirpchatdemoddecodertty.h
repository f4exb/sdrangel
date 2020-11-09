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

#ifndef INCLUDE_CHIRPCHATDEMODDECODERTTY_H
#define INCLUDE_CHIRPCHATDEMODDECODERTTY_H

#include <vector>
#include <QString>

class ChirpChatDemodDecoderTTY
{
public:
    static void decodeSymbols(const std::vector<unsigned short>& symbols, QString& str);

private:
    enum TTYState
    {
        TTYLetters,
        TTYFigures
    };

    static const char ttyLetters[32];
    static const char ttyFigures[32];
    static const char lettersTag = 0x1f;
    static const char figuresTag = 0x1b;
};

#endif // INCLUDE_CHIRPCHATDEMODDECODERTTY_H