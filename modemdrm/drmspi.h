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

#ifndef MODEMDRM_DRMSPI_H_
#define MODEMDRM_DRMSPI_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "export.h"
#include "modemdr/spi.h"
#include "drmdata.h"
#include "drmmsc.h"
#include "drmsdc.h"

namespace modemdrm
{

// The Service and Programme Information (programme guide) of a multiplex:
// every data component with user application 0x007 (DAB domain, packet mode
// with data units), decoded from the multiplex frames whatever service is
// selected. The SPI decoder is shared with DAB; bearers are "drm:" ones.
class MODEMDRM_API DRMSPIDecoder
{
public:
    struct Component
    {
        int streamId = -1;
        int packetId = -1;
        int packetLengthBytes = 0;
        std::set<int> shortIds;  // the services that signal it
        DRMPacketDemultiplexer packets;
        modemdr::SPIDecoder decoder{modemdr::SPIDomain::DRM};
    };

    void reset();
    // A multiplex frame was lost: partial data units go, files stay.
    void frameLost();
    // One decoded multiplex frame. Returns true when the information changed.
    bool push(const SDCData& sdc, const std::vector<MSCLogicalStreamFrame>& streams);
    // Everything decoded from all the components
    std::shared_ptr<const modemdr::SPIData> data() const { return m_data; }
    // Components by stream and packet Id.
    const std::map<std::pair<int, int>, Component>& components() const { return m_components; }

private:
    void rebuild();

    std::map<std::pair<int, int>, Component> m_components;
    std::shared_ptr<const modemdr::SPIData> m_data = std::make_shared<modemdr::SPIData>();
};

} // namespace modemdrm

#endif // MODEMDRM_DRMSPI_H_
