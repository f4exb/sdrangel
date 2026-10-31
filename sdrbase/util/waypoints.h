///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_WAYPOINTS_H
#define INCLUDE_WAYPOINTS_H

#include <QString>
#include <QStringList>
#include <QFile>
#include <QMultiHash>
#include <QGeoCoordinate>

#include <stdio.h>
#include <string.h>

#include "export.h"

#include "util/units.h"
#include "util/httpdownloadmanager.h"

#define WAYPOINTS_URL     "https://sdrangel.org/downloads/waypoints.zip"

// Aviation waypoints
struct SDRBASE_API Waypoint {

    QString m_name;             // Typically 5 characters
    float m_latitude;
    float m_longitude;

    static QMultiHash<QString, Waypoint *> *readCSV(const QString &filename);
    static QMultiHash<QString, Waypoint *> *readBGL(const QString& path, QStringList *errors = nullptr);
};

class SDRBASE_API Waypoints : public QObject {
    Q_OBJECT

public:
    Waypoints(QObject* parent = nullptr);
    ~Waypoints();

    void downloadWaypoints();

    static const Waypoint* findWayPoint(const QString& name, const QStringList& nearby = QStringList());
    // The same choice made against positions rather than names. Needed wherever the
    // fixes around a name are things no waypoint database holds - an oceanic clearance
    // surrounds its named entry point with bare latitudes and longitudes - because the
    // name form can only score against names it can itself look up, and scores nothing
    // at all when it cannot.
    static const Waypoint* findWayPoint(const QString& name, const QList<QGeoCoordinate>& nearby);
    static QSharedPointer<const QMultiHash<QString, Waypoint *>> getWaypoints();

private:
    HttpDownloadManager m_dlm;

    static QSharedPointer<QMultiHash<QString, Waypoint *>> m_waypoints;

    static QDateTime m_waypointsModifiedDateTime;

    static QMultiHash<QString, Waypoint *> *readWaypoints();

    static QString getDataDir();
    static QString getWaypointsZipFilename();
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
