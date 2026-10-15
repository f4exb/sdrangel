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

#include "ssereader.h"

std::vector<std::string> SseReader::feed(const std::string& bytes)
{
    std::vector<std::string> events;
    m_buffer += bytes;

    for (;;)
    {
        size_t end = m_buffer.find('\n');

        if (end == std::string::npos) {
            break;
        }

        std::string line = m_buffer.substr(0, end);
        m_buffer.erase(0, end + 1);

        if (!line.empty() && (line.back() == '\r')) {
            line.pop_back();
        }

        takeLine(line, events);
    }

    return events;
}

void SseReader::takeLine(const std::string& line, std::vector<std::string>& events)
{
    if (line.empty())
    {
        // Blank line ends the event. An event with no data at all dispatches nothing
        if (m_haveData) {
            events.push_back(m_data);
        }

        m_data.clear();
        m_haveData = false;
        return;
    }

    if (line[0] == ':') {
        return; // comment, such as the keepalive
    }

    size_t colon = line.find(':');
    std::string field = (colon == std::string::npos) ? line : line.substr(0, colon);

    if (field != "data") {
        return; // event, id and retry play no part in the MCP transport
    }

    std::string value = (colon == std::string::npos) ? std::string() : line.substr(colon + 1);

    if (!value.empty() && (value[0] == ' ')) {
        value.erase(0, 1);
    }

    if (m_haveData) {
        m_data += '\n';
    }

    m_data += value;
    m_haveData = true;
}
