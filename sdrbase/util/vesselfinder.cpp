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

#include "vesselfinder.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

const char *VesselFinder::m_userAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) SDRangel/1.0";

VesselFinder::VesselFinder(QObject *parent) :
    QObject(parent),
    m_networkManager(new QNetworkAccessManager(this))
{
    connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &VesselFinder::handleReply
    );
}

VesselFinder::~VesselFinder()
{
    disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &VesselFinder::handleReply
    );
    qDeleteAll(m_photos);
}

void VesselFinder::setCommonHeaders(QNetworkRequest& request)
{
    request.setRawHeader("User-Agent", m_userAgent);
    request.setRawHeader("Accept-Language", "en-US,en;q=0.8");
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
    );
}

QString VesselFinder::identifier(const QString& imo, const QString& mmsi)
{
    bool imoOk = false;
    bool mmsiOk = false;
    const qulonglong imoNumber = imo.trimmed().toULongLong(&imoOk);
    const qulonglong mmsiNumber = mmsi.trimmed().toULongLong(&mmsiOk);

    if (imoOk && (imoNumber != 0)) {
        return QString("imo:%1").arg(imoNumber);
    }
    if (mmsiOk && (mmsiNumber != 0)) {
        return QString("mmsi:%1").arg(mmsiNumber);
    }

    return QString();
}

void VesselFinder::getShipPhoto(const QString& imo, const QString& mmsi)
{
    const QString id = identifier(imo, mmsi);

    if (id.isEmpty()) {
        return;
    }

    if (m_photos.contains(id))
    {
        VesselFinderPhoto *photo = m_photos.value(id);

        if (photo->m_complete) {
            emit shipPhoto(photo);
        }

        return;
    }

    VesselFinderPhoto *photo = new VesselFinderPhoto();
    photo->m_id = id;
    photo->m_imo = imo.trimmed();
    photo->m_mmsi = mmsi.trimmed();
    m_photos.insert(id, photo);

    requestGallery(photo, id.startsWith("imo:"));
}

void VesselFinder::requestGallery(VesselFinderPhoto *photo, bool useIMO)
{
    QUrl url("https://www.vesselfinder.com/gallery");
    QUrlQuery query;
    query.addQueryItem(useIMO ? "imo" : "mmsi", useIMO ? photo->m_imo : photo->m_mmsi);
    url.setQuery(query);

    photo->m_queryUsesIMO = useIMO;
    photo->m_link = url.toString(QUrl::FullyEncoded);

    QNetworkRequest request(url);
    setCommonHeaders(request);
    request.setRawHeader("Accept", "text/html,application/xhtml+xml");
    request.setAttribute(QNetworkRequest::User, GalleryRequest);
    request.setOriginatingObject(photo);
    m_networkManager->get(request);
}

void VesselFinder::handleReply(QNetworkReply *reply)
{
    if (!reply) {
        return;
    }

    VesselFinderPhoto *photo = qobject_cast<VesselFinderPhoto *>(
        reply->request().originatingObject()
    );
    const RequestType requestType = static_cast<RequestType>(
        reply->request().attribute(QNetworkRequest::User).toInt()
    );

    if (!photo)
    {
        qDebug() << "VesselFinder::handleReply: missing photo request context";
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "VesselFinder::handleReply:" << reply->url() << reply->errorString();

        if ((requestType != GalleryRequest) || !tryMMSIFallback(photo)) {
            finish(photo);
        }

        reply->deleteLater();
        return;
    }

    const QByteArray bytes = reply->readAll();

    if (requestType == GalleryRequest)
    {
        if (!parseGallery(photo, bytes) && !tryMMSIFallback(photo)) {
            finish(photo);
        }
    }
    else
    {
        parseImage(photo, bytes);
    }

    reply->deleteLater();
}

