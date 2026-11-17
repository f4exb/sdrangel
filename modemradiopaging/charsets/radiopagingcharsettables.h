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

#ifndef MODEMRADIOPAGING_RADIOPAGINGCHARSETTABLES_H
#define MODEMRADIOPAGING_RADIOPAGINGCHARSETTABLES_H

#include "radiopagingcharset.h"

// The tables of the codepages, one file each; RadioPagingCharset::codepages() names them
namespace RadioPagingCharsetTables {

// POCSAG (7 bits); the ASCII table is the FLEX generic one too
extern const RadioPagingCharset::Table pocsagAscii;
extern const RadioPagingCharset::Table pocsagAsciiUppercase;
extern const RadioPagingCharset::Table pocsagLatinBritish;
extern const RadioPagingCharset::Table pocsagLatinDanishNorwegian;
extern const RadioPagingCharset::Table pocsagLatinFrench;
extern const RadioPagingCharset::Table pocsagLatinGerman;
extern const RadioPagingCharset::Table pocsagLatinItalian;
extern const RadioPagingCharset::Table pocsagLatinPortuguese;
extern const RadioPagingCharset::Table pocsagLatinSpanish;
extern const RadioPagingCharset::Table pocsagLatinSwedish;
extern const RadioPagingCharset::Table pocsagLatinSwiss;
extern const RadioPagingCharset::Table pocsagLatinHebrew;
extern const RadioPagingCharset::Table pocsagMotorolaAdvisorCyrillic;
extern const RadioPagingCharset::Table pocsagMotorolaAdvisorLinguistLatinCyrillic;
extern const RadioPagingCharset::Table pocsagMultitoneAscii;
extern const RadioPagingCharset::Table pocsagNec21aLatinCyrillic;
extern const RadioPagingCharset::Table pocsagNecMaximaCompactCyrillic;
extern const RadioPagingCharset::Table pocsagPhilipsPrg2220LatinCyrillic;
extern const RadioPagingCharset::Table pocsagSwissphoneDe506Cyrillic;

// FLEX (7 bits): Motorola Linguist
extern const RadioPagingCharset::Table flexLinguistArabic;
extern const RadioPagingCharset::Table flexLinguistArabicEnglish;
extern const RadioPagingCharset::Table flexLinguistCyrillic;
extern const RadioPagingCharset::Table flexLinguistGreek;
extern const RadioPagingCharset::Table flexLinguistHebrew;
extern const RadioPagingCharset::Table flexLinguistIcelandic;
extern const RadioPagingCharset::Table flexLinguistLatin1;
extern const RadioPagingCharset::Table flexLinguistLatin2;
extern const RadioPagingCharset::Table flexLinguistLatin3;
extern const RadioPagingCharset::Table flexLinguistLatin4;

// Special formats on top of a table
extern const RadioPagingCharset::Transcoder beepwearProAlarm;    // FLEX: Timex BeepwearPRO alarm command, over ASCII
extern const RadioPagingCharset::Transcoder rot13;               // POCSAG and FLEX: ROT-13 of the letters, over ASCII
extern const RadioPagingCharset::Transcoder kanji;               // FLEX: Shift JIS characters of HEX/binary messages
extern const RadioPagingCharset::Table flexLinguistPolish;
extern const RadioPagingCharset::Table flexLinguistTurkish;
extern const RadioPagingCharset::Table flexLinguistVietnamese;

// GSC (6 bits): the character set of Rep. 900-2 Table VII
extern const RadioPagingCharset::Table gscAscii;

} // namespace RadioPagingCharsetTables

#endif // MODEMRADIOPAGING_RADIOPAGINGCHARSETTABLES_H
