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

#include <QColor>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "audio/audiodevicemanager.h"
#include "messagecentersettings.h"
#include "radiopagingpage.h"

MessageCenterSettings::MessageCenterSettings() :
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void MessageCenterSettings::resetToDefaults()
{
    m_title = "Message Center";
    m_rgbColor = QColor(0, 170, 127).rgb();
    m_logSize = MessageCenterEngine::m_defaultLogSize;
    m_historyEnabled = true;
    m_historyMaxMB = MessageCenterEngine::m_defaultHistoryMB;
    m_historyDays = MessageCenterEngine::m_defaultHistoryDays;
    m_claimTimeoutSec = 10;
    m_silentSec = 20;
    m_routes = MessageCenterEngine::defaultRoutes();
    m_contacts.clear();
    m_sinks.clear();
    m_sources.clear();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_workspaceIndex = 0;
    m_geometryBytes.clear();
    m_hidden = false;
    m_logColumns.clear();
    m_voicePlaybackDeviceName = AudioDeviceManager::m_defaultDeviceName;
}

QByteArray MessageCenterSettings::routesToJson(const std::vector<MessageCenterEngine::Route>& routes)
{
    QJsonArray array;

    for (const MessageCenterEngine::Route& route : routes)
    {
        QJsonObject o;
        o["name"] = route.m_name;
        o["enabled"] = route.m_enabled;
        o["stop"] = route.m_stop;
        o["sinks"] = QJsonArray::fromStringList(route.m_sinks);
        o["filter"] = route.m_filter.toJson();

        if (!route.m_decodeCharset.empty()) {
            o["decodeCharset"] = QJsonObject::fromVariantMap(RadioPagingPage::charsetData(route.m_decodeCharset));
        }
        if (!route.m_decodeType.isEmpty()) {
            o["decodeType"] = route.m_decodeType;
        }

        array.append(o);
    }

    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

std::vector<MessageCenterEngine::Route> MessageCenterSettings::routesFromJson(const QByteArray& json)
{
    std::vector<MessageCenterEngine::Route> routes;

    for (const QJsonValue& v : QJsonDocument::fromJson(json).array())
    {
        QJsonObject o = v.toObject();
        MessageCenterEngine::Route route;
        route.m_name = o["name"].toString();
        route.m_enabled = o["enabled"].toBool(true);
        route.m_stop = o["stop"].toBool(false);

        for (const QJsonValue& s : o["sinks"].toArray()) {
            route.m_sinks.append(s.toString());
        }

        route.m_filter = MessageCenterFilter::fromJson(o["filter"].toObject());
        route.m_decodeCharset = RadioPagingPage::charsetKey(o["decodeCharset"].toObject().toVariantMap());
        route.m_decodeType = o["decodeType"].toString();
        routes.push_back(route);
    }

    return routes;
}

const QStringList& MessageCenterFileSetting::sinkTypes()
{
    static const QStringList types = {"file", "udp", "command", "speech", "mark", "map"};
    return types;
}

const QStringList& MessageCenterFileSetting::sourceTypes()
{
    static const QStringList types = {"file", "csv", "udp"};
    return types;
}

QString MessageCenterFileSetting::check(bool sink) const
{
    const QString type = typeOrDefault();

    if (m_name.isEmpty()) {
        return "A name is needed";
    }
    if (sink && ((m_name == "log") || (m_name == "queue"))) {
        return QString("%1 is the name of a sink of the message center").arg(m_name);
    }
    if (!(sink ? sinkTypes() : sourceTypes()).contains(type)) {
        return QString("%1: %2 is not a type of %3").arg(m_name).arg(type).arg(sink ? "sink" : "source");
    }
    if (((type == "file") || (type == "csv")) && m_path.isEmpty()) {
        return QString("%1: a file needs a path").arg(m_name);
    }
    if (type == "udp")
    {
        if ((m_port < 1) || (m_port > 65535)) {
            return QString("%1: the port must be 1 to 65535").arg(m_name);
        }
        // A sink sends to an address; a source listens on one, the default is this computer
        if (sink ? m_address.isEmpty() : false) {
            return QString("%1: a UDP sink needs an address").arg(m_name);
        }
        if (!m_address.isEmpty() && QHostAddress(m_address).isNull()) {
            return QString("%1: %2 is not an IP address").arg(m_name).arg(m_address);
        }
        if (m_format < 0 || m_format > 1) {
            return QString("%1: the format must be 0 or 1").arg(m_name);
        }
    }
    if ((type == "command") && m_text.trimmed().isEmpty()) {
        return QString("%1: a command sink needs a command").arg(m_name);
    }
    if ((type == "speech") && m_text.trimmed().isEmpty()) {
        return QString("%1: a speech sink needs a text").arg(m_name);
    }
    if ((type == "mark") && m_icon.isEmpty() && (m_color == 0) && (m_background == 0)) {
        return QString("%1: a mark needs an icon, a colour of the text or a colour of the row").arg(m_name);
    }

    return QString();
}

QStringList MessageCenterSettings::presetNames()
{
    return {"Empty", "Emergency"};
}

bool MessageCenterSettings::preset(const QString& name, std::vector<MessageCenterEngine::Route>& routes, QList<MessageCenterFileSetting>& sinks)
{
    if (!presetNames().contains(name)) {
        return false;
    }

    routes = MessageCenterEngine::defaultRoutes();
    sinks.clear();

    if (name == "Empty") {
        return true;
    }

    // An example: a few words of the text (English, and Dutch of the P2000 network) and an icon for each kind of message
    static const struct { const char *name; const char *text; const char *icon; } kinds[] = {
        {"Ambulance", "ambu|reanimatie|cardiac|drowning", ":ambulance"},
        {"Fire", "brand|fire|smoke", ":fire_engine"},
        {"Accident", "ongeval|accident|crash", ":collision"},
        {"Hazardous", "gaslucht|hazmat", ":biohazard"},
        {"Helicopter", "lifeliner|helicopter", ":helicopter"},
        {"Police", "politie|police", ":police_car_light"},
        {"Call back", "graag contact|please call", ":telephone_receiver"},
        {"Plant alarm", "high level|fault", ":wrench"},
        {"Test", "test page|testoproep", ":satellite_antenna"}
    };

    for (const auto& kind : kinds)
    {
        MessageCenterFileSetting sink;
        sink.m_name = kind.name;
        sink.m_type = "mark";
        sink.m_icon = kind.icon;
        sinks.append(sink);

        MessageCenterEngine::Route route;
        route.m_name = kind.name;
        route.m_filter.m_text = kind.text;
        route.m_sinks = QStringList(kind.name);
        routes.push_back(route);
    }

    return true;
}

QByteArray MessageCenterSettings::filesToJson(const QList<MessageCenterFileSetting>& files)
{
    QJsonArray array;

    for (const MessageCenterFileSetting& file : files)
    {
        QJsonObject o;
        o["name"] = file.m_name;
        o["path"] = file.m_path;
        o["format"] = file.m_format;

        // What a file does not have is not written: the settings of the files are as they were
        if (!file.m_type.isEmpty() && (file.m_type != "file")) {
            o["type"] = file.m_type;
        }
        if (!file.m_address.isEmpty()) {
            o["address"] = file.m_address;
        }
        if (file.m_port != 0) {
            o["port"] = file.m_port;
        }
        if (!file.m_text.isEmpty()) {
            o["text"] = file.m_text;
        }
        if (!file.m_icon.isEmpty()) {
            o["icon"] = file.m_icon;
        }
        if (file.m_color != 0) {
            o["color"] = (double) file.m_color;
        }
        if (file.m_background != 0) {
            o["background"] = (double) file.m_background;
        }

        array.append(o);
    }

    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

QList<MessageCenterFileSetting> MessageCenterSettings::filesFromJson(const QByteArray& json)
{
    QList<MessageCenterFileSetting> files;

    for (const QJsonValue& v : QJsonDocument::fromJson(json).array())
    {
        QJsonObject o = v.toObject();
        MessageCenterFileSetting file;
        file.m_name = o["name"].toString();
        file.m_path = o["path"].toString();
        file.m_format = o["format"].toInt();
        file.m_type = o["type"].toString();
        file.m_address = o["address"].toString();
        file.m_port = o["port"].toInt();
        file.m_text = o["text"].toString();
        file.m_icon = o["icon"].toString();
        file.m_color = (quint32) o["color"].toDouble(0);
        file.m_background = (quint32) o["background"].toDouble(0);
        files.append(file);
    }

    return files;
}

QByteArray MessageCenterSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_title);
    s.writeU32(2, m_rgbColor);
    s.writeS32(3, m_logSize);
    s.writeS32(4, m_claimTimeoutSec);
    s.writeS32(5, m_silentSec);
    s.writeBlob(6, routesToJson(m_routes));
    s.writeBlob(7, filesToJson(m_sinks));
    s.writeBlob(8, filesToJson(m_sources));

    if (m_rollupState) {
        s.writeBlob(9, m_rollupState->serialize());
    }

    s.writeS32(10, m_workspaceIndex);
    s.writeBlob(11, m_geometryBytes);
    s.writeBool(12, m_hidden);
    s.writeBool(13, m_useReverseAPI);
    s.writeString(14, m_reverseAPIAddress);
    s.writeU32(15, m_reverseAPIPort);
    s.writeU32(16, m_reverseAPIFeatureSetIndex);
    s.writeU32(17, m_reverseAPIFeatureIndex);
    s.writeBlob(18, m_logColumns);
    s.writeString(19, m_voicePlaybackDeviceName);
    s.writeBool(20, m_historyEnabled);
    s.writeS32(21, m_historyMaxMB);
    s.writeS32(22, 2);      // Routing: the log is a sink
    s.writeBlob(23, MessageCenterContact::listToJson(m_contacts));
    s.writeS32(24, m_historyDays);

    return s.final();
}

