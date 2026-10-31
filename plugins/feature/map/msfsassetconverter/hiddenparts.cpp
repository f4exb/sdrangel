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

#include "hiddenparts.h"

#include <QFile>
#include <QJsonArray>
#include <QRegularExpression>
#include <QVector>
#include <QXmlStreamReader>

namespace {

// A visibility code that is a constant false: "{ 0 }", "0"
bool neverTrue(const QString &code)
{
    QString compact = code;
    compact.remove(QRegularExpression(QStringLiteral("[\\s{}]")));
    return compact == QStringLiteral("0");
}

void dropMeshes(QJsonArray *nodes, int index, int *dropped, QSet<int> *visited)
{
    if ((index < 0) || (index >= nodes->size()) || visited->contains(index)) {
        return;
    }
    visited->insert(index);
    QJsonObject node = nodes->at(index).toObject();
    if (node.contains("mesh"))
    {
        node.remove("mesh");
        node.remove("skin");
        (*nodes)[index] = node;
        (*dropped)++;
    }
    for (const QJsonValue &child : node.value("children").toArray()) {
        dropMeshes(nodes, child.toInt(-1), dropped, visited);
    }
}

} // namespace

QSet<QString> readMsfsNeverVisibleNodes(const QString &xmlFile)
{
    QSet<QString> nodes;
    QFile file(xmlFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return nodes;
    }
    // <Component ID="GROUND_GPUPIPE" Node="GROUND_GPUPIPE"><Visibility><Parameter><Code>{ 0 }
    // or a visibility template's VISIBILITY_CODE, within the component
    struct Component {
        QString m_node;
        QString m_code;
        bool m_hasCode = false;
    };
    QVector<Component> components;
    QStringList elements;
    QXmlStreamReader xml(&file);
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement())
        {
            elements.append(xml.name().toString());
            if (xml.name().compare(QStringLiteral("Component"), Qt::CaseInsensitive) == 0)
            {
                const auto attributes = xml.attributes();
                QString node = attributes.value(QStringLiteral("Node")).toString();
                if (node.isEmpty()) {
                    node = attributes.value(QStringLiteral("ID")).toString();
                }
                components.append({node, {}, false});
            }
        }
        else if (xml.isCharacters() && !components.isEmpty() && (elements.size() >= 2))
        {
            const QString &element = elements.last();
            const bool code = (element.compare(QStringLiteral("Code"), Qt::CaseInsensitive) == 0)
                && elements.contains(QStringLiteral("Visibility"), Qt::CaseInsensitive);
            if (code || (element.compare(QStringLiteral("VISIBILITY_CODE"), Qt::CaseInsensitive) == 0))
            {
                components.last().m_code += xml.text().toString();
                components.last().m_hasCode = true;
            }
        }
        else if (xml.isEndElement() && !elements.isEmpty())
        {
            const QString name = elements.takeLast();
            if ((name.compare(QStringLiteral("Component"), Qt::CaseInsensitive) == 0) && !components.isEmpty())
            {
                const Component component = components.takeLast();
                if (component.m_hasCode && !component.m_node.isEmpty() && neverTrue(component.m_code)) {
                    nodes.insert(component.m_node.toLower());
                }
            }
        }
    }
    return nodes;
}

QString dropHiddenParts(QJsonObject *root, const QSet<QString> &neverVisible)
{
    QJsonArray nodes = root->value("nodes").toArray();
    QJsonArray meshes = root->value("meshes").toArray();
    const QJsonArray materials = root->value("materials").toArray();

    QSet<int> frost;
    for (qsizetype i = 0; i < materials.size(); i++)
    {
        const QString code = materials.at(i).toObject().value("extras").toObject()
            .value("ASOBO_material_code").toString();
        if (code.compare(QStringLiteral("GeoDecalFrosted"), Qt::CaseInsensitive) == 0) {
            frost.insert(static_cast<int>(i));
        }
    }

    // Frost primitives. A mesh that would be left with none - glTF needs at least one -
    // is instead no longer drawn by any node
    int frostPrimitives = 0;
    QSet<int> emptied;
    for (qsizetype m = 0; m < meshes.size() && !frost.isEmpty(); m++)
    {
        QJsonObject mesh = meshes.at(m).toObject();
        const QJsonArray primitives = mesh.value("primitives").toArray();
        QJsonArray kept;
        for (const QJsonValue &primitive : primitives)
        {
            if (frost.contains(primitive.toObject().value("material").toInt(-1))) {
                frostPrimitives++;
            } else {
                kept.append(primitive);
            }
        }
        if (kept.size() == primitives.size()) {
            continue;
        }
        if (kept.isEmpty())
        {
            emptied.insert(static_cast<int>(m));
        }
        else
        {
            mesh.insert("primitives", kept);
            meshes[m] = mesh;
        }
    }

    int hiddenMeshes = 0;
    int emptiedNodes = 0;
    QSet<int> visited;
    for (qsizetype i = 0; i < nodes.size(); i++)
    {
        const QJsonObject node = nodes.at(i).toObject();
        if (neverVisible.contains(node.value("name").toString().toLower())) {
            dropMeshes(&nodes, static_cast<int>(i), &hiddenMeshes, &visited);
        }
    }
    for (qsizetype i = 0; i < nodes.size(); i++)
    {
        QJsonObject node = nodes.at(i).toObject();
        if (node.contains("mesh") && emptied.contains(node.value("mesh").toInt()))
        {
            node.remove("mesh");
            node.remove("skin");
            nodes[i] = node;
            emptiedNodes++;
        }
    }
    root->insert("nodes", nodes);
    root->insert("meshes", meshes);

    if ((hiddenMeshes == 0) && (frostPrimitives == 0)) {
        return QString();
    }
    return QStringLiteral("%1 never-visible part%2 and %3 frost primitive%4 removed")
        .arg(hiddenMeshes)
        .arg(hiddenMeshes == 1 ? QString() : QStringLiteral("s"))
        .arg(frostPrimitives)
        .arg(frostPrimitives == 1 ? QString() : QStringLiteral("s"));
}

QString dropNodeMeshes(QJsonObject *root, const QSet<int> &nodeIndices)
{
    if (nodeIndices.isEmpty()) {
        return QString();
    }
    QJsonArray nodes = root->value("nodes").toArray();
    int meshNodes = 0;
    for (const QJsonValue &node : nodes)
    {
        if (node.toObject().contains("mesh")) {
            meshNodes++;
        }
    }
    int dropped = 0;
    // visited makes this independent of the order the set comes out in: where one of these
    // nodes is an ancestor of another, whichever is reached first takes the subtree with it
    QSet<int> visited;
    for (const int index : nodeIndices) {
        dropMeshes(&nodes, index, &dropped, &visited);
    }
    if (dropped == 0) {
        return QString();
    }
    // Whatever the animations say, a model is not reduced to nothing: one with no meshes
    // left loads, draws nothing, and has no bounding sphere to judge its size by
    if (dropped >= meshNodes) {
        return QStringLiteral("WARNING: none dropped - the %1 animated node%2 would have taken "
                              "all %3 meshes")
            .arg(nodeIndices.size())
            .arg(nodeIndices.size() == 1 ? QString() : QStringLiteral("s"))
            .arg(meshNodes);
    }
    root->insert(QStringLiteral("nodes"), nodes);
    return QStringLiteral("%1 mesh%2 off %3 animated node%4")
        .arg(dropped)
        .arg(dropped == 1 ? QString() : QStringLiteral("es"))
        .arg(nodeIndices.size())
        .arg(nodeIndices.size() == 1 ? QString() : QStringLiteral("s"));
}
