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

#include "aviationweather.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

AviationWeather::AviationWeather()
{
    connect(&m_timer, &QTimer::timeout, this, &AviationWeather::update);
}

AviationWeather* AviationWeather::create(const QString& apiKey, const QString& service)
{
    if (service == "checkwxapi.com")
    {
        if (!apiKey.isEmpty())
        {
            return new CheckWXAPI(apiKey);
        }
        else
        {
            qDebug() << "AviationWeather::create: An API key is required for: " << service;
            return nullptr;
        }
    }
    else
    {
        qDebug() << "AviationWeather::create: Unsupported service: " << service;
        return nullptr;
    }
}

void AviationWeather::getWeatherPeriodically(const QString &icao, int periodInMins)
{
    m_icao = icao;
    m_timer.setInterval(periodInMins*60*1000);
    m_timer.start();
    update();
}

void AviationWeather::update()
{
    getWeather(m_icao);
}

CheckWXAPI::CheckWXAPI(const QString& apiKey) :
    m_apiKey(apiKey)
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &CheckWXAPI::handleReply
    );
}

CheckWXAPI::~CheckWXAPI()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &CheckWXAPI::handleReply
    );
    delete m_networkManager;
}

void CheckWXAPI::getWeather(const QString &icao)
{
    QUrl url(QString("https://api.checkwx.com/metar/%1/decoded").arg(icao));
    QNetworkRequest req(url);
    req.setRawHeader(QByteArray("X-API-Key"), m_apiKey.toUtf8());

    m_networkManager->get(req);
}

void CheckWXAPI::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            if (document.isObject())
            {
                QJsonObject obj = document.object();
                if (obj.contains(QStringLiteral("data")))
                {
                    QJsonValue val = obj.value(QStringLiteral("data"));
                    if (val.isArray())
                    {
                        QJsonArray array = val.toArray();
                        for (auto mainObjRef : array)
                        {
                            QJsonObject mainObj = mainObjRef.toObject();
                            METAR metar;

                            if (mainObj.contains(QStringLiteral("icao"))) {
                               metar.m_icao = mainObj.value(QStringLiteral("icao")).toString();
                            }

                            if (mainObj.contains(QStringLiteral("raw_text"))) {
                               metar.m_text = mainObj.value(QStringLiteral("raw_text")).toString();
                            }

                            if (mainObj.contains(QStringLiteral("observed"))) {
                               metar.m_dateTime = QDateTime::fromString(mainObj.value(QStringLiteral("observed")).toString(), Qt::ISODate);
                            }

                            if (mainObj.contains(QStringLiteral("wind"))) {
                                QJsonObject windObj = mainObj.value(QStringLiteral("wind")).toObject();
                                if (windObj.contains(QStringLiteral("degrees"))) {
                                    metar.m_windDirection = windObj.value(QStringLiteral("degrees")).toDouble();
                                }
                                if (windObj.contains(QStringLiteral("speed_kts"))) {
                                    metar.m_windSpeed = windObj.value(QStringLiteral("speed_kts")).toDouble();
                                }
                                if (windObj.contains(QStringLiteral("wind.gust_kts"))) {
                                    metar.m_windGusts = windObj.value(QStringLiteral("wind.gust_kts")).toDouble();
                                }
                            }
                            if (mainObj.contains(QStringLiteral("visibility"))) {
                                QJsonObject visibiltyObj = mainObj.value(QStringLiteral("visibility")).toObject();
                                if (visibiltyObj.contains(QStringLiteral("meters"))) {
                                    metar.m_visibility = visibiltyObj.value(QStringLiteral("meters")).toString();
                                }
                            }

                            if (mainObj.contains(QStringLiteral("conditions"))) {
                                QJsonArray conditions = mainObj.value(QStringLiteral("conditions")).toArray();
                                for (auto condition : conditions) {
                                    QJsonObject conditionObj = condition.toObject();
                                    if (conditionObj.contains(QStringLiteral("text"))) {
                                        metar.m_conditions.append(conditionObj.value(QStringLiteral("text")).toString());
                                    }
                                }
                            }

                            if (mainObj.contains(QStringLiteral("ceiling"))) {
                                QJsonObject ceilingObj = mainObj.value(QStringLiteral("ceiling")).toObject();
                                if (ceilingObj.contains(QStringLiteral("feet"))) {
                                    metar.m_ceiling = ceilingObj.value(QStringLiteral("feet")).toDouble();
                                }
                            }

                            if (mainObj.contains(QStringLiteral("clouds"))) {
                                QJsonArray clouds = mainObj.value(QStringLiteral("clouds")).toArray();
                                for (auto cloud : clouds) {
                                    QJsonObject cloudObj = cloud.toObject();
                                    // "Clear skies" doesn't have an altitude
                                    if (cloudObj.contains(QStringLiteral("text")) && cloudObj.contains(QStringLiteral("feet"))) {
                                        metar.m_clouds.append(QString("%1 %2 ft").arg(cloudObj.value(QStringLiteral("text")).toString())
                                                                                 .arg(cloudObj.value(QStringLiteral("feet")).toDouble()));
                                    } else if (cloudObj.contains(QStringLiteral("text"))) {
                                        metar.m_clouds.append(cloudObj.value(QStringLiteral("text")).toString());
                                    }
                                }
                            }

                            if (mainObj.contains(QStringLiteral("temperature"))) {
                                QJsonObject tempObj = mainObj.value(QStringLiteral("temperature")).toObject();
                                if (tempObj.contains(QStringLiteral("celsius"))) {
                                    metar.m_temperature = tempObj.value(QStringLiteral("celsius")).toDouble();
                                }
                            }

                            if (mainObj.contains(QStringLiteral("dewpoint"))) {
                                QJsonObject dewpointObj = mainObj.value(QStringLiteral("dewpoint")).toObject();
                                if (dewpointObj.contains(QStringLiteral("celsius"))) {
                                    metar.m_dewpoint = dewpointObj.value(QStringLiteral("celsius")).toDouble();
                                }
                            }

                            if (mainObj.contains(QStringLiteral("barometer"))) {
                                QJsonObject pressureObj = mainObj.value(QStringLiteral("barometer")).toObject();
                                if (pressureObj.contains(QStringLiteral("hpa"))) {
                                    metar.m_pressure = pressureObj.value(QStringLiteral("hpa")).toDouble();
                                }
                            }

                            if (mainObj.contains(QStringLiteral("humidity"))) {
                                QJsonObject humidityObj = mainObj.value(QStringLiteral("humidity")).toObject();
                                if (humidityObj.contains(QStringLiteral("percent"))) {
                                    metar.m_humidity = humidityObj.value(QStringLiteral("percent")).toDouble();
                                }
                            }

                            if (mainObj.contains(QStringLiteral("flight_category"))) {
                               metar.m_flightCateogory = mainObj.value(QStringLiteral("flight_category")).toString();
                            }

                            if (!metar.m_icao.isEmpty()) {
                                emit weatherUpdated(metar);
                            } else {
                                qDebug() << "CheckWXAPI::handleReply: object doesn't contain icao: " << mainObj;
                            }
                        }
                    }
                    else
                    {
                        qDebug() << "CheckWXAPI::handleReply: data isn't an array: " << obj;
                    }
                }
                else
                {
                    qDebug() << "CheckWXAPI::handleReply: Object doesn't contain data: " << obj;
                }
            }
            else
            {
                qDebug() << "CheckWXAPI::handleReply: Document is not an object: " << document;
            }
        }
        else
        {
            qDebug() << "CheckWXAPI::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "CheckWXAPI::handleReply: reply is null";
    }
}
