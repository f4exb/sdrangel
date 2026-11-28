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

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QRegularExpression>

#include "releaseupdateservicegithub.h"

namespace
{
    const QString settingsGroup = "General/ReleaseCheck/GitHub";
    const QUrl releaseApiUrl(
        "https://api.github.com/repos/f4exb/sdrangel/releases/latest"
    );
    const QRegularExpression releaseTagPattern(
        QRegularExpression::anchoredPattern("v?(\\d+\\.\\d+\\.\\d+)")
    );
    const QRegularExpression etagPattern(
        QRegularExpression::anchoredPattern("(W/)?\"[!#-~]*\"")
    );
    const qint64 maxResponseSize = 1024 * 1024;
    const qint64 responseReadBufferSize = 64 * 1024;
    const qint64 maxRetryDelaySeconds = 24 * 60 * 60;

    bool isValidETag(const QString& etag)
    {
        return etag.size() <= 1024 &&
               etagPattern.match(etag).hasMatch();
    }

    const QRegularExpression releasePathPattern(
        QRegularExpression::anchoredPattern(
            "/[A-Za-z0-9][A-Za-z0-9-]*"           // owner
            "/[A-Za-z0-9_][A-Za-z0-9._-]*"        // repo
            "/releases/tag/[A-Za-z0-9._-]+"));    // tag

    bool isValidReleaseUrl(const QUrl& url)
    {
        return url.isValid() &&
               url.scheme() == "https" &&
               url.host() == "github.com" &&
               (url.port(-1) == -1 || url.port(-1) == 443) &&
               url.userInfo().isEmpty() &&
               url.query().isEmpty() &&
               url.fragment().isEmpty() &&
               releasePathPattern.match(url.path()).hasMatch();
    }

    // Seconds to wait until the given epoch time, clamped to
    // [0, maxRetryDelaySeconds] plus a 60 s safety margin.
    qint64 secondsUntilEpoch(qint64 epochSeconds)
    {
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const qint64 bounded = qBound(
            qint64(0), epochSeconds, now + maxRetryDelaySeconds);
        return qMax(qint64(0), bounded - now) + 60;
    }

    // Failures caused by the network being unavailable, which is expected
    // when offline and not worth a warning.
    bool isConnectivityError(QNetworkReply::NetworkError error)
    {
        switch (error)
        {
        case QNetworkReply::ConnectionRefusedError:
        case QNetworkReply::RemoteHostClosedError:
        case QNetworkReply::HostNotFoundError:
        case QNetworkReply::TimeoutError:
        case QNetworkReply::OperationCanceledError:
        case QNetworkReply::TemporaryNetworkFailureError:
        case QNetworkReply::NetworkSessionFailedError:
        case QNetworkReply::UnknownNetworkError:
        case QNetworkReply::ProxyConnectionRefusedError:
        case QNetworkReply::ProxyNotFoundError:
        case QNetworkReply::ProxyTimeoutError:
            return true;
        default:
            return false;
        }
    }

    // Removes the given keys from the GitHub cache settings
    void clearCachedSettings(const QStringList& keys)
    {
        QSettings settings;
        settings.beginGroup(settingsGroup);

        for (const QString& key : keys) {
            settings.remove(key);
        }

        settings.endGroup();
        settings.sync();

        if (settings.status() != QSettings::NoError) {
            qWarning() << "ReleaseCheck: failed to clear GitHub cache settings";
        }
    }
}

ReleaseUpdateServiceGitHub::ReleaseUpdateServiceGitHub(QObject *parent) :
    ReleaseUpdateService(parent)
{
    QSettings settings;
    settings.beginGroup(settingsGroup);
    m_etag = settings.value("ETag").toString();
    m_updateUrl = settings.value("UpdateUrl").toString();
    m_latestVersion = settings.value("LatestVersion").toString();
    settings.endGroup();

    if (!m_etag.isEmpty() && !isValidETag(m_etag))
    {
        m_etag.clear();
        clearCachedSettings({"ETag"});
    }

    const QRegularExpressionMatch cachedVersionMatch =
        releaseTagPattern.match(m_latestVersion);

    const QUrl cachedUrl(m_updateUrl);

    if (!cachedVersionMatch.hasMatch() ||
        !isValidReleaseUrl(cachedUrl))
    {
        const bool hadCachedData =
            !m_latestVersion.isEmpty() ||
            !m_updateUrl.isEmpty() ||
            !m_etag.isEmpty();

        // Clear memory
        m_latestVersion.clear();
        m_updateUrl.clear();
        m_etag.clear();

        // Clear settings
        if (hadCachedData) {
            clearCachedSettings({"LatestVersion", "UpdateUrl", "ETag"});
        }
    }
    else {
        m_latestVersion = cachedVersionMatch.captured(1);
    }

    connect(
        &m_networkAccessManager,
        &QNetworkAccessManager::finished,
        this,
        &ReleaseUpdateServiceGitHub::handleReply
    );

    if (!m_latestVersion.isEmpty() && !m_updateUrl.isEmpty())
    {
        QTimer::singleShot(0, this, [this]()
        {
            ReleaseInfo release;
            release.version = m_latestVersion;
            release.updateUrl = m_updateUrl;
            emit releaseAvailable(release);
        });
    }
}

