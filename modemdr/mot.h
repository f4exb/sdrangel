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

#ifndef MODEMDR_MOT_H_
#define MODEMDR_MOT_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "export.h"

// The Multimedia Object Transfer protocol (ETSI EN 301 234), as DAB and DRM carry it in MSC data
// groups (EN 300 401 clause 5.3.3): header mode for the SlideShow, directory mode for the
// Broadcast Website and SPI.

namespace modemdr
{

// A complete MOT object in header mode, and its header parameters
struct MODEMDR_API MOTObject
{
    int transportId = -1;
    int contentType = -1;       // 2 image (TS 101 756 table 17)
    int contentSubtype = -1;    // image: 0 GIF, 1 JFIF (JPEG), 2 BMP, 3 PNG
    std::string name;           // ContentName, as UTF-8
    std::string mimeType;
    // SlideShow (TS 101 499 clause 6.2)
    bool triggerNow = true;     // TriggerTime absent or "now"
    int categoryId = -1;
    int slideId = -1;
    std::string categoryTitle;
    std::string clickThroughURL;
    std::string alternativeLocationURL;
    bool alert = false;
    std::vector<std::uint8_t> body;
};

// MOT in header mode, as the SlideShow uses: data groups of type 3 carry an object's header and
// of type 4 its body, each segmented and identified by transport Id.
class MODEMDR_API MOTDecoder
{
public:
    void reset();
    // One MSC data group (with its header and CRC); appends the objects it completes.
    void push(const std::vector<std::uint8_t>& dataGroup, std::vector<MOTObject>& completed);
    int invalidDataGroups() const { return m_invalidDataGroups; }

private:
    struct Segments
    {
        std::map<int, std::vector<std::uint8_t>> parts;
        int last = -1;      // Number of the last segment, once seen
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

    void complete(int transportId, std::vector<MOTObject>& completed);

    std::map<int, Pending> m_pending;
    // The object delivered last (transport Id and body size): its repetitions are dropped, but a
    // carousel cycling its slides shows each again.
    std::deque<std::pair<int, std::size_t>> m_delivered;
    std::uint64_t m_clock = 0;
    int m_invalidDataGroups = 0;
};

// A file of an MOT directory (a carousel). The data is shared, so that copies are cheap.
struct MODEMDR_API MOTFile
{
    int transportId = -1;
    int contentType = -1;
    int contentSubtype = -1;
    std::string name;           // ContentName, as UTF-8, without a leading "/"
    std::string mimeType;
    // The user application's own header parameters (ids 0x25 to 0x3f), whose meaning depends
    // on the application
    std::map<int, std::vector<std::uint8_t>> parameters;
    std::shared_ptr<const std::vector<std::uint8_t>> data;
};

// The files of an MOT directory received so far, as a Broadcast Website (ETSI TS 101 498) sees them
struct MODEMDR_API MOTWebsite
{
    std::map<std::string, MOTFile> files;   // By ContentName
    std::set<std::string> listed;   // The names the current directory lists, received or not
    std::string startPage;          // The root's DirectoryIndex, else index.html or the first page
    std::string indexName;          // The DirectoryIndex, the file a directory URL gives (TS 101 498-1 A.1.1)
    int directoryObjects = 0;       // Objects the current directory describes
    // Changes when the decoder starts again (another service, component or station): a viewer
    // of the website shown before starts again too
    std::uint64_t generation = 0;
};

// MOT in directory mode (EN 301 234 clause 7.2), as the Broadcast Website and SPI use: an MOT
// directory (data group type 6, or 7 compressed) holds every object's header, and bodies (type 4)
// are matched to it by transport Id. A new directory replaces the old one: files it no longer
// lists are removed.
class MODEMDR_API MOTDirectoryDecoder
{
public:
    void reset();
    // The DirectoryIndex profile to prefer (TS 101 498-1 clause 5.5.1).
    void setProfiles(const std::vector<std::uint8_t>& profiles) { m_profiles = profiles; }
    // One MSC data group; true when a file arrived or the directory changed.
    bool push(const std::vector<std::uint8_t>& dataGroup);
    const MOTWebsite& website() const { return m_website; }
    // The files received, by name
    const std::map<std::string, MOTFile>& files() const { return m_website.files; }
    int invalidDataGroups() const { return m_invalidDataGroups; }

    // The memory limits: broadcast data is untrusted
    static constexpr std::size_t MaximumObjects = 4096U;
    static constexpr std::size_t MaximumPendingEntities = 1024U;
    static constexpr std::size_t SegmentOverhead = 64U; // Counted for each segment held
    static constexpr std::size_t MaximumObjectBytes = 4194304U;
    static constexpr std::size_t MaximumPendingBytes = 8388608U;
    static constexpr std::size_t MaximumWebsiteBytes = 16777216U;

private:
    struct Entry
    {
        MOTFile file;               // Without its data
        int compression = 0;        // CompressionType, 0 for none
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

    struct Unlisted
    {
        std::vector<std::uint8_t> data;
        std::uint64_t lastUse = 0;
    };

    bool applyDirectory(int transportId, const std::vector<std::uint8_t>& directory);
    bool addFile(int transportId, const std::vector<std::uint8_t>& body);
    void chooseStartPage();
    void release(const Segments& segments);
    void dropUnlisted();

    std::vector<std::uint8_t> m_profiles;
    std::map<int, Segments> m_directorySegments;    // By data group type and transport Id
    int m_directoryTransportId = -1;
    std::map<int, Entry> m_entries;                 // The directory, by transport Id
    std::vector<std::pair<int, std::string>> m_directoryIndex; // Profile, name
    std::map<int, Segments> m_bodies;               // Bodies being received
    std::map<int, Unlisted> m_unlisted;             // Complete bodies the directory does not list (yet)
    std::size_t m_pendingBytes = 0;                 // Held segments and bodies, with their overhead
    std::map<std::string, int> m_fileTransportIds;
    std::set<int> m_failed;                         // Bodies that could not be decoded
    MOTWebsite m_website;
    std::size_t m_websiteBytes = 0;
    std::uint64_t m_generation = 0;                 // The website's, counted up at each reset
    std::uint64_t m_clock = 0;
    int m_invalidDataGroups = 0;
};

} // namespace modemdr

#endif // MODEMDR_MOT_H_
