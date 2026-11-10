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

#include <algorithm>

#include "dsdangel61gateway.h"
#include "dsdangel61session.h"
#include "dsp/dv/dvangel61.h"

void DSDAngel61Gateway::update(DSDAngel61Session& session, const Publish& publish)
{
    // A call is the records from one to the next last-flagged, offered once its routing is known
    // (and its path, or the preamble's second frame). A data group's messages (no record carries
    // them) go ahead of the records that came with them, so before the call's end
    for (const std::vector<uint8_t>& message : session.takeGatewayMessages())
    {
        if (m_started) {
            publishMessage(message, session.getSample(), publish);
        } else if (m_messagesPending.size() < (size_t) DSDAngel61Session::MaxGatewayMessages) {
            m_messagesPending.push_back(message);
        }
    }

    for (const std::vector<uint8_t>& record : session.takeGatewayRecords())
    {
        if (m_started)
        {
            publishRecord(record, session.getSample(), publish);
            continue;
        }
        if (m_pending.size() >= (size_t) DSDAngel61Session::MaxGatewayRecords) {
            m_pending.erase(m_pending.begin());
        }
        m_pending.push_back(record);
    }

    if (!m_started && !m_pending.empty() && session.hasRouting() && (session.hasPath() || (m_pending.size() >= 2))
        && (session.getRouting().m_source != DVAngel61::CallsignNone))
    {
        // (the records up to the call's last are this call's; any after it the next one's, which
        // wait for its own Start)
        size_t count = 0;

        while ((count < m_pending.size()) && !isLast(m_pending[count])) {
            count++;
        }

        count = std::min(count + 1, m_pending.size());

        DVFrameEvent start;
        start.protocol = "angel61";
        start.kind = DVFrameEvent::Start;
        start.callId = ++m_callId;
        m_sequence = 0;
        start.sequence = m_sequence++;
        start.source = QString::fromStdString(DVAngel61::decodeCallsign(session.getRouting().m_source));
        start.destination = QString::fromStdString(DVAngel61::decodeCallsign(session.getRouting().m_destination));
        start.group = DVAngel61::isGroup(session.getRouting().m_destination);
        if (session.hasPath() && session.getPath().m_repeater1) {
            start.via = QString::fromStdString(DVAngel61::decodeCallsign(session.getPath().m_repeater1));
        }
        publish(start);
        m_started = true;
        std::vector<std::vector<uint8_t>> messages;
        messages.swap(m_messagesPending);
        for (const std::vector<uint8_t>& message : messages) {
            publishMessage(message, session.getSample(), publish);
        }
        std::vector<std::vector<uint8_t>> pending(m_pending.begin(), m_pending.begin() + count);
        m_pending.erase(m_pending.begin(), m_pending.begin() + count);
        for (const std::vector<uint8_t>& record : pending) {
            publishRecord(record, session.getSample(), publish);
        }
    }

    // A call that ended with its routing never known: not offered, and not to be taken for the
    // next one's
    if (!m_started)
    {
        size_t ended = 0;

        for (size_t i = 0; i < m_pending.size(); i++)
        {
            if (isLast(m_pending[i])) {
                ended = i + 1;
            }
        }

        if (ended > 0)
        {
            m_pending.erase(m_pending.begin(), m_pending.begin() + ended);
            m_messagesPending.clear();
        }
    }
}

void DSDAngel61Gateway::reset(const Publish& publish)
{
    if (m_started)
    {
        DVFrameEvent lost;
        lost.protocol = "angel61";
        lost.kind = DVFrameEvent::Lost;
        lost.callId = m_callId;
        lost.sequence = m_sequence++;
        lost.reason = "reset";
        publish(lost);
        m_started = false;
    }

    m_pending.clear();
    m_messagesPending.clear();
}

void DSDAngel61Gateway::publishRecord(const std::vector<uint8_t>& record, int64_t sample, const Publish& publish)
{
    // A revision 3 frame record (DVAngel61::packRecord) as the network's "angel61-frame"
    const bool last = isLast(record);
    DVFrameEvent event;
    event.protocol = "angel61";
    event.kind = DVFrameEvent::Frame;
    event.callId = m_callId;
    event.sequence = m_sequence++;
    event.sampleIndex = sample;
    event.frame.format = "angel61-frame";
    event.frame.type = last ? "end" : "frame";
    event.frame.sequence = record[1] & 0x3F;
    event.frame.data = QByteArray((const char*) record.data(), (int) record.size());
    publish(event);

    if (last)
    {
        DVFrameEvent end = event;
        end.kind = DVFrameEvent::End;
        end.sequence = m_sequence++;
        end.frame = DVFrame();
        publish(end);
        m_started = false;
    }
}

void DSDAngel61Gateway::publishMessage(const std::vector<uint8_t>& message, int64_t sample, const Publish& publish)
{
    // A whole message of the call (DVAngel61::packRelayMessage), beside its records
    DVFrameEvent event;
    event.protocol = "angel61";
    event.kind = DVFrameEvent::Frame;
    event.callId = m_callId;
    event.sequence = m_sequence++;
    event.sampleIndex = sample;
    event.frame.format = "angel61-message";
    event.frame.type = "message";
    event.frame.data = QByteArray((const char*) message.data(), (int) message.size());
    publish(event);
}
