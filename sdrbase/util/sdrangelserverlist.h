///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_SDRANGELSERVERLIST_H
#define INCLUDE_SDRANGELSERVERLIST_H

#include <QtCore>
#include <QTimer>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkDiskCache;

// Gets a list of public SDRangel Servers from https://sdrangel.org/websdr/
class SDRBASE_API SDRangelServerList : public QObject
{
    Q_OBJECT

public:

    struct SDRangelServer {
        QString m_address;
        quint16 m_port;
        QString m_protocol;
        qint64 m_minFrequency;
        qint64 m_maxFrequency;
        int m_maxSampleRate;
        QString m_device;
        QString m_antenna;
        bool m_remoteControl;
        QString m_stationName;
        QString m_location;
        float m_latitude;
        float m_longitude;
        float m_altitude;
        bool m_isotropic;
        float m_azimuth;
        float m_elevation;
        int m_clients;
        int m_maxClients;
        int m_timeLimit;        // In minutes
    };

    SDRangelServerList();
    ~SDRangelServerList();

    void getData();
    void getDataPeriodically(int periodInMins=1);

public slots:
    void handleReply(QNetworkReply* reply);
    void update();

signals:
    void dataUpdated(const QList<SDRangelServer>& sdrs);  // Emitted when data are available.

private:
    QNetworkAccessManager *m_networkManager;
    QNetworkDiskCache *m_cache;
    QTimer m_timer;             // Timer for periodic updates

    void handleJSON(const QString& url, const QByteArray& bytes);

};

#endif /* INCLUDE_SDRANGELSERVERLIST_H */
