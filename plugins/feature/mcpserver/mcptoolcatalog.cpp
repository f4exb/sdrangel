// SPDX-License-Identifier: GPL-3.0-or-later
#include "mcptoolcatalog.h"

#include <algorithm>
#include <cmath>
#include <QRegularExpression>

#include "mcperror.h"

QSet<QString> MCPToolCatalog::initialTools()
{
    return {"get_status", "listen", "scan", "cleanup", "get_receiving_guide", "search_tools"};
}

QJsonArray MCPToolCatalog::filter(const QJsonArray& tools, const QSet<QString>& names)
{
    QJsonArray result;
    for (const QJsonValue& tool : tools)
    {
        if (names.contains(tool.toObject()["name"].toString())) {
            result.append(tool);
        }
    }
    return result;
}

QJsonObject MCPToolCatalog::search(const QJsonArray& tools, const QJsonObject& arguments)
{
    const QJsonValue queryValue = arguments.value("query");
    if (!queryValue.isString() || queryValue.toString().size() > 256) {
        throw MCPToolError("query must be a string of at most 256 characters");
    }
    const QString query = queryValue.toString().trimmed().toLower();
    const QStringList terms = query.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (terms.isEmpty()) {
        throw MCPToolError("query must contain a tool name or keywords");
    }

    const QJsonValue limitValue = arguments.value("limit");
    const double requestedLimit = limitValue.isUndefined() ? 5 : limitValue.toDouble(-1);
    if (!limitValue.isUndefined() && (!limitValue.isDouble() || requestedLimit < 1
        || requestedLimit > 20 || std::floor(requestedLimit) != requestedLimit)) {
        throw MCPToolError("limit must be an integer from 1 to 20");
    }
    const int limit = static_cast<int>(requestedLimit);

    struct Match { QJsonObject tool; int score; };
    QList<Match> matches;
    for (const QJsonValue& value : tools)
    {
        const QJsonObject tool = value.toObject();
        const QString name = tool["name"].toString().toLower();
        const QString description = tool["description"].toString().toLower();
        int score = name == query ? 1000 : 0;
        for (const QString& term : terms)
        {
            if (name.contains(term)) { score += 10; }
            if (description.contains(term)) { score += 1; }
        }
        if (score > 0) { matches.append({tool, score}); }
    }
    std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
        return a.score > b.score;
    });
    QJsonArray found;
    for (int i = 0; i < qMin(limit, static_cast<int>(matches.size())); ++i) {
        found.append(matches[i].tool);
    }
    return {{"tools", found}, {"total", matches.size()}};
}
