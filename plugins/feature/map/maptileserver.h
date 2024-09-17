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
#include <QPainter>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkDiskCache>

class MapTileServer : public QTcpServer
{
    Q_OBJECT
private:
    QString m_thunderforestAPIKey;
    QString m_maptilerAPIKey;
    QNetworkAccessManager m_manager;
    QMutex m_mutex;

    struct TileJob {
        QTcpSocket* m_socket;
        QList<QString> m_urls;
        QHash<QString, QImage> m_images;
        QString m_format;
    };
    QList<TileJob *> m_tileJobs;
    QHash<QNetworkReply *, TileJob *> m_replies;

    QNetworkDiskCache *m_cache;

    QString m_radarPath;
    QString m_satellitePath;
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
        m_radarPath(""),
        m_satellitePath(""),
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

    void setRadarPath(const QString& radarPath)
    {
        m_radarPath = radarPath;
    }

    void setSatellitePath(const QString& satellitePath)
    {
        m_satellitePath = satellitePath;
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

    QImage combine(const TileJob *job)
    {
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
            if (img.format() != QImage::Format_Invalid) {
                painter.drawImage(image.rect(), img);
            }
        }

        return image;
    }

    void replyImage(QTcpSocket* socket, const QImage& image, const QString& format)
    {
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
            qDebug() << "HTTP Request: " << line;
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
                        baseMapURL = QString("http://1.basemaps.cartocdn.com/%4/%3/%1/%2.png").arg(x).arg(y).arg(z).arg(map);
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
                    if (m_displayClouds && !m_satellitePath.isEmpty()) {
                        job->m_urls.append(QString("https://tilecache.rainviewer.com%4/256/%3/%1/%2/0/0_0.png").arg(x).arg(y).arg(z).arg(m_satellitePath));
                    }
                    if (m_displayRain && !m_radarPath.isEmpty()) {
                        job->m_urls.append(QString("https://tilecache.rainviewer.com%4/256/%3/%1/%2/4/1_1.png").arg(x).arg(y).arg(z).arg(m_radarPath));
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
            QImage image;
            if (!reply->error())
            {
                if (!image.loadFromData(data))
                {
                    qDebug() << "MapTileServer::downloadFinished: Failed to load image: " << url;
                }
            }
            else
            {
                qDebug() << "MapTileServer::downloadFinished: Error: " << reply->error() << "for" << url;
            }

            bool found = false;
            TileJob *job = m_replies[reply];
            if (!m_tileJobs.contains(job)) {
                qDebug() << "job has been deleted!";
            }
            for (const auto& jobURL : job->m_urls)
            {
                if (jobURL == url)
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
