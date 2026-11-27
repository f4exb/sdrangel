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

#include <limits>
#include <QDebug>
#include <QRegularExpression>
#include <QSettings>
#include <QVersionNumber>

#include "releaseupdatechecker.h"

namespace
{
    const qint64 checkInterval = 24 * 60 * 60;      // 24 hours
    const qint64 retryInterval = 6 * 60 * 60;       //  6 hours
    const int checkTimerInterval = 60 * 60 * 1000;  //  1 hour in milliseconds
    const qint64 minRetryDelay = 60;                //  1 minute
    const qint64 maxRetryDelay = 24 * 60 * 60;      // 24 hours
    const int startupDelay = 10 * 1000;             // first check not before 10 s after startup (ms)

    const QString settingsGroup = "General/ReleaseCheck";
    const QRegularExpression versionPattern(
        QRegularExpression::anchoredPattern(
            "v?(\\d+\\.\\d+\\.\\d+)(?:[-+].*)?"
        )
    );
}

ReleaseUpdateChecker::ReleaseUpdateChecker(
    ReleaseUpdateService *service,
    const QString& runningVersion,
    QObject *parent
) :
    QObject(parent),
    m_service(service),
    m_runningVersion(
        versionPattern.match(runningVersion).hasMatch()
            ? versionPattern.match(runningVersion).captured(1)
            : runningVersion
    ),
    m_updateAvailable(false),
    m_runningIsNewer(false),
    m_hasCheckedThisSession(false),
    m_checkInProgress(false),
    m_retryDue(false),
    m_startupDelayElapsed(false)
{

    QSettings loadSettings;
    loadSettings.beginGroup(settingsGroup);
    m_lastCheckTime = loadSettings.value("LastCheckTime").toDateTime();

    const QDateTime loadTime = QDateTime::currentDateTimeUtc();

    if (m_lastCheckTime.isValid() && m_lastCheckTime > loadTime) {
        qWarning() << "ReleaseCheck: saved check time is in the future; resetting it";
        m_lastCheckTime = QDateTime();
        loadSettings.remove("LastCheckTime");
        loadSettings.sync();
    }
    loadSettings.endGroup();

    connect(
        m_service,
        &ReleaseUpdateService::checkCompleted,
        this,
        [this]()
        {
            m_checkInProgress = false;
            m_hasCheckedThisSession = true;
            m_runningIsNewer = false;
            m_lastCheckTime = QDateTime::currentDateTimeUtc();

            QSettings settings;
            settings.beginGroup(settingsGroup);
            settings.setValue("LastCheckTime", m_lastCheckTime);
            settings.endGroup();
            settings.sync();

            m_retryTimer.stop();

            if (settings.status() != QSettings::NoError) {
                qWarning() << "ReleaseCheck: failed to save check time";
            }
        }
    );

    connect(
        m_service,
        &ReleaseUpdateService::checkFailed,
        this,
        [this](const QString& error, qint64 retryDelaySeconds, QtMsgType level)
        {
            m_checkInProgress = false;

            const qint64 effectiveRetryDelay = retryDelaySeconds >= 0
                ? qBound(minRetryDelay, retryDelaySeconds, maxRetryDelay)
                : retryInterval;

            const qint64 maxTimerDelaySeconds =
                std::numeric_limits<int>::max() / 1000;
            const int retryDelayMilliseconds = static_cast<int>(
                qMin(effectiveRetryDelay, maxTimerDelaySeconds) * 1000
            );

            m_retryTimer.start(retryDelayMilliseconds);

            const qint64 retryHours = effectiveRetryDelay / 3600;
            const qint64 retryMinutes = (effectiveRetryDelay % 3600) / 60;
            const qint64 retrySeconds = effectiveRetryDelay % 60;

            QString retryTime;
            if (retryHours > 0) {
                retryTime = QString("%1h %2m %3s").arg(retryHours).arg(retryMinutes).arg(retrySeconds);
            } else if (retryMinutes > 0) {
                retryTime = QString("%1m %2s").arg(retryMinutes).arg(retrySeconds);
            } else {
                retryTime = QString("%1s").arg(retrySeconds);
            }
            if (level == QtDebugMsg) {
                qDebug().noquote() << "ReleaseCheck:" << error << "retry in" << retryTime;
            } else {
                qWarning().noquote() << "ReleaseCheck:" << error << "retry in" << retryTime;
            }
        }
    );

    connect(
        m_service,
        &ReleaseUpdateService::releaseAvailable,
        this,
        [this](const ReleaseUpdateService::ReleaseInfo& release)
        {
            evaluateUpdate(release.version);

            const QDateTime now = QDateTime::currentDateTimeUtc();
            const qint64 remaining = secondsUntilNextCheck(now);

            const qint64 remainingMinutes = (remaining + 59) / 60;
            const QString nextCheck = m_checkInProgress
                ? "check in progress"
                : remaining == 0
                    ? "eligible now"
                    : QString("%1h %2m")
                        .arg(remainingMinutes / 60)
                        .arg(remainingMinutes % 60);

            qDebug().noquote() << "ReleaseCheck: running:" << m_runningVersion
                     << "available:" << release.version
                     << "next check:" << nextCheck;

            if (m_runningIsNewer) {
                checkIfDue();
            }
        }
    );

    connect(
        &m_checkTimer,
        &QTimer::timeout,
        this,
        &ReleaseUpdateChecker::checkIfDue
    );

    m_retryTimer.setSingleShot(true);
    connect(
        &m_retryTimer,
        &QTimer::timeout,
        this,
        [this]()
        {
            m_retryDue = true;
            checkIfDue();
        }
    );

    if (!versionPattern.match(runningVersion).hasMatch()) {
        qWarning() << "ReleaseCheck: running version" << runningVersion
                   << "is not in X.Y.Z form; update notifications are disabled";
    }

    m_startupTimer.setSingleShot(true);
    connect(
        &m_startupTimer,
        &QTimer::timeout,
        this,
        [this]()
        {
            m_startupDelayElapsed = true;
            checkIfDue();
        }
    );

    m_startupTimer.start(startupDelay);
    m_checkTimer.start(checkTimerInterval);
}

