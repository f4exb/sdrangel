///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
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

#ifndef INCLUDE_MAPTILESERVER_H_
#define INCLUDE_MAPTILESERVER_H_

#include <QTcpServer>
#include <QTcpSocket>
#include <QRegularExpression>
#include <QDebug>
#include <QtNetwork>
#include <QImage>
#include <QImageReader>
#include <QBuffer>
#include <QPainter>
#include <QUrlQuery>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkDiskCache>

#include "util/profiler.h"

class MapTileServer : public QTcpServer
{
    Q_OBJECT
private:
    QString m_thunderforestAPIKey;
    QString m_maptilerAPIKey;
    QString m_cartoDBAPIKey;
    QNetworkAccessManager m_manager;
    QMutex m_mutex;

    struct TileJob {
        QTcpSocket* m_socket;
        QList<QString> m_urls;
        QHash<QString, QImage> m_images;
        // Where in the downloaded image this tile is, as a fraction of it, for the
        // layers that are fetched from an ancestor tile - see ancestorTile()
        QHash<QString, QRectF> m_sourceRects;
        QString m_format;
    };
    QList<TileJob *> m_tileJobs;
    QHash<QNetworkReply *, TileJob *> m_replies;

    QNetworkDiskCache *m_cache;

    QString m_radarPath;
    QString m_cloudTime;
    QString m_nasaGlobalImageryPath;
    QString m_nasaGlobalImageryFormat;
    bool m_displayRain;
    bool m_displayClouds;
    bool m_displaySeaMarks;
    bool m_displayRailways;
    bool m_displayNASAGlobalImagery;

public:
    // port - port to listen on / is listening on. Use 0 for any free port.
    MapTileServer(quint16 &port, QObject* parent = 0) :
        QTcpServer(parent),
        m_thunderforestAPIKey(""),
        m_maptilerAPIKey(""),
        m_cartoDBAPIKey(""),
        m_radarPath(""),
        m_cloudTime(""),
        m_nasaGlobalImageryPath(""),
        m_nasaGlobalImageryFormat(""),
        m_displayRain(false),
        m_displayClouds(false),
        m_displaySeaMarks(false),
        m_displayRailways(false),
        m_displayNASAGlobalImagery(false)
    {
        connect(&m_manager, &QNetworkAccessManager::finished, this, &MapTileServer::downloadFinished);
        listen(QHostAddress::Any, port);
        port = serverPort();

        QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
        QDir writeableDir(locations[0]);
        if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("maptiles"))) {
            qDebug() << "Failed to create cache/maptiles";
        }

        m_cache = new QNetworkDiskCache();
        m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("maptiles"));
        m_cache->setMaximumCacheSize(1000000000);
        m_manager.setCache(m_cache);
    }

    ~MapTileServer()
    {
       disconnect(&m_manager, &QNetworkAccessManager::finished, this, &MapTileServer::downloadFinished);
       delete m_cache;
    }

    void setThunderforestAPIKey(const QString& thunderforestAPIKey)
    {
        m_thunderforestAPIKey = thunderforestAPIKey;
    }

    void setMaptilerAPIKey(const QString& maptilerAPIKey)
    {
        m_maptilerAPIKey = maptilerAPIKey;
    }

    void setCartoDBAPIKey(const QString& cartoDBAPIKey)
    {
        m_cartoDBAPIKey = cartoDBAPIKey;
    }

    //!< Throw away every cached upstream tile. QNetworkDiskCache cannot be asked to
    //!< remove entries matching a URL, only one known URL at a time or all of them
    void clearCache()
    {
        if (m_cache) {
            m_cache->clear();
        }
    }

    void setRadarPath(const QString& radarPath)
    {
        m_radarPath = radarPath;
    }

    void setCloudTime(const QString& cloudTime)
    {
        m_cloudTime = cloudTime;
    }

    void setNASAGlobalImageryPath(const QString& nasaGlobalImageryPath)
    {
        m_nasaGlobalImageryPath = nasaGlobalImageryPath;
    }

    void setNASAGlobalImageryFormat(const QString& nasaGlobalImageryFormat)
    {
        m_nasaGlobalImageryFormat = nasaGlobalImageryFormat;
    }

    void setDisplaySeaMarks(bool displaySeaMarks)
    {
        m_displaySeaMarks = displaySeaMarks;
    }

    void setDisplayRailways(bool displayRailways)
    {
        m_displayRailways = displayRailways;
    }

    void setDisplayRain(bool displayRain)
    {
        m_displayRain = displayRain;
    }

    void setDisplayClouds(bool displayClouds)
    {
        m_displayClouds = displayClouds;
    }

    void setDisplayNASAGlobalImagery(bool displayNASAGlobalImagery)
    {
        m_displayNASAGlobalImagery = displayNASAGlobalImagery;
    }

    void incomingConnection(qintptr socket) override
    {
        QTcpSocket* s = new QTcpSocket(this);
        connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
        connect(s, SIGNAL(disconnected()), this, SLOT(discardClient()));
        s->setSocketDescriptor(socket);
        //addPendingConnection(socket);
    }

    bool isHttpRedirect(QNetworkReply *reply)
    {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 304 is file not changed, but maybe we did
        return (status >= 301 && status <= 308);
    }

    QNetworkReply *download(const QUrl &url)
    {
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setRawHeader("User-Agent", "SDRangel"); // Required by a.tile.openstreetmap.org

        // Don't cache rainviwer data as it's dynamic
        if (!url.toString().contains("tilecache.rainviewer")) {
            request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
        }

        QNetworkReply *reply = m_manager.get(request);
#ifndef QT_NO_OPENSSL
        connect(reply, &QNetworkReply::sslErrors, this, &MapTileServer::sslErrors);
#endif
        //qDebug() << "MapTileServer: Downloading from " << url;
        return reply;
    }

    // EUMETSAT's global geostationary infrared composite, which replaced RainViewer's
    // satellite layer when that stopped publishing. It is a WMS rather than a tile
    // server, so each tile is a GetMap for that tile's own bounding box. The service
    // advertises only CRS:84 and EPSG:4326, but GeoServer reprojects to web mercator on
    // request, which is what lets it line up with the other layers here
    // Beyond a source's own largest zoom there is nothing more to see: RainViewer serves
    // a "zoom not supported" placeholder past 7 (the same 1370 byte image at every level
    // beyond it), and the cloud composite is a few km per pixel published every three
    // hours, so past 6 it is only being enlarged anyway. Rather than ask for a tile that
    // does not exist, the tile this one sits inside is asked for and the right part of it
    // used, which also means every tile under that ancestor shares one request - so after
    // the first it is a cache hit rather than another second of waiting on a WMS
    // RainViewer's radar stops at 7; the cloud composite is coarse enough that 6 is
    // already past the point where it carries any more detail
    static constexpr int rainMaxZoom = 7;
    static constexpr int cloudMaxZoom = 6;

    struct Ancestor {
        int m_z, m_x, m_y;
        QRectF m_source;    // Fraction of the ancestor image this tile covers
    };

    static Ancestor ancestorTile(int z, int x, int y, int maxZoom)
    {
        if (z <= maxZoom) {
            return { z, x, y, QRectF(0.0, 0.0, 1.0, 1.0) };
        }
        const int depth = z - maxZoom;
        const int span = 1 << depth;                // Tiles across the ancestor
        const double fraction = 1.0 / (double) span;
        return { maxZoom, x >> depth, y >> depth,
                 QRectF((x & (span - 1)) * fraction, (y & (span - 1)) * fraction,
                        fraction, fraction) };
    }

    QString cloudTileURL(int z, int x, int y) const
    {
        // Web mercator bounds, in metres
        const double half = 20037508.342789244;
        const double size = 2.0 * half / (double)(1 << z);
        const double minX = -half + x * size;
        const double maxY = half - y * size;

        // Built with QUrlQuery rather than by formatting a string: a percent-encoded
        // literal in an arg() format is read as a placeholder, so "image%2Fpng" had its
        // %2 replaced by the second argument and the server was asked for a format
        // called "image<timestamp>Fpng". It answered with a ServiceException document,
        // which arrives as a successful download of something that is not an image
        QUrlQuery query;
        query.addQueryItem("service", "WMS");
        query.addQueryItem("version", "1.1.1");
        query.addQueryItem("request", "GetMap");
        query.addQueryItem("layers", "mumi:worldcloudmap_ir108");
        query.addQueryItem("styles", "");
        query.addQueryItem("srs", "EPSG:3857");
        query.addQueryItem("bbox", QString("%1,%2,%3,%4")
            .arg(minX, 0, 'f', 3).arg(maxY - size, 0, 'f', 3)
            .arg(minX + size, 0, 'f', 3).arg(maxY, 0, 'f', 3));
        query.addQueryItem("width", "256");
        query.addQueryItem("height", "256");
        query.addQueryItem("format", "image/png");
        query.addQueryItem("transparent", "true");
        query.addQueryItem("time", m_cloudTime);

        QUrl url("https://view.eumetsat.int/geoserver/wms");
        url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    // The infrared image is opaque grey - dark where it is looking at warm ground, bright
    // at cold cloud tops - so drawn as it comes it would hide the map. Brightness becomes
    // opacity, which leaves clear air transparent and cloud white, and gives a softer
    // edge than a straight colour-key would. The 3D map gets the same effect from
    // Cesium's colorToAlpha, which is a threshold rather than a ramp
    static constexpr int cloudAlphaFloor = 110;     // Below this it is clear air
    static constexpr int cloudAlphaCeiling = 210;   // Above this, solid cloud

    static QImage cloudToAlpha(const QImage &src)
    {
        QImage out = src.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < out.height(); y++)
        {
            QRgb *line = (QRgb *) out.scanLine(y);
            for (int x = 0; x < out.width(); x++)
            {
                const QRgb p = line[x];
                if (qAlpha(p) == 0) {
                    continue;
                }
                // Measured over the whole composite: warm ground sits around 70-100
                // and cold cloud tops run from about 160 up, so everything below the
                // floor is clear air and everything above the ceiling is solid cloud
                const int luma = qGray(p);
                const int alpha = qBound(0, (luma - cloudAlphaFloor) * 255
                                            / (cloudAlphaCeiling - cloudAlphaFloor), 255);
                line[x] = qRgba(255, 255, 255, alpha);
            }
        }
        return out;
    }

    QImage combine(const TileJob *job)
    {
        PROFILER_START()
        // Don't use job->m_images[job->m_urls[0]].size() as not always valid (E.g. map tiler can return http 204 - no content)
        // Do we need to support 512x512?
        QImage image(QSize(256, 256), QImage::Format_ARGB32_Premultiplied);
        image.fill(qPremultiply(QColor(0, 0, 0, 0).rgba()));
        QPainter painter(&image);

        for (int i = 0; i < job->m_images.size(); i++) {
            const QImage &img = job->m_images[job->m_urls[i]];
            //qDebug() << "Image format " << i << " is " << img.format() << img.size();
        }

        for (int i = 0; i < job->m_images.size(); i++) {
            const QImage &img = job->m_images[job->m_urls[i]];
            //img.save(QString("in%1.png").arg(i), "PNG");
            if (img.format() != QImage::Format_Invalid)
            {
                const QImage &drawn = job->m_urls[i].contains("view.eumetsat.int")
                    ? cloudToAlpha(img) : img;
                const QRectF fraction = job->m_sourceRects.value(job->m_urls[i],
                                                                 QRectF(0.0, 0.0, 1.0, 1.0));
                if (fraction == QRectF(0.0, 0.0, 1.0, 1.0))
                {
                    painter.drawImage(image.rect(), drawn);
                }
                else
                {
                    // Enlarging a small piece of the ancestor, so sample it smoothly
                    // rather than showing its pixels as blocks
                    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
                    painter.drawImage(QRectF(image.rect()), drawn,
                        QRectF(fraction.x() * drawn.width(), fraction.y() * drawn.height(),
                               qMax(1.0, fraction.width() * drawn.width()),
                               qMax(1.0, fraction.height() * drawn.height())));
                }
            }
        }

        PROFILER_STOP("Map tile combine")
        return image;
    }

    // A single-layer tile that is already what was asked for doesn't need to be decoded,
    // composited and re-encoded - the bytes we downloaded can go straight to the client.
    // Only the image header is read, to check it is the size combine() would have produced
    bool canPassThrough(const QByteArray& data, QNetworkReply *reply, const TileJob *job) const
    {
        if (!job || (job->m_urls.size() != 1) || reply->error() || data.isEmpty()) {
            return false;
        }

        const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        if (job->m_format == "PNG")
        {
            if (!contentType.startsWith("image/png")) {
                return false;
            }
        }
        else if (!contentType.startsWith("image/jpeg") && !contentType.startsWith("image/jpg"))
        {
            return false;
        }

        QByteArray copy(data);
        QBuffer buffer(&copy);
        QImageReader reader(&buffer);

        return reader.size() == QSize(256, 256);
    }

    void replyData(QTcpSocket* socket, const QByteArray& data)
    {
        socket->write("HTTP/1.0 200 Ok\r\n"
            "Content-Type: image/png\r\n"
            "\r\n");
        socket->write(data);
        socket->close();

        if (socket->state() == QTcpSocket::UnconnectedState) {
            delete socket;
        }
    }

    void replyImage(QTcpSocket* socket, const QImage& image, const QString& format)
    {
        PROFILER_START()
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, qPrintable(format));

        //qDebug() << "socket: " << socket << "thread:" << QThread::currentThread();
        socket->write("HTTP/1.0 200 Ok\r\n"
            "Content-Type: image/png\r\n"
            "\r\n");
        socket->write(buffer.buffer());
        socket->close();

        if (socket->state() == QTcpSocket::UnconnectedState) {
            delete socket;
        }
        PROFILER_STOP("Map tile encode")
    }

    void replyError(QTcpSocket* socket)
    {
        QTextStream os(socket);
        os.setAutoDetectUnicode(true);
        os << "HTTP/1.0 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<html>Not found</html>\r\n";
        socket->close();

        if (socket->state() == QTcpSocket::UnconnectedState) {
            delete socket;
        }
    }

