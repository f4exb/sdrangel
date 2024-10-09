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

#include "sdrangelserverlist.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QRegularExpression>

SDRangelServerList::SDRangelServerList()
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(m_networkManager, &QNetworkAccessManager::finished, this, &SDRangelServerList::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("sdrangelserver"))) {
        qDebug() << "Failed to create cache/sdrangelserver";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("sdrangelserver"));
    m_cache->setMaximumCacheSize(100000000);
    m_networkManager->setCache(m_cache);

    connect(&m_timer, &QTimer::timeout, this, &SDRangelServerList::update);
}

SDRangelServerList::~SDRangelServerList()
{
    QObject::disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &SDRangelServerList::handleReply);
    delete m_networkManager;
}

void SDRangelServerList::getData()
{
    QUrl url = QUrl("https://sdrangel.org/websdr/websdrs.json");
    m_networkManager->get(QNetworkRequest(url));
}

void SDRangelServerList::getDataPeriodically(int periodInMins)
{
    m_timer.setInterval(periodInMins*60*1000);
    m_timer.start();
    update();
}

void SDRangelServerList::update()
{
    getData();
}

void SDRangelServerList::handleReply(QNetworkReply* reply)
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
            qDebug() << "SDRangelServerList::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "SDRangelServerList::handleReply: reply is null";
    }
}

void SDRangelServerList::handleJSON(const QString& url, const QByteArray& bytes)
{
    (void) url;

    QList<SDRangelServer> sdrs;
    QJsonDocument document = QJsonDocument::fromJson(bytes);

    if (document.isArray())
    {
        QJsonArray servers = document.array();

        for (auto valRef : servers)
        {
            if (valRef.isObject())
            {
                QJsonObject serverObj = valRef.toObject();
                SDRangelServer sdr;

                if (serverObj.contains(QStringLiteral("address"))) {
                    sdr.m_address = serverObj.value(QStringLiteral("address")).toString();
                }
                if (serverObj.contains(QStringLiteral("port"))) {
                    sdr.m_port = serverObj.value(QStringLiteral("port")).toInt();
                }
                if (serverObj.contains(QStringLiteral("protocol"))) {
                    sdr.m_protocol = serverObj.value(QStringLiteral("protocol")).toString();
                }
                if (serverObj.contains(QStringLiteral("minFrequency"))) {
                    sdr.m_minFrequency = serverObj.value(QStringLiteral("minFrequency")).toInt();
                }
                if (serverObj.contains(QStringLiteral("maxFrequency"))) {
                    sdr.m_maxFrequency = serverObj.value(QStringLiteral("maxFrequency")).toInt();
                }
                if (serverObj.contains(QStringLiteral("maxSampleRate"))) {
                    sdr.m_maxSampleRate = serverObj.value(QStringLiteral("maxSampleRate")).toInt();
                }
                if (serverObj.contains(QStringLiteral("device"))) {
                    sdr.m_device = serverObj.value(QStringLiteral("device")).toString();
                }
                if (serverObj.contains(QStringLiteral("antenna"))) {
                    sdr.m_antenna = serverObj.value(QStringLiteral("antenna")).toString();
                }
                if (serverObj.contains(QStringLiteral("remoteControl"))) {
                    sdr.m_remoteControl = serverObj.value(QStringLiteral("remoteControl")).toInt() == 1;
                }
                if (serverObj.contains(QStringLiteral("stationName"))) {
                    sdr.m_stationName = serverObj.value(QStringLiteral("stationName")).toString();
                }
                if (serverObj.contains(QStringLiteral("location"))) {
                    sdr.m_location = serverObj.value(QStringLiteral("location")).toString();
                }
                if (serverObj.contains(QStringLiteral("latitude"))) {
                    sdr.m_latitude = serverObj.value(QStringLiteral("latitude")).toDouble();
                }
                if (serverObj.contains(QStringLiteral("longitude"))) {
                    sdr.m_longitude = serverObj.value(QStringLiteral("longitude")).toDouble();
                }
                if (serverObj.contains(QStringLiteral("altitude"))) {
                    sdr.m_altitude = serverObj.value(QStringLiteral("altitude")).toDouble();
                }
                if (serverObj.contains(QStringLiteral("isotropic"))) {
                    sdr.m_isotropic = serverObj.value(QStringLiteral("isotropic")).toInt() == 1;
                }
                if (serverObj.contains(QStringLiteral("azimuth"))) {
                    sdr.m_azimuth = serverObj.value(QStringLiteral("azimuth")).toDouble();
                }
                if (serverObj.contains(QStringLiteral("elevation"))) {
                    sdr.m_elevation = serverObj.value(QStringLiteral("elevation")).toDouble();
                }
                if (serverObj.contains(QStringLiteral("clients"))) {
                    sdr.m_clients = serverObj.value(QStringLiteral("clients")).toInt();
                }
                if (serverObj.contains(QStringLiteral("maxClients"))) {
                    sdr.m_maxClients = serverObj.value(QStringLiteral("maxClients")).toInt();
                }
                if (serverObj.contains(QStringLiteral("timeLimit"))) {
                    sdr.m_timeLimit = serverObj.value(QStringLiteral("timeLimit")).toInt();
                }

                sdrs.append(sdr);
            }
            else
            {
                qDebug() << "SDRangelServerList::handleJSON: Element not an object:\n" << valRef;
            }
        }
    }
    else
    {
        qDebug() << "SDRangelServerList::handleJSON: Doc doesn't contain an array:\n" << document;
    }

    emit dataUpdated(sdrs);
}
