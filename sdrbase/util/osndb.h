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

#ifndef INCLUDE_OSNDB_H
#define INCLUDE_OSNDB_H

#include <QString>
#include <QHash>
#include <QList>
#include <QDebug>
#include <QIcon>
#include <QMutex>
#include <QStandardPaths>
#include <QDateTime>

#include <stdio.h>
#include <string.h>

#include "util/csv.h"
#include "util/httpdownloadmanager.h"
#include "export.h"

#define OSNDB_URL "https://opensky-network.org/datasets/metadata/aircraftDatabase.zip"

struct SDRBASE_API AircraftInformation {

    int m_icao;
    QString m_registration;
    QString m_manufacturerName;
    QString m_model;
    QString m_owner;
    QString m_operator;
    QString m_operatorICAO;
    QString m_registered;

    static void init()
    {
        QMutexLocker locker(&m_mutex);
        if (!m_prefixMap)
        {
            // Read registration prefix to country map
            m_prefixMap = CSV::hash(":/flags/regprefixmap.csv");
            // Read operator air force to military map
            m_militaryMap = CSV::hash(":/flags/militarymap.csv");
        }
    }


    // Get flag based on registration
    QString getFlag() const;

    static QString getAirlineIconPath(const QString &operatorICAO);

    // Try to find an airline logo based on ICAO
    static QIcon *getAirlineIcon(const QString &operatorICAO);

    static QString getFlagIconPath(const QString &country);

    // Try to find an flag logo based on a country
    static QIcon *getFlagIcon(const QString &country);

private:

    static QHash<QString, QIcon *> m_airlineIcons; // Hashed on airline ICAO
    static QHash<QString, bool> m_airlineMissingIcons; // Hash containing which ICAOs we don't have icons for
    static QHash<QString, QIcon *> m_flagIcons;    // Hashed on country
    static QHash<QString, QString> *m_prefixMap;   // Registration to country (flag name)
    static QHash<QString, QString> *m_militaryMap;   // Operator airforce to military (flag name)
    static QMutex m_mutex;

};

class SDRBASE_API OsnDB : public QObject {
    Q_OBJECT

public:

    OsnDB(QObject* parent=nullptr);
    ~OsnDB();

    void downloadAircraftInformation();

    static QSharedPointer<const QHash<int, AircraftInformation *>> getAircraftInformation();
    static QSharedPointer<const QHash<QString, AircraftInformation *>> getAircraftInformationByReg();

    static QString getOSNDBZipFilename()
    {
        return getDataDir() + "/aircraftDatabase.zip";
    }

    static QString getOSNDBFilename()
    {
        return getDataDir() + "/aircraftDatabase.csv";
    }

    static QString getDataDir()
    {
        // Get directory to store app data in (aircraft & airport databases and user-definable icons)
        QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
        // First dir is writable
        return locations[0];
    }

private:
    HttpDownloadManager m_dlm;

    static QSharedPointer<const QHash<int, AircraftInformation *>> m_aircraftInformation;
    static QSharedPointer<const QHash<QString, AircraftInformation *>> m_aircraftInformationByReg;
    static QDateTime m_modifiedDateTime;

    // Write a reduced size and validated version of the DB, so it loads quicker
    static bool writeFastDB(const QString &filename, const QHash<int, AircraftInformation *> *aircraftInfo);

    // Read smaller CSV file with no validation. Takes about 0.5s instead of 2s.
    static QHash<int, AircraftInformation *> *readFastDB(const QString &filename);

    // Read OpenSky Network CSV file
    // This is large and contains lots of data we don't want, so we convert to
    // a smaller version to speed up loading time
    // Note that we use C file functions rather than QT, as these are ~30% faster
    // and the QT version seemed to occasionally crash
    static QHash<int, AircraftInformation *> *readOSNDB(const QString &filename);

    static QString getFastDBFilename()
    {
        return getDataDir() + "/aircraftDatabaseFast.csv";
    }

    // Create hash table using registration as key
    static QHash<QString, AircraftInformation *> *registrationHash(const QHash<int, AircraftInformation *> *in);

private slots:
    void downloadFinished(const QString& filename, bool success);

signals:
    void downloadingURL(const QString& url);
    void downloadError(const QString& error);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadAircraftInformationFinished();

};

#endif

