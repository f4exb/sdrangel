///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
//                                                                               //
// BGL structure and identifier conversion based on atools:                      //
// https://github.com/albar965/atools                                            //
// Copyright (C) 2015-2026 Alexander Barthel                                     //
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

#include "bglwaypointreader.h"

#include <algorithm>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QVector>

#include "waypoints.h"
#include "csv.h"

namespace {

const quint32 BGL_MAGIC_1 = 0x19920201;
const quint32 BGL_MAGIC_2 = 0x08051803;
const quint32 BGL_HEADER_SIZE = 0x38;
const quint32 BGL_SECTION_SIZE = 20;
const quint32 BGL_WAYPOINT_SECTION = 0x22;
const quint16 BGL_WAYPOINT_RECORD = 0x22;
const quint16 BGL_WAYPOINT_RECORD_MSFS_2024 = 0x108;

struct Section
{
    quint32 m_subsectionSize;
    quint32 m_subsectionCount;
    quint32 m_firstSubsectionOffset;
    quint32 m_totalSubsectionSize;
};

bool within(qint64 offset, qint64 length, qint64 limit)
{
    return (offset >= 0) && (length >= 0) && (offset <= limit) && (length <= limit - offset);
}

QString decodeIcao(quint64 value, int characterCount, int bitShift)
{
    value >>= bitShift;

    if (value == 0) {
        return QString();
    }

    quint64 codes[8] = {};
    int codeCount = 0;

    if (value > 37)
    {
        while ((value > 37) && (codeCount < characterCount))
        {
            const quint64 code = value % 38;
            codes[codeCount++] = code;
            value = (value - code) / 38;

            if ((value < 38) && (codeCount < characterCount)) {
                codes[codeCount++] = value;
            }
        }
    }
    else
    {
        codes[codeCount++] = value;
    }

    QString ident;

    for (int i = 0; i < codeCount; i++)
    {
        const quint64 code = codes[i];

        if (code == 0) {
            break;
        } else if ((code > 1) && (code < 12)) {
            ident.prepend(QChar('0' + static_cast<char>(code - 2)));
        } else if ((code >= 12) && (code < 38)) {
            ident.prepend(QChar('A' + static_cast<char>(code - 12)));
        } else {
            return QString();
        }
    }

    return ident;
}

double decodeLongitude(qint32 value)
{
    return value * (360.0 / (3.0 * 0x10000000)) - 180.0;
}

double decodeLatitude(qint32 value)
{
    return 90.0 - value * (180.0 / (2.0 * 0x10000000));
}

void addError(QStringList *errors, const QString& filename, const QString& message)
{
    if (errors) {
        errors->append(QStringLiteral("%1: %2").arg(filename, message));
    }
}

QString sourcePath(const QFileInfo& source)
{
    const QString canonicalPath = source.canonicalFilePath();
    return QDir::cleanPath(canonicalPath.isEmpty() ? source.absoluteFilePath() : canonicalPath);
}

QList<QFileInfo> sourceFiles(const QFileInfo& source)
{
    QList<QFileInfo> files;

    if (source.isFile())
    {
        files.append(source);
    }
    else if (source.isDir())
    {
        QDirIterator iterator(
            source.absoluteFilePath(),
            QStringList() << QStringLiteral("*.bgl") << QStringLiteral("*.BGL"),
            QDir::Files,
            QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            files.append(QFileInfo(iterator.next()));
        }
    }

    std::sort(files.begin(), files.end(), [](const QFileInfo& a, const QFileInfo& b) {
        return a.absoluteFilePath() < b.absoluteFilePath();
    });
    return files;
}

QByteArray sourceFingerprint(const QList<QFileInfo>& files, const QString& rootPath)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);

    for (const QFileInfo& file : files)
    {
        QString filename = file.absoluteFilePath();
        if (filename.startsWith(rootPath)) {
            filename = filename.mid(rootPath.size());
        }
        hash.addData(filename.toUtf8());
        hash.addData("\0", 1);
        hash.addData(QByteArray::number(file.size()));
        hash.addData("\0", 1);
        hash.addData(QByteArray::number(file.lastModified().toMSecsSinceEpoch()));
        hash.addData("\0", 1);
    }

    return hash.result().toHex();
}

