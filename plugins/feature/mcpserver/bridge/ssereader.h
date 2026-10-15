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

#ifndef INCLUDE_MCPBRIDGE_SSEREADER_H_
#define INCLUDE_MCPBRIDGE_SSEREADER_H_

#include <string>
#include <vector>

//!< Pulls the data payloads out of a text/event-stream. Bytes arrive in whatever pieces the
//!< socket hands over, so events are only emitted once their terminating blank line is seen.
//!< Comment lines, which the server sends to keep the connection alive, are dropped.
class SseReader
{
public:
    //!< Adds bytes and returns any events they completed
    std::vector<std::string> feed(const std::string& bytes);

private:
    void takeLine(const std::string& line, std::vector<std::string>& events);

    std::string m_buffer;   //!< Bytes not yet forming a whole line
    std::string m_data;     //!< data: lines of the event being assembled
    bool m_haveData = false;
};

#endif // INCLUDE_MCPBRIDGE_SSEREADER_H_
