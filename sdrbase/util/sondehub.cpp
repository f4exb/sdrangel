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

#include "sondehub.h"
#include "util/radiosonde.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

SondeHub::SondeHub()
{
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &SondeHub::handleReply);
}

SondeHub::~SondeHub()
{
    disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &SondeHub::handleReply);
    delete m_networkManager;
}

SondeHub* SondeHub::create()
{
    return new SondeHub();
}

void SondeHub::upload(
    const QString uploaderCallsign,
    QDateTime timeReceived,
    RS41Frame *frame,
    const RS41Subframe *subframe,
    float uploaderLat,
    float uploaderLon,
    float uploaderAlt
    )
{
    // Check we have required data
    if (!frame->m_statusValid || !frame->m_posValid) {
        return;
    }

    QJsonArray uploaderPos {
        QString::number(uploaderLat, 'f', 5).toDouble(),
        QString::number(uploaderLon, 'f', 5).toDouble(),
        QString::number(uploaderAlt, 'f', 1).toDouble()
    };

    QJsonObject obj {
        {"software_name", "SDRangel"},
        {"software_version", qApp->applicationVersion()},
        {"uploader_callsign", uploaderCallsign},
        {"time_received", timeReceived.toUTC().toString("yyyy-MM-ddTHH:mm:ss.zzz000Z")},
        {"manufacturer", "Vaisala"},
        {"type", "RS41"},
        {"uploader_position", uploaderPos}
    };

    if (frame->m_statusValid)
    {
        obj.insert("frame", frame->m_frameNumber);
        obj.insert("serial", frame->m_serial);
        obj.insert("batt", QString::number(frame->m_batteryVoltage, 'f', 2).toDouble());
    }

    if (frame->m_measValid)
    {
        // Don't upload uncalibrated measurements, as there can be a significant error
        if (frame->isTemperatureCalibrated()) {
            obj.insert("temp", QString::number(frame->getTemperatureFloat(subframe), 'f', 1).toDouble());
        }
        if (frame->isHumidityCalibrated())
        {
            float humidity = frame->getHumidityFloat(subframe);
            if (humidity != 0.0f) {
                obj.insert("humidity", QString::number(humidity, 'f', 1).toDouble());
            }
        }
        if (frame->isPressureCalibrated())
        {
            float pressure = frame->getPressureFloat(subframe);
            if (pressure != 0.0f) {
                obj.insert("pressure", QString::number(pressure, 'f', 2).toDouble());
            }
        }
    }

    if (frame->m_gpsInfoValid)
    {
        obj.insert("datetime", frame->m_gpsDateTime.toUTC().addSecs(18).toString("yyyy-MM-ddTHH:mm:ss.zzz000Z")); // +18 adjusts UTC to GPS time
    }

    if (frame->m_posValid)
    {
        obj.insert("lat", QString::number(frame->m_latitude, 'f', 5).toDouble());
        obj.insert("lon", QString::number(frame->m_longitude, 'f', 5).toDouble());
        obj.insert("alt", QString::number(frame->m_height, 'f', 1).toDouble());
        obj.insert("vel_h", QString::number(frame->m_speed, 'f', 2).toDouble());
        obj.insert("vel_v", QString::number(frame->m_verticalRate, 'f', 2).toDouble());
        obj.insert("heading", QString::number(frame->m_heading, 'f', 1).toDouble());
        obj.insert("sats", frame->m_satellitesUsed);
    }

    if (!subframe->getFrequencyMHz().isEmpty()) {
        obj.insert("frequency", std::round(subframe->getFrequencyMHz().toFloat() * 100.0) / 100.0);
    }

    if (subframe->getType() != "RS41") {
        obj.insert("subtype", subframe->getType());
    }

    //obj.insert("dev", true);
    //qDebug() << obj;
    QJsonArray payloads {
        obj
    };

    QJsonDocument doc(payloads);
    QByteArray data = doc.toJson();

    QUrl url(QString("https://api.v2.sondehub.org/sondes/telemetry"));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "sdrangel");
    request.setRawHeader("Date", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toLatin1());

    m_networkManager->put(request, data);
}

