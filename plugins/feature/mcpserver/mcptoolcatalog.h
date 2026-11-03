// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef INCLUDE_FEATURE_MCPTOOLCATALOG_H_
#define INCLUDE_FEATURE_MCPTOOLCATALOG_H_

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QString>

// Discovery operates on the real registry's serialized definitions, retaining schemas and hints.
namespace MCPToolCatalog {
    QSet<QString> initialTools();
    QJsonArray filter(const QJsonArray& tools, const QSet<QString>& names);
    QJsonObject search(const QJsonArray& tools, const QJsonObject& arguments);
}

#endif // INCLUDE_FEATURE_MCPTOOLCATALOG_H_
