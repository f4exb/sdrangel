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

#include <QDebug>
#include <QRegularExpression>
#include <QScopedPointer>

#include "SWGMapItem.h"
#include "SWGMapCoordinate.h"
#include "SWGMapAircraftState.h"

#include "maincore.h"
#include "util/messagequeue.h"
#include "util/ais.h"
#include "util/ax25.h"
#include "util/aprs.h"
#include "util/radiosonde.h"

#include "mcpdatafeed.h"

// Channels that publish decoded packets, and the pipes they publish them on
static const QStringList packetURIs = {
    QStringLiteral("sdrangel.channel.aisdemod"),
    QStringLiteral("sdrangel.channel.packetdemod"),
    QStringLiteral("sdrangel.channel.chirpchatdemod"),
    QStringLiteral("sdrangel.channel.m17demod"),
    QStringLiteral("sdrangel.channel.meshtasticdemod"),
    QStringLiteral("sdrangel.channel.meshcoredemod"),
    QStringLiteral("sdrangel.channel.inmarsatdemod"),
    QStringLiteral("sdrangel.channel.radiosondedemod"),
};
static const QStringList packetPipes = {"packets", "ais", "radiosonde"};

// Channels and features that publish map items (same list as the Map feature)
static const QStringList mapURIs = {
    QStringLiteral("sdrangel.channel.acarsdemod"),
    QStringLiteral("sdrangel.channel.adsbdemod"),
    QStringLiteral("sdrangel.feature.ais"),
    QStringLiteral("sdrangel.feature.aprs"),
    QStringLiteral("sdrangel.channel.aptdemod"),
    QStringLiteral("sdrangel.channel.dscdemod"),
    QStringLiteral("sdrangel.channel.ft8demod"),
    QStringLiteral("sdrangel.channel.heatmap"),
    QStringLiteral("sdrangel.channel.ilsdemod"),
    QStringLiteral("sdrangel.channel.inmarsatdemod"),
    QStringLiteral("sdrangel.channel.pagerdemod"),
    QStringLiteral("sdrangel.feature.radiosonde"),
    QStringLiteral("sdrangel.feature.startracker"),
    QStringLiteral("sdrangel.feature.satellitetracker"),
    QStringLiteral("sdrangel.feature.sid"),
    QStringLiteral("sdrangel.feature.vorlocalizer"),
};

MCPDataFeed::MCPDataFeed(QObject *parent) :
    QObject(parent),
    m_packetHandler(packetURIs, packetPipes),
    m_mapHandler(mapURIs, QStringList{"mapitems"}),
    m_packetSequence(0),
    m_packetsDropped(0),
    m_mapItemUpdates(0)
{
    connect(&m_packetHandler, &AvailableChannelOrFeatureHandler::messageEnqueued, this, &MCPDataFeed::handlePacketMessages);
    connect(&m_mapHandler, &AvailableChannelOrFeatureHandler::messageEnqueued, this, &MCPDataFeed::handleMapMessages);
    connect(&m_mapHandler, &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged, this, &MCPDataFeed::handleMapSourcesChanged);
    connect(&m_packetHandler, &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged, this, &MCPDataFeed::handleSourcesChanged);
    connect(&m_mapHandler, &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged, this, &MCPDataFeed::handleSourcesChanged);
    m_packetHandler.scanAvailableChannelsAndFeatures();
    m_mapHandler.scanAvailableChannelsAndFeatures();
    refreshSources();
}

// The handlers replace their lists on the main thread as channels and features come and go, so
// get_server_status must not walk them from an HTTP thread. Take a copy here instead.
void MCPDataFeed::refreshSources()
{
    QStringList packetSources;
    QStringList mapSources;

    for (const auto& entry : m_packetHandler.getAvailableChannelOrFeatureList()) {
        packetSources.append(entry.getId() + " " + entry.m_type);
    }

    for (const auto& entry : m_mapHandler.getAvailableChannelOrFeatureList()) {
        mapSources.append(entry.getId() + " " + entry.m_type);
    }

    QMutexLocker locker(&m_mutex);
    m_packetSources = packetSources;
    m_mapSources = mapSources;
}

void MCPDataFeed::handleSourcesChanged()
{
    refreshSources();
}

// ---------------------------------------------------------------------------
// Receiving
// ---------------------------------------------------------------------------

