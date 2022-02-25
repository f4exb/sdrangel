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
#include <QRegularExpression>

HttpDownloadManager::HttpDownloadManager()
{
    connect(&manager, &QNetworkAccessManager::finished, this, &HttpDownloadManager::downloadFinished);
}

QNetworkReply *HttpDownloadManager::download(const QUrl &url, const QString &filename)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = manager.get(request);

    connect(reply, &QNetworkReply::sslErrors, this, &HttpDownloadManager::sslErrors);

    qDebug() << "HttpDownloadManager: Downloading from " << url << " to " << filename;
    m_downloads.append(reply);
    m_filenames.append(filename);
    return reply;
}

// Indicate if we have any downloads in progress
bool HttpDownloadManager::downloading() const
{
    return m_filenames.size() > 0;
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

bool HttpDownloadManager::writeToFile(const QString &filename, const QByteArray &data)
{
    QFile file(filename);

    // Make sure directory to save the file in exists
    QFileInfo fileInfo(filename);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists())
        dir.mkpath(".");

    if (file.open(QIODevice::WriteOnly))
    {
        file.write(data);
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
    int idx = m_downloads.indexOf(reply);
    QString filename = m_filenames[idx];
    bool success = false;
    bool retry = false;

    if (!reply->error())
    {
        if (!isHttpRedirect(reply))
        {
            QByteArray data = reply->readAll();

            // Google drive can redirect downloads to a virus scan warning page
            // We need to use URL with confirm code and retry
            if (url.startsWith("https://drive.google.com/uc?export=download")
                && data.startsWith("<!DOCTYPE html>")
                && !filename.endsWith(".html")
               )
            {
                QRegularExpression regexp("action=\\\"(.*?)\\\"");
                QRegularExpressionMatch match = regexp.match(data);
                if (match.hasMatch())
                {
                    m_downloads.removeAll(reply);
                    m_filenames.remove(idx);

                    QString action = match.captured(1);
                    action = action.replace("&amp;", "&");
                    qDebug() << "HttpDownloadManager: Skipping Go ogle drive warning - downloading " << action;
                    QUrl newUrl(action);
                    QNetworkReply *newReply = download(newUrl, filename);

                    // Indicate that we are retrying, so progress dialogs can be updated
                    emit retryDownload(filename, reply, newReply);

                    retry = true;
                }
                else
                {
                    qDebug() << "HttpDownloadManager: Can't find action URL in Google Drive page " << data;
                }
            }
            else if (writeToFile(filename, data))
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

    if (!retry)
    {
        m_downloads.removeAll(reply);
        m_filenames.remove(idx);
        emit downloadComplete(filename, success, url, reply->errorString());
    }
    reply->deleteLater();
}