void SondeHub::updatePosition(
    const QString& callsign,
    float latitude,
    float longitude,
    float altitude,
    const QString& radio,
    const QString& antenna,
    const QString& email,
    bool mobile
    )
{
    QJsonArray position {
        latitude, longitude, altitude
    };

    QJsonObject obj {
        {"software_name", "SDRangel"},
        {"software_version", qApp->applicationVersion()},
        {"uploader_callsign", callsign},
        {"uploader_position", position},
        {"uploader_radio", radio},
        {"uploader_antenna", antenna},
        {"uploader_contact_email", email},
        {"mobile", mobile}
    };

    QJsonDocument doc(obj);
    QByteArray data = doc.toJson();

    QUrl url(QString("https://api.v2.sondehub.org/listeners"));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "sdrangel");

    m_networkManager->put(request, data);
}

void SondeHub::getPrediction(const QString& serial)
{
    QUrl url(QString("https://api.v2.sondehub.org/predictions?vehicles=%1").arg(serial));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "sdrangel");

    m_networkManager->get(request);
}

void SondeHub::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QByteArray bytes = reply->readAll();
            QJsonDocument document = QJsonDocument::fromJson(bytes);
            if (document.isObject())
            {
                QJsonObject obj = document.object();
                if (obj.contains(QStringLiteral("message")))
                {
                    QString message = obj.value(QStringLiteral("message")).toString();
                    qWarning() << "SondeHub message:" << message;
                }
                if (obj.contains(QStringLiteral("errors")))
                {
                    QJsonArray errors = obj.value(QStringLiteral("errors")).toArray();
                    for (auto errorObjRef : errors)
                    {
                        QJsonObject errorObj = errorObjRef.toObject();
                        if (errorObj.contains(QStringLiteral("error_message")))
                        {
                            QString errorMessage = errorObj.value(QStringLiteral("error_message")).toString();
                            qWarning() << "SondeHub error:" << errorMessage;
                            if (errorObj.contains(QStringLiteral("payload")))
                            {
                                QJsonObject payload = errorObj.value(QStringLiteral("payload")).toObject();
                                qWarning() << "SondeHub error:" << QJsonDocument(payload);
                            }
                        }
                        else
                        {
                            qWarning() << "SondeHub error:" << QJsonDocument(errorObj);
                        }
                    }
                }
                 //qDebug() << "SondeHub::handleReply: obj" << QJsonDocument(obj);
            }
            else if (document.isArray())
            {
                QJsonArray array = document.array();

                for (auto arrayRef : array)
                {
                    if (arrayRef.isObject())
                    {
                        QJsonObject obj = arrayRef.toObject();

                        if (obj.contains(QStringLiteral("vehicle")) && obj.contains(QStringLiteral("data")))
                        {
                            QJsonArray data;
                            // Perhaps a bug that data is a string rather than an array?
                            if (obj.value(QStringLiteral("data")).isString())
                            {
                                QJsonDocument dataDocument = QJsonDocument::fromJson(obj.value(QStringLiteral("data")).toString().toUtf8());
                                data = dataDocument.array();
                            }
                            else
                            {
                                data = obj.value(QStringLiteral("data")).toArray();
                            }

                            QList<Position> positions;
                            for (auto dataObjRef : data)
                            {
                                QJsonObject positionObj = dataObjRef.toObject();
                                Position position;

                                position.m_dateTime = QDateTime::fromSecsSinceEpoch(positionObj.value(QStringLiteral("time")).toInt());
                                position.m_latitude = positionObj.value(QStringLiteral("lat")).toDouble();
                                position.m_longitude = positionObj.value(QStringLiteral("lon")).toDouble();
                                position.m_altitude = positionObj.value(QStringLiteral("alt")).toDouble();
                                positions.append(position);
                            }

                            emit prediction(obj.value("vehicle").toString(), positions);
                        }
                    }
                    else
                    {
                        qDebug() << "SondeHub::handleReply:" << bytes;
                    }
                }
            }
            else
            {
                qDebug() << "SondeHub::handleReply:" << bytes;
            }
        }
        else
        {
            qDebug() << "SondeHub::handleReply: error: " << reply->error() << reply->readAll();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "SondeHub::handleReply: reply is null";
    }
}
