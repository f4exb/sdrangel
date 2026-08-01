///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_VESSELFINDER_H
#define INCLUDE_VESSELFINDER_H

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QString>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class SDRBASE_API VesselFinderPhoto : public QObject
{
    Q_OBJECT

public:
    QString m_id;
    QString m_imo;
    QString m_mmsi;
    QString m_imageUrl;
    QString m_link;
    QString m_photographer;
    QPixmap m_pixmap;
    bool m_queryUsesIMO = false;
    bool m_complete = false;
};

// VesselFinder gallery wrapper. Gallery results and images are cached in memory so
// selecting the same vessel repeatedly does not generate additional web requests.
class SDRBASE_API VesselFinder : public QObject
{
    Q_OBJECT

public:
    explicit VesselFinder(QObject *parent = nullptr);
    ~VesselFinder();

    void getShipPhoto(const QString& imo, const QString& mmsi);

signals:
    void shipPhoto(const VesselFinderPhoto *photo);

private slots:
    void handleReply(QNetworkReply *reply);

private:
    enum RequestType
    {
        GalleryRequest = 1,
        ImageRequest
    };

    static const char *m_userAgent;
    static void setCommonHeaders(QNetworkRequest& request);
    static QString identifier(const QString& imo, const QString& mmsi);
    static QString htmlToPlainText(QString html);
    void requestGallery(VesselFinderPhoto *photo, bool useIMO);
    bool parseGallery(VesselFinderPhoto *photo, const QByteArray& bytes);
    void parseImage(VesselFinderPhoto *photo, const QByteArray& bytes);
    bool tryMMSIFallback(VesselFinderPhoto *photo);
    void finish(VesselFinderPhoto *photo);

    QNetworkAccessManager *m_networkManager;
    QHash<QString, VesselFinderPhoto *> m_photos;
};

#endif // INCLUDE_VESSELFINDER_H
