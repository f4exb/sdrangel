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
#include "drmdata.h"
#include "drmmsc.h"
#include "drmsdc.h"

namespace modemdrm
{

// One Journaline (ETSI TS 102 979) news object: a menu linking to other
// objects, a page of text, a title or a list.
struct MODEMDRM_API JournalineObject
{
    enum Type
    {
        Menu = 1,
        PlainText = 2,
        TitleOnly = 3,
        List = 4
    };

    struct Item
    {
        std::string text;     // UTF-8, line breaks as '\n'
        int linkId = -1;      // menu items: the object linked to
        bool newRow = true;   // list items: a new row rather than another column
    };

    int objectId = -1;        // 0 is the root menu
    int type = PlainText;
    bool staticObject = false;
    int revision = 0;
    std::string title;
    std::vector<Item> items;  // menu entries, the text (one item) or list cells
};

// Inflates raw DEFLATE data (RFC 1951) into output, up to maximumOutput bytes.
// Returns false for invalid or oversized data.
MODEMDRM_API bool inflateRaw(const std::uint8_t* data, std::size_t size,
    std::vector<std::uint8_t>& output, std::size_t maximumOutput);

// The objects of one Journaline service, from its data units: each an MSC data
// group (EN 300 401 clause 5.3.3) carrying one NML object, which may be
// compressed. The latest revision of each object is kept.
class MODEMDRM_API JournalineDecoder
{
public:
    void reset();
    // One data unit; true when it added or changed an object.
    bool push(const std::vector<std::uint8_t>& dataUnit);
    // Parses one NML object, without its transport.
    static bool parseObject(const std::uint8_t* data, std::size_t size, JournalineObject& object);

    const std::map<int, JournalineObject>& objects() const { return m_objects; }
    int invalidDataUnits() const { return m_invalidDataUnits; }

    static constexpr std::size_t MaximumObjects = 1024U; // the root menu is always kept
    static constexpr std::size_t MaximumObjectBytes = 4096U; // NML objects are at most 4 kB

private:

    std::map<int, JournalineObject> m_objects;
    std::map<int, std::uint64_t> m_received; // reception order, for eviction
    std::uint64_t m_clock = 0;
    int m_invalidDataUnits = 0;
};

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
