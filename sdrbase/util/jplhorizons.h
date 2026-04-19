///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_JPLHORIZONS_H
#define INCLUDE_JPLHORIZONS_H

#include <QtCore>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkDiskCache;

// NASA JPL Horizons Ephemeris
class SDRBASE_API JPLHorizons : public QObject
{
    Q_OBJECT
protected:
    JPLHorizons();

public:
    struct Ephemeris {
        QDateTime m_dateTime;
        float m_ra;
        float m_dec;
    };

    struct BodyID {
        int m_id;
        QString m_name;
        QString m_designation;
    };

    static JPLHorizons* create();

    ~JPLHorizons();
    void getMajorBodiesList();
    bool getBodyId(const QString& body, int &id);

public slots:
    void getData(const QString& target, QDateTime startDateTime, QDateTime stopDateTime, double latitude, double longitude, double altitudeKM);

private slots:
    void handleReply(QNetworkReply* reply);

signals:
    void ephemeridesUpdated(const QString &target, const QList<JPLHorizons::Ephemeris>& ephemerides);  // Called when new data available.
    void majorBodiesUpdated(const QHash<QString, BodyID>& bodies);

private:
    void handleEphemerides(const QByteArray& bytes);
    void handleMajorBodiesList(const QByteArray& bytes);

    QNetworkAccessManager *m_networkManager;
    QNetworkDiskCache *m_cache;

    static QMutex m_mutex;
    static QHash<QString, BodyID> m_bodies;
    static const QString m_majorBodiesURL;

};

#endif /* INCLUDE_JPLHORIZONS_H */

