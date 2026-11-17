///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef MODEMRADIOPAGING_RADIOPAGINGKANJI_H
#define MODEMRADIOPAGING_RADIOPAGINGKANJI_H

#include <string>

#include "export.h"

// FLEX Japanese Text Service (STD-43A 1.3.1, 3.10.2.3): the text is Shift JIS codes in a HEX/binary message, a
// character of the blocking length B each: 16 bits (B = 0000) for Shift JIS, a 2-byte code of a KANJI character or
// the code of a 1-byte character; 8 bits for the JIS X 0201 table only. The data bits of a character are sent the
// first bit the least significant, as all the data of HEX/binary messages: as hex digits (4 bits each, first bit
// the least significant), a 16-bit character is 4 digits, the lowest one first.
class MODEMRADIOPAGING_API RadioPagingKanji
{
public:
    //!< The hex digits of a text (UTF-8) in Shift JIS characters of blocking bits (8 or 16). False and an error if a
    //!< character has no Shift JIS code (8 bits: no 1-byte code)
    static bool encode(const std::string& text, int blocking, std::string& hex, std::string& error);
    //!< The text (UTF-8) of the hex digits of Shift JIS characters of blocking bits (8 or 16); a character with a lost
    //!< digit ('?') is '?', lost fragments ('|') a space. Right to left (D = 1): the first character is the last
    //!< of the text
    static std::string decode(const std::string& hex, int blocking, bool rightToLeft = false);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGKANJI_H
