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

#ifndef MODEMDR_SPI_H_
#define MODEMDR_SPI_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "export.h"
#include "mot.h"

// Service and Programme Information (SPI, formerly the EPG): station names, descriptions and
// logos (SI), and programme schedules (PI). ETSI TS 102 818 gives the XML, and TS 102 371 its
// binary encoding and transport: MOT directory mode, in a packet mode data service with user
// application type 0x007 (in DRM, a data application in the DAB domain).

namespace modemdr
{

// The system a bearer is on: "dab:" or "drm:" (TS 102 371 clause 5.4.5.1)
enum class SPIDomain
{
    DAB,
    DRM
};

// A service, as SPI identifies it. DRM services have only an SId (24 bits).
struct MODEMDR_API SPIBearer
{
    int ecc = -1;
    int eid = -1;
    uint32_t sid = 0;
    int scids = 0;

    bool operator==(const SPIBearer& other) const
    {
        return (ecc == other.ecc) && (eid == other.eid) && (sid == other.sid) && (scids == other.scids);
    }
};

struct MODEMDR_API SPILogo
{
    enum Type
    {
        Unrestricted = 2,       // Size from width and height
        ColourSquare = 4,       // 32x32 PNG
        ColourRectangle = 6     // 112x32 PNG
    };

    std::string url;            // The MOT ContentName of the image
    std::string mimeType;
    int type = Unrestricted;
    int width = 0;
    int height = 0;
};

// Names and descriptions, as several elements give them
struct MODEMDR_API SPIText
{
    std::string shortName;      // Up to 8 characters
    std::string mediumName;     // Up to 16
    std::string longName;       // Up to 128
    std::string shortDescription;
    std::string longDescription;
    std::vector<SPILogo> logos;
    std::vector<std::string> genres;    // TV-Anytime classification URNs

    // The medium name, else the long one (also when the medium name is truncated), else the short
    std::string name() const;
    // The longest description given
    std::string description() const;
};

struct MODEMDR_API SPIService
{
    SPIText text;
    std::vector<SPIBearer> bearers;
};

struct MODEMDR_API SPIProgramme
{
    SPIText text;
    int64_t start = 0;          // UTC, seconds since 1970
    int duration = 0;           // Seconds
    bool onAir = true;          // False for placeholders
    bool recommended = false;
};

// Everything decoded so far. Images are the MOT files they are in.
struct MODEMDR_API SPIData
{
    std::vector<SPIService> services;
    std::map<uint32_t, std::vector<SPIProgramme>> schedules;     // By SId, in start order
    std::map<std::string, MOTFile> images;                       // By ContentName
    std::map<std::string, MOTFile> objects;                      // The SI and PI objects, as received

    const SPIService *service(uint32_t sid) const;
    // A service's logo: the one nearest the given width, of those received
    const MOTFile *logo(uint32_t sid, int width) const;
    // The programme on at a time (UTC seconds), if any
    const SPIProgramme *programmeAt(uint32_t sid, int64_t time) const;
};

// The binary encoding (TS 102 371 clause 5), decoded to a tree
struct MODEMDR_API SPIElement
{
    int tag = 0;
    std::map<int, std::vector<uint8_t>> attributes;    // By tag, raw
    std::map<int, std::string> strings;                // String attributes, tokens expanded
    std::string cdata;
    std::vector<SPIElement> children;

    // Parse one top-level element (epg or serviceInformation). False if it is malformed.
    static bool parse(const uint8_t *data, std::size_t size, SPIElement& element);
    const SPIElement *child(int tag) const;
};

// Decodes the SPI of a packet mode data component: its MSC data groups
class MODEMDR_API SPIDecoder
{
public:
    explicit SPIDecoder(SPIDomain domain = SPIDomain::DAB) : m_domain(domain) {}
    void reset();
    // One MSC data group; true when the decoded information changed.
    bool push(const std::vector<uint8_t>& dataGroup);
    std::shared_ptr<const SPIData> data() const { return m_data; }
    // The MOT files received, by name
    const std::map<std::string, MOTFile>& files() const { return m_mot.files(); }
    // The information in a set of MOT files: SI and PI objects and images
    static std::shared_ptr<SPIData> decode(const std::map<std::string, MOTFile>& files, int *invalidObjects = nullptr,
        SPIDomain domain = SPIDomain::DAB);
    int invalidObjects() const { return m_invalidObjects; }

    // Decode one SI or PI object (MOT content type 7, subtypes 0 and 1) into data
    static bool decodeSI(const uint8_t *object, std::size_t size, SPIData& data, SPIDomain domain = SPIDomain::DAB);
    static bool decodePI(const uint8_t *object, std::size_t size, const SPIBearer *scope, SPIData& data,
        SPIDomain domain = SPIDomain::DAB);
    // TS 102 371 clause 5.4.5: a bearer's binary id, a time point and a genre
    static bool decodeBearer(const std::vector<uint8_t>& id, SPIBearer& bearer, SPIDomain domain = SPIDomain::DAB);
    static bool decodeTime(const std::vector<uint8_t>& time, int64_t& utc);
    static std::string decodeGenre(const std::vector<uint8_t>& href);

    static constexpr std::size_t MaximumProgrammes = 2048;    // Per service

private:
    void rebuild();

    SPIDomain m_domain;
    MOTDirectoryDecoder m_mot;
    std::shared_ptr<const SPIData> m_data = std::make_shared<SPIData>();
    int m_invalidObjects = 0;
};

} // namespace modemdr

#endif // MODEMDR_SPI_H_
