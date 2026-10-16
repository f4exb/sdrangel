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

#include <algorithm>

#include "dtv/dtvpsi.h"
#include "dtv/dtvsections.h"

DTVPSI::DTVPSI() :
    m_transportStreamId(-1)
{
}

void DTVPSI::reset()
{
    m_partial.clear();
    m_pat.reset();
    m_nit.reset();
    m_transportStreamId = -1;
    m_channels.clear();
}

const std::map<int, int>& DTVPSI::getChannels() const
{
    static const std::map<int, int> none;
    auto it = m_channels.find(m_transportStreamId);
    return it != m_channels.end() ? it->second : none;
}

void DTVPSI::feed(const uint8_t *data, int length)
{
    // Complete a packet split between feeds
    if (!m_partial.empty())
    {
        int needed = std::min(188 - (int) m_partial.size(), length);
        m_partial.insert(m_partial.end(), data, data + needed);
        data += needed;
        length -= needed;

        if (m_partial.size() < 188) {
            return;
        }

        if (m_partial[0] == 0x47) {
            packet(m_partial.data());
        }

        m_partial.clear();
    }

    while (length > 0)
    {
        if (data[0] != 0x47) // Lost alignment: find the next sync byte
        {
            data++;
            length--;
            continue;
        }

        if (length < 188)
        {
            m_partial.assign(data, data + length);
            return;
        }

        packet(data);
        data += 188;
        length -= 188;
    }
}

void DTVPSI::packet(const uint8_t *p)
{
    if (p[1] & 0x80) { // Uncorrectable errors
        return;
    }

    const int pid = ((p[1] & 0x1F) << 8) | p[2];

    auto callback = [this](const uint8_t *s, int length) { section(s, length); };

    if (pid == m_patPid) {
        m_pat.packet(p, callback);
    } else if (pid == m_nitPid) {
        m_nit.packet(p, callback);
    }
}

void DTVPSI::section(const uint8_t *s, int length)
{
    // The long form, with a CRC; current, not a table to apply next
    if ((length < 12) || !(s[1] & 0x80) || !(s[5] & 0x01)) {
        return;
    }

    if (s[0] == m_patTable) {
        pat(s, length);
    } else if (s[0] == m_nitActual) {
        nit(s, length);
    }
}

// program_association_section (ISO/IEC 13818-1 2.4.4.3): the transport stream id
void DTVPSI::pat(const uint8_t *s, int length)
{
    (void) length;
    m_transportStreamId = (s[3] << 8) | s[4];
}

// network_information_section (EN 300 468 5.2.1), with logical_channel_descriptors in the transport stream loop
void DTVPSI::nit(const uint8_t *s, int length)
{
    if (length < 16) {
        return;
    }

    const int end = length - 4; // Before the CRC
    int i = 8;
    const int networkDescriptorsLength = ((s[i] & 0x0F) << 8) | s[i + 1];
    i += 2 + networkDescriptorsLength;

    if (i + 2 > end) {
        return;
    }

    const int loopEnd = std::min(end, i + 2 + (((s[i] & 0x0F) << 8) | s[i + 1]));
    i += 2;

    // Each transport stream's services' numbers apart: a service id can be another service's in another transport
    // stream of the network. The NIT can come before the PAT says which is this one
    while (i + 6 <= loopEnd)
    {
        const int transportStreamId = (s[i] << 8) | s[i + 1];
        std::map<int, int>& channels = m_channels[transportStreamId];
        const int descriptorsEnd = std::min(loopEnd, i + 6 + (((s[i + 4] & 0x0F) << 8) | s[i + 5]));
        i += 6;

        while (i + 2 <= descriptorsEnd)
        {
            const int tag = s[i];
            const int descriptorEnd = std::min(descriptorsEnd, i + 2 + s[i + 1]);

            if (tag == m_logicalChannelDescriptor)
            {
                for (int j = i + 2; j + 4 <= descriptorEnd; j += 4)
                {
                    const int serviceId = (s[j] << 8) | s[j + 1];
                    const int lcn = ((s[j + 2] & 0x03) << 8) | s[j + 3];
                    channels[serviceId] = lcn;
                }
            }

            i = descriptorEnd;
        }

        i = descriptorsEnd;
    }
}