private slots:

    void readClient()
    {
        QMutexLocker locker(&m_mutex);

        QTcpSocket* socket = (QTcpSocket*)sender();
        if (socket->canReadLine())
        {
            QString line = socket->readLine();
            //qDebug() << "HTTP Request: " << line;
            QStringList tokens = QString(line).split(QRegularExpression("[ \r\n][ \r\n]*"));
            if (tokens[0] == "GET")
            {
                QString xml = "";

                // Create multiple requests for each image
                // https://wiki.openstreetmap.org/wiki/Raster_tile_providers
                // rain radar: https://tilecache.rainviewer.com/v2/radar/{timestamp=1705359600}/{size=256}/z/x/y/{color=0}/{options=1_1}.png

                // "GET /street/1/2/3.png HTTP/1.1\r\n"
                const QRegularExpression re("\\/([A-Za-z0-9\\-_]+)\\/([0-9]+)\\/([0-9]+)\\/([0-9]+).(png|jpg)");
                QRegularExpressionMatch match = re.match(tokens[1]);
                if (match.hasMatch())
                {
                    QString map, x, y, z, format;

                    map = match.captured(1);
                    z = match.captured(2);
                    x = match.captured(3);
                    y = match.captured(4);
                    format = match.captured(5);

                    TileJob *job = new TileJob;
                    //qDebug() << "Created job" << job << "socket:" << socket << "thread:" << QThread::currentThread() ;
                    job->m_socket = socket;
                    if (format == "png") {
                        job->m_format = "PNG";
                    } else {
                        job->m_format = "JPG";
                    }

                    // This should match code in OSMTemplateServer::readClient
                    QString baseMapURL;
                    if (map == "street") {
                        baseMapURL = QString("https://tile.openstreetmap.org/%3/%1/%2.png").arg(x).arg(y).arg(z);
                    } else if (map == "satellite") {
                        baseMapURL = QString("https://api.maptiler.com/tiles/satellite-v2/%3/%1/%2.jpg?key=%4").arg(x).arg(y).arg(z).arg(m_maptilerAPIKey);
                    } else if ((map == "dark_nolabels") || (map == "light_nolabels")) {
                        baseMapURL = QString("http://1.basemaps.cartocdn.com/%4/%3/%1/%2.png?key=%5").arg(x).arg(y).arg(z).arg(map).arg(m_cartoDBAPIKey);
                    } else {
                        baseMapURL = QString("http://a.tile.thunderforest.com/%4/%3/%1/%2.png?apikey=%5").arg(x).arg(y).arg(z).arg(map).arg(m_thunderforestAPIKey);
                    }

                    job->m_urls.append(baseMapURL);
                    if (m_displaySeaMarks) {
                        job->m_urls.append(QString("https://tiles.openseamap.org/seamark/%3/%1/%2.png").arg(x).arg(y).arg(z));
                    }
                    if (m_displayRailways) {
                        job->m_urls.append(QString("https://a.tiles.openrailwaymap.org/standard/%3/%1/%2.png").arg(x).arg(y).arg(z));
                    }
                    if (m_displayNASAGlobalImagery && !m_nasaGlobalImageryPath.isEmpty()) {
                        job->m_urls.append(QString("https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/%4/%3/%2/%1.%5").arg(x).arg(y).arg(z).arg(m_nasaGlobalImageryPath).arg(m_nasaGlobalImageryFormat)); // x,y reversed compared to others
                    }
                    if (m_displayClouds && !m_cloudTime.isEmpty())
                    {
                        const Ancestor a = ancestorTile(z.toInt(), x.toInt(), y.toInt(), cloudMaxZoom);
                        const QString url = cloudTileURL(a.m_z, a.m_x, a.m_y);
                        job->m_urls.append(url);
                        job->m_sourceRects.insert(url, a.m_source);
                    }
                    if (m_displayRain && !m_radarPath.isEmpty())
                    {
                        const Ancestor a = ancestorTile(z.toInt(), x.toInt(), y.toInt(), rainMaxZoom);
                        const QString url = QString("https://tilecache.rainviewer.com%4/256/%3/%1/%2/4/1_1.png")
                            .arg(a.m_x).arg(a.m_y).arg(a.m_z).arg(m_radarPath);
                        job->m_urls.append(url);
                        job->m_sourceRects.insert(url, a.m_source);
                    }
                    m_tileJobs.append(job);
                    for (const auto& url : job->m_urls)
                    {
                        QNetworkReply *reply = download(QUrl(url));
                        m_replies.insert(reply, job);
                    }
                }
                else
                {
                    replyError(socket);
                }
            }
        }
    }

    void discardClient()
    {
        QTcpSocket* socket = (QTcpSocket*)sender();
        //qDebug() << "discardClient socket:" << socket;
        socket->deleteLater();
        for (auto job : m_tileJobs) {
            if (job->m_socket == socket) {
                //qDebug() << "Socket closed on active job. job: " << job << "socket" << socket;
                job->m_socket = nullptr;
            }
        }
    }

    void downloadFinished(QNetworkReply *reply)
    {
        QMutexLocker locker(&m_mutex);
        //QString url = reply->url().toEncoded().constData();
        QString url = reply->request().url().toEncoded().constData(); // reply->url() may differ if redirection occurred, so use requested

        if (!isHttpRedirect(reply))
        {
            QByteArray data = reply->readAll();

            bool found = false;
            TileJob *job = m_replies[reply];
            if (!m_tileJobs.contains(job)) {
                qDebug() << "job has been deleted!";
            }

            const bool passThrough = canPassThrough(data, reply, job);

            QImage image;
            if (!reply->error())
            {
                if (!passThrough && !image.loadFromData(data))
                {
                    qDebug() << "MapTileServer::downloadFinished: Failed to load image: " << url;
                }
            }
            else
            {
                qDebug() << "MapTileServer::downloadFinished: Error: " << reply->error() << "for" << url;
            }
            for (const auto& jobURL : job->m_urls)
            {
                if (jobURL == url)
                {
                    if (passThrough)
                    {
                        if (job->m_socket)
                        {
                            replyData(job->m_socket, data);
                            job->m_socket = nullptr;
                            m_tileJobs.removeAll(job);
                            delete job;
                        }
                        else
                        {
                            qDebug() << "Socket was null. URL: " << url << "job:" << job;
                        }
                    }
                    else
                    {
                        job->m_images.insert(url, image);
                        if (job->m_urls.size() == job->m_images.size())
                        {
                            // All images available
                            QImage combinedImage = combine(job);
                            if (job->m_socket)
                            {
                                replyImage(job->m_socket, combinedImage, job->m_format);
                                job->m_socket = nullptr;
                                m_tileJobs.removeAll(job);
                                delete job;
                                //qDebug() << "Delete job" << job;
                            }
                            else
                            {
                                qDebug() << "Socket was null. URL: " << url << "job:" << job;
                            }
                        }
                    }
                    found = true;
                    break;
                }
            }
            if (!found) {
                qDebug() << "MapTileServer::downloadFinished: Failed to match URL: " << url;
            }
        }
        else
        {
            qDebug() << "MapTileServer::downloadFinished: Redirect";
        }
        reply->deleteLater();
        m_replies.remove(reply);
    }

#ifndef QT_NO_OPENSSL
    void sslErrors(const QList<QSslError> &sslErrors)
    {
        for (const QSslError &error : sslErrors)
        {
            qCritical() << "MapTileServer: SSL error" << (int)error.error() << ": " << error.errorString();
    #ifdef ANDROID
            // On Android 6 (but not on 12), we always seem to get: "The issuer certificate of a locally looked up certificate could not be found"
            // which causes downloads to fail, so ignore
            if (error.error() == QSslError::UnableToGetLocalIssuerCertificate)
            {
                QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
                QList<QSslError> errorsThatCanBeIgnored;
                errorsThatCanBeIgnored << QSslError(QSslError::UnableToGetLocalIssuerCertificate, error.certificate());
                reply->ignoreSslErrors(errorsThatCanBeIgnored);
            }
    #endif
        }
    }
#endif

};

#endif
