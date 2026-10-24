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

#ifndef INCLUDE_FEATURE_AIRCRAFTTABLEMODELS_H_
#define INCLUDE_FEATURE_AIRCRAFTTABLEMODELS_H_

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QIcon>
#include <QHash>

#include "aircrafttracker.h"

// True while data is arriving for the aircraft or flight in this row
static const int ActiveRole = Qt::UserRole + 100;
// Columns whose display value does not sort the way the underlying value does - the OOOI
// times show "hh:mm" but must order across midnight - publish the real value here
static const int SortRole = Qt::UserRole + 101;

// Model/view for the aircraft table: rows are display snapshots from the
// tracker; icons are resolved lazily on the GUI side
// Filters a table down to the rows we are currently receiving data for, or to those we
// are not - the same model feeds both the live and the archive views
class ActiveFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
protected:
    // Sorting is on the display role, so a column showing a shortened value would sort
    // by its text. One that publishes SortRole is ordered by that instead.
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        const QVariant l = left.data(SortRole);
        if (l.isValid()) {
            return l.toDateTime() < right.data(SortRole).toDateTime();
        }
        return QSortFilterProxyModel::lessThan(left, right);
    }

public:
    ActiveFilterProxy(bool wantActive, QObject *parent = nullptr) :
        QSortFilterProxyModel(parent),
        m_wantActive(wantActive)
    {
        // A row number is a position, so every row below an insertion, a removal or a
        // re-sort is now numbered differently and the whole column has to be repainted.
        // Without this the numbers only refresh where the view happens to redraw.
        auto renumber = [this]()
        {
            if (rowCount() > 0) {
                emit headerDataChanged(Qt::Vertical, 0, rowCount() - 1);
            }
        };
        connect(this, &QAbstractItemModel::rowsInserted, this, renumber);
        connect(this, &QAbstractItemModel::rowsRemoved, this, renumber);
        connect(this, &QAbstractItemModel::layoutChanged, this, renumber);
        connect(this, &QAbstractItemModel::modelReset, this, renumber);
    }

    // Row numbers down the side, counting what is DISPLAYED rather than the underlying
    // rows. It has to be done here rather than by just showing the vertical header: the
    // source models return nothing for a vertical section, so the header would be blank,
    // and QSortFilterProxyModel's own implementation maps the section back to its SOURCE
    // row, so the numbers would jump about as soon as the table was sorted. Numbering the
    // view position means the top row is always 1, whatever the sort order.
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override
    {
        if (orientation == Qt::Vertical)
        {
            if (role == Qt::DisplayRole) {
                return section + 1;
            }
            return QVariant();
        }
        return QSortFilterProxyModel::headerData(section, orientation, role);
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override
    {
        const QModelIndex index = sourceModel()->index(row, 0, parent);
        return sourceModel()->data(index, ActiveRole).toBool() == m_wantActive;
    }

private:
    bool m_wantActive;
};

class AircraftTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Col {
        COL_ICAO,
        COL_REG,
        COL_TYPE,
        COL_FLIGHT,
        COL_AIRLINE,
        COL_SIDEVIEW,
        COL_COUNTRY,
        COL_LATITUDE,
        COL_LONGITUDE,
        COL_DISTANCE,
        COL_ALTITUDE,
        COL_HEADING,
        COL_SPEED,
        COL_PROTOCOLS,
        COL_MESSAGES,
        COL_LAST_SEEN,
        // Appended rather than placed beside the callsign where it belongs: the saved
        // column widths and order are stored by index, so inserting in the middle would
        // shift every one of them. Drag it where you want it.
        COL_FLIGHT_NO,
        COL_SELCAL,         // Appended for the same reason
        COL_COUNT
    };

    explicit AircraftTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void upsert(const QList<AircraftDisplay>& aircraft);
    void remove(const QList<quint64>& ids);
    void clear();
    const AircraftDisplay *aircraftAt(int row) const;
    const AircraftDisplay *aircraftById(quint64 id) const;
    //!< The model row holding this aircraft, or -1 if it has none
    int rowOf(quint64 id) const;

private:
    struct Row {
        AircraftDisplay m_display;
        bool m_iconsResolved = false;
        QIcon m_country;
        QIcon m_airline;
        QString m_airlineText;
        QIcon m_sideview;
        QString m_type;
    };
    QList<Row> m_rows;
    QHash<quint64, int> m_rowById;

    void resolveIcons(Row& row) const;
    void reindex(int from);
};

// Model/view for the flights table
class FlightTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    // In the order the table reads left to right: who and which aircraft, where it is
    // going, what it has sent, when it did the four things a flight does, when it was
    // heard, and last the two that are more diagnostic than operational.
    //
    // Columns used to be appended here rather than placed where they belonged, because
    // the saved widths and order are stored BY INDEX and inserting one shifted them all.
    // That is settled by moving the stored layout to its own settings keys, so a layout
    // saved before this reorder falls back to the defaults - which are this order - once,
    // rather than being applied to the wrong columns.
    enum Col {
        COL_FLIGHT,         // Callsign, which is what the aircraft transmits
        COL_FLIGHT_NO,      // Flight number, which is what a passenger is sold
        COL_REG,
        COL_FROM,
        COL_TO,
        COL_DOCS,
        COL_LS,             // Loadsheet
        COL_CL,             // Any clearance: oceanic, and cleared for departure
        COL_FP,             // Flight plan
        COL_OUT,            // OOOI: off the gate
        COL_OFF,            //       airborne
        COL_ON,             //       landed
        COL_IN,             //       on the gate
        COL_FIRST_SEEN,
        COL_LAST_SEEN,
        COL_PROTOCOLS,
        COL_ROUTE_SOURCE,   // Where From and To came from
        COL_COUNT
    };

    explicit FlightTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void upsert(const QList<FlightDisplay>& flights);
    void remove(const QList<quint64>& ids);
    void clear();
    const FlightDisplay *flightAt(int row) const;
    //!< The model row holding this flight, or -1 if it has none
    int rowOf(quint64 id) const;

private:
    QList<FlightDisplay> m_rows;
    QHash<quint64, int> m_rowById;

    void reindex(int from);
};

#endif // INCLUDE_FEATURE_AIRCRAFTTABLEMODELS_H_
