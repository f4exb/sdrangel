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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_FEATURE_MAPVIEWCOORDINATOR_H_
#define INCLUDE_FEATURE_MAPVIEWCOORDINATOR_H_

#include <QObject>
#include <QGeoRectangle>

class MapGUI;

// Decides when the 2D map's filters and tracks need to be recalculated as the map is
// panned and zoomed.
//
// QML reports a change on every rendered frame of a drag, and each one used to re-filter
// every item in the model - which includes every airport and navaid in the world - four
// times over. Two things stop that here:
//
//  - calls arriving in the same turn of the event loop are coalesced in to one
//  - the filters are given a rectangle larger than what is on screen, and are only
//    updated once the visible area leaves it. Items that come in to view during a small
//    pan are already in the model, so nothing appears late
class MapViewCoordinator : public QObject
{
    Q_OBJECT

public:

    MapViewCoordinator(MapGUI *gui);

    // Called from QML whenever the map is panned or zoomed
    Q_INVOKABLE void viewChanged(const QGeoRectangle& visibleRect, double zoomLevel);

    // How much larger than the visible area the filters are given, as a proportion
    static constexpr double m_margin = 1.2;
    // Zoom has to change by more than this before the filters are updated
    static constexpr double m_zoomEpsilon = 0.05;

private slots:

    void apply();

private:

    bool materialChange() const;
    static QGeoRectangle inflate(const QGeoRectangle& rect);

    MapGUI *m_gui;
    QGeoRectangle m_visibleRect;    // What QML last told us is on screen
    double m_zoomLevel;
    QGeoRectangle m_appliedRect;    // Inflated rectangle the filters are currently using
    double m_appliedZoom;
    bool m_applied;                 // Whether anything has been applied yet
    bool m_applyQueued;
};

#endif // INCLUDE_FEATURE_MAPVIEWCOORDINATOR_H_
