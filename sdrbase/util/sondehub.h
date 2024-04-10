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

#ifndef INCLUDE_SONDEHUB_H
#define INCLUDE_SONDEHUB_H

#include <QtCore>
#include <QDateTime>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;
class RS41Frame;
class RS41Subframe;

class SDRBASE_API SondeHub : public QObject
{
    Q_OBJECT
protected:
    SondeHub();

public:

    static SondeHub* create();

    ~SondeHub();

    void upload(
        const QString uploaderCallsign,
        QDateTime timeReceived,
        RS41Frame *frame,
        const RS41Subframe *subframe,
        float uploaderLat,
        float uploaderLon,
        float uploaderAlt
    );

    void updatePosition(
        const QString& callsign,
        float latitude,
        float longitude,
        float altitude,
        const QString& radio,
        const QString& antenna,
        const QString& email,
        bool mobile
    );


private slots:
    void handleReply(QNetworkReply* reply);

private:

    QNetworkAccessManager *m_networkManager;

};

#endif /* INCLUDE_SONDEHUB_H */
