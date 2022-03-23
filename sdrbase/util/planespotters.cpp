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

#include "planespotters.h"

#include <QDebug>
#include <QFile>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

PlaneSpotters::PlaneSpotters()
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PlaneSpotters::handleReply
    );
}

PlaneSpotters::~PlaneSpotters()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &PlaneSpotters::handleReply
    );
    delete m_networkManager;
}

void PlaneSpotters::getAircraftPhoto(const QString& icao)
{
    if (m_photos.contains(icao))
    {
        emit aircraftPhoto(m_photos[icao]);
    }
    else
    {
        // Create a new photo hash table entry
        PlaneSpottersPhoto *photo = new PlaneSpottersPhoto();
        photo->m_icao = icao;
        m_photos.insert(icao, photo);

        // Fetch from network
        QUrl url(QString("https://api.planespotters.net/pub/photos/hex/%1").arg(icao));
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", "SDRangel/1.0"); // Get 403 error without this
        request.setOriginatingObject(photo);
        m_networkManager->get(request);
    }
}

void PlaneSpotters::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            if (reply->url().path().startsWith("/pub/photos/hex")) {
                parseJson((PlaneSpottersPhoto *)reply->request().originatingObject(), reply->readAll());
            } else {
                parsePhoto((PlaneSpottersPhoto *)reply->request().originatingObject(), reply->readAll());
            }
        }
        else
        {
            qDebug() << "PlaneSpotters::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "PlaneSpotters::handleReply: reply is null";
    }
}

void PlaneSpotters::parsePhoto(PlaneSpottersPhoto *photo, QByteArray bytes)
{
    photo->m_pixmap.loadFromData(bytes);
    emit aircraftPhoto(photo);
}

void PlaneSpotters::parseJson(PlaneSpottersPhoto *photo, QByteArray bytes)
{
    QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (document.isObject())
    {
        QJsonObject obj = document.object();
        if (obj.contains(QStringLiteral("photos")))
        {
            QJsonArray photos = obj.value(QStringLiteral("photos")).toArray();
            if (photos.size() > 0)
            {
                QJsonObject photoObj = photos[0].toObject();

                if (photoObj.contains(QStringLiteral("id"))) {
                    photo->m_link = photoObj.value(QStringLiteral("id")).toString();
                }
                if (photoObj.contains(QStringLiteral("thumbnail")))
                {
                    QJsonObject thumbnailObj = photoObj.value(QStringLiteral("thumbnail")).toObject();
                    photo->m_thumbnail.m_src = thumbnailObj.value(QStringLiteral("src")).toString();
                    QJsonObject sizeObj = thumbnailObj.value(QStringLiteral("size")).toObject();
                    photo->m_thumbnail.m_width = sizeObj.value(QStringLiteral("width")).toInt();
                    photo->m_thumbnail.m_height = sizeObj.value(QStringLiteral("width")).toInt();
                }
                if (photoObj.contains(QStringLiteral("link"))) {
                    photo->m_link = photoObj.value(QStringLiteral("link")).toString();
                }
                if (photoObj.contains(QStringLiteral("photographer"))) {
                    photo->m_photographer = photoObj.value(QStringLiteral("photographer")).toString();
                }

                if (!photo->m_thumbnail.m_src.isEmpty())
                {
                    QUrl url(photo->m_thumbnail.m_src);
                    QNetworkRequest request(url);
                    request.setOriginatingObject(photo);
                    m_networkManager->get(request);
                }
            }
            else
            {
                qDebug() << "PlaneSpotters::handleReply: data array is empty";
            }
        }
        else
        {
            qDebug() << "PlaneSpotters::handleReply: Object doesn't contain data: " << obj;
        }
    }
    else
    {
        qDebug() << "PlaneSpotters::handleReply: Document is not an object: " << document;
    }
}
