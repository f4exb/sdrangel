///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_APRSFI_H
#define INCLUDE_APRSFI_H

#include <QtCore>
#include <QMutex>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;

// aprs.fi API
// Allows querying APRS and AIS data
// Data can be cached to help avoid rate limiting on the server
class SDRBASE_API APRSFi : public QObject
{
    Q_OBJECT
protected:
    APRSFi(const QString& apiKey, int cacheValidMins);

public:

    struct LocationData {
        QString m_name;
        QDateTime m_firstTime;          // First time this position was reported
        QDateTime m_lastTime;           // Last time this position was reported
        float m_latitude;
        float m_longitude;
        QString m_callsign;

        QDateTime m_dateTime;           // Data/time this data was received from APRS.fi

        LocationData() :
            m_latitude(NAN),
            m_longitude(NAN)
        {
        }
    };

    struct AISData : LocationData {
        QString m_mmsi;
        QString m_imo;

        AISData()
        {
        }
    };

    // Keys are free from https://aprs.fi/ - so get your own
    static APRSFi* create(const QString& apiKey="184212.WhYgz2jqu3l2O", int cacheValidMins=10);

    ~APRSFi();
    void getData(const QStringList& names);
    void getData(const QString& name);

private slots:
    void handleReply(QNetworkReply* reply);

signals:
    void dataUpdated(const QList<AISData>& data);  // Called when new data available.

private:
    bool containsNonNull(const QJsonObject& obj, const QString &key) const;

    QNetworkAccessManager *m_networkManager;
    QString m_apiKey;
    int m_cacheValidMins;
    static QMutex m_mutex;
    static QHash<QString, AISData> m_aisCache;
};

#endif /* INCLUDE_APRSFI_H */
