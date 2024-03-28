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

#ifndef INCLUDE_STIX_H
#define INCLUDE_STIX_H

#include <QtCore>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;

// Solar Orbiter STIX (Spectrometer/Telescope for Imaging X-rays) instrument
// Gets solar flare data - Newest data is often about 24 hours old
class SDRBASE_API STIX : public QObject
{
    Q_OBJECT
protected:
    STIX();

public:
    struct SDRBASE_API FlareData {
        QString m_id;
        QDateTime m_startDateTime;
        QDateTime m_endDateTime;
        QDateTime m_peakDateTime;
        int m_duration; // In seconds
        double m_flux;
        FlareData() :
            m_duration(0),
            m_flux(NAN)
        {
        }

        QString getLightCurvesURL() const;
        QString getDataURL() const;
    };

    static STIX* create();

    ~STIX();
    void getDataPeriodically(int periodInMins=60);

public slots:
    void getData();

private slots:
    void handleReply(QNetworkReply* reply);

signals:
    void dataUpdated(const QList<STIX::FlareData>& data);  // Called when new data available.

private:
    bool containsNonNull(const QJsonObject& obj, const QString &key) const;

    QTimer m_dataTimer;             // Timer for periodic updates
    QNetworkAccessManager *m_networkManager;

    QDateTime m_mostRecent;
    QList<STIX::FlareData> m_data;

};

#endif /* INCLUDE_STIX_H */

