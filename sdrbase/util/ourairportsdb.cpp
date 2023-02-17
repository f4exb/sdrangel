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

#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QList>
#include <QDebug>
#include <QLocale>
#include <QStandardPaths>

#include "util/ourairportsdb.h"

QMutex OurAirportsDB::m_mutex;
QSharedPointer<QHash<int, AirportInformation *>> OurAirportsDB::m_airportsById;
QSharedPointer<QHash<QString, AirportInformation *>> OurAirportsDB::m_airportsByIdent;
QDateTime OurAirportsDB::m_modifiedDateTime;

AirportInformation::~AirportInformation()
{
    qDeleteAll(m_frequencies);
}

QString AirportInformation::getImageName() const
{
    switch (m_type)
    {
    case AirportType::Large:
        return "airport_large.png";
    case AirportType::Medium:
        return "airport_medium.png";
    case AirportType::Heliport:
        return "heliport.png";
    default:
        return "airport_small.png";
    }
}

OurAirportsDB::OurAirportsDB(QObject *parent) :
    QObject(parent)
{
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &OurAirportsDB::downloadFinished);
}

OurAirportsDB::~OurAirportsDB()
{
    disconnect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &OurAirportsDB::downloadFinished);
}

void OurAirportsDB::downloadAirportInformation()
{
    // Download airport database
    QString urlString = AIRPORTS_URL;
    QUrl dbURL(urlString);
    qDebug() << "OurAirportsDB::downloadAirportInformation: Downloading " << urlString;
    emit downloadingURL(urlString);
    QNetworkReply *reply = m_dlm.download(dbURL, OurAirportsDB::getAirportDBFilename());
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesRead, qint64 totalBytes) {
        emit downloadProgress(bytesRead, totalBytes);
    });
}

void OurAirportsDB::downloadFinished(const QString& filename, bool success)
{
    if (!success)
    {
        qWarning() << "OurAirportsDB::downloadFinished: Failed to download: " << filename;
        emit downloadError(QString("Failed to download: %1").arg(filename));
    }
    else if (filename == OurAirportsDB::getAirportDBFilename())
    {
        // Now download airport frequencies
        QString urlString = AIRPORT_FREQUENCIES_URL;
        QUrl dbURL(urlString);
        qDebug() << "OurAirportsDB::downloadFinished: Downloading " << urlString;
        emit downloadingURL(urlString);
        QNetworkReply *reply = m_dlm.download(dbURL, OurAirportsDB::getAirportFrequenciesDBFilename());
        connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesRead, qint64 totalBytes) {
            emit downloadProgress(bytesRead, totalBytes);
        });
    }
    else if (filename == OurAirportsDB::getAirportFrequenciesDBFilename())
    {
        emit downloadAirportInformationFinished();
    }
    else
    {
        qDebug() << "OurAirportsDB::downloadFinished: Unexpected filename: " << filename;
        emit downloadError(QString("Unexpected filename: %1").arg(filename));
    }
}

QSharedPointer<const QHash<int, AirportInformation *>> OurAirportsDB::getAirportsById()
{
    QMutexLocker locker(&m_mutex);

    readDB();
    return m_airportsById;
}

QSharedPointer<const QHash<QString, AirportInformation *>> OurAirportsDB::getAirportsByIdent()
{
    QMutexLocker locker(&m_mutex);

    readDB();
    return m_airportsByIdent;
}

void OurAirportsDB::readDB()
{
    QFileInfo airportDBFileInfo(getAirportDBFilename());
    QDateTime airportDBModifiedDateTime = airportDBFileInfo.lastModified();

    if (!m_airportsById || (airportDBModifiedDateTime > m_modifiedDateTime))
    {
        // Using shared pointer, so old object, if it exists, will be deleted, when no longer user
        m_airportsById = QSharedPointer<QHash<int, AirportInformation *>>(OurAirportsDB::readAirportsDB(getAirportDBFilename()));
        if (m_airportsById != nullptr)
        {
            OurAirportsDB::readFrequenciesDB(getAirportFrequenciesDBFilename(), m_airportsById.get());
            m_airportsByIdent = QSharedPointer<QHash<QString, AirportInformation *>>(identHash(m_airportsById.get()));
        }

        m_modifiedDateTime = airportDBModifiedDateTime;
    }
}

