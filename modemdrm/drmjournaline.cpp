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

#include "drmjournaline.h"

#include <algorithm>
#include <array>

#include "drmbitutils.h"

namespace modemdrm
{

void DRMJournalineDecoder::reset()
{
    m_components.clear();
}

void DRMJournalineDecoder::frameLost()
{
    for (auto& component : m_components) {
        component.second.packets.frameLost();
    }
}

bool DRMJournalineDecoder::reconfigure()
{
    bool changed = false;

    for (auto& component : m_components)
    {
        if (!component.second.shortIds.empty())
        {
            component.second.shortIds.clear();
            component.second.revision = ++m_revision;
            changed = true;
        }
    }

    return changed;
}

bool DRMJournalineDecoder::push(const SDCData& sdc, const std::vector<MSCLogicalStreamFrame>& streams)
{
    // The components the SDC currently signals, and their services.
    std::map<std::pair<int, int>, std::pair<int, std::set<int>>> signalled;

    for (const SDCDataApplication& application : sdc.dataApplications)
    {
        if (application.userApplicationType == SDCDataApplication::Journaline && application.packetMode
            && application.dataUnits && application.applicationDomain == 1)
        {
            auto& entry = signalled[{application.streamId, application.packetId}];
            entry.first = application.packetLengthBytes;
            entry.second.insert(application.shortId);
        }
    }

    // Components are kept while the SDC is rebuilt (after a lost lock its
    // entities return over several blocks): reset() forgets them with the
    // station. One whose packet length changed starts again.
    bool changed = false;

    for (const auto& entry : signalled)
    {
        auto existing = m_components.find(entry.first);

        if ((existing != m_components.end()) && (existing->second.packetLengthBytes != entry.second.first))
        {
            changed = changed || !existing->second.decoder.objects().empty();
            m_components.erase(existing);
        }

        Component& component = m_components[entry.first];
        component.streamId = entry.first.first;
        component.packetId = entry.first.second;
        component.packetLengthBytes = entry.second.first;

        if (!std::includes(component.shortIds.begin(), component.shortIds.end(), entry.second.second.begin(), entry.second.second.end()))
        {
            component.shortIds.insert(entry.second.second.begin(), entry.second.second.end());
            component.revision = ++m_revision;
            changed = true;
        }

        const auto stream = std::find_if(streams.begin(), streams.end(),
            [&component](const MSCLogicalStreamFrame& frame) { return frame.streamId == component.streamId; });

        if (stream == streams.end()) {
            continue;
        }

        std::vector<std::uint8_t> bytes = stream->partA;
        bytes.insert(bytes.end(), stream->partB.begin(), stream->partB.end());
        std::vector<std::vector<std::uint8_t>> dataUnits;
        component.packets.push(bytes, component.packetLengthBytes, component.packetId, true, dataUnits);

        for (const auto& dataUnit : dataUnits)
        {
            if (component.decoder.push(dataUnit))
            {
                component.revision = ++m_revision;
                changed = true;
            }
        }
    }

    return changed;
}

} // namespace modemdrm
