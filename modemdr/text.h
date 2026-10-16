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

#ifndef MODEMDR_TEXT_H_
#define MODEMDR_TEXT_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "export.h"

namespace modemdr
{

// Text in the character sets of DAB and DRM data (ETSI TS 101 756 tables 1 and 19): 0 the
// complete EBU Latin based repertoire, 4 ISO 8859-1, 6 UCS-2 and 15 UTF-8, as UTF-8.
class MODEMDR_API Text
{
public:
    // A label or text. Line break and end of headline codes give '\n'; other control codes and
    // invalid UTF-8 are dropped. Trailing zeros (padding) are removed.
    static std::string toUTF8(const std::uint8_t *data, std::size_t length, int charset);
    // An MOT ContentName (EN 301 234 clause 6.2.2.1.1): as toUTF8, except that bytes below 0x80
    // of the EBU Latin set are ASCII. Names are paths and URLs, which links must match.
    static std::string contentNameToUTF8(const std::uint8_t *data, std::size_t length, int charset);
};

} // namespace modemdr

#endif // MODEMDR_TEXT_H_
