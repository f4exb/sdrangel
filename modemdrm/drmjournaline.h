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

#ifndef MODEMDRM_DRMJOURNALINE_H_
#define MODEMDRM_DRMJOURNALINE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "export.h"
#include "modemdr/inflate.h"
#include "modemdr/journaline.h"
#include "drmdata.h"
#include "drmmsc.h"
#include "drmsdc.h"

namespace modemdrm
{

// Journaline is shared with DAB
using modemdr::JournalineObject;
using modemdr::JournalineDecoder;
using modemdr::inflateRaw;

// The Journaline services of a multiplex: every data component with user
// application 0x44a (DAB domain, packet mode with data units), decoded from
// the multiplex frames whatever service is selected. Services that signal
// the same stream and packet Id share one component.
class MODEMDRM_API DRMJournalineDecoder
{
public:
    struct Component
    {
        int streamId = -1;
        int packetId = -1;
        int packetLengthBytes = 0;
        std::set<int> shortIds;  // the services it belongs to
        DRMPacketDemultiplexer packets;
        JournalineDecoder decoder;
        // Changes with its objects or services; never repeats, even for a
        // component that replaces another
        std::uint64_t revision = 0;
    };

    void reset();
    // At a confirmed reconfiguration: the services of each component are
    // taken again from the SDC that follows (objects are kept for a component
    // signalled again). Returns true when a service lost its component.
    bool reconfigure();
    // A multiplex frame was lost: partial data units go, objects stay.
    void frameLost();
    // One decoded multiplex frame. Returns true when an object changed.
    bool push(const SDCData& sdc, const std::vector<MSCLogicalStreamFrame>& streams);
    // Components by stream and packet Id.
    const std::map<std::pair<int, int>, Component>& components() const { return m_components; }

private:
    std::map<std::pair<int, int>, Component> m_components;
    std::uint64_t m_revision = 0;
};

} // namespace modemdrm

#endif // MODEMDRM_DRMJOURNALINE_H_
