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

#ifndef INCLUDE_GRB_H
#define INCLUDE_GRB_H

#include <QtCore>
#include <QTimer>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkDiskCache;

// GRB (Gamma Ray Burst) database
// Gets GRB database from GRBweb https://user-web.icecube.wisc.edu/~grbweb_public/
// Uses summary .txt file so only contains last 1000 GRBs
class SDRBASE_API GRB : public QObject
{
    Q_OBJECT
protected:
    GRB();

public:

    struct SDRBASE_API Data {

        QString m_name;         // E.g: GRB240310A
        QString m_fermiName;    // Name used by Fermi telescope. E.g. GRB240310236. Can be None if not detected by Fermi
        QDateTime m_dateTime;
        float m_ra;             // Right Ascension
        float m_dec;            // Declination
        float m_fluence;        // erg/cm^2

        QString getFermiURL() const;        // Get URL where Fermi data is stored
        QString getFermiPlotURL() const;
        QString getFermiSkyMapURL() const;
        QString getSwiftURL() const;

    };

    static GRB* create();

    ~GRB();
    void getDataPeriodically(int periodInMins=1440); // GRBweb is updated every 24 hours, usually just after 9am UTC

public slots:
    void getData();

private slots:
    void handleReply(QNetworkReply* reply);

signals:
    void dataUpdated(const QList<Data>data);  // Called when new data is available.

private:

    QTimer m_dataTimer;             // Timer for periodic updates
    QNetworkAccessManager *m_networkManager;
    QNetworkDiskCache *m_cache;

    void handleText(QByteArray& bytes);

};

#endif /* INCLUDE_GRB_H */
