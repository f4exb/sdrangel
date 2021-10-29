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

#ifndef INCLUDE_FLIGHTINFORMATION_H
#define INCLUDE_FLIGHTINFORMATION_H

#include <QtCore>
#include <QDateTime>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;

// Flight information API wrapper
// Allows searching for departure/arrival airports and status of a flight
// Currently supports aviationstack.com
class SDRBASE_API FlightInformation : public QObject
{
    Q_OBJECT

protected:
    FlightInformation();

public:
    struct Flight {
        QString m_flightICAO;
        QString m_flightIATA;
        QString m_flightStatus; // active, landed...
        QString m_departureAirport;
        QString m_departureICAO;
        QString m_departureTerminal;
        QString m_departureGate;
        QDateTime m_departureScheduled;
        QDateTime m_departureEstimated;
        QDateTime m_departureActual;
        QString m_arrivalAirport;
        QString m_arrivalICAO;
        QString m_arrivalTerminal;
        QString m_arrivalGate;
        QDateTime m_arrivalScheduled;
        QDateTime m_arrivalEstimated;
        QDateTime m_arrivalActual;
    };

    static FlightInformation* create(const QString& apiKey, const QString& service="aviationstack.com");

    virtual void getFlightInformation(const QString& flight) = 0;

signals:
    void flightUpdated(const Flight& flight);  // Called when new data available.

private:

};

class SDRBASE_API AviationStack : public FlightInformation {
    Q_OBJECT
public:

    AviationStack(const QString& apiKey);
    ~AviationStack();
    virtual void getFlightInformation(const QString& flight) override;

private:
    void parseJson(QByteArray bytes);

    QString m_apiKey;
    QNetworkAccessManager *m_networkManager;

public slots:
    void handleReply(QNetworkReply* reply);

};

#endif /* INCLUDE_FLIGHTINFORMATION_H */
