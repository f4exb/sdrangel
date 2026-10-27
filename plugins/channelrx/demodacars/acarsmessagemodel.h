///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                      //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_ACARSMESSAGEMODEL_H
#define INCLUDE_ACARSMESSAGEMODEL_H

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <QList>
#include <QLocale>

#include "acarsdemodworker.h"

// Columns of the received messages table. At file scope rather than inside the model,
// because the GUI refers to them constantly and qualifying every one would not make
// any of it clearer.
enum MessageCol {
    MESSAGE_COL_DATETIME,
    MESSAGE_COL_DIR,
    MESSAGE_COL_GS,
    MESSAGE_COL_GS_DECODE,
    MESSAGE_COL_ADDRESS,
    MESSAGE_COL_ACK,
    MESSAGE_COL_LABEL,
    MESSAGE_COL_LABEL_DECODE,
    MESSAGE_COL_ID,
    MESSAGE_COL_ORIGIN,
    MESSAGE_COL_ORIGIN_DECODE,
    MESSAGE_COL_MSG_NUM,
    MESSAGE_COL_BLOCK_SEQUENCE,
    MESSAGE_COL_FLIGHT,
    MESSAGE_COL_TEXT,
    MESSAGE_COL_TEXT_DECODE,
    MESSAGE_COL_ATC,
    MESSAGE_COL_LATITUDE,
    MESSAGE_COL_LONGITUDE,
    MESSAGE_COL_ALTITUDE,
    MESSAGE_COL_HEX,
    // Appended rather than placed next to Dir where it belongs, because the saved column
    // widths and order are stored by index - putting it in the middle would shift every
    // one of them. Drag it where you want it; the order is remembered.
    MESSAGE_COL_PROTOCOL,
    MESSAGE_COL_RATE,
    MESSAGE_COL_COUNT
};

// The received messages table.
//
// This was a QTableWidget until 2026-08-26, and the reason it is not any more is that
// hiding a row there is O(rows already present): QTableView::setRowHidden goes to
// QHeaderView::setSectionHidden, which invalidates the section start positions and then
// walks every section. Measured over 22000 rows with alternate rows hidden - which is
// what an Aero P channel produces, since its housekeeping alternates with real traffic -
// the cost of adding a row rose from 19 us to 390 us and was still climbing. Batching
// the hides and disabling updates around them changed nothing; the cost is inside
// setSectionHidden itself.
//
// A filtered row simply does not exist in a proxy's mapping, so there is no section to
// hide. The same measurement through QSortFilterProxyModel is FLAT at 2.8 us a row -
// faster than the QTableWidget managed even with no hiding at all (8.7 us), because
// nothing allocates 22 QTableWidgetItems per row any more.
//
// Values that depend on a setting rather than on the message - the date format, the
// country-specific mode decode - are computed in data() rather than written into every
// row, so changing the setting is one dataChanged rather than a walk over the table.
class AcarsMessageModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit AcarsMessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void addRow(const AcarsRowEvent& event);
    void clear();

    bool isValidRow(int row) const { return (row >= 0) && (row < m_rows.size()); }
    // The whole decoded frame, so a handler can read what it needs instead of scraping
    // it back out of the cell text
    const AcarsRowEvent& event(int row) const { return m_rows.at(row).m_event; }

    void setShowDate(bool showDate);

private:
    struct Row {
        AcarsRowEvent m_event;
    };

    QList<Row> m_rows;
    bool m_showDate;
    QLocale m_locale;

    QString dateTimeText(const QDateTime& dateTime) const;
    static QString modeDecode(const AcarsRowEvent& event);
    void columnChanged(int column);
};

// Hides frames carrying no usable information, applies the filter pattern, and sorts by
// the underlying value rather than by the displayed text - which matters most for the
// date and time, where the locale's date string does not sort chronologically and the
// date can be hidden altogether.
class AcarsMessageFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit AcarsMessageFilter(QObject *parent = nullptr);

    void setHideNoInfo(bool hide);
    void setFilter(int column, const QString& pattern);

    // Row numbers down the side. They count the rows on show rather than the rows
    // received, so the last one is how many messages are in the table.
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

    // Qt 6.10 brought these, deprecating invalidateFilter() in their favour. On
    // anything older they stand in for it - see acarsmessagemodel.cpp
#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
    void beginFilterChange();
    void endFilterChange();
#endif

private:
    bool m_hideNoInfo;
    int m_filterColumn;
    QRegularExpression m_filterRe;
    bool m_filterValid;
};

#endif // INCLUDE_ACARSMESSAGEMODEL_H
