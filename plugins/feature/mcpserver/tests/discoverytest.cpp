// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include <cstdio>
#include <cstdlib>
#include <QProcess>
#include "httplistener.h"
#include "mcprequesthandler.h"
#include "maincore.h"
#include "mcpprotocol.h"
#include "mcpstreams.h"
#include "mcptoolcatalog.h"

class DiscoveryTest : public QObject
{
    Q_OBJECT
    static QJsonObject request(MCPProtocol& protocol, MCPProtocol::Context& context,
        const QString& method, const QJsonObject& params = {})
    {
        QJsonObject response;
        const QJsonObject message{{"jsonrpc", "2.0"}, {"id", 1}, {"method", method}, {"params", params}};
        if (!protocol.handleMessage(message, response, context)) { return {}; }
        return response;
    }
    static QJsonObject call(MCPProtocol& protocol, MCPProtocol::Context& context,
        const QString& name, const QJsonObject& args = {})
    {
        return request(protocol, context, "tools/call", {{"name", name}, {"arguments", args}})["result"].toObject();
    }
private slots:
    void initTestCase()
    {
        QCoreApplication::setLibraryPaths({});
        MainCore::instance();
    }

    void discoveryAndSessionIsolation()
    {
        MCPStreams streams;
        MCPProtocol protocol(nullptr);
        protocol.setStreams(&streams);
        MCPProtocol::Context full;
        const QJsonObject initialized = request(protocol, full, "initialize")["result"].toObject();
        QVERIFY(!initialized["capabilities"].toObject()["tools"].toObject()["listChanged"].toBool());
        full.m_sessionId = full.m_newSessionId;
        const QJsonArray all = request(protocol, full, "tools/list")["result"].toObject()["tools"].toArray();
        QVERIFY(all.size() > 60);

        MCPProtocol::Context compact;
        compact.m_compactTools = true;
        const QJsonObject compactInit = request(protocol, compact, "initialize")["result"].toObject();
        QVERIFY(compactInit["capabilities"].toObject()["tools"].toObject()["listChanged"].toBool());
        QVERIFY(compactInit["instructions"].toString().contains("search_tools"));
        compact.m_sessionId = compact.m_newSessionId;
        // After initialize the URL option is no longer needed; the session owns the profile.
        compact.m_compactTools = false;
        const QJsonArray initial = request(protocol, compact, "tools/list")["result"].toObject()["tools"].toArray();
        QCOMPARE(initial.size(), 8);
        QCOMPARE(initial, MCPToolCatalog::filter(protocol.getTools().listTools(true), MCPToolCatalog::initialTools()));

        const QString other = streams.createSession(true);
        QString error;
        const auto stream = streams.open(compact.m_sessionId, error);
        const auto otherStream = streams.open(other, error);
        QVERIFY(!stream.isNull());
        QVERIFY(!otherStream.isNull());
        const QJsonObject found = call(protocol, compact, "search_tools", {{"query", "list_docs"}, {"limit", 1}});
        QVERIFY(!found["isError"].toBool());
        const QJsonArray matches = found["structuredContent"].toObject()["tools"].toArray();
        QCOMPARE(matches.size(), 1);
        QCOMPARE(matches.first().toObject()["name"].toString(), QString("list_docs"));
        const QJsonObject event = QJsonDocument::fromJson(stream->waitForEvent(1)).object();
        QCOMPARE(event["method"].toString(), QString("notifications/tools/list_changed"));
        QVERIFY(otherStream->waitForEvent(1).isEmpty());
        QCOMPARE(streams.visibleTools(other), MCPToolCatalog::initialTools());
        const QJsonArray expanded = request(protocol, compact, "tools/list")["result"].toObject()["tools"].toArray();
        QCOMPARE(expanded.size(), 9);
        QVERIFY(expanded.contains(matches.first()));
        QVERIFY(all.contains(matches.first())); // The original definition and safety hints are retained.
        QVERIFY(!call(protocol, compact, "list_docs")["isError"].toBool());
        QCOMPARE(request(protocol, full, "tools/list")["result"].toObject()["tools"].toArray(), all);

        // A duplicate search and a missing match do not generate redundant notifications.
        call(protocol, compact, "search_tools", {{"query", "list_docs"}, {"limit", 1}});
        call(protocol, compact, "search_tools", {{"query", "no_such_tool"}});
        QVERIFY(stream->waitForEvent(1).isEmpty());
        // Failed validation cannot reveal anything or change the catalog.
        QVERIFY(call(protocol, compact, "search_tools", {{"query", "list_docs"}, {"limit", 0}})["isError"].toBool());
        QVERIFY(call(protocol, compact, "search_tools", {{"query", "list_docs"}, {"unexpected", true}})["isError"].toBool());
        QCOMPARE(request(protocol, compact, "tools/list")["result"].toObject()["tools"].toArray(), expanded);
        QVERIFY(request(protocol, compact, "tools/call", {{"name", "nonexistent"}}).contains("error"));
        streams.endSession(compact.m_sessionId);
        QVERIFY(stream->isClosed());
        QVERIFY(!streams.usesCompactTools(compact.m_sessionId));
        streams.close(stream);
        streams.close(otherStream);
    }

