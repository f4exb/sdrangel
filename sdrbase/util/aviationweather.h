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

#ifndef INCLUDE_AVIATIONWEATHER_H
#define INCLUDE_AVIATIONWEATHER_H

#include <QtCore>
#include <QTimer>

#include "export.h"

#include <cmath>

#include <QDateTime>

class QNetworkAccessManager;
class QNetworkReply;

// Aviation Weather API wrapper
// Allows METAR information to be obtained for a given airport
// Currently supports checkwxapi.com
class SDRBASE_API AviationWeather : public QObject
{
    Q_OBJECT

public:
    struct METAR {
        QString m_icao;                 // ICAO of reporting station/airport
        QString m_text;                 // Raw METAR text
        QDateTime m_dateTime;           // Date&time of observation
        float m_windDirection;          // Direction wind is blowing from, in degrees
        float m_windSpeed;              // Wind speed in knots
        float m_windGusts;              // Wind gusts in knots
        QString m_visibility;           // Visibility in metres (may be non-numeric)
        QStringList m_conditions;       // Weather conditions (Rain, snow)
        float m_ceiling;                // Ceiling in feet
        QStringList m_clouds;           // Cloud types and altitudes
        float m_temperature;            // Air temperature in Celsuis
        float m_dewpoint;               // Dewpoint in Celsuius
        float m_pressure;               // Air pressure in hPa/mb
        float m_humidity;               // Humidity in %
        QString m_flightCateogory;      // VFR/MVFR/IFR/LIFR

        METAR() :
           m_windDirection(NAN),
           m_windSpeed(NAN),
           m_windGusts(NAN),
           m_ceiling(NAN),
           m_temperature(NAN),
           m_dewpoint(NAN),
           m_pressure(NAN),
           m_humidity(NAN)
        {
        }

        QString decoded(const QString joinArg="\n") const
        {
            QStringList s;
            if (m_dateTime.isValid()) {
                s.append(QString("Observed: %1").arg(m_dateTime.toString()));
            }
            if (!std::isnan(m_windDirection) && !std::isnan(m_windSpeed)) {
                s.append(QString("Wind: %1%2 / %3 knts").arg(m_windDirection).arg(QChar(0xb0)).arg(m_windSpeed));
            }
            if (!std::isnan(m_windGusts) ) {
                s.append(QString("Gusts: %1 knts").arg(m_windGusts));
            }
            if (!m_visibility.isEmpty()) {
                s.append(QString("Visibility: %1 metres").arg(m_visibility));
            }
            if (!m_conditions.isEmpty()) {
                s.append(QString("Conditions: %1").arg(m_conditions.join(", ")));
            }
            if (!std::isnan(m_ceiling)) {
                s.append(QString("Ceiling: %1 ft").arg(m_ceiling));
            }
            if (!m_clouds.isEmpty()) {
                s.append(QString("Clouds: %1").arg(m_clouds.join(", ")));
            }
            if (!std::isnan(m_temperature)) {
                s.append(QString("Temperature: %1 %2C").arg(m_temperature).arg(QChar(0xb0)));
            }
            if (!std::isnan(m_dewpoint)) {
                s.append(QString("Dewpoint: %1 %2C").arg(m_dewpoint).arg(QChar(0xb0)));
            }
            if (!std::isnan(m_pressure)) {
                s.append(QString("Pressure: %1 hPa").arg(m_pressure));
            }
            if (!std::isnan(m_humidity)) {
                s.append(QString("Humidity: %1 %").arg(m_humidity));
            }
            if (!m_flightCateogory.isEmpty()) {
                s.append(QString("Category: %1").arg(m_flightCateogory));
            }
            return s.join(joinArg);
        }
    };

protected:
    AviationWeather();

public:
    static AviationWeather* create(const QString& apiKey, const QString& service="checkwxapi.com");

    virtual void getWeather(const QString &icao) = 0;
    void getWeatherPeriodically(const QString &, int periodInMins);

public slots:
    void update();

signals:
    void weatherUpdated(const METAR &metar);  // Called when new data available. If no value is available, parameter will be NAN

private:
    QTimer m_timer;             // Timer for periodic updates
    QString m_icao;             // Saved airport ICAO for period updates

};

class SDRBASE_API CheckWXAPI : public AviationWeather {
    Q_OBJECT
public:

    CheckWXAPI(const QString& apiKey);
    ~CheckWXAPI();
    virtual void getWeather(const QString &icao) override;

private:

    QString m_apiKey;
    QNetworkAccessManager *m_networkManager;

public slots:
    void handleReply(QNetworkReply* reply);

};

#endif /* INCLUDE_AVIATIONWEATHER_H */
