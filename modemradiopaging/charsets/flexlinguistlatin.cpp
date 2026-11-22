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

// Motorola Linguist FLEX codepages of the Latin alphabets: ASCII with the national letters on some codes. Glyphs
// other than ASCII are UTF-8 escapes with their code point

#include "radiopagingcharsetbuild.h"

using namespace RadioPagingCharsetTables;

namespace {

// Polish: the letters on 0x40, 0x5B-0x5E, 0x60 and 0x7B-0x7E (L stroke on 0x5C and 0x7C)
const Entries7 polish = fromAscii({
    {0x40, {"\xC4\x98"}},   // U+0118 E ogonek
    {0x5B, {"\xC5\xB9"}},   // U+0179 Z acute
    {0x5C, {"\xC5\x81"}},   // U+0141 L stroke
    {0x5D, {"\xC5\x83"}},   // U+0143 N acute
    {0x5E, {"\xC5\x9A"}},   // U+015A S acute
    {0x60, {"\xC4\x84"}},   // U+0104 A ogonek
    {0x7B, {"\xC3\x93"}},   // U+00D3 O acute
    {0x7C, {"\xC5\x81"}},   // U+0141 L stroke
    {0x7D, {"\xC5\xBB"}},   // U+017B Z dot
    {0x7E, {"\xC4\x86"}},   // U+0106 C acute
});

// Icelandic: the letters on control codes 0x02-0x0F and 0x12-0x1F, ASCII from 0x20
const Entries7 icelandic = fromAscii({
    {0x02, {"\xC3\x81"}},   // U+00C1 A acute
    {0x03, {"\xC3\xA1"}},   // U+00E1 a acute
    {0x04, {"\xC3\x8D"}},   // U+00CD I acute
    {0x05, {"\xC3\xAD"}},   // U+00ED i acute
    {0x06, {"\xC3\x96"}},   // U+00D6 O umlaut
    {0x07, {"\xC3\xB6"}},   // U+00F6 o umlaut
    {0x09, {"\xC3\xB0"}},   // U+00F0 eth
    {0x0B, {"\xC3\x9D"}},   // U+00DD Y acute
    {0x0C, {"\xC3\xBD"}},   // U+00FD y acute
    {0x0E, {"\xC3\x9A"}},   // U+00DA U acute
    {0x0F, {"\xC3\xBA"}},   // U+00FA u acute
    {0x12, {"\xC3\x81"}},   // U+00C1 A acute
    {0x13, {"\xC3\xA1"}},   // U+00E1 a acute
    {0x14, {"\xC3\x8D"}},   // U+00CD I acute
    {0x15, {"\xC3\xAD"}},   // U+00ED i acute
    {0x16, {"\xC3\x93"}},   // U+00D3 O acute
    {0x17, {"\xC3\xB3"}},   // U+00F3 o acute
    {0x18, {"\xC3\x9E"}},   // U+00DE thorn
    {0x19, {"\xC3\xBE"}},   // U+00FE thorn
    {0x1A, {"\xC3\x90"}},   // U+00D0 eth
    {0x1C, {"\xC3\x86"}},   // U+00C6 AE
    {0x1D, {"\xC3\xA6"}},   // U+00E6 ae
    {0x1E, {"\xC3\x89"}},   // U+00C9 E acute
    {0x1F, {"\xC3\xA9"}},   // U+00E9 e acute
});

// Latin-1 (ISO 8859-1 letters): symbols on control codes, the accented uppercase letters on 0x5E-0x7F
const Entries7 latin1 = fromAscii({
    {0x01, {"\xC2\xA4"}},   // U+00A4 currency
    {0x02, {"\xC2\xB5"}},   // U+00B5 micro
    {0x05, {"\xC2\xA2"}},   // U+00A2 cent
    {0x06, {"\xC3\x97"}},   // U+00D7 multiplication
    {0x07, {"\xC3\xB7"}},   // U+00F7 division
    {0x08, {"\xC2\xA3"}},   // U+00A3 pound
    {0x09, {"\xC2\xB0"}},   // U+00B0 degree
    {0x0B, {"\xC2\xAB"}},   // U+00AB left guillemet
    {0x0C, {"\xC2\xBF"}},   // U+00BF inverted question mark
    {0x0E, {"\xC2\xBB"}},   // U+00BB right guillemet
    {0x0F, {"\xC2\xA1"}},   // U+00A1 inverted exclamation mark
    {0x10, {"\xC2\xAC"}},   // U+00AC not
    {0x11, {"\xC2\xAF"}},   // U+00AF macron
    {0x12, {"^"}},
    {0x13, {"_"}},
    {0x14, {"\xC2\xA7"}},   // U+00A7 section
    {0x15, {"\xC2\xB2"}},   // U+00B2 superscript 2
    {0x16, {"|"}},
    {0x18, {"\xC2\xB3"}},   // U+00B3 superscript 3
    {0x19, {"~"}},
    {0x1A, {"\xC2\xB7"}},   // U+00B7 middle dot
    {0x1C, {"\xC2\xA5"}},   // U+00A5 yen
    {0x1D, {"\xC2\xB1"}},   // U+00B1 plus-minus
    {0x1E, {"\xC2\xB7"}},   // U+00B7 middle dot
    {0x5E, {"\xC3\x9E"}},   // U+00DE thorn
    {0x5F, {"\xC3\x9D"}},   // U+00DD Y acute
    {0x61, {"\xC3\x81"}},   // U+00C1 A acute
    {0x62, {"\xC3\x82"}},   // U+00C2 A circumflex
    {0x63, {"\xC3\x83"}},   // U+00C3 A tilde
    {0x64, {"\xC3\x84"}},   // U+00C4 A umlaut
    {0x65, {"\xC3\x85"}},   // U+00C5 A ring
    {0x66, {"\xC3\x86"}},   // U+00C6 AE
    {0x67, {"\xC3\x87"}},   // U+00C7 C cedilla
    {0x68, {"\xC3\x88"}},   // U+00C8 E grave
    {0x69, {"\xC3\x89"}},   // U+00C9 E acute
    {0x6A, {"\xC3\x8A"}},   // U+00CA E circumflex
    {0x6B, {"\xC3\x8B"}},   // U+00CB E umlaut
    {0x6C, {"\xC3\x8C"}},   // U+00CC I grave
    {0x6D, {"\xC3\x8D"}},   // U+00CD I acute
    {0x6E, {"\xC3\x8E"}},   // U+00CE I circumflex
    {0x6F, {"\xC3\x8F"}},   // U+00CF I umlaut
    {0x70, {"\xC3\x90"}},   // U+00D0 eth
    {0x71, {"\xC3\x80"}},   // U+00C0 A grave
    {0x72, {"\xC3\x93"}},   // U+00D3 O acute
    {0x73, {"\xC3\x94"}},   // U+00D4 O circumflex
    {0x74, {"\xC3\x95"}},   // U+00D5 O tilde
    {0x75, {"\xC3\x98"}},   // U+00D8 O stroke
    {0x76, {"\xC3\x9A"}},   // U+00DA U acute
    {0x77, {"\xC3\x9B"}},   // U+00DB U circumflex
    {0x78, {"\xC3\x9C"}},   // U+00DC U umlaut
    {0x79, {"\xC3\x99"}},   // U+00D9 U grave
    {0x7A, {"{"}},
    {0x7C, {"}"}},
    {0x7E, {"\xC3\x91"}},   // U+00D1 N tilde
});

// Turkish: the Latin-1 table with the Turkish letters S cedilla, I dot and G breve
const Entries7 turkish = replace(latin1, {
    {0x5E, {"\xC5\x9E"}},   // U+015E S cedilla
    {0x5F, {"\xC4\xB0"}},   // U+0130 I dot
    {0x70, {"\xC4\x9E"}},   // U+011E G breve
    {0x75, {"\xC3\x96"}},   // U+00D6 O umlaut
    {0x76, {"\xC3\x98"}},   // U+00D8 O stroke
    {0x77, {"\xC3\x9A"}},   // U+00DA U acute
    {0x78, {"\xC3\x9B"}},   // U+00DB U circumflex
    {0x79, {"\xC3\x9C"}},   // U+00DC U umlaut
    {0x7A, {"\xC3\x99"}},   // U+00D9 U grave
    {0x7C, {"\xC3\x92"}},   // U+00D2 O grave
});

// Latin-2 (ISO 8859-2 letters): symbols and ASCII punctuation on control codes, the letters on 0x3B-0x3E, 0x40,
// 0x5B-0x5F and 0x61-0x7E
const Entries7 latin2 = fromAscii({
    {0x01, {"\xC2\xA4"}},   // U+00A4 currency
    {0x02, {"\xC2\xB5"}},   // U+00B5 micro
    {0x05, {"\xC2\xA2"}},   // U+00A2 cent
    {0x06, {"\xC3\x97"}},   // U+00D7 multiplication
    {0x07, {"\xC3\xB7"}},   // U+00F7 division
    {0x08, {"\xC2\xA3"}},   // U+00A3 pound
    {0x09, {"\xC2\xB0"}},   // U+00B0 degree
    {0x0B, {"="}},
    {0x0C, {">"}},
    {0x0E, {"@"}},
    {0x0F, {"["}},
    {0x10, {"\\"}},
    {0x11, {"]"}},
    {0x12, {"^"}},
    {0x13, {"_"}},
    {0x14, {"\xC2\xA7"}},   // U+00A7 section
    {0x15, {"{"}},
    {0x16, {"|"}},
    {0x18, {"}"}},
    {0x19, {"~"}},
    {0x1A, {"\xC2\xB7"}},   // U+00B7 middle dot
    {0x1C, {"\xC2\xA5"}},   // U+00A5 yen
    {0x1D, {"\xC2\xB1"}},   // U+00B1 plus-minus
    {0x1E, {";"}},
    {0x3B, {"\xC5\x81"}},   // U+0141 L stroke
    {0x3C, {"\xC4\xBD"}},   // U+013D L caron
    {0x3D, {"\xC5\xA4"}},   // U+0164 T caron
    {0x3E, {"\xC5\x87"}},   // U+0147 N caron
    {0x40, {"\xC4\x84"}},   // U+0104 A ogonek
    {0x5B, {"\xC4\xB4"}},   // U+0134 J circumflex
    {0x5C, {"\xC5\xBB"}},   // U+017B Z dot
    {0x5D, {"\xC5\xAE"}},   // U+016E U ring
    {0x5E, {"\xC5\x9E"}},   // U+015E S cedilla
    {0x5F, {"\xC3\x9D"}},   // U+00DD Y acute
    {0x61, {"\xC3\x81"}},   // U+00C1 A acute
    {0x62, {"\xC3\x82"}},   // U+00C2 A circumflex
    {0x63, {"\xC4\x82"}},   // U+0102 A breve
    {0x64, {"\xC3\x84"}},   // U+00C4 A umlaut
    {0x65, {"\xC4\xB9"}},   // U+0139 L acute
    {0x66, {"\xC4\x86"}},   // U+0106 C acute
    {0x67, {"\xC3\x87"}},   // U+00C7 C cedilla
    {0x68, {"\xC4\x8C"}},   // U+010C C caron
    {0x69, {"\xC3\x89"}},   // U+00C9 E acute
    {0x6A, {"\xC4\x98"}},   // U+0118 E ogonek
    {0x6B, {"\xC3\x8B"}},   // U+00CB E umlaut
    {0x6C, {"\xC4\x9A"}},   // U+011A E caron
    {0x6D, {"\xC3\x8D"}},   // U+00CD I acute
    {0x6E, {"\xC3\x8E"}},   // U+00CE I circumflex
    {0x6F, {"\xC4\x8E"}},   // U+010E D caron
    {0x70, {"\xC4\x90"}},   // U+0110 D stroke
    {0x71, {"\xC5\x94"}},   // U+0154 R acute
    {0x72, {"\xC3\x93"}},   // U+00D3 O acute
    {0x73, {"\xC3\x94"}},   // U+00D4 O circumflex
    {0x74, {"\xC5\x90"}},   // U+0150 O double acute
    {0x75, {"\xC3\x96"}},   // U+00D6 O umlaut
    {0x76, {"\xC5\x98"}},   // U+0158 R caron
    {0x77, {"\xC3\x9A"}},   // U+00DA U acute
    {0x78, {"\xC5\xB0"}},   // U+0170 U double acute
    {0x79, {"\xC3\x9C"}},   // U+00DC U umlaut
    {0x7A, {"\xC5\xBD"}},   // U+017D Z caron
    {0x7B, {"\xC5\xB9"}},   // U+0179 Z acute
    {0x7C, {"\xC5\x83"}},   // U+0143 N acute
    {0x7D, {"\xC5\x9A"}},   // U+015A S acute
    {0x7E, {"\xC5\xA0"}},   // U+0160 S caron
});

// Latin-3 (ISO 8859-3 letters): the Latin-2 table with the letters of Maltese and Esperanto
const Entries7 latin3 = replace(latin2, {
    {0x0C, {"\xC2\xBF"}},   // U+00BF inverted question mark
    {0x0F, {}},
    {0x10, {"\xC2\xAC"}},   // U+00AC not
    {0x11, {"\xC2\xAF"}},   // U+00AF macron
    {0x14, {"\xC3\x9F"}},   // U+00DF sharp s
    {0x1E, {"\xC2\xB7"}},   // U+00B7 middle dot
    {0x3B, {";"}},
    {0x3C, {"<"}},
    {0x3D, {"\xC4\xA4"}},   // U+0124 H circumflex
    {0x3E, {">"}},
    {0x40, {"\xC5\x9C"}},   // U+015C S circumflex
    {0x5B, {"["}},
    {0x5D, {"]"}},
    {0x5F, {"\xC4\xB0"}},   // U+0130 I dot
    {0x63, {"\xC4\xA6"}},   // U+0126 H stroke
    {0x64, {"\xC3\x83"}},   // U+00C3 A tilde
    {0x65, {"\xC4\x8A"}},   // U+010A C dot
    {0x66, {"\xC4\x88"}},   // U+0108 C circumflex
    {0x68, {"\xC3\x88"}},   // U+00C8 E grave
    {0x6A, {"\xC3\x8A"}},   // U+00CA E circumflex
    {0x6C, {"\xC3\x8C"}},   // U+00CC I grave
    {0x6F, {"\xC3\x8F"}},   // U+00CF I umlaut
    {0x70, {"\xC4\x9E"}},   // U+011E G breve
    {0x71, {"\xC3\x80"}},   // U+00C0 A grave
    {0x74, {"\xC4\xA0"}},   // U+0120 G dot
    {0x76, {"\xC4\x9C"}},   // U+011C G circumflex
    {0x78, {"\xC3\x9B"}},   // U+00DB U circumflex
    {0x7A, {"\xC3\x99"}},   // U+00D9 U grave
    {0x7B, {"\xC5\xAC"}},   // U+016C U breve
    {0x7C, {"\xC3\x8C"}},   // U+00CC I grave
    {0x7D, {"\xC4\xB4"}},   // U+0134 J circumflex
    {0x7E, {"\xC3\x91"}},   // U+00D1 N tilde
});

// Latin-4 (ISO 8859-4 letters): the Latin-2 table with the letters of the Baltic languages
const Entries7 latin4 = replace(latin2, {
    {0x3B, {"\xC4\xA2"}},   // U+0122 G cedilla
    {0x3C, {"\xC5\xAA"}},   // U+016A U macron
    {0x3D, {"\xC5\xB2"}},   // U+0172 U ogonek
    {0x3E, {"\xC5\xA8"}},   // U+0168 U tilde
    {0x5B, {"\xC5\xA6"}},   // U+0166 T stroke
    {0x5C, {"\xC4\xBB"}},   // U+013B L cedilla
    {0x5D, {"\xC5\x85"}},   // U+0145 N cedilla
    {0x5E, {"\xC4\x90"}},   // U+0110 D stroke
    {0x5F, {"\xC5\x96"}},   // U+0156 R cedilla
    {0x63, {"\xC3\x83"}},   // U+00C3 A tilde
    {0x65, {"\xC3\x85"}},   // U+00C5 A ring
    {0x66, {"\xC3\x86"}},   // U+00C6 AE
    {0x67, {"\xC4\xAE"}},   // U+012E I ogonek
    {0x6C, {"\xC4\x96"}},   // U+0116 E dot
    {0x6F, {"\xC4\xAA"}},   // U+012A I macron
    {0x71, {"\xC4\x80"}},   // U+0100 A macron
    {0x72, {"\xC4\xB6"}},   // U+0136 K cedilla
    {0x74, {"\xC3\x95"}},   // U+00D5 O tilde
    {0x76, {"\xC3\x98"}},   // U+00D8 O stroke
    {0x78, {"\xC3\x9B"}},   // U+00DB U circumflex
    {0x7B, {"\xC4\x92"}},   // U+0112 E macron
    {0x7C, {"\xC3\x95"}},   // U+00D5 O tilde
    {0x7D, {"\xC4\xA8"}},   // U+0128 I tilde
});

} // namespace

const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistPolish = {7, polish.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistIcelandic = {7, icelandic.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistLatin1 = {7, latin1.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistLatin2 = {7, latin2.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistLatin3 = {7, latin3.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistLatin4 = {7, latin4.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistTurkish = {7, turkish.data()};
