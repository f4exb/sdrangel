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

#include <cstring>

#include "dvbdemodsections.h"

struct DVBDemodSectionCRC::CRCTable
{
    uint32_t m_table[256];

    CRCTable()
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i << 24;

            for (int b = 0; b < 8; b++) {
                c = (c & 0x80000000) ? (c << 1) ^ 0x04C11DB7 : (c << 1);
            }

            m_table[i] = c;
        }
    }
};

const DVBDemodSectionCRC::CRCTable DVBDemodSectionCRC::m_crcTable; // Built before use from any thread

uint32_t dvbSectionCRC(const uint8_t *data, int length)
{
    uint32_t crc = 0xFFFFFFFF;

    for (int i = 0; i < length; i++) {
        crc = (crc << 8) ^ DVBDemodSectionCRC::m_crcTable.m_table[((crc >> 24) ^ data[i]) & 0xFF];
    }

    return crc;
}

void DVBDemodSectionAssembler::reset()
{
    m_data.clear();
    m_inSection = false;
    m_continuity = -1;
}

// A complete section, if there's enough data: passed on if its CRC is correct. Sections with the long form
// (section_syntax_indicator 1) have a CRC, and of the short form only the TOT (0x73): the TDT (0x70), RST, ST and DIT
// don't (EN 300 468 5.2.5)
void DVBDemodSectionAssembler::complete(const SectionCallback& callback)
{
    const int length = 3 + (((m_data[1] & 0x0F) << 8) | m_data[2]);
    const bool hasCRC = (m_data[1] & 0x80) || (m_data[0] == 0x73);

    if ((length >= 7) && (!hasCRC || (dvbSectionCRC(m_data.data(), length) == 0))) {
        callback(m_data.data(), length);
    }
}

void DVBDemodSectionAssembler::packet(const uint8_t *p, const SectionCallback& callback)
{
    const bool start = (p[1] & 0x40) != 0;
    const int adaptation = (p[3] >> 4) & 3;
    const int continuity = p[3] & 0x0F;

    if (!(adaptation & 1)) { // No payload
        return;
    }

    // A packet sent twice (the same continuity counter and contents) is allowed (ISO/IEC 13818-1 2.4.3.3): the copy's
    // ignored. Another with the same counter, or a counter that skips, is a packet lost
    if ((m_continuity >= 0) && (continuity == m_continuity) && (std::memcmp(p, m_last, sizeof(m_last)) == 0)) {
        return;
    }

    // Lost a packet of the section: with a new one starting in this packet too, the bytes before its pointer aren't
    // the end of the one lost from
    if ((m_continuity >= 0) && (continuity != ((m_continuity + 1) & 0x0F))) {
        m_inSection = false;
    }

    m_continuity = continuity;
    std::memcpy(m_last, p, sizeof(m_last));
    int offset = 4;

    if (adaptation & 2) {
        offset += 1 + p[4];
    }

    if (offset >= 188) {
        return;
    }

    if (start)
    {
        const int pointer = p[offset];
        offset++;

        // The end of the previous section comes before the pointer
        if (m_inSection && (offset + pointer <= 188))
        {
            m_data.insert(m_data.end(), p + offset, p + offset + pointer);

            if ((m_data.size() >= 3) && ((int) m_data.size() >= 3 + (((m_data[1] & 0x0F) << 8) | m_data[2]))) {
                complete(callback);
            }
        }

        offset += pointer;
        m_data.clear();
        m_inSection = offset < 188;
    }

    // Sections may follow each other in a packet, until stuffing (0xFF) or the end of the packet
    while (m_inSection && (offset < 188))
    {
        if (m_data.empty() && (p[offset] == 0xFF))
        {
            m_inSection = false;
            break;
        }

        m_data.insert(m_data.end(), p + offset, p + 188);
        offset = 188;

        if (m_data.size() < 3) {
            break;
        }

        const int length = 3 + (((m_data[1] & 0x0F) << 8) | m_data[2]);

        if ((int) m_data.size() < length) {
            break;
        }

        complete(callback);
        // What follows the section in this packet starts another
        const int used = (int) m_data.size() - length;
        offset = 188 - used;
        m_data.clear();
    }
}
