///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE                                        //
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

#include "stix.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>

STIX::STIX()
{
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &STIX::handleReply);
    connect(&m_dataTimer, &QTimer::timeout, this, &STIX::getData);
}


STIX::~STIX()
{
    disconnect(&m_dataTimer, &QTimer::timeout, this, &STIX::getData);
    disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &STIX::handleReply);
    delete m_networkManager;
}

STIX* STIX::create()
{
    return new STIX();
}

void STIX::getDataPeriodically(int periodInMins)
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

void STIX::getData()
{
    QUrlQuery data(QString("https://datacenter.stix.i4ds.net/api/request/flare-list"));
    QDateTime start;

    if (m_mostRecent.isValid()) {
        start = m_mostRecent;
    } else {
        start = QDateTime::currentDateTime().addDays(-5);
    }

    data.addQueryItem("start_utc", start.toString(Qt::ISODate));
    data.addQueryItem("end_utc", QDateTime::currentDateTime().toString(Qt::ISODate));
    data.addQueryItem("sort", "time");

    QUrl url("https://datacenter.stix.i4ds.net/api/request/flare-list");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    m_networkManager->post(request, data.toString(QUrl::FullyEncoded).toUtf8());
}

bool STIX::containsNonNull(const QJsonObject& obj, const QString &key) const
{
    if (obj.contains(key))
    {
        QJsonValue val = obj.value(key);
        return !val.isNull();
    }
    return false;
}

void STIX::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());

            if (document.isArray())
            {
                QJsonArray array = document.array();
                QList<FlareData> data;
                for (auto valRef : array)
                {
                    if (valRef.isObject())
                    {
                        QJsonObject obj = valRef.toObject();

                        FlareData measurement;

                        if (obj.contains(QStringLiteral("flare_id"))) {
                            measurement.m_id = obj.value(QStringLiteral("flare_id")).toString();
                        }
                        if (obj.contains(QStringLiteral("start_UTC")))
                        {
                            measurement.m_startDateTime = QDateTime::fromString(obj.value(QStringLiteral("start_UTC")).toString(), Qt::ISODate);
                            if (!m_mostRecent.isValid() || (measurement.m_startDateTime > m_mostRecent)) {
                                m_mostRecent = measurement.m_startDateTime;
                            }
                        }
                        if (obj.contains(QStringLiteral("end_UTC"))) {
                            measurement.m_endDateTime = QDateTime::fromString(obj.value(QStringLiteral("end_UTC")).toString(), Qt::ISODate);
                        }
                        if (obj.contains(QStringLiteral("peak_UTC"))) {
                            measurement.m_peakDateTime = QDateTime::fromString(obj.value(QStringLiteral("peak_UTC")).toString(), Qt::ISODate);
                        }
                        if (obj.contains(QStringLiteral("duration"))) {
                            measurement.m_duration = obj.value(QStringLiteral("duration")).toInt();
                        }
                        if (obj.contains(QStringLiteral("GOES_flux"))) {
                            measurement.m_flux = obj.value(QStringLiteral("GOES_flux")).toDouble();
                        }

                        data.append(measurement);
                    }
                    else
                    {
                        qDebug() << "STIX::handleReply: Array element is not an object: " << valRef;
                    }
                }
                if (data.size() > 0)
                {
                    m_data.append(data);
                    emit dataUpdated(m_data);
                }
                else
                {
                    qDebug() << "STIX::handleReply: No data in array: " << document;
                }
            }
            else
            {
                qDebug() << "STIX::handleReply: Document is not an array: " << document;
            }
        }
        else
        {
            qDebug() << "STIX::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "STIX::handleReply: reply is null";
    }
}

 QString STIX::FlareData::getLightCurvesURL() const
 {
     return QString("https://datacenter.stix.i4ds.net/view/plot/lightcurves?start=%1&span=%2").arg(m_startDateTime.toSecsSinceEpoch()).arg(m_duration);
 }

 QString STIX::FlareData::getDataURL() const
 {
     return QString("https://datacenter.stix.i4ds.net/view/list/fits/%1/%2").arg(m_startDateTime.toSecsSinceEpoch()).arg(m_duration);
 }
