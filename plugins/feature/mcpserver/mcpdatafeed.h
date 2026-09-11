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

#ifndef INCLUDE_FEATURE_MCPDATAFEED_H_
#define INCLUDE_FEATURE_MCPDATAFEED_H_

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QStringList>
#include <QObject>
#include <QString>

#include "availablechannelorfeaturehandler.h"

class MessageQueue;
class Message;

namespace SWGSDRangel {
    class SWGMapItem;
}

// Collects live data from other plugins through the message pipes: decoded packets
// (AIS, AX.25/APRS, radiosonde, Meshtastic...) on the "packets", "ais" and "radiosonde"
// pipes, and map items (aircraft, ships, sondes, satellites...) on the "mapitems" pipe.
//
// Lives in the main thread where the pipe messages are delivered; the stores are read
// from the HTTP threads under a mutex.
class MCPDataFeed : public QObject
{
    Q_OBJECT
public:
    struct Packet
    {
        qint64 m_sequence;
        QDateTime m_dateTime;
        QString m_source;      //!< Channel id, e.g. "R0:1"
        QString m_sourceType;  //!< Channel type, e.g. "AISDemod"
        QByteArray m_bytes;
        QJsonObject m_decoded; //!< Protocol specific decode, may be empty
    };

    struct MapItem
    {
        QDateTime m_updated;
        QString m_source;      //!< Channel or feature id, e.g. "R0:1" or "F:0"
        QString m_sourceType;  //!< Plugin type, e.g. "ADSBDemod"
        QString m_name;
        QJsonObject m_item;    //!< Selected fields of the map item
    };

    explicit MCPDataFeed(QObject *parent = nullptr);

    // Packets, most recent last. Filters are optional; sinceSequence returns only packets after that sequence number
    QJsonObject getPackets(const QString& source, const QString& type, int limit, qint64 sinceSequence);
    void clearPackets();
    QJsonObject getMapItems(const QString& source, const QString& name, int limit, bool includeTrack);
    QJsonObject getStatus();

    static const int m_maxPackets = 1000;

private:
    AvailableChannelOrFeatureHandler m_packetHandler;
    AvailableChannelOrFeatureHandler m_mapHandler;
    QMutex m_mutex;
    QList<Packet> m_packets;
    qint64 m_packetSequence;
    qint64 m_packetsDropped;
    QMap<QString, MapItem> m_mapItems;   //!< Keyed by source + "|" + name
    qint64 m_mapItemUpdates;

    // Summaries of the handlers' source lists. The handlers rebuild those lists on the main
    // thread whenever a channel or feature comes or goes, so they cannot be read from an HTTP
    // thread; these copies are refreshed on the main thread and read under the mutex.
    QStringList m_packetSources;
    QStringList m_mapSources;

    void refreshSources();  //!< Main thread only
    bool findSource(const AvailableChannelOrFeatureHandler& handler, const QObject *object, QString& id, QString& type) const;
    void handlePacket(const Message& message);
    void handleMapItem(const Message& message);
    static QJsonObject decodePacket(const QString& sourceType, const QByteArray& bytes);
    static QString printable(const QByteArray& bytes);
    static QString stripHtml(const QString& html);
    static QJsonObject mapItemToJson(SWGSDRangel::SWGMapItem *item, bool includeTrack);
    static QJsonObject packetToJson(const Packet& packet);

signals:
    void packetsChanged();   //!< A packet was added to the buffer
    void mapItemsChanged();  //!< A map item was added, updated or removed

private slots:
    void handlePacketMessages(MessageQueue *queue);
    void handleMapMessages(MessageQueue *queue);
    void handleMapSourcesChanged(const QStringList& renameFrom, const QStringList& renameTo, const QStringList& removed, const QStringList& added);
    void handleSourcesChanged();
};

#endif // INCLUDE_FEATURE_MCPDATAFEED_H_
