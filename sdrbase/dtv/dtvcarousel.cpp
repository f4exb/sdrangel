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
#include <cstring>

#include "dtv/dtvcarousel.h"

namespace {

// Reads big-endian fields, stopping (ok false) at the end rather than reading past it
struct Reader
{
    const uint8_t *m_data;
    int m_length;
    int m_offset = 0;
    bool m_ok = true;

    Reader(const uint8_t *data, int length) : m_data(data), m_length(length) {}

    bool has(int n)
    {
        if ((n < 0) || (m_offset + n > m_length)) {
            m_ok = false;
        }
        return m_ok;
    }
    uint32_t u8() { return has(1) ? m_data[m_offset++] : 0; }
    uint32_t u16()
    {
        if (!has(2)) {
            return 0;
        }
        const uint32_t v = (m_data[m_offset] << 8) | m_data[m_offset + 1];
        m_offset += 2;
        return v;
    }
    uint32_t u32()
    {
        if (!has(4)) {
            return 0;
        }
        const uint32_t v = ((uint32_t) m_data[m_offset] << 24) | (m_data[m_offset + 1] << 16) | (m_data[m_offset + 2] << 8) | m_data[m_offset + 3];
        m_offset += 4;
        return v;
    }
    uint64_t u64()
    {
        const uint64_t high = u32();
        return (high << 32) | u32();
    }
    const uint8_t *bytes(int n)
    {
        if (!has(n)) {
            return nullptr;
        }
        const uint8_t *p = m_data + m_offset;
        m_offset += n;
        return p;
    }
    void skip(int n) { bytes(n); }
    int left() const { return m_length - m_offset; }
    const uint8_t *here() const { return m_data + m_offset; }
};

constexpr int m_dsiMessage = 0x1006;
constexpr int m_diiMessage = 0x1002;
constexpr int m_ddbMessage = 0x1003;
constexpr uint32_t m_tagBIOP = 0x49534F06;              //!< TAG_BIOP profile
constexpr uint32_t m_tagObjectLocation = 0x49534F50;    //!< BIOP::ObjectLocation component
constexpr uint32_t m_tagConnBinder = 0x49534F40;        //!< DSM::ConnBinder component
constexpr int m_compressedModuleDescriptor = 0x09;

} // namespace

bool DTVCarousel::ObjectRef::operator<(const ObjectRef& other) const
{
    if (m_carouselId != other.m_carouselId) {
        return m_carouselId < other.m_carouselId;
    }
    if (m_moduleId != other.m_moduleId) {
        return m_moduleId < other.m_moduleId;
    }
    return m_objectKey < other.m_objectKey;
}

DTVCarousel::DTVCarousel(int pid) :
    m_pid(pid),
    m_carouselId(-1),
    m_revision(0)
{
}

void DTVCarousel::reset()
{
    m_carouselId = -1;
    m_gateway = ObjectRef();
    m_associationTags.clear();
    m_modules.clear();
    m_diiModules.clear();
    m_objects.clear();
    m_revision++;
}

QString DTVCarousel::kindName(Kind kind)
{
    switch (kind)
    {
    case Kind::File: return "file";
    case Kind::Directory: return "directory";
    case Kind::Gateway: return "service gateway";
    case Kind::Stream: return "stream";
    default: return "unknown";
    }
}

QString DTVCarousel::localPath(const QString& path)
{
    // Names have no separators, and aren't . or .. (parseModule), so only Windows' reserved characters are left
    QString local = path.startsWith('/') ? path.mid(1) : path;

    for (QChar& c : local)
    {
        if ((c < QChar(0x20)) || QString("\\:*?\"<>|").contains(c)) {
            c = '_';
        }
    }

    return local;
}

