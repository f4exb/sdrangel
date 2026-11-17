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

// ASCII of the pagers without lowercase letters (Motorola Bravo Alpha and other uppercase only models).
// 0x20-0x5F: ASCII; lowercase letters are encoded as uppercase, U+2014 em dash as -, the backtick as the
// apostrophe. 0x60-0x7E: the lowercase codes, shown uppercase. Glyphs other than ASCII are UTF-8 escapes

#include "radiopagingcharsettables.h"

namespace {

const RadioPagingCharset::Entry entries[128] = {
    /* 0x00 */ {}, /* 0x01 */ {}, /* 0x02 */ {}, /* 0x03 */ {},
    /* 0x04 */ {}, /* 0x05 */ {}, /* 0x06 */ {}, /* 0x07 */ {},
    /* 0x08 */ {}, /* 0x09 */ {}, /* 0x0A */ {"\xE2\x90\x8A" /* U+240A symbol for line feed */, "\n"}, /* 0x0B */ {},
    /* 0x0C */ {}, /* 0x0D */ {"\xE2\x90\x8D" /* U+240D symbol for carriage return */, "\r"}, /* 0x0E */ {}, /* 0x0F */ {},
    /* 0x10 */ {}, /* 0x11 */ {}, /* 0x12 */ {}, /* 0x13 */ {},
    /* 0x14 */ {}, /* 0x15 */ {}, /* 0x16 */ {}, /* 0x17 */ {},
    /* 0x18 */ {}, /* 0x19 */ {}, /* 0x1A */ {}, /* 0x1B */ {},
    /* 0x1C */ {}, /* 0x1D */ {}, /* 0x1E */ {}, /* 0x1F */ {},
    /* 0x20 */ {" "}, /* 0x21 */ {"!"}, /* 0x22 */ {"\""}, /* 0x23 */ {"#"},
    /* 0x24 */ {"$"}, /* 0x25 */ {"%"}, /* 0x26 */ {"&"}, /* 0x27 */ {"'", "`"},
    /* 0x28 */ {"("}, /* 0x29 */ {")"}, /* 0x2A */ {"*"}, /* 0x2B */ {"+"},
    /* 0x2C */ {","}, /* 0x2D */ {"-", "\xE2\x80\x94" /* U+2014 */}, /* 0x2E */ {"."}, /* 0x2F */ {"/"},
    /* 0x30 */ {"0"}, /* 0x31 */ {"1"}, /* 0x32 */ {"2"}, /* 0x33 */ {"3"},
    /* 0x34 */ {"4"}, /* 0x35 */ {"5"}, /* 0x36 */ {"6"}, /* 0x37 */ {"7"},
    /* 0x38 */ {"8"}, /* 0x39 */ {"9"}, /* 0x3A */ {":"}, /* 0x3B */ {";"},
    /* 0x3C */ {"<"}, /* 0x3D */ {"="}, /* 0x3E */ {">"}, /* 0x3F */ {"?"},
    /* 0x40 */ {"@"}, /* 0x41 */ {"A", "a"}, /* 0x42 */ {"B", "b"}, /* 0x43 */ {"C", "c"},
    /* 0x44 */ {"D", "d"}, /* 0x45 */ {"E", "e"}, /* 0x46 */ {"F", "f"}, /* 0x47 */ {"G", "g"},
    /* 0x48 */ {"H", "h"}, /* 0x49 */ {"I", "i"}, /* 0x4A */ {"J", "j"}, /* 0x4B */ {"K", "k"},
    /* 0x4C */ {"L", "l"}, /* 0x4D */ {"M", "m"}, /* 0x4E */ {"N", "n"}, /* 0x4F */ {"O", "o"},
    /* 0x50 */ {"P", "p"}, /* 0x51 */ {"Q", "q"}, /* 0x52 */ {"R", "r"}, /* 0x53 */ {"S", "s"},
    /* 0x54 */ {"T", "t"}, /* 0x55 */ {"U", "u"}, /* 0x56 */ {"V", "v"}, /* 0x57 */ {"W", "w"},
    /* 0x58 */ {"X", "x"}, /* 0x59 */ {"Y", "y"}, /* 0x5A */ {"Z", "z"}, /* 0x5B */ {"["},
    /* 0x5C */ {"\\"}, /* 0x5D */ {"]"}, /* 0x5E */ {"^"}, /* 0x5F */ {"_"},
    /* 0x60 */ {"`"}, /* 0x61 */ {"A"}, /* 0x62 */ {"B"}, /* 0x63 */ {"C"},
    /* 0x64 */ {"D"}, /* 0x65 */ {"E"}, /* 0x66 */ {"F"}, /* 0x67 */ {"G"},
    /* 0x68 */ {"H"}, /* 0x69 */ {"I"}, /* 0x6A */ {"J"}, /* 0x6B */ {"K"},
    /* 0x6C */ {"L"}, /* 0x6D */ {"M"}, /* 0x6E */ {"N"}, /* 0x6F */ {"O"},
    /* 0x70 */ {"P"}, /* 0x71 */ {"Q"}, /* 0x72 */ {"R"}, /* 0x73 */ {"S"},
    /* 0x74 */ {"T"}, /* 0x75 */ {"U"}, /* 0x76 */ {"V"}, /* 0x77 */ {"W"},
    /* 0x78 */ {"X"}, /* 0x79 */ {"Y"}, /* 0x7A */ {"Z"}, /* 0x7B */ {"{"},
    /* 0x7C */ {"|"}, /* 0x7D */ {"}"}, /* 0x7E */ {"~"}, /* 0x7F */ {},
};

} // namespace

