///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_UTIL_OURAIRPORTSDB_H
#define INCLUDE_UTIL_OURAIRPORTSDB_H

#include <QString>
#include <QMutex>
#include <QDateTime>
#include <QHash>

#include <stdio.h>
#include <string.h>

#include "util/csv.h"
#include "util/httpdownloadmanager.h"
#include "export.h"

#define AIRPORTS_URL "https://davidmegginson.github.io/ourairports-data/airports.csv"
#define AIRPORT_FREQUENCIES_URL "https://davidmegginson.github.io/ourairports-data/airport-frequencies.csv"

class SDRBASE_API AirportInformation {

public:
    enum AirportType {
        Small,
        Medium,
        Large,
        Heliport
    };

    struct FrequencyInformation {
        QString m_type;
        QString m_description;
        float m_frequency;      // In MHz
    };

    int m_id;
    QString m_ident;
    AirportType m_type;
    QString m_name;
    float m_latitude;
    float m_longitude;
    float m_elevation;
    QVector<FrequencyInformation *> m_frequencies;

    ~AirportInformation();
    QString getImageName() const;

};

class SDRBASE_API OurAirportsDB : public QObject {
    Q_OBJECT

public:

    OurAirportsDB(QObject *parent=nullptr);
    ~OurAirportsDB();

    void downloadAirportInformation();

    static QSharedPointer<const QHash<int, AirportInformation *>> getAirportsById();
    static QSharedPointer<const QHash<QString, AirportInformation *>> getAirportsByIdent();

private:
    HttpDownloadManager m_dlm;

    static QSharedPointer<QHash<int, AirportInformation *>> m_airportsById;
    static QSharedPointer<QHash<QString, AirportInformation *>> m_airportsByIdent;
    static QDateTime m_modifiedDateTime;

    static QMutex m_mutex;

    static QString getDataDir();

    static void readDB();

    // Read OurAirport's airport CSV file
    // See comments for readOSNDB
    static QHash<int, AirportInformation *> *readAirportsDB(const QString &filename);

    // Create hash table using ICAO identifier as key
    static QHash<QString, AirportInformation *> *identHash(QHash<int, AirportInformation *> *in);

    // Read OurAirport's airport frequencies CSV file
    static bool readFrequenciesDB(const QString &filename, QHash<int, AirportInformation *> *airportInfo);

    static QString trimQuotes(const QString s);

    static QString getAirportDBFilename();
    static QString getAirportFrequenciesDBFilename();

private slots:
    void downloadFinished(const QString& filename, bool success);

signals:
    void downloadingURL(const QString& url);
    void downloadError(const QString& error);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadAirportInformationFinished();

};

#endif

