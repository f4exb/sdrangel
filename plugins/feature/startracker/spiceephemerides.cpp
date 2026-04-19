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

#include <QDir>
#include <QUrl>
#include <QDebug>

#include "spiceephemerides.h"
#include "spice.h"

SpiceEphemerides::SpiceEphemerides(QObject *parentWidget) :
    m_parentWidget(parentWidget)
{
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &SpiceEphemerides::downloadComplete);
}

// Returns if download was required
bool SpiceEphemerides::download(const QStringList &emphemerides)
{
    QDir downloadDir = QDir(HttpDownloadManager::downloadDir());
    QString ephemeridesDirPath = downloadDir.path() + "/ephemerides";
    QDir ephemeridesDir(ephemeridesDirPath);
    bool downloadRequired = false;

    if (!ephemeridesDir.exists())
    {
        if (!downloadDir.mkdir("ephemerides")) {
            qWarning() << "Failed to make directory" << (downloadDir.path() + "/ephemerides");
        }
    }
    if (ephemeridesDir.exists())
    {
        for (const auto& ephemeris : emphemerides)
        {
            if (ephemeris.startsWith("http://") || ephemeris.startsWith("https://"))
            {
                QUrl ephemerisURL(ephemeris);
                QString ephemerisFilename = urlToFilename(ephemeris);

                if (!QFileInfo::exists(ephemerisFilename))
                {
                    qDebug() << "Downloading ephemeris from" << ephemerisURL << "to" << ephemerisFilename;
                    m_pendingDownloads.append(ephemerisFilename);
#ifdef SERVER_MODE
                    m_dlm.download(ephemerisURL, ephemerisFilename);
#else
                    m_dlm.download(ephemerisURL, ephemerisFilename, (QWidget *) m_parentWidget);
#endif
                    downloadRequired = true;
                }
                else
                {
                    qDebug() << "Ephemeris" << ephemerisFilename << "already downloaded";
                }
            }
        }
    }

    return downloadRequired;
}

void SpiceEphemerides::downloadComplete(const QString &filename, bool success, const QString &url, const QString &errorMessage)
{
    (void) success;
    (void) url;
    (void) errorMessage;

    m_completedDownloads.append(filename);
    if (m_completedDownloads == m_pendingDownloads)
    {
        m_pendingDownloads.clear();
        m_completedDownloads.clear();
        emit allDownloadsComplete();
    }
}

bool SpiceEphemerides::checkDownloaded(const QStringList &emphemeridesURLs) const
{
    bool downloaded = true;

    for (const auto& file : getEphemeridesFiles(emphemeridesURLs))
    {
        if (!QFileInfo::exists(file))
        {
            downloaded = false;
            break;
        }
    }

    return downloaded;
}

QStringList SpiceEphemerides::getEphemeridesFiles(const QStringList &emphemeridesURLs)
{
    QStringList ephemeridesFiles;

    for (const auto& ephemeris : emphemeridesURLs) {
        ephemeridesFiles.append(urlToFilename(ephemeris));
    }
    return ephemeridesFiles;
}

QString SpiceEphemerides::urlToFilename(const QString &ephemerisURL)
{
    if (ephemerisURL.startsWith("http://") || ephemerisURL.startsWith("https://"))
    {
        QString ephemeridesDirPath = HttpDownloadManager::downloadDir() + "/ephemerides";

        return ephemeridesDirPath + "/" + QUrl(ephemerisURL).fileName();
    }
    else
    {
        return ephemerisURL;
    }
}

QStringList SpiceEphemerides::getTargets(const QStringList &emphemeridesURLs)
{
    QStringList ephemeridesFiles = getEphemeridesFiles(emphemeridesURLs);
    QStringList targets;

    spiceLock(ephemeridesFiles);

    for (const auto& file : ephemeridesFiles) {
       targets.append(getSPICETargets(file));
    }

    spiceUnlock();

    targets.removeDuplicates();

    return targets;
}