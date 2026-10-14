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

#include <memory>

#include <QHostAddress>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include "dtv/dtvapps.h"
#include "dtv/dtvcarouselserver.h"

namespace {

constexpr int m_maxHeader = 16384;
constexpr int m_retryMs = 250;
const QString m_hostPrefix = "dvb-";
const QString m_hostSuffix = ".localhost";

} // namespace

DTVCarouselServer::DTVCarouselServer(DTVApps *apps, QObject *parent) :
    QObject(parent),
    m_apps(apps),
    m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &DTVCarouselServer::newConnection);
}

DTVCarouselServer::~DTVCarouselServer()
{
}

bool DTVCarouselServer::start()
{
    if (m_server->isListening()) {
        return true;
    }

    if (!m_server->listen(QHostAddress::LocalHost, 0))
    {
        qWarning("DTVCarouselServer::start: can't listen: %s", qPrintable(m_server->errorString()));
        return false;
    }

    return true;
}

int DTVCarouselServer::getPort() const
{
    return m_server->isListening() ? m_server->serverPort() : 0;
}

QString DTVCarouselServer::httpUrl(const QString& dvbUrl) const
{
    // dvb://onid.tsid.sid.ctag/path, the onid and tsid may be empty, as they're not needed to find the carousel
    if (!dvbUrl.startsWith("dvb://", Qt::CaseInsensitive) || !m_server->isListening()) {
        return QString();
    }

    const QString rest = dvbUrl.mid(6);
    const int slash = rest.indexOf('/');
    const QStringList triplet = rest.left(slash < 0 ? rest.size() : slash).split('.');

    if (triplet.size() != 4) {
        return QString();
    }

    bool sidOK, tagOK;
    triplet[2].toInt(&sidOK, 16);
    triplet[3].toInt(&tagOK, 16);

    if (!sidOK || !tagOK) {
        return QString();
    }

    const QString host = m_hostPrefix + QString("%1-%2-%3-%4")
        .arg(triplet[0].isEmpty() ? QString("x") : triplet[0].toLower())
        .arg(triplet[1].isEmpty() ? QString("x") : triplet[1].toLower())
        .arg(triplet[2].toLower())
        .arg(triplet[3].toLower()) + m_hostSuffix;
    return QString("http://%1:%2%3").arg(host).arg(m_server->serverPort()).arg(slash < 0 ? QString("/") : rest.mid(slash));
}

QString DTVCarouselServer::dvbUrl(const QString& httpUrl) const
{
    const QUrl url(httpUrl);

    if (!isServerUrl(httpUrl)) {
        return QString();
    }

    QStringList parts = url.host().mid(m_hostPrefix.size(), url.host().size() - m_hostPrefix.size() - m_hostSuffix.size()).split('-');

    for (QString& part : parts)
    {
        if (part == "x") {
            part.clear();
        }
    }

    QString result = QString("dvb://%1%2").arg(parts.join('.')).arg(url.path(QUrl::FullyEncoded));

    if (url.hasQuery()) {
        result += "?" + url.query(QUrl::FullyEncoded);
    }

    if (url.hasFragment()) {
        result += "#" + url.fragment(QUrl::FullyEncoded);
    }

    return result;
}

bool DTVCarouselServer::isServerUrl(const QString& url) const
{
    const QUrl u(url);
    int serviceId, componentTag;
    return m_server->isListening() && (u.scheme() == "http") && (u.port() == m_server->serverPort())
        && parseHost(u.host(), serviceId, componentTag);
}

bool DTVCarouselServer::parseHost(const QString& host, int& serviceId, int& componentTag)
{
    QString name = host.toLower();
    const int colon = name.indexOf(':');

    if (colon >= 0) {
        name = name.left(colon);
    }

    if (!name.startsWith(m_hostPrefix) || !name.endsWith(m_hostSuffix)) {
        return false;
    }

    const QStringList parts = name.mid(m_hostPrefix.size(), name.size() - m_hostPrefix.size() - m_hostSuffix.size()).split('-');

    if (parts.size() != 4) {
        return false;
    }

    bool sidOK, tagOK;
    serviceId = parts[2].toInt(&sidOK, 16);
    componentTag = parts[3].toInt(&tagOK, 16);
    return sidOK && tagOK && (serviceId > 0) && (serviceId <= 0xFFFF) && (componentTag >= 0) && (componentTag <= 0xFF);
}

