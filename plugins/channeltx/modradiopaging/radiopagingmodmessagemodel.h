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

#ifndef INCLUDE_RADIOPAGINGMODMESSAGEMODEL_H
#define INCLUDE_RADIOPAGINGMODMESSAGEMODEL_H

#include <vector>

#include <QAbstractTableModel>
#include <QHash>
#include <QString>

#include "util/messagerecord.h"

// The messages of the transmitter, one row each from the time it is queued to its last event, in the order they were
// queued. The events of the channel (queued, on air, sent, dropped, a copy sent) update the row of the message in
// place: rows do not move. All the messages not finished are kept, and the last finished ones (retention); the
// oldest finished go first. The view draws the rows it shows: a queue of tens of thousands costs as much as a few.
class RadioPagingModMessageModel : public QAbstractTableModel
{
public:
    enum Column {
        ColId,
        ColState,
        ColFirst,           //!< Date and time of the first event (queued)
        ColLast,            //!< Time of the last event
        ColEvents,          //!< Number of events
        ColProtocol,
        ColAddress,
        ColType,
        ColMessage,
        ColInfo,
        ColCount
    };

    enum State {
        Waiting,
        OnAir,
        Sent,
        Dropped
    };

    static const int m_stateRole = Qt::UserRole + 20;   //!< The State of the row (filters)

    explicit RadioPagingModMessageModel(QObject *parent = nullptr);

    //!< A record of the channel and its state (queued, on air, sent, dropped). A copy of a message sent before
    //!< (retransmission_of) is an event of the row of that message
    void update(const MessageRecord& record, const QString& state);
    //!< Finished messages kept
    void setRetention(int rows);
    void clear();

    const MessageRecord& record(int row) const { return m_rows[row].m_record; }
    State state(int row) const { return m_rows[row].m_state; }
    //!< The row of the message being sent last, -1 if none
    int onAirRow() const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    static State stateOf(const QString& state);
    static QString stateName(State state);

private:
    struct Row
    {
        MessageRecord m_record;
        State m_state = Waiting;
        qint64 m_firstMs = 0;
        qint64 m_lastMs = 0;
        int m_events = 0;
        QString m_text;         //!< The message on one line
        QString m_info;         //!< The chips of its protocol fields: plain, HTML, tool tip
        QString m_infoHtml;
        QString m_infoTip;
        QString m_eventsTip;    //!< The history, one event a line
    };

    std::vector<Row> m_rows;
    QHash<QString, int> m_rowOf;    //!< Row of each record id
    int m_finished = 0;             //!< Rows sent or dropped
    int m_retention = 1000;
    int m_onAir = -1;               //!< The last row put on air (onAirRow)

    static void fill(Row& row);
    void evict();
    void reindex();
};

#endif // INCLUDE_RADIOPAGINGMODMESSAGEMODEL_H
