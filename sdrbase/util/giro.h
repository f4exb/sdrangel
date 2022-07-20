///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_GIRO_H
#define INCLUDE_GIRO_H

#include <QtCore>
#include <QTimer>
#include <QJsonDocument>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;

// GIRO - Global Ionosphere Radio Observatory
// Gets MUFD, TEC, foF2 and other data for various stations around the world
// Also gets MUF and foF2 contours as GeoJSON
// Data from https://prop.kc2g.com/stations/
class SDRBASE_API GIRO : public QObject
{
    Q_OBJECT
protected:
    GIRO();

public:
    // See the following paper for an explanation of some of these variables
    // https://sbgf.org.br/mysbgf/eventos/expanded_abstracts/13th_CISBGf/A%20Simple%20Method%20to%20Calculate%20the%20Maximum%20Usable%20Frequency.pdf
    struct GIROStationData {
        QString m_station;
        float m_latitude;
        float m_longitude;
        QDateTime m_dateTime;
        float m_mufd;           // Maximum usable frequency
        float m_md;             // Propagation coefficient? D=3000km?
        float m_tec;            // Total electron content
        float m_foF2;           // Critical frequency of F2 layer in ionosphere (highest frequency to be reflected)
        float m_hmF2;           // F2 layer height of peak electron density (km?)
        float m_foE;            // Critical frequency of E layer
        int m_confidence;
        GIROStationData() :
            m_latitude(NAN),
            m_longitude(NAN),
            m_mufd(NAN),
            m_md(NAN),
            m_tec(NAN),
            m_foF2(NAN),
            m_hmF2(NAN),
            m_foE(NAN),
            m_confidence(-1)
        {
        }
    };

    static GIRO* create(const QString& service="prop.kc2g.com");

    ~GIRO();
    void getDataPeriodically(int periodInMins);
    void getMUFPeriodically(int periodInMins);
    void getfoF2Periodically(int periodInMins);

private slots:
    void getData();
    void getMUF();
    void getfoF2();

private slots:
    void handleReply(QNetworkReply* reply);

signals:
    void dataUpdated(const GIROStationData& data);  // Called when new data available.
    void mufUpdated(const QJsonDocument& doc);
    void foF2Updated(const QJsonDocument& doc);

private:
    bool containsNonNull(const QJsonObject& obj, const QString &key) const;

    QTimer m_dataTimer;             // Timer for periodic updates
    QTimer m_mufTimer;
    QTimer m_foF2Timer;
    QNetworkAccessManager *m_networkManager;

};

#endif /* INCLUDE_GIRO_H */
