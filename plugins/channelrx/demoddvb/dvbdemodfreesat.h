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

#ifndef INCLUDE_DVBDEMODFREESAT_H
#define INCLUDE_DVBDEMODFREESAT_H

#include <cstdint>
#include <string>

// The Huffman compression of text used by Freesat and Freeview in the UK: EN 300 468 strings starting 0x1F, then
// encoding_type_id 1 or 2 for the table. Each character's code depends on the previous character
class DVBDemodFreesat
{
public:
    struct Code
    {
        uint32_t m_value;   //!< Left aligned
        uint8_t m_bits;
        uint8_t m_next;     //!< The character it codes: 0 ends the string, 1 escapes to literal bytes
    };

    //! text: the whole string, from the 0x1F. Appends the decoded bytes to out. False if it isn't valid
    static bool decode(const uint8_t *text, int length, std::string& out);

private:
    static constexpr uint8_t m_endOfString = 0;
    static constexpr uint8_t m_escape = 1;
    //! For each previous character, the codes of the next, and where they start (dvbdemodfreesattables.cpp)
    static const Code m_table1[];
    static const uint16_t m_index1[];
    static const Code m_table2[];
    static const uint16_t m_index2[];

    static const Code *table(int number);
    static const uint16_t *index(int number);
    static int indexSize(int number);
};

#endif // INCLUDE_DVBDEMODFREESAT_H
