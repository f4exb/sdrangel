///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "giro.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QJsonObject>

GIRO::GIRO()
{
    connect(&m_indexTimer, &QTimer::timeout, this, &GIRO::getIndex);
    connect(&m_dataTimer, &QTimer::timeout, this, &GIRO::getData);
    connect(&m_mufTimer, &QTimer::timeout, this, qOverload<>(&GIRO::getMUF));
    connect(&m_foF2Timer, &QTimer::timeout, this, qOverload<>(&GIRO::getfoF2));
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &GIRO::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("giro"))) {
        qDebug() << "Failed to create cache/giro";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("giro"));
    m_cache->setMaximumCacheSize(100000000);
    m_networkManager->setCache(m_cache);
}

GIRO::~GIRO()
{
    disconnect(&m_indexTimer, &QTimer::timeout, this, &GIRO::getIndex);
    disconnect(&m_dataTimer, &QTimer::timeout, this, &GIRO::getData);
    disconnect(&m_mufTimer, &QTimer::timeout, this, qOverload<>(&GIRO::getMUF));
    disconnect(&m_foF2Timer, &QTimer::timeout, this, qOverload<>(&GIRO::getfoF2));
    disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &GIRO::handleReply);
    delete m_networkManager;
}

GIRO* GIRO::create(const QString& service)
{
    if (service == "prop.kc2g.com")
    {
        return new GIRO();
    }
    else
    {
        qDebug() << "GIRO::create: Unsupported service: " << service;
        return nullptr;
    }
}

void GIRO::getIndexPeriodically(int periodInMins)
{
    if (periodInMins > 0)
    {
        m_indexTimer.setInterval(periodInMins*60*1000);
        m_indexTimer.start();
        getIndex();
    }
    else
    {
        m_indexTimer.stop();
    }
}

void GIRO::getDataPeriodically(int periodInMins)
{
    if (periodInMins > 0)
    {
        m_dataTimer.setInterval(periodInMins*60*1000);
        m_dataTimer.start();
        getData();
    }
    else
    {
        m_dataTimer.stop();
    }
}

void GIRO::getMUFPeriodically(int periodInMins)
{
    if (periodInMins > 0)
    {
        m_mufTimer.setInterval(periodInMins*60*1000);
        m_mufTimer.start();
        getMUF();
    }
    else
    {
        m_mufTimer.stop();
    }
}

void GIRO::getfoF2Periodically(int periodInMins)
{
    if (periodInMins > 0)
    {
        m_foF2Timer.setInterval(periodInMins*60*1000);
        m_foF2Timer.start();
        getfoF2();
    }
    else
    {
        m_foF2Timer.stop();
    }
}

void GIRO::getIndex()
{
    QUrl url(QString("https://prop.kc2g.com/api/available_nowcasts.json?days=5"));
    m_networkManager->get(QNetworkRequest(url));
}

void GIRO::getData()
{
    QUrl url(QString("https://prop.kc2g.com/api/stations.json"));
    m_networkManager->get(QNetworkRequest(url));
}

void GIRO::getfoF2()
{
    getMUF("current");
}

void GIRO::getMUF()
{
    getMUF("current");
}

void GIRO::getfoF2(const QString& runId)
{
    QUrl url(QString("https://prop.kc2g.com/renders/%1/fof2-normal-now.geojson").arg(runId));
    m_networkManager->get(QNetworkRequest(url));
}

void GIRO::getMUF(const QString& runId)
{
    QUrl url(QString("https://prop.kc2g.com/renders/%1/mufd-normal-now.geojson").arg(runId));
    m_networkManager->get(QNetworkRequest(url));
}

bool GIRO::containsNonNull(const QJsonObject& obj, const QString &key) const
{
    if (obj.contains(key))
    {
        QJsonValue val = obj.value(key);
        return !val.isNull();
    }
    return false;
}

