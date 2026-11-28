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

#ifndef INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATESERVICE_H
#define INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATESERVICE_H

#include <QDebug>
#include <QObject>
#include <QString>

class ReleaseUpdateService : public QObject
{
    Q_OBJECT

public:
    struct ReleaseInfo
    {
        QString version;
        QString updateUrl;
    };

    explicit ReleaseUpdateService(QObject *parent = nullptr) :
        QObject(parent)
    {}

    // Starts an asynchronous check. Each invocation must eventually emit
    // exactly one of checkCompleted() or checkFailed().
    virtual void checkForUpdate() = 0;
    virtual void openUpdate() = 0;
    virtual QString updateNotificationText() const = 0;

signals:
    // Emitted when valid release information is available.
    // May be emitted from cached information without a network check.
    void releaseAvailable(const ReleaseInfo& release);

    // Emitted when a requested check completes successfully.
    // Emit before releaseAvailable() when the check provides new information.
    void checkCompleted();

    // Emitted when a requested check fails.
    void checkFailed(const QString& error, qint64 retryDelaySeconds = -1, QtMsgType level = QtDebugMsg);
};

ReleaseUpdateService *createReleaseUpdateService(QObject *parent = nullptr);

#endif // INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATESERVICE_H
