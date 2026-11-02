///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <limits>

#if (QT_VERSION < QT_VERSION_CHECK(6, 6, 0))
#include <QtGui/private/qzipreader_p.h>
#else
#include <QtCore/private/qzipreader_p.h>
#endif

#include "waypoints.h"
#include "bglwaypointreader.h"
#include "csv.h"

namespace {

double distance(const Waypoint *a, const Waypoint *b)
{
    const double lat1 = Units::degreesToRadians(static_cast<double>(a->m_latitude));
    const double lat2 = Units::degreesToRadians(static_cast<double>(b->m_latitude));
    const double deltaLat = lat2 - lat1;
    const double deltaLon = Units::degreesToRadians(static_cast<double>(b->m_longitude - a->m_longitude));
    const double sinHalfDeltaLat = std::sin(deltaLat / 2.0);
    const double sinHalfDeltaLon = std::sin(deltaLon / 2.0);
    const double haversine = sinHalfDeltaLat * sinHalfDeltaLat
        + std::cos(lat1) * std::cos(lat2) * sinHalfDeltaLon * sinHalfDeltaLon;

    return 2.0 * std::asin(std::sqrt(std::min(1.0, haversine)));
}

}

QMultiHash<QString, Waypoint *> *Waypoint::readCSV(const QString &filename)
{
    QMultiHash<QString, Waypoint *> *waypoints = new QMultiHash<QString, Waypoint *>();
    QFile file(filename);

    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        QString error;

        QStringList cols;
        while(CSV::readRow(in, &cols))
        {
            if ((cols.size() < 3) || cols[0].startsWith('#')) {
                continue;
            }

            bool latitudeOk;
            bool longitudeOk;
            const float latitude = cols[1].toFloat(&latitudeOk);
            const float longitude = cols[2].toFloat(&longitudeOk);
            if (cols[0].isEmpty() || !latitudeOk || !longitudeOk) {
                continue;
            }

            Waypoint *waypoint = new Waypoint();
            waypoint->m_name = cols[0];
            waypoint->m_latitude = latitude;
            waypoint->m_longitude = longitude;
            waypoints->insert(waypoint->m_name, waypoint);
        }

        file.close();
    }
    else
    {
        qDebug() << "Waypoint::readCSV: Could not open " << filename << " for reading.";
    }
    return waypoints;
}

QMultiHash<QString, Waypoint *> *Waypoint::readBGL(const QString& path, QStringList *errors)
{
    return BGLWaypointReader::read(path, errors);
}

QSharedPointer<QMultiHash<QString, Waypoint *>> Waypoints::m_waypoints;

QDateTime Waypoints::m_waypointsModifiedDateTime;

Waypoints::Waypoints(QObject *parent) :
    QObject(parent)
{
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &Waypoints::downloadFinished);
}

Waypoints::~Waypoints()
{
    disconnect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &Waypoints::downloadFinished);
}

QString Waypoints::getDataDir()
{
    // Get directory to store app data in
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    return locations[0];
}

QString Waypoints::getWaypointsFilename()
{
    return getDataDir() + "/" + "waypoints.csv";
}

QString Waypoints::getWaypointsZipFilename()
{
    return getDataDir() + "/" + "waypoints.zip";
}

void Waypoints::downloadWaypoints()
{
    QString filename = getWaypointsZipFilename();
    QString urlString = WAYPOINTS_URL;
    QUrl dbURL(urlString);
    qDebug() << "Waypoints::downloadWaypoints: Downloading " << urlString;
    emit downloadingURL(urlString);
    m_dlm.download(dbURL, filename);
}

