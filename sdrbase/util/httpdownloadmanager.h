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

#ifndef INCLUDE_HTTPDOWNLOADMANAGER_H
#define INCLUDE_HTTPDOWNLOADMANAGER_H

#include <QtCore>
#include <QtNetwork>

#include "export.h"

class QSslError;

// Class to download files via http and write them to disk
class SDRBASE_API HttpDownloadManager : public QObject
{
    Q_OBJECT
public:
    HttpDownloadManager();
    QNetworkReply *download(const QUrl &url, const QString &filename);
    bool downloading() const;

    static QString downloadDir();

protected:
    static qint64 fileAgeInDays(const QString& filename);

private:
    QNetworkAccessManager manager;
    QVector<QNetworkReply *> m_downloads;
    QVector<QString> m_filenames;

    static bool writeToFile(const QString &filename, const QByteArray &data);
    static bool isHttpRedirect(QNetworkReply *reply);

public slots:
    void downloadFinished(QNetworkReply *reply);
    void sslErrors(const QList<QSslError> &errors);

signals:
    void downloadComplete(const QString &filename, bool success, const QString &url, const QString &errorMessage);
    void retryDownload(const QString &filename, QNetworkReply *oldReply, QNetworkReply *newReply);

};

#endif /* INCLUDE_HTTPDOWNLOADMANAGER_H */
