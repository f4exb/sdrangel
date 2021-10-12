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

#ifndef INCLUDE_WEATHER_H
#define INCLUDE_WEATHER_H

#include <QtCore>
#include <QTimer>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;

// Weather API wrapper
// Allows temperature, pressure and humidity to be obtained for a given latitude and longitude
// Currently supports openweathermap.org
class SDRBASE_API Weather : public QObject
{
    Q_OBJECT
protected:
    Weather();

public:
    static Weather* create(const QString& apiKey, const QString& service="openweathermap.org");

    virtual void getWeather(float latitude, float longitude) = 0;
    void getWeatherPeriodically(float latitude, float longitude, int periodInMins);

public slots:
    void update();

signals:
    void weatherUpdated(float temperature, float pressure, float humidity);  // Called when new data available. If no value is available, parameter will be NAN

private:
    QTimer m_timer;             // Timer for periodic updates
    float m_latitude;           // Saved latitude for periodic updates
    float m_longitude;          // Saved longitude for periodic updates

};

class SDRBASE_API OpenWeatherMap : public Weather {
    Q_OBJECT
public:

    OpenWeatherMap(const QString& apiKey);
    ~OpenWeatherMap();
    virtual void getWeather(float latitude, float longitude) override;

private:

    QString m_apiKey;
    QNetworkAccessManager *m_networkManager;


public slots:
    void handleReply(QNetworkReply* reply);

};

#endif /* INCLUDE_WEATHER_H */