const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagAsciiUppercase = {7, entries};

namespace {

// Multitone RPR series: the same uppercase layout, without the aliases of the apostrophe and the hyphen
const RadioPagingCharset::Entry multitone[128] = {
    /* 0x00 */ {}, /* 0x01 */ {}, /* 0x02 */ {}, /* 0x03 */ {},
    /* 0x04 */ {}, /* 0x05 */ {}, /* 0x06 */ {}, /* 0x07 */ {},
    /* 0x08 */ {}, /* 0x09 */ {}, /* 0x0A */ {"\xE2\x90\x8A" /* U+240A symbol for line feed */, "\n"}, /* 0x0B */ {},
    /* 0x0C */ {}, /* 0x0D */ {"\xE2\x90\x8D" /* U+240D symbol for carriage return */, "\r"}, /* 0x0E */ {}, /* 0x0F */ {},
    /* 0x10 */ {}, /* 0x11 */ {}, /* 0x12 */ {}, /* 0x13 */ {},
    /* 0x14 */ {}, /* 0x15 */ {}, /* 0x16 */ {}, /* 0x17 */ {},
    /* 0x18 */ {}, /* 0x19 */ {}, /* 0x1A */ {}, /* 0x1B */ {},
    /* 0x1C */ {}, /* 0x1D */ {}, /* 0x1E */ {}, /* 0x1F */ {},
    /* 0x20 */ {" "}, /* 0x21 */ {"!"}, /* 0x22 */ {"\""}, /* 0x23 */ {"#"},
    /* 0x24 */ {"$"}, /* 0x25 */ {"%"}, /* 0x26 */ {"&"}, /* 0x27 */ {"'"},
    /* 0x28 */ {"("}, /* 0x29 */ {")"}, /* 0x2A */ {"*"}, /* 0x2B */ {"+"},
    /* 0x2C */ {","}, /* 0x2D */ {"-"}, /* 0x2E */ {"."}, /* 0x2F */ {"/"},
    /* 0x30 */ {"0"}, /* 0x31 */ {"1"}, /* 0x32 */ {"2"}, /* 0x33 */ {"3"},
    /* 0x34 */ {"4"}, /* 0x35 */ {"5"}, /* 0x36 */ {"6"}, /* 0x37 */ {"7"},
    /* 0x38 */ {"8"}, /* 0x39 */ {"9"}, /* 0x3A */ {":"}, /* 0x3B */ {";"},
    /* 0x3C */ {"<"}, /* 0x3D */ {"="}, /* 0x3E */ {">"}, /* 0x3F */ {"?"},
    /* 0x40 */ {"@"}, /* 0x41 */ {"A", "a"}, /* 0x42 */ {"B", "b"}, /* 0x43 */ {"C", "c"},
    /* 0x44 */ {"D", "d"}, /* 0x45 */ {"E", "e"}, /* 0x46 */ {"F", "f"}, /* 0x47 */ {"G", "g"},
    /* 0x48 */ {"H", "h"}, /* 0x49 */ {"I", "i"}, /* 0x4A */ {"J", "j"}, /* 0x4B */ {"K", "k"},
    /* 0x4C */ {"L", "l"}, /* 0x4D */ {"M", "m"}, /* 0x4E */ {"N", "n"}, /* 0x4F */ {"O", "o"},
    /* 0x50 */ {"P", "p"}, /* 0x51 */ {"Q", "q"}, /* 0x52 */ {"R", "r"}, /* 0x53 */ {"S", "s"},
    /* 0x54 */ {"T", "t"}, /* 0x55 */ {"U", "u"}, /* 0x56 */ {"V", "v"}, /* 0x57 */ {"W", "w"},
    /* 0x58 */ {"X", "x"}, /* 0x59 */ {"Y", "y"}, /* 0x5A */ {"Z", "z"}, /* 0x5B */ {"["},
    /* 0x5C */ {"\\"}, /* 0x5D */ {"]"}, /* 0x5E */ {"^"}, /* 0x5F */ {"_"},
    /* 0x60 */ {"`"}, /* 0x61 */ {"A"}, /* 0x62 */ {"B"}, /* 0x63 */ {"C"},
    /* 0x64 */ {"D"}, /* 0x65 */ {"E"}, /* 0x66 */ {"F"}, /* 0x67 */ {"G"},
    /* 0x68 */ {"H"}, /* 0x69 */ {"I"}, /* 0x6A */ {"J"}, /* 0x6B */ {"K"},
    /* 0x6C */ {"L"}, /* 0x6D */ {"M"}, /* 0x6E */ {"N"}, /* 0x6F */ {"O"},
    /* 0x70 */ {"P"}, /* 0x71 */ {"Q"}, /* 0x72 */ {"R"}, /* 0x73 */ {"S"},
    /* 0x74 */ {"T"}, /* 0x75 */ {"U"}, /* 0x76 */ {"V"}, /* 0x77 */ {"W"},
    /* 0x78 */ {"X"}, /* 0x79 */ {"Y"}, /* 0x7A */ {"Z"}, /* 0x7B */ {"{"},
    /* 0x7C */ {"|"}, /* 0x7D */ {"}"}, /* 0x7E */ {"~"}, /* 0x7F */ {},
};

} // namespace

const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagMultitoneAscii = {7, multitone};