QString ReleaseUpdateServiceGitHub::updateNotificationText() const
{
    return tr("SDRangel update available");
}

void ReleaseUpdateServiceGitHub::checkForUpdate()
{
    startRequest(false);
}

void ReleaseUpdateServiceGitHub::startRequest(bool retryWithoutETag)
{
    QNetworkRequest request(releaseApiUrl);

    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setRawHeader("User-Agent", "SDRangel");
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
    );
    request.setTransferTimeout(30000);   // 30 seconds

    if (!retryWithoutETag && !m_etag.isEmpty()) {
        request.setRawHeader("If-None-Match", m_etag.toUtf8());
    }

    qDebug().noquote() << "ReleaseCheck: checking" << releaseApiUrl
             << "ETag present:" << !m_etag.isEmpty();

    QNetworkReply *reply = m_networkAccessManager.get(request);

    reply->setReadBufferSize(responseReadBufferSize);
    reply->setProperty("releaseCheckResponse", QByteArray());
    reply->setProperty("releaseCheckResponseTooLarge", false);
    reply->setProperty("releaseCheckUnconditionalRetry", retryWithoutETag);
    connect(
        reply,
        &QNetworkReply::redirected,
        this,
        [](const QUrl &url)
        {
            qDebug() << "ReleaseCheck: GitHub API redirected to" << url;
        }
    );

    connect(
        reply,
        &QIODevice::readyRead,
        this,
        [reply]()
        {
            QByteArray response =
                reply->property("releaseCheckResponse").toByteArray();

            while (reply->bytesAvailable() > 0)
            {
                const QByteArray chunk = reply->read(
                    qMin(
                        reply->bytesAvailable(),
                        responseReadBufferSize
                    )
                );

                if (response.size() + chunk.size() > maxResponseSize)
                {
                    reply->setProperty(
                        "releaseCheckResponseTooLarge",
                        true
                    );
                    reply->abort();
                    return;
                }

                response.append(chunk);
            }

            reply->setProperty("releaseCheckResponse", response);
        }
    );
}

void ReleaseUpdateServiceGitHub::handleReply(QNetworkReply *reply)
{
    if (reply->property("releaseCheckResponseTooLarge").toBool())
    {
        reply->deleteLater();
        emit checkFailed("GitHub response exceeded maximum size", -1, QtWarningMsg);
        return;
    }

    // readyRead is not guaranteed to have delivered everything before
    // finished() fires, so pick up any remaining bytes here.
    QByteArray response = reply->property("releaseCheckResponse").toByteArray();
    const QByteArray tail = reply->readAll();

    if (response.size() + tail.size() > maxResponseSize)
    {
        reply->deleteLater();
        emit checkFailed("GitHub response exceeded maximum size", -1, QtWarningMsg);
        return;
    }

    response.append(tail);

    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    const QNetworkReply::NetworkError networkError = reply->error();
    const QString errorString = reply->errorString();

    qDebug() << "ReleaseCheck: response"
         << reply->url()
         << "HTTP:" << statusCode;

    if (handleRateLimit(reply, response, statusCode)) {
        return;
    }

    if (networkError != QNetworkReply::NoError)
    {
        const QString error = QString(
            "GitHub release check failed: HTTP %1, network error %2: %3")
            .arg(statusCode)
            .arg(networkError)
            .arg(errorString);

        reply->deleteLater();
        emit checkFailed(error, -1, isConnectivityError(networkError) ? QtDebugMsg : QtWarningMsg);
        return;
    }

    if (statusCode != 304 && statusCode != 200)
    {
        const QString error = QString(
            "GitHub release check failed: HTTP %1: %2")
            .arg(statusCode)
            .arg(errorString);

        reply->deleteLater();
        emit checkFailed(error);
        return;
    }

    if (statusCode == 304)
    {
        handleNotModified(reply);
        return;
    }

    handleRelease(reply, response);
}

// https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api#checking-the-status-of-your-rate-limit