void Waypoints::downloadFinished(const QString& filename, bool success)
{
    if (!success)
    {
        qWarning() << "Waypoints::downloadFinished: Failed to download: " << filename;
        emit downloadError(QString("Failed to download: %1").arg(filename));
    }
    else if (filename == getWaypointsZipFilename())
    {
        // Extract waypoints.csv from the downloaded archive
        QZipReader reader(filename);

        if (reader.extractAll(getDataDir()))
        {
            emit downloadWaypointsFinished();
        }
        else
        {
            qWarning() << "Waypoints::downloadFinished: Failed to extract files from " << filename;
            emit downloadError(QString("Failed to extract files from %1").arg(filename));
        }
    }
    else
    {
        qDebug() << "Waypoints::downloadFinished: Unexpected filename: " << filename;
        emit downloadError(QString("Unexpected filename: %1").arg(filename));
    }
}

// Read waypoints
QMultiHash<QString, Waypoint *> *Waypoints::readWaypoints()
{
    return Waypoint::readCSV(getWaypointsFilename());
}

QSharedPointer<const QMultiHash<QString, Waypoint *>> Waypoints::getWaypoints()
{
    QDateTime filesDateTime = getWaypointsModifiedDateTime();

    if (!m_waypoints || (filesDateTime > m_waypointsModifiedDateTime))
    {
        // Using shared pointer, so old object, if it exists, will be deleted, when no longer used
        m_waypoints = QSharedPointer<QMultiHash<QString, Waypoint *>>(readWaypoints());
        m_waypointsModifiedDateTime = filesDateTime;
    }
    return m_waypoints;
}

// Gets the date and time the waypoint file was most recently modified
QDateTime Waypoints::getWaypointsModifiedDateTime()
{
    QFileInfo fileInfo(getWaypointsFilename());
    return fileInfo.lastModified();
}

// Find a waypoint by name. If the name is ambiguous, use nearby waypoint names
// to select the candidate closest to the surrounding route.
const Waypoint *Waypoints::findWayPoint(const QString& name, const QList<QGeoCoordinate>& nearby)
{
    const QList<Waypoint *> matches = m_waypoints->values(name);

    if (matches.isEmpty()) {
        return nullptr;
    }

    if ((matches.size() == 1) || nearby.isEmpty()) {
        return matches.constFirst();
    }

    const Waypoint *closest = matches.constFirst();
    double closestDistance = std::numeric_limits<double>::max();

    for (const Waypoint *candidate : matches)
    {
        const QGeoCoordinate at(candidate->m_latitude, candidate->m_longitude);
        double totalDistance = 0.0;
        int nearbyCount = 0;

        for (const QGeoCoordinate& position : nearby)
        {
            if (!position.isValid()) {
                continue;
            }
            totalDistance += at.distanceTo(position);
            nearbyCount++;
        }

        if ((nearbyCount > 0) && ((totalDistance / nearbyCount) < closestDistance))
        {
            closest = candidate;
            closestDistance = totalDistance / nearbyCount;
        }
    }

    return closest;
}

const Waypoint *Waypoints::findWayPoint(const QString& name, const QStringList& nearby)
{
    const QList<Waypoint *> matches = m_waypoints->values(name);

    if (matches.isEmpty()) {
        return nullptr;
    }

    if ((matches.size() == 1) || nearby.isEmpty()) {
        return matches.constFirst();
    }

    const Waypoint *closest = matches.constFirst();
    double closestDistance = std::numeric_limits<double>::max();

    for (const Waypoint *candidate : matches)
    {
        double totalDistance = 0.0;
        int nearbyCount = 0;

        for (const QString& nearbyName : nearby)
        {
            // The target name cannot provide any disambiguating information.
            if (nearbyName == name) {
                continue;
            }

            const QList<Waypoint *> nearbyMatches = m_waypoints->values(nearbyName);

            if (!nearbyMatches.isEmpty())
            {
                double nearestDistance = std::numeric_limits<double>::max();

                for (const Waypoint *nearbyWaypoint : nearbyMatches) {
                    nearestDistance = std::min(nearestDistance, distance(candidate, nearbyWaypoint));
                }

                totalDistance += nearestDistance;
                nearbyCount++;
            }
        }

        if ((nearbyCount > 0) && ((totalDistance / nearbyCount) < closestDistance))
        {
            closest = candidate;
            closestDistance = totalDistance / nearbyCount;
        }
    }

    return closest;
}
