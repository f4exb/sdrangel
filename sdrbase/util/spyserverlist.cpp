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

#include "spyserverlist.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QNetworkDiskCache>
#include <QRegularExpression>

SpyServerList::SpyServerList()
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(m_networkManager, &QNetworkAccessManager::finished, this, &SpyServerList::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("spyserver"))) {
        qDebug() << "Failed to create cache/spyserver";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("spyserver"));
    m_cache->setMaximumCacheSize(100000000);
    m_networkManager->setCache(m_cache);

    connect(&m_timer, &QTimer::timeout, this, &SpyServerList::update);
}

SpyServerList::~SpyServerList()
{
    QObject::disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &SpyServerList::handleReply);
    delete m_networkManager;
}

void SpyServerList::getData()
{
    QUrl url(QString("https://airspy.com/directory/status.json"));
    m_networkManager->get(QNetworkRequest(url));
}

void SpyServerList::getDataPeriodically(int periodInMins)
{
    m_timer.setInterval(periodInMins*60*1000);
    m_timer.start();
    update();
}

void SpyServerList::update()
{
    getData();
}

void SpyServerList::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QString url = reply->url().toEncoded().constData();
            QByteArray bytes = reply->readAll();

            handleJSON(url, bytes);
        }
        else
        {
            qDebug() << "SpyServerList::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "SpyServerList::handleReply: reply is null";
    }
}

void SpyServerList::handleJSON(const QString& url, const QByteArray& bytes)
{
    (void) url;

    QList<SpyServer> sdrs;
    QJsonDocument document = QJsonDocument::fromJson(bytes);

    if (document.isObject())
    {
        QJsonObject obj = document.object();
        if (obj.contains(QStringLiteral("servers")))
        {
            QJsonArray servers = obj.value(QStringLiteral("servers")).toArray();

            for (auto valRef : servers)
            {
                if (valRef.isObject())
                {
                     QJsonObject serverObj = valRef.toObject();
                     SpyServer sdr;

                     if (serverObj.contains(QStringLiteral("generalDescription"))) {
                         sdr.m_generalDescription = serverObj.value(QStringLiteral("generalDescription")).toString();
                     }
                     if (serverObj.contains(QStringLiteral("deviceType"))) {
                         sdr.m_deviceType = serverObj.value(QStringLiteral("deviceType")).toString();
                     }
                     if (serverObj.contains(QStringLiteral("streamingHost"))) {
                         sdr.m_streamingHost = serverObj.value(QStringLiteral("streamingHost")).toString();
                     }
                     if (serverObj.contains(QStringLiteral("streamingPort"))) {
                         sdr.m_streamingPort = serverObj.value(QStringLiteral("streamingPort")).toInt();
                     }
                     if (serverObj.contains(QStringLiteral("currentClientCount"))) {
                         sdr.m_currentClientCount = serverObj.value(QStringLiteral("currentClientCount")).toInt();
                     }
                     if (serverObj.contains(QStringLiteral("maxClients"))) {
                         sdr.m_maxClients = serverObj.value(QStringLiteral("maxClients")).toInt();
                     }
                     if (serverObj.contains(QStringLiteral("antennaType"))) {
                         sdr.m_antennaType = serverObj.value(QStringLiteral("antennaType")).toString();
                     }
                     if (serverObj.contains(QStringLiteral("antennaLocation")))
                     {
                         QJsonObject location = serverObj.value(QStringLiteral("antennaLocation")).toObject();
                         sdr.m_latitude = location.value(QStringLiteral("lat")).toDouble();
                         sdr.m_longitude = location.value(QStringLiteral("long")).toDouble();
                     }
                     if (serverObj.contains(QStringLiteral("minimumFrequency"))) {
                         sdr.m_minimumFrequency = serverObj.value(QStringLiteral("minimumFrequency")).toInt();
                     }
                     if (serverObj.contains(QStringLiteral("maximumFrequency"))) {
                         sdr.m_maximumFrequency = serverObj.value(QStringLiteral("maximumFrequency")).toInt();
                     }
                     if (serverObj.contains(QStringLiteral("fullControlAllowed"))) {
                         sdr.m_fullControlAllowed = serverObj.value(QStringLiteral("fullControlAllowed")).toBool();
                     }
                     if (serverObj.contains(QStringLiteral("online"))) {
                         sdr.m_online = serverObj.value(QStringLiteral("online")).toBool();
                     }

                     sdrs.append(sdr);
                }
            }
        }
    }
    else
    {
        qDebug() << "SpyServerList::handleJSON: Doc doesn't contain an object:\n" << document;
    }

    emit dataUpdated(sdrs);
}
