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

#ifndef INCLUDE_FEATURE_MCPREQUESTHANDLER_H_
#define INCLUDE_FEATURE_MCPREQUESTHANDLER_H_

#include <QMutex>
#include <QAtomicInt>
#include <QString>
#include <QJsonDocument>

#include "httprequesthandler.h"

class MCPProtocol;
class MCPStreams;

// MCP Streamable HTTP transport on top of the qtwebapp HTTP server.
//
// Clients POST JSON-RPC 2.0 messages to /mcp and receive a JSON response. A GET with
// Accept: text/event-stream opens a server to client SSE stream, which carries
// notifications/resources/updated for the resources the client has subscribed to.
// DELETE ends the session named by the Mcp-Session-Id header.
//
// service() is called from the HTTP connection handler threads, never from the main thread,
// so it is safe to block here: while serving an SSE stream this thread is held for the life
// of the stream.
class MCPRequestHandler : public qtwebapp::HttpRequestHandler
{
    Q_OBJECT
public:
    MCPRequestHandler(MCPProtocol *protocol, MCPStreams *streams, QObject *parent = nullptr);

    void setToken(const QString& token);
    void resetStatistics();
    int getRequestCount() const { return m_requestCount.loadRelaxed(); }
    QString getLastRequest() const;

    virtual void service(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response) override;

private:
    MCPProtocol *m_protocol;
    MCPStreams *m_streams;
    QString m_token;
    QAtomicInt m_requestCount;
    QString m_lastRequest;
    mutable QMutex m_mutex;

    bool checkOrigin(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    bool checkAuthorization(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void handlePost(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void handleGet(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void handleDelete(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    bool checkSession(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response, QString& sessionId);
    bool checkProtocolVersion(qtwebapp::HttpRequest& request, qtwebapp::HttpResponse& response);
    void sendJson(qtwebapp::HttpResponse& response, int status, const QJsonDocument& doc);
    void sendEmpty(qtwebapp::HttpResponse& response, int status, const QByteArray& reason);
    void sendHttpError(qtwebapp::HttpResponse& response, int status, const QByteArray& reason, const QString& message);
    void recordRequest(const QString& what);
};

#endif // INCLUDE_FEATURE_MCPREQUESTHANDLER_H_
