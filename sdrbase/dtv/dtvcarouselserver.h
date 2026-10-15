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

#ifndef INCLUDE_DTVCAROUSELSERVER_H
#define INCLUDE_DTVCAROUSELSERVER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QString>

#include "export.h"

class DTVApps;
class QNetworkAccessManager;
class QTcpServer;
class QTcpSocket;

// Serves the object carousels DTVApps receives over HTTP, on the loopback interface, for a browser running their
// HbbTV applications. A carousel's dvb: URL (TS 102 851: dvb://onid.tsid.sid.ctag/path, in hex) is served as
// http://dvb-onid-tsid-sid-ctag.localhost:port/path: each carousel its own origin, as its applications read files by
// paths from its root, and *.localhost is the loopback interface to the browser (RFC 6761) without a resolver. A file
// not received yet is waited for, as a receiver does while it loads the carousel's modules
class SDRBASE_API DTVCarouselServer : public QObject
{
    Q_OBJECT

public:
    explicit DTVCarouselServer(DTVApps *apps, QObject *parent = nullptr);
    ~DTVCarouselServer();

    bool start();               //!< Listens on a port of the loopback interface
    int getPort() const;
    //! The HTTP URL a dvb: URL's served at: empty if it isn't one of a carousel (as dvb://current.ait/...)
    QString httpUrl(const QString& dvbUrl) const;
    //! The dvb: URL an HTTP URL of the server's is of: empty if it isn't one
    QString dvbUrl(const QString& httpUrl) const;
    //! Whether a URL's of the server
    bool isServerUrl(const QString& url) const;
    static QString contentType(const QString& path);
    //! Requests a carousel's application makes to other sites, made for it with the origin a TV gives it, its
    //! service's dvb: URL (dvb://onid.tsid.sid), which a browser can't always be made to (as Qt 5's WebEngine): to
    //! getProxyPath()?url=..., of the carousel's own origin, so they aren't cross-origin. Off until enabled
    void setProxy(bool enabled, const QString& userAgent);
    //! As /__sdrangel_proxy/token: the token's random, so the proxy's only of pages given it
    QString getProxyPath() const;

    static constexpr int m_waitMs = 20000;      //!< For a file not received yet

signals:
    //! A request answered: the file's path, and the HTTP status
    void served(const QString& path, int status);

private:
    struct Request
    {
        QPointer<QTcpSocket> m_socket;
        QByteArray m_header;
        QElapsedTimer m_waiting;
        bool m_head = false;
        int m_serviceId = 0;
        int m_componentTag = 0;
        QString m_path;
    };

    void newConnection();
    void readRequest(QTcpSocket *socket);
    void respond(Request *request);
    void reply(QTcpSocket *socket, int status, const QByteArray& reason, const QByteArray& type, const QByteArray& body, bool head);
    void proxy(QTcpSocket *socket, const QByteArray& method, const QByteArray& target, const QList<QByteArray>& lines,
        const QByteArray& body, const QString& host);
    static bool parseHost(const QString& host, int& serviceId, int& componentTag);

    DTVApps *m_apps;
    QTcpServer *m_server;
    QNetworkAccessManager *m_network;
    QByteArray m_proxyToken;
    bool m_proxyEnabled;
    QString m_userAgent;
};

#endif // INCLUDE_DTVCAROUSELSERVER_H
