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

#include "messagerecord.h"

#include <QJsonDocument>
#include <QJsonValue>

#include "util/csv.h"

const char * const MessageRecord::m_pipeName = "messages";

namespace {

QJsonValue stringOrNull(const QString& s)
{
    return s.isNull() ? QJsonValue(QJsonValue::Null) : QJsonValue(s);
}

QJsonValue intOrNull(qint64 v)
{
    return v < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(v);
}

QString readString(const QJsonObject& json, const char *key)
{
    QJsonValue v = json.value(key);
    return v.isString() ? v.toString() : QString();
}

qint64 readInt(const QJsonObject& json, const char *key, qint64 nullValue = -1)
{
    QJsonValue v = json.value(key);
    return v.isDouble() ? (qint64) v.toDouble() : nullValue;
}

QString csvString(const QString& s)
{
    return s.isNull() ? QString("") : CSV::escape(s);
}

QString csvInt(qint64 v)
{
    return v < 0 ? QString("") : QString::number(v);
}

} // namespace

MessageRecord::MessageRecord()
{
    resetToDefaults();
}

void MessageRecord::resetToDefaults()
{
    m_id = QString();
    m_timestamp = 0;
    m_direction = DirectionRX;
    m_protocol = QString();
    m_mode = QString();
    m_kind = KindMessage;
    m_msgType = QString();
    m_address = QString();
    m_polarity = PolarityUnknown;
    m_speed = -1;
    m_text = QString();
    m_rawData.clear();
    m_rawBits = -1;
    m_errorsCorrected = -1;
    m_errorsUncorrectable = -1;
    m_ber = -1.0f;
    m_charStatus = QString();
    m_frequency = -1;
    m_channel = QString();
    m_deviceSetIndex = -1;
    m_channelIndex = -1;
    m_centerFrequency = -1;
    m_channelOffset = 0;
    m_duplicate = DuplicateNone;
    m_partial = false;
    m_protocolData.clear();
}

// The key of the duplicate detection of the receiver of the message (RadioPagingDedup): the frequency, the protocol,
// the network (polarity) and the address, which has the POCSAG function and the GSC address number. The receiver
// compares a message with the last messages of its key
QString MessageRecord::dedupKey() const
{
    return QString("%1|%2|%3|%4").arg(m_frequency).arg(m_protocol, polarityToString(m_polarity), m_address);
}

QStringList MessageRecord::addresses() const
{
    QStringList list;

    if (!m_address.isEmpty()) {
        list.append(m_address);
    }

    for (const QVariant& member : m_protocolData.value("group_capcodes").toList())
    {
        if (!list.contains(member.toString())) {
            list.append(member.toString());
        }
    }

    return list;
}

QJsonObject MessageRecord::toJson() const
{
    QJsonObject json;

    json.insert("schemaVersion", m_schemaVersion);
    json.insert("id", m_id);
    json.insert("timestamp", m_timestamp);
    json.insert("direction", directionToString(m_direction));
    json.insert("protocol", stringOrNull(m_protocol));
    json.insert("mode", stringOrNull(m_mode));
    json.insert("kind", kindToString(m_kind));
    json.insert("msgType", stringOrNull(m_msgType));
    json.insert("address", stringOrNull(m_address));
    json.insert("polarity", polarityToString(m_polarity));
    json.insert("speed", intOrNull(m_speed));
    json.insert("text", stringOrNull(m_text));
    json.insert("rawData", m_rawBits < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(QString::fromLatin1(m_rawData.toHex())));
    json.insert("rawBits", intOrNull(m_rawBits));
    json.insert("errorsCorrected", intOrNull(m_errorsCorrected));
    json.insert("errorsUncorrectable", intOrNull(m_errorsUncorrectable));
    json.insert("ber", m_ber < 0.0f ? QJsonValue(QJsonValue::Null) : QJsonValue((double) m_ber));
    json.insert("charStatus", stringOrNull(m_charStatus));
    json.insert("frequency", intOrNull(m_frequency));
    json.insert("channel", stringOrNull(m_channel));
    json.insert("deviceSetIndex", intOrNull(m_deviceSetIndex));
    json.insert("channelIndex", intOrNull(m_channelIndex));
    json.insert("centerFrequency", intOrNull(m_centerFrequency));
    json.insert("channelOffset", m_centerFrequency < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(m_channelOffset));
    json.insert("duplicate", duplicateToString(m_duplicate));
    json.insert("dedupKey", dedupKey());
    json.insert("partial", m_partial);
    json.insert("protocolData", QJsonObject::fromVariantMap(m_protocolData));

    return json;
}

