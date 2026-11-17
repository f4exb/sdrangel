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

#ifndef INCLUDE_UTIL_MESSAGEENDPOINT_H
#define INCLUDE_UTIL_MESSAGEENDPOINT_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "export.h"
#include "util/messagerecord.h"

// A channel that exchanges messages with the message center: what it handles and its state. A
// channel announces itself on the "messages" pipe with event records (protocolData "event" =
// register, status or unregister); the message center keeps the last one of each alias. A status
// record carries the whole description too, so a message center started later learns the
// channels at their next announcement.
struct SDRBASE_API MessageEndpoint
{
    enum Role { Receiver, Transmitter };
    enum Status { Offline, Idle, Busy };

    QString m_channel;                  //!< The channel name (title): the transmitter of a page
    Role m_role = Receiver;
    Status m_status = Offline;
    QStringList m_protocols;            //!< Lower case protocol names it handles
    QList<int> m_speeds;                //!< Bits per second, empty: any
    QList<int> m_polarities;            //!< MessageRecord::Polarity values it handles, empty: both
    int m_queueDepth = 0;               //!< Pages in its own queue
    int m_capacity = 0;                 //!< Pages its queue can take, 0: it does not say
    qint64 m_frequency = -1;
    qint64 m_lastSeen = 0;              //!< Milliseconds since the epoch
    QVariantMap m_capabilities;         //!< The rest as announced
    //! The fields of protocolData its records have, described for the conditions of filters: each a map of key, name
    //! (readable), protocol (lower case), type (list, number or text) and values (of a list)
    QVariantList m_fields;

    //!< The endpoint of an event record, false if it is not one
    static bool fromRecord(const MessageRecord& record, MessageEndpoint& endpoint, QString& event);
    //!< The event record for an endpoint: register, status or unregister
    static MessageRecord toRecord(const MessageEndpoint& endpoint, const QString& event);
    bool canTransmit(const MessageRecord& page) const;
    bool ready() const;                 //!< Idle or busy with room in its queue
};

#endif // INCLUDE_UTIL_MESSAGEENDPOINT_H
