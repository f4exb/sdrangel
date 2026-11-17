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

#include <QColor>
#include <QDateTime>
#include <QIcon>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "gui/pagingmessagetable.h"

#include "radiopagingmodmessagemodel.h"

namespace {

// An event of the history of a record: its time (ms since the epoch, 0 if not known) and its name
qint64 eventMs(const QVariant& event)
{
    const QDateTime time = QDateTime::fromString(event.toMap().value("time").toString(), Qt::ISODateWithMs);
    return time.isValid() ? time.toMSecsSinceEpoch() : 0;
}

} // namespace

RadioPagingModMessageModel::RadioPagingModMessageModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

RadioPagingModMessageModel::State RadioPagingModMessageModel::stateOf(const QString& state)
{
    if (state == "on air") {
        return OnAir;
    } else if (state == "sent") {
        return Sent;
    } else if (state == "dropped") {
        return Dropped;
    } else {
        return Waiting;
    }
}

QString RadioPagingModMessageModel::stateName(State state)
{
    static const char *names[] = {"waiting", "on air", "sent", "dropped"};
    return names[state];
}

// The values of the row shown, from its record: made once a change, not at each paint
void RadioPagingModMessageModel::fill(Row& row)
{
    const MessageRecord& rec = row.m_record;
    const QVariantList events = rec.m_protocolData.value("events").toList();

    row.m_events = events.size();
    row.m_firstMs = events.isEmpty() ? rec.m_timestamp : eventMs(events.first());
    row.m_lastMs = events.isEmpty() ? rec.m_timestamp : eventMs(events.last());
    row.m_text = PagingMessageTable::recordText(rec).simplified();
    PagingMessageTable::recordInfo(rec, row.m_info, row.m_infoHtml, row.m_infoTip);

    QStringList lines;

    for (const QVariant& event : events)
    {
        const QVariantMap entry = event.toMap();
        const QDateTime time = QDateTime::fromMSecsSinceEpoch(eventMs(event));
        lines.append(QString("%1  %2").arg(time.toString("yyyy-MM-dd HH:mm:ss.zzz"), entry.value("event").toString()));
    }

    row.m_eventsTip = lines.join('\n');
}

void RadioPagingModMessageModel::update(const MessageRecord& record, const QString& state)
{
    // A copy: the history of its message, which it carries whole
    const QString copyOf = record.m_protocolData.value("retransmission_of").toString();

    if (!copyOf.isEmpty())
    {
        auto it = m_rowOf.constFind(copyOf);

        if (it == m_rowOf.constEnd()) {
            return;     // Its message is gone from the table
        }

        Row& row = m_rows[it.value()];
        row.m_record.m_protocolData.insert("events", record.m_protocolData.value("events"));
        fill(row);
        emit dataChanged(index(it.value(), 0), index(it.value(), ColCount - 1));
        return;
    }

    const State newState = stateOf(state);
    auto it = m_rowOf.constFind(record.m_id);

    if (it != m_rowOf.constEnd())
    {
        Row& row = m_rows[it.value()];
        const bool wasFinished = (row.m_state == Sent) || (row.m_state == Dropped);
        const bool finished = (newState == Sent) || (newState == Dropped);

        row.m_record = record;
        row.m_state = newState;
        fill(row);

        if (finished && !wasFinished) {
            m_finished++;
        }
        if (newState == OnAir) {
            m_onAir = it.value();
        }

        emit dataChanged(index(it.value(), 0), index(it.value(), ColCount - 1));
        evict();
        return;
    }

    Row row;
    row.m_record = record;
    row.m_state = newState;
    fill(row);

    const int at = (int) m_rows.size();
    beginInsertRows(QModelIndex(), at, at);
    m_rows.push_back(row);
    m_rowOf.insert(record.m_id, at);
    endInsertRows();

    if (newState == OnAir) {
        m_onAir = at;
    }

    if ((newState == Sent) || (newState == Dropped))
    {
        m_finished++;
        evict();
    }
}

void RadioPagingModMessageModel::setRetention(int rows)
{
    m_retention = std::max(0, rows);
    evict();
}

void RadioPagingModMessageModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_rowOf.clear();
    m_finished = 0;
    m_onAir = -1;
    endResetModel();
}

int RadioPagingModMessageModel::onAirRow() const
{
    // The last row put on air, while it is (looked for at each tick: not a scan of the rows)
    return ((m_onAir >= 0) && (m_onAir < (int) m_rows.size()) && (m_rows[m_onAir].m_state == OnAir)) ? m_onAir : -1;
}