bool VesselFinder::parseGallery(VesselFinderPhoto *photo, const QByteArray& bytes)
{
    const QString html = QString::fromUtf8(bytes);
    const QRegularExpression photoLinkExpression(
        "<a\\b[^>]*href\\s*=\\s*[\\\"']/ship-photos/(\\d+)[\\\"'][^>]*>",
        QRegularExpression::CaseInsensitiveOption
    );
    const QRegularExpressionMatch photoLinkMatch = photoLinkExpression.match(html);

    if (!photoLinkMatch.hasMatch()) {
        return false;
    }

    const int linkStart = photoLinkMatch.capturedStart();
    const int linkEnd = html.indexOf("</a>", photoLinkMatch.capturedEnd(), Qt::CaseInsensitive);

    if (linkEnd < 0) {
        return false;
    }

    const QString photoAnchor = html.mid(linkStart, linkEnd - linkStart);
    const QRegularExpression imageExpression(
        "<img\\b[^>]*src\\s*=\\s*[\\\"']([^\\\"']+)[\\\"']",
        QRegularExpression::CaseInsensitiveOption
    );
    const QRegularExpressionMatch imageMatch = imageExpression.match(photoAnchor);

    if (!imageMatch.hasMatch()) {
        return false;
    }

    photo->m_imageUrl = imageMatch.captured(1);
    photo->m_imageUrl.replace("&amp;", "&");

    const int nextPicture = html.indexOf("class=\"picture", linkEnd, Qt::CaseInsensitive);
    const QString cardTail = html.mid(
        linkEnd,
        nextPicture < 0 ? -1 : nextPicture - linkEnd
    );
    const QRegularExpression photographerExpression(
        "<a\\b[^>]*class\\s*=\\s*[\\\"'][^\\\"']*photographer[^\\\"']*[\\\"'][^>]*>(.*?)</a>",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    const QRegularExpressionMatch photographerMatch = photographerExpression.match(cardTail);

    if (photographerMatch.hasMatch()) {
        photo->m_photographer = htmlToPlainText(photographerMatch.captured(1));
    }

    QUrl imageUrl(photo->m_imageUrl);

    if (imageUrl.isRelative()) {
        imageUrl = QUrl(photo->m_link).resolved(imageUrl);
    }

    QNetworkRequest request(imageUrl);
    setCommonHeaders(request);
    request.setRawHeader("Accept", "image/avif,image/webp,image/png,image/jpeg,*/*;q=0.8");
    request.setRawHeader("Referer", photo->m_link.toUtf8());
    request.setAttribute(QNetworkRequest::User, ImageRequest);
    request.setOriginatingObject(photo);
    m_networkManager->get(request);

    return true;
}

QString VesselFinder::htmlToPlainText(QString html)
{
    html.remove(QRegularExpression("<[^>]*>"));
    html.replace("&nbsp;", " ", Qt::CaseInsensitive);
    html.replace("&#160;", " ", Qt::CaseInsensitive);
    html.replace("&amp;", "&", Qt::CaseInsensitive);
    html.replace("&quot;", "\"", Qt::CaseInsensitive);
    html.replace("&#39;", "'", Qt::CaseInsensitive);
    html.replace("&#x27;", "'", Qt::CaseInsensitive);
    html.replace("&lt;", "<", Qt::CaseInsensitive);
    html.replace("&gt;", ">", Qt::CaseInsensitive);
    return html.simplified();
}

void VesselFinder::parseImage(VesselFinderPhoto *photo, const QByteArray& bytes)
{
    if (!photo->m_pixmap.loadFromData(bytes)) {
        qDebug() << "VesselFinder::parseImage: unable to decode" << bytes.size() << "bytes";
    }

    finish(photo);
}

bool VesselFinder::tryMMSIFallback(VesselFinderPhoto *photo)
{
    bool mmsiOk = false;
    const qulonglong mmsi = photo->m_mmsi.toULongLong(&mmsiOk);

    if (photo->m_queryUsesIMO && mmsiOk && (mmsi != 0))
    {
        requestGallery(photo, false);
        return true;
    }

    return false;
}

void VesselFinder::finish(VesselFinderPhoto *photo)
{
    photo->m_complete = true;
    emit shipPhoto(photo);
}