bool MCPDataFeed::findSource(const AvailableChannelOrFeatureHandler& handler, const QObject *object, QString& id, QString& type) const
{
    for (const auto& entry : handler.getAvailableChannelOrFeatureList())
    {
        if (entry.m_object == object)
        {
            id = entry.getId();
            type = entry.m_type;
            return true;
        }
    }

    id = "unknown";
    type = "unknown";
    return false;
}

void MCPDataFeed::handlePacketMessages(MessageQueue *queue)
{
    Message *message;

    while ((message = queue->pop()) != nullptr)
    {
        handlePacket(*message);
        delete message;
    }
}

void MCPDataFeed::handleMapMessages(MessageQueue *queue)
{
    Message *message;

    while ((message = queue->pop()) != nullptr)
    {
        handleMapItem(*message);
        delete message;
    }
}

void MCPDataFeed::handlePacket(const Message& message)
{
    if (!MainCore::MsgPacket::match(message)) {
        return;
    }

    const MainCore::MsgPacket& msg = (const MainCore::MsgPacket&) message;
    Packet packet;
    findSource(m_packetHandler, msg.getPipeSource(), packet.m_source, packet.m_sourceType);
    packet.m_dateTime = msg.getDateTime();
    packet.m_bytes = msg.getPacket();
    packet.m_decoded = decodePacket(packet.m_sourceType, packet.m_bytes);

    {
        QMutexLocker locker(&m_mutex);
        packet.m_sequence = ++m_packetSequence;
        m_packets.append(packet);

        while (m_packets.size() > m_maxPackets)
        {
            m_packets.removeFirst();
            m_packetsDropped++;
        }
    }

    emit packetsChanged();
}

void MCPDataFeed::handleMapItem(const Message& message)
{
    if (!MainCore::MsgMapItem::match(message)) {
        return;
    }

    const MainCore::MsgMapItem& msg = (const MainCore::MsgMapItem&) message;
    SWGSDRangel::SWGMapItem *swgItem = msg.getSWGMapItem();

    // Producers allocate a separate item for each registered pipe, so this one belongs to us.
    // MainCore::MsgMapItem has no destructor, so deleting the message would not free it.
    QScopedPointer<SWGSDRangel::SWGMapItem> owned(swgItem);

    if (!swgItem || !swgItem->getName()) {
        return;
    }

    MapItem item;
    findSource(m_mapHandler, msg.getPipeSource(), item.m_source, item.m_sourceType);
    item.m_name = *swgItem->getName();
    item.m_updated = QDateTime::currentDateTimeUtc();
    QString key = item.m_source + "|" + item.m_name;
    // As in the Map feature, an item without an image is a request to remove it
    bool remove = !swgItem->getImage() || swgItem->getImage()->isEmpty();

    if (!remove) {
        item.m_item = mapItemToJson(swgItem, true);
    }

    {
        QMutexLocker locker(&m_mutex);
        m_mapItemUpdates++;

        if (remove) {
            m_mapItems.remove(key);
        } else {
            m_mapItems[key] = item;
        }
    }

    emit mapItemsChanged();
}

