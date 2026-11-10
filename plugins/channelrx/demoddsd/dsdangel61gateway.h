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

#ifndef INCLUDE_DSDANGEL61GATEWAY_H
#define INCLUDE_DSDANGEL61GATEWAY_H

#include <functional>
#include <vector>
#include <cstdint>

#include "dsp/dv/dvframe.h"

class DSDAngel61Session;

/**
 * What a DSD Demodulator gives a gateway of its Angel61 sessions (plan C7): each call as DVFrameEvents
 * for the "dvframes" pipe - a Start once the call's routing is known (and its path, or the preamble's
 * second frame), its frame records ("angel61-frame"), a data group's whole messages
 * ("angel61-message", DVAngel61::packRelayMessage) and an End after the last-flagged record.
 *
 * Records and messages that come before the routing wait (at most the session's MaxGatewayRecords
 * and MaxGatewayMessages); the messages go ahead of the records that came with them. A call that
 * ends (its last-flagged record) before its routing is known is not offered: what waited is
 * dropped, never published under the next call's Start. One publisher serves all of a
 * demodulator's sessions, as the calls they hold do not overlap.
 */
class DSDAngel61Gateway
{
public:
    typedef std::function<void(const DVFrameEvent&)> Publish;

    //! The session's gateway records and messages since the last call, published through `publish`
    void update(DSDAngel61Session& session, const Publish& publish);
    //! The sessions were reset (protocol, rate): a call offered is ended as lost, what waited dropped
    void reset(const Publish& publish);

private:
    quint64 m_callId = 0;
    quint64 m_sequence = 0;
    std::vector<std::vector<uint8_t>> m_pending;           //!< the call's records, until its routing is known
    std::vector<std::vector<uint8_t>> m_messagesPending;   //!< a data group's messages, before the call's Start
    bool m_started = false;

    void publishRecord(const std::vector<uint8_t>& record, int64_t sample, const Publish& publish);
    static bool isLast(const std::vector<uint8_t>& record) { return (record.size() > 1) && ((record[1] & 0x40) != 0); }
    void publishMessage(const std::vector<uint8_t>& message, int64_t sample, const Publish& publish);
};

#endif // INCLUDE_DSDANGEL61GATEWAY_H
