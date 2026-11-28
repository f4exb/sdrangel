///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                                //
// written by Robin Getz <rgetz503@gmail.com>                                        //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATESERVICEGITHUB_H
#define INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATESERVICEGITHUB_H

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QString>

#include "releaseupdateservice.h"

class QNetworkReply;

class ReleaseUpdateServiceGitHub : public ReleaseUpdateService
{
    Q_OBJECT

public:
    explicit ReleaseUpdateServiceGitHub(QObject *parent = nullptr);

    void checkForUpdate() override;
    void openUpdate() override;
    QString updateNotificationText() const override;

private:
    void handleReply(QNetworkReply *reply);
    bool handleRateLimit(QNetworkReply *reply, const QByteArray& response, int statusCode);
    void handleNotModified(QNetworkReply *reply);
    void handleRelease(QNetworkReply *reply, const QByteArray& response);
    void startRequest(bool retryWithoutETag);

    QNetworkAccessManager m_networkAccessManager;
    QString m_updateUrl;
    QString m_etag;
    QString m_latestVersion;
};

#endif // INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATESERVICEGITHUB_H