QString OurAirportsDB::getDataDir()
{
    // Get directory to store app data in
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    return locations[0];
}

QString OurAirportsDB::getAirportDBFilename()
{
    return getDataDir() + "/airportDatabase.csv";
}

QString OurAirportsDB::getAirportFrequenciesDBFilename()
{
    return getDataDir() + "/airportFrequenciesDatabase.csv";
}

QString OurAirportsDB::trimQuotes(const QString s)
{
    if (s.startsWith('\"') && s.endsWith('\"')) {
        return s.mid(1, s.size() - 2);
    } else {
        return s;
    }
}

// Read OurAirport's airport CSV file
// See comments for readOSNDB
QHash<int, AirportInformation *> *OurAirportsDB::readAirportsDB(const QString &filename)
{
    int cnt = 0;
    QHash<int, AirportInformation *> *airportInfo = nullptr;

    // Column numbers used for the data as of 2020/10/28
    int idCol = 0;
    int identCol = 1;
    int typeCol = 2;
    int nameCol = 3;
    int latitudeCol = 4;
    int longitudeCol = 5;
    int elevationCol = 6;

    qDebug() << "OurAirportsDB::readAirportsDB: " << filename;

    FILE *file;
    QByteArray utfFilename = filename.toUtf8();
    QLocale cLocale(QLocale::C);
    if ((file = fopen(utfFilename.constData(), "r")) != NULL)
    {
        char row[2048];

        if (fgets(row, sizeof(row), file))
        {
            airportInfo = new QHash<int, AirportInformation *>();
            airportInfo->reserve(70000);

            // Read header
            int idx = 0;
            char *p = strtok(row, ",");
            while (p != NULL)
            {
                if (!strcmp(p, "id"))
                    idCol = idx;
                else if (!strcmp(p, "ident"))
                    identCol = idx;
                else if (!strcmp(p, "type"))
                    typeCol = idx;
                else if (!strcmp(p, "name"))
                    nameCol = idx;
                else if (!strcmp(p, "latitude_deg"))
                    latitudeCol = idx;
                else if (!strcmp(p, "longitude_deg"))
                    longitudeCol = idx;
                else if (!strcmp(p, "elevation_ft"))
                    elevationCol = idx;
                p = strtok(NULL, ",");
                idx++;
            }
            // Read data
            while (fgets(row, sizeof(row), file))
            {
                int id = 0;
                char *idString = NULL;
                char *ident = NULL;
                size_t identLen = 0;
                char *type = NULL;
                size_t typeLen = 0;
                char *name = NULL;
                size_t nameLen = 0;
                float latitude = 0.0f;
                char *latitudeString = NULL;
                size_t latitudeLen = 0;
                float longitude = 0.0f;
                char *longitudeString = NULL;
                size_t longitudeLen = 0;
                float elevation = 0.0f;
                char *elevationString = NULL;
                size_t elevationLen = 0;

                p = strtok(row, ",");
                idx = 0;
                while (p != NULL)
                {
                    // Read strings, stripping quotes
                    if (idx == idCol)
                    {
                        idString = p;
                        idString[strlen(idString)] = '\0';
                        id = strtol(idString, NULL, 10);
                    }
                    else if (idx == identCol)
                    {
                        ident = p+1;
                        identLen = strlen(ident)-1;
                        ident[identLen] = '\0';
                    }
                    else if (idx == typeCol)
                    {
                        type = p+1;
                        typeLen = strlen(type)-1;
                        type[typeLen] = '\0';
                    }
                    else if (idx == nameCol)
                    {
                        name = p+1;
                        nameLen = strlen(name)-1;
                        name[nameLen] = '\0';
                    }
                    else if (idx == latitudeCol)
                    {
                        latitudeString = p;
                        latitudeLen = strlen(latitudeString)-1;
                        latitudeString[latitudeLen] = '\0';
                        latitude = cLocale.toFloat(latitudeString);
                    }
                    else if (idx == longitudeCol)
                    {
                        longitudeString = p;
                        longitudeLen = strlen(longitudeString)-1;
                        longitudeString[longitudeLen] = '\0';
                        longitude = cLocale.toFloat(longitudeString);
                    }
                    else if (idx == elevationCol)
                    {
                        elevationString = p;
                        elevationLen = strlen(elevationString)-1;
                        elevationString[elevationLen] = '\0';
                        elevation = cLocale.toFloat(elevationString);
                    }
                    p = strtok(NULL, ",");
                    idx++;
                }

                // Only create the entry if we have some interesting data
                if (((latitude != 0.0f) || (longitude != 0.0f)) && (type && strcmp(type, "closed")))
                {
                    AirportInformation *airport = new AirportInformation();
                    airport->m_id = id;
                    airport->m_ident = QString(ident);
                    if (!strcmp(type, "small_airport")) {
                        airport->m_type = AirportInformation::AirportType::Small;
                    } else if (!strcmp(type, "medium_airport")) {
                        airport->m_type = AirportInformation::AirportType::Medium;
                    } else if (!strcmp(type, "large_airport")) {
                        airport->m_type = AirportInformation::AirportType::Large;
                    } else if (!strcmp(type, "heliport")) {
                        airport->m_type = AirportInformation::AirportType::Heliport;
                    }
                    airport->m_name = QString(name);
                    airport->m_latitude = latitude;
                    airport->m_longitude = longitude;
                    airport->m_elevation = elevation;
                    airportInfo->insert(id, airport);
                    cnt++;
                }
            }
        }
        fclose(file);
    }
    else
        qDebug() << "OurAirportsDB::readAirportsDB: Failed to open " << filename;

    qDebug() << "OurAirportsDB::readAirportsDB: Read " << cnt << " airports";

    return airportInfo;
}