DTVCarousel::Kind DTVCarousel::kindFromId(const uint8_t *id, int length)
{
    // "fil", "dir", "srg", "str", "ste", usually with a terminating 0
    if (length < 3) {
        return Kind::Unknown;
    }

    if (std::memcmp(id, "fil", 3) == 0) {
        return Kind::File;
    } else if (std::memcmp(id, "dir", 3) == 0) {
        return Kind::Directory;
    } else if (std::memcmp(id, "srg", 3) == 0) {
        return Kind::Gateway;
    } else if ((std::memcmp(id, "str", 3) == 0) || (std::memcmp(id, "ste", 3) == 0)) {
        return Kind::Stream;
    } else {
        return Kind::Unknown;
    }
}

bool DTVCarousel::section(const uint8_t *s, int length)
{
    // The section header (8 bytes), then the message (from a 12 byte header with its adaptation), then the CRC
    if (length < 8 + 12 + 4) {
        return false;
    }

    const uint8_t *m = s + 8;
    const int messageLength = length - 8 - 4;

    // protocolDiscriminator 0x11 and dsmccType 0x03 (download) of the dsmccMessageHeader or dsmccDownloadDataHeader
    if ((m[0] != 0x11) || (m[1] != 0x03)) {
        return false;
    }

    const int messageId = (m[2] << 8) | m[3];
    const uint32_t id = ((uint32_t) m[4] << 24) | (m[5] << 16) | (m[6] << 8) | m[7];   // transactionId, or downloadId
    const int adaptationLength = m[9];
    const int bodyLength = std::min(messageLength - 12 - adaptationLength, ((m[10] << 8) | m[11]) - adaptationLength);

    if (bodyLength <= 0) {
        return false;
    }

    const uint8_t *body = m + 12 + adaptationLength;
    const int revision = m_revision;

    if ((s[0] == 0x3B) && (messageId == m_dsiMessage))
    {
        dsi(body, bodyLength);
    }
    else if ((s[0] == 0x3B) && (messageId == m_diiMessage))
    {
        dii(body, bodyLength, id);
    }
    else if ((s[0] == 0x3C) && (messageId == m_ddbMessage) && (m_carouselId >= 0) && (id == (uint32_t) m_carouselId))
    {
        ddb(body, bodyLength);
    }

    return m_revision != revision;
}

// DownloadServerInitiate: in an object carousel, the IOR of the service gateway (ServiceGatewayInfo)
void DTVCarousel::dsi(const uint8_t *m, int length)
{
    Reader r(m, length);
    r.skip(20);                     // serverId
    r.skip(r.u16());                // compatibilityDescriptor
    const int privateDataLength = r.u16();

    if (!r.has(privateDataLength)) {
        return;
    }

    ObjectRef gateway;
    Kind kind;
    int used;
    std::vector<int> tags;

    if (!parseIOR(r.here(), privateDataLength, used, gateway, kind, &tags) || (gateway.m_moduleId < 0)) {
        return;
    }

    if ((gateway.m_carouselId != m_carouselId) || (gateway.m_moduleId != m_gateway.m_moduleId)
        || (gateway.m_objectKey != m_gateway.m_objectKey))
    {
        // A new carousel, or one restructured
        reset();
        m_carouselId = gateway.m_carouselId;
        m_gateway = gateway;
    }

    m_associationTags.insert(tags.begin(), tags.end());
}