QString ReleaseUpdateChecker::updateNotificationText() const
{
    return m_service->updateNotificationText();
}

bool ReleaseUpdateChecker::updateAvailable() const
{
    return m_updateAvailable;
}

qint64 ReleaseUpdateChecker::secondsUntilNextCheck(const QDateTime& now) const
{
    if (m_runningIsNewer || !m_lastCheckTime.isValid()) {
        return 0;
    }

    return qMax(
        qint64(0),
        checkInterval - m_lastCheckTime.secsTo(now)
    );
}

void ReleaseUpdateChecker::evaluateUpdate(const QString& latestVersion)
{
    const QRegularExpressionMatch runningMatch =
        versionPattern.match(m_runningVersion);
    const QRegularExpressionMatch latestMatch =
        versionPattern.match(latestVersion);

    bool available = false;
    m_runningIsNewer = false;

    if (runningMatch.hasMatch() && latestMatch.hasMatch()) {
        const QVersionNumber running =
            QVersionNumber::fromString(runningMatch.captured(1));
        const QVersionNumber latest =
            QVersionNumber::fromString(latestMatch.captured(1));

        const int comparison = QVersionNumber::compare(running, latest);
        m_runningIsNewer = (comparison > 0);
        available = comparison < 0;
    }

    if (available != m_updateAvailable) {
        m_updateAvailable = available;
        emit updateAvailabilityChanged(available);
    }
}

void ReleaseUpdateChecker::checkIfDue()
{
    if (!m_startupDelayElapsed || m_checkInProgress || m_retryTimer.isActive()) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (!m_retryDue && secondsUntilNextCheck(now) > 0) {
        return;
    }

    m_retryDue = false;
    m_checkInProgress = true;

    qDebug() << "ReleaseCheck: starting update check";
    m_service->checkForUpdate();
}

void ReleaseUpdateChecker::openUpdate()
{
    m_service->openUpdate();
}