QString cacheFilename(const QString& rootPath)
{
    const QByteArray pathHash = QCryptographicHash::hash(
        rootPath.toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    const QString appDataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDirectory.isEmpty()) {
        return QString();
    }
    const QString cacheDirectory = appDataDirectory + QStringLiteral("/cache");
    QDir().mkpath(cacheDirectory);
    return QDir(cacheDirectory).filePath(
        QStringLiteral("navigraph-waypoints-%1.csv").arg(QString::fromLatin1(pathHash)));
}

QMultiHash<QString, Waypoint *> *readCache(
    const QString& filename,
    const QByteArray& fingerprint)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return nullptr;
    }

    const QList<QByteArray> header = file.readLine().trimmed().split(',');
    file.close();
    if ((header.size() != 3)
        || (header[0] != QByteArrayLiteral("# SDRangel Navigraph waypoint cache v1"))
        || (header[1] != fingerprint))
    {
        return nullptr;
    }

    bool countOk;
    const int expectedCount = header[2].toInt(&countOk);
    if (!countOk || (expectedCount <= 0)) {
        return nullptr;
    }

    QMultiHash<QString, Waypoint *> *waypoints = Waypoint::readCSV(filename);
    if (waypoints->size() == expectedCount) {
        return waypoints;
    }

    qDeleteAll(*waypoints);
    delete waypoints;
    return nullptr;
}

bool writeCache(
    const QString& filename,
    const QByteArray& fingerprint,
    const QMultiHash<QString, Waypoint *>& waypoints)
{
    QSaveFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "# SDRangel Navigraph waypoint cache v1," << fingerprint << ',' << waypoints.size() << '\n';
    for (auto i = waypoints.constBegin(); i != waypoints.constEnd(); ++i)
    {
        const Waypoint *waypoint = i.value();
        out << CSV::escape(waypoint->m_name) << ','
            << QString::number(waypoint->m_latitude, 'g', 9) << ','
            << QString::number(waypoint->m_longitude, 'g', 9) << '\n';
    }
    out.flush();

    return (out.status() == QTextStream::Ok) && file.commit();
}

bool readWaypointRecord(
    QDataStream& stream,
    QFile& file,
    qint64 recordOffset,
    qint64 subsectionEnd,
    QMultiHash<QString, Waypoint *>& waypoints,
    QString *error)
{
    if (!within(recordOffset, 6, subsectionEnd))
    {
        *error = QStringLiteral("truncated waypoint record header at offset 0x%1").arg(recordOffset, 0, 16);
        return false;
    }

    if (!file.seek(recordOffset))
    {
        *error = QStringLiteral("could not seek to waypoint record at offset 0x%1").arg(recordOffset, 0, 16);
        return false;
    }

    quint16 recordType;
    quint32 recordSize;
    stream >> recordType >> recordSize;

    if (stream.status() != QDataStream::Ok)
    {
        *error = QStringLiteral("could not read waypoint record header at offset 0x%1").arg(recordOffset, 0, 16);
        return false;
    }

    if ((recordSize < 6) || !within(recordOffset, recordSize, subsectionEnd))
    {
        *error = QStringLiteral("invalid waypoint record size %1 at offset 0x%2").arg(recordSize).arg(recordOffset, 0, 16);
        return false;
    }

    const bool msfs2024 = recordType == BGL_WAYPOINT_RECORD_MSFS_2024;

    if ((recordType != BGL_WAYPOINT_RECORD) && !msfs2024) {
        return true;
    }

    const quint32 minimumSize = msfs2024 ? 36 : 28;

    if (recordSize < minimumSize)
    {
        *error = QStringLiteral("waypoint record 0x%1 is too short (%2 bytes) at offset 0x%3")
            .arg(recordType, 0, 16).arg(recordSize).arg(recordOffset, 0, 16);
        return false;
    }

    quint8 waypointType;
    quint8 airwayCount;
    qint32 longitudeValue;
    qint32 latitudeValue;
    float magneticVariation;
    quint64 encodedIdent;

    stream >> waypointType >> airwayCount >> longitudeValue >> latitudeValue >> magneticVariation;

    if (msfs2024)
    {
        stream >> encodedIdent;
    }
    else
    {
        quint32 shortIdent;
        stream >> shortIdent;
        encodedIdent = shortIdent;
    }

    // The remaining fixed fields contain region/airport flags and, for the
    // MSFS 2024 record, four currently unknown bytes. Airway data follows.
    quint32 regionFlags;
    stream >> regionFlags;

    if (msfs2024)
    {
        quint32 unknown;
        stream >> unknown;
    }

    Q_UNUSED(waypointType)
    Q_UNUSED(airwayCount)
    Q_UNUSED(magneticVariation)
    Q_UNUSED(regionFlags)

    if (stream.status() != QDataStream::Ok)
    {
        *error = QStringLiteral("could not read waypoint record at offset 0x%1").arg(recordOffset, 0, 16);
        return false;
    }

    const QString ident = decodeIcao(encodedIdent, msfs2024 ? 8 : 5, msfs2024 ? 6 : 5);
    const double latitude = decodeLatitude(latitudeValue);
    const double longitude = decodeLongitude(longitudeValue);

    // Unnamed records cannot be looked up by Waypoints. Reject malformed
    // coordinates while retaining valid polar waypoints such as NPOLE.
    if (!ident.isEmpty()
        && (latitude >= -90.0) && (latitude <= 90.0)
        && (longitude >= -180.0) && (longitude <= 180.0))
    {
        Waypoint *waypoint = new Waypoint();
        waypoint->m_name = ident;
        waypoint->m_latitude = static_cast<float>(latitude);
        waypoint->m_longitude = static_cast<float>(longitude);
        waypoints.insert(ident, waypoint);
    }

    return true;
}