// DownloadInfoIndication: the modules of (part of) the carousel, with their sizes and versions
void DTVCarousel::dii(const uint8_t *m, int length, uint32_t transactionId)
{
    Reader r(m, length);
    const uint32_t downloadId = r.u32();

    if ((m_carouselId < 0) || (downloadId != (uint32_t) m_carouselId)) {
        return;
    }

    const int blockSize = r.u16();
    r.skip(1 + 1 + 4 + 4);          // windowSize, ackPeriod, tCDownloadWindow, tCDownloadScenario
    r.skip(r.u16());                // compatibilityDescriptor
    const int numberOfModules = r.u16();

    if (!r.m_ok || (blockSize == 0)) {
        return;
    }

    std::vector<int> listed;

    for (int i = 0; (i < numberOfModules) && r.m_ok; i++)
    {
        const int moduleId = r.u16();
        const int64_t moduleSize = r.u32();
        const int moduleVersion = r.u8();
        const int moduleInfoLength = r.u8();
        const uint8_t *info = r.bytes(moduleInfoLength);

        if (!r.m_ok) {
            break;
        }

        // BIOP::ModuleInfo: timeouts, taps (the PIDs the module's blocks are on), and descriptors
        Reader ri(info, moduleInfoLength);
        ri.skip(4 + 4 + 4);         // moduleTimeOut, blockTimeOut, minBlockTime
        const int tapsCount = ri.u8();

        for (int t = 0; (t < tapsCount) && ri.m_ok; t++)
        {
            ri.skip(2 + 2);         // id, use
            const int associationTag = ri.u16();
            ri.skip(ri.u8());       // selector

            if (ri.m_ok) {
                m_associationTags.insert(associationTag);
            }
        }

        int originalSize = -1;
        const int userInfoLength = ri.u8();
        const uint8_t *userInfo = ri.bytes(userInfoLength);

        for (int d = 0; userInfo && (d + 2 <= userInfoLength); d += 2 + userInfo[d + 1])
        {
            // compressed_module_descriptor: compression_method (zlib), original_size
            if ((userInfo[d] == m_compressedModuleDescriptor) && (userInfo[d + 1] >= 5) && (d + 7 <= userInfoLength)) {
                originalSize = (int) std::min<uint32_t>(0x7FFFFFFF, ((uint32_t) userInfo[d + 3] << 24) | (userInfo[d + 4] << 16) | (userInfo[d + 5] << 8) | userInfo[d + 6]);
            }
        }

        if ((moduleSize > m_maxModuleSize) || (originalSize > m_maxModuleSize)) {
            continue;
        }

        listed.push_back(moduleId);
        auto it = m_modules.find(moduleId);

        if ((it != m_modules.end()) && ((it->second.m_version != moduleVersion) || (it->second.m_size != moduleSize)
            || (it->second.m_blockSize != blockSize) || (it->second.m_originalSize != originalSize)))
        {
            dropModule(moduleId);
            it = m_modules.end();
        }

        if (it == m_modules.end())
        {
            Module module;
            module.m_size = (int) moduleSize;
            module.m_version = moduleVersion;
            module.m_blockSize = blockSize;
            module.m_originalSize = originalSize;
            module.m_blocks.assign((moduleSize + blockSize - 1) / blockSize, false);
            Module& added = m_modules[moduleId] = module;

            if (moduleSize == 0) {
                completeModule(moduleId, added);
            }
        }
    }

    // The modules no DII lists any more are gone. A DII's identified by its transactionId's identification (bits 1
    // to 15): the version (bits 16 to 29) and update flag (bit 0) change when it's updated
    m_diiModules[(transactionId >> 1) & 0x7FFF] = listed;
    std::set<int> current;

    for (const auto& dii : m_diiModules) {
        current.insert(dii.second.begin(), dii.second.end());
    }

    std::vector<int> gone;
    qint64 total = 0;

    for (const auto& module : m_modules)
    {
        if (current.count(module.first) == 0) {
            gone.push_back(module.first);
        } else {
            total += module.second.m_size;
        }
    }

    for (int moduleId : gone)
    {
        dropModule(moduleId);
        m_modules.erase(moduleId);
    }

    // A carousel far larger than any real one: its data isn't believed
    if (total > m_maxCarouselSize)
    {
        const int64_t carouselId = m_carouselId;
        const ObjectRef gateway = m_gateway;
        reset();
        m_carouselId = carouselId;
        m_gateway = gateway;
    }
}

