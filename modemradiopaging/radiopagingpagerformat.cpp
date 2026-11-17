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

#include <algorithm>

#include <QDateTime>
#include <QUuid>
#include <QVariantMap>

#include "util/csv.h"

#include "radiopagingpagerformat.h"

bool RadioPagingPagerFormat::isCompletion(const MessageRecord& record)
{
    return record.m_protocolData.contains("voice_end") && record.m_text.isEmpty();
}

QStringList RadioPagingPagerFormat::fields(const MessageRecord& record)
{
    if ((record.m_kind != MessageRecord::KindMessage) || isCompletion(record)) {
        return QStringList();
    }

    const QVariantMap& d = record.m_protocolData;
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(record.m_timestamp);
    QString address = record.m_address;
    QString functionBits;

    if ((record.m_protocol == "pocsag") && d.contains("ric"))
    {
        address = QString("%1").arg(d.value("ric").toULongLong(), 7, 10, QChar('0'));
        functionBits = QString::number(d.value("function").toInt());
    }
    else if ((record.m_protocol == "flex") && d.contains("capcode"))
    {
        address = QString("%1").arg(d.value("capcode").toULongLong(), 9, 10, QChar('0'));
    }
    else if (d.contains("function"))
    {
        functionBits = QString::number(d.value("function").toInt());
    }

    QString alpha, numeric;
    bool isNumeric = record.m_msgType.contains("numeric") && !record.m_msgType.contains("alpha");

    (isNumeric ? numeric : alpha) = record.m_text;

    QString altType = d.value("alt_type").toString();
    QString altText = d.value("alt_text").toString();

    if (!altText.isEmpty())
    {
        if (isNumeric && altType.contains("alpha")) {
            alpha = altText;
        } else if (!isNumeric && altType.contains("numeric") && !altType.contains("alpha")) {
            numeric = altText;
        }
    }

    return {dateTime.date().toString(), dateTime.time().toString(), address, functionBits, alpha, numeric,
        record.m_protocol.toUpper(), record.m_msgType};
}

QByteArray RadioPagingPagerFormat::datagram(const MessageRecord& record)
{
    QByteArray datagram;

    for (const QString& text : fields(record))
    {
        datagram.append(text.toLatin1());
        datagram.append('\0');
    }

    return datagram;
}

QStringList RadioPagingPagerFormat::csvHeader()
{
    return csvColumns() + QStringList{"Baud", "Protocol", "Type", "Frame"};
}

QString RadioPagingPagerFormat::csvRow(const MessageRecord& record)
{
    QStringList f = fields(record);

    if (f.isEmpty()) {
        return QString();
    }

    QVariant frame = record.m_protocolData.value("frame");
    QStringList row = {
        f[0], f[1], f[2], f[3], CSV::escape(f[4]), CSV::escape(f[5]),
        // The Pager demodulator counts the codewords BCH could not correct as BCH parity errors, and those left with a
        // wrong parity bit as even parity errors: the uncorrectable errors are the first, none is the second. Unknown
        // (negative) is none
        QString("0"), QString::number(std::max(0, record.m_errorsUncorrectable)),
        QString::number(std::max(0, record.m_speed)), f[6], CSV::escape(f[7]), frame.isValid() ? frame.toString() : QString()
    };

    return row.join(',');
}

namespace {

// The record of the fields the Pager demodulator gives of a message
void setPagerMessage(MessageRecord& record, const QDateTime& dateTime, const QString& address, const QString& functionBits,
    const QString& alpha, const QString& numeric, const QString& protocol, const QString& type)
{
    record.resetToDefaults();
    record.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.m_timestamp = dateTime.toMSecsSinceEpoch();
    record.m_direction = MessageRecord::DirectionRX;
    record.m_kind = MessageRecord::KindMessage;
    record.m_protocol = protocol.toLower();
    record.m_address = address;

    // The type the Pager demodulator gives (FLEX: its message types), else the field that has the text
    QString lower = type.toLower();
    bool isNumeric = !numeric.isEmpty() && alpha.isEmpty();

    if (lower.startsWith("alpha")) {
        isNumeric = false;
    } else if (lower.contains("numeric")) {
        isNumeric = true;
    }

    record.m_msgType = lower.isEmpty() ? (isNumeric ? QString("numeric") : QString("alphanumeric")) : lower;
    record.m_text = isNumeric ? numeric : alpha;

    // The reading in the other field is the alternative
    QString other = isNumeric ? alpha : numeric;

    if (!other.isEmpty() && (other != record.m_text))
    {
        record.m_protocolData.insert("alt_type", isNumeric ? "alphanumeric" : "numeric");
        record.m_protocolData.insert("alt_text", other);
    }

    bool ok;
    int function = functionBits.toInt(&ok);

    if (ok) {
        record.m_protocolData.insert("function", function);
    }
}

} // namespace

bool RadioPagingPagerFormat::fromDatagram(const QByteArray& datagram, qint64 now, MessageRecord& record)
{
    QList<QByteArray> fields = datagram.split('\0');

    // Eight fields, each ended by a NUL
    if ((fields.size() != 9) || !fields.last().isEmpty()) {
        return false;
    }

    QStringList f;

    for (int i = 0; i < 8; i++) {
        f.append(QString::fromLatin1(fields[i]));
    }

    QDateTime dateTime(QDate::fromString(f[0]), QTime::fromString(f[1]));

    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromMSecsSinceEpoch(now);
    }
    if (f[6].isEmpty() || f[2].isEmpty()) {
        return false;
    }

    setPagerMessage(record, dateTime, f[2], f[3], f[4], f[5], f[6], f[7]);
    return true;
}

QStringList RadioPagingPagerFormat::csvColumns()
{
    return {"Date", "Time", "Address", "Function Bits", "Alpha", "Numeric", "Even Parity Errors", "BCH Parity Errors"};
}

bool RadioPagingPagerFormat::fromCsvRow(const QHash<QString, int>& columns, const QStringList& row, MessageRecord& record)
{
    auto field = [&](const char *name) {
        int col = columns.value(name, -1);
        return ((col >= 0) && (col < row.size())) ? row[col] : QString();
    };

    for (const QString& name : csvColumns())
    {
        if (!columns.contains(name)) {
            return false;
        }
    }

    QString address = field("Address");

    if (address.isEmpty() || (row.size() < columns.size())) {
        return false;
    }

    QDateTime dateTime(QDate::fromString(field("Date")), QTime::fromString(field("Time")));

    if (!dateTime.isValid()) {
        return false;
    }

    // Logs written before the Pager demodulator recorded the protocol are POCSAG
    QString protocol = field("Protocol").isEmpty() ? QString("POCSAG") : field("Protocol");
    setPagerMessage(record, dateTime, address, field("Function Bits"), field("Alpha"), field("Numeric"), protocol, field("Type"));

    int baud = field("Baud").toInt();

    if ((baud > 0) && (protocol.toUpper() == "POCSAG")) {
        record.m_speed = baud;
        record.m_mode = QString::number(baud);
    }

    // The parity errors of the Pager demodulator are codewords it could not correct (BCH) or whose parity bit is wrong
    record.m_errorsUncorrectable = field("Even Parity Errors").toInt() + field("BCH Parity Errors").toInt();

    if (!field("Frame").isEmpty()) {
        record.m_protocolData.insert("frame", field("Frame"));
    }

    return true;
}
