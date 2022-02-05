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
    connect(this, &HttpDownloadManager::retryDownload, this, &HttpDownloadManagerGUI::retryDownload);
}

QNetworkReply *HttpDownloadManagerGUI::download(const QUrl &url, const QString &filename, QWidget *parent)
{
    m_filenames.append(filename);
    QNetworkReply *reply = HttpDownloadManager::download(url, filename);
    if (parent != nullptr)
    {
        QProgressDialog *progressDialog = new QProgressDialog(parent);
        progressDialog->setCancelButton(nullptr);
        progressDialog->setMinimumDuration(500);
        progressDialog->setLabelText(QString("Downloading: %1\nTo: %2.").arg(url.toString(), filename));
        Qt::WindowFlags flags = progressDialog->windowFlags();
        flags |= Qt::CustomizeWindowHint;
        flags &= ~Qt::WindowCloseButtonHint;
        flags &= ~Qt::WindowContextHelpButtonHint;
        progressDialog->setWindowFlags(flags);
        m_progressDialogs.append(progressDialog);
        connect(reply, &QNetworkReply::downloadProgress, this, [progressDialog](qint64 bytesRead, qint64 totalBytes) {
            if (progressDialog)
            {
                progressDialog->setMaximum(totalBytes);
                progressDialog->setValue(bytesRead);
            }
        });
    }
    else
    {
        m_progressDialogs.append(nullptr);
    }
    return reply;
}

// Confirm if user wants to re-download an existing file
bool HttpDownloadManagerGUI::confirmDownload(const QString& filename, QWidget *parent, int maxAge)
{
    qint64 age = HttpDownloadManager::fileAgeInDays(filename);
    if ((age == -1) || (age > maxAge))
    {
        return true;
    }
    else
    {
        QMessageBox::StandardButton reply;
        if (age == 0) {
            reply = QMessageBox::question(parent, "Confirm download", "This file was last downloaded today. Are you sure you wish to redownload it?", QMessageBox::Yes|QMessageBox::No);
        } else if (age == 1) {
            reply = QMessageBox::question(parent, "Confirm download", "This file was last downloaded yesterday. Are you sure you wish to redownload it?", QMessageBox::Yes|QMessageBox::No);
        } else {
            reply = QMessageBox::question(parent, "Confirm download", QString("This file was last downloaded %1 days ago. Are you sure you wish to redownload this file?").arg(age), QMessageBox::Yes|QMessageBox::No);
        }
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
        {
            progressDialog->close();
            delete progressDialog;
        }
        m_filenames.remove(idx);
        m_progressDialogs.remove(idx);
    }
}

void HttpDownloadManagerGUI::retryDownload(const QString &filename, QNetworkReply *oldReply, QNetworkReply *newReply)
{
    (void) oldReply;

    int idx = m_filenames.indexOf(filename);
    if (idx >= 0)
    {
        QProgressDialog *progressDialog = m_progressDialogs[idx];
        if (progressDialog != nullptr)
        {
            connect(newReply, &QNetworkReply::downloadProgress, this, [progressDialog](qint64 bytesRead, qint64 totalBytes) {
                if (progressDialog)
                {
                    progressDialog->setMaximum(totalBytes);
                    progressDialog->setValue(bytesRead);
                }
            });
        }
    }
}
