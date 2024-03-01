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

#ifndef INCLUDE_RAINVIEWER_H
#define INCLUDE_RAINVIEWER_H

#include <QtCore>
#include <QTimer>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;

// RainViewer API wrapper (https://www.rainviewer.com/)
// Gets details of currently available weather radar and satellite IR data
class SDRBASE_API RainViewer : public QObject
{
    Q_OBJECT

public:
    RainViewer();
   ~RainViewer();

    void getPath();
    void getPathPeriodically(int periodInMins=15);

public slots:
    void update();
    void handleReply(QNetworkReply* reply);

signals:
    void pathUpdated(const QString& radarPath, const QString& satellitePath);  // Emitted when paths to new data are available.

private:
    QTimer m_timer;             // Timer for periodic updates
    QNetworkAccessManager *m_networkManager;

};

#endif /* INCLUDE_RAINVIEWER_H */