bool readFile(const QString& filename, QMultiHash<QString, Waypoint *>& waypoints, QString *error)
{
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly))
    {
        *error = QStringLiteral("could not open file");
        return false;
    }

    const qint64 fileSize = file.size();

    if (fileSize < BGL_HEADER_SIZE)
    {
        *error = QStringLiteral("file is too small to contain a BGL header");
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    quint32 magic1;
    quint32 headerSize;
    quint32 lowDateTime;
    quint32 highDateTime;
    quint32 magic2;
    quint32 sectionCount;
    stream >> magic1 >> headerSize >> lowDateTime >> highDateTime >> magic2 >> sectionCount;
    Q_UNUSED(lowDateTime)
    Q_UNUSED(highDateTime)

    if ((stream.status() != QDataStream::Ok) || (magic1 != BGL_MAGIC_1) || (magic2 != BGL_MAGIC_2))
    {
        *error = QStringLiteral("not a supported section-based BGL file");
        return false;
    }

    if ((headerSize < BGL_HEADER_SIZE) || !within(headerSize, static_cast<qint64>(sectionCount) * BGL_SECTION_SIZE, fileSize))
    {
        *error = QStringLiteral("invalid BGL header or section count");
        return false;
    }

    if (!file.seek(headerSize))
    {
        *error = QStringLiteral("could not seek to BGL section table");
        return false;
    }

    QVector<Section> waypointSections;

    for (quint32 i = 0; i < sectionCount; i++)
    {
        quint32 type;
        quint32 sizeFlag;
        quint32 subsectionCount;
        quint32 firstSubsectionOffset;
        quint32 totalSubsectionSize;
        stream >> type >> sizeFlag >> subsectionCount >> firstSubsectionOffset >> totalSubsectionSize;

        if (stream.status() != QDataStream::Ok)
        {
            *error = QStringLiteral("could not read BGL section table");
            return false;
        }

        if (type == BGL_WAYPOINT_SECTION)
        {
            Section section;
            section.m_subsectionSize = ((sizeFlag & 0x10000) | 0x40000) >> 14;
            section.m_subsectionCount = subsectionCount;
            section.m_firstSubsectionOffset = firstSubsectionOffset;
            section.m_totalSubsectionSize = totalSubsectionSize;
            waypointSections.append(section);
        }
    }

    for (const Section& section : waypointSections)
    {
        if ((section.m_subsectionSize < 16)
            || !within(section.m_firstSubsectionOffset, section.m_totalSubsectionSize, fileSize)
            || (static_cast<quint64>(section.m_subsectionSize) * section.m_subsectionCount > section.m_totalSubsectionSize))
        {
            *error = QStringLiteral("invalid waypoint subsection table");
            return false;
        }

        for (quint32 i = 0; i < section.m_subsectionCount; i++)
        {
            const qint64 subsectionOffset = section.m_firstSubsectionOffset + static_cast<qint64>(i) * section.m_subsectionSize;

            if (!within(subsectionOffset, 16, fileSize) || !file.seek(subsectionOffset))
            {
                *error = QStringLiteral("invalid waypoint subsection entry at offset 0x%1").arg(subsectionOffset, 0, 16);
                return false;
            }

            qint32 subsectionId;
            qint32 recordCount;
            qint32 firstRecordOffset;
            qint32 dataSize;
            stream >> subsectionId >> recordCount >> firstRecordOffset >> dataSize;
            Q_UNUSED(subsectionId)

            if ((stream.status() != QDataStream::Ok)
                || (recordCount < 0) || (firstRecordOffset < 0) || (dataSize < 0)
                || !within(firstRecordOffset, dataSize, fileSize))
            {
                *error = QStringLiteral("invalid waypoint subsection entry at offset 0x%1").arg(subsectionOffset, 0, 16);
                return false;
            }

            const qint64 subsectionEnd = static_cast<qint64>(firstRecordOffset) + dataSize;
            qint64 recordOffset = firstRecordOffset;

            for (qint32 recordIndex = 0; recordIndex < recordCount; recordIndex++)
            {
                if (!readWaypointRecord(stream, file, recordOffset, subsectionEnd, waypoints, error)) {
                    return false;
                }

                if (!file.seek(recordOffset + 2))
                {
                    *error = QStringLiteral("could not seek within waypoint record at offset 0x%1").arg(recordOffset, 0, 16);
                    return false;
                }

                quint32 recordSize;
                stream >> recordSize;

                if ((stream.status() != QDataStream::Ok) || (recordSize < 6))
                {
                    *error = QStringLiteral("invalid waypoint record at offset 0x%1").arg(recordOffset, 0, 16);
                    return false;
                }

                recordOffset += recordSize;
            }
        }
    }

    return true;
}

}

