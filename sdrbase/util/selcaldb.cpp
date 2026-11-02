///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <algorithm>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QTextStream>

#if (QT_VERSION < QT_VERSION_CHECK(6, 6, 0))
#include <QtGui/private/qzipreader_p.h>
#else
#include <QtCore/private/qzipreader_p.h>
#endif

#include "util/selcaldb.h"
#include "util/osndb.h"
#include "util/corsproxy.h"
#include "util/csv.h"

QSharedPointer<const QMultiHash<QString, QString>> SELCALDB::m_registrations;
QSharedPointer<const QHash<QString, QString>> SELCALDB::m_codes;
QDateTime SELCALDB::m_modifiedDateTime;
QMutex SELCALDB::m_mutex;

SELCALDB::SELCALDB(QObject *parent) :
    QObject(parent)
{
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &SELCALDB::downloadFinished);
}

SELCALDB::~SELCALDB()
{
    disconnect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &SELCALDB::downloadFinished);
}

QString SELCALDB::getZipFilename()
{
    return OsnDB::getDataDir() + "/selcal.zip";
}

QString SELCALDB::getCSVFilename()
{
    return OsnDB::getDataDir() + "/selcal.csv";
}

void SELCALDB::downloadSELCALInformation()
{
    QString filename = getZipFilename();
    QString urlString = SELCALDB_URL;
    QUrl dbURL(CORSProxy::adjustHost(urlString));
    qDebug() << "SELCALDB::downloadSELCALInformation: Downloading " << urlString;
    emit downloadingURL(urlString);
    QNetworkReply *reply = m_dlm.download(dbURL, filename);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesRead, qint64 totalBytes) {
        emit downloadProgress(bytesRead, totalBytes);
    });
}

void SELCALDB::downloadFinished(const QString& filename, bool success)
{
    if (!success)
    {
        qWarning() << "SELCALDB::downloadFinished: Failed to download: " << filename;
        emit downloadError(QString("Failed to download: %1").arg(filename));
    }
    else if (filename == getZipFilename())
    {
        // Extract .csv file from .zip file
        QZipReader reader(filename);
        if (reader.extractAll(OsnDB::getDataDir()))
        {
            emit downloadSELCALInformationFinished();
        }
        else
        {
            qWarning() << "SELCALDB::downloadFinished - Failed to extract files from " << filename;
            emit downloadError(QString("Failed to extract files from %1").arg(filename));
        }
    }
    else
    {
        qDebug() << "SELCALDB::downloadFinished: Unexpected filename: " << filename;
        emit downloadError(QString("Unexpected filename: %1").arg(filename));
    }
}

QString SELCALDB::normalizeCode(const QString& code)
{
    // Tone designations, in the order used to write each pair
    static const QString order = QStringLiteral("ABCDEFGHJKLMPQRSTUVWXYZ123456789");
    QString symbols = code.toUpper().remove('-').remove(' ');

    if (symbols.size() != 4) {
        return QString();
    }
    for (const auto& symbol : symbols)
    {
        if (!order.contains(symbol)) {
            return QString();
        }
    }
    auto pair = [](QString pair) {
        std::sort(pair.begin(), pair.end(), [](QChar a, QChar b) { return order.indexOf(a) < order.indexOf(b); });
        return pair;
    };
    return pair(symbols.left(2)) + "-" + pair(symbols.mid(2));
}

QMultiHash<QString, QString> *SELCALDB::readCSV(const QString& filename)
{
    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "SELCALDB::readCSV: Could not open " << filename;
        return nullptr;
    }

    QTextStream in(&file);
    QString error;
    QHash<QString, int> columns = CSV::readHeader(in, {"Code", "Registration"}, error);

    if (!error.isEmpty())
    {
        qWarning() << "SELCALDB::readCSV: " << filename << ": " << error;
        return nullptr;
    }

    int codeCol = columns["Code"];
    int registrationCol = columns["Registration"];
    int lastCol = std::max(codeCol, registrationCol);
    QMultiHash<QString, QString> *registrations = new QMultiHash<QString, QString>();
    QStringList row;

    while (CSV::readRow(in, &row))
    {
        if (row.size() <= lastCol) {
            continue;
        }
        QString code = normalizeCode(row[codeCol]);
        QString registration = row[registrationCol].trimmed();
        if (!code.isEmpty() && !registration.isEmpty()) {
            registrations->insert(code, registration);
        }
    }

    qDebug() << "SELCALDB::readCSV: Read " << registrations->size() << " SELCAL codes from " << filename;
    return registrations;
}

QSharedPointer<const QMultiHash<QString, QString>> SELCALDB::getRegistrations()
{
    QMutexLocker locker(&m_mutex);
    QFileInfo fileInfo(getCSVFilename());

    if (!fileInfo.exists()) {
        return m_registrations;
    }

    // Reload if the file has been downloaded since it was read
    QDateTime modifiedDateTime = fileInfo.lastModified();
    if (!m_registrations || (modifiedDateTime > m_modifiedDateTime))
    {
        QMultiHash<QString, QString> *registrations = readCSV(getCSVFilename());
        if (registrations)
        {
            m_registrations = QSharedPointer<const QMultiHash<QString, QString>>(registrations);
            m_modifiedDateTime = modifiedDateTime;

            QHash<QString, QString> *codes = new QHash<QString, QString>();
            for (auto it = registrations->constBegin(); it != registrations->constEnd(); ++it)
            {
                QString& held = (*codes)[it.value()];
                if (!held.split(", ").contains(it.key())) {
                    held = held.isEmpty() ? it.key() : held + ", " + it.key();
                }
            }
            m_codes = QSharedPointer<const QHash<QString, QString>>(codes);
        }
    }

    return m_registrations;
}

QSharedPointer<const QHash<QString, QString>> SELCALDB::getCodesByRegistration()
{
    getRegistrations();     // Loads, or reloads after a download, both tables together
    QMutexLocker locker(&m_mutex);
    return m_codes;
}

QStringList SELCALDB::getRegistrations(const QString& code)
{
    QSharedPointer<const QMultiHash<QString, QString>> registrations = getRegistrations();

    if (!registrations) {
        return QStringList();
    }

    // values() returns the most recently inserted first
    QStringList list = registrations->values(normalizeCode(code));
    std::reverse(list.begin(), list.end());
    return list;
}
