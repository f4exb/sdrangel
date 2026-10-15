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

#include "journaline.h"

#include <algorithm>
#include <array>

#include "crc.h"
#include "inflate.h"

namespace modemdr
{


// The text of an NML section, up to the next NML code (a byte below 0x10).
// Escape codes 0x10-0x1f within it: 0x10 is a preferred line break, data
// sections (0x1a, 0x1b) and extended codes (0x1c, 0x1d) are skipped with
// their content, and the rest (word breaks, highlighting) are dropped.
static bool sectionText(const std::uint8_t*& position, const std::uint8_t* end, std::string& text)
{
    text.clear();

    while (position < end && *position >= 0x10U)
    {
        const std::uint8_t byte = *position;

        if (byte >= 0x20U)
        {
            text.push_back(static_cast<char>(byte));
            ++position;
            continue;
        }

        ++position;

        if (byte == 0x10U) {
            text.push_back('\n');
        }
        else if (byte == 0x1aU || byte == 0x1bU)
        {
            // A length byte, then length + 1 bytes of data.
            if (position >= end || static_cast<std::size_t>(end - position) < 2U + *position) {
                return false;
            }

            position += 2U + *position;
        }
        else if (byte == 0x1cU || byte == 0x1dU)
        {
            if (position >= end) {
                return false;
            }

            ++position;
        }
    }

    return true;
}

bool JournalineDecoder::parseObject(const std::uint8_t* data, std::size_t size, JournalineObject& object)
{
    // Header: object Id, then type (3 bits), static, compressed and revision.
    if (size < 4U) {
        return false;
    }

    object = JournalineObject();
    object.objectId = (data[0] << 8) | data[1];
    object.type = data[2] >> 5;
    object.staticObject = (data[2] & 0x10U) != 0U;
    const bool compressed = (data[2] & 0x08U) != 0U;
    object.revision = data[2] & 0x07;

    if (object.type < JournalineObject::Menu || object.type > JournalineObject::List) {
        return false;
    }

    std::vector<std::uint8_t> inflated;
    const std::uint8_t* position = data + 3;
    const std::uint8_t* end = data + size;

    if (compressed)
    {
        // A compression method byte (8, DEFLATE), then the raw DEFLATE body.
        if (*position != 0x08U || !inflateRaw(position + 1, static_cast<std::size_t>(end - position - 1), inflated, MaximumObjectBytes)) {
            return false;
        }

        position = inflated.data();
        end = inflated.data() + inflated.size();
    }

    if (position >= end || *position != 0x01U) {
        return false;
    }

    ++position;

    if (!sectionText(position, end, object.title)) {
        return false;
    }

    if (object.type == JournalineObject::TitleOnly) {
        return true;
    }

    while (position < end)
    {
        const std::uint8_t code = *position++;
        JournalineObject::Item item;

        if (object.type == JournalineObject::Menu && code == 0x02U)
        {
            if (end - position < 2) {
                return false;
            }

            item.linkId = (position[0] << 8) | position[1];
            position += 2;
        }
        else if (object.type == JournalineObject::PlainText && code == 0x03U) {
        } else if (object.type == JournalineObject::List && (code == 0x04U || code == 0x05U)) {
            item.newRow = code == 0x04U;
        } else {
            return false;
        }

        if (!sectionText(position, end, item.text)) {
            return false;
        }

        object.items.push_back(std::move(item));
    }

    return object.type != JournalineObject::PlainText || !object.items.empty();
}

void JournalineDecoder::reset()
{
    m_objects.clear();
    m_received.clear();
    m_clock = 0;
    m_invalidDataUnits = 0;
}

bool JournalineDecoder::push(const std::vector<std::uint8_t>& dataUnit)
{
    // MSC data group: header, optional extension and session header, the
    // NML object and, when flagged, a CRC.
    if (dataUnit.size() < 3U)
    {
        ++m_invalidDataUnits;
        return false;
    }

    const bool extension = (dataUnit[0] & 0x80U) != 0U;
    const bool crcPresent = (dataUnit[0] & 0x40U) != 0U;
    const bool segmented = (dataUnit[0] & 0x20U) != 0U;
    const bool userAccess = (dataUnit[0] & 0x10U) != 0U;
    std::size_t end = dataUnit.size();

    if (crcPresent)
    {
        const std::uint16_t crc = static_cast<std::uint16_t>((dataUnit[end - 2U] << 8) | dataUnit[end - 1U]);

        if (end < 4U || CRC::crc16(dataUnit.data(), end - 2U) != crc)
        {
            ++m_invalidDataUnits;
            return false;
        }

        end -= 2U;
    }

    std::size_t position = 2U + (extension ? 2U : 0U) + (segmented ? 2U : 0U);

    if (userAccess)
    {
        if (position >= end)
        {
            ++m_invalidDataUnits;
            return false;
        }

        position += 1U + (dataUnit[position] & 0x0fU);
    }

    JournalineObject object;

    if (position >= end || !parseObject(dataUnit.data() + position, end - position, object))
    {
        ++m_invalidDataUnits;
        return false;
    }

    m_received[object.objectId] = ++m_clock;
    const auto existing = m_objects.find(object.objectId);

    if (existing != m_objects.end() && existing->second.revision == object.revision) {
        return false; // a repetition
    }

    m_objects[object.objectId] = std::move(object);

    // Keep the most recently received objects and the root menu.
    while (m_objects.size() > MaximumObjects)
    {
        auto oldest = m_received.end();

        for (auto entry = m_received.begin(); entry != m_received.end(); ++entry)
        {
            if (entry->first != 0 && (oldest == m_received.end() || entry->second < oldest->second)) {
                oldest = entry;
            }
        }

        m_objects.erase(oldest->first);
        m_received.erase(oldest);
    }

    return true;
}

} // namespace modemdr
