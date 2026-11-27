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

#ifndef INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATECHECKER_H
#define INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATECHECKER_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>

#include "releaseupdateservice.h"

class ReleaseUpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit ReleaseUpdateChecker(
        ReleaseUpdateService *service,
        const QString& runningVersion,
        QObject *parent = nullptr
    );

    bool updateAvailable() const;
    void checkIfDue();
    void openUpdate();
    QString updateNotificationText() const;

signals:
    void updateAvailabilityChanged(bool available);

private:
    void evaluateUpdate(const QString& latestVersion);
    qint64 secondsUntilNextCheck(const QDateTime& now) const;

    ReleaseUpdateService *m_service;
    QString m_runningVersion;
    QDateTime m_lastCheckTime;
    QTimer m_checkTimer;
    QTimer m_retryTimer;
    QTimer m_startupTimer;
    bool m_updateAvailable;
    bool m_runningIsNewer;
    bool m_hasCheckedThisSession;
    bool m_checkInProgress;
    bool m_retryDue;
    bool m_startupDelayElapsed;
};

#endif // INCLUDE_SDRGUI_RELEASECHECK_RELEASEUPDATECHECKER_H
