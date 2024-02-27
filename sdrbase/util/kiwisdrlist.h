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

#ifndef INCLUDE_KIWISDRLIST_H
#define INCLUDE_KIWISDRLIST_H

#include <QtCore>
#include <QTimer>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkDiskCache;

// Gets a list of public Kiwi SDRs from http://kiwisdr.com/public/
class SDRBASE_API KiwiSDRList : public QObject
{
    Q_OBJECT

public:

    struct KiwiSDR {
        QString m_url;
        QString m_status;   // Only seems to be 'active'
        QString m_offline;  // Only seems to be 'no'
        QString m_name;
        QString m_sdrHW;
        qint64 m_lowFrequency;
        qint64 m_highFrequency;
        int m_users;
        int m_usersMax;
        float m_latitude;
        float m_longitude;
        int m_altitude; // Above sea level (Not sure if ft or m)
        QString m_location;
        QString m_antenna;
        bool m_antennaConnected;
        QString m_snr;
    };

    KiwiSDRList();
    ~KiwiSDRList();

    void getData();
    void getDataPeriodically(int periodInMins=4);

public slots:
    void handleReply(QNetworkReply* reply);
    void update();

signals:
    void dataUpdated(const QList<KiwiSDR>& sdrs);  // Emitted when data are available.

private:
    QNetworkAccessManager *m_networkManager;
    QNetworkDiskCache *m_cache;
    QTimer m_timer;             // Timer for periodic updates

    void handleHTML(const QString& url, const QByteArray& bytes);

};

#endif /* INCLUDE_KIWISDRLIST_H */
