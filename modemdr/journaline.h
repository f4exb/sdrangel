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

#ifndef MODEMDR_JOURNALINE_H_
#define MODEMDR_JOURNALINE_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "export.h"

namespace modemdr
{

// One Journaline (ETSI TS 102 979) news object: a menu linking to other
// objects, a page of text, a title or a list.
struct MODEMDR_API JournalineObject
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

// The objects of one Journaline service, from its data units: each an MSC data
// group (EN 300 401 clause 5.3.3) carrying one NML object, which may be
// compressed. The latest revision of each object is kept.
class MODEMDR_API JournalineDecoder
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

} // namespace modemdr

#endif // MODEMDR_JOURNALINE_H_
