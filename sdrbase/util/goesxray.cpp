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

#include "goesxray.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>

GOESXRay::GOESXRay()
{
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &GOESXRay::handleReply);
    connect(&m_dataTimer, &QTimer::timeout, this, &GOESXRay::getData);
}


GOESXRay::~GOESXRay()
{
    disconnect(&m_dataTimer, &QTimer::timeout, this, &GOESXRay::getData);
    disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &GOESXRay::handleReply);
    delete m_networkManager;
}

GOESXRay* GOESXRay::create(const QString& service)
{
    if (service == "services.swpc.noaa.gov")
    {
        return new GOESXRay();
    }
    else
    {
        qDebug() << "GOESXRay::create: Unsupported service: " << service;
        return nullptr;
    }
}

void GOESXRay::getDataPeriodically(int periodInMins)
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

void GOESXRay::getData()
{
    // Around 160kB per file
    QUrl url(QString("https://services.swpc.noaa.gov/json/goes/primary/xrays-6-hour.json"));
    m_networkManager->get(QNetworkRequest(url));

    QUrl secondaryURL(QString("https://services.swpc.noaa.gov/json/goes/secondary/xrays-6-hour.json"));
    m_networkManager->get(QNetworkRequest(secondaryURL));

    QUrl protonPrimaryURL(QString("https://services.swpc.noaa.gov/json/goes/primary/integral-protons-plot-6-hour.json"));
    m_networkManager->get(QNetworkRequest(protonPrimaryURL));
}

bool GOESXRay::containsNonNull(const QJsonObject& obj, const QString &key) const
{
    if (obj.contains(key))
    {
        QJsonValue val = obj.value(key);
        return !val.isNull();
    }
    return false;
}

void GOESXRay::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QByteArray bytes = reply->readAll();
            bool primary = reply->url().toString().contains("primary");

            if (reply->url().fileName() == "xrays-6-hour.json") {
                handleXRayJson(bytes, primary);
            } else if (reply->url().fileName() == "integral-protons-plot-6-hour.json") {
                handleProtonJson(bytes, primary);
            } else {
                qDebug() << "GOESXRay::handleReply: unexpected filename: " << reply->url().fileName();
            }
        }
        else
        {
            qDebug() << "GOESXRay::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "GOESXRay::handleReply: reply is null";
    }
}

void GOESXRay::handleXRayJson(const QByteArray& bytes, bool primary)
{
    QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (document.isArray())
    {
        QJsonArray array = document.array();
        QList<XRayData> data;
        for (auto valRef : array)
        {
            if (valRef.isObject())
            {
                QJsonObject obj = valRef.toObject();

                XRayData measurement;

                if (obj.contains(QStringLiteral("satellite"))) {
                    measurement.m_satellite = QString("GOES %1").arg(obj.value(QStringLiteral("satellite")).toInt());
                }
                if (containsNonNull(obj, QStringLiteral("time_tag"))) {
                    measurement.m_dateTime = QDateTime::fromString(obj.value(QStringLiteral("time_tag")).toString(), Qt::ISODate);
                }
                if (containsNonNull(obj, QStringLiteral("flux"))) {
                    measurement.m_flux = obj.value(QStringLiteral("flux")).toDouble();
                }
                if (containsNonNull(obj, QStringLiteral("energy")))
                {
                    QString energy = obj.value(QStringLiteral("energy")).toString();
                    if (energy == "0.05-0.4nm") {
                        measurement.m_band = XRayData::SHORT;
                    } else if (energy == "0.1-0.8nm") {
                        measurement.m_band = XRayData::LONG;
                    } else {
                        qDebug() << "GOESXRay::handleXRayJson: Unknown energy: " << energy;
                    }
                }

                data.append(measurement);
            }
            else
            {
                qDebug() << "GOESXRay::handleXRayJson: Array element is not an object: " << valRef;
            }
        }
        if (data.size() > 0) {
            emit xRayDataUpdated(data, primary);
        } else {
            qDebug() << "GOESXRay::handleXRayJson: No data in array: " << document;
        }
    }
    else
    {
        qDebug() << "GOESXRay::handleXRayJson: Document is not an array: " << document;
    }
}

void GOESXRay::handleProtonJson(const QByteArray& bytes, bool primary)
{
    QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (document.isArray())
    {
        QJsonArray array = document.array();
        QList<ProtonData> data;
        for (auto valRef : array)
        {
            if (valRef.isObject())
            {
                QJsonObject obj = valRef.toObject();

                ProtonData measurement;

                if (obj.contains(QStringLiteral("satellite"))) {
                    measurement.m_satellite = QString("GOES %1").arg(obj.value(QStringLiteral("satellite")).toInt());
                }
                if (containsNonNull(obj, QStringLiteral("time_tag"))) {
                    measurement.m_dateTime = QDateTime::fromString(obj.value(QStringLiteral("time_tag")).toString(), Qt::ISODate);
                }
                if (containsNonNull(obj, QStringLiteral("flux"))) {
                    measurement.m_flux = obj.value(QStringLiteral("flux")).toDouble();
                }
                if (containsNonNull(obj, QStringLiteral("energy")))
                {
                    QString energy = obj.value(QStringLiteral("energy")).toString();
                    QString value = energy.mid(2).split(' ')[0];
                    measurement.m_energy = value.toInt(); // String like: ">=50 MeV"
                }

                data.append(measurement);
            }
            else
            {
                qDebug() << "GOESXRay::handleProtonJson: Array element is not an object: " << valRef;
            }
        }
        if (data.size() > 0) {
            emit protonDataUpdated(data, primary);
        } else {
            qDebug() << "GOESXRay::handleProtonJson: No data in array: " << document;
        }
    }
    else
    {
        qDebug() << "GOESXRay::handleProtonJson: Document is not an array: " << document;
    }
}