// A source that is removed takes its map items with it (as in the Map feature), and sources that are
// renumbered when another channel or feature is deleted keep their items under the new id.
// The lists hold long ids such as "F:1 StarTracker"; the id is the part before the space.
void MCPDataFeed::handleMapSourcesChanged(const QStringList& renameFrom, const QStringList& renameTo, const QStringList& removed, const QStringList& added)
{
    (void) added;
    QMutexLocker locker(&m_mutex);

    for (const QString& longId : removed)
    {
        QString id = longId.section(' ', 0, 0);

        for (auto it = m_mapItems.begin(); it != m_mapItems.end();)
        {
            if (it->m_source == id) {
                it = m_mapItems.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (renameFrom.size() == renameTo.size())
    {
        QMap<QString, MapItem> renamed;

        for (auto it = m_mapItems.begin(); it != m_mapItems.end(); ++it)
        {
            MapItem item = it.value();

            for (int i = 0; i < renameFrom.size(); i++)
            {
                if (item.m_source == renameFrom[i].section(' ', 0, 0))
                {
                    item.m_source = renameTo[i].section(' ', 0, 0);
                    break;
                }
            }

            renamed[item.m_source + "|" + item.m_name] = item;
        }

        m_mapItems = renamed;
    }

    locker.unlock();

    // Items were dropped or re-keyed, so anything subscribed to the resource is now stale
    emit mapItemsChanged();
}

// ---------------------------------------------------------------------------
// Decoding
// ---------------------------------------------------------------------------

QString MCPDataFeed::printable(const QByteArray& bytes)
{
    QString s;

    for (char c : bytes) {
        s.append(((c >= 0x20) && (c < 0x7f)) ? QChar(c) : QChar('.'));
    }

    return s;
}

QString MCPDataFeed::stripHtml(const QString& html)
{
    static const QRegularExpression tags("<[^>]*>");
    static const QRegularExpression breaks("<br\\s*/?>", QRegularExpression::CaseInsensitiveOption);
    QString s = html;
    s.replace(breaks, "\n");
    s.remove(tags);
    s.replace("&nbsp;", " ").replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">");
    return s.trimmed();
}

QJsonObject MCPDataFeed::decodePacket(const QString& sourceType, const QByteArray& bytes)
{
    QJsonObject decoded;

    if (sourceType == "AISDemod")
    {
        AISMessage *ais = AISMessage::decode(bytes);

        if (ais)
        {
            decoded["protocol"] = "AIS";
            decoded["messageType"] = ais->getType();
            decoded["messageId"] = ais->m_id;
            decoded["mmsi"] = ais->m_mmsi;

            if (ais->hasPosition())
            {
                decoded["latitude"] = ais->getLatitude();
                decoded["longitude"] = ais->getLongitude();
            }
            if (ais->hasCourse()) {
                decoded["course"] = ais->getCourse();
            }
            if (ais->hasSpeed()) {
                decoded["speedKnots"] = ais->getSpeed();
            }
            if (ais->hasHeading()) {
                decoded["heading"] = ais->getHeading();
            }

            QString summary = ais->toString();

            if (!summary.isEmpty()) {
                decoded["summary"] = summary;
            }

            delete ais;
        }
    }
    else if (sourceType == "RadiosondeDemod")
    {
        RS41Frame frame(bytes);
        decoded["protocol"] = "RS41";

        if (frame.m_statusValid)
        {
            decoded["serial"] = frame.m_serial;
            decoded["frameNumber"] = frame.m_frameNumber;
            decoded["batteryVoltage"] = frame.m_batteryVoltage;
        }
        if (frame.m_posValid)
        {
            decoded["latitude"] = frame.m_latitude;
            decoded["longitude"] = frame.m_longitude;
            decoded["heightMetres"] = frame.m_height;
            decoded["speedMetresPerSecond"] = frame.m_speed;
            decoded["heading"] = frame.m_heading;
        }
    }
    else if ((sourceType == "PacketDemod") || (sourceType == "ChirpChatDemod") || (sourceType == "M17Demod"))
    {
        AX25Packet ax25;

        if (ax25.decode(bytes))
        {
            decoded["protocol"] = "AX.25";
            decoded["from"] = ax25.m_from;
            decoded["to"] = ax25.m_to;

            if (!ax25.m_via.isEmpty()) {
                decoded["via"] = ax25.m_via;
            }

            decoded["frameType"] = ax25.m_type;
            decoded["pid"] = ax25.m_pid;
            decoded["data"] = printable(ax25.m_data);

            APRSPacket aprs;

            if (aprs.decode(ax25))
            {
                QJsonObject a;

                if (aprs.m_hasPosition)
                {
                    a["latitude"] = aprs.m_latitude;
                    a["longitude"] = aprs.m_longitude;
                }
                if (aprs.m_hasAltitude) {
                    a["altitudeFeet"] = aprs.m_altitudeFt;
                }
                if (!aprs.m_comment.isEmpty()) {
                    a["comment"] = aprs.m_comment;
                }
                if (!aprs.m_status.isEmpty()) {
                    a["status"] = aprs.m_status;
                }
                if (aprs.m_hasSymbol) {
                    a["symbol"] = QString("%1%2").arg(aprs.m_symbolTable).arg(aprs.m_symbolCode);
                }

                decoded["aprs"] = a;
            }
        }
    }

    return decoded;
}

QJsonObject MCPDataFeed::packetToJson(const Packet& packet)
{
    QJsonObject p;
    p["sequence"] = (double) packet.m_sequence;
    p["time"] = packet.m_dateTime.toString(Qt::ISODateWithMs);
    p["source"] = packet.m_source;
    p["sourceType"] = packet.m_sourceType;
    p["length"] = packet.m_bytes.size();
    p["hex"] = QString(packet.m_bytes.toHex());
    p["text"] = printable(packet.m_bytes);

    if (!packet.m_decoded.isEmpty()) {
        p["decoded"] = packet.m_decoded;
    }

    return p;
}

QJsonObject MCPDataFeed::mapItemToJson(SWGSDRangel::SWGMapItem *item, bool includeTrack)
{
    QJsonObject j;
    j["latitude"] = item->getLatitude();
    j["longitude"] = item->getLongitude();
    j["altitude"] = item->getAltitude();

    if (item->getLabel() && !item->getLabel()->isEmpty()) {
        j["label"] = *item->getLabel();
    }
    if (item->getText() && !item->getText()->isEmpty()) {
        j["text"] = stripHtml(*item->getText());
    }
    if (item->getImage() && !item->getImage()->isEmpty()) {
        j["image"] = *item->getImage();
    }
    if (item->getModel() && !item->getModel()->isEmpty()) {
        j["model"] = *item->getModel();
    }
    if (item->getPositionDateTime() && !item->getPositionDateTime()->isEmpty()) {
        j["positionDateTime"] = *item->getPositionDateTime();
    }

    j["heading"] = item->getHeading();
    j["type"] = item->getType();

    if (item->getAircraftState() && item->getAircraftState()->isSet())
    {
        QJsonObject *state = item->getAircraftState()->asJsonObject();
        j["aircraftState"] = *state;
        delete state;
    }

    if (includeTrack && item->getTrack() && !item->getTrack()->isEmpty())
    {
        QJsonArray track;

        for (auto *c : *item->getTrack())
        {
            QJsonObject point;
            point["latitude"] = c->getLatitude();
            point["longitude"] = c->getLongitude();
            point["altitude"] = c->getAltitude();
            track.append(point);
        }

        j["track"] = track;
    }

    return j;
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

QJsonObject MCPDataFeed::getPackets(const QString& source, const QString& type, int limit, qint64 sinceSequence)
{
    QMutexLocker locker(&m_mutex);
    QJsonArray packets;
    int matched = 0;

    // Walk from the newest so the limit keeps the most recent packets, then restore order
    for (int i = m_packets.size() - 1; (i >= 0) && (packets.size() < limit); i--)
    {
        const Packet& packet = m_packets[i];

        if (packet.m_sequence <= sinceSequence) {
            break;
        }
        if (!source.isEmpty() && (packet.m_source != source)) {
            continue;
        }
        if (!type.isEmpty() && (packet.m_sourceType.compare(type, Qt::CaseInsensitive) != 0)) {
            continue;
        }

        matched++;
        packets.prepend(packetToJson(packet));
    }

    QJsonObject result;
    result["packets"] = packets;
    result["count"] = packets.size();
    result["totalStored"] = m_packets.size();
    result["lastSequence"] = (double) m_packetSequence;
    result["dropped"] = (double) m_packetsDropped;
    return result;
}

void MCPDataFeed::clearPackets()
{
    {
        QMutexLocker locker(&m_mutex);
        m_packets.clear();
    }

    // Subscribers would otherwise keep showing the packets that were just discarded
    emit packetsChanged();
}

QJsonObject MCPDataFeed::getMapItems(const QString& source, const QString& name, int limit, bool includeTrack)
{
    QMutexLocker locker(&m_mutex);
    QJsonArray items;
    QString wantedName = name.trimmed().toLower();

    for (const MapItem& item : m_mapItems)
    {
        if (!source.isEmpty() && (item.m_source != source) && (item.m_sourceType.compare(source, Qt::CaseInsensitive) != 0)) {
            continue;
        }
        if (!wantedName.isEmpty() && !item.m_name.toLower().contains(wantedName)) {
            continue;
        }

        QJsonObject j = item.m_item;

        if (!includeTrack) {
            j.remove("track");
        }

        j["name"] = item.m_name;
        j["source"] = item.m_source;
        j["sourceType"] = item.m_sourceType;
        j["updated"] = item.m_updated.toString(Qt::ISODateWithMs);
        items.append(j);

        if (items.size() >= limit) {
            break;
        }
    }

    QJsonObject result;
    result["items"] = items;
    result["count"] = items.size();
    result["totalItems"] = m_mapItems.size();
    return result;
}

QJsonObject MCPDataFeed::getStatus()
{
    QMutexLocker locker(&m_mutex);
    QJsonObject status;
    status["packetsStored"] = m_packets.size();
    status["packetsReceived"] = (double) m_packetSequence;
    status["mapItems"] = m_mapItems.size();
    status["mapItemUpdates"] = (double) m_mapItemUpdates;

    status["packetSources"] = QJsonArray::fromStringList(m_packetSources);
    status["mapItemSources"] = QJsonArray::fromStringList(m_mapSources);
    return status;
}
