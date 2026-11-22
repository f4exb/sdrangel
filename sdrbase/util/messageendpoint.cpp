///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#include <QDateTime>
#include <QUuid>

#include <algorithm>

#include "util/messageendpoint.h"

namespace {

QList<int> intsFromVariant(const QVariant& value)
{
    QList<int> out;

    for (const QVariant& v : value.toList()) {
        out.append(v.toInt());
    }

    return out;
}

QVariantList intsToVariant(const QList<int>& values)
{
    QVariantList out;

    for (int v : values) {
        out.append(v);
    }

    return out;
}

} // namespace

bool MessageEndpoint::fromRecord(const MessageRecord& record, MessageEndpoint& endpoint, QString& event)
{
    const QVariantMap& d = record.m_protocolData;
    event = d.value("event").toString();

    if ((record.m_kind != MessageRecord::KindEvent) || ((event != "register") && (event != "status") && (event != "unregister"))
        || d.value("endpoint").toString().isEmpty()) {
        return false;
    }

    endpoint = MessageEndpoint();
    endpoint.m_channel = d.value("endpoint").toString();
    endpoint.m_role = (d.value("role").toString() == "tx") ? Transmitter : Receiver;
    QString status = d.value("status").toString();
    endpoint.m_status = (event == "unregister") ? Offline : (status == "busy") ? Busy : (status == "offline") ? Offline : Idle;
    endpoint.m_protocols = d.value("protocols").toStringList();
    endpoint.m_speeds = intsFromVariant(d.value("speeds"));
    endpoint.m_polarities = intsFromVariant(d.value("polarities"));
    endpoint.m_queueDepth = std::max(0, d.value("queueDepth").toInt());
    endpoint.m_capacity = std::max(0, d.value("capacity").toInt());
    endpoint.m_frequency = record.m_frequency;
    endpoint.m_lastSeen = record.m_timestamp;
    endpoint.m_fields = d.value("fields").toList();

    for (auto it = d.begin(); it != d.end(); ++it)
    {
        if (it.key().startsWith("cap_")) {
            endpoint.m_capabilities.insert(it.key().mid(4), it.value());
        }
    }

    return true;
}

MessageRecord MessageEndpoint::toRecord(const MessageEndpoint& endpoint, const QString& event)
{
    static const char *statuses[] = {"offline", "idle", "busy"};
    MessageRecord record;
    record.m_timestamp = QDateTime::currentMSecsSinceEpoch();
    record.m_kind = MessageRecord::KindEvent;
    record.m_direction = (endpoint.m_role == Transmitter) ? MessageRecord::DirectionTX : MessageRecord::DirectionRX;
    record.m_channel = endpoint.m_channel;
    record.m_frequency = endpoint.m_frequency;
    record.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.m_protocolData.insert("event", event);
    record.m_protocolData.insert("endpoint", endpoint.m_channel);
    record.m_protocolData.insert("role", endpoint.m_role == Transmitter ? "tx" : "rx");
    record.m_protocolData.insert("status", statuses[std::max(0, std::min(2, (int) endpoint.m_status))]);
    record.m_protocolData.insert("protocols", endpoint.m_protocols);
    record.m_protocolData.insert("speeds", intsToVariant(endpoint.m_speeds));
    record.m_protocolData.insert("polarities", intsToVariant(endpoint.m_polarities));
    record.m_protocolData.insert("queueDepth", endpoint.m_queueDepth);
    record.m_protocolData.insert("capacity", endpoint.m_capacity);

    for (auto it = endpoint.m_capabilities.begin(); it != endpoint.m_capabilities.end(); ++it) {
        record.m_protocolData.insert("cap_" + it.key(), it.value());
    }
    if (!endpoint.m_fields.isEmpty()) {
        record.m_protocolData.insert("fields", endpoint.m_fields);
    }

    return record;
}

bool MessageEndpoint::canTransmit(const MessageRecord& page) const
{
    if (m_role != Transmitter) {
        return false;
    }
    if (!m_protocols.isEmpty() && !m_protocols.contains(page.m_protocol, Qt::CaseInsensitive)) {
        return false;
    }
    if (!m_polarities.isEmpty() && (page.m_polarity != MessageRecord::PolarityUnknown) && !m_polarities.contains((int) page.m_polarity)) {
        return false;
    }
    if (!m_speeds.isEmpty() && (page.m_speed > 0) && !m_speeds.contains(page.m_speed)) {
        return false;
    }

    return true;
}

bool MessageEndpoint::ready() const
{
    if (m_status == Offline) {
        return false;
    }

    // One that does not say how many it can take is given a page when it is idle
    return (m_capacity > 0) ? (m_queueDepth < m_capacity) : (m_status == Idle);
}
