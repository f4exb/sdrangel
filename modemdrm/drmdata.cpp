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

#include "drmdata.h"

#include <algorithm>
#include <limits>

#include "drmbitutils.h"
#include "drmjournaline.h"

namespace modemdrm
{

namespace
{

// One segment of an MOT entity in an MSC data group (EN 300 401 clause
// 5.3.3): its data group type, segment number and transport Id.
struct MOTSegment
{
    int type = -1;
    bool last = false;
    int number = 0;
    int transportId = -1;
    std::size_t position = 0; // of the segment data in the data group
    std::size_t size = 0;
};

enum class SegmentResult
{
    Segment,  // a segment of an MOT entity
    Other,    // another type, or a data group without segment and transport Id fields
    Invalid
};

// types: a bit (1 << type) for each data group type wanted
SegmentResult parseSegment(const std::vector<std::uint8_t>& dataGroup, unsigned types, MOTSegment& segment)
{
    // MSC data group header (EN 300 401 clause 5.3.3.1).
    if (dataGroup.size() < 2U) {
        return SegmentResult::Invalid;
    }

    const bool extension = (dataGroup[0] & 0x80U) != 0U;
    const bool crcPresent = (dataGroup[0] & 0x40U) != 0U;
    const bool segmented = (dataGroup[0] & 0x20U) != 0U;
    const bool userAccess = (dataGroup[0] & 0x10U) != 0U;
    segment.type = dataGroup[0] & 0x0f;
    std::size_t end = dataGroup.size();

    if (crcPresent)
    {
        if (end < 4U) {
            return SegmentResult::Invalid;
        }

        const std::uint16_t crc = static_cast<std::uint16_t>((dataGroup[end - 2U] << 8) | dataGroup[end - 1U]);

        if (CRC::crc16Bytes(dataGroup.data(), end - 2U) != crc) {
            return SegmentResult::Invalid;
        }

        end -= 2U;
    }

    if (!(types & (1U << segment.type)) || !segmented || !userAccess) {
        return SegmentResult::Other;
    }

    std::size_t position = 2U + (extension ? 2U : 0U);

    // Session header: segment field, then the user access field.
    if (position + 3U > end) {
        return SegmentResult::Invalid;
    }

    segment.last = (dataGroup[position] & 0x80U) != 0U;
    segment.number = ((dataGroup[position] & 0x7f) << 8) | dataGroup[position + 1U];
    position += 2U;
    const bool transportIdPresent = (dataGroup[position] & 0x10U) != 0U;
    const std::size_t accessLength = dataGroup[position] & 0x0fU;
    ++position;

    if (!transportIdPresent || (accessLength < 2U) || (position + accessLength > end)) {
        return SegmentResult::Invalid;
    }

    segment.transportId = (dataGroup[position] << 8) | dataGroup[position + 1U];
    position += accessLength;

    // MOT segmentation header (EN 301 234 clause 5.1.1): repetition count
    // and segment size.
    if (position + 2U > end) {
        return SegmentResult::Invalid;
    }

    segment.size = static_cast<std::size_t>(((dataGroup[position] & 0x1f) << 8) | dataGroup[position + 1U]);
    position += 2U;

    if (position + segment.size > end) {
        return SegmentResult::Invalid;
    }

    segment.position = position;
    return SegmentResult::Segment;
}

// MOT header information (EN 301 234 clause 6): the header core and the
// header extension parameters used here.
struct MOTHeader
{
    std::size_t bodySize = 0;
    std::size_t headerSize = 0;
    int contentType = -1;
    int contentSubtype = -1;
    std::string name;
    std::string mimeType;
    int compression = 0;
    bool scrambled = false; // CAInfo: conditional access, which is not supported
};

// Calls parameter(id, data, length) for each parameter of a header or
// directory extension; false if they overrun it.
template <typename Parameter>
bool parseParameters(const std::uint8_t* data, std::size_t size, Parameter parameter)
{
    // A PLI selects the data field length (EN 301 234 clause 6.2).
    std::size_t position = 0;

    while (position < size)
    {
        const int pli = data[position] >> 6;
        const int id = data[position] & 0x3f;
        ++position;
        std::size_t length = 0;

        if (pli == 1) {
            length = 1U;
        } else if (pli == 2) {
            length = 4U;
        }
        else if (pli == 3)
        {
            if (position >= size) {
                return false;
            }

            if (data[position] & 0x80U)
            {
                if (position + 2U > size) {
                    return false;
                }

                length = (static_cast<std::size_t>(data[position] & 0x7fU) << 8) | data[position + 1U];
                position += 2U;
            }
            else
            {
                length = data[position] & 0x7fU;
                ++position;
            }
        }

        if (position + length > size) {
            return false;
        }

        parameter(id, data + position, length);
        position += length;
    }

    return true;
}

bool parseHeader(const std::uint8_t* data, std::size_t size, MOTHeader& header)
{
    // Header core (EN 301 234 clause 6.1): body size (28 bits), header size
    // (13), content type (6) and subtype (9).
    if (size < 7U) {
        return false;
    }

    header.bodySize = (static_cast<std::size_t>(data[0]) << 20) | (static_cast<std::size_t>(data[1]) << 12)
        | (static_cast<std::size_t>(data[2]) << 4) | (data[3] >> 4);
    header.headerSize = (static_cast<std::size_t>(data[3] & 0x0fU) << 9) | (static_cast<std::size_t>(data[4]) << 1)
        | (data[5] >> 7);
    header.contentType = (data[5] >> 1) & 0x3f;
    header.contentSubtype = ((data[5] & 0x01) << 8) | data[6];

    if ((header.headerSize < 7U) || (header.headerSize > size)) {
        return false;
    }

    return parseParameters(data + 7, header.headerSize - 7U, [&header](int id, const std::uint8_t* field, std::size_t length) {
        if ((id == 0x0c) && (length >= 1U)) {
            header.name.assign(reinterpret_cast<const char*>(field) + 1, length - 1U); // after its character set byte
        } else if (id == 0x10) {
            header.mimeType.assign(reinterpret_cast<const char*>(field), length);
        } else if ((id == 0x11) && (length == 1U)) {
            header.compression = field[0];
        } else if (id == 0x23) {
            header.scrambled = true;
        }
    });
}

std::uint32_t crc32(const std::vector<std::uint8_t>& data)
{
    // RFC 1952 section 8
    std::uint32_t crc = 0xffffffffU;

    for (std::uint8_t byte : data)
    {
        crc ^= byte;

        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }

    return ~crc;
}

} // namespace

void DRMPacketDemultiplexer::reset()
{
    m_continuity.fill(-1);
    for (auto& unit : m_units) {
        unit.clear();
    }

    m_unitValid.fill(false);
    m_lostPackets.fill(0);
    m_validPackets = 0;
    m_invalidPackets = 0;
}

void DRMPacketDemultiplexer::frameLost()
{
    // The next packet's continuity index cannot show how many were lost
    m_continuity.fill(-1);
    for (auto& unit : m_units) {
        unit.clear();
    }

    m_unitValid.fill(false);
    m_lostPackets.fill(0);
}

void DRMPacketDemultiplexer::push(
    const std::vector<std::uint8_t>& stream,
    int packetLengthBytes,
    int packetId,
    bool dataUnits,
    std::vector<std::vector<std::uint8_t>>& completed
)
{
    if (packetLengthBytes <= 0 || packetId < 0 || packetId > 3) {
        return;
    }

    // A header byte, the data field and a CRC over both.
    const std::size_t packetBytes = static_cast<std::size_t>(packetLengthBytes) + 3U;

    for (std::size_t offset = 0; offset + packetBytes <= stream.size(); offset += packetBytes)
    {
        const std::uint8_t* packet = stream.data() + offset;
        const std::uint16_t crc = static_cast<std::uint16_t>((packet[packetBytes - 2U] << 8) | packet[packetBytes - 1U]);

        if (CRC::crc16Bytes(packet, packetBytes - 2U) != crc)
        {
            // Its packet Id is unknown; the continuity index of the next
            // good packet shows which data unit lost it (while fewer than 8
            // are lost).
            ++m_invalidPackets;
            for (int& lost : m_lostPackets) {
                lost = std::min(lost + 1, 8);
            }

            continue;
        }

        ++m_validPackets;
        const bool first = (packet[0] & 0x80U) != 0U;
        const bool last = (packet[0] & 0x40U) != 0U;
        const int id = (packet[0] >> 4) & 0x03;
        const bool padded = (packet[0] & 0x08U) != 0U;
        const int continuity = packet[0] & 0x07;

        if (m_continuity[static_cast<std::size_t>(id)] >= 0
            && (((m_continuity[static_cast<std::size_t>(id)] + 1) % 8 != continuity)
                || (m_lostPackets[static_cast<std::size_t>(id)] >= 8))) {
            m_unitValid[static_cast<std::size_t>(id)] = false;
        }

        m_continuity[static_cast<std::size_t>(id)] = continuity;
        m_lostPackets[static_cast<std::size_t>(id)] = 0;

        // A padded packet's first data byte is its count of useful bytes.
        const std::uint8_t* data = packet + 1;
        std::size_t useful = static_cast<std::size_t>(packetLengthBytes);

        if (padded)
        {
            useful = data[0];
            ++data;

            if (useful > static_cast<std::size_t>(packetLengthBytes) - 1U)
            {
                m_unitValid[static_cast<std::size_t>(id)] = false;
                continue;
            }
        }

        if (id != packetId) {
            continue;
        }

        if (!dataUnits)
        {
            if (useful > 0U) {
                completed.emplace_back(data, data + useful);
            }

            continue;
        }

        std::vector<std::uint8_t>& unit = m_units[static_cast<std::size_t>(id)];

        if (first)
        {
            unit.clear();
            m_unitValid[static_cast<std::size_t>(id)] = true;
        }

        // An empty packet (first, last, no useful data) only fills the frame.
        if (!m_unitValid[static_cast<std::size_t>(id)] || (first && last && useful == 0U)) {
            continue;
        }

        unit.insert(unit.end(), data, data + useful);

        if (unit.size() > MaximumDataUnitBytes)
        {
            m_unitValid[static_cast<std::size_t>(id)] = false;
            unit.clear();
            continue;
        }

        if (last)
        {
            completed.push_back(std::move(unit));
            unit.clear();
            m_unitValid[static_cast<std::size_t>(id)] = false;
        }
    }
}

bool MOTDecoder::Segments::complete() const
{
    if (last < 0 || parts.size() != static_cast<std::size_t>(last) + 1U) {
        return false;
    }

    return parts.begin()->first == 0 && parts.rbegin()->first == last;
}

std::vector<std::uint8_t> MOTDecoder::Segments::join() const
{
    std::vector<std::uint8_t> joined;
    joined.reserve(bytes);

    for (const auto& part : parts) {
        joined.insert(joined.end(), part.second.begin(), part.second.end());
    }

    return joined;
}

void MOTDecoder::reset()
{
    m_pending.clear();
    m_delivered.clear();
    m_clock = 0;
    m_invalidDataGroups = 0;
}

void MOTDecoder::push(const std::vector<std::uint8_t>& dataGroup, std::vector<MOTObject>& completed)
{
    // Header mode: headers (type 3) and bodies (type 4), which need the
    // segment and transport Id fields. Directories (types 6 and 7) are not
    // used by the slideshow.
    MOTSegment segment;
    const SegmentResult result = parseSegment(dataGroup, (1U << 3) | (1U << 4), segment);

    if (result == SegmentResult::Invalid)
    {
        ++m_invalidDataGroups;
        return;
    }

    if (result != SegmentResult::Segment) {
        return;
    }

    const int type = segment.type;
    const bool lastSegment = segment.last;
    const int segmentNumber = segment.number;
    const int transportId = segment.transportId;
    const std::size_t position = segment.position;
    const std::size_t segmentSize = segment.size;
    auto found = m_pending.find(transportId);

    if (found == m_pending.end())
    {
        // Keep the most recently used objects only.
        if (m_pending.size() >= MaximumObjects)
        {
            const auto oldest = std::min_element(m_pending.begin(), m_pending.end(),
                [](const auto& left, const auto& right) { return left.second.lastUse < right.second.lastUse; });
            m_pending.erase(oldest);
        }

        found = m_pending.emplace(transportId, Pending()).first;
    }

    Pending& pending = found->second;
    pending.lastUse = ++m_clock;
    Segments& segments = type == 3 ? pending.header : pending.body;

    if (segments.parts.find(segmentNumber) == segments.parts.end())
    {
        segments.parts.emplace(segmentNumber, std::vector<std::uint8_t>(
            dataGroup.begin() + static_cast<std::ptrdiff_t>(position),
            dataGroup.begin() + static_cast<std::ptrdiff_t>(position + segmentSize)));
        segments.bytes += segmentSize;
    }

    if (lastSegment) {
        segments.last = segmentNumber;
    }

    if (pending.header.bytes + pending.body.bytes > MaximumObjectBytes)
    {
        m_pending.erase(found);
        ++m_invalidDataGroups;
        return;
    }

    if (pending.header.complete() && pending.body.complete()) {
        complete(transportId, completed);
    }
}

bool MOTDecoder::parseHeader(const std::vector<std::uint8_t>& header, MOTObject& object)
{
    MOTHeader parsed;

    if (!modemdrm::parseHeader(header.data(), header.size(), parsed) || (parsed.bodySize != object.body.size())
        || parsed.scrambled) {
        return false;
    }

    object.contentType = parsed.contentType;
    object.contentSubtype = parsed.contentSubtype;
    object.name = parsed.name;
    object.mimeType = parsed.mimeType;
    return true;
}

void MOTDecoder::complete(int transportId, std::vector<MOTObject>& completed)
{
    const auto found = m_pending.find(transportId);
    MOTObject object;
    object.transportId = transportId;
    object.body = found->second.body.join();
    const std::vector<std::uint8_t> header = found->second.header.join();
    m_pending.erase(found);

    if (!parseHeader(header, object))
    {
        ++m_invalidDataGroups;
        return;
    }

    const std::pair<int, std::size_t> key(transportId, object.body.size());

    if (std::find(m_delivered.begin(), m_delivered.end(), key) != m_delivered.end()) {
        return; // a repetition of the slide shown
    }

    m_delivered.push_back(key);

    if (m_delivered.size() > DeliveredMemory) {
        m_delivered.pop_front();
    }

    completed.push_back(std::move(object));
}

const SDCDataApplication* DRMSlideshowDecoder::find(const SDCData& sdc, int shortId)
{
    const auto found = std::find_if(sdc.dataApplications.begin(), sdc.dataApplications.end(),
        [shortId](const SDCDataApplication& application) {
            return application.shortId == shortId && application.packetMode && application.dataUnits
                && application.applicationDomain == 1
                && application.userApplicationType == SDCDataApplication::MOTSlideshow;
        });

    return found == sdc.dataApplications.end() ? nullptr : &*found;
}

void DRMSlideshowDecoder::reset()
{
    m_packets.reset();
    m_mot.reset();
    m_configuredStream = -1;
    m_configuredPacketId = -1;
    m_configuredPacketLength = -1;
}

bool DRMSlideshowDecoder::setService(std::int64_t serviceIdentifier)
{
    const bool changed = (m_service >= 0) && (serviceIdentifier != m_service);

    if (changed) {
        reset();
    }

    m_service = serviceIdentifier;
    return changed;
}

void DRMSlideshowDecoder::push(
    const SDCDataApplication& application,
    const std::vector<std::uint8_t>& stream,
    std::vector<MOTObject>& completed
)
{
    if (application.streamId != m_configuredStream || application.packetId != m_configuredPacketId
        || application.packetLengthBytes != m_configuredPacketLength)
    {
        reset();
        m_configuredStream = application.streamId;
        m_configuredPacketId = application.packetId;
        m_configuredPacketLength = application.packetLengthBytes;
    }

    std::vector<std::vector<std::uint8_t>> dataGroups;
    m_packets.push(stream, application.packetLengthBytes, application.packetId, application.dataUnits, dataGroups);

    for (const auto& dataGroup : dataGroups) {
        m_mot.push(dataGroup, completed);
    }
}

bool MOTDirectoryDecoder::Segments::complete() const
{
    if ((last < 0) || (parts.size() != static_cast<std::size_t>(last) + 1U)) {
        return false;
    }

    return (parts.begin()->first == 0) && (parts.rbegin()->first == last);
}

std::vector<std::uint8_t> MOTDirectoryDecoder::Segments::join() const
{
    std::vector<std::uint8_t> joined;
    joined.reserve(bytes);

    for (const auto& part : parts) {
        joined.insert(joined.end(), part.second.begin(), part.second.end());
    }

    return joined;
}

void MOTDirectoryDecoder::reset()
{
    m_directorySegments.clear();
    m_directoryTransportId = -1;
    m_entries.clear();
    m_directoryIndex.clear();
    m_bodies.clear();
    m_unlisted.clear();
    m_pendingBytes = 0;
    m_fileTransportIds.clear();
    m_failed.clear();
    m_website = MOTWebsite();
    m_website.generation = ++m_generation;
    m_websiteBytes = 0;
    m_clock = 0;
    m_invalidDataGroups = 0;
}

bool MOTDirectoryDecoder::gunzip(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& output,
    std::size_t maximumOutput)
{
    // RFC 1952: a header, raw DEFLATE data, then CRC-32 and size.
    if ((size < 18U) || (data[0] != 0x1fU) || (data[1] != 0x8bU) || (data[2] != 8U)) {
        return false;
    }

    const int flags = data[3];
    std::size_t position = 10U;

    if (flags & 0x04)
    {
        // FEXTRA
        if (position + 2U > size) {
            return false;
        }

        position += 2U + (data[position] | (data[position + 1U] << 8));
    }

    for (int field : {0x08, 0x10})
    {
        // FNAME and FCOMMENT, each zero terminated
        if (flags & field)
        {
            while ((position < size) && (data[position] != 0U)) {
                ++position;
            }

            ++position;
        }
    }

    if (flags & 0x02) {
        position += 2U; // FHCRC
    }

    if (position + 8U > size) {
        return false;
    }

    const std::uint32_t crc = data[size - 8U] | (data[size - 7U] << 8) | (data[size - 6U] << 16)
        | (static_cast<std::uint32_t>(data[size - 5U]) << 24);
    const std::size_t expected = data[size - 4U] | (data[size - 3U] << 8) | (data[size - 2U] << 16)
        | (static_cast<std::size_t>(data[size - 1U]) << 24);
    return inflateRaw(data + position, size - position - 8U, output, maximumOutput) && (output.size() == expected)
        && (crc32(output) == crc);
}

bool MOTDirectoryDecoder::push(const std::vector<std::uint8_t>& dataGroup)
{
    // Directory mode: the directory (types 6 and 7) and bodies (type 4)
    MOTSegment segment;
    const SegmentResult result = parseSegment(dataGroup, (1U << 4) | (1U << 6) | (1U << 7), segment);

    if (result == SegmentResult::Invalid)
    {
        ++m_invalidDataGroups;
        return false;
    }

    if (result != SegmentResult::Segment) {
        return false;
    }

    const bool directory = segment.type != 4;

    // A repetition of the current directory, or of a body already filed
    if (directory && (segment.transportId == m_directoryTransportId)) {
        return false;
    }

    if (!directory)
    {
        const auto entry = m_entries.find(segment.transportId);

        if ((entry != m_entries.end()) && (m_fileTransportIds.count(entry->second.name) != 0U)
            && (m_fileTransportIds.at(entry->second.name) == segment.transportId)) {
            return false;
        }

        if ((m_unlisted.count(segment.transportId) != 0U) || (m_failed.count(segment.transportId) != 0U)) {
            return false;
        }
    }

    // A compressed and an uncompressed directory may share a transport Id
    const int key = directory ? ((segment.type << 16) | segment.transportId) : segment.transportId;
    std::map<int, Segments>& entities = directory ? m_directorySegments : m_bodies;
    Segments& segments = entities[key];
    segments.lastUse = ++m_clock;

    if (segments.parts.find(segment.number) == segments.parts.end())
    {
        segments.parts.emplace(segment.number, std::vector<std::uint8_t>(
            dataGroup.begin() + static_cast<std::ptrdiff_t>(segment.position),
            dataGroup.begin() + static_cast<std::ptrdiff_t>(segment.position + segment.size)));
        segments.bytes += segment.size;
        m_pendingBytes += segment.size + SegmentOverhead;
    }

    if (segment.last) {
        segments.last = segment.number;
    }

    if (segments.bytes > MaximumObjectBytes)
    {
        release(segments);
        entities.erase(key);
        ++m_invalidDataGroups;
        return false;
    }

    // Keep the memory bounded: drop the least recently used, an entity being
    // received or a body waiting for its directory (which, kept first, could
    // fill the budget and keep that directory from assembling)
    const auto leastRecent = [](auto& map) {
        return std::min_element(map.begin(), map.end(),
            [](const auto& left, const auto& right) { return left.second.lastUse < right.second.lastUse; });
    };

    while ((m_pendingBytes > MaximumPendingBytes) || (m_directorySegments.size() + m_bodies.size() > MaximumPendingEntities))
    {
        const auto oldestDirectory = leastRecent(m_directorySegments);
        const auto oldestBody = leastRecent(m_bodies);
        const auto oldestUnlisted = leastRecent(m_unlisted);
        const bool haveDirectory = oldestDirectory != m_directorySegments.end();
        const bool haveBody = oldestBody != m_bodies.end();
        const bool tooManyEntities = m_directorySegments.size() + m_bodies.size() > MaximumPendingEntities;
        const std::uint64_t entityUse = std::min(haveDirectory ? oldestDirectory->second.lastUse : std::numeric_limits<std::uint64_t>::max(),
            haveBody ? oldestBody->second.lastUse : std::numeric_limits<std::uint64_t>::max());

        if (!haveDirectory && !haveBody && (oldestUnlisted == m_unlisted.end())) {
            break;
        }

        if (!tooManyEntities && (oldestUnlisted != m_unlisted.end()) && (oldestUnlisted->second.lastUse < entityUse)) {
            dropUnlisted();
        } else if (!haveDirectory && !haveBody) {
            dropUnlisted();
        }
        else if (!haveBody || (haveDirectory && (oldestDirectory->second.lastUse < oldestBody->second.lastUse)))
        {
            release(oldestDirectory->second);
            m_directorySegments.erase(oldestDirectory);
        }
        else
        {
            release(oldestBody->second);
            m_bodies.erase(oldestBody);
        }
    }

    const auto current = entities.find(key);

    if ((current == entities.end()) || !current->second.complete()) {
        return false;
    }

    const std::vector<std::uint8_t> entity = current->second.join();
    release(current->second);
    entities.erase(current);
    return directory ? applyDirectory(segment.transportId, entity) : addFile(segment.transportId, entity);
}

void MOTDirectoryDecoder::release(const Segments& segments)
{
    m_pendingBytes -= std::min(m_pendingBytes, segments.bytes + segments.parts.size() * SegmentOverhead);
}

void MOTDirectoryDecoder::dropUnlisted()
{
    // The body that has waited longest
    const auto oldest = std::min_element(m_unlisted.begin(), m_unlisted.end(),
        [](const auto& left, const auto& right) { return left.second.lastUse < right.second.lastUse; });
    m_pendingBytes -= std::min(m_pendingBytes, oldest->second.data.size() + SegmentOverhead);
    m_unlisted.erase(oldest);
}

bool MOTDirectoryDecoder::applyDirectory(int transportId, const std::vector<std::uint8_t>& entity)
{
    std::vector<std::uint8_t> uncompressed;
    const std::vector<std::uint8_t>* directory = &entity;

    if (!entity.empty() && (entity[0] & 0x80U))
    {
        // Compressed directory (clause 7.2.8): entity size, CompressionId and
        // uncompressed length, then the compressed directory.
        if (entity.size() < 9U)
        {
            ++m_invalidDataGroups;
            return false;
        }

        const std::size_t entitySize = std::min(entity.size(), (static_cast<std::size_t>(entity[0] & 0x3fU) << 24)
            | (static_cast<std::size_t>(entity[1]) << 16) | (static_cast<std::size_t>(entity[2]) << 8) | entity[3]);

        if ((entity[4] != 1U) || (entitySize < 9U) || !gunzip(entity.data() + 9, entitySize - 9U, uncompressed, MaximumObjectBytes))
        {
            ++m_invalidDataGroups;
            return false;
        }

        directory = &uncompressed;
    }

    // Directory (clause 7.2.3): size, objects, carousel period, segment size
    // and the extension length, then the extension and the entries.
    const std::vector<std::uint8_t>& bytes = *directory;

    if ((bytes.size() < 13U) || (bytes[0] & 0xc0U))
    {
        ++m_invalidDataGroups;
        return false;
    }

    const std::size_t size = std::min(bytes.size(), (static_cast<std::size_t>(bytes[0] & 0x3fU) << 24)
        | (static_cast<std::size_t>(bytes[1]) << 16) | (static_cast<std::size_t>(bytes[2]) << 8) | bytes[3]);
    const std::size_t objects = (static_cast<std::size_t>(bytes[4]) << 8) | bytes[5];
    const std::size_t extensionLength = (static_cast<std::size_t>(bytes[11]) << 8) | bytes[12];

    if ((size < 13U + extensionLength) || (objects > MaximumObjects))
    {
        ++m_invalidDataGroups;
        return false;
    }

    std::vector<std::pair<int, std::string>> directoryIndex;
    const bool extensionValid = parseParameters(bytes.data() + 13, extensionLength,
        [&directoryIndex](int id, const std::uint8_t* field, std::size_t length) {
            // DirectoryIndex: TS 101 498-1 gives 0x22, EN 301 234 table 3 0x20
            if (((id == 0x22) || (id == 0x20)) && (length >= 2U))
            {
                std::string name(reinterpret_cast<const char*>(field) + 1, length - 1U);

                while (!name.empty() && (name.front() == '/')) {
                    name.erase(name.begin());
                }

                directoryIndex.emplace_back(field[0], name);
            }
        });
    std::map<int, Entry> entries;
    std::size_t position = 13U + extensionLength;

    for (std::size_t object = 0; extensionValid && (object < objects); ++object)
    {
        MOTHeader header;

        if ((position + 2U > size) || !parseHeader(bytes.data() + position + 2, size - position - 2U, header)) {
            break;
        }

        Entry entry;
        entry.name = header.name;
        entry.scrambled = header.scrambled;

        // ContentNames have no leading "/" (TS 101 498-1 clause 5.4.1), but be lenient
        while (!entry.name.empty() && (entry.name.front() == '/')) {
            entry.name.erase(entry.name.begin());
        }

        entry.mimeType = header.mimeType;
        entry.compression = header.compression;
        entry.bodySize = header.bodySize;
        entries[(bytes[position] << 8) | bytes[position + 1U]] = entry;
        position += 2U + header.headerSize;
    }

    if (!extensionValid || (entries.size() != objects))
    {
        ++m_invalidDataGroups;
        return false;
    }

    m_directoryTransportId = transportId;
    m_entries = std::move(entries);
    m_directoryIndex = std::move(directoryIndex);
    m_website.directoryObjects = static_cast<int>(m_entries.size());
    m_website.listed.clear();

    for (const auto& entry : m_entries) {
        m_website.listed.insert(entry.second.name);
    }

    // Files the new directory no longer names are gone (a new version of one
    // replaces it when its body arrives); bodies it does not list will not be
    // needed.
    for (auto file = m_website.files.begin(); file != m_website.files.end();)
    {
        const std::string& name = file->first;

        if (m_website.listed.count(name) == 0U)
        {
            m_websiteBytes -= file->second.data->size();
            m_fileTransportIds.erase(file->first);
            file = m_website.files.erase(file);
        }
        else
        {
            ++file;
        }
    }

    for (auto body = m_bodies.begin(); body != m_bodies.end();)
    {
        if (m_entries.count(body->first)) {
            ++body;
        }
        else
        {
            release(body->second);
            body = m_bodies.erase(body);
        }
    }

    m_failed.clear();

    // Bodies that arrived before their directory
    std::map<int, Unlisted> unlisted;
    unlisted.swap(m_unlisted);

    for (const auto& body : unlisted)
    {
        m_pendingBytes -= std::min(m_pendingBytes, body.second.data.size() + SegmentOverhead);

        if (m_entries.count(body.first)) {
            addFile(body.first, body.second.data);
        }
    }

    chooseStartPage();
    return true;
}

bool MOTDirectoryDecoder::addFile(int transportId, const std::vector<std::uint8_t>& body)
{
    const auto entry = m_entries.find(transportId);

    if (entry == m_entries.end())
    {
        // Keep it for the directory that lists it
        if (m_pendingBytes + body.size() + SegmentOverhead <= MaximumPendingBytes)
        {
            m_unlisted[transportId] = {body, ++m_clock};
            m_pendingBytes += body.size() + SegmentOverhead;
        }

        return false;
    }

    std::vector<std::uint8_t> data;

    // Conditional access is not supported: a scrambled object is discarded
    if ((body.size() != entry->second.bodySize) || entry->second.scrambled)
    {
        m_failed.insert(transportId);
        ++m_invalidDataGroups;
        return false;
    }

    if (entry->second.compression == 0) {
        data = body;
    }
    else if ((entry->second.compression != 1) || !gunzip(body.data(), body.size(), data, MaximumObjectBytes))
    {
        // Not gzip, the only CompressionId defined (TS 101 756 table 18)
        m_failed.insert(transportId);
        ++m_invalidDataGroups;
        return false;
    }

    const std::string& name = entry->second.name;
    const auto existing = m_website.files.find(name);
    const std::size_t replaced = existing == m_website.files.end() ? 0U : existing->second.data->size();

    if (name.empty() || (m_websiteBytes - replaced + data.size() > MaximumWebsiteBytes))
    {
        m_failed.insert(transportId);
        return false;
    }

    m_websiteBytes = m_websiteBytes - replaced + data.size();
    MOTWebsiteFile& file = m_website.files[name];
    file.mimeType = entry->second.mimeType;
    file.data = std::make_shared<const std::vector<std::uint8_t>>(std::move(data));
    m_fileTransportIds[name] = transportId;
    chooseStartPage();
    return true;
}

void MOTDirectoryDecoder::chooseStartPage()
{
    const auto listed = [this](const std::string& name) {
        return m_website.listed.count(name) != 0U;
    };

    // The root's DirectoryIndex for a profile the service signals, else the
    // first, if the directory lists it (some carousels name a missing page)
    std::string start;

    for (const auto& index : m_directoryIndex)
    {
        if (listed(index.second)
            && (std::find(m_profiles.begin(), m_profiles.end(), static_cast<std::uint8_t>(index.first)) != m_profiles.end()))
        {
            start = index.second;
            break;
        }
    }

    for (const auto& index : m_directoryIndex)
    {
        if (start.empty() && listed(index.second)) {
            start = index.second;
        }
    }

    // Otherwise the usual index, else the first page

    for (const char *name : {"index.html", "index.htm"})
    {
        if (start.empty() && listed(name)) {
            start = name;
        }
    }

    for (const auto& entry : m_entries)
    {
        const std::string& name = entry.second.name;
        const auto endsWith = [&name](const std::string& suffix) {
            return (name.size() > suffix.size()) && (name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0);
        };

        if (start.empty() && ((entry.second.mimeType.rfind("text/html", 0) == 0) || endsWith(".htm") || endsWith(".html"))) {
            start = name;
        }
    }

    m_website.startPage = start;

    // A directory URL gives the DirectoryIndex's file, else index.html
    const bool fromIndex = std::any_of(m_directoryIndex.begin(), m_directoryIndex.end(),
        [&start](const auto& index) { return index.second == start; });
    const auto slash = start.rfind('/');
    m_website.indexName = !fromIndex ? std::string("index.html")
        : (slash == std::string::npos ? start : start.substr(slash + 1U));

    if (m_website.indexName.empty()) {
        m_website.indexName = "index.html";
    }
}

const SDCDataApplication* DRMWebsiteDecoder::find(const SDCData& sdc, int shortId)
{
    const auto found = std::find_if(sdc.dataApplications.begin(), sdc.dataApplications.end(),
        [shortId](const SDCDataApplication& application) {
            return (application.shortId == shortId) && application.packetMode && application.dataUnits
                && (application.applicationDomain == 1)
                && (application.userApplicationType == SDCDataApplication::BroadcastWebsite);
        });

    return found == sdc.dataApplications.end() ? nullptr : &*found;
}

void DRMWebsiteDecoder::reset()
{
    m_packets.reset();
    m_mot.reset();
    m_configuredStream = -1;
    m_configuredPacketId = -1;
    m_configuredPacketLength = -1;
}

bool DRMWebsiteDecoder::setService(std::int64_t serviceIdentifier)
{
    const bool changed = (m_service >= 0) && (serviceIdentifier != m_service);

    if (changed) {
        reset();
    }

    m_service = serviceIdentifier;
    return changed;
}

bool DRMWebsiteDecoder::push(const SDCDataApplication& application, const std::vector<std::uint8_t>& stream)
{
    // A changed component starts again: the website shown goes too
    bool changed = false;

    if ((application.streamId != m_configuredStream) || (application.packetId != m_configuredPacketId)
        || (application.packetLengthBytes != m_configuredPacketLength))
    {
        changed = !website().files.empty();
        reset();
        m_configuredStream = application.streamId;
        m_configuredPacketId = application.packetId;
        m_configuredPacketLength = application.packetLengthBytes;
    }

    // The user application data lists the service's profiles, up to a 0x00
    const auto end = std::find(application.applicationData.begin(), application.applicationData.end(), 0U);
    m_mot.setProfiles(std::vector<std::uint8_t>(application.applicationData.begin(), end));

    std::vector<std::vector<std::uint8_t>> dataGroups;
    m_packets.push(stream, application.packetLengthBytes, application.packetId, application.dataUnits, dataGroups);

    for (const auto& dataGroup : dataGroups) {
        changed = m_mot.push(dataGroup) || changed;
    }

    return changed;
}

} // namespace modemdrm
