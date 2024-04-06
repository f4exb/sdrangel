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

#ifndef INCLUDE_GOESXRAY_H
#define INCLUDE_GOESXRAY_H

#include <QtCore>
#include <QTimer>
#include <QJsonObject>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;

// GOES X-Ray data
// This gets 1-minute averages of solar X-rays the 1-8 Angstrom (0.1-0.8 nm) and 0.5-4.0 Angstrom (0.05-0.4 nm) passbands from the GOES satellites
// https://www.swpc.noaa.gov/products/goes-x-ray-flux
// There are primary and secondary data sources, from different satellites, as sometimes they can be in eclipse
// Also gets Proton flux (Which may be observed on Earth a couple of days after a large flare/CME)
class SDRBASE_API GOESXRay : public QObject
{
    Q_OBJECT
protected:
    GOESXRay();

public:
    struct XRayData {
        QDateTime m_dateTime;
        QString m_satellite;
        double m_flux;
        enum Band {
            UNKNOWN,
            SHORT,  // 0.05-0.4nm
            LONG    // 0.1-0.8nm
        } m_band;
        XRayData() :
            m_flux(NAN),
            m_band(UNKNOWN)
        {
        }
    };

    struct ProtonData {
        QDateTime m_dateTime;
        QString m_satellite;
        double m_flux;
        int m_energy; // 10=10MeV, 50MeV, 100MeV, 500MeV
        ProtonData() :
            m_flux(NAN),
            m_energy(0)
        {
        }
    };

    static GOESXRay* create(const QString& service="services.swpc.noaa.gov");

    ~GOESXRay();
    void getDataPeriodically(int periodInMins=10);

public slots:
    void getData();

private slots:
    void handleReply(QNetworkReply* reply);

signals:
    void xRayDataUpdated(const QList<GOESXRay::XRayData>& data, bool primary);  // Called when new data available.
    void protonDataUpdated(const QList<GOESXRay::ProtonData> &data, bool primary);

private:
    bool containsNonNull(const QJsonObject& obj, const QString &key) const;
    void handleXRayJson(const QByteArray& bytes, bool primary);
    void handleProtonJson(const QByteArray& bytes, bool primary);

    QTimer m_dataTimer;             // Timer for periodic updates
    QNetworkAccessManager *m_networkManager;

};

#endif /* INCLUDE_GOESXRAY_H */

