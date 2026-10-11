///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DVBDEMODTEXT_H
#define INCLUDE_DVBDEMODTEXT_H

#include <cstdint>

#include <QString>

// Text in DVB service information (EN 300 468 annex A): the first byte may select the character table, ISO/IEC 6937
// (with non-spacing diacritics before the letter) being the default. 0x1F is Freesat / Freeview Huffman compression.
// Control codes: 0x8A is a line break; emphasis on and off (0x86, 0x87) are dropped
namespace DVBDemodText
{
    QString decode(const uint8_t *text, int length);
}

//! DVBDemodText::decode()'s character tables and decoders
class DVBDemodTextCharsets
{
private:
    static const char16_t m_iso6937High[96];
    static const char16_t m_iso8859High[16][96];
    static const char16_t m_iso6937Diacritics[15];

    static bool control(uint32_t c, QString& out);
    static QString iso6937(const uint8_t *s, int length);
    static QString iso8859(const uint8_t *s, int length, int part);
    static QString removeControls(const QString& text);

    friend QString DVBDemodText::decode(const uint8_t *text, int length);
};

#endif // INCLUDE_DVBDEMODTEXT_H
