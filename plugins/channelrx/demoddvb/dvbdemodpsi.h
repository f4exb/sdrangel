///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_DVBDEMODPSI_H
#define INCLUDE_DVBDEMODPSI_H

#include <cstdint>
#include <map>
#include <vector>

#include "dvbdemodsections.h"

// What FFmpeg doesn't give of the transport stream's tables: the transport stream id, from the PAT, which changes
// with the multiplex, and the logical channel numbers of services (the numbers viewers select them by), from the
// logical channel descriptors (tag 0x83, as used in the UK, Europe and Australia) in the NIT. Fed with the
// transport stream, in any size pieces
class DVBDemodPSI
{
public:
    DVBDemodPSI();
    void reset();       //!< For a new stream: forgets everything
    void feed(const uint8_t *data, int length);
    //! -1 until the PAT has been received
    int getTransportStreamId() const { return m_transportStreamId; }
    //! Service id to logical channel number, of the transport stream received (none until the PAT says which it is)
    const std::map<int, int>& getChannels() const;

private:
    static constexpr int m_patPid = 0x0000;
    static constexpr int m_nitPid = 0x0010;
    static constexpr int m_patTable = 0x00;
    static constexpr int m_nitActual = 0x40;
    static constexpr int m_logicalChannelDescriptor = 0x83;

    void packet(const uint8_t *packet);
    void section(const uint8_t *data, int length);
    void pat(const uint8_t *data, int length);
    void nit(const uint8_t *data, int length);

    std::vector<uint8_t> m_partial;     //!< Of a TS packet split between feeds
    DVBDemodSectionAssembler m_pat;
    DVBDemodSectionAssembler m_nit;
    int m_transportStreamId;
    std::map<int, std::map<int, int>> m_channels;   //!< By transport stream id (the NIT's): service id to number
};

#endif // INCLUDE_DVBDEMODPSI_H
