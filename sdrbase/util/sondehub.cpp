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
        uploaderLat, uploaderLon, uploaderAlt
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
        obj.insert("batt", frame->m_batteryVoltage);
    }

    if (frame->m_measValid)
    {
        // Don't upload uncalibrated measurements, as there can be a significant error
        if (frame->isTemperatureCalibrated()) {
            obj.insert("temp", frame->getTemperatureFloat(subframe));
        }
        if (frame->isHumidityCalibrated())
        {
            float humidity = frame->getHumidityFloat(subframe);
            if (humidity != 0.0f) {
                obj.insert("humidity", humidity);
            }
        }
        if (frame->isPressureCalibrated())
        {
            float pressure = frame->getPressureFloat(subframe);
            if (pressure != 0.0f) {
                obj.insert("pressure", pressure);
            }
        }
    }

    if (frame->m_gpsInfoValid)
    {
        obj.insert("datetime", frame->m_gpsDateTime.toUTC().addSecs(18).toString("yyyy-MM-ddTHH:mm:ss.zzz000Z")); // +18 adjusts UTC to GPS time
    }

    if (frame->m_posValid)
    {
        obj.insert("lat", frame->m_latitude);
        obj.insert("lon", frame->m_longitude);
        obj.insert("alt", frame->m_height);
        obj.insert("vel_h", frame->m_speed);
        obj.insert("vel_v", frame->m_verticalRate);
        obj.insert("heading", frame->m_heading);
        obj.insert("sats", frame->m_satellitesUsed);
    }

    if (!subframe->getFrequencyMHz().isEmpty()) {
        obj.insert("frequency", std::round(subframe->getFrequencyMHz().toFloat() * 100.0) / 100.0);
    }

    if (subframe->getType() != "RS41") {
        obj.insert("subtype", subframe->getType());
    }

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

void SondeHub::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QByteArray bytes = reply->readAll();
            //qDebug() << bytes;
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
