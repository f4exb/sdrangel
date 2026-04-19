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

#ifndef INCLUDE_SPICE_EPHEMERIDES_H_
#define INCLUDE_SPICE_EPHEMERIDES_H_

#include <QObject>

#ifdef SERVER_MODE
#include "util/httpdownloadmanager.h"
#else
#include <QWidget>
#include "gui/httpdownloadmanagergui.h"
#endif

class SpiceEphemerides : public QObject
{
    Q_OBJECT

public:

    explicit SpiceEphemerides(QObject *parentWidget = nullptr);
    bool download(const QStringList &emphemerides);
    bool checkDownloaded(const QStringList &emphemerides) const;
    QStringList getTargets(const QStringList &ephemerisURL);
    static QStringList getEphemeridesFiles(const QStringList &emphemeridesURLs);

private:

    static QString urlToFilename(const QString &ephemerisURL);

    QObject *m_parentWidget;
#ifdef SERVER_MODE
    HttpDownloadManager m_dlm;
#else
    HttpDownloadManagerGUI m_dlm;
#endif

    QStringList m_pendingDownloads;
    QStringList m_completedDownloads;

private slots:
    void downloadComplete(const QString &filename, bool success, const QString &url, const QString &errorMessage);

signals:
    void allDownloadsComplete();

};

#endif // INCLUDE_SPICE_EPHEMERIDES_H_
