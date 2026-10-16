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
#include "modemdr/mot.h"
#include "drmsdc.h"

namespace modemdrm
{

// MOT is shared with DAB
using modemdr::MOTObject;
using modemdr::MOTDecoder;
using modemdr::MOTFile;
using modemdr::MOTWebsite;
using modemdr::MOTDirectoryDecoder;

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