// The finished rows over the retention go, the oldest first, by batches (the rows are indexed again after each).
// They are at the top: the messages are sent about in the order they were queued. Finished rows after a message
// that does not finish (a page waiting for a group) wait until there are twice as many, then all go but the last
void RadioPagingModMessageModel::evict()
{
    const int batch = 100;

    if (m_finished <= m_retention + batch) {
        return;
    }

    int count = 0;

    while ((count < (int) m_rows.size()) && (count < m_finished - m_retention)
        && ((m_rows[count].m_state == Sent) || (m_rows[count].m_state == Dropped))) {
        count++;
    }

    if (count > 0)
    {
        beginRemoveRows(QModelIndex(), 0, count - 1);
        m_rows.erase(m_rows.begin(), m_rows.begin() + count);
        m_finished -= count;
        m_onAir -= count;
        reindex();
        endRemoveRows();
        return;
    }

    if (m_finished <= 2 * m_retention + batch) {
        return;
    }

    beginResetModel();
    int drop = m_finished - m_retention;
    std::vector<Row> kept;
    kept.reserve(m_rows.size());

    for (Row& row : m_rows)
    {
        if ((drop > 0) && ((row.m_state == Sent) || (row.m_state == Dropped)))
        {
            drop--;
            m_finished--;
            continue;
        }

        kept.push_back(std::move(row));
    }

    m_rows.swap(kept);
    reindex();
    m_onAir = -1;

    for (int row = (int) m_rows.size() - 1; (row >= 0) && (m_onAir < 0); row--)
    {
        if (m_rows[row].m_state == OnAir) {
            m_onAir = row;
        }
    }
    endResetModel();
}

void RadioPagingModMessageModel::reindex()
{
    m_rowOf.clear();
    m_rowOf.reserve((int) m_rows.size());

    for (int row = 0; row < (int) m_rows.size(); row++) {
        m_rowOf.insert(m_rows[row].m_record.m_id, row);
    }
}

int RadioPagingModMessageModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : (int) m_rows.size();
}

int RadioPagingModMessageModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QVariant RadioPagingModMessageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation != Qt::Horizontal) || (section < 0) || (section >= ColCount)) {
        return QVariant();
    }
    if (role == Qt::DisplayRole)
    {
        static const char *names[] = {"#", "State", "First event", "Last", "Events", "Protocol", "Address", "Type", "Message", "Info"};
        return QString(names[section]);
    }
    if (role != Qt::ToolTipRole) {
        return QVariant();
    }

    static const char *tips[] = {
        "Id of the message in this transmitter",
        "waiting: queued, not sent yet\non air: its blocks are being sent\nsent: sent completely (and its copies)\n"
            "dropped: cleared, aborted, or it could not be sent",
        "Date and time of its first event: when it was queued",
        "Time of its last event",
        "Its events: queued, on air, sent or dropped, each copy sent.\nThe tool tip of a row lists them with their times; "
            "double click a row for the record",
        "Protocol and speed or mode",
        "Address (capcode) of the pager",
        "Type of the message",
        "The message, on one line. A voice message: click it to play the voice sent",
        "The protocol fields where it went and how: frame, batch, phase, message number, fragments..."
    };

    return QString(tips[section]);
}

QVariant RadioPagingModMessageModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || (index.row() >= (int) m_rows.size())) {
        return QVariant();
    }

    const Row& row = m_rows[index.row()];
    const MessageRecord& rec = row.m_record;

    switch (role)
    {
    case m_stateRole:
        return (int) row.m_state;
    case Qt::BackgroundRole:
        return (row.m_state == OnAir) ? QVariant(QColor("#f0dc8c")) : QVariant();
    case Qt::ForegroundRole:
        if (row.m_state == OnAir) {
            return QColor("#141414");
        }
        return (row.m_state == Dropped) ? QVariant(QColor("#909090")) : QVariant();
    case Qt::ToolTipRole:
        if ((index.column() == ColMessage) && !row.m_text.isEmpty()) {
            return row.m_text;
        } else if (index.column() == ColInfo) {
            return row.m_infoTip;
        } else {
            return row.m_eventsTip;
        }
    case Qt::DecorationRole:
        if ((index.column() == ColMessage) && !rec.m_protocolData.value("voice_file").toString().isEmpty()) {
            return QIcon(":/sound_on.png");
        }
        return QVariant();
    case PagingMessageTable::m_htmlRole:
        if (index.column() == ColType) {
            return PagingMessageTable::typeChipHtml(rec.m_msgType);
        } else if (index.column() == ColInfo) {
            return row.m_infoHtml;
        }
        return QVariant();
    case Qt::DisplayRole:
        break;
    default:
        return QVariant();
    }

    switch (index.column())
    {
    case ColId:
        return rec.m_protocolData.value("tx_id");
    case ColState:
        return stateName(row.m_state);
    case ColFirst:
        return QDateTime::fromMSecsSinceEpoch(row.m_firstMs).toString("yyyy-MM-dd HH:mm:ss");
    case ColLast:
        return QDateTime::fromMSecsSinceEpoch(row.m_lastMs).toString("HH:mm:ss");
    case ColEvents:
        return row.m_events;
    case ColProtocol:
        return rec.m_mode.isEmpty() ? rec.m_protocol.toUpper() : QString("%1 %2").arg(rec.m_protocol.toUpper(), rec.m_mode);
    case ColAddress:
        return rec.m_address;
    case ColType:
        return rec.m_msgType;
    case ColMessage:
        return row.m_text;
    case ColInfo:
        return row.m_info;
    default:
        return QVariant();
    }
}