bool MessageRecord::fromJson(const QJsonObject& json, MessageRecord& record)
{
    record.resetToDefaults();

    if (!json.contains("id") || !json.contains("timestamp") || !json.contains("protocol")) {
        return false;
    }

    record.m_id = readString(json, "id");
    record.m_timestamp = readInt(json, "timestamp", 0);
    record.m_direction = directionFromString(readString(json, "direction"));
    record.m_protocol = readString(json, "protocol");
    record.m_mode = readString(json, "mode");
    record.m_kind = kindFromString(readString(json, "kind"));
    record.m_msgType = readString(json, "msgType");
    record.m_address = readString(json, "address");
    record.m_polarity = polarityFromString(readString(json, "polarity"));
    record.m_speed = (int) readInt(json, "speed");
    record.m_text = readString(json, "text");
    record.m_rawBits = (int) readInt(json, "rawBits");
    if (record.m_rawBits >= 0) {
        record.m_rawData = QByteArray::fromHex(readString(json, "rawData").toLatin1());
    }
    record.m_errorsCorrected = (int) readInt(json, "errorsCorrected");
    record.m_errorsUncorrectable = (int) readInt(json, "errorsUncorrectable");
    QJsonValue ber = json.value("ber");
    record.m_ber = ber.isDouble() ? (float) ber.toDouble() : -1.0f;
    record.m_charStatus = readString(json, "charStatus");
    record.m_frequency = readInt(json, "frequency");
    // channelAlias: the name in the logs written before it was channel
    record.m_channel = json.contains("channel") ? readString(json, "channel") : readString(json, "channelAlias");
    record.m_deviceSetIndex = (int) readInt(json, "deviceSetIndex");
    record.m_channelIndex = (int) readInt(json, "channelIndex");
    record.m_centerFrequency = readInt(json, "centerFrequency");
    QJsonValue offset = json.value("channelOffset");
    record.m_channelOffset = offset.isDouble() ? (qint64) offset.toDouble() : 0;
    record.m_duplicate = duplicateFromString(readString(json, "duplicate"));
    record.m_partial = json.value("partial").toBool(false);
    record.m_protocolData = json.value("protocolData").toObject().toVariantMap();

    return true;
}

QByteArray MessageRecord::toJsonLine() const
{
    QByteArray line = QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
    line.append('\n');
    return line;
}

QStringList MessageRecord::csvHeader()
{
    return {
        "id", "timestamp", "direction", "protocol", "mode", "kind", "msgType",
        "address", "polarity", "speed", "text", "rawData", "rawBits",
        "errorsCorrected", "errorsUncorrectable", "ber", "charStatus",
        "frequency", "channel", "deviceSetIndex", "channelIndex",
        "centerFrequency", "channelOffset", "duplicate", "dedupKey", "partial", "protocolData"
    };
}

QString MessageRecord::toCSV() const
{
    QStringList fields;

    fields.append(CSV::escape(m_id));
    fields.append(QString::number(m_timestamp));
    fields.append(directionToString(m_direction));
    fields.append(csvString(m_protocol));
    fields.append(csvString(m_mode));
    fields.append(kindToString(m_kind));
    fields.append(csvString(m_msgType));
    fields.append(csvString(m_address));
    fields.append(polarityToString(m_polarity));
    fields.append(csvInt(m_speed));
    fields.append(csvString(m_text));
    fields.append(m_rawBits < 0 ? QString("") : QString::fromLatin1(m_rawData.toHex()));
    fields.append(csvInt(m_rawBits));
    fields.append(csvInt(m_errorsCorrected));
    fields.append(csvInt(m_errorsUncorrectable));
    fields.append(m_ber < 0.0f ? QString("") : QString::number(m_ber));
    fields.append(csvString(m_charStatus));
    fields.append(csvInt(m_frequency));
    fields.append(csvString(m_channel));
    fields.append(csvInt(m_deviceSetIndex));
    fields.append(csvInt(m_channelIndex));
    fields.append(csvInt(m_centerFrequency));
    fields.append(m_centerFrequency < 0 ? QString("") : QString::number(m_channelOffset));
    fields.append(duplicateToString(m_duplicate));
    fields.append(CSV::escape(dedupKey()));
    fields.append(m_partial ? "1" : "0");
    fields.append(CSV::escape(QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(m_protocolData)).toJson(QJsonDocument::Compact))));

    return fields.join(',');
}

