///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_ADSBAIRCRAFTTABLEMODEL_H
#define INCLUDE_ADSBAIRCRAFTTABLEMODEL_H

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QList>

#include "adsbdemodsettings.h"

struct Aircraft;

// Model for the table of aircraft.
//
// The aircraft own their cell values; this presents them as rows. Changes are
// coalesced: decoding marks cells dirty and flushDirty() emits dataChanged for them
// periodically, rather than the table being repainted and re-sorted on every frame,
// which is what a busy sky used to cost.
// Sorts on the raw cell values, with empty cells always ordered before values, so that
// every column treats them the same way
class ADSBAircraftSortProxy : public QSortFilterProxyModel {
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    // Row numbers count the rows as displayed, so they say how many aircraft there are
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

class ADSBAircraftTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    // Raw cell value, used for sorting, so formatted numbers still sort numerically
    static const int SortRole = Qt::UserRole;

    explicit ADSBAircraftTableModel(QObject *parent = nullptr);

    void setSettings(const ADSBDemodSettings *settings) { m_settings = settings; }

    void addAircraft(Aircraft *aircraft);
    void removeAircraft(Aircraft *aircraft);
    void clear();

    int rowForAircraft(const Aircraft *aircraft) const;
    Aircraft *aircraftAt(int row) const { return ((row >= 0) && (row < m_aircraft.size())) ? m_aircraft[row] : nullptr; }

    // Repaint the cells that have changed since the last call
    void flushDirty();
    // Everything needs redrawing, e.g. after a change of units
    void allDataChanged();
    void aircraftDataChanged(const Aircraft *aircraft);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    static const QString *columnNames();
    // The icons are 85x20 pixel images. On a scaled display Qt will not draw them
    // larger than that in device pixels, which leaves them undersized, so they are
    // scaled for the display when they are loaded
    static void setDevicePixelRatio(qreal dpr);
    static QVariant icon(const QString& path, const QSize& size);

private:
    QList<Aircraft *> m_aircraft;
    const ADSBDemodSettings *m_settings;
};

#endif // INCLUDE_ADSBAIRCRAFTTABLEMODEL_H