bool ReleaseUpdateServiceGitHub::handleRateLimit(QNetworkReply *reply, const QByteArray& response, int statusCode)
{
    if (statusCode != 403 && statusCode != 429) {
        return false;
    }

    const QByteArray remaining = reply->rawHeader("X-RateLimit-Remaining").trimmed();
    const QByteArray reset = reply->rawHeader("X-RateLimit-Reset").trimmed();
    const QByteArray retryAfter = reply->rawHeader("Retry-After").trimmed();

    const QJsonDocument document = QJsonDocument::fromJson(response);
    const QString message = document.object().value("message").toString();

    const bool primaryRateLimit = remaining == "0";
    const bool secondaryRateLimit =
        statusCode == 429 ||
        !retryAfter.isEmpty() ||
        message.contains("secondary rate limit", Qt::CaseInsensitive);

    if (primaryRateLimit || secondaryRateLimit)
    {
        qint64 retryDelaySeconds = -1;
        QString error;

        if (primaryRateLimit)
        {
            error = "GitHub primary API rate limit exceeded";

            bool resetIsValid = false;
            const qint64 resetTime = reset.toLongLong(&resetIsValid);

            if (resetIsValid)
            {
                retryDelaySeconds = secondsUntilEpoch(resetTime);
                error += QString(" (reset timestamp: %1)")
                    .arg(QString::fromLatin1(reset));
            }
        }
        else
        {
            error = "GitHub secondary API rate limit exceeded";

            bool retryAfterIsValid = false;
            const qint64 retryAfterSeconds = retryAfter.toLongLong(&retryAfterIsValid);

            if (retryAfterIsValid && retryAfterSeconds >= 0) {
                retryDelaySeconds = qMin(retryAfterSeconds, maxRetryDelaySeconds);
            }
            else
            {
                const QDateTime retryTime = QDateTime::fromString(
                    QString::fromLatin1(retryAfter),
                    Qt::RFC2822Date
                );

                if (retryTime.isValid())
                {
                    retryDelaySeconds = secondsUntilEpoch(retryTime.toSecsSinceEpoch());
                }
                else {
                    retryDelaySeconds = 60 * 60;
                }
            }

            if (!retryAfter.isEmpty()) {
                error += QString(" (retry after %1 seconds)").arg(QString::fromLatin1(retryAfter));
            }
        }

        reply->deleteLater();
        emit checkFailed(error, retryDelaySeconds);
        return true;
    }

    return false;
}

// the requested webpage or file has not changed since you last loaded it.
void ReleaseUpdateServiceGitHub::handleNotModified(QNetworkReply *reply)
{
    const QRegularExpressionMatch cachedVersionMatch = releaseTagPattern.match(m_latestVersion);
    const QUrl cachedUrl(m_updateUrl);

    if (!cachedVersionMatch.hasMatch() ||
        !isValidReleaseUrl(cachedUrl))
    {
        if (reply->property("releaseCheckUnconditionalRetry").toBool())
        {
            reply->deleteLater();
            emit checkFailed("GitHub returned 304 without valid cached release information", -1, QtWarningMsg);
            return;
        }
        qWarning() << "ReleaseCheck: received 304 but cached release is invalid;"
                      " retrying without ETag";

        m_etag.clear();
        clearCachedSettings({"ETag"});

        reply->deleteLater();
        startRequest(true);
        return;
    }

    ReleaseInfo release;
    release.version = m_latestVersion;
    release.updateUrl = m_updateUrl;

    reply->deleteLater();
    emit checkCompleted();
    emit releaseAvailable(release);
}

void ReleaseUpdateServiceGitHub::handleRelease(
    QNetworkReply *reply,
    const QByteArray& response)
{
    const QJsonDocument document = QJsonDocument::fromJson(response);

    if (!document.isObject())
    {
        reply->deleteLater();
        emit checkFailed("GitHub returned invalid JSON");
        return;
    }

    const QJsonObject object = document.object();
    const QString tagName = object.value("tag_name").toString();
    const QString updateUrl = object.value("html_url").toString();

    const QRegularExpressionMatch match =
        releaseTagPattern.match(tagName);

    const QUrl url(updateUrl);

    if (!match.hasMatch() ||
        !isValidReleaseUrl(url))
    {
        const QString error = QString("GitHub returned invalid release information; tag = %1 url=%2")
            .arg(tagName.left(64)).arg(updateUrl.left(256));

        reply->deleteLater();
        emit checkFailed(error, -1 , QtWarningMsg);
        return;
    }

    ReleaseInfo release;
    release.version = match.captured(1);
    release.updateUrl = updateUrl;

    const QString newETag = QString::fromLatin1(
        reply->rawHeader("ETag")
    );
    m_etag = isValidETag(newETag) ? newETag : QString();

    qDebug().noquote() << "ReleaseCheck: latest release"
             << release.version
             << "ETag received:" << !newETag.isEmpty()
             << "ETag valid:" << !m_etag.isEmpty();

    reply->deleteLater();

    m_updateUrl = release.updateUrl;
    m_latestVersion = release.version;

    QSettings settings;
    settings.beginGroup(settingsGroup);
    settings.setValue("LatestVersion", m_latestVersion);
    settings.setValue("ETag", m_etag);
    settings.setValue("UpdateUrl", m_updateUrl);
    settings.endGroup();
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        qWarning() << "ReleaseCheck: failed to persist GitHub settings";
    }

    emit checkCompleted();
    emit releaseAvailable(release);
}

void ReleaseUpdateServiceGitHub::openUpdate()
{
    const QUrl url(m_updateUrl);

    if (!isValidReleaseUrl(url))
    {
        qWarning() << "ReleaseCheck: no valid update URL to open";
        return;
    }

    qDebug().noquote() << "ReleaseCheck: opening" << url;

    if (!QDesktopServices::openUrl(url)) {
        qWarning() << "ReleaseCheck: failed to open" << url;
    }
}
