///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_HTTPDOWNLOADMANAGERGUI_H
#define INCLUDE_HTTPDOWNLOADMANAGERGUI_H

#include "util/httpdownloadmanager.h"

#include "export.h"

class QWidget;
class QProgressDialog;

// Class to download files via http and write them to disk with progress dialog
class SDRGUI_API HttpDownloadManagerGUI : public HttpDownloadManager
{
public:
    HttpDownloadManagerGUI();
    QNetworkReply *download(const QUrl &url, const QString &filename, QWidget *parent = nullptr);

    static bool confirmDownload(const QString& filename, QWidget *parent = nullptr, int maxAge = 100);

private:
    QVector<QString> m_filenames;
    QVector<QProgressDialog *> m_progressDialogs;

public slots:
    void downloadCompleteGUI(const QString &filename, bool success);
    void retryDownload(const QString &filename, QNetworkReply *oldReply, QNetworkReply *newReply);

};

#endif /* INCLUDE_HTTPDOWNLOADMANAGERGUI_H */
