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

#ifndef MODEMRADIOPAGING_RADIOPAGINGCHARSETBUILD_H
#define MODEMRADIOPAGING_RADIOPAGINGCHARSETBUILD_H

#include <array>
#include <initializer_list>

#include "radiopagingcharsettables.h"

namespace RadioPagingCharsetTables {

typedef std::array<RadioPagingCharset::Entry, 128> Entries7;

// A code and its glyph and aliases (none: the code has no glyph)
struct Replaced
{
    int m_code;
    std::initializer_list<const char*> m_glyphs;
};

// The entries of some codes of a table replaced
inline Entries7 replace(Entries7 entries, std::initializer_list<Replaced> replaced)
{
    for (const Replaced& r : replaced)
    {
        int i = 0;

        for (const char *glyph : r.m_glyphs)
        {
            if (i < RadioPagingCharset::m_maxGlyphs - 1) {
                entries[r.m_code][i++] = glyph;
            }
        }
        for (; i < RadioPagingCharset::m_maxGlyphs; i++) {
            entries[r.m_code][i] = nullptr;
        }
    }

    return entries;
}

// The ASCII table with the entries of some codes replaced
inline Entries7 fromAscii(std::initializer_list<Replaced> replaced)
{
    Entries7 entries;

    for (int code = 0; code < 128; code++)
    {
        for (int i = 0; i < RadioPagingCharset::m_maxGlyphs; i++) {
            entries[code][i] = pocsagAscii.m_entries[code][i];
        }
    }

    return replace(entries, replaced);
}

} // namespace RadioPagingCharsetTables

#endif // MODEMRADIOPAGING_RADIOPAGINGCHARSETBUILD_H
