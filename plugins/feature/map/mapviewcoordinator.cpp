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

#include <cmath>

#include <QTimer>

#include "util/profiler.h"

#include "mapviewcoordinator.h"
#include "mapgui.h"

MapViewCoordinator::MapViewCoordinator(MapGUI *gui) :
    m_gui(gui),
    m_zoomLevel(0.0),
    m_appliedZoom(0.0),
    m_applied(false),
    m_applyQueued(false)
{
}

void MapViewCoordinator::viewChanged(const QGeoRectangle& visibleRect, double zoomLevel)
{
    m_visibleRect = visibleRect;
    m_zoomLevel = zoomLevel;

    // A wheel notch changes the zoom and the centre, and a drag reports a change per
    // rendered frame, so wait until the end of this turn of the event loop and act once
    if (!m_applyQueued)
    {
        m_applyQueued = true;
        QTimer::singleShot(0, this, &MapViewCoordinator::apply);
    }
}

void MapViewCoordinator::apply()
{
    PROFILER_START()
    m_applyQueued = false;

    if (!m_visibleRect.isValid() || !materialChange())
    {
        PROFILER_STOP("Map view apply")
        return;
    }

    m_appliedRect = inflate(m_visibleRect);
    m_appliedZoom = m_zoomLevel;
    m_applied = true;

    m_gui->applyMapView(m_appliedRect, m_visibleRect, m_zoomLevel);
    PROFILER_STOP("Map view apply")
}

// Only worth redoing the filters once what is on screen has left the area they were
// given, or the zoom has actually changed
bool MapViewCoordinator::materialChange() const
{
    if (!m_applied) {
        return true;
    }
    if (std::fabs(m_zoomLevel - m_appliedZoom) > m_zoomEpsilon) {
        return true;
    }

    return !m_appliedRect.contains(m_visibleRect);
}

QGeoRectangle MapViewCoordinator::inflate(const QGeoRectangle& rect)
{
    QGeoRectangle inflated = rect;

    inflated.setWidth(std::min(360.0, rect.width() * m_margin));
    inflated.setHeight(std::min(180.0, rect.height() * m_margin));

    return inflated;
}