// Create hash table using ICAO identifier as key
QHash<QString, AirportInformation *> *OurAirportsDB::identHash(QHash<int, AirportInformation *> *in)
{
    QHash<QString, AirportInformation *> *out = new QHash<QString, AirportInformation *>();
    QHashIterator<int, AirportInformation *> i(*in);
    while (i.hasNext())
    {
        i.next();
        AirportInformation *info = i.value();
        out->insert(info->m_ident, info);
    }
    return out;
}

// Read OurAirport's airport frequencies CSV file
bool OurAirportsDB::readFrequenciesDB(const QString &filename, QHash<int, AirportInformation *> *airportInfo)
{
    int cnt = 0;

    // Column numbers used for the data as of 2020/10/28
    int airportRefCol = 1;
    int typeCol = 3;
    int descriptionCol = 4;
    int frequencyCol = 5;

    qDebug() << "OurAirportsDB::readFrequenciesDB: " << filename;

    QFile file(filename);
    if (file.open(QIODevice::ReadOnly))
    {
        QList<QByteArray> colNames;
        int idx;

        // Read header
        if (!file.atEnd())
        {
            QByteArray row = file.readLine().trimmed();
            colNames = row.split(',');
            // Work out which columns the data is in, based on the headers
            idx = colNames.indexOf("airport_ref");
            if (idx >= 0)
                 airportRefCol = idx;
            idx = colNames.indexOf("type");
            if (idx >= 0)
                 typeCol = idx;
            idx = colNames.indexOf("descrption");
            if (idx >= 0)
                 descriptionCol = idx;
            idx = colNames.indexOf("frequency_mhz");
            if (idx >= 0)
                 frequencyCol = idx;
        }
        // Read data
        while (!file.atEnd())
        {
            QByteArray row = file.readLine();
            QList<QByteArray> cols = row.split(',');

            bool ok = false;
            int airportRef = cols[airportRefCol].toInt(&ok, 10);
            if (ok)
            {
                if (airportInfo->contains(airportRef))
                {
                    QString type = trimQuotes(cols[typeCol]);
                    QString description = trimQuotes(cols[descriptionCol]);
                    float frequency = cols[frequencyCol].toFloat();

                    AirportInformation::FrequencyInformation *frequencyInfo = new AirportInformation::FrequencyInformation();
                    frequencyInfo->m_type = type;
                    frequencyInfo->m_description = description;
                    frequencyInfo->m_frequency = frequency;
                    airportInfo->value(airportRef)->m_frequencies.append(frequencyInfo);
                    cnt++;
                }
            }
        }
        file.close();
    }
    else
    {
        qDebug() << "Failed to open " << filename << " " << file.errorString();
        return false;
    }

    qDebug() << "OurAirportsDB::readFrequenciesDB: - read " << cnt << " airports";

    return true;
}

