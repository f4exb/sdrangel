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

// Motorola Linguist FLEX codepages of the other scripts: ASCII with the letters of the script on some codes.
// Glyphs other than ASCII are UTF-8 escapes with their code point, or Cyr letters

#include "radiopagingcharsetbuild.h"
#include "radiopagingcyrillic.h"

using namespace RadioPagingCharsetTables;

namespace {

// Cyrillic: ASCII to 0x3F, the Cyrillic lowercase letters on 0x40-0x5E and the uppercase ones on 0x60-0x7E in
// the phonetic order (KOI-7)
const Entries7 cyrillic = fromAscii({
    {0x40, {Cyr::yu}}, {0x41, {Cyr::a}}, {0x42, {Cyr::b}}, {0x43, {Cyr::ts}},
    {0x44, {Cyr::d}}, {0x45, {Cyr::e}}, {0x46, {Cyr::f}}, {0x47, {Cyr::g}},
    {0x48, {Cyr::kh}}, {0x49, {Cyr::i}}, {0x4A, {Cyr::j}}, {0x4B, {Cyr::k}},
    {0x4C, {Cyr::l}}, {0x4D, {Cyr::m}}, {0x4E, {Cyr::n}}, {0x4F, {Cyr::o}},
    {0x50, {Cyr::p}}, {0x51, {Cyr::ya}}, {0x52, {Cyr::r}}, {0x53, {Cyr::s}},
    {0x54, {Cyr::t}}, {0x55, {Cyr::u}}, {0x56, {Cyr::zh}}, {0x57, {Cyr::v}},
    {0x58, {Cyr::soft}}, {0x59, {Cyr::y}}, {0x5A, {Cyr::z}}, {0x5B, {Cyr::sh}},
    {0x5C, {Cyr::eh}}, {0x5D, {Cyr::shch}}, {0x5E, {Cyr::ch}},
    {0x60, {Cyr::YU}}, {0x61, {Cyr::A}}, {0x62, {Cyr::B}}, {0x63, {Cyr::TS}},
    {0x64, {Cyr::D}}, {0x65, {Cyr::E}}, {0x66, {Cyr::F}}, {0x67, {Cyr::G}},
    {0x68, {Cyr::KH}}, {0x69, {Cyr::I}}, {0x6A, {Cyr::J}}, {0x6B, {Cyr::K}},
    {0x6C, {Cyr::L}}, {0x6D, {Cyr::M}}, {0x6E, {Cyr::N}}, {0x6F, {Cyr::O}},
    {0x70, {Cyr::P}}, {0x71, {Cyr::YA}}, {0x72, {Cyr::R}}, {0x73, {Cyr::S}},
    {0x74, {Cyr::T}}, {0x75, {Cyr::U}}, {0x76, {Cyr::ZH}}, {0x77, {Cyr::V}},
    {0x78, {Cyr::SOFT}}, {0x79, {Cyr::Y}}, {0x7A, {Cyr::Z}}, {0x7B, {Cyr::SH}},
    {0x7C, {Cyr::EH}}, {0x7D, {Cyr::SHCH}}, {0x7E, {Cyr::CH}},
});

// Greek: Latin-1 style symbols on the control codes, the Greek capitals that do not look like Latin ones, the
// accented capitals and the Greek punctuation on 0x61-0x7A; A-Z stand for the Greek lookalikes
const Entries7 greek = fromAscii({
    {0x01, {"\xC2\xA4"}}, {0x02, {"\xC2\xB5"}}, {0x05, {"\xC2\xA2"}}, {0x06, {"\xC3\x97"}},
    {0x07, {"\xC3\xB7"}}, {0x08, {"\xC2\xA3"}}, {0x09, {"\xC2\xB0"}}, {0x0B, {"\xC2\xAB"}},
    {0x0C, {"\xC2\xBF"}}, {0x0E, {"\xC2\xBB"}}, {0x0F, {"\xC2\xA1"}}, {0x10, {"\xC2\xAC"}},
    {0x11, {}}, {0x12, {"\xC2\xBD"}}, {0x13, {"\xC2\xA6"}}, {0x14, {"\xC2\xA7"}},
    {0x15, {"\xC2\xB2"}}, {0x16, {}}, {0x18, {"\xC2\xB3"}}, {0x19, {"\xC2\xA9"}},
    {0x1A, {}}, {0x1C, {"\xC2\xA5"}}, {0x1D, {"\xC2\xB1"}}, {0x1E, {"\xC2\xB7"}},
    {0x5E, {"^"}}, {0x5F, {"_"}},
    {0x61, {"\xCE\x93"}},   // U+0393 Gamma
    {0x62, {"\xCE\x94"}},   // U+0394 Delta
    {0x63, {"\xCE\x86"}},   // U+0386 Alpha tonos
    {0x64, {"\xCE\x88"}},   // U+0388 Epsilon tonos
    {0x65, {"\xCE\x89"}},   // U+0389 Eta tonos
    {0x66, {"\xCE\x8A"}},   // U+038A Iota tonos
    {0x67, {"\xCE\x98"}},   // U+0398 Theta
    {0x68, {"\xCE\xA8"}},   // U+03A8 Psi
    {0x69, {"\xCE\xA9"}},   // U+03A9 Omega
    {0x6A, {"\xCE\xAA"}},   // U+03AA Iota dialytika
    {0x6B, {"\xCE\x9B"}},   // U+039B Lambda
    {0x6C, {"\xCE\x8C"}},   // U+038C Omicron tonos
    {0x6D, {"\xCE\x8F"}},   // U+038F Omega tonos
    {0x6E, {"\xCA\xBD"}},   // U+02BD modifier letter reversed comma
    {0x6F, {"\xCA\xBC"}},   // U+02BC modifier letter apostrophe
    {0x70, {"\xCE\xA0"}},   // U+03A0 Pi
    {0x71, {"\xCE\xA3"}},   // U+03A3 Sigma
    {0x72, {"\xCE\xA5"}},   // U+03A5 Upsilon
    {0x73, {"\xCE\xA6"}},   // U+03A6 Phi
    {0x74, {"\xCE\xAB"}},   // U+03AB Upsilon dialytika
    {0x75, {"\xCE\x8E"}},   // U+038E Upsilon tonos
    {0x76, {"\xCE\x9E"}},   // U+039E Xi
    {0x77, {"\xC2\xB7"}},   // U+00B7 middle dot (ano teleia)
    {0x78, {"'"}},
    {0x79, {"\xE2\x80\x94"}},   // U+2014 em dash
    {0x7A, {"\xE2\x80\xA5"}},   // U+2025 two dot leader
    {0x7C, {"|"}}, {0x7E, {"~"}},
});

// Hebrew: Latin-1 style symbols on the control codes, the Hebrew letters alef to tav on 0x60-0x7A
const Entries7 hebrew = fromAscii({
    {0x01, {"\xC2\xA4"}}, {0x02, {"\xC2\xB5"}}, {0x05, {"\xC2\xA2"}}, {0x06, {"\xC3\x97"}},
    {0x07, {"\xC3\xB7"}}, {0x08, {"\xC2\xA3"}}, {0x09, {"\xC2\xB0"}}, {0x0B, {"\xC2\xAB"}},
    {0x0C, {"\xC2\xBF"}}, {0x0E, {"\xC2\xBB"}}, {0x0F, {"\xC2\xA1"}}, {0x10, {"\xC2\xAC"}},
    {0x11, {"\xC2\xAD"}}, {0x12, {"\xC2\xBD"}}, {0x13, {"\xC2\xA6"}}, {0x14, {"\xC2\xA7"}},
    {0x15, {"\xC2\xB2"}}, {0x16, {"`"}}, {0x18, {"\xC2\xB3"}}, {0x19, {"\xC2\xA9"}},
    {0x1A, {"\xC2\xB7"}}, {0x1C, {"\xC2\xA5"}}, {0x1D, {"\xC2\xB1"}}, {0x1E, {"."}},
    {0x5E, {"^"}}, {0x5F, {"_"}},
    {0x60, {"\xD7\x90"}}, {0x61, {"\xD7\x91"}}, {0x62, {"\xD7\x92"}}, {0x63, {"\xD7\x93"}},   // U+05D0-U+05D3
    {0x64, {"\xD7\x94"}}, {0x65, {"\xD7\x95"}}, {0x66, {"\xD7\x96"}}, {0x67, {"\xD7\x97"}},   // U+05D4-U+05D7
    {0x68, {"\xD7\x98"}}, {0x69, {"\xD7\x99"}}, {0x6A, {"\xD7\x9A"}}, {0x6B, {"\xD7\x9B"}},   // U+05D8-U+05DB
    {0x6C, {"\xD7\x9C"}}, {0x6D, {"\xD7\x9D"}}, {0x6E, {"\xD7\x9E"}}, {0x6F, {"\xD7\x9F"}},   // U+05DC-U+05DF
    {0x70, {"\xD7\xA0"}}, {0x71, {"\xD7\xA1"}}, {0x72, {"\xD7\xA2"}}, {0x73, {"\xD7\xA3"}},   // U+05E0-U+05E3
    {0x74, {"\xD7\xA4"}}, {0x75, {"\xD7\xA5"}}, {0x76, {"\xD7\xA6"}}, {0x77, {"\xD7\xA7"}},   // U+05E4-U+05E7
    {0x78, {"\xD7\xA8"}}, {0x79, {"\xD7\xA9"}}, {0x7A, {"\xD7\xAA"}},                           // U+05E8-U+05EA
    {0x7C, {"|"}}, {0x7E, {"~"}},
});

// Arabic: A P M on 0x01-0x03, the Arabic letters on the control codes, 0x26, 0x27 and 0x40-0x7D (the positional
// forms of the pager shown as the base letters, lam-alef as two letters), the Arabic comma and question mark;
// no LF, CR or 0x7E
const Entries7 arabic = fromAscii({
    {0x01, {"A"}}, {0x02, {"P"}}, {0x03, {"M"}},
    {0x05, {"\xD8\xAD"}}, {0x07, {"\xD8\xB9"}}, {0x09, {"\xD8\xAC"}}, {0x0A, {}},
    {0x0B, {"\xDA\x86"}}, {0x0C, {"\xD8\xAC"}}, {0x0D, {}}, {0x10, {"\xD8\xAD"}},
    {0x12, {"\xD8\xAE"}}, {0x14, {"\xD8\xBA"}}, {0x16, {"\xD8\xB9"}}, {0x18, {"\xD8\xAD"}},
    {0x19, {"\xDA\x86"}}, {0x1A, {"\xD8\xAC"}}, {0x1C, {"\xDA\x98"}}, {0x1D, {"\xD8\xB9"}},
    {0x26, {"\xD9\xA3"}},   // U+0663 Arabic-Indic digit three
    {0x27, {"\xD8\xB3"}},
    {0x2C, {"\xD8\x8C"}},   // U+060C Arabic comma
    {0x2D, {"."}},
    {0x3F, {"\xD8\x9F"}},   // U+061F Arabic question mark
    {0x40, {"\xD8\xA9"}}, {0x41, {"\xD9\x80"}}, {0x42, {"\xD8\xA2"}}, {0x43, {"\xD8\xA2"}},
    {0x44, {"\xD8\xAC"}}, {0x45, {"\xD8\xA7"}}, {0x46, {"\xD8\xAA"}}, {0x47, {"\xD8\xA7"}},
    {0x48, {"\xD8\xA8"}}, {0x49, {"\xD8\xA9"}}, {0x4A, {"\xD8\xAA"}}, {0x4B, {"\xD8\xAB"}},
    {0x4C, {"\xD8\xAC"}}, {0x4D, {"\xD8\xAD"}}, {0x4E, {"\xD8\xAE"}}, {0x4F, {"\xD8\xAF"}},
    {0x50, {"\xD8\xB0"}}, {0x51, {"\xD8\xB1"}}, {0x52, {"\xD8\xB2"}}, {0x53, {"\xD8\xB3"}},
    {0x54, {"\xD8\xB4"}}, {0x55, {"\xD8\xB5"}}, {0x56, {"\xD8\xB6"}}, {0x57, {"\xD8\xB7"}},
    {0x58, {"\xD8\xB8"}}, {0x59, {"\xD8\xB9"}}, {0x5A, {"\xD8\xBA"}},
    {0x5B, {"\xD9\x84\xD8\xA7"}},   // U+0644 U+0627 lam-alef
    {0x5C, {"\xD9\x83"}}, {0x5D, {"\xD9\x83"}}, {0x5E, {"\xD9\x87"}}, {0x5F, {"\xD8\xB9"}},
    {0x60, {"\xD9\x80"}}, {0x61, {"\xD9\x81"}}, {0x62, {"\xD9\x82"}}, {0x63, {"\xD9\x83"}},
    {0x64, {"\xD9\x84"}}, {0x65, {"\xD9\x85"}}, {0x66, {"\xD9\x86"}}, {0x67, {"\xD9\x87"}},
    {0x68, {"\xD9\x88"}}, {0x69, {"\xD9\x89"}}, {0x6A, {"\xD9\x8A"}}, {0x6B, {"\xD9\x84"}},
    {0x6C, {"\xD8\xA8"}}, {0x6D, {"\xD9\x86"}}, {0x6E, {"\xD8\xAB"}}, {0x6F, {"\xD9\x87"}},
    {0x70, {"\xD9\x86"}}, {0x71, {"\xD9\x88"}}, {0x72, {"\xD8\xB2"}}, {0x73, {"\xD8\xB9"}},
    {0x74, {"\xD8\xBA"}}, {0x75, {"\xD8\xA2"}}, {0x76, {"\xD9\x87"}}, {0x77, {"\xD9\x83"}},
    {0x78, {"\xD9\x84"}}, {0x79, {"\xD9\x85"}}, {0x7A, {"\xD9\x86"}}, {0x7B, {"\xD8\xA2"}},
    {0x7C, {"\xD8\xA8"}}, {0x7D, {"\xD9\x8A"}}, {0x7E, {}},
});

// Vietnamese: ASCII letters and punctuation on the control codes, the Vietnamese capitals on 0x21-0x2F, 0x3B-0x40
// and 0x46-0x7E between the ASCII capitals that need no mark
const Entries7 vietnamese = fromAscii({
    {0x01, {"O"}}, {0x02, {"-"}}, {0x03, {"+"}}, {0x05, {"I"}}, {0x06, {"="}}, {0x07, {"S"}},
    {0x08, {"$"}}, {0x09, {"B"}}, {0x0B, {"G"}}, {0x0C, {"."}}, {0x10, {"F"}}, {0x11, {"%"}},
    {0x12, {"J"}}, {0x13, {"\xE2\x80\x9C"}}, {0x14, {"W"}}, {0x15, {"!"}}, {0x16, {"Z"}}, {0x17, {"*"}},
    {0x18, {"Q"}}, {0x19, {")"}}, {0x1A, {"("}}, {0x1C, {"?"}}, {0x1D, {","}}, {0x1E, {"#"}},
    {0x1F, {"/"}},
    {0x21, {"\xC3\x82"}}, {0x22, {"\xC4\x82"}}, {0x23, {"\xE1\xBA\xA2"}}, {0x24, {"\xC3\x83"}},
    {0x25, {"\xC3\x80"}}, {0x26, {"\xC3\x81"}}, {0x27, {"\xE1\xBA\xA0"}}, {0x28, {"\xE1\xBA\xA8"}},
    {0x29, {"\xE1\xBA\xAA"}}, {0x2A, {"\xE1\xBA\xA4"}}, {0x2B, {"\xE1\xBA\xA6"}}, {0x2C, {"\xE1\xBA\xAC"}},
    {0x2D, {"\xE1\xBA\xB2"}}, {0x2E, {"\xE1\xBA\xB4"}}, {0x2F, {"\xE1\xBA\xAE"}},
    {0x3B, {"\xE1\xBA\xB6"}}, {0x3C, {"\xC3\x8A"}}, {0x3D, {"\xE1\xBA\xBA"}}, {0x3E, {"\xE1\xBA\xBC"}},
    {0x3F, {"\xC3\x89"}}, {0x40, {"\xC3\x88"}},
    {0x46, {"\xC4\x90"}}, {0x47, {"\xE1\xBB\xB6"}}, {0x49, {"\xE1\xBA\xB8"}}, {0x4A, {"\xE1\xBB\x82"}},
    {0x4F, {"\xE1\xBB\x84"}}, {0x51, {"\xE1\xBA\xB0"}}, {0x53, {"\xC3\x9D"}}, {0x57, {"\xE1\xBB\xB8"}},
    {0x5A, {"\xE1\xBA\xBE"}}, {0x5B, {"\xE1\xBB\x80"}}, {0x5C, {"\xE1\xBB\x86"}}, {0x5D, {"\xC3\x94"}},
    {0x5E, {"\xE1\xBB\x8E"}}, {0x5F, {"\xC3\x95"}}, {0x60, {"\xC3\x93"}}, {0x61, {"\xC3\x92"}},
    {0x62, {"\xE1\xBB\x8C"}}, {0x63, {"\xE1\xBB\x94"}}, {0x64, {"\xE1\xBB\x96"}}, {0x65, {"\xE1\xBB\x90"}},
    {0x66, {"\xE1\xBB\x92"}}, {0x67, {"\xE1\xBB\x98"}}, {0x68, {"\xC6\xA1"}}, {0x69, {"\xE1\xBB\x9E"}},
    {0x6A, {"\xC3\x95"}}, {0x6B, {"\xE1\xBB\x9A"}}, {0x6C, {"\xE1\xBB\x9C"}}, {0x6D, {"\xE1\xBB\xA2"}},
    {0x6E, {"\xC6\xAF"}}, {0x6F, {"\xE1\xBB\xA6"}}, {0x70, {"\xC5\xA8"}}, {0x71, {"\xC3\x9A"}},
    {0x72, {"\xC3\x99"}}, {0x73, {"\xE1\xBB\xA4"}}, {0x74, {"\xE1\xBB\xA6"}}, {0x75, {"\xC5\xA8"}},
    {0x76, {"\xC3\x9A"}}, {0x77, {"\xC3\x99"}}, {0x78, {"\xE1\xBB\xA4"}}, {0x79, {"\xE1\xBB\x88"}},
    {0x7A, {"\xC4\xA8"}}, {0x7B, {"\xC3\x8C"}}, {0x7C, {"\xC3\x8D"}}, {0x7D, {"\xE1\xBB\x8A"}},
    {0x7E, {"\xE1\xBB\xB4"}},
});

// Arabic/English: the Arabic table with the Latin letters B-Z on control and punctuation codes, no space
const Entries7 arabicEnglish = replace(arabic, {
    {0x05, {"B"}}, {0x07, {"C"}}, {0x09, {"D"}}, {0x0B, {"E"}}, {0x0C, {"F"}}, {0x10, {"G"}},
    {0x12, {"H"}}, {0x14, {"I"}}, {0x16, {"J"}}, {0x18, {"K"}}, {0x19, {"L"}}, {0x1A, {"M"}},
    {0x1C, {"N"}}, {0x1D, {"O"}}, {0x20, {}}, {0x21, {"P"}}, {0x22, {"Q"}}, {0x23, {"R"}},
    {0x25, {"S"}}, {0x28, {"T"}}, {0x29, {"U"}}, {0x2A, {"V"}}, {0x3B, {"W"}}, {0x3C, {"X"}},
    {0x3D, {"Y"}}, {0x3E, {"Z"}},
});

} // namespace

const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistCyrillic = {7, cyrillic.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistGreek = {7, greek.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistHebrew = {7, hebrew.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistArabic = {7, arabic.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistArabicEnglish = {7, arabicEnglish.data()};
const RadioPagingCharset::Table RadioPagingCharsetTables::flexLinguistVietnamese = {7, vietnamese.data()};
