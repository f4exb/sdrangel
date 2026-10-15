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

#ifndef MODEMDRM_DRMDATA_H_
#define MODEMDRM_DRMDATA_H_

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "export.h"
#include "drmsdc.h"

namespace modemdrm
{

// A complete MOT object (ETSI EN 301 234): the body and what its header says
// about it.
struct MODEMDRM_API MOTObject
{
    int transportId = -1;
    int contentType = -1;    // 2 image (EN 301 234 annex A)
    int contentSubtype = -1; // image: 0 GIF, 1 JFIF, 2 BMP, 3 PNG
    std::string name;        // ContentName, without its character set byte
    std::string mimeType;
    std::vector<std::uint8_t> body;
};

// Packet mode (ES 201 980 clause 6.6.4): fixed-length packets, each a header
// byte, a data field and a CRC, fill the stream's bytes of a multiplex frame.
// Packets of one packet Id with first and last flags delimit a data unit.
class MODEMDRM_API DRMPacketDemultiplexer
{
public:
    void reset();
    // A multiplex frame of the stream was lost: the data units it would have
    // continued cannot complete (those completed are not affected).
    void frameLost();
    // Splits one multiplex frame of the stream into packets and appends the
    // data units of packetId it completes (every packet's data, without data
    // units).
    void push(
        const std::vector<std::uint8_t>& stream,
        int packetLengthBytes,
        int packetId,
        bool dataUnits,
        std::vector<std::vector<std::uint8_t>>& completed
    );
    int validPackets() const { return m_validPackets; }
    int invalidPackets() const { return m_invalidPackets; }

private:
    static constexpr std::size_t MaximumDataUnitBytes = 65536U;

    std::array<int, 4> m_continuity {{-1, -1, -1, -1}};
    std::array<std::vector<std::uint8_t>, 4> m_units;
    std::array<bool, 4> m_unitValid {{false, false, false, false}};
    // Packets that failed their CRC since each packet Id's last good one:
    // from 8 on, its 3-bit continuity index can wrap without showing a gap
    std::array<int, 4> m_lostPackets {{0, 0, 0, 0}};
    int m_validPackets = 0;
    int m_invalidPackets = 0;
};

// MOT in header mode, as the slideshow uses (ETSI TS 101 499): MSC data
// groups (ETSI EN 300 401 clause 5.3.3) of type 3 carry an object's header
// and of type 4 its body, each segmented and identified by transport Id.
class MODEMDRM_API MOTDecoder
{
public:
    void reset();
    // One MSC data group; appends the objects it completes.
    void push(const std::vector<std::uint8_t>& dataGroup, std::vector<MOTObject>& completed);
    int invalidDataGroups() const { return m_invalidDataGroups; }

private:
    struct Segments
    {
        std::map<int, std::vector<std::uint8_t>> parts;
        int last = -1;      // number of the last segment, once seen
        std::size_t bytes = 0;
        bool complete() const;
        std::vector<std::uint8_t> join() const;
    };

    struct Pending
    {
        Segments header;
        Segments body;
        std::uint64_t lastUse = 0;
    };

    static constexpr std::size_t MaximumObjects = 8U;
    static constexpr std::size_t MaximumObjectBytes = 1048576U;
    static constexpr std::size_t DeliveredMemory = 1U;

    static bool parseHeader(const std::vector<std::uint8_t>& header, MOTObject& object);
    void complete(int transportId, std::vector<MOTObject>& completed);

    std::map<int, Pending> m_pending;
    // The object delivered last (transport Id and body size): its repetitions
    // are dropped, but a carousel cycling its slides shows each again.
    std::deque<std::pair<int, std::size_t>> m_delivered;
    std::uint64_t m_clock = 0;
    int m_invalidDataGroups = 0;
};

// A file of a MOT Broadcast Website. The data is shared, so that copies of a
// website are cheap.
struct MODEMDRM_API MOTWebsiteFile
{
    std::string mimeType;
    std::shared_ptr<const std::vector<std::uint8_t>> data;
};

// The files of a MOT Broadcast Website (ETSI TS 101 498) received so far.
struct MODEMDRM_API MOTWebsite
{
    std::map<std::string, MOTWebsiteFile> files; // by ContentName, a path without a leading "/"
    std::set<std::string> listed; // the names the current directory lists, received or not
    std::string startPage;    // the root's DirectoryIndex, else index.html or the first page
    std::string indexName;    // the DirectoryIndex, the file a directory URL gives (TS 101 498-1 A.1.1)
    int directoryObjects = 0; // objects the current directory describes
    // Changes when the decoder starts again (another service, component or
    // station): a viewer of the website shown before starts again too
    std::uint64_t generation = 0;
};

// MOT in directory mode (ETSI EN 301 234 clause 7.2), as the Broadcast
// Website uses: an MOT directory (data group type 6, or 7 compressed) holds
// every object's header, and bodies (type 4) are matched to it by transport
// Id. A new directory replaces the old one: files it no longer lists are
// removed.
class MODEMDRM_API MOTDirectoryDecoder
{
public:
    void reset();
    // The DirectoryIndex profile to prefer (TS 101 498-1 clause 5.5.1).
    void setProfiles(const std::vector<std::uint8_t>& profiles) { m_profiles = profiles; }
    // One MSC data group; true when a file arrived or the directory changed.
    bool push(const std::vector<std::uint8_t>& dataGroup);
    const MOTWebsite& website() const { return m_website; }
    int invalidDataGroups() const { return m_invalidDataGroups; }

