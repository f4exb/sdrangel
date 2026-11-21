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

// NEC 21A uppercase Latin and Cyrillic (checked on pagers). The layout of the Advisor Linguist one but: the
// Serbian and Macedonian letters at 0x10-0x16 (I grave, Lje, Nje, Dje, Kje, short U, Dzhe), no symbols in the
// control codes, 0x5C 0x5E 0x5F 0x7B 0x7D ASCII, Dze at 0x7C; Dje at 0x13 and 0x78. The glyph of a lookalike
// is the Latin one; lowercase is encoded as uppercase

#include "radiopagingcharsettables.h"
#include "radiopagingcyrillic.h"

namespace {

const RadioPagingCharset::Entry entries[128] = {
    /* 0x00 */ {}, /* 0x01 */ {}, /* 0x02 */ {}, /* 0x03 */ {},
    /* 0x04 */ {}, /* 0x05 */ {}, /* 0x06 */ {}, /* 0x07 */ {},
    /* 0x08 */ {}, /* 0x09 */ {}, /* 0x0A */ {"\xE2\x90\x8A" /* U+240A symbol for line feed */, "\n"}, /* 0x0B */ {},
    /* 0x0C */ {}, /* 0x0D */ {"\xE2\x90\x8D" /* U+240D symbol for carriage return */, "\r"}, /* 0x0E */ {}, /* 0x0F */ {},
    /* 0x10 */ {Cyr::I_GRAVE, Cyr::i_grave}, /* 0x11 */ {Cyr::LJE, Cyr::lje}, /* 0x12 */ {Cyr::NJE, Cyr::nje},
    /* 0x13 */ {Cyr::DJE, Cyr::dje},
    /* 0x14 */ {Cyr::KJE, Cyr::kje}, /* 0x15 */ {Cyr::SHORT_U, Cyr::short_u}, /* 0x16 */ {Cyr::DZHE, Cyr::dzhe}, /* 0x17 */ {},
    /* 0x18 */ {}, /* 0x19 */ {}, /* 0x1A */ {}, /* 0x1B */ {},
    /* 0x1C */ {}, /* 0x1D */ {}, /* 0x1E */ {}, /* 0x1F */ {},
    /* 0x20 */ {" "}, /* 0x21 */ {"!"}, /* 0x22 */ {"\""}, /* 0x23 */ {"#"},
    /* 0x24 */ {"$"}, /* 0x25 */ {"%"}, /* 0x26 */ {"&"}, /* 0x27 */ {"'", "`"},
    /* 0x28 */ {"("}, /* 0x29 */ {")"}, /* 0x2A */ {"*"}, /* 0x2B */ {"+"},
    /* 0x2C */ {","}, /* 0x2D */ {"-", "\xE2\x80\x94" /* U+2014 em dash */}, /* 0x2E */ {"."}, /* 0x2F */ {"/"},
    /* 0x30 */ {"0"}, /* 0x31 */ {"1"}, /* 0x32 */ {"2"}, /* 0x33 */ {"3"},
    /* 0x34 */ {"4"}, /* 0x35 */ {"5"}, /* 0x36 */ {"6"}, /* 0x37 */ {"7"},
    /* 0x38 */ {"8"}, /* 0x39 */ {"9"}, /* 0x3A */ {":"}, /* 0x3B */ {";"},
    /* 0x3C */ {"<"}, /* 0x3D */ {"="}, /* 0x3E */ {">"}, /* 0x3F */ {"?"},
    /* 0x40 */ {"@"},
    /* 0x41 */ {"A", Cyr::A, "a", Cyr::a},
    /* 0x42 */ {"B", Cyr::V, "b", Cyr::v},
    /* 0x43 */ {"C", Cyr::S, "c", Cyr::s},
    /* 0x44 */ {"D", "d"},
    /* 0x45 */ {"E", Cyr::E, "e", Cyr::e},
    /* 0x46 */ {"F", "f"},
    /* 0x47 */ {"G", "g"},
    /* 0x48 */ {"H", Cyr::N, "h", Cyr::n},
    /* 0x49 */ {"I", Cyr::UKR_I, "i", Cyr::ukr_i, "\xC9\xAA" /* U+026A small capital I */},
    /* 0x4A */ {"J", "j"},
    /* 0x4B */ {"K", Cyr::K, "k", Cyr::k},
    /* 0x4C */ {"L", "l"},
    /* 0x4D */ {"M", Cyr::M, "m", Cyr::m},
    /* 0x4E */ {"N", "n"},
    /* 0x4F */ {"O", Cyr::O, "o", Cyr::o},
    /* 0x50 */ {"P", Cyr::R, "p", Cyr::r},
    /* 0x51 */ {"Q", "q"},
    /* 0x52 */ {"R", "r"},
    /* 0x53 */ {"S", "s"},
    /* 0x54 */ {"T", Cyr::T, "t", Cyr::t},
    /* 0x55 */ {"U", "u"},
    /* 0x56 */ {"V", "v"},
    /* 0x57 */ {"W", "w"},
    /* 0x58 */ {"X", Cyr::KH, "x", Cyr::kh},
    /* 0x59 */ {"Y", "y"},
    /* 0x5A */ {"Z", "z"},
    /* 0x5B */ {"["}, /* 0x5C */ {"\\"}, /* 0x5D */ {"]"}, /* 0x5E */ {"^"},
    /* 0x5F */ {"_"},
    /* 0x60 */ {"`"}, /* 0x61 */ {Cyr::B, Cyr::b}, /* 0x62 */ {Cyr::G, Cyr::g},
    /* 0x63 */ {Cyr::GJE, Cyr::GHE_UP, Cyr::gje, Cyr::ghe_up},
    /* 0x64 */ {Cyr::D, Cyr::d}, /* 0x65 */ {Cyr::YO, Cyr::yo}, /* 0x66 */ {Cyr::ZH, Cyr::zh}, /* 0x67 */ {Cyr::Z, Cyr::z},
    /* 0x68 */ {Cyr::I, Cyr::i}, /* 0x69 */ {Cyr::J, Cyr::j}, /* 0x6A */ {Cyr::L, Cyr::l}, /* 0x6B */ {Cyr::P, Cyr::p},
    /* 0x6C */ {Cyr::U, Cyr::u}, /* 0x6D */ {Cyr::F, Cyr::f}, /* 0x6E */ {Cyr::TS, Cyr::ts}, /* 0x6F */ {Cyr::CH, Cyr::ch},
    /* 0x70 */ {Cyr::SH, Cyr::sh}, /* 0x71 */ {Cyr::SHCH, Cyr::shch}, /* 0x72 */ {Cyr::HARD, Cyr::hard}, /* 0x73 */ {Cyr::Y, Cyr::y},
    /* 0x74 */ {Cyr::SOFT, Cyr::soft}, /* 0x75 */ {Cyr::EH, Cyr::eh}, /* 0x76 */ {Cyr::YU, Cyr::yu}, /* 0x77 */ {Cyr::YA, Cyr::ya},
    /* 0x78 */ {Cyr::DJE, Cyr::dje}, /* 0x79 */ {Cyr::UKR_IE, Cyr::ukr_ie},
    /* 0x7A */ {Cyr::YI, "\xC3\x8F" /* U+00CF I diaeresis */, Cyr::yi, "\xC3\xAF" /* U+00EF i diaeresis */},
    /* 0x7B */ {"{"},
    /* 0x7C */ {Cyr::DZE, Cyr::dze}, /* 0x7D */ {"}"}, /* 0x7E */ {"~"}, /* 0x7F */ {},
};

} // namespace

const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagNec21aLatinCyrillic = {7, entries};
