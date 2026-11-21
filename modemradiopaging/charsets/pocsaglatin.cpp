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

// The national variants of 7-bit ASCII of the POCSAG pagers: ASCII with the national characters on some of the
// codes 0x23, 0x40, 0x5B-0x60 and 0x7B-0x7E. Each table is the ASCII table with its replaced codes; glyphs are
// UTF-8 escapes with their code point

#include <array>
#include <initializer_list>
#include <utility>

#include "radiopagingcharsettables.h"

namespace {

typedef std::array<RadioPagingCharset::Entry, 128> Entries;

Entries national(std::initializer_list<std::pair<int, const char*>> replaced)
{
    Entries entries;

    for (int code = 0; code < 128; code++)
    {
        for (int i = 0; i < RadioPagingCharset::m_maxGlyphs; i++) {
            entries[code][i] = RadioPagingCharsetTables::pocsagAscii.m_entries[code][i];
        }
    }

    for (const auto& r : replaced)
    {
        for (int i = 0; i < RadioPagingCharset::m_maxGlyphs; i++) {
            entries[r.first][i] = nullptr;
        }
        entries[r.first][0] = r.second;
    }

    return entries;
}

const Entries british = national({
    {0x23, "\xC2\xA3"},     // U+00A3 pound
    {0x7E, "-"},
});

const Entries danishNorwegian = national({
    {0x40, "\xC2\xA7"},     // U+00A7 section
    {0x5B, "\xC3\x86"},     // U+00C6 AE
    {0x5C, "\xC3\x98"},     // U+00D8 O stroke
    {0x5D, "\xC3\x85"},     // U+00C5 A ring
    {0x7B, "\xC5\x93"},     // U+0153 oe
    {0x7C, "\xC3\xB8"},     // U+00F8 o stroke
    {0x7D, "\xC3\xA5"},     // U+00E5 a ring
    {0x7E, "-"},
});

const Entries french = national({
    {0x23, "\xC2\xA3"},     // U+00A3 pound
    {0x40, "\xC3\xA0"},     // U+00E0 a grave
    {0x5B, "\xC2\xB0"},     // U+00B0 degree
    {0x5C, "\xC3\xA7"},     // U+00E7 c cedilla
    {0x5D, "\xC2\xA7"},     // U+00A7 section
    {0x60, "\xC3\xA9"},     // U+00E9 e acute
    {0x7B, "\xE2\x80\x99"}, // U+2019 right single quotation mark
    {0x7C, "\xC3\xB9"},     // U+00F9 u grave
    {0x7D, "\xC3\xA8"},     // U+00E8 e grave
    {0x7E, "-"},
});

const Entries german = national({
    {0x40, "\xC2\xA7"},     // U+00A7 section
    {0x5B, "\xC3\x84"},     // U+00C4 A umlaut
    {0x5C, "\xC3\x96"},     // U+00D6 O umlaut
    {0x5D, "\xC3\x9C"},     // U+00DC U umlaut
    {0x7B, "\xC3\xA4"},     // U+00E4 a umlaut
    {0x7C, "\xC3\xB6"},     // U+00F6 o umlaut
    {0x7D, "\xC3\xBC"},     // U+00FC u umlaut
    {0x7E, "\xC3\x9F"},     // U+00DF sharp s
});

const Entries italian = national({
    {0x23, "\xC2\xA3"},     // U+00A3 pound
    {0x40, "\xC2\xA7"},     // U+00A7 section
    {0x5B, "\xC2\xB0"},     // U+00B0 degree
    {0x5C, "\xC3\xA7"},     // U+00E7 c cedilla
    {0x5D, "\xC3\xA9"},     // U+00E9 e acute
    {0x60, "\xC3\xB9"},     // U+00F9 u grave
    {0x7B, "\xC3\xA0"},     // U+00E0 a grave
    {0x7C, "\xC3\xB2"},     // U+00F2 o grave
    {0x7D, "\xC3\xA8"},     // U+00E8 e grave
    {0x7E, "\xC3\x8C"},     // U+00CC I grave
});

const Entries portuguese = national({
    {0x23, "\xC2\xA3"},     // U+00A3 pound
    {0x7E, "-"},
});

const Entries spanish = national({
    {0x23, "\xC2\xA1"},     // U+00A1 inverted exclamation mark
    {0x5B, "\xC3\xA7"},     // U+00E7 c cedilla
    {0x5C, "\xC3\x91"},     // U+00D1 N tilde
    {0x5D, "\xC2\xBA"},     // U+00BA masculine ordinal
    {0x5E, "\xC2\xAA"},     // U+00AA feminine ordinal
    {0x60, "\xC2\xBF"},     // U+00BF inverted question mark
    {0x7B, "\xC3\xA7"},     // U+00E7 c cedilla
    {0x7C, "\xC3\xA5"},     // U+00E5 a ring
    {0x7D, "\xC3\x9C"},     // U+00DC U umlaut
    {0x7E, "\xC3\xBC"},     // U+00FC u umlaut
});

const Entries swedish = national({
    {0x23, "\xC2\xA3"},     // U+00A3 pound
    {0x40, "\xC3\x89"},     // U+00C9 E acute
    {0x5B, "\xC3\x84"},     // U+00C4 A umlaut
    {0x5C, "\xC3\x96"},     // U+00D6 O umlaut
    {0x5D, "\xC3\x85"},     // U+00C5 A ring
    {0x5E, "\xC3\x9C"},     // U+00DC U umlaut
    {0x60, "\xC3\xA9"},     // U+00E9 e acute
    {0x7B, "\xC3\xA4"},     // U+00E4 a umlaut
    {0x7C, "\xC3\xB6"},     // U+00F6 o umlaut
    {0x7D, "\xC3\xA5"},     // U+00E5 a ring
    {0x7E, "\xC3\xBC"},     // U+00FC u umlaut
});

const Entries swiss = national({
    {0x23, "\xC3\xB9"},     // U+00F9 u grave
    {0x40, "\xC3\xA0"},     // U+00E0 a grave
    {0x5B, "\xC3\xA9"},     // U+00E9 e acute
    {0x5C, "\xC3\xA7"},     // U+00E7 c cedilla
    {0x5D, "\xC3\xAA"},     // U+00EA e circumflex
    {0x5E, "\xC3\xAE"},     // U+00EE i circumflex
    {0x5F, "\xC3\xA8"},     // U+00E8 e grave
    {0x60, "\xC3\xB2"},     // U+00F2 o grave
    {0x7B, "\xC3\xA0"},     // U+00E0 a grave
    {0x7C, "\xC3\xB2"},     // U+00F2 o grave
    {0x7D, "\xC3\xA8"},     // U+00E8 e grave
    {0x7E, "\xC3\xBB"},     // U+00FB u circumflex
});

// Hebrew (SI 960): the letters alef to tav, with the final forms, on the codes 0x60 to 0x7A, in place of the lower
// case letters
const Entries hebrew = national({
    {0x60, "\xD7\x90"},     // U+05D0 alef
    {0x61, "\xD7\x91"},     // U+05D1 bet
    {0x62, "\xD7\x92"},     // U+05D2 gimel
    {0x63, "\xD7\x93"},     // U+05D3 dalet
    {0x64, "\xD7\x94"},     // U+05D4 he
    {0x65, "\xD7\x95"},     // U+05D5 vav
    {0x66, "\xD7\x96"},     // U+05D6 zayin
    {0x67, "\xD7\x97"},     // U+05D7 het
    {0x68, "\xD7\x98"},     // U+05D8 tet
    {0x69, "\xD7\x99"},     // U+05D9 yod
    {0x6A, "\xD7\x9A"},     // U+05DA final kaf
    {0x6B, "\xD7\x9B"},     // U+05DB kaf
    {0x6C, "\xD7\x9C"},     // U+05DC lamed
    {0x6D, "\xD7\x9D"},     // U+05DD final mem
    {0x6E, "\xD7\x9E"},     // U+05DE mem
    {0x6F, "\xD7\x9F"},     // U+05DF final nun
    {0x70, "\xD7\xA0"},     // U+05E0 nun
    {0x71, "\xD7\xA1"},     // U+05E1 samekh
    {0x72, "\xD7\xA2"},     // U+05E2 ayin
    {0x73, "\xD7\xA3"},     // U+05E3 final pe
    {0x74, "\xD7\xA4"},     // U+05E4 pe
    {0x75, "\xD7\xA5"},     // U+05E5 final tsadi
    {0x76, "\xD7\xA6"},     // U+05E6 tsadi
    {0x77, "\xD7\xA7"},     // U+05E7 qof
    {0x78, "\xD7\xA8"},     // U+05E8 resh
    {0x79, "\xD7\xA9"},     // U+05E9 shin
    {0x7A, "\xD7\xAA"},     // U+05EA tav
});

} // namespace

const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagLatinHebrew = {7, hebrew.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagLatinBritish = {7, british.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagLatinDanishNorwegian = {7, danishNorwegian.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagLatinFrench = {7, french.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagLatinGerman = {7, german.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagLatinItalian = {7, italian.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagLatinPortuguese = {7, portuguese.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagLatinSpanish = {7, spanish.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagLatinSwedish = {7, swedish.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::pocsagLatinSwiss = {7, swiss.data()};