    // Body compression (EN 301 234 clause 6.2.2.1.3, TS 101 756 table 18).
    static bool gunzip(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& output,
        std::size_t maximumOutput);

    // The memory limits: broadcast data is untrusted
    static constexpr std::size_t MaximumObjects = 4096U;
    static constexpr std::size_t MaximumPendingEntities = 1024U;
    static constexpr std::size_t SegmentOverhead = 64U; // counted for each segment held
    static constexpr std::size_t MaximumObjectBytes = 4194304U;
    static constexpr std::size_t MaximumPendingBytes = 8388608U;
    static constexpr std::size_t MaximumWebsiteBytes = 16777216U;

private:
    struct Entry
    {
        std::string name;
        std::string mimeType;
        int compression = 0; // CompressionType, 0 for none
        bool scrambled = false;
        std::size_t bodySize = 0;
    };

    struct Segments
    {
        std::map<int, std::vector<std::uint8_t>> parts;
        int last = -1;
        std::size_t bytes = 0;
        std::uint64_t lastUse = 0;
        bool complete() const;
        std::vector<std::uint8_t> join() const;
    };


    bool applyDirectory(int transportId, const std::vector<std::uint8_t>& directory);
    bool addFile(int transportId, const std::vector<std::uint8_t>& body);
    void chooseStartPage();
    void release(const Segments& segments);
    void dropUnlisted();

    std::vector<std::uint8_t> m_profiles;
    std::map<int, Segments> m_directorySegments; // by transport Id
    int m_directoryTransportId = -1;
    std::map<int, Entry> m_entries;              // the directory, by transport Id
    std::vector<std::pair<int, std::string>> m_directoryIndex; // profile, name
    std::map<int, Segments> m_bodies;            // bodies being received
    struct Unlisted
    {
        std::vector<std::uint8_t> data;
        std::uint64_t lastUse = 0;
    };
    std::map<int, Unlisted> m_unlisted;          // complete bodies the directory does not list (yet)
    std::size_t m_pendingBytes = 0;              // held segments and bodies, with their overhead
    std::map<std::string, int> m_fileTransportIds;
    std::set<int> m_failed;                      // bodies that could not be decoded
    MOTWebsite m_website;
    std::size_t m_websiteBytes = 0;
    std::uint64_t m_generation = 0;              // the website's, counted up at each reset
    std::uint64_t m_clock = 0;
    int m_invalidDataGroups = 0;
};

// The MOT slideshow data component of a service (SDC entity 5, user
// application 0x002 in the DAB domain, packet mode with data units).
class MODEMDRM_API DRMSlideshowDecoder
{
public:
    // The service's slideshow component in the SDC, or nullptr.
    static const SDCDataApplication* find(const SDCData& sdc, int shortId);

    void reset();
    // The service decoded for: another one starts again, before its data is
    // decoded. Returns true when that dropped another service's data.
    bool setService(std::int64_t serviceIdentifier);
    // A multiplex frame was lost: partial data units go, objects stay.
    void frameLost() { m_packets.frameLost(); }
    // The component's stream bytes of one multiplex frame; appends the MOT
    // objects completed (images and anything else the carousel sends).
    void push(
        const SDCDataApplication& application,
        const std::vector<std::uint8_t>& stream,
        std::vector<MOTObject>& completed
    );
    int validPackets() const { return m_packets.validPackets(); }
    int invalidPackets() const { return m_packets.invalidPackets(); }
    int invalidDataGroups() const { return m_mot.invalidDataGroups(); }

private:
    DRMPacketDemultiplexer m_packets;
    MOTDecoder m_mot;
    std::int64_t m_service = -1;
    int m_configuredStream = -1;
    int m_configuredPacketId = -1;
    int m_configuredPacketLength = -1;
};

// The MOT Broadcast Website data component of a service (SDC entity 5, user
// application 0x003 in the DAB domain, packet mode with data units).
class MODEMDRM_API DRMWebsiteDecoder
{
public:
    // The service's Broadcast Website component in the SDC, or nullptr.
    static const SDCDataApplication* find(const SDCData& sdc, int shortId);

    void reset();
    // As DRMSlideshowDecoder::setService and frameLost.
    bool setService(std::int64_t serviceIdentifier);
    void frameLost() { m_packets.frameLost(); }
    // The component's stream bytes of one multiplex frame; true when the
    // website changed.
    bool push(const SDCDataApplication& application, const std::vector<std::uint8_t>& stream);
    const MOTWebsite& website() const { return m_mot.website(); }
    int validPackets() const { return m_packets.validPackets(); }
    int invalidPackets() const { return m_packets.invalidPackets(); }
    int invalidDataGroups() const { return m_mot.invalidDataGroups(); }

private:
    DRMPacketDemultiplexer m_packets;
    MOTDirectoryDecoder m_mot;
    std::int64_t m_service = -1;
    int m_configuredStream = -1;
    int m_configuredPacketId = -1;
    int m_configuredPacketLength = -1;
};

} // namespace modemdrm

#endif // MODEMDRM_DRMDATA_H_
