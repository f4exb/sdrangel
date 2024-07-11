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

#ifndef INCLUDE_SPYSERVERLIST_H
#define INCLUDE_SPYSERVERLIST_H

#include <QtCore>
#include <QTimer>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkDiskCache;

// Gets a list of public SpyServers from https://airspy.com/directory/status.json
class SDRBASE_API SpyServerList : public QObject
{
    Q_OBJECT

public:

    struct SpyServer {
        QString m_generalDescription;
        QString m_deviceType;
        QString m_streamingHost; // IP address
        int m_streamingPort;    // IP port
        int m_currentClientCount;
        int m_maxClients;
        QString m_antennaType;
        float m_latitude;
        float m_longitude;
        qint64 m_minimumFrequency;
        qint64 m_maximumFrequency;
        bool m_fullControlAllowed;
        bool m_online;
    };

    SpyServerList();
    ~SpyServerList();

    void getData();
    void getDataPeriodically(int periodInMins=2);

public slots:
    void handleReply(QNetworkReply* reply);
    void update();

signals:
    void dataUpdated(const QList<SpyServer>& sdrs);  // Emitted when data are available.

private:
    QNetworkAccessManager *m_networkManager;
    QNetworkDiskCache *m_cache;
    QTimer m_timer;             // Timer for periodic updates

    void handleJSON(const QString& url, const QByteArray& bytes);

};

#endif /* INCLUDE_SPYSERVERLIST_H */
