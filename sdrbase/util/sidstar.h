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

#ifndef INCLUDE_SIDSTAR_H
#define INCLUDE_SIDSTAR_H

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>

#include "export.h"
#include "util/httpdownloadmanager.h"

#define SIDSTAR_URL "https://www.sdrangel.org/downloads/sidstar.zip"

// One ordered waypoint in a standard instrument departure or arrival procedure.
struct SDRBASE_API SidStarPoint
{
    QString m_country;
    QString m_airport;
    QString m_procedureType;
    QString m_procedure;
    int m_sequence = 0;
    QString m_waypoint;
    float m_latitude = 0.0f;
    float m_longitude = 0.0f;
    bool m_hasMinimumAltitude = false;
    int m_minimumAltitudeFt = 0;
    bool m_hasMaximumAltitude = false;
    int m_maximumAltitudeFt = 0;
};

// Aviation standard instrument departures (SIDs) and standard terminal arrival
// routes (STARs).
class SDRBASE_API SidStar : public QObject
{
    Q_OBJECT

public:
    SidStar(QObject *parent = nullptr);
    ~SidStar();

    void downloadSidStar();

    // Returns the procedure's points in sequence order. procedureType may be
    // "SID" or "STAR" when an airport uses the same name for both; without
    // it, both groups are returned in type and then sequence order.
    static QList<SidStarPoint> getWaypoints(
        const QString& procedure,
        const QString& airport,
        const QString& procedureType = QString()
    );

private:
    typedef QHash<QString, QList<SidStarPoint>> ProcedureMap;

    HttpDownloadManager m_dlm;

    static QSharedPointer<ProcedureMap> m_procedures;
    static QDateTime m_sidStarModifiedDateTime;
    static QMutex m_mutex;

    static ProcedureMap *readCSV(const QString& filename);
    static QString procedureKey(const QString& airport, const QString& procedure);
    static QString getDataDir();
    static QString getSidStarZipFilename();
    static QString getSidStarFilename();
    static QDateTime getSidStarModifiedDateTime();

private slots:
    void downloadFinished(const QString& filename, bool success);

signals:
    void downloadingURL(const QString& url);
    void downloadError(const QString& error);
    void downloadSidStarFinished();
};

#endif // INCLUDE_SIDSTAR_H
