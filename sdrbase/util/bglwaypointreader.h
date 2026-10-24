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

#ifndef INCLUDE_BGLWAYPOINTREADER_H
#define INCLUDE_BGLWAYPOINTREADER_H

#include <QMultiHash>
#include <QString>
#include <QStringList>

#include "export.h"

struct Waypoint;

// Reads FSX/P3D/MSFS waypoint sections from either one BGL file or all BGL
// files below a directory. Successful reads are cached as CSV in the application
// data cache and invalidated when any source filename, size or modification date
// changes. The caller owns the returned hash and waypoints.
class SDRBASE_API BGLWaypointReader
{
public:
    static QMultiHash<QString, Waypoint *> *read(const QString& path, QStringList *errors = nullptr);
};

#endif // INCLUDE_BGLWAYPOINTREADER_H