bool MessageCenterSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytes;

        d.readString(1, &m_title, "Message Center");
        d.readU32(2, &m_rgbColor, QColor(0, 170, 127).rgb());
        d.readS32(3, &m_logSize, MessageCenterEngine::m_defaultLogSize);
        d.readS32(4, &m_claimTimeoutSec, 10);
        d.readS32(5, &m_silentSec, 20);
        m_logSize = std::max(MessageCenterEngine::m_minLogSize, std::min(MessageCenterEngine::m_maxLogSize, m_logSize));
        m_claimTimeoutSec = std::max(1, std::min(3600, m_claimTimeoutSec));
        m_silentSec = std::max(6, std::min(3600, m_silentSec));
        d.readBlob(6, &bytes);
        m_routes = routesFromJson(bytes);
        d.readBlob(7, &bytes);
        m_sinks = filesFromJson(bytes);
        d.readBlob(8, &bytes);
        m_sources = filesFromJson(bytes);

        if (m_rollupState)
        {
            d.readBlob(9, &bytes);
            m_rollupState->deserialize(bytes);
        }

        d.readS32(10, &m_workspaceIndex, 0);
        d.readBlob(11, &m_geometryBytes);
        d.readBool(12, &m_hidden, false);
        d.readBool(13, &m_useReverseAPI, false);
        d.readString(14, &m_reverseAPIAddress, "127.0.0.1");
        uint32_t utmp;
        d.readU32(15, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(16, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(17, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;
        d.readBlob(18, &m_logColumns);
        d.readString(19, &m_voicePlaybackDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(20, &m_historyEnabled, true);
        d.readS32(21, &m_historyMaxMB, MessageCenterEngine::m_defaultHistoryMB);
        d.readBlob(23, &bytes);
        m_contacts = MessageCenterContact::listFromJson(bytes);

        // Settings of before the log was a sink: a record no route selected was logged. The default route at the top
        // keeps logging every message
        qint32 routingVersion;
        d.readS32(22, &routingVersion, 1);

        if (routingVersion < 2)
        {
            std::vector<MessageCenterEngine::Route> routes = MessageCenterEngine::defaultRoutes();
            routes.insert(routes.end(), m_routes.begin(), m_routes.end());
            m_routes = routes;
        }
        m_historyMaxMB = std::max(1, std::min(10000, m_historyMaxMB));
        d.readS32(24, &m_historyDays, MessageCenterEngine::m_defaultHistoryDays);
        m_historyDays = std::max(1, std::min(3650, m_historyDays));
        return true;
    }

    resetToDefaults();
    return false;
}

void MessageCenterSettings::applySettings(const QStringList& settingsKeys, const MessageCenterSettings& settings)
{
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("logSize")) {
        m_logSize = settings.m_logSize;
    }
    if (settingsKeys.contains("historyEnabled")) {
        m_historyEnabled = settings.m_historyEnabled;
    }
    if (settingsKeys.contains("historyMaxMB")) {
        m_historyMaxMB = settings.m_historyMaxMB;
    }
    if (settingsKeys.contains("historyDays")) {
        m_historyDays = settings.m_historyDays;
    }
    if (settingsKeys.contains("claimTimeoutSec")) {
        m_claimTimeoutSec = settings.m_claimTimeoutSec;
    }
    if (settingsKeys.contains("silentSec")) {
        m_silentSec = settings.m_silentSec;
    }
    if (settingsKeys.contains("routes")) {
        m_routes = settings.m_routes;
    }
    if (settingsKeys.contains("contacts")) {
        m_contacts = settings.m_contacts;
    }
    if (settingsKeys.contains("sinks")) {
        m_sinks = settings.m_sinks;
    }
    if (settingsKeys.contains("sources")) {
        m_sources = settings.m_sources;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIFeatureSetIndex")) {
        m_reverseAPIFeatureSetIndex = settings.m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex")) {
        m_reverseAPIFeatureIndex = settings.m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("logColumns")) {
        m_logColumns = settings.m_logColumns;
    }
    if (settingsKeys.contains("voicePlaybackDeviceName")) {
        m_voicePlaybackDeviceName = settings.m_voicePlaybackDeviceName;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}
