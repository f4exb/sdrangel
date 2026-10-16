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
#include <limits>

#include "crc.h"
#include "inflate.h"
#include "mot.h"
#include "text.h"

namespace modemdr
{

namespace
{

// One segment of an MOT entity in an MSC data group: its data group type, segment number
// and transport Id.
struct MOTSegment
{
    int type = -1;
    bool last = false;
    int number = 0;
    int transportId = -1;
    std::size_t position = 0;
    std::size_t size = 0;
};

enum class SegmentResult
{
    Segment,
    Other,
    Invalid
};

SegmentResult parseSegment(const std::vector<uint8_t>& dataGroup, unsigned types, MOTSegment& segment)
{
    // MSC data group header (EN 300 401 clause 5.3.3.1)
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
        if (!CRC::crc16Check(dataGroup.data(), end)) {
            return SegmentResult::Invalid;
        }
        end -= 2U;
    }

    if (!(types & (1U << segment.type)) || !segmented || !userAccess) {
        return SegmentResult::Other;
    }

    std::size_t position = 2U + (extension ? 2U : 0U);

    // Session header: segment field, then the user access field
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

    // MOT segmentation header (EN 301 234 clause 5.1.1): repetition count and segment size
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

// Calls parameter(id, data, length) for each header extension parameter; false if they
// overrun it.
template <typename Parameter>
bool parseParameters(const uint8_t *data, std::size_t size, Parameter parameter)
{
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

// A ContentName: its first byte gives its character set (EN 301 234 clause 6.2.2.1.1)
std::string contentName(const uint8_t *field, std::size_t length)
{
    if (length < 1U) {
        return "";
    }
    return Text::contentNameToUTF8(field + 1, length - 1U, field[0] >> 4);
}

// Untrusted text that should be ASCII (MIME types, URLs)
std::string asciiString(const uint8_t *field, std::size_t length)
{
    std::string s;

    for (std::size_t i = 0; i < length; i++)
    {
        if ((field[i] >= 0x20) && (field[i] < 0x7f)) {
            s.push_back(static_cast<char>(field[i]));
        }
    }
    return s;
}

// The header core (EN 301 234 clause 6.1): body size (28 bits), header size (13), content type
// (6) and subtype (9). False if it is short or gives a header size it does not have.
bool parseHeaderCore(const uint8_t *data, std::size_t size, std::size_t& bodySize, std::size_t& headerSize,
    int& contentType, int& contentSubtype)
{
    if (size < 7U) {
        return false;
    }

    bodySize = (static_cast<std::size_t>(data[0]) << 20) | (static_cast<std::size_t>(data[1]) << 12)
        | (static_cast<std::size_t>(data[2]) << 4) | (data[3] >> 4);
    headerSize = (static_cast<std::size_t>(data[3] & 0x0fU) << 9) | (static_cast<std::size_t>(data[4]) << 1)
        | (data[5] >> 7);
    contentType = (data[5] >> 1) & 0x3f;
    contentSubtype = ((data[5] & 0x01) << 8) | data[6];

    return (headerSize >= 7U) && (headerSize <= size);
}

bool parseHeader(const std::vector<uint8_t>& data, MOTObject& object, std::size_t& bodySize)
{
    std::size_t headerSize;

    if (!parseHeaderCore(data.data(), data.size(), bodySize, headerSize, object.contentType, object.contentSubtype)) {
        return false;
    }

    bool scrambled = false;
    const bool ok = parseParameters(data.data() + 7, headerSize - 7U, [&](int id, const uint8_t *field, std::size_t length) {
        switch (id)
        {
        case 0x05: // TriggerTime: validity flag zero means "now"
            if (length >= 4U) {
                object.triggerNow = (field[0] & 0x80U) == 0U;
            }
            break;
        case 0x0c:
            object.name = contentName(field, length);
            break;
        case 0x10:
            object.mimeType = asciiString(field, length);
            break;
        case 0x23: // CAInfo: conditional access is not supported
            scrambled = true;
            break;
        case 0x25: // CategoryID/SlideID
            if (length >= 2U)
            {
                object.categoryId = field[0];
                object.slideId = field[1];
            }
            break;
        case 0x26:
            object.categoryTitle = Text::toUTF8(field, length, 15);
            break;
        case 0x27:
            object.clickThroughURL = asciiString(field, length);
            break;
        case 0x28:
            object.alternativeLocationURL = asciiString(field, length);
            break;
        case 0x29:
            object.alert = (length >= 1U) && (field[0] == 1);
            break;
        default:
            break;
        }
    });

    return ok && !scrambled;
}

// A directory entry's header: its file's name, MIME type and application parameters
bool parseDirectoryHeader(const uint8_t *data, std::size_t size, MOTFile& file, int& compression, bool& scrambled,
    std::size_t& bodySize, std::size_t& headerSize)
{
    if (!parseHeaderCore(data, size, bodySize, headerSize, file.contentType, file.contentSubtype)) {
        return false;
    }

    return parseParameters(data + 7, headerSize - 7U, [&](int id, const uint8_t *field, std::size_t length) {
        if (id == 0x0c)
        {
            file.name = contentName(field, length);
            // ContentNames have no leading "/", but be lenient
            while (!file.name.empty() && (file.name.front() == '/')) {
                file.name.erase(file.name.begin());
            }
        }
        else if (id == 0x10)
        {
            file.mimeType = asciiString(field, length);
        }
        else if ((id == 0x11) && (length == 1U))
        {
            compression = field[0];
        }
        else if (id == 0x23)
        {
            scrambled = true;
        }
        else if (id >= 0x25)
        {
            file.parameters[id].assign(field, field + length);
        }
    });
}

} // anonymous namespace

bool MOTDecoder::Segments::complete() const
{
    if ((last < 0) || (parts.size() != static_cast<std::size_t>(last) + 1U)) {
        return false;
    }
    return (parts.begin()->first == 0) && (parts.rbegin()->first == last);
}

std::vector<uint8_t> MOTDecoder::Segments::join() const
{
    std::vector<uint8_t> joined;
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

void MOTDecoder::push(const std::vector<uint8_t>& dataGroup, std::vector<MOTObject>& completed)
{
    // Header mode: headers (type 3) and bodies (type 4)
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

    auto found = m_pending.find(segment.transportId);

    if (found == m_pending.end())
    {
        // Keep the most recently used objects only
        if (m_pending.size() >= MaximumObjects)
        {
            const auto oldest = std::min_element(m_pending.begin(), m_pending.end(),
                [](const auto& left, const auto& right) { return left.second.lastUse < right.second.lastUse; });
            m_pending.erase(oldest);
        }
        found = m_pending.emplace(segment.transportId, Pending()).first;
    }

    Pending& pending = found->second;
    pending.lastUse = ++m_clock;
    Segments& segments = segment.type == 3 ? pending.header : pending.body;

    if (segments.parts.find(segment.number) == segments.parts.end())
    {
        segments.parts.emplace(segment.number, std::vector<uint8_t>(
            dataGroup.begin() + static_cast<std::ptrdiff_t>(segment.position),
            dataGroup.begin() + static_cast<std::ptrdiff_t>(segment.position + segment.size)));
        segments.bytes += segment.size;
    }

    if (segment.last) {
        segments.last = segment.number;
    }

    if (pending.header.bytes + pending.body.bytes > MaximumObjectBytes)
    {
        m_pending.erase(found);
        ++m_invalidDataGroups;
        return;
    }

    if (pending.header.complete() && pending.body.complete()) {
        complete(segment.transportId, completed);
    }
}

void MOTDecoder::complete(int transportId, std::vector<MOTObject>& completed)
{
    const auto found = m_pending.find(transportId);
    MOTObject object;
    object.transportId = transportId;
    object.body = found->second.body.join();
    const std::vector<uint8_t> header = found->second.header.join();
    m_pending.erase(found);

    std::size_t bodySize = 0;
    if (!parseHeader(header, object, bodySize) || (bodySize != object.body.size()))
    {
        ++m_invalidDataGroups;
        return;
    }

    const std::pair<int, std::size_t> key(transportId, object.body.size());

    if (std::find(m_delivered.begin(), m_delivered.end(), key) != m_delivered.end()) {
        return; // A repetition of the object delivered last
    }

    m_delivered.push_back(key);
    if (m_delivered.size() > DeliveredMemory) {
        m_delivered.pop_front();
    }

    completed.push_back(std::move(object));
}

bool MOTDirectoryDecoder::Segments::complete() const
{
    if ((last < 0) || (parts.size() != static_cast<std::size_t>(last) + 1U)) {
        return false;
    }

    return (parts.begin()->first == 0) && (parts.rbegin()->first == last);
}

std::vector<uint8_t> MOTDirectoryDecoder::Segments::join() const
{
    std::vector<uint8_t> joined;
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

bool MOTDirectoryDecoder::push(const std::vector<uint8_t>& dataGroup)
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

        if ((entry != m_entries.end()) && (m_fileTransportIds.count(entry->second.file.name) != 0U)
            && (m_fileTransportIds.at(entry->second.file.name) == segment.transportId)) {
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
        segments.parts.emplace(segment.number, std::vector<uint8_t>(
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
        const uint64_t entityUse = std::min(haveDirectory ? oldestDirectory->second.lastUse : std::numeric_limits<uint64_t>::max(),
            haveBody ? oldestBody->second.lastUse : std::numeric_limits<uint64_t>::max());

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

    const std::vector<uint8_t> entity = current->second.join();
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

bool MOTDirectoryDecoder::applyDirectory(int transportId, const std::vector<uint8_t>& entity)
{
    std::vector<uint8_t> uncompressed;
    const std::vector<uint8_t>* directory = &entity;

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
    const std::vector<uint8_t>& bytes = *directory;

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
        [&directoryIndex](int id, const uint8_t* field, std::size_t length) {
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
        Entry entry;
        std::size_t headerSize = 0;

        if ((position + 2U > size)
            || !parseDirectoryHeader(bytes.data() + position + 2, size - position - 2U, entry.file, entry.compression,
                entry.scrambled, entry.bodySize, headerSize)) {
            break;
        }

        entry.file.transportId = (bytes[position] << 8) | bytes[position + 1U];
        entries[entry.file.transportId] = entry;
        position += 2U + headerSize;
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
        m_website.listed.insert(entry.second.file.name);
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

bool MOTDirectoryDecoder::addFile(int transportId, const std::vector<uint8_t>& body)
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

    std::vector<uint8_t> data;

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

    const std::string& name = entry->second.file.name;
    const auto existing = m_website.files.find(name);
    const std::size_t replaced = existing == m_website.files.end() ? 0U : existing->second.data->size();

    if (name.empty() || (m_websiteBytes - replaced + data.size() > MaximumWebsiteBytes))
    {
        m_failed.insert(transportId);
        return false;
    }

    m_websiteBytes = m_websiteBytes - replaced + data.size();
    MOTFile& file = m_website.files[name];
    file = entry->second.file;
    file.data = std::make_shared<const std::vector<uint8_t>>(std::move(data));
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
            && (std::find(m_profiles.begin(), m_profiles.end(), static_cast<uint8_t>(index.first)) != m_profiles.end()))
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
        const std::string& name = entry.second.file.name;
        const auto endsWith = [&name](const std::string& suffix) {
            return (name.size() > suffix.size()) && (name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0);
        };

        if (start.empty() && ((entry.second.file.mimeType.rfind("text/html", 0) == 0) || endsWith(".htm") || endsWith(".html"))) {
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

} // namespace modemdr
