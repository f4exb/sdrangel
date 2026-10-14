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

#ifndef INCLUDE_DTVSECTIONS_H
#define INCLUDE_DTVSECTIONS_H

#include <cstdint>
#include <functional>
#include <vector>

#include "export.h"

//! CRC-32/MPEG-2 of PSI/SI sections: 0 over a section including its CRC, if it's correct
SDRBASE_API uint32_t dtvSectionCRC(const uint8_t *data, int length);

//! dtvSectionCRC()'s table
class DTVSectionCRC
{
private:
    struct CRCTable;

    static const CRCTable m_crcTable;

    friend SDRBASE_API uint32_t dtvSectionCRC(const uint8_t *data, int length);
};

//! Assembles the sections (ISO/IEC 13818-1 2.4.4) carried by one PID's transport stream packets
class SDRBASE_API DTVSectionAssembler
{
public:
    //! Called with each complete section whose CRC is correct, or that has none: sections with the short form
    //! (section_syntax_indicator 0) other than the TOT, as the TDT. Parsers of long form tables should check the indicator
    typedef std::function<void(const uint8_t *section, int length)> SectionCallback;

    void reset();
    //! A 188-byte packet of the PID, without uncorrectable errors
    void packet(const uint8_t *p, const SectionCallback& callback);

private:
    void complete(const SectionCallback& callback);

    std::vector<uint8_t> m_data;    //!< Section being assembled
    bool m_inSection = false;
    int m_continuity = -1;
    uint8_t m_last[188];            //!< The last packet, to recognise a duplicate of it
};

#endif // INCLUDE_DTVSECTIONS_H