QMultiHash<QString, Waypoint *> *BGLWaypointReader::read(const QString& path, QStringList *errors)
{
    QMultiHash<QString, Waypoint *> *waypoints = new QMultiHash<QString, Waypoint *>();
    const QFileInfo fileInfo(path);

    if (!fileInfo.isFile() && !fileInfo.isDir())
    {
        addError(errors, path, QStringLiteral("path does not exist"));
        return waypoints;
    }

    const QString rootPath = sourcePath(fileInfo);
    const QList<QFileInfo> files = sourceFiles(fileInfo);
    if (files.isEmpty()) {
        return waypoints;
    }

    const QByteArray fingerprint = sourceFingerprint(files, rootPath);
    const QString cache = cacheFilename(rootPath);
    QMultiHash<QString, Waypoint *> *cachedWaypoints = cache.isEmpty()
        ? nullptr : readCache(cache, fingerprint);
    if (cachedWaypoints)
    {
        delete waypoints;
        qDebug() << "BGLWaypointReader::read: Read" << cachedWaypoints->size()
                 << "waypoints from cache" << cache;
        return cachedWaypoints;
    }

    bool readError = false;
    for (const QFileInfo& file : files)
    {
        QString error;

        if (!readFile(file.absoluteFilePath(), *waypoints, &error))
        {
            addError(errors, file.absoluteFilePath(), error);
            readError = true;
        }
    }

    if (!cache.isEmpty() && !waypoints->isEmpty() && !readError)
    {
        if (writeCache(cache, fingerprint, *waypoints)) {
            qDebug() << "BGLWaypointReader::read: Cached" << waypoints->size()
                     << "waypoints in" << cache;
        } else {
            qWarning() << "BGLWaypointReader::read: Could not write waypoint cache" << cache;
        }
    }

    return waypoints;
}