void GIRO::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());

            QString fileName = reply->url().fileName();
            if (fileName == "available_nowcasts.json")
            {
                handleIndex(document);
            }
            else if (fileName == "stations.json")
            {
                handleStations(document);
            }
            else if (fileName == "mufd-normal-now.geojson")
            {
                emit mufUpdated(document);
            }
            else if (fileName == "fof2-normal-now.geojson")
            {
                emit foF2Updated(document);
            }
            else
            {
                qDebug() << "GIRO::handleReply: unexpected filename: " << fileName;
            }
        }
        else
        {
            qDebug() << "GIRO::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "GIRO::handleReply: reply is null";
    }
}

void GIRO::handleStations(QJsonDocument& document)
{
    if (document.isArray())
    {
        QJsonArray array = document.array();
        for (auto valRef : array)
        {
            if (valRef.isObject())
            {
                QJsonObject obj = valRef.toObject();

                GIROStationData data;

                if (obj.contains(QStringLiteral("station")))
                {
                    QJsonObject stationObj = obj.value(QStringLiteral("station")).toObject();

                    if (stationObj.contains(QStringLiteral("name"))) {
                        data.m_station = stationObj.value(QStringLiteral("name")).toString();
                    }
                    if (stationObj.contains(QStringLiteral("latitude"))) {
                        data.m_latitude = (float)stationObj.value(QStringLiteral("latitude")).toString().toFloat();
                    }
                    if (stationObj.contains(QStringLiteral("longitude"))) {
                        data.m_longitude = (float)stationObj.value(QStringLiteral("longitude")).toString().toFloat();
                        if (data.m_longitude >= 180.0f) {
                            data.m_longitude -= 360.0f;
                        }
                    }
                }

                if (containsNonNull(obj, QStringLiteral("time"))) {
                    data.m_dateTime = QDateTime::fromString(obj.value(QStringLiteral("time")).toString(), Qt::ISODateWithMs);
                }
                if (containsNonNull(obj, QStringLiteral("mufd"))) {
                    data.m_mufd = (float)obj.value(QStringLiteral("mufd")).toDouble();
                }
                if (containsNonNull(obj, QStringLiteral("md"))) {
                    data.m_md =  obj.value(QStringLiteral("md")).toString().toFloat();
                }
                if (containsNonNull(obj, QStringLiteral("tec"))) {
                    data.m_tec = (float)obj.value(QStringLiteral("tec")).toDouble();
                }
                if (containsNonNull(obj, QStringLiteral("fof2"))) {
                    data.m_foF2 = (float)obj.value(QStringLiteral("fof2")).toDouble();
                }
                if (containsNonNull(obj, QStringLiteral("hmf2"))) {
                    data.m_hmF2 = (float)obj.value(QStringLiteral("hmf2")).toDouble();
                }
                if (containsNonNull(obj, QStringLiteral("foe"))) {
                    data.m_foE = (float)obj.value(QStringLiteral("foe")).toDouble();
                }
                if (containsNonNull(obj, QStringLiteral("cs"))) {
                    data.m_confidence = (int)obj.value(QStringLiteral("cs")).toDouble();
                }

                emit dataUpdated(data);
            }
            else
            {
                qDebug() << "GIRO::handleReply: Array element is not an object: " << valRef;
            }
        }
    }
    else
    {
        qDebug() << "GIRO::handleReply: Document is not an array: " << document;
    }
}

void GIRO::handleIndex(QJsonDocument& document)
{
    if (document.isArray())
    {
        QJsonArray array = document.array();

        m_index.clear();

        for (auto valRef : array)
        {
            if (valRef.isObject())
            {
                QJsonObject obj = valRef.toObject();

                DataSet item;

                int ts = obj.value(QStringLiteral("ts")).toInt();
                item.m_dateTime = QDateTime::fromSecsSinceEpoch(ts);

                item.m_runId = QString::number(obj.value(QStringLiteral("run_id")).toInt());

                qDebug() << item.m_dateTime << item.m_runId;

                m_index.append(item);
            }
        }

        emit indexUpdated(m_index);
    }
}

QString GIRO::getRunId(const QDateTime& dateTime)
{
    // Index is ordered newest first
    for (int i = 0; i < m_index.size(); i++)
    {
        if (dateTime > m_index[i].m_dateTime) {
            return m_index[i].m_runId;
        }
    }
    return "";
}