    void fixedCatalogCallsAndGuards()
    {
        MCPStreams streams;
        MCPProtocol protocol(nullptr);
        protocol.setStreams(&streams);
        MCPProtocol::Context compact;
        compact.m_compactTools = true;
        request(protocol, compact, "initialize");
        const QJsonArray initial = request(protocol, compact, "tools/list")["result"].toObject()["tools"].toArray();
        const QJsonArray found = call(protocol, compact, "search_tools", {{"query", "list_docs"}, {"limit", 1}})
            ["structuredContent"].toObject()["tools"].toArray();
        QVERIFY(!initial.contains(found.first()));
        for (const QString& query : {QString("documentation"), QString("search_tools"), QString("read_tool")})
        {
            const QJsonArray hits = call(protocol, compact, "search_tools", {{"query", query}, {"limit", 20}})
                ["structuredContent"].toObject()["tools"].toArray();
            for (const QJsonValue& hit : hits) {
                QVERIFY(!MCPToolCatalog::isCompactTool(hit.toObject()["name"].toString()));
            }
        }
        // Keep the initial catalog frozen: no SSE connection or tools/list refresh.
        const QJsonObject routed = call(protocol, compact, "read_tool", {{"name", "list_docs"}, {"arguments", QJsonObject()}});
        QVERIFY(!routed["isError"].toBool());
        QCOMPARE(routed, call(protocol, compact, "list_docs"));
        QVERIFY(!routed["structuredContent"].toObject().contains("content")); // No double wrapping.

        const QList<QJsonObject> invalidReads = {
            {{"name", "set_center_frequency"}, {"arguments", QJsonObject{{"deviceSetIndex", 0}, {"frequency", 145500000}}}},
            {{"name", "write_tool"}, {"arguments", QJsonObject()}},
            {{"name", "read_tool"}, {"arguments", QJsonObject()}},
            {{"name", "search_tools"}, {"arguments", QJsonObject()}},
            {{"name", "unknown_tool"}, {"arguments", QJsonObject()}},
            {{"name", "list_docs"}, {"arguments", QJsonArray()}},
            {{"name", "list_docs"}},
            {{"name", "list_docs"}, {"arguments", "{}"}},
            {{"name", "list_docs"}, {"arguments", QJsonValue()}},
            {{"name", 1}, {"arguments", QJsonObject()}},
            {{"name", "  "}, {"arguments", QJsonObject()}},
            {{"name", "list_docs"}, {"arguments", QJsonObject()}, {"unknown", 1}}
        };
        for (const QJsonObject& args : invalidReads) {
            QVERIFY(call(protocol, compact, "read_tool", args)["isError"].toBool());
        }
        // A get_ prefix does not imply read-only: history enables collection on first use.
        const QJsonObject historyRead = call(protocol, compact, "read_tool", {
            {"name", "get_spectrum_history"}, {"arguments", QJsonObject{{"deviceSetIndex", 0}}}});
        QVERIFY(historyRead["isError"].toBool());
        QVERIFY(historyRead["content"].toArray().first().toObject()["text"].toString().contains("write_tool"));
        QVERIFY(call(protocol, compact, "write_tool", {{"name", "list_docs"}, {"arguments", QJsonObject()}})["isError"].toBool());
        // Native target validation failures must remain errors, not successful nested results.
        QCOMPARE(call(protocol, compact, "read_tool", {{"name", "list_docs"}, {"arguments", QJsonObject{{"unknown", true}}}}),
            call(protocol, compact, "list_docs", {{"unknown", true}}));
        QVERIFY(call(protocol, compact, "write_tool", {{"name", "set_center_frequency"}, {"arguments", QJsonObject()}})["isError"].toBool());

        MCPProtocol::Context full;
        request(protocol, full, "initialize");
        const QJsonArray all = request(protocol, full, "tools/list")["result"].toObject()["tools"].toArray();
        for (const QJsonValue& value : all) {
            QVERIFY(!MCPToolCatalog::isCompactTool(value.toObject()["name"].toString()));
        }
        QVERIFY(request(protocol, full, "tools/call", {{"name", "search_tools"},
            {"arguments", QJsonObject{{"query", "list_docs"}}}}).contains("error"));
        QVERIFY(request(protocol, full, "tools/call", {{"name", "read_tool"},
            {"arguments", QJsonObject{{"name", "list_docs"}, {"arguments", QJsonObject()}}}}).contains("error"));
        for (const QJsonValue& value : initial)
        {
            const QJsonObject tool = value.toObject();
            if (tool["name"] == "read_tool")
            {
                const QJsonObject hints = tool["annotations"].toObject();
                QVERIFY(hints["readOnlyHint"].toBool());
                QVERIFY(!hints["destructiveHint"].toBool(true));
                QVERIFY(hints["openWorldHint"].toBool(true));
            }
            if (tool["name"] == "write_tool")
            {
                const QJsonObject hints = tool["annotations"].toObject();
                QVERIFY(!hints["readOnlyHint"].toBool());
                QVERIFY(hints["destructiveHint"].toBool(true));
                QVERIFY(!hints["idempotentHint"].toBool());
            }
        }
    }

