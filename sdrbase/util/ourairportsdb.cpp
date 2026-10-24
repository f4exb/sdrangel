///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Daniele Forsi <iu5hkx@gmail.com>                           //
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

// The file has 19 columns today and gains one occasionally, so this is the ceiling
// rather than the count - a row with more simply keeps the ones that fit
#define MAX_CSV_FIELDS 32

QMutex OurAirportsDB::m_mutex;
QSharedPointer<QHash<int, AirportInformation *>> OurAirportsDB::m_airportsById;
QSharedPointer<QHash<QString, AirportInformation *>> OurAirportsDB::m_airportsByIdent;
QSharedPointer<QHash<QString, AirportInformation *>> OurAirportsDB::m_airportsByIata;
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

QSharedPointer<const QHash<QString, AirportInformation *>> OurAirportsDB::getAirportsByIata()
{
    QMutexLocker locker(&m_mutex);

    readDB();
    return m_airportsByIata;
}

void OurAirportsDB::readDB()
{
    QFileInfo airportDBFileInfo(getAirportDBFilename());
    QDateTime airportDBModifiedDateTime = airportDBFileInfo.lastModified();

    if (!m_airportsById || (airportDBModifiedDateTime > m_modifiedDateTime))
    {
        QHash<int, AirportInformation *> *airportInfo = OurAirportsDB::readAirportsDB(getAirportDBFilename());

        if (airportInfo)
        {
            // Using shared pointer, so old object, if it exists, will be deleted when no longer used
            QSharedPointer<QHash<int, AirportInformation *>> airportsById(
                airportInfo,
                [](QHash<int, AirportInformation *> *airportInfo) {
                    qDeleteAll(*airportInfo);
                    delete airportInfo;
                }
            );
            OurAirportsDB::readFrequenciesDB(getAirportFrequenciesDBFilename(), airportsById.get());
            QSharedPointer<QHash<QString, AirportInformation *>> airportsByIdent(
                identHash(airportsById.get()),
                [airportsById](QHash<QString, AirportInformation *> *index) { delete index; }
            );
            QSharedPointer<QHash<QString, AirportInformation *>> airportsByIata(
                iataHash(airportsById.get()),
                [airportsById](QHash<QString, AirportInformation *> *index) { delete index; }
            );

            // Replace all three indexes together only after the new database was loaded successfully.
            m_airportsById = airportsById;
            m_airportsByIdent = airportsByIdent;
            m_airportsByIata = airportsByIata;
            m_modifiedDateTime = airportDBModifiedDateTime;
        }
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
// One CSV row into its fields, in place, keeping the empty ones.
//
// strtok cannot do this: it treats a run of delimiters as one, so a row with an empty
// field loses it and every column after it shifts left. 83,575 of the 83,000-odd rows in
// this file have at least one - "no",,,"00AA" is an airport with neither an ICAO nor an
// IATA code - so anything read from beyond the first empty field came out as its
// neighbour. It went unnoticed because everything read here used to live in the first
// seven columns, before any of them.
//
// Quotes are respected, since a name may contain a comma. The fields keep their quotes;
// the caller strips them as it already did.
static int splitCsvRow(char *row, char **fields, int maxFields)
{
    int count = 0;
    char *p = row;

    while (count < maxFields)
    {
        fields[count++] = p;

        bool quoted = false;
        while (*p && (quoted || ((*p != ',') && (*p != '\n') && (*p != '\r'))))
        {
            if (*p == '"') {
                quoted = !quoted;
            }
            p++;
        }
        const bool more = (*p == ',');
        *p = '\0';
        if (!more) {
            break;
        }
        p++;
    }
    return count;
}

// A quoted field without its quotes, and an empty string for a field that has none
static QString csvField(const char *field)
{
    if (!field || !*field) {
        return QString();
    }
    QString value = QString(field);
    if (value.startsWith('"') && value.endsWith('"') && (value.size() >= 2)) {
        value = value.mid(1, value.size() - 2);
    }
    return value;
}

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
    int iataCol = 13;

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
            char *headerFields[MAX_CSV_FIELDS];
            const int headerCount = splitCsvRow(row, headerFields, MAX_CSV_FIELDS);
            for (int idx = 0; idx < headerCount; idx++)
            {
                const QString name = csvField(headerFields[idx]);
                if (name == "id")
                    idCol = idx;
                else if (name == "ident")
                    identCol = idx;
                else if (name == "type")
                    typeCol = idx;
                else if (name == "name")
                    nameCol = idx;
                else if (name == "latitude_deg")
                    latitudeCol = idx;
                else if (name == "longitude_deg")
                    longitudeCol = idx;
                else if (name == "elevation_ft")
                    elevationCol = idx;
                else if (name == "iata_code")
                    iataCol = idx;
            }
            // Read data
            char *fields[MAX_CSV_FIELDS];
            while (fgets(row, sizeof(row), file))
            {
                const int count = splitCsvRow(row, fields, MAX_CSV_FIELDS);

                auto value = [&fields, count](int column)
                {
                    return ((column >= 0) && (column < count)) ? csvField(fields[column])
                                                               : QString();
                };

                const QString type = value(typeCol);
                const QString latitudeString = value(latitudeCol);
                const QString longitudeString = value(longitudeCol);
                const float latitude = cLocale.toFloat(latitudeString);
                const float longitude = cLocale.toFloat(longitudeString);

                // Only create the entry if we have some interesting data
                if (((latitude != 0.0f) || (longitude != 0.0f)) && (type != "closed"))
                {
                    AirportInformation *airport = new AirportInformation();
                    airport->m_id = value(idCol).toInt();
                    airport->m_ident = value(identCol);
                    airport->m_iata = value(iataCol);
                    if (type == "small_airport") {
                        airport->m_type = AirportInformation::AirportType::Small;
                    } else if (type == "medium_airport") {
                        airport->m_type = AirportInformation::AirportType::Medium;
                    } else if (type == "large_airport") {
                        airport->m_type = AirportInformation::AirportType::Large;
                    } else if (type == "heliport") {
                        airport->m_type = AirportInformation::AirportType::Heliport;
                    }
                    airport->m_name = value(nameCol);
                    airport->m_latitude = latitude;
                    airport->m_longitude = longitude;
                    airport->m_elevation = cLocale.toFloat(value(elevationCol));
                    const int id = airport->m_id;
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

// Create hash table using the three letter IATA code as key
//
// Only the airports that have one, which is a small minority - the file holds every
// airstrip and heliport, and an IATA code belongs to somewhere with scheduled service.
// Where two claim the same code the larger wins: they are not quite unique in the data,
// and a loadsheet means the airline one.
QHash<QString, AirportInformation *> *OurAirportsDB::iataHash(QHash<int, AirportInformation *> *in)
{
    QHash<QString, AirportInformation *> *out = new QHash<QString, AirportInformation *>();
    QHashIterator<int, AirportInformation *> i(*in);
    while (i.hasNext())
    {
        i.next();
        AirportInformation *info = i.value();
        if (info->m_iata.size() != 3) {
            continue;
        }
        AirportInformation *existing = out->value(info->m_iata, nullptr);
        if (!existing || (info->m_type > existing->m_type)) {
            out->insert(info->m_iata, info);
        }
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
            idx = colNames.indexOf("description");
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
