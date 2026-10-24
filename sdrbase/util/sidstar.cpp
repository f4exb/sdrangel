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

#include "sidstar.h"

#include <algorithm>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStandardPaths>

#if (QT_VERSION < QT_VERSION_CHECK(6, 6, 0))
#include <QtGui/private/qzipreader_p.h>
#else
#include <QtCore/private/qzipreader_p.h>
#endif

QSharedPointer<SidStar::ProcedureMap> SidStar::m_procedures;
QDateTime SidStar::m_sidStarModifiedDateTime;
QMutex SidStar::m_mutex;

SidStar::SidStar(QObject *parent) :
    QObject(parent)
{
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &SidStar::downloadFinished);
}

SidStar::~SidStar()
{
    disconnect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &SidStar::downloadFinished);
}

QString SidStar::procedureKey(const QString& airport, const QString& procedure)
{
    return airport.trimmed().toUpper() + QChar(0x1f) + procedure.trimmed().toUpper();
}

// Whether a procedure name is one a flight plan would have to shorten.
//
// An ARINC 424 procedure identifier is the significant fix, a validity digit and a route
// letter, and it is limited to six characters. A five letter fix makes seven - ROXOG, 1,
// H - so the fix is truncated to four and the plan files ROXO1H where the procedure is
// really ROXOG1H.
static bool truncatableProcedure(const QString& procedure)
{
    if (procedure.size() != 7) {
        return false;
    }

    for (int i = 0; i < 5; i++)
    {
        if (!procedure[i].isLetter()) {
            return false;
        }
    }

    return procedure[5].isDigit() && procedure[6].isLetter();
}

SidStar::ProcedureMap *SidStar::readCSV(const QString& filename)
{
    ProcedureMap *procedures = new ProcedureMap();
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "SidStar::readCSV: Could not open" << filename << "for reading.";
        return procedures;
    }

    const QList<QByteArray> requiredColumns {
        "country",
        "airport",
        "procedure_type",
        "procedure",
        "sequence",
        "waypoint",
        "latitude",
        "longitude",
        "minimum_altitude_ft",
        "maximum_altitude_ft"
    };
    QByteArray headerLine = file.readLine();

    while (headerLine.endsWith('\n') || headerLine.endsWith('\r')) {
        headerLine.chop(1);
    }

    const QList<QByteArray> header = headerLine.split(',');
    QHash<QByteArray, int> columns;

    for (const QByteArray& column : requiredColumns)
    {
        const int index = header.indexOf(column);

        if (index < 0)
        {
            qWarning() << "SidStar::readCSV: Missing column" << column << "in" << filename;
            return procedures;
        }

        columns.insert(column, index);
    }

    int highestColumn = 0;

    for (const QByteArray& column : requiredColumns) {
        highestColumn = std::max(highestColumn, columns.value(column));
    }

    const int countryColumn = columns.value("country");
    const int airportColumn = columns.value("airport");
    const int procedureTypeColumn = columns.value("procedure_type");
    const int procedureColumn = columns.value("procedure");
    const int sequenceColumn = columns.value("sequence");
    const int waypointColumn = columns.value("waypoint");
    const int latitudeColumn = columns.value("latitude");
    const int longitudeColumn = columns.value("longitude");
    const int minimumAltitudeColumn = columns.value("minimum_altitude_ft");
    const int maximumAltitudeColumn = columns.value("maximum_altitude_ft");
    int invalidRows = 0;
    int pointCount = 0;

    // These fields are identifiers and numbers and do not contain quoted commas,
    // so line-based parsing is substantially faster than the generic CSV reader.
    while (!file.atEnd())
    {
        QByteArray line = file.readLine();

        while (line.endsWith('\n') || line.endsWith('\r')) {
            line.chop(1);
        }

        const QList<QByteArray> fields = line.split(',');

        if (fields.size() <= highestColumn)
        {
            invalidRows++;
            continue;
        }

        bool sequenceValid;
        bool latitudeValid;
        bool longitudeValid;
        SidStarPoint point;

        point.m_country = QString::fromUtf8(fields[countryColumn]);
        point.m_airport = QString::fromUtf8(fields[airportColumn]);
        point.m_procedureType = QString::fromUtf8(fields[procedureTypeColumn]);
        point.m_procedure = QString::fromUtf8(fields[procedureColumn]);
        point.m_sequence = fields[sequenceColumn].toInt(&sequenceValid);
        point.m_waypoint = QString::fromUtf8(fields[waypointColumn]);
        point.m_latitude = fields[latitudeColumn].toFloat(&latitudeValid);
        point.m_longitude = fields[longitudeColumn].toFloat(&longitudeValid);
        point.m_minimumAltitudeFt = fields[minimumAltitudeColumn].toInt(&point.m_hasMinimumAltitude);
        point.m_maximumAltitudeFt = fields[maximumAltitudeColumn].toInt(&point.m_hasMaximumAltitude);

        if (!sequenceValid || !latitudeValid || !longitudeValid)
        {
            invalidRows++;
            continue;
        }

        (*procedures)[procedureKey(point.m_airport, point.m_procedure)].append(point);
        pointCount++;
    }

    ProcedureMap::iterator procedure = procedures->begin();

    while (procedure != procedures->end())
    {
        QList<SidStarPoint>& points = procedure.value();
        std::sort(points.begin(), points.end(), [](const SidStarPoint& a, const SidStarPoint& b) {
            const int typeOrder = QString::compare(a.m_procedureType, b.m_procedureType, Qt::CaseInsensitive);

            if (typeOrder != 0) {
                return typeOrder < 0;
            }

            return a.m_sequence < b.m_sequence;
        });
        ++procedure;
    }

    // Index the six character form a flight plan would file as well as the full name,
    // so that a plan naming ROXO1H finds ROXOG1H. See truncatableProcedure().
    //
    // Only where it is unambiguous, which on the current data means always: of the 1475
    // procedures long enough to be truncated, no two at the same airport shorten to the
    // same thing and none of them collides with a procedure that is genuinely six
    // characters. Both are checked rather than assumed, because an alias that shadowed a
    // real procedure would draw the wrong route rather than none - a worse failure than
    // the one this fixes.
    QHash<QString, QString> aliases;         //!< Short form -> the one full name, or empty
    const QList<QString> keys = procedures->keys();

    for (const QString& key : keys)
    {
        const int split = key.indexOf(QChar(0x1f));

        if (split < 0) {
            continue;
        }

        const QString procedure = key.mid(split + 1);

        if (!truncatableProcedure(procedure)) {
            continue;
        }

        const QString shortKey = procedureKey(key.left(split),
                                              procedure.left(4) + procedure.mid(5));

        // A procedure that really is six characters keeps its own name
        if (procedures->contains(shortKey)) {
            continue;
        }

        // Two full names shortening to one - neither gets the alias
        aliases.insert(shortKey, aliases.contains(shortKey) ? QString() : key);
    }

    int aliasCount = 0;

    for (auto alias = aliases.cbegin(); alias != aliases.cend(); ++alias)
    {
        if (alias.value().isEmpty()) {
            qWarning() << "SidStar::readCSV: Ambiguous shortened procedure" << alias.key()
                       << "- not indexed";
            continue;
        }

        procedures->insert(alias.key(), procedures->value(alias.value()));
        aliasCount++;
    }

    qDebug() << "SidStar::readCSV: Read" << pointCount << "points in"
             << procedures->size() - aliasCount << "procedures from" << filename
             << "with" << aliasCount << "shortened names also indexed";

    if (invalidRows > 0) {
        qWarning() << "SidStar::readCSV: Ignored" << invalidRows << "invalid rows in" << filename;
    }

    return procedures;
}