QString DTVCarouselServer::contentType(const QString& path)
{
    const QString lower = path.toLower();
    const int dot = lower.lastIndexOf('.');
    const QString extension = (dot >= 0) && (dot > lower.lastIndexOf('/')) ? lower.mid(dot + 1) : QString();

    // HbbTV's XHTML (application/vnd.hbbtv.xhtml+xml) as HTML, which the browser's more lenient with
    if ((extension == "html") || (extension == "htm") || (extension == "xhtml") || (extension == "cehtml")) {
        return "text/html";
    } else if (extension == "js") {
        return "application/javascript";
    } else if (extension == "css") {
        return "text/css";
    } else if (extension == "json") {
        return "application/json";
    } else if (extension == "xml") {
        return "application/xml";
    } else if (extension == "png") {
        return "image/png";
    } else if ((extension == "jpg") || (extension == "jpeg")) {
        return "image/jpeg";
    } else if (extension == "gif") {
        return "image/gif";
    } else if (extension == "svg") {
        return "image/svg+xml";
    } else if (extension == "woff") {
        return "font/woff";
    } else if (extension == "woff2") {
        return "font/woff2";
    } else if (extension == "ttf") {
        return "font/ttf";
    } else if (extension == "mp4") {
        return "video/mp4";
    } else if (extension == "mp3") {
        return "audio/mpeg";
    } else if (extension == "txt") {
        return "text/plain";
    } else {
        return "application/octet-stream";
    }
}

void DTVCarouselServer::newConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection())
    {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { readRequest(socket); });
    }
}

void DTVCarouselServer::readRequest(QTcpSocket *socket)
{
    QByteArray header = socket->property("header").toByteArray() + socket->readAll();

    if (header.size() > m_maxHeader)
    {
        reply(socket, 431, "Request Header Fields Too Large", "text/plain", QByteArray(), false);
        return;
    }

    const int end = header.indexOf("\r\n\r\n");

    if (end < 0)
    {
        socket->setProperty("header", header);
        return;
    }

    // One request a connection: it's closed after the reply
    disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
    const QList<QByteArray> lines = header.left(end).split('\n');
    const QList<QByteArray> requestLine = lines[0].trimmed().split(' ');

    if ((requestLine.size() < 2) || ((requestLine[0] != "GET") && (requestLine[0] != "HEAD")))
    {
        reply(socket, 405, "Method Not Allowed", "text/plain", QByteArray(), false);
        return;
    }

    std::shared_ptr<Request> request(new Request());
    request->m_socket = socket;
    request->m_head = requestLine[0] == "HEAD";
    QString host;

    for (int i = 1; i < lines.size(); i++)
    {
        const QByteArray line = lines[i].trimmed();

        if (line.toLower().startsWith("host:")) {
            host = QString::fromLatin1(line.mid(5).trimmed());
        }
    }

    // The path, without the query or fragment, decoded: of names in the carousel's tree
    QByteArray target = requestLine[1];
    const int query = target.indexOf('?');

    if (query >= 0) {
        target = target.left(query);
    }

    const QString path = QUrl::fromPercentEncoding(target);

    if (!parseHost(host, request->m_serviceId, request->m_componentTag) || !path.startsWith('/') || path.split('/').contains(".."))
    {
        reply(socket, 404, "Not Found", "text/plain", QByteArray(), request->m_head);
        return;
    }

    request->m_path = path.endsWith('/') ? path + "index.html" : path;
    request->m_waiting.start();
    respond(request.get());

    if (request->m_socket && request->m_socket->property("pending").toBool())
    {
        // Not received yet: tried again, until it is or it's waited too long. The timer goes with the socket
        QTimer *timer = new QTimer(socket);
        timer->setInterval(m_retryMs);
        connect(timer, &QTimer::timeout, this, [this, request, timer]() {
            if (!request->m_socket || !request->m_socket->property("pending").toBool()) {
                timer->stop();
            } else {
                respond(request.get());
            }
        });
        timer->start();
    }
}

void DTVCarouselServer::respond(Request *request)
{
    QTcpSocket *socket = request->m_socket;

    if (!socket) {
        return;
    }

    const int pid = m_apps->getComponentPid(request->m_serviceId, request->m_componentTag);
    QByteArray data;

    if ((pid >= 0) && m_apps->getCarouselFile(pid, request->m_path, data))
    {
        socket->setProperty("pending", false);
        reply(socket, 200, "OK", contentType(request->m_path).toLatin1(), data, request->m_head);
        emit served(request->m_path, 200);
        return;
    }

    // Not there: if its carousel's complete it isn't in it, else it may yet be received
    if (((pid >= 0) && m_apps->isCarouselComplete(pid)) || (request->m_waiting.elapsed() >= m_waitMs))
    {
        socket->setProperty("pending", false);
        reply(socket, 404, "Not Found", "text/plain", QByteArray(), request->m_head);
        emit served(request->m_path, 404);
    }
    else
    {
        socket->setProperty("pending", true);
    }
}

void DTVCarouselServer::reply(QTcpSocket *socket, int status, const QByteArray& reason, const QByteArray& type, const QByteArray& body, bool head)
{
    QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + " " + reason + "\r\n";
    response += "Content-Type: " + type + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Cache-Control: no-cache\r\n";
    response += "Connection: close\r\n\r\n";

    if (!head) {
        response += body;
    }

    socket->write(response);
    socket->disconnectFromHost();
}