// DownloadDataBlock: a block of a module
bool DTVCarousel::ddb(const uint8_t *m, int length)
{
    Reader r(m, length);
    const int moduleId = r.u16();
    const int moduleVersion = r.u8();
    r.skip(1);
    const int blockNumber = r.u16();

    if (!r.m_ok) {
        return false;
    }

    auto it = m_modules.find(moduleId);

    if ((it == m_modules.end()) || it->second.m_complete || (it->second.m_version != moduleVersion)
        || (blockNumber >= (int) it->second.m_blocks.size()) || it->second.m_blocks[blockNumber]) {
        return false;
    }

    Module& module = it->second;
    const int offset = blockNumber * module.m_blockSize;
    const int expected = std::min(module.m_blockSize, module.m_size - offset);

    if (r.left() < expected) {
        return false;
    }

    if (module.m_data.empty()) {
        module.m_data.resize(module.m_size);
    }

    std::memcpy(module.m_data.data() + offset, r.here(), expected);
    module.m_blocks[blockNumber] = true;
    module.m_blocksReceived++;

    if (module.m_blocksReceived == (int) module.m_blocks.size()) {
        completeModule(moduleId, module);
    }

    return true;
}

void DTVCarousel::completeModule(int moduleId, Module& module)
{
    if (module.m_originalSize >= 0)
    {
        // zlib (RFC 1950), as qUncompress takes it after the size it expects, big-endian
        QByteArray compressed;
        compressed.reserve(4 + module.m_size);
        compressed.append((char) (module.m_originalSize >> 24));
        compressed.append((char) (module.m_originalSize >> 16));
        compressed.append((char) (module.m_originalSize >> 8));
        compressed.append((char) module.m_originalSize);
        compressed.append((const char *) module.m_data.data(), module.m_size);
        const QByteArray data = qUncompress(compressed);

        if (data.size() != module.m_originalSize)
        {
            // Corrupt: received again
            module.m_blocks.assign(module.m_blocks.size(), false);
            module.m_blocksReceived = 0;
            return;
        }

        parseModule(moduleId, (const uint8_t *) data.constData(), data.size());
    }
    else
    {
        parseModule(moduleId, module.m_data.data(), module.m_size);
    }

    module.m_complete = true;
    std::vector<uint8_t>().swap(module.m_data);     // What's needed of it is in the objects
    m_revision++;
}

// The BIOP messages of a module: each a file, directory, service gateway or stream (TR 101 202 4.7.3)
void DTVCarousel::parseModule(int moduleId, const uint8_t *data, int length)
{
    int offset = 0;

    while (offset + 12 <= length)
    {
        // magic "BIOP", biop_version 1.0, byte_order 0 (big-endian), message_type 0, message_size
        if ((std::memcmp(data + offset, "BIOP", 4) != 0) || (data[offset + 6] != 0)) {
            return;
        }

        const int64_t messageSize = ((uint32_t) data[offset + 8] << 24) | (data[offset + 9] << 16) | (data[offset + 10] << 8) | data[offset + 11];

        if (offset + 12 + messageSize > length) {
            return;
        }

        Reader r(data + offset + 12, (int) messageSize);
        offset += 12 + (int) messageSize;

        ObjectRef ref;
        ref.m_carouselId = (uint32_t) m_carouselId;
        ref.m_moduleId = moduleId;
        const int keyLength = r.u8();
        const uint8_t *key = r.bytes(keyLength);
        const int kindLength = r.u32();
        const uint8_t *kindId = r.bytes(kindLength);
        r.skip(r.u16());            // objectInfo
        const int contexts = r.u8();

        for (int i = 0; (i < contexts) && r.m_ok; i++)
        {
            r.skip(4);              // context_id
            r.skip(r.u16());        // context_data
        }

        const int bodyLength = r.u32();
        const uint8_t *body = r.bytes(bodyLength);

        if (!r.m_ok) {
            continue;
        }

        ref.m_objectKey = QByteArray((const char *) key, keyLength);
        Object object;
        object.m_kind = kindFromId(kindId, kindLength);
        Reader b(body, bodyLength);

        if (object.m_kind == Kind::File)
        {
            const int contentLength = b.u32();
            const uint8_t *content = b.bytes(contentLength);

            if (!b.m_ok) {
                continue;
            }

            object.m_content = QByteArray((const char *) content, contentLength);
        }
        else if ((object.m_kind == Kind::Directory) || (object.m_kind == Kind::Gateway))
        {
            const int bindings = b.u16();

            for (int i = 0; (i < bindings) && b.m_ok; i++)
            {
                // BIOP::Name: its components (one, in DVB): the id (the name) and kind
                Binding binding;
                const int components = b.u8();

                for (int c = 0; (c < components) && b.m_ok; c++)
                {
                    const int idLength = b.u8();
                    const uint8_t *id = b.bytes(idLength);
                    b.skip(b.u8());     // kind

                    if (id)
                    {
                        int n = idLength;

                        while ((n > 0) && (id[n - 1] == 0)) {
                            n--;
                        }

                        binding.m_name = QString::fromUtf8((const char *) id, n);
                    }
                }

                b.skip(1);              // bindingType
                int used = 0;

                if (!b.m_ok || !parseIOR(b.here(), b.left(), used, binding.m_object, binding.m_kind, nullptr)) {
                    break;
                }

                b.skip(used);
                const int objectInfoLength = b.u16();
                const uint8_t *objectInfo = b.bytes(objectInfoLength);
                binding.m_size = -1;

                // A file's objectInfo starts with its size (DSM::File::ContentSize, 64 bits)
                if (objectInfo && (binding.m_kind == Kind::File) && (objectInfoLength >= 8))
                {
                    Reader ri(objectInfo, objectInfoLength);
                    binding.m_size = (qint64) ri.u64();
                }

                // Names are of one component, without separators
                if (b.m_ok && !binding.m_name.isEmpty() && !binding.m_name.contains('/') && !binding.m_name.contains('\\')
                    && (binding.m_name != ".") && (binding.m_name != "..")) {
                    object.m_bindings.push_back(binding);
                }
            }
        }

        m_objects[ref] = std::move(object);
    }
}

