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

// Motorola Advisor and Scriptor LX1 Cyrillic (checked on pagers): 0x20-0x3F ASCII, 0x40-0x5E Cyrillic lowercase in
// the phonetic order (KOI-7), 0x5F underscore, 0x60-0x7E the uppercase letters in the same order. Yo is taken for
// E and the soft sign for the hard sign: they share a code

#include "radiopagingcharsettables.h"
#include "radiopagingcyrillic.h"

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
    /* 0x24 */ {"$"}, /* 0x25 */ {"%"}, /* 0x26 */ {"&"}, /* 0x27 */ {"'"},
    /* 0x28 */ {"("}, /* 0x29 */ {")"}, /* 0x2A */ {"*"}, /* 0x2B */ {"+"},
    /* 0x2C */ {","}, /* 0x2D */ {"-"}, /* 0x2E */ {"."}, /* 0x2F */ {"/"},
    /* 0x30 */ {"0"}, /* 0x31 */ {"1"}, /* 0x32 */ {"2"}, /* 0x33 */ {"3"},
    /* 0x34 */ {"4"}, /* 0x35 */ {"5"}, /* 0x36 */ {"6"}, /* 0x37 */ {"7"},
    /* 0x38 */ {"8"}, /* 0x39 */ {"9"}, /* 0x3A */ {":"}, /* 0x3B */ {";"},
    /* 0x3C */ {"<"}, /* 0x3D */ {"="}, /* 0x3E */ {">"}, /* 0x3F */ {"?"},
    /* 0x40 */ {Cyr::yu}, /* 0x41 */ {Cyr::a}, /* 0x42 */ {Cyr::b}, /* 0x43 */ {Cyr::ts},
    /* 0x44 */ {Cyr::d}, /* 0x45 */ {Cyr::e, Cyr::yo}, /* 0x46 */ {Cyr::f}, /* 0x47 */ {Cyr::g},
    /* 0x48 */ {Cyr::kh}, /* 0x49 */ {Cyr::i}, /* 0x4A */ {Cyr::j}, /* 0x4B */ {Cyr::k},
    /* 0x4C */ {Cyr::l}, /* 0x4D */ {Cyr::m}, /* 0x4E */ {Cyr::n}, /* 0x4F */ {Cyr::o},
    /* 0x50 */ {Cyr::p}, /* 0x51 */ {Cyr::ya}, /* 0x52 */ {Cyr::r}, /* 0x53 */ {Cyr::s},
    /* 0x54 */ {Cyr::t}, /* 0x55 */ {Cyr::u}, /* 0x56 */ {Cyr::zh}, /* 0x57 */ {Cyr::v},
    /* 0x58 */ {Cyr::hard, Cyr::soft}, /* 0x59 */ {Cyr::y}, /* 0x5A */ {Cyr::z}, /* 0x5B */ {Cyr::sh},
    /* 0x5C */ {Cyr::eh}, /* 0x5D */ {Cyr::shch}, /* 0x5E */ {Cyr::ch}, /* 0x5F */ {"_"},
    /* 0x60 */ {Cyr::YU}, /* 0x61 */ {Cyr::A}, /* 0x62 */ {Cyr::B}, /* 0x63 */ {Cyr::TS},
    /* 0x64 */ {Cyr::D}, /* 0x65 */ {Cyr::E, Cyr::YO}, /* 0x66 */ {Cyr::F}, /* 0x67 */ {Cyr::G},
    /* 0x68 */ {Cyr::KH}, /* 0x69 */ {Cyr::I}, /* 0x6A */ {Cyr::J}, /* 0x6B */ {Cyr::K},
    /* 0x6C */ {Cyr::L}, /* 0x6D */ {Cyr::M}, /* 0x6E */ {Cyr::N}, /* 0x6F */ {Cyr::O},
    /* 0x70 */ {Cyr::P}, /* 0x71 */ {Cyr::YA}, /* 0x72 */ {Cyr::R}, /* 0x73 */ {Cyr::S},
    /* 0x74 */ {Cyr::T}, /* 0x75 */ {Cyr::U}, /* 0x76 */ {Cyr::ZH}, /* 0x77 */ {Cyr::V},
    /* 0x78 */ {Cyr::HARD, Cyr::SOFT}, /* 0x79 */ {Cyr::Y}, /* 0x7A */ {Cyr::Z}, /* 0x7B */ {Cyr::SH},
    /* 0x7C */ {Cyr::EH}, /* 0x7D */ {Cyr::SHCH}, /* 0x7E */ {Cyr::CH}, /* 0x7F */ {},
};

} // namespace

const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagMotorolaAdvisorCyrillic = {7, entries};
