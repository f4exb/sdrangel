///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_WAYPOINTS_H
#define INCLUDE_WAYPOINTS_H

#include <QString>
#include <QFile>
#include <QHash>

#include <stdio.h>
#include <string.h>

#include "export.h"

#include "util/units.h"
#include "util/httpdownloadmanager.h"

#define WAYPOINTS_URL     "https://github.com/srcejon/aviationwaypoints/waypoints.csv"

// Aviation waypoints
struct SDRBASE_API Waypoint {

    QString m_name;             // Typically 5 characters
    float m_latitude;
    float m_longitude;

    static QHash<QString, Waypoint *> *readCSV(const QString &filename);
};

class SDRBASE_API Waypoints : public QObject {
    Q_OBJECT

public:
    Waypoints(QObject* parent = nullptr);
    ~Waypoints();

    void downloadWaypoints();

    static const Waypoint* findWayPoint(const QString& name);
    static QSharedPointer<const QHash<QString, Waypoint *>> getWaypoints();

private:
    HttpDownloadManager m_dlm;

    static QSharedPointer<QHash<QString, Waypoint *>> m_waypoints;

    static QDateTime m_waypointsModifiedDateTime;

    static QHash<QString, Waypoint *> *readWaypoints();

    static QString getDataDir();
    static QString getWaypointsFilename();
    static QDateTime getWaypointsModifiedDateTime();

public slots:
    void downloadFinished(const QString& filename, bool success);

signals:
    void downloadingURL(const QString& url);
    void downloadError(const QString& error);
    void downloadWaypointsFinished();

};

#endif // INCLUDE_WAYPOINTS_H
