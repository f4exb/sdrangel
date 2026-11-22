// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include "mcptoolcatalog.h"
#include "mcperror.h"

class CatalogTest : public QObject
{
    Q_OBJECT
private slots:
    void searchAndFilter()
    {
        const QJsonObject schema{{"type", "object"}, {"properties", QJsonObject{{"source", QJsonObject{{"type", "string"}}}}}};
        const QJsonObject hints{{"readOnlyHint", true}, {"openWorldHint", true}};
        const QJsonObject ships{{"name", "get_map_items"}, {"description", "Decoded ships and aircraft"},
            {"inputSchema", schema}, {"annotations", hints}};
        const QJsonObject status{{"name", "get_status"}, {"description", "Current receiver status"}};
        const QJsonArray tools{status, ships};
        QCOMPARE(MCPToolCatalog::search(tools, {{"query", "GET_MAP_ITEMS"}, {"limit", 1}})["tools"].toArray(), QJsonArray{ships});
        QCOMPARE(MCPToolCatalog::search(tools, {{"query", "ships"}})["tools"].toArray(), QJsonArray{ships});
        QCOMPARE(MCPToolCatalog::search(tools, {{"query", "current ships"}})["tools"].toArray().size(), 2);
        QCOMPARE(MCPToolCatalog::search(tools, {{"query", "get_"}, {"limit", 1}})["total"].toInt(), 2);
        QCOMPARE(MCPToolCatalog::search(tools, {{"query", "missing"}})["tools"].toArray().size(), 0);
        QCOMPARE(MCPToolCatalog::filter(tools, {"get_map_items"}), QJsonArray{ships});
        QVERIFY(MCPToolCatalog::initialTools().contains("search_tools"));
    }

    void exactNameWinsAndOrderingIsStable()
    {
        const QJsonObject exact{{"name", "read"}, {"description", "Read a value"}};
        const QJsonObject other{{"name", "read_more"}, {"description", "Read a value"}};
        const QJsonArray tools{other, exact};
        const QJsonArray found = MCPToolCatalog::search(tools, {{"query", "read"}})["tools"].toArray();
        QCOMPARE(found.first().toObject(), exact);
        QCOMPARE(MCPToolCatalog::search(tools, {{"query", "value"}})["tools"].toArray(), tools);
    }

    void invalidArguments()
    {
        const QList<QJsonObject> invalid = {
            {}, {{"query", 1}}, {{"query", "  "}}, {{"query", QString(257, 'x')}},
            {{"query", "x"}, {"limit", 0}}, {{"query", "x"}, {"limit", 21}},
            {{"query", "x"}, {"limit", 1.5}}, {{"query", "x"}, {"limit", "2"}},
            {{"query", "x"}, {"limit", QJsonValue()}}, {{"query", "x"}, {"limit", true}}
        };
        for (const QJsonObject& args : invalid) {
            QVERIFY_EXCEPTION_THROWN(MCPToolCatalog::search({}, args), MCPToolError);
        }
    }
};
QTEST_GUILESS_MAIN(CatalogTest)
#include "catalogtest.moc"