void DTVCarousel::dropModule(int moduleId)
{
    bool dropped = false;

    for (auto it = m_objects.begin(); it != m_objects.end();)
    {
        if (it->first.m_moduleId == moduleId)
        {
            it = m_objects.erase(it);
            dropped = true;
        }
        else
        {
            ++it;
        }
    }

    auto module = m_modules.find(moduleId);

    if ((module != m_modules.end()) && module->second.m_complete) {
        dropped = true;
    }

    if (dropped) {
        m_revision++;
    }
}

// An IOP::IOR: the object's kind (its type_id), and from the BIOP profile its location (carousel, module and object
// key) and the taps of its ConnBinder (the PIDs to look for it on, by association tag)
bool DTVCarousel::parseIOR(const uint8_t *data, int length, int& used, ObjectRef& object, Kind& kind, std::vector<int> *tags)
{
    Reader r(data, length);
    const int typeIdLength = r.u32();
    const uint8_t *typeId = r.bytes(typeIdLength);
    kind = typeId ? kindFromId(typeId, typeIdLength) : Kind::Unknown;
    r.skip((4 - (typeIdLength & 3)) & 3);   // Aligned to 4 bytes
    const int profiles = r.u32();

    for (int p = 0; (p < profiles) && r.m_ok; p++)
    {
        const uint32_t profileTag = r.u32();
        const int profileLength = r.u32();
        const uint8_t *profile = r.bytes(profileLength);

        if (!profile || (profileTag != m_tagBIOP)) {
            continue;
        }

        Reader pr(profile, profileLength);
        pr.skip(1);                 // profile_data_byte_order
        const int components = pr.u8();

        for (int c = 0; (c < components) && pr.m_ok; c++)
        {
            const uint32_t componentTag = pr.u32();
            const int componentLength = pr.u8();
            const uint8_t *component = pr.bytes(componentLength);

            if (!component) {
                break;
            }

            Reader cr(component, componentLength);

            if (componentTag == m_tagObjectLocation)
            {
                object.m_carouselId = cr.u32();
                const int moduleId = cr.u16();
                cr.skip(2);         // version
                const int keyLength = cr.u8();
                const uint8_t *key = cr.bytes(keyLength);

                if (cr.m_ok)
                {
                    object.m_moduleId = moduleId;
                    object.m_objectKey = QByteArray((const char *) key, keyLength);
                }
            }
            else if ((componentTag == m_tagConnBinder) && tags)
            {
                const int taps = cr.u8();

                for (int t = 0; (t < taps) && cr.m_ok; t++)
                {
                    cr.skip(2 + 2);     // id, use
                    const int associationTag = cr.u16();
                    cr.skip(cr.u8());   // selector

                    if (cr.m_ok) {
                        tags->push_back(associationTag);
                    }
                }
            }
        }
    }

    used = r.m_offset;
    return r.m_ok;
}

