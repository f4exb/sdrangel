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

#include "httpdownloadmanagergui.h"

#include <QDebug>
#include <QProgressDialog>
#include <QMessageBox>

HttpDownloadManagerGUI::HttpDownloadManagerGUI()
{
    connect(this, &HttpDownloadManager::downloadComplete, this, &HttpDownloadManagerGUI::downloadCompleteGUI);
}

QNetworkReply *HttpDownloadManagerGUI::download(const QUrl &url, const QString &filename, QWidget *parent)
{
    QNetworkReply *reply = HttpDownloadManager::download(url, filename);
    if (parent != nullptr)
    {
        QProgressDialog *progressDialog = new QProgressDialog(parent);
        progressDialog->setAttribute(Qt::WA_DeleteOnClose);
        progressDialog->setCancelButton(nullptr);
        progressDialog->setMinimumDuration(500);
        progressDialog->setLabelText(QString("Downloading %1.").arg(url.toString()));
        m_progressDialogs.append(progressDialog);
        connect(reply, &QNetworkReply::downloadProgress, this, [progressDialog](qint64 bytesRead, qint64 totalBytes) {
            progressDialog->setMaximum(totalBytes);
            progressDialog->setValue(bytesRead);
        });
    }
    else
        m_progressDialogs.append(nullptr);
    return reply;
}

// Confirm if user wants to re-download an existing file
bool HttpDownloadManagerGUI::confirmDownload(const QString& filename, QWidget *parent, int maxAge)
{
    qint64 age = HttpDownloadManager::fileAgeInDays(filename);
    if ((age == -1) || (age > maxAge))
        return true;
    else
    {
        QMessageBox::StandardButton reply;
        if (age == 0)
            reply = QMessageBox::question(parent, "Confirm download", "This file was last downloaded today. Are you sure you wish to redownload it?", QMessageBox::Yes|QMessageBox::No);
        else if (age == 1)
            reply = QMessageBox::question(parent, "Confirm download", "This file was last downloaded yesterday. Are you sure you wish to redownload it?", QMessageBox::Yes|QMessageBox::No);
        else
            reply = QMessageBox::question(parent, "Confirm download", QString("This file was last downloaded %1 days ago. Are you sure you wish to redownload this file?").arg(age), QMessageBox::Yes|QMessageBox::No);
        return reply == QMessageBox::Yes;
    }
}

void HttpDownloadManagerGUI::downloadCompleteGUI(const QString& filename, bool success)
{
    (void) success;
    int idx = m_filenames.indexOf(filename);

    if (idx >= 0)
    {
        QProgressDialog *progressDialog = m_progressDialogs[idx];
        if (progressDialog != nullptr)
            progressDialog->close();
        m_filenames.remove(idx);
        m_progressDialogs.remove(idx);
    }
}