    void expiredSessionsReleaseDiscoveryState()
    {
        MCPStreams streams;
        const QString first = streams.createSession(true);
        // Keep timestamps distinct so the oldest idle session is deterministic.
        QTest::qWait(2);
        for (int i = 0; i < MCPStreams::m_maxSessions; ++i) { streams.createSession(true); }
        QVERIFY(!streams.sessionExists(first));
        QVERIFY(!streams.usesCompactTools(first));
    }
};
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const bool httpTest = app.arguments().contains("--http-test");
    if (httpTest || app.arguments().contains("--serve"))
    {
        QCoreApplication::setLibraryPaths({});
        MainCore::instance();
        MCPStreams streams;
        MCPProtocol protocol(nullptr);
        protocol.setStreams(&streams);
        MCPRequestHandler handler(&protocol, &streams);
        qtwebapp::HttpListenerSettings settings;
        settings.host = "127.0.0.1";
        settings.port = 0;
        qtwebapp::HttpListener listener(settings, &handler);
        if (!listener.isListening()) { return 1; }
        std::printf("PORT=%u\n", listener.serverPort());
        std::fflush(stdout);
        QProcess client;
        if (httpTest)
        {
#ifdef MCP_HTTP_TEST_PYTHON
            // Paths are fixed by CMake's test configuration. Only the ephemeral port is
            // passed at runtime; the Python client never launches an arbitrary executable.
            client.setProcessChannelMode(QProcess::ForwardedChannels);
            QObject::connect(&client, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [](int code, QProcess::ExitStatus status) {
                    std::fflush(nullptr);
                    // MainCore's global workers need the same one-shot exit as below.
                    std::_Exit(status == QProcess::NormalExit ? code : 1);
                });
            QObject::connect(&client, &QProcess::errorOccurred, [](QProcess::ProcessError) {
                std::fflush(nullptr);
                std::_Exit(1);
            });
            client.start(QString::fromUtf8(MCP_HTTP_TEST_PYTHON), {
                QString::fromUtf8(MCP_HTTP_TEST_SCRIPT), QString::number(listener.serverPort())});
#else
            return 1;
#endif
        }
        return app.exec();
    }
    DiscoveryTest test;
    const int result = QTest::qExec(&test, argc, argv);
    // As in toolsdump, this one-shot process never runs MainCore's application event
    // loop. All test-local objects have been destroyed; skip its global worker teardown.
    std::fflush(nullptr);
    std::_Exit(result);
}
#include "discoverytest.moc"
