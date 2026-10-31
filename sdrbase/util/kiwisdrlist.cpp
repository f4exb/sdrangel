///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "kiwisdrlist.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkDiskCache>
#include <QRegularExpression>

#include "util/corsproxy.h"

KiwiSDRList::KiwiSDRList()
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(m_networkManager, &QNetworkAccessManager::finished, this, &KiwiSDRList::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("kiwisdr"))) {
        qDebug() << "Failed to create cache/kiwisdr";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("kiwisdr"));
    m_cache->setMaximumCacheSize(100000000);
    m_networkManager->setCache(m_cache);

    connect(&m_timer, &QTimer::timeout, this, &KiwiSDRList::update);
}

KiwiSDRList::~KiwiSDRList()
{
    QObject::disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &KiwiSDRList::handleReply);
    delete m_networkManager;
}

void KiwiSDRList::getData()
{
    // kiwisdr.com/public/ now sits behind a "Click to show KiwiSDRs" page, so use
    // the list that rx.linkfanel.net generates from it
#ifdef __EMSCRIPTEN__
    // rx.linkfanel.net doesn't support https, but it's needed for Emscripten - our CORS proxy handles it
    QUrl url = CORSProxy::adjustHost(QUrl("https://rx.linkfanel.net/kiwisdr_com.js"));
#else
    QUrl url = CORSProxy::adjustHost(QUrl("http://rx.linkfanel.net/kiwisdr_com.js"));
#endif
    m_networkManager->get(QNetworkRequest(url));
}

void KiwiSDRList::getDataPeriodically(int periodInMins)
{
    if (periodInMins > 0)
    {
        m_timer.setInterval(periodInMins*60*1000);
        m_timer.start();
        update();
    }
    else
    {
        m_timer.stop();
    }
}

void KiwiSDRList::update()
{
    getData();
}

void KiwiSDRList::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QString url = reply->url().toEncoded().constData();
            QByteArray bytes = reply->readAll();

            handleJS(url, bytes);
        }
        else
        {
            qDebug() << "KiwiSDRList::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "KiwiSDRList::handleReply: reply is null";
    }
}

void KiwiSDRList::handleJS(const QString& url, const QByteArray& bytes)
{
    (void) url;

    QList<KiwiSDR> sdrs;

    // File is JavaScript: "var kiwisdr_com = [ {...}, {...}, ];"
    // Extract the array and drop the trailing comma, so it can be parsed as JSON
    int start = bytes.indexOf('[');
    int end = bytes.lastIndexOf('}');

    if ((start < 0) || (end < start))
    {
        qDebug() << "KiwiSDRList::handleJS: No receiver array found";
        return;
    }

    QByteArray json = bytes.mid(start, end - start + 1) + "]";
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(json, &error);

    if (!document.isArray())
    {
        qDebug() << "KiwiSDRList::handleJS: Failed to parse:" << error.errorString() << "at offset" << error.offset;
        return;
    }

    QJsonArray array = document.array();
    QRegularExpression freqRe("([\\d]+)-([\\d]+)");
    QRegularExpression gpsRe("([\\d.+-]+), ([\\d.+-]+)");

    for (const auto& value : array)
    {
        QJsonObject obj = value.toObject();
        KiwiSDR sdr{};

        sdr.m_url = obj.value("url").toString();
        if (sdr.m_url.isEmpty()) {
            continue;
        }

        sdr.m_status = obj.value("status").toString();
        sdr.m_offline = obj.value("offline").toString();
        sdr.m_name = obj.value("name").toString();
        sdr.m_sdrHW = obj.value("sdr_hw").toString();

        QRegularExpressionMatch freqMatch = freqRe.match(obj.value("bands").toString());

        if (freqMatch.hasMatch())
        {
            sdr.m_lowFrequency = freqMatch.captured(1).toLongLong();
            sdr.m_highFrequency = freqMatch.captured(2).toLongLong();
        }

        sdr.m_users = obj.value("users").toString().toInt();
        sdr.m_usersMax = obj.value("users_max").toString().toInt();

        QRegularExpressionMatch gpsMatch = gpsRe.match(obj.value("gps").toString());

        if (gpsMatch.hasMatch())
        {
            sdr.m_latitude = gpsMatch.captured(1).toFloat();
            sdr.m_longitude = gpsMatch.captured(2).toFloat();
        }

        sdr.m_altitude = obj.value("asl").toString().toInt();
        sdr.m_location = obj.value("loc").toString();
        sdr.m_antenna = obj.value("antenna").toString();
        sdr.m_antennaConnected = obj.value("ant_connected").toString() == "1";
        sdr.m_snr = obj.value("snr").toString();

        sdrs.append(sdr);
    }

    emit dataUpdated(sdrs);
}
