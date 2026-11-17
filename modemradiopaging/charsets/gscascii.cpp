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

// GSC alphanumeric character set, Rep. 900-2 Table VII: 6-bit codes, uppercase only. A code is the ASCII character
// code - 0x20, but 0x3C (CR/LF), 0x3E (the NUL fill) and 0x3F ('_'). Lowercase letters are taken as uppercase, and
// \ { } LF as the nearest codes. CR/LF is shown as the symbol for carriage return (U+240D), so a message is one line;
// it is taken from that symbol, CR, LF and the symbols for line feed and newline

#include "radiopagingcharsettables.h"

namespace {

const RadioPagingCharset::Entry entries[64] = {
    /* 0x00 */ {" "}, /* 0x01 */ {"!"}, /* 0x02 */ {"\""}, /* 0x03 */ {"#"},
    /* 0x04 */ {"$"}, /* 0x05 */ {"%"}, /* 0x06 */ {"&"}, /* 0x07 */ {"'"},
    /* 0x08 */ {"("}, /* 0x09 */ {")"}, /* 0x0A */ {"*"}, /* 0x0B */ {"+"},
    /* 0x0C */ {","}, /* 0x0D */ {"-"}, /* 0x0E */ {"."}, /* 0x0F */ {"/"},
    /* 0x10 */ {"0"}, /* 0x11 */ {"1"}, /* 0x12 */ {"2"}, /* 0x13 */ {"3"},
    /* 0x14 */ {"4"}, /* 0x15 */ {"5"}, /* 0x16 */ {"6"}, /* 0x17 */ {"7"},
    /* 0x18 */ {"8"}, /* 0x19 */ {"9"}, /* 0x1A */ {":"}, /* 0x1B */ {";"},
    /* 0x1C */ {"<"}, /* 0x1D */ {"="}, /* 0x1E */ {">"}, /* 0x1F */ {"?"},
    /* 0x20 */ {"@", "\\"}, /* 0x21 */ {"A", "a"}, /* 0x22 */ {"B", "b"}, /* 0x23 */ {"C", "c"},
    /* 0x24 */ {"D", "d"}, /* 0x25 */ {"E", "e"}, /* 0x26 */ {"F", "f"}, /* 0x27 */ {"G", "g"},
    /* 0x28 */ {"H", "h"}, /* 0x29 */ {"I", "i"}, /* 0x2A */ {"J", "j"}, /* 0x2B */ {"K", "k"},
    /* 0x2C */ {"L", "l"}, /* 0x2D */ {"M", "m"}, /* 0x2E */ {"N", "n"}, /* 0x2F */ {"O", "o"},
    /* 0x30 */ {"P", "p"}, /* 0x31 */ {"Q", "q"}, /* 0x32 */ {"R", "r"}, /* 0x33 */ {"S", "s"},
    /* 0x34 */ {"T", "t"}, /* 0x35 */ {"U", "u"}, /* 0x36 */ {"V", "v"}, /* 0x37 */ {"W", "w"},
    /* 0x38 */ {"X", "x"}, /* 0x39 */ {"Y", "y"}, /* 0x3A */ {"Z", "z"}, /* 0x3B */ {"[", "{"},
    /* 0x3C */ {"\xE2\x90\x8D" /* U+240D symbol for carriage return */, "\r", "\n", "\xE2\x90\x8A" /* U+240A */,
        "\xE2\x90\xA4" /* U+2424 symbol for newline */}, /* 0x3D */ {"]", "}"}, /* 0x3E */ {}, /* 0x3F */ {"_"},
};

} // namespace

const RadioPagingCharset::Table RadioPagingCharsetTables::gscAscii = {6, entries};
