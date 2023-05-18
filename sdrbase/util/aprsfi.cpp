///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include "aprsfi.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>

QMutex APRSFi::m_mutex;
QHash<QString, APRSFi::AISData> APRSFi::m_aisCache;

APRSFi::APRSFi(const QString& apiKey, int cacheValidMins) :
    m_apiKey(apiKey),
    m_cacheValidMins(cacheValidMins)
{
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &APRSFi::handleReply);
}

APRSFi::~APRSFi()
{
    disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &APRSFi::handleReply);
    delete m_networkManager;
}

APRSFi* APRSFi::create(const QString& apiKey, int cacheValidMins)
{
    return new APRSFi(apiKey, cacheValidMins);
}

void APRSFi::getData(const QStringList& names)
{
    QStringList nonCachedNames;
    QDateTime currentDateTime = QDateTime::currentDateTime();

    QMutexLocker locker(&m_mutex);
    for (const auto& name : names)
    {
        bool cached = false;
        QList<AISData> dataList;
        if (m_aisCache.contains(name))
        {
            const AISData& d = m_aisCache[name];
            if (d.m_dateTime.secsTo(currentDateTime) < m_cacheValidMins*60)
            {
                dataList.append(d);
                cached = true;
            }
        }
        if (dataList.size() > 0) {
            emit dataUpdated(dataList);
        }
        if (!cached) {
            nonCachedNames.append(name);
        }
    }
    if (nonCachedNames.size() > 0)
    {
        QString nameList = nonCachedNames.join(",");
        QUrl url(QString("https://api.aprs.fi/api/get"));
        QUrlQuery query;
        query.addQueryItem("name", nameList);
        query.addQueryItem("what", "loc");
        query.addQueryItem("apikey", m_apiKey);
        query.addQueryItem("format", "json");
        url.setQuery(query);
        m_networkManager->get(QNetworkRequest(url));
    }
}

void APRSFi::getData(const QString& name)
{
    QStringList names;
    names.append(name);
    getData(names);
}

bool APRSFi::containsNonNull(const QJsonObject& obj, const QString &key) const
{
    if (obj.contains(key))
    {
        QJsonValue val = obj.value(key);
        return !val.isNull();
    }
    return false;
}

void APRSFi::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());

            if (document.isObject())
            {
                QJsonObject docObj = document.object();
                QDateTime receivedDateTime = QDateTime::currentDateTime();

                if (docObj.contains(QStringLiteral("entries")))
                {
                    QJsonArray array = docObj.value(QStringLiteral("entries")).toArray();

                    QList<AISData> data;
                    for (auto valRef : array)
                    {
                        if (valRef.isObject())
                        {
                            QJsonObject obj = valRef.toObject();

                            AISData measurement;

                            measurement.m_dateTime = receivedDateTime;
                            if (obj.contains(QStringLiteral("name"))) {
                                measurement.m_name = obj.value(QStringLiteral("name")).toString();
                            }
                            if (obj.contains(QStringLiteral("mmsi"))) {
                                measurement.m_mmsi = obj.value(QStringLiteral("mmsi")).toString();
                            }
                            if (containsNonNull(obj, QStringLiteral("time"))) {
                                measurement.m_firstTime = QDateTime::fromString(obj.value(QStringLiteral("time")).toString(), Qt::ISODate);
                            }
                            if (containsNonNull(obj, QStringLiteral("lastTime"))) {
                                measurement.m_lastTime = QDateTime::fromString(obj.value(QStringLiteral("lastTime")).toString(), Qt::ISODate);
                            }
                            if (containsNonNull(obj, QStringLiteral("lat"))) {
                                measurement.m_latitude = obj.value(QStringLiteral("lat")).toDouble();
                            }
                            if (containsNonNull(obj, QStringLiteral("lng"))) {
                                measurement.m_longitude = obj.value(QStringLiteral("lng")).toDouble();
                            }

                            data.append(measurement);

                            if (!measurement.m_mmsi.isEmpty())
                            {
                                QMutexLocker locker(&m_mutex);
                                m_aisCache.insert(measurement.m_mmsi, measurement);
                            }
                        }
                        else
                        {
                            qDebug() << "APRSFi::handleReply: Array element is not an object: " << valRef;
                        }
                    }
                    if (data.size() > 0) {
                        emit dataUpdated(data);
                    } else {
                        qDebug() << "APRSFi::handleReply: No data in array: " << document;
                    }
                }
            }
            else
            {
                qDebug() << "APRSFi::handleReply: Document is not an object: " << document;
            }
        }
        else
        {
            qWarning() << "APRSFi::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qWarning() << "APRSFi::handleReply: reply is null";
    }
}