QList<SidStarPoint> SidStar::getWaypoints(
    const QString& procedure,
    const QString& airport,
    const QString& procedureType
)
{
    QMutexLocker locker(&m_mutex);
    const QDateTime fileDateTime = getSidStarModifiedDateTime();

    if (!m_procedures || (fileDateTime > m_sidStarModifiedDateTime))
    {
        m_procedures = QSharedPointer<ProcedureMap>(readCSV(getSidStarFilename()));
        m_sidStarModifiedDateTime = fileDateTime;
    }

    const QList<SidStarPoint> points = m_procedures->value(procedureKey(airport, procedure));

    if (procedureType.isEmpty()) {
        return points;
    }

    QList<SidStarPoint> matchingPoints;
    matchingPoints.reserve(points.size());

    for (const SidStarPoint& point : points)
    {
        if (point.m_procedureType.compare(procedureType, Qt::CaseInsensitive) == 0) {
            matchingPoints.append(point);
        }
    }

    return matchingPoints;
}

QString SidStar::getDataDir()
{
    const QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    return locations[0];
}

QString SidStar::getSidStarZipFilename()
{
    return getDataDir() + "/sidstar.zip";
}

QString SidStar::getSidStarFilename()
{
    return getDataDir() + "/sidstar.csv";
}

QDateTime SidStar::getSidStarModifiedDateTime()
{
    const QFileInfo fileInfo(getSidStarFilename());
    return fileInfo.lastModified();
}

void SidStar::downloadSidStar()
{
    const QString filename = getSidStarZipFilename();
    const QString urlString = SIDSTAR_URL;
    const QUrl url(urlString);

    qDebug() << "SidStar::downloadSidStar: Downloading" << urlString;
    emit downloadingURL(urlString);
    m_dlm.download(url, filename);
}

void SidStar::downloadFinished(const QString& filename, bool success)
{
    if (!success)
    {
        qWarning() << "SidStar::downloadFinished: Failed to download:" << filename;
        emit downloadError(QString("Failed to download: %1").arg(filename));
    }
    else if (filename == getSidStarZipFilename())
    {
        QZipReader reader(filename);

        if (reader.extractAll(getDataDir()))
        {
            emit downloadSidStarFinished();
        }
        else
        {
            qWarning() << "SidStar::downloadFinished: Failed to extract files from" << filename;
            emit downloadError(QString("Failed to extract files from %1").arg(filename));
        }
    }
    else
    {
        qWarning() << "SidStar::downloadFinished: Unexpected filename:" << filename;
        emit downloadError(QString("Unexpected filename: %1").arg(filename));
    }
}
