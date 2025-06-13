///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_AURORA_H
#define INCLUDE_AURORA_H

#include <QtCore>
#include <QTimer>
#include <QJsonDocument>
#include <QImage>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkDiskCache;

// Aurora prediction
// Data from https://services.swpc.noaa.gov/
class SDRBASE_API Aurora : public QObject
{
    Q_OBJECT
protected:
    Aurora();

public:

    static Aurora* create(const QString& service="noaa.gov");

    ~Aurora();
    void getDataPeriodically(int periodInMins=30);

public slots:
    void getData();

private slots:
    void handleReply(QNetworkReply* reply);

signals:
    void dataUpdated(const QImage& data);  // Called when new data available.

private:

    void handleJSON(QJsonDocument& document);

    QTimer m_dataTimer;             // Timer for periodic updates
    QNetworkAccessManager *m_networkManager;
    QNetworkDiskCache *m_cache;

    static const unsigned char m_colorMap[256*3];

};

#endif /* INCLUDE_AURORA_H */
