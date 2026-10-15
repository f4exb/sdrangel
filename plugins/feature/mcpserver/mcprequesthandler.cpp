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

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QUrl>

#include "httprequest.h"
#include "httpresponse.h"

#include "mcpprotocol.h"
#include "mcpstreams.h"
#include "mcprequesthandler.h"

MCPRequestHandler::MCPRequestHandler(MCPProtocol *protocol, MCPStreams *streams, QObject *parent) :
    qtwebapp::HttpRequestHandler(parent),
    m_protocol(protocol),
    m_streams(streams),
    m_requestCount(0)
{
}

void MCPRequestHandler::setToken(const QString& token)
{
    QMutexLocker locker(&m_mutex);
    m_token = token;
}

void MCPRequestHandler::resetStatistics()
{
    QMutexLocker locker(&m_mutex);
    m_requestCount.storeRelaxed(0);
    m_lastRequest.clear();
}

QString MCPRequestHandler::getLastRequest() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastRequest;
}

void MCPRequestHandler::recordRequest(const QString& what)
{
    QMutexLocker locker(&m_mutex);
    m_requestCount.fetchAndAddRelaxed(1);
    m_lastRequest = what;
}

void MCPRequestHandler::service(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    QByteArray path = request.getPath();
    QByteArray method = request.getMethod();

    if ((path != "/mcp") && (path != "/mcp/") && (path != "/"))
    {
        sendHttpError(response, 404, "Not Found", QString("Unknown path %1. The MCP endpoint is /mcp").arg(QString(path)));
        return;
    }

    if (!checkOrigin(request, response)) {
        return;
    }

    if (!checkAuthorization(request, response)) {
        return;
    }

    if (!checkProtocolVersion(request, response)) {
        return;
    }

    if (method == "POST")
    {
        handlePost(request, response);
    }
    else if (method == "GET")
    {
        handleGet(request, response);
    }
    else if (method == "DELETE")
    {
        handleDelete(request, response);
    }
    else if (method == "OPTIONS")
    {
        response.setHeader("Allow", "POST, GET, DELETE");
        sendEmpty(response, 204, "No Content");
    }
    else
    {
        response.setHeader("Allow", "POST, GET, DELETE");
        sendEmpty(response, 405, "Method Not Allowed");
    }
}

// Clients send MCP-Protocol-Version on every request after initialize. A version this server
// does not implement is refused with 400 rather than served on a guess, as the Streamable HTTP
// transport requires. A missing header means the oldest version, which is supported.
bool MCPRequestHandler::checkProtocolVersion(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    QString version = QString::fromUtf8(request.getHeader("MCP-Protocol-Version")).trimmed();

    if (version.isEmpty() || MCPProtocol::isSupportedVersion(version)) {
        return true;
    }

    sendHttpError(response, 400, "Bad Request",
        QString("Unsupported MCP protocol version %1. This server supports %2")
            .arg(version).arg(MCPProtocol::supportedVersions().join(", ")));
    return false;
}

// A session id is optional: clients that send one must send a valid one, and a client that
// sends none shares the anonymous session. An unknown id gets 404, which tells the client
// to start a new session with initialize, as the specification requires.
bool MCPRequestHandler::checkSession(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response, QString& sessionId)
{
    sessionId = QString::fromUtf8(request.getHeader("Mcp-Session-Id")).trimmed();

    if (sessionId.isEmpty() || m_streams->sessionExists(sessionId)) {
        return true;
    }

    sendHttpError(response, 404, "Not Found", QString("Unknown session %1. Call initialize to start a new one").arg(sessionId));
    return false;
}

// Opens the server to client event stream. This thread is held until the stream is closed:
// by the client going away, by the session ending, by the server stopping, or by the
// lifetime backstop below.
void MCPRequestHandler::handleGet(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    QByteArray accept = request.getHeader("Accept");

    if (!accept.contains("text/event-stream"))
    {
        sendHttpError(response, 406, "Not Acceptable", "GET opens the event stream and needs Accept: text/event-stream");
        return;
    }

    QString sessionId;

    if (!checkSession(request, response, sessionId)) {
        return;
    }

    QString error;
    MCPStreams::StreamPtr stream = m_streams->open(sessionId, error);

    if (stream.isNull())
    {
        sendHttpError(response, 503, "Service Unavailable", error);
        return;
    }

    recordRequest("GET event stream");
    response.setStatus(200);
    response.setHeader("Content-Type", "text/event-stream");
    response.setHeader("Cache-Control", "no-cache");
    response.setHeader("X-Accel-Buffering", "no"); // ask proxies not to buffer the stream

    if (!sessionId.isEmpty()) {
        response.setHeader("Mcp-Session-Id", sessionId.toUtf8());
    }

    // Opens the response in chunked mode and sends the headers
    response.write(QByteArray(": stream opened\n\n"), false);
    response.flush();

    QElapsedTimer keepalive;
    keepalive.start();

    while (!stream->isClosed() && response.isConnected() && (stream->ageMs() < MCPStreams::m_maxStreamLifetimeMs))
    {
        // A short wait so that closing the stream, and with it stopping the server, is prompt
        QByteArray event = stream->waitForEvent(250);

        if (!event.isEmpty())
        {
            response.write("data: " + event + "\n\n", false);
            response.flush();
        }
        else if (keepalive.elapsed() > 15000)
        {
            // Keeps intermediaries from timing the connection out, and eventually fails
            // if the client has gone away without closing the connection
            response.write(QByteArray(": keepalive\n\n"), false);
            response.flush();
            keepalive.restart();
        }
    }

    response.write(QByteArray(), true);
    m_streams->close(stream);
}

