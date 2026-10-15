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

#include "drmspi.h"

#include <algorithm>

namespace modemdrm
{

void DRMSPIDecoder::reset()
{
    m_components.clear();
    m_data = std::make_shared<modemdr::SPIData>();
}

void DRMSPIDecoder::frameLost()
{
    for (auto& component : m_components) {
        component.second.packets.frameLost();
    }
}

bool DRMSPIDecoder::push(const SDCData& sdc, const std::vector<MSCLogicalStreamFrame>& streams)
{
    // The components the SDC signals, with the services that signal them
    std::map<std::pair<int, int>, std::pair<int, std::set<int>>> signalled;

    for (const SDCDataApplication& application : sdc.dataApplications)
    {
        if (application.userApplicationType == SDCDataApplication::SPI && application.packetMode
            && application.dataUnits && application.applicationDomain == 1)
        {
            auto& entry = signalled[{application.streamId, application.packetId}];
            entry.first = application.packetLengthBytes;
            entry.second.insert(application.shortId);
        }
    }

    // The guide is kept until another multiplex: a component no longer
    // signalled keeps its files
    bool changed = false;

    for (const auto& entry : signalled)
    {
        auto existing = m_components.find(entry.first);

        if ((existing != m_components.end()) && (existing->second.packetLengthBytes != entry.second.first))
        {
            changed = changed || !existing->second.decoder.files().empty();
            m_components.erase(existing);
        }

        Component& component = m_components[entry.first];
        component.streamId = entry.first.first;
        component.packetId = entry.first.second;
        component.packetLengthBytes = entry.second.first;
        component.shortIds.insert(entry.second.second.begin(), entry.second.second.end());

        const auto stream = std::find_if(streams.begin(), streams.end(),
            [&component](const MSCLogicalStreamFrame& frame) { return frame.streamId == component.streamId; });

        if (stream == streams.end()) {
            continue;
        }

        std::vector<std::uint8_t> bytes = stream->partA;
        bytes.insert(bytes.end(), stream->partB.begin(), stream->partB.end());
        std::vector<std::vector<std::uint8_t>> dataGroups;
        component.packets.push(bytes, component.packetLengthBytes, component.packetId, true, dataGroups);

        for (const auto& dataGroup : dataGroups) {
            changed = component.decoder.push(dataGroup) || changed;
        }
    }

    if (changed) {
        rebuild();
    }

    return changed;
}

// The files of every component together
void DRMSPIDecoder::rebuild()
{
    if (m_components.size() == 1)
    {
        m_data = m_components.begin()->second.decoder.data();
        return;
    }

    std::map<std::string, modemdr::MOTFile> files;

    for (const auto& component : m_components)
    {
        for (const auto& file : component.second.decoder.files()) {
            files[file.first] = file.second;
        }
    }

    m_data = modemdr::SPIDecoder::decode(files, nullptr, modemdr::SPIDomain::DRM);
}

} // namespace modemdrm
