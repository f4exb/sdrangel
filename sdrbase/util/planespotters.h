///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_PLANESPOTTERS_H
#define INCLUDE_PLANESPOTTERS_H

#include <QtCore>
#include <QDateTime>
#include <QPixmap>
#include <QHash>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;

class SDRBASE_API PlaneSpottersPhoto : public QObject {
    Q_OBJECT

    struct Thumbnail {
        QString m_src;
        int m_width;
        int m_height;
    };

public:
    QString m_icao;
    QString m_id;
    Thumbnail m_thumbnail;
    Thumbnail m_largeThumbnail;
    QString m_link;
    QString m_photographer
;
    QPixmap m_pixmap;
};

// PlaneSpotters API wrapper
// Allows downloading of images of aircraft from https://www.planespotters.net/
// Note that API terms of use require us not to cache images on disk
class SDRBASE_API PlaneSpotters : public QObject
{
    Q_OBJECT

public:
    PlaneSpotters();
    ~PlaneSpotters();


    void getAircraftPhoto(const QString& icao);

signals:
    void aircraftPhoto(const PlaneSpottersPhoto *photo);  // Called when photo is available.

private:
    void parseJson(PlaneSpottersPhoto *photo, QByteArray bytes);
    void parsePhoto(PlaneSpottersPhoto *photo, QByteArray bytes);

    QNetworkAccessManager *m_networkManager;
    QHash<QString, PlaneSpottersPhoto *> m_photos;

public slots:
    void handleReply(QNetworkReply* reply);

};

#endif /* INCLUDE_PLANESPOTTERS_H */