void MCPRequestHandler::handleDelete(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    QString sessionId;

    if (!checkSession(request, response, sessionId)) {
        return;
    }

    if (sessionId.isEmpty())
    {
        // Nothing identifies what to end
        response.setHeader("Allow", "POST, GET, DELETE");
        sendHttpError(response, 405, "Method Not Allowed", "DELETE ends a session and needs the Mcp-Session-Id header");
        return;
    }

    recordRequest("DELETE session");
    m_streams->endSession(sessionId);
    sendEmpty(response, 200, "OK");
}

// Guard against DNS rebinding and other cross-site attacks from browsers. Native MCP clients
// send no Origin header. A browser page is only accepted from localhost; the opaque "null"
// origin (sandboxed frames, file:// pages, redirects) is rejected because it cannot be trusted.
bool MCPRequestHandler::checkOrigin(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    QByteArray origin = request.getHeader("Origin").trimmed();

    if (origin.isEmpty()) {
        return true;
    }

    QUrl url(QString::fromUtf8(origin));
    QString host = url.host();
    QString scheme = url.scheme();

    if (url.isValid() && ((scheme == "http") || (scheme == "https"))
        && ((host == "localhost") || (host == "127.0.0.1") || (host == "::1")))
    {
        return true;
    }

    sendHttpError(response, 403, "Forbidden", QString("Origin %1 not allowed").arg(QString(origin)));
    return false;
}

bool MCPRequestHandler::checkAuthorization(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    QString token;
    {
        QMutexLocker locker(&m_mutex);
        token = m_token;
    }

    if (token.isEmpty()) {
        return true;
    }

    QByteArray auth = request.getHeader("Authorization").trimmed();

    if (auth.startsWith("Bearer ") && (QString(auth.mid(7)).trimmed() == token)) {
        return true;
    }

    response.setHeader("WWW-Authenticate", "Bearer");
    sendHttpError(response, 401, "Unauthorized", "Missing or invalid bearer token");
    return false;
}

void MCPRequestHandler::handlePost(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response)
{
    // Only accept declared JSON. Browsers can send text/plain cross-origin without a preflight,
    // so requiring application/json is part of the cross-site defence.
    QByteArray contentType = request.getHeader("Content-Type").trimmed().toLower();

    if (!contentType.startsWith("application/json"))
    {
        sendHttpError(response, 415, "Unsupported Media Type", "Content-Type must be application/json");
        return;
    }

    QString sessionId;

    if (!checkSession(request, response, sessionId)) {
        return;
    }

    MCPProtocol::Context context;
    context.m_sessionId = sessionId;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(request.getBody(), &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        QJsonObject error = MCPProtocol::makeError(QJsonValue(), MCPProtocol::ParseError,
            QString("Parse error: %1").arg(parseError.errorString()));
        sendJson(response, 400, QJsonDocument(error));
        return;
    }

    // A batch is an array of messages. Batches were removed in protocol version 2025-06-18
    // but older clients may still send them so both forms are handled.
    QList<QJsonValue> messages;
    bool batch = doc.isArray();

    if (batch)
    {
        for (const auto& value : doc.array()) {
            messages.append(value);
        }
    }
    else if (doc.isObject())
    {
        messages.append(doc.object());
    }

    if (messages.isEmpty())
    {
        QJsonObject error = MCPProtocol::makeError(QJsonValue(), MCPProtocol::InvalidRequest,
            batch ? "Invalid request: empty batch" : "Invalid request: expected a JSON-RPC object or array");
        sendJson(response, 400, QJsonDocument(error));
        return;
    }

    QJsonArray responses;

    for (const auto& value : messages)
    {
        if (!value.isObject())
        {
            responses.append(MCPProtocol::makeError(QJsonValue(), MCPProtocol::InvalidRequest, "Invalid request: batch member is not an object"));
            continue;
        }

        QJsonObject message = value.toObject();
        QString method = message["method"].toString();

        if (!method.isEmpty()) {
            recordRequest(method == "tools/call" ? QString("tools/call %1").arg(message["params"].toObject()["name"].toString()) : method);
        }

        QJsonObject reply;

        if (m_protocol->handleMessage(message, reply, context)) {
            responses.append(reply);
        }
    }

    // initialize creates a session; the client sends it back in the Mcp-Session-Id header
    if (!context.m_newSessionId.isEmpty()) {
        response.setHeader("Mcp-Session-Id", context.m_newSessionId.toUtf8());
    }

    if (responses.isEmpty())
    {
        // Only notifications or responses: acknowledge with no body
        sendEmpty(response, 202, "Accepted");
    }
    else if (batch)
    {
        sendJson(response, 200, QJsonDocument(responses));
    }
    else
    {
        sendJson(response, 200, QJsonDocument(responses.first().toObject()));
    }
}

void MCPRequestHandler::sendJson(qtwebapp::HttpResponse& response, int status, const QJsonDocument& doc)
{
    response.setStatus(status);
    response.setHeader("Content-Type", "application/json");
    response.write(doc.toJson(QJsonDocument::Compact), true);
}

void MCPRequestHandler::sendEmpty(qtwebapp::HttpResponse& response, int status, const QByteArray& reason)
{
    response.setStatus(status, reason);
    response.write(QByteArray(), true);
}

void MCPRequestHandler::sendHttpError(qtwebapp::HttpResponse& response, int status, const QByteArray& reason, const QString& message)
{
    QJsonObject error;
    error["error"] = message;
    response.setStatus(status, reason);
    response.setHeader("Content-Type", "application/json");
    response.write(QJsonDocument(error).toJson(QJsonDocument::Compact), true);
}
