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

#include "flightinformation.h"

#include <QDebug>
#include <QFile>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

FlightInformation::FlightInformation()
{
}

FlightInformation* FlightInformation::create(const QString& apiKey, const QString& service)
{
    if (service == "aviationstack.com")
    {
        if (!apiKey.isEmpty())
        {
            return new AviationStack(apiKey);
        }
        else
        {
            qDebug() << "FlightInformation::create: An API key is required for: " << service;
            return nullptr;
        }
    }
    else
    {
        qDebug() << "FlightInformation::create: Unsupported service: " << service;
        return nullptr;
    }
}

AviationStack::AviationStack(const QString& apiKey) :
    m_apiKey(apiKey)
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AviationStack::handleReply
    );
}

AviationStack::~AviationStack()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AviationStack::handleReply
    );
    delete m_networkManager;
}

void AviationStack::getFlightInformation(const QString& flight)
{
    QUrl url(QString("http://api.aviationstack.com/v1/flights"));
    QUrlQuery query;
    query.addQueryItem("flight_icao",flight);
    query.addQueryItem("access_key", m_apiKey);
    url.setQuery(query);

    m_networkManager->get(QNetworkRequest(url));
}

void AviationStack::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            parseJson(reply->readAll());
        }
        else
        {
            qDebug() << "AviationStack::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "AviationStack::handleReply: reply is null";
    }
}

void AviationStack::parseJson(QByteArray bytes)
{
    QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (document.isObject())
    {
        QJsonObject obj = document.object();
        if (obj.contains(QStringLiteral("data")))
        {
            QJsonArray data = obj.value(QStringLiteral("data")).toArray();
            if (data.size() > 0)
            {
                QJsonObject flightObj = data[0].toObject();

                Flight flight;

                if (flightObj.contains(QStringLiteral("flight_status"))) {
                    flight.m_flightStatus = flightObj.value(QStringLiteral("flight_status")).toString();
                }
                if (flightObj.contains(QStringLiteral("departure")))
                {
                    QJsonObject departure = flightObj.value(QStringLiteral("departure")).toObject();
                    flight.m_departureAirport = departure.value(QStringLiteral("airport")).toString();
                    flight.m_departureICAO = departure.value(QStringLiteral("icao")).toString();
                    flight.m_departureTerminal = departure.value(QStringLiteral("terminal")).toString();
                    flight.m_departureGate = departure.value(QStringLiteral("gate")).toString();
                    flight.m_departureScheduled = QDateTime::fromString(departure.value(QStringLiteral("scheduled")).toString(), Qt::ISODate);
                    flight.m_departureEstimated = QDateTime::fromString(departure.value(QStringLiteral("estimated")).toString(), Qt::ISODate);
                    flight.m_departureActual = QDateTime::fromString(departure.value(QStringLiteral("actual")).toString(), Qt::ISODate);
                }
                if (flightObj.contains(QStringLiteral("arrival")))
                {
                    QJsonObject departure = flightObj.value(QStringLiteral("arrival")).toObject();
                    flight.m_arrivalAirport = departure.value(QStringLiteral("airport")).toString();
                    flight.m_arrivalICAO = departure.value(QStringLiteral("icao")).toString();
                    flight.m_arrivalTerminal = departure.value(QStringLiteral("terminal")).toString();
                    flight.m_arrivalGate = departure.value(QStringLiteral("gate")).toString();
                    flight.m_arrivalScheduled = QDateTime::fromString(departure.value(QStringLiteral("scheduled")).toString(), Qt::ISODate);
                    flight.m_arrivalEstimated = QDateTime::fromString(departure.value(QStringLiteral("estimated")).toString(), Qt::ISODate);
                    flight.m_arrivalActual = QDateTime::fromString(departure.value(QStringLiteral("actual")).toString(), Qt::ISODate);
                }
                if (flightObj.contains(QStringLiteral("flight")))
                {
                    QJsonObject flightNo = flightObj.value(QStringLiteral("flight")).toObject();
                    flight.m_flightICAO = flightNo.value(QStringLiteral("icao")).toString();
                    flight.m_flightIATA = flightNo.value(QStringLiteral("iata")).toString();
                }
                emit flightUpdated(flight);
            }
            else
            {
                qDebug() << "AviationStack::handleReply: data array is empty";
            }
        }
        else
        {
            qDebug() << "AviationStack::handleReply: Object doesn't contain data: " << obj;
        }
    }
    else
    {
        qDebug() << "AviationStack::handleReply: Document is not an object: " << document;
    }
}
