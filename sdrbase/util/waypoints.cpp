///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "waypoints.h"
#include "csv.h"

QHash<QString, Waypoint *> *Waypoint::readCSV(const QString &filename)
{
    QHash<QString, Waypoint *> *waypoints = new QHash<QString, Waypoint *>();
    QFile file(filename);

    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        QString error;

        QStringList cols;
        while(CSV::readRow(in, &cols))
        {
            Waypoint *waypoint = new Waypoint();
            waypoint->m_name = cols[0];
            waypoint->m_latitude = cols[1].toFloat();
            waypoint->m_longitude = cols[2].toFloat();
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

QSharedPointer<QHash<QString, Waypoint *>> Waypoints::m_waypoints;

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

void Waypoints::downloadWaypoints()
{
    QString filename = getWaypointsFilename();
    QString urlString = WAYPOINTS_URL;
    QUrl dbURL(urlString);
    qDebug() << "Waypoints::downloadWaypoints: Downloading " << urlString;
    emit downloadingURL(urlString);
    m_dlm.download(dbURL, filename);
}

void Waypoints::downloadFinished(const QString& filename, bool success)
{
    if (!success) {
        qDebug() << "Waypoints::downloadFinished: Failed: " << filename;
    }

    if (filename == getWaypointsFilename())
    {
        emit downloadWaypointsFinished();
    }
    else
    {
        qDebug() << "Waypoints::downloadFinished: Unexpected filename: " << filename;
        emit downloadError(QString("Unexpected filename: %1").arg(filename));
    }
}

// Read waypoints
QHash<QString, Waypoint *> *Waypoints::readWaypoints()
{
    return Waypoint::readCSV(getWaypointsFilename());
}

QSharedPointer<const QHash<QString, Waypoint *>> Waypoints::getWaypoints()
{
    QDateTime filesDateTime = getWaypointsModifiedDateTime();

    if (!m_waypoints || (filesDateTime > m_waypointsModifiedDateTime))
    {
        // Using shared pointer, so old object, if it exists, will be deleted, when no longer used
        m_waypoints = QSharedPointer<QHash<QString, Waypoint *>>(readWaypoints());
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

// Find a waypoint by name
const Waypoint *Waypoints::findWayPoint(const QString& name)
{
    if (m_waypoints->contains(name)) {
        return m_waypoints->value(name);
    } else {
        return nullptr;
    }
}
