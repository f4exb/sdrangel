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

#ifndef INCLUDE_DSEMODPACKETBUILDER_H
#define INCLUDE_DSEMODPACKETBUILDER_H

#include <vector>

#include "dsemodtxsession.h"

// Gateway packets contain channel-coded payloads, not PCM. Keep them intact
// except for RF framing (preamble, CACH, BS sync and configured colour code).
class DSEModPacketBuilder
{
public:
    void reset(const DSEModSettings& settings);
    bool append(const DSEModTxSession::Frame& frame, std::vector<int8_t>& levels, QString& error);
    void finish(std::vector<int8_t>& levels);
    static bool validate(const DSEModTxSession::Frame& frame, QString& error);
private:
    DSEModSettings m_settings;
    bool m_started = false, m_ended = false;
    int m_cach = 0;
    QByteArray m_header;
    void dmr(const QByteArray& burst, const QString& type, std::vector<int8_t>& levels);
};

#endif
