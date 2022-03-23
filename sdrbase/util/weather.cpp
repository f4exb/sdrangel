///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "weather.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

Weather::Weather()
{
    connect(&m_timer, &QTimer::timeout, this, &Weather::update);
}

Weather* Weather::create(const QString& apiKey, const QString& service)
{
    if (service == "openweathermap.org")
    {
        if (!apiKey.isEmpty())
        {
            return new OpenWeatherMap(apiKey);
        }
        else
        {
            qDebug() << "Weather::create: An API key is required for: " << service;
            return nullptr;
        }
    }
    else
    {
        qDebug() << "Weather::create: Unsupported service: " << service;
        return nullptr;
    }
}

void Weather::getWeatherPeriodically(float latitude, float longitude, int periodInMins)
{
    m_latitude = latitude;
    m_longitude = longitude;
    m_timer.setInterval(periodInMins*60*1000);
    m_timer.start();
    update();
}

void Weather::update()
{
    getWeather(m_latitude, m_longitude);
}

OpenWeatherMap::OpenWeatherMap(const QString& apiKey) :
    m_apiKey(apiKey)
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &OpenWeatherMap::handleReply
    );
}

OpenWeatherMap::~OpenWeatherMap()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &OpenWeatherMap::handleReply
    );
    delete m_networkManager;
}

void OpenWeatherMap::getWeather(float latitude, float longitude)
{
    QUrl url(QString("http://api.openweathermap.org/data/2.5/weather"));
    QUrlQuery query;
    query.addQueryItem("lat", QString::number(latitude));
    query.addQueryItem("lon", QString::number(longitude));
    query.addQueryItem("mode", "json");
    query.addQueryItem("units", "metric");
    query.addQueryItem("appid", m_apiKey);
    url.setQuery(query);

    m_networkManager->get(QNetworkRequest(url));
}

void OpenWeatherMap::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            if (document.isObject())
            {
                QJsonObject obj = document.object();
                if (obj.contains(QStringLiteral("main")))
                {
                    QJsonValue val = obj.value(QStringLiteral("main"));
                    QJsonObject mainObj = val.toObject();
                    float temp = NAN, pressure = NAN, humidity = NAN;
                    if (mainObj.contains(QStringLiteral("temp"))) {
                       temp = mainObj.value(QStringLiteral("temp")).toDouble();
                    }
                    if (mainObj.contains(QStringLiteral("pressure"))) {
                        pressure = mainObj.value(QStringLiteral("pressure")).toDouble();
                    }
                    if (mainObj.contains(QStringLiteral("humidity"))) {
                        humidity = mainObj.value(QStringLiteral("humidity")).toDouble();
                    }
                    emit weatherUpdated(temp, pressure, humidity);
                }
                else
                {
                    qDebug() << "OpenWeatherMap::handleReply: Object doesn't contain a main: " << obj;
                }
            }
            else
            {
                qDebug() << "OpenWeatherMap::handleReply: Document is not an object: " << document;
            }
        }
        else
        {
            qDebug() << "OpenWeatherMap::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "OpenWeatherMap::handleReply: reply is null";
    }
}
