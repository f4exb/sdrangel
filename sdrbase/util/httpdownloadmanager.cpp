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

#include "httpdownloadmanager.h"

#include <QDebug>
#include <QFile>
#include <QDateTime>

HttpDownloadManager::HttpDownloadManager()
{
    connect(&manager, &QNetworkAccessManager::finished, this, &HttpDownloadManager::downloadFinished);
}

QNetworkReply *HttpDownloadManager::download(const QUrl &url, const QString &filename)
{
    QNetworkRequest request(url);
    QNetworkReply *reply = manager.get(request);

    connect(reply, &QNetworkReply::sslErrors, this, &HttpDownloadManager::sslErrors);

    qDebug() << "HttpDownloadManager: Downloading from " << url << " to " << filename;
    downloads.append(reply);
    filenames.append(filename);
    return reply;
}

qint64 HttpDownloadManager::fileAgeInDays(const QString& filename)
{
    QFile file(filename);
    if (file.exists())
    {
        QDateTime modified = file.fileTime(QFileDevice::FileModificationTime);
        if (modified.isValid())
            return modified.daysTo(QDateTime::currentDateTime());
        else
            return -1;
    }
    return -1;
}

// Get default directory to write downloads to
QString HttpDownloadManager::downloadDir()
{
    // Get directory to store app data in
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    return locations[0];
}

void HttpDownloadManager::sslErrors(const QList<QSslError> &sslErrors)
{
    for (const QSslError &error : sslErrors)
        qCritical() << "HttpDownloadManager: SSL error: " << error.errorString();
}

bool HttpDownloadManager::isHttpRedirect(QNetworkReply *reply)
{
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // 304 is file not changed, but maybe we did
    return (status >= 301 && status <= 308);
}

bool HttpDownloadManager::writeToFile(const QString &filename, QIODevice *data)
{
    QFile file(filename);

    // Make sure directory to save the file in exists
    QFileInfo fileInfo(filename);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists())
        dir.mkpath(".");

    if (file.open(QIODevice::WriteOnly))
    {
        file.write(data->readAll());
        file.close();
        return true;
    }
    else
    {
        qCritical() << "HttpDownloadManager: Could not open " << filename << " for writing: " << file.errorString();
        return false;
    }
}

void HttpDownloadManager::downloadFinished(QNetworkReply *reply)
{
    QString url = reply->url().toEncoded().constData();
    int idx = downloads.indexOf(reply);
    QString filename = filenames[idx];
    bool success = false;

    if (!reply->error())
    {
        if (!isHttpRedirect(reply))
        {
            if (writeToFile(filename, reply))
            {
                success = true;
                qDebug() << "HttpDownloadManager: Download from " << url << " to " << filename << " finshed.";
            }
        }
        else
        {
            qDebug() << "HttpDownloadManager: Request to download " << url << " was redirected.";
        }
    }
    else
    {
        qCritical() << "HttpDownloadManager: Download of " << url << " failed: " << reply->errorString();
    }

    downloads.removeAll(reply);
    filenames.remove(idx);
    reply->deleteLater();
    emit downloadComplete(filename, success);
}
