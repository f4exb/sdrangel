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

// The Cyrillic codepages of NEC Maxima/Compact, Philips PRG2220 and Swissphone DE506 (Partner too)

#include "radiopagingcharsettables.h"
#include "radiopagingcyrillic.h"

namespace {

#define ASCII_CONTROLS \
    /* 0x00 */ {}, /* 0x01 */ {}, /* 0x02 */ {}, /* 0x03 */ {}, \
    /* 0x04 */ {}, /* 0x05 */ {}, /* 0x06 */ {}, /* 0x07 */ {}, \
    /* 0x08 */ {}, /* 0x09 */ {}, /* 0x0A */ {"\xE2\x90\x8A" /* U+240A symbol for line feed */, "\n"}, /* 0x0B */ {}, \
    /* 0x0C */ {}, /* 0x0D */ {"\xE2\x90\x8D" /* U+240D symbol for carriage return */, "\r"}, /* 0x0E */ {}, /* 0x0F */ {}, \
    /* 0x10 */ {}, /* 0x11 */ {}, /* 0x12 */ {}, /* 0x13 */ {}, \
    /* 0x14 */ {}, /* 0x15 */ {}, /* 0x16 */ {}, /* 0x17 */ {}, \
    /* 0x18 */ {}, /* 0x19 */ {}, /* 0x1A */ {}, /* 0x1B */ {}, \
    /* 0x1C */ {}, /* 0x1D */ {}, /* 0x1E */ {}, /* 0x1F */ {}

// NEC Maxima, Compact, OI Electric PB2301 (the NEC 21A family, its manual layout): ASCII Latin at 0x41-0x5A (the
// Cyrillic letters that look alike are encoded there, A B C E H K M O P T X), the other Cyrillic letters at
// 0x61-0x77 in the alphabetical order, Dze at 0x7C
const RadioPagingCharset::Entry necMaximaCompact[128] = {
    ASCII_CONTROLS,
    /* 0x20 */ {" "}, /* 0x21 */ {"!"}, /* 0x22 */ {"\""}, /* 0x23 */ {"#"},
    /* 0x24 */ {"$"}, /* 0x25 */ {"%"}, /* 0x26 */ {"&"}, /* 0x27 */ {"'"},
    /* 0x28 */ {"("}, /* 0x29 */ {")"}, /* 0x2A */ {"*"}, /* 0x2B */ {"+"},
    /* 0x2C */ {","}, /* 0x2D */ {"-"}, /* 0x2E */ {"."}, /* 0x2F */ {"/"},
    /* 0x30 */ {"0"}, /* 0x31 */ {"1"}, /* 0x32 */ {"2"}, /* 0x33 */ {"3"},
    /* 0x34 */ {"4"}, /* 0x35 */ {"5"}, /* 0x36 */ {"6"}, /* 0x37 */ {"7"},
    /* 0x38 */ {"8"}, /* 0x39 */ {"9"}, /* 0x3A */ {":"}, /* 0x3B */ {";"},
    /* 0x3C */ {"<"}, /* 0x3D */ {"="}, /* 0x3E */ {">"}, /* 0x3F */ {"?"},
    /* 0x40 */ {"@"},
    /* 0x41 */ {"A", "a", Cyr::A, Cyr::a},
    /* 0x42 */ {"B", "b", Cyr::V, Cyr::v},
    /* 0x43 */ {"C", "c", Cyr::S, Cyr::s},
    /* 0x44 */ {"D", "d"},
    /* 0x45 */ {"E", "e", Cyr::E, Cyr::e},
    /* 0x46 */ {"F", "f"},
    /* 0x47 */ {"G", "g"},
    /* 0x48 */ {"H", "h", Cyr::N, Cyr::n},
    /* 0x49 */ {"I", "i"},
    /* 0x4A */ {"J", "j"},
    /* 0x4B */ {"K", "k", Cyr::K, Cyr::k},
    /* 0x4C */ {"L", "l"},
    /* 0x4D */ {"M", "m", Cyr::M, Cyr::m},
    /* 0x4E */ {"N", "n"},
    /* 0x4F */ {"O", "o", Cyr::O, Cyr::o},
    /* 0x50 */ {"P", "p", Cyr::R, Cyr::r},
    /* 0x51 */ {"Q", "q"},
    /* 0x52 */ {"R", "r"},
    /* 0x53 */ {"S", "s"},
    /* 0x54 */ {"T", "t", Cyr::T, Cyr::t},
    /* 0x55 */ {"U", "u"},
    /* 0x56 */ {"V", "v"},
    /* 0x57 */ {"W", "w"},
    /* 0x58 */ {"X", "x", Cyr::KH, Cyr::kh},
    /* 0x59 */ {"Y", "y"},
    /* 0x5A */ {"Z", "z"},
    /* 0x5B */ {"["}, /* 0x5C */ {"\\"}, /* 0x5D */ {"]"}, /* 0x5E */ {"^"},
    /* 0x5F */ {"_"},
    /* 0x60 */ {}, /* 0x61 */ {Cyr::B, Cyr::b}, /* 0x62 */ {Cyr::G, Cyr::g}, /* 0x63 */ {},
    /* 0x64 */ {Cyr::D, Cyr::d}, /* 0x65 */ {Cyr::YO, Cyr::yo}, /* 0x66 */ {Cyr::ZH, Cyr::zh},
    /* 0x67 */ {Cyr::Z, Cyr::z},
    /* 0x68 */ {Cyr::I, Cyr::i}, /* 0x69 */ {Cyr::J, Cyr::j}, /* 0x6A */ {Cyr::L, Cyr::l}, /* 0x6B */ {Cyr::P, Cyr::p},
    /* 0x6C */ {Cyr::U, Cyr::u}, /* 0x6D */ {Cyr::F, Cyr::f}, /* 0x6E */ {Cyr::TS, Cyr::ts}, /* 0x6F */ {Cyr::CH, Cyr::ch},
    /* 0x70 */ {Cyr::SH, Cyr::sh}, /* 0x71 */ {Cyr::SHCH, Cyr::shch}, /* 0x72 */ {Cyr::HARD, Cyr::hard}, /* 0x73 */ {Cyr::Y, Cyr::y},
    /* 0x74 */ {Cyr::SOFT, Cyr::soft}, /* 0x75 */ {Cyr::EH, Cyr::eh}, /* 0x76 */ {Cyr::YU, Cyr::yu}, /* 0x77 */ {Cyr::YA, Cyr::ya},
    /* 0x78 */ {}, /* 0x79 */ {}, /* 0x7A */ {}, /* 0x7B */ {"{"},
    /* 0x7C */ {Cyr::DZE, Cyr::dze}, /* 0x7D */ {"}"}, /* 0x7E */ {"~"}, /* 0x7F */ {},
};

// Philips PRG2220: ASCII to 0x5F, the Cyrillic uppercase letters in the alphabetical order at 0x60-0x7F (Yo taken
// for E)
const RadioPagingCharset::Entry philipsPrg2220[128] = {
    ASCII_CONTROLS,
    /* 0x20 */ {" "}, /* 0x21 */ {"!"}, /* 0x22 */ {"\""}, /* 0x23 */ {"#"},
    /* 0x24 */ {"$"}, /* 0x25 */ {"%"}, /* 0x26 */ {"&"}, /* 0x27 */ {"'"},
    /* 0x28 */ {"("}, /* 0x29 */ {")"}, /* 0x2A */ {"*"}, /* 0x2B */ {"+"},
    /* 0x2C */ {","}, /* 0x2D */ {"-"}, /* 0x2E */ {"."}, /* 0x2F */ {"/"},
    /* 0x30 */ {"0"}, /* 0x31 */ {"1"}, /* 0x32 */ {"2"}, /* 0x33 */ {"3"},
    /* 0x34 */ {"4"}, /* 0x35 */ {"5"}, /* 0x36 */ {"6"}, /* 0x37 */ {"7"},
    /* 0x38 */ {"8"}, /* 0x39 */ {"9"}, /* 0x3A */ {":"}, /* 0x3B */ {";"},
    /* 0x3C */ {"<"}, /* 0x3D */ {"="}, /* 0x3E */ {">"}, /* 0x3F */ {"?"},
    /* 0x40 */ {"@"}, /* 0x41 */ {"A"}, /* 0x42 */ {"B"}, /* 0x43 */ {"C"},
    /* 0x44 */ {"D"}, /* 0x45 */ {"E"}, /* 0x46 */ {"F"}, /* 0x47 */ {"G"},
    /* 0x48 */ {"H"}, /* 0x49 */ {"I"}, /* 0x4A */ {"J"}, /* 0x4B */ {"K"},
    /* 0x4C */ {"L"}, /* 0x4D */ {"M"}, /* 0x4E */ {"N"}, /* 0x4F */ {"O"},
    /* 0x50 */ {"P"}, /* 0x51 */ {"Q"}, /* 0x52 */ {"R"}, /* 0x53 */ {"S"},
    /* 0x54 */ {"T"}, /* 0x55 */ {"U"}, /* 0x56 */ {"V"}, /* 0x57 */ {"W"},
    /* 0x58 */ {"X"}, /* 0x59 */ {"Y"}, /* 0x5A */ {"Z"}, /* 0x5B */ {"["},
    /* 0x5C */ {"\\"}, /* 0x5D */ {"]"}, /* 0x5E */ {"^"}, /* 0x5F */ {"_"},
    /* 0x60 */ {Cyr::A}, /* 0x61 */ {Cyr::B}, /* 0x62 */ {Cyr::V}, /* 0x63 */ {Cyr::G},
    /* 0x64 */ {Cyr::D}, /* 0x65 */ {Cyr::E, Cyr::YO}, /* 0x66 */ {Cyr::ZH}, /* 0x67 */ {Cyr::Z},
    /* 0x68 */ {Cyr::I}, /* 0x69 */ {Cyr::J}, /* 0x6A */ {Cyr::K}, /* 0x6B */ {Cyr::L},
    /* 0x6C */ {Cyr::M}, /* 0x6D */ {Cyr::N}, /* 0x6E */ {Cyr::O}, /* 0x6F */ {Cyr::P},
    /* 0x70 */ {Cyr::R}, /* 0x71 */ {Cyr::S}, /* 0x72 */ {Cyr::T}, /* 0x73 */ {Cyr::U},
    /* 0x74 */ {Cyr::F}, /* 0x75 */ {Cyr::KH}, /* 0x76 */ {Cyr::TS}, /* 0x77 */ {Cyr::CH},
    /* 0x78 */ {Cyr::SH}, /* 0x79 */ {Cyr::SHCH}, /* 0x7A */ {Cyr::HARD}, /* 0x7B */ {Cyr::Y},
    /* 0x7C */ {Cyr::SOFT}, /* 0x7D */ {Cyr::EH}, /* 0x7E */ {Cyr::YU}, /* 0x7F */ {Cyr::YA},
};

// Swissphone DE506 Cyrillic firmware, Partner: the Cyrillic letters in the alphabetical order at 0x41-0x5F, Ya at
// 0x23 and Eh at 0x40; no lowercase codes. The glyph of a lookalike is the Cyrillic one; lowercase and the Latin
// lookalikes are encoded as the uppercase letter
const RadioPagingCharset::Entry swissphoneDe506[128] = {
    ASCII_CONTROLS,
    /* 0x20 */ {" "}, /* 0x21 */ {"!"}, /* 0x22 */ {"\""}, /* 0x23 */ {Cyr::YA, Cyr::ya},
    /* 0x24 */ {}, /* 0x25 */ {"%"}, /* 0x26 */ {"&"}, /* 0x27 */ {},
    /* 0x28 */ {"("}, /* 0x29 */ {")"}, /* 0x2A */ {"*"}, /* 0x2B */ {"+"},
    /* 0x2C */ {","}, /* 0x2D */ {"-"}, /* 0x2E */ {"."}, /* 0x2F */ {"/"},
    /* 0x30 */ {"0"}, /* 0x31 */ {"1"}, /* 0x32 */ {"2"}, /* 0x33 */ {"3"},
    /* 0x34 */ {"4"}, /* 0x35 */ {"5"}, /* 0x36 */ {"6"}, /* 0x37 */ {"7"},
    /* 0x38 */ {"8"}, /* 0x39 */ {"9"}, /* 0x3A */ {":"}, /* 0x3B */ {},
    /* 0x3C */ {}, /* 0x3D */ {"="}, /* 0x3E */ {}, /* 0x3F */ {"?"},
    /* 0x40 */ {Cyr::EH, Cyr::eh},
    /* 0x41 */ {Cyr::A, Cyr::a, "A", "a"},
    /* 0x42 */ {Cyr::B, Cyr::b},
    /* 0x43 */ {Cyr::V, Cyr::v, "B", "b"},
    /* 0x44 */ {Cyr::G, Cyr::g},
    /* 0x45 */ {Cyr::D, Cyr::d},
    /* 0x46 */ {Cyr::E, Cyr::e, Cyr::YO, Cyr::yo, "E", "e"},
    /* 0x47 */ {Cyr::ZH, Cyr::zh},
    /* 0x48 */ {Cyr::Z, Cyr::z},
    /* 0x49 */ {Cyr::I, Cyr::i},
    /* 0x4A */ {Cyr::J, Cyr::j},
    /* 0x4B */ {Cyr::K, Cyr::k, "K", "k"},
    /* 0x4C */ {Cyr::L, Cyr::l},
    /* 0x4D */ {Cyr::M, Cyr::m, "M", "m"},
    /* 0x4E */ {Cyr::N, Cyr::n, "H", "h"},
    /* 0x4F */ {Cyr::O, Cyr::o, "O", "o"},
    /* 0x50 */ {Cyr::P, Cyr::p},
    /* 0x51 */ {Cyr::R, Cyr::r, "P", "p"},
    /* 0x52 */ {Cyr::S, Cyr::s, "C", "c"},
    /* 0x53 */ {Cyr::T, Cyr::t, "T", "t"},
    /* 0x54 */ {Cyr::U, Cyr::u},
    /* 0x55 */ {Cyr::F, Cyr::f},
    /* 0x56 */ {Cyr::KH, Cyr::kh, "X", "x"},
    /* 0x57 */ {Cyr::TS, Cyr::ts},
    /* 0x58 */ {Cyr::CH, Cyr::ch},
    /* 0x59 */ {Cyr::SH, Cyr::sh},
    /* 0x5A */ {Cyr::SHCH, Cyr::shch},
    /* 0x5B */ {Cyr::HARD, Cyr::hard},
    /* 0x5C */ {Cyr::Y, Cyr::y},
    /* 0x5D */ {"]"},
    /* 0x5E */ {Cyr::SOFT, Cyr::soft},
    /* 0x5F */ {Cyr::YU, Cyr::yu},
    /* 0x60 */ {}, /* 0x61 */ {}, /* 0x62 */ {}, /* 0x63 */ {},
    /* 0x64 */ {}, /* 0x65 */ {}, /* 0x66 */ {}, /* 0x67 */ {},
    /* 0x68 */ {}, /* 0x69 */ {}, /* 0x6A */ {}, /* 0x6B */ {},
    /* 0x6C */ {}, /* 0x6D */ {}, /* 0x6E */ {}, /* 0x6F */ {},
    /* 0x70 */ {}, /* 0x71 */ {}, /* 0x72 */ {}, /* 0x73 */ {},
    /* 0x74 */ {}, /* 0x75 */ {}, /* 0x76 */ {}, /* 0x77 */ {},
    /* 0x78 */ {}, /* 0x79 */ {}, /* 0x7A */ {}, /* 0x7B */ {"{"},
    /* 0x7C */ {"|"}, /* 0x7D */ {"}"}, /* 0x7E */ {}, /* 0x7F */ {},
};

#undef ASCII_CONTROLS

} // namespace

const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagNecMaximaCompactCyrillic = {7, necMaximaCompact};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagPhilipsPrg2220LatinCyrillic = {7, philipsPrg2220};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagSwissphoneDe506Cyrillic = {7, swissphoneDe506};
