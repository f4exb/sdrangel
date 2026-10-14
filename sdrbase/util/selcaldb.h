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

#ifndef INCLUDE_SELCALDB_H
#define INCLUDE_SELCALDB_H

#include <QDateTime>
#include <QMultiHash>
#include <QMutex>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

#include "util/httpdownloadmanager.h"
#include "export.h"

#define SELCALDB_URL "https://sdrangel.org/downloads/selcal.zip"

// Database of SELCAL codes assigned to aircraft, from a CSV file with Code and
// Registration columns. Codes are not unique, so a code can have several
// registrations.
class SDRBASE_API SELCALDB : public QObject {
    Q_OBJECT

public:

    SELCALDB(QObject* parent=nullptr);
    ~SELCALDB();

    void downloadSELCALInformation();

    // Registrations keyed by normalized code. Reloaded if the file has changed.
    // Null if the database has not been downloaded.
    static QSharedPointer<const QMultiHash<QString, QString>> getRegistrations();

    // Registrations of aircraft assigned a code, in file order.
    static QStringList getRegistrations(const QString& code);

    // Code in the form AB-CD, with each pair in tone designation order, or an
    // empty string if it is not a valid code.
    static QString normalizeCode(const QString& code);

    static QMultiHash<QString, QString> *readCSV(const QString& filename);

    static QString getZipFilename();
    static QString getCSVFilename();

private:
    HttpDownloadManager m_dlm;

    static QSharedPointer<const QMultiHash<QString, QString>> m_registrations;
    static QDateTime m_modifiedDateTime;
    static QMutex m_mutex;

private slots:
    void downloadFinished(const QString& filename, bool success);

signals:
    void downloadingURL(const QString& url);
    void downloadError(const QString& error);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadSELCALInformationFinished();

};

#endif