DTVCarousel::Progress DTVCarousel::getProgress() const
{
    Progress progress;

    for (const auto& module : m_modules)
    {
        progress.m_modules++;
        progress.m_bytes += module.second.m_size;

        if (module.second.m_complete)
        {
            progress.m_modulesComplete++;
            progress.m_bytesComplete += module.second.m_size;
        }
        else if (module.second.m_blockSize > 0)
        {
            progress.m_bytesComplete += std::min<qint64>(module.second.m_size, (qint64) module.second.m_blocksReceived * module.second.m_blockSize);
        }
    }

    return progress;
}

bool DTVCarousel::isComplete() const
{
    if (m_modules.empty()) {
        return false;
    }

    for (const auto& module : m_modules)
    {
        if (!module.second.m_complete) {
            return false;
        }
    }

    return true;
}

void DTVCarousel::walk(const ObjectRef& directory, const QString& path, int depth, std::set<ObjectRef>& visited,
                       std::vector<Entry> *entries, std::map<QString, QByteArray> *files, const QString *wanted) const
{
    auto it = m_objects.find(directory);

    if ((it == m_objects.end()) || (depth > m_maxDepth) || !visited.insert(directory).second) {
        return;
    }

    for (const Binding& binding : it->second.m_bindings)
    {
        const QString childPath = path + "/" + binding.m_name;
        auto child = m_objects.find(binding.m_object);
        const bool received = child != m_objects.end();

        // Only objects of this carousel are here: others (in other carousels) can't be received from it
        if (binding.m_object.m_carouselId != (uint32_t) m_carouselId) {
            continue;
        }

        if (binding.m_kind == Kind::File)
        {
            if (entries) {
                entries->push_back(Entry{childPath, Kind::File, received ? (qint64) child->second.m_content.size() : binding.m_size, received});
            }

            if (received && files && (!wanted || (*wanted == childPath))) {
                (*files)[childPath] = child->second.m_content;
            }
        }
        else
        {
            if (entries) {
                entries->push_back(Entry{childPath, binding.m_kind, -1, received});
            }

            if ((binding.m_kind == Kind::Directory) && received
                && (!wanted || wanted->startsWith(childPath + "/"))) {
                walk(binding.m_object, childPath, depth + 1, visited, entries, files, wanted);
            }
        }
    }
}

std::vector<DTVCarousel::Entry> DTVCarousel::getEntries() const
{
    std::vector<Entry> entries;
    std::set<ObjectRef> visited;
    walk(m_gateway, "", 0, visited, &entries, nullptr, nullptr);
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.m_path < b.m_path; });
    return entries;
}

bool DTVCarousel::getFile(const QString& path, QByteArray& data) const
{
    // Only the directories on the way to it are walked
    const QString wanted = path.startsWith('/') ? path : "/" + path;
    std::map<QString, QByteArray> files;
    std::set<ObjectRef> visited;
    walk(m_gateway, "", 0, visited, nullptr, &files, &wanted);
    auto it = files.find(wanted);

    if (it == files.end()) {
        return false;
    }

    data = it->second;
    return true;
}

std::map<QString, QByteArray> DTVCarousel::getFiles() const
{
    std::map<QString, QByteArray> files;
    std::set<ObjectRef> visited;
    walk(m_gateway, "", 0, visited, nullptr, &files, nullptr);
    return files;
}