bool MessageRecord::fromCSV(const QHash<QString, int>& columns, const QStringList& row, MessageRecord& record)
{
    // An empty field is what toCSV() writes for null: the same record as fromJson() makes from a JSON null
    auto field = [&](const char *name) {
        int col = columns.value(name, -1);
        return ((col >= 0) && (col < row.size())) ? row[col] : QString();
    };
    auto stringValue = [&](const char *name) {
        QString s = field(name);
        return s.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(s);
    };
    auto intValue = [&](const char *name) {
        bool ok;
        qint64 v = field(name).toLongLong(&ok);
        return ok ? QJsonValue(v) : QJsonValue(QJsonValue::Null);
    };

    QJsonObject json;

    if (field("id").isEmpty() || field("timestamp").isEmpty() || field("protocol").isEmpty()) {
        return false;
    }

    json.insert("id", stringValue("id"));
    json.insert("timestamp", intValue("timestamp"));
    json.insert("direction", field("direction"));
    json.insert("protocol", stringValue("protocol"));
    json.insert("mode", stringValue("mode"));
    json.insert("kind", field("kind"));
    json.insert("msgType", stringValue("msgType"));
    json.insert("address", stringValue("address"));
    json.insert("polarity", field("polarity"));
    json.insert("speed", intValue("speed"));
    json.insert("text", stringValue("text"));
    json.insert("rawData", stringValue("rawData"));
    json.insert("rawBits", intValue("rawBits"));
    json.insert("errorsCorrected", intValue("errorsCorrected"));
    json.insert("errorsUncorrectable", intValue("errorsUncorrectable"));

    bool ok;
    double ber = field("ber").toDouble(&ok);
    json.insert("ber", ok ? QJsonValue(ber) : QJsonValue(QJsonValue::Null));

    json.insert("charStatus", stringValue("charStatus"));
    json.insert("frequency", intValue("frequency"));
    json.insert("channel", columns.contains("channel") ? stringValue("channel") : stringValue("channelAlias"));
    json.insert("deviceSetIndex", intValue("deviceSetIndex"));
    json.insert("channelIndex", intValue("channelIndex"));
    json.insert("centerFrequency", intValue("centerFrequency"));

    qint64 offset = field("channelOffset").toLongLong(&ok);
    json.insert("channelOffset", ok ? QJsonValue(offset) : QJsonValue(QJsonValue::Null));

    json.insert("duplicate", field("duplicate"));
    json.insert("partial", field("partial") == "1");
    json.insert("protocolData",
        QJsonDocument::fromJson(field("protocolData").toUtf8()).object());

    return fromJson(json, record);
}

QString MessageRecord::directionToString(Direction direction)
{
    return direction == DirectionTX ? "tx" : "rx";
}

MessageRecord::Direction MessageRecord::directionFromString(const QString& string)
{
    return string == "tx" ? DirectionTX : DirectionRX;
}

QString MessageRecord::kindToString(Kind kind)
{
    switch (kind)
    {
    case KindEvent:
        return "event";
    case KindDiagnostic:
        return "diagnostic";
    case KindMessage:
    default:
        return "message";
    }
}

MessageRecord::Kind MessageRecord::kindFromString(const QString& string)
{
    if (string == "event") {
        return KindEvent;
    } else if (string == "diagnostic") {
        return KindDiagnostic;
    } else {
        return KindMessage;
    }
}

QString MessageRecord::polarityToString(Polarity polarity)
{
    switch (polarity)
    {
    case PolarityNormal:
        return "normal";
    case PolarityInverted:
        return "inverted";
    case PolarityUnknown:
    default:
        return "unknown";
    }
}

MessageRecord::Polarity MessageRecord::polarityFromString(const QString& string)
{
    if (string == "normal") {
        return PolarityNormal;
    } else if (string == "inverted") {
        return PolarityInverted;
    } else {
        return PolarityUnknown;
    }
}

QString MessageRecord::duplicateToString(Duplicate duplicate)
{
    switch (duplicate)
    {
    case DuplicateDup:
        return "dup";
    case DuplicateDupPlus:
        return "dup+";
    case DuplicateNone:
    default:
        return "none";
    }
}

MessageRecord::Duplicate MessageRecord::duplicateFromString(const QString& string)
{
    if (string == "dup") {
        return DuplicateDup;
    } else if (string == "dup+") {
        return DuplicateDupPlus;
    } else {
        return DuplicateNone;
    }
}
