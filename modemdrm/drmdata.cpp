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

#include "drmbitutils.h"

namespace modemdrm
{

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
