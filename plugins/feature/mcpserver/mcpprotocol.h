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

#ifndef INCLUDE_FEATURE_MCPPROTOCOL_H_
#define INCLUDE_FEATURE_MCPPROTOCOL_H_

#include <QMutex>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "mcptools.h"

class MCPStreams;

class WebAPIAdapterInterface;

// Model Context Protocol JSON-RPC layer: initialize, tools, resources and prompts.
// Transport independent. Calls are serialized with a mutex so tool handlers
// never run concurrently.
class MCPProtocol
{
public:
    enum ErrorCode {
        ParseError = -32700,
        InvalidRequest = -32600,
        MethodNotFound = -32601,
        InvalidParams = -32602,
        InternalError = -32603
    };

    // Per request state exchanged with the transport
    struct Context
    {
        QString m_sessionId;    //!< In: the Mcp-Session-Id header of the request, empty if none
        QString m_newSessionId; //!< Out: set by initialize, to be returned in the response header
    };

    explicit MCPProtocol(WebAPIAdapterInterface *webAPIAdapterInterface);

    void setStreams(MCPStreams *streams) { m_streams = streams; m_tools.setStreams(streams); }

    // Handles one JSON-RPC message. Returns true if response holds a reply that must be sent
    // (i.e. the message was a request rather than a notification or response).
    bool handleMessage(const QJsonObject& message, QJsonObject& response, Context& context);

    void setCaptureDir(const QString& dir) { m_tools.capture().setCaptureDir(dir); }
    MCPDataFeed& getDataFeed() { return m_tools.dataFeed(); }
    MCPTools& getTools() { return m_tools; }
    void setOwnerFeature(const QObject *feature) { m_tools.setOwnerFeature(feature); }

    static QJsonObject makeResult(const QJsonValue& id, const QJsonValue& result);
    static QJsonObject makeError(const QJsonValue& id, int code, const QString& message);

    static QStringList supportedVersions();
    static bool isSupportedVersion(const QString& version);

    static const char* const m_serverName;
    static const char* const m_latestProtocolVersion;

private:
    WebAPIAdapterInterface *m_webAPIAdapterInterface;
    MCPTools m_tools;
    MCPStreams *m_streams;
    QMutex m_mutex;

    QJsonValue dispatch(const QString& method, const QJsonObject& params, Context& context);
    //!< True for the tools that block for seconds, which must not hold the dispatch lock
    static bool isLongRunning(const QString& method, const QJsonObject& params);

    QJsonValue initialize(const QJsonObject& params, Context& context);
    QJsonValue resourcesSubscribe(const QJsonObject& params, Context& context, bool subscribe);
    QJsonValue toolsList(const QJsonObject& params);
    QJsonValue toolsCall(const QJsonObject& params);
    QJsonValue resourcesList(const QJsonObject& params);
    QJsonValue resourcesTemplatesList(const QJsonObject& params);
    QJsonValue resourcesRead(const QJsonObject& params);
    QJsonValue promptsList(const QJsonObject& params);
    QJsonValue promptsGet(const QJsonObject& params);

    static QJsonObject textContent(const QString& text);
    static QJsonObject promptMessage(const QString& text);
    static QString instructions();
};

#endif // INCLUDE_FEATURE_MCPPROTOCOL_H_
