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

#include "primmerge.h"

#include <QHash>
#include <QJsonArray>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QSet>
#include <QStringList>
#include <QVector3D>

#include <cstring>

namespace {

constexpr int ComponentUnsignedByte = 5121;
constexpr int ComponentUnsignedShort = 5123;
constexpr int ComponentUnsignedInt = 5125;
constexpr int ComponentFloat = 5126;
constexpr int ModeTriangles = 4;

int componentCount(const QString &type)
{
    static const QHash<QString, int> counts = {
        {"SCALAR", 1}, {"VEC2", 2}, {"VEC3", 3}, {"VEC4", 4}, {"MAT2", 4}, {"MAT3", 9}, {"MAT4", 16}};
    return counts.value(type, 0);
}

int componentSize(int componentType)
{
    switch (componentType)
    {
    case 5120:
    case 5121:
        return 1;
    case 5122:
    case 5123:
        return 2;
    case 5125:
    case 5126:
        return 4;
    default:
        return 0;
    }
}

// An accessor's elements, tightly packed, whatever the buffer view's stride
bool readElements(const QJsonObject &root, const QVector<QByteArray> &buffers, int accessorIndex,
                  QByteArray *elements, int *elementSize, qint64 *count)
{
    const QJsonObject accessor = root.value("accessors").toArray().at(accessorIndex).toObject();
    if (accessor.isEmpty() || accessor.contains("sparse")) {
        return false;
    }
    const int size = componentSize(accessor.value("componentType").toInt())
        * componentCount(accessor.value("type").toString());
    const QJsonObject view = root.value("bufferViews").toArray().at(accessor.value("bufferView").toInt(-1)).toObject();
    const int bufferIndex = view.value("buffer").toInt(-1);
    if ((size == 0) || view.isEmpty() || (bufferIndex < 0) || (bufferIndex >= buffers.size())) {
        return false;
    }
    const QByteArray &buffer = buffers.at(bufferIndex);
    const qint64 n = static_cast<qint64>(accessor.value("count").toDouble());
    const qint64 stride = static_cast<qint64>(view.value("byteStride").toDouble(size));
    const qint64 offset = static_cast<qint64>(view.value("byteOffset").toDouble() + accessor.value("byteOffset").toDouble());
    if ((n <= 0) || (offset + (n - 1) * stride + size > buffer.size())) {
        return false;
    }
    elements->resize(n * size);
    for (qint64 i = 0; i < n; i++) {
        std::memcpy(elements->data() + i * size, buffer.constData() + offset + i * stride, size);
    }
    *elementSize = size;
    *count = n;
    return true;
}

// Indices as plain numbers, whatever width they were stored at
bool readIndices(const QJsonObject &root, const QVector<QByteArray> &buffers, int accessorIndex,
                 QVector<quint32> *indices)
{
    QByteArray raw;
    int size = 0;
    qint64 n = 0;
    if (!readElements(root, buffers, accessorIndex, &raw, &size, &n)) {
        return false;
    }
    const int componentType = root.value("accessors").toArray().at(accessorIndex)
                                  .toObject().value("componentType").toInt();
    indices->resize(static_cast<int>(n));
    for (qint64 i = 0; i < n; i++)
    {
        switch (componentType)
        {
        case ComponentUnsignedByte:
            (*indices)[static_cast<int>(i)] = static_cast<quint8>(raw.at(static_cast<int>(i)));
            break;
        case ComponentUnsignedShort:
        {
            quint16 v;
            std::memcpy(&v, raw.constData() + i * 2, 2);
            (*indices)[static_cast<int>(i)] = v;
            break;
        }
        case ComponentUnsignedInt:
        {
            quint32 v;
            std::memcpy(&v, raw.constData() + i * 4, 4);
            (*indices)[static_cast<int>(i)] = v;
            break;
        }
        default:
            return false;
        }
    }
    return true;
}

int appendBufferView(QJsonObject *root, QVector<QByteArray> *buffers, const QByteArray &data)
{
    QByteArray &buffer = (*buffers)[0];
    while (buffer.size() % 4) {
        buffer.append('\0');
    }
    const qint64 offset = buffer.size();
    buffer.append(data);

    QJsonArray views = root->value("bufferViews").toArray();
    views.append(QJsonObject{{"buffer", 0}, {"byteOffset", offset}, {"byteLength", data.size()}});
    root->insert("bufferViews", views);
    return static_cast<int>(views.size() - 1);
}

int appendAccessor(QJsonObject *root, int bufferView, int componentType, const QString &type,
                   qint64 count, bool normalized)
{
    QJsonObject accessor{
        {"bufferView", bufferView},
        {"componentType", componentType},
        {"count", count},
        {"type", type}
    };
    if (normalized) {
        accessor.insert("normalized", true);
    }
    QJsonArray accessors = root->value("accessors").toArray();
    accessors.append(accessor);
    root->insert("accessors", accessors);
    return static_cast<int>(accessors.size() - 1);
}

QMatrix4x4 localMatrix(const QJsonObject &node)
{
    QMatrix4x4 m;
    if (node.contains("matrix"))
    {
        const QJsonArray a = node.value("matrix").toArray();
        float values[16];
        for (int i = 0; i < 16; i++) {
            values[i] = static_cast<float>(a.at(i).toDouble());
        }
        return QMatrix4x4(values).transposed();    // glTF is column major, the constructor row major
    }
    const QJsonArray t = node.value("translation").toArray();
    const QJsonArray r = node.value("rotation").toArray();
    const QJsonArray s = node.value("scale").toArray();
    if (t.size() == 3) {
        m.translate(t.at(0).toDouble(), t.at(1).toDouble(), t.at(2).toDouble());
    }
    if (r.size() == 4) {
        m.rotate(QQuaternion(r.at(3).toDouble(), r.at(0).toDouble(), r.at(1).toDouble(), r.at(2).toDouble()));
    }
    if (s.size() == 3) {
        m.scale(s.at(0).toDouble(), s.at(1).toDouble(), s.at(2).toDouble());
    }
    return m;
}

void worldMatrices(const QJsonArray &nodes, int index, const QMatrix4x4 &parent, QHash<int, QMatrix4x4> *world)
{
    if ((index < 0) || (index >= nodes.size()) || world->contains(index)) {
        return;
    }
    const QJsonObject node = nodes.at(index).toObject();
    const QMatrix4x4 m = parent * localMatrix(node);
    world->insert(index, m);
    for (const QJsonValue &child : node.value("children").toArray()) {
        worldMatrices(nodes, child.toInt(-1), m, world);
    }
}

QVector3D linear(const QMatrix4x4 &m, const QVector3D &v)
{
    return QVector3D(m(0, 0) * v.x() + m(0, 1) * v.y() + m(0, 2) * v.z(),
                     m(1, 0) * v.x() + m(1, 1) * v.y() + m(1, 2) * v.z(),
                     m(2, 0) * v.x() + m(2, 1) * v.y() + m(2, 2) * v.z());
}

// Everything an animation drives, and everything below it: baking a transform into the
// vertices of such a node would freeze the movement
QSet<int> animatedSubtrees(const QJsonObject &root)
{
    const QJsonArray nodes = root.value("nodes").toArray();
    QList<int> pending;
    for (const QJsonValue &a : root.value("animations").toArray())
    {
        for (const QJsonValue &c : a.toObject().value("channels").toArray())
        {
            const int node = c.toObject().value("target").toObject().value("node").toInt(-1);
            if (node >= 0) {
                pending.append(node);
            }
        }
    }
    for (const QJsonValue &s : root.value("skins").toArray())
    {
        for (const QJsonValue &j : s.toObject().value("joints").toArray()) {
            pending.append(j.toInt(-1));
        }
    }
    for (int i = 0; i < nodes.size(); i++)
    {
        if (nodes.at(i).toObject().contains("skin")) {
            pending.append(i);
        }
    }

    QSet<int> pinned;
    while (!pending.isEmpty())
    {
        const int i = pending.takeLast();
        if ((i < 0) || (i >= nodes.size()) || pinned.contains(i)) {
            continue;
        }
        pinned.insert(i);
        for (const QJsonValue &c : nodes.at(i).toObject().value("children").toArray()) {
            pending.append(c.toInt(-1));
        }
    }
    return pinned;
}

// What makes two primitives mergeable: the same material drawn the same way, with the same
// attributes stored the same way, so the merged buffers are a plain concatenation
QString primitiveKey(const QJsonObject &root, const QJsonObject &primitive)
{
    const QJsonObject attributes = primitive.value("attributes").toObject();
    QStringList parts;
    parts.append(QString::number(primitive.value("material").toInt(-1)));
    QStringList names = attributes.keys();
    names.sort();
    const QJsonArray accessors = root.value("accessors").toArray();
    for (const QString &name : names)
    {
        const QJsonObject a = accessors.at(attributes.value(name).toInt(-1)).toObject();
        parts.append(QString("%1:%2:%3:%4").arg(name)
                         .arg(a.value("componentType").toInt())
                         .arg(a.value("type").toString())
                         .arg(a.value("normalized").toBool() ? 1 : 0));
    }
    return parts.join('|');
}

struct Source {
    int m_node;
    int m_mesh;
    int m_primitive;
};

} // namespace

QString mergeStaticPrimitives(QJsonObject *root, QVector<QByteArray> *buffers)
{
    if (buffers->isEmpty()) {
        return QString();
    }
    QJsonArray nodes = root->value("nodes").toArray();
    QJsonArray meshes = root->value("meshes").toArray();
    const QJsonArray accessors = root->value("accessors").toArray();
    if (nodes.isEmpty() || meshes.isEmpty()) {
        return QString();
    }

    const int scene = root->value("scene").toInt(0);
    QJsonArray scenes = root->value("scenes").toArray();
    if ((scene < 0) || (scene >= scenes.size())) {
        return QString();
    }
    QJsonObject sceneObject = scenes.at(scene).toObject();
    QJsonArray sceneNodes = sceneObject.value("nodes").toArray();

    QHash<int, QMatrix4x4> world;
    for (const QJsonValue &r : sceneNodes) {
        worldMatrices(nodes, r.toInt(-1), QMatrix4x4(), &world);
    }

    const QSet<int> pinned = animatedSubtrees(*root);

    // A mesh shared by more than one node cannot take one node's transform
    QHash<int, int> meshUsers;
    for (int i = 0; i < nodes.size(); i++)
    {
        const int mesh = nodes.at(i).toObject().value("mesh").toInt(-1);
        if (mesh >= 0) {
            meshUsers[mesh] = meshUsers.value(mesh, 0) + 1;
        }
    }

    QHash<QString, QVector<Source>> groups;
    QStringList order;
    for (int i = 0; i < nodes.size(); i++)
    {
        const QJsonObject node = nodes.at(i).toObject();
        const int mesh = node.value("mesh").toInt(-1);
        if ((mesh < 0) || pinned.contains(i) || !world.contains(i) || (meshUsers.value(mesh) != 1)) {
            continue;
        }
        const QJsonArray primitives = meshes.at(mesh).toObject().value("primitives").toArray();
        for (int p = 0; p < primitives.size(); p++)
        {
            const QJsonObject primitive = primitives.at(p).toObject();
            if ((primitive.value("mode").toInt(ModeTriangles) != ModeTriangles)
                || !primitive.contains("indices")
                || primitive.contains("targets")
                || primitive.contains("extensions")) {
                continue;
            }
            const QString key = primitiveKey(*root, primitive);
            if (!groups.contains(key)) {
                order.append(key);
            }
            groups[key].append(Source{i, mesh, p});
        }
    }

    QJsonArray mergedPrimitives;
    QSet<QPair<int, int>> consumed;       // mesh, primitive
    int mergedFrom = 0;
    for (const QString &key : order)
    {
        const QVector<Source> &members = groups.value(key);
        if (members.size() < 2) {
            continue;
        }
        const QJsonObject first = meshes.at(members.first().m_mesh).toObject()
                                      .value("primitives").toArray()
                                      .at(members.first().m_primitive).toObject();
        const QJsonObject firstAttributes = first.value("attributes").toObject();
        QStringList names = firstAttributes.keys();
        names.sort();

        QHash<QString, QByteArray> merged;
        QVector<quint32> mergedIndices;
        quint32 base = 0;
        bool ok = true;
        for (const Source &s : members)
        {
            const QJsonObject primitive = meshes.at(s.m_mesh).toObject().value("primitives")
                                              .toArray().at(s.m_primitive).toObject();
            const QJsonObject attributes = primitive.value("attributes").toObject();
            const QMatrix4x4 &m = world[s.m_node];
            const QMatrix4x4 normalMatrix = m.inverted().transposed();
            const bool mirrored = m.determinant() < 0.0f;

            qint64 vertexCount = -1;
            QHash<QString, QByteArray> chunk;
            for (const QString &name : names)
            {
                QByteArray raw;
                int size = 0;
                qint64 n = 0;
                if (!readElements(*root, *buffers, attributes.value(name).toInt(-1), &raw, &size, &n)) {
                    ok = false;
                    break;
                }
                if ((vertexCount >= 0) && (n != vertexCount)) {
                    ok = false;
                    break;
                }
                vertexCount = n;
                if ((name == "POSITION") || (name == "NORMAL") || (name == "TANGENT"))
                {
                    if (accessors.at(attributes.value(name).toInt(-1)).toObject()
                            .value("componentType").toInt() != ComponentFloat) {
                        ok = false;
                        break;
                    }
                    const int stride = size / 4;
                    for (qint64 v = 0; v < n; v++)
                    {
                        float f[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                        std::memcpy(f, raw.constData() + v * size, size);
                        QVector3D t(f[0], f[1], f[2]);
                        if (name == "POSITION") {
                            t = m.map(t);
                        } else {
                            t = linear(name == "NORMAL" ? normalMatrix : m, t).normalized();
                        }
                        f[0] = t.x();
                        f[1] = t.y();
                        f[2] = t.z();       // TANGENT keeps its handedness in w
                        std::memcpy(raw.data() + v * size, f, static_cast<size_t>(stride) * 4);
                    }
                }
                chunk.insert(name, raw);
            }
            if (!ok || (vertexCount <= 0)) {
                ok = false;
                break;
            }
            QVector<quint32> indices;
            if (!readIndices(*root, *buffers, primitive.value("indices").toInt(-1), &indices)
                || (indices.size() % 3)) {
                ok = false;
                break;
            }
            if (static_cast<qint64>(base) + vertexCount > 0xffffffffLL) {
                ok = false;
                break;
            }
            for (const QString &name : names) {
                merged[name].append(chunk.value(name));
            }
            // A mirroring transform baked into the vertices turns the triangles inside out,
            // and there is no per-node matrix left for Cesium to pick the cull face from
            for (int t = 0; t + 2 < indices.size(); t += 3)
            {
                mergedIndices.append(indices.at(t) + base);
                mergedIndices.append(indices.at(mirrored ? t + 2 : t + 1) + base);
                mergedIndices.append(indices.at(mirrored ? t + 1 : t + 2) + base);
            }
            base += static_cast<quint32>(vertexCount);
        }
        if (!ok) {
            continue;
        }

        QJsonObject attributes;
        for (const QString &name : names)
        {
            const QJsonObject like = accessors.at(firstAttributes.value(name).toInt(-1)).toObject();
            const QByteArray &data = merged.value(name);
            const int view = appendBufferView(root, buffers, data);
            const int accessor = appendAccessor(root, view, like.value("componentType").toInt(),
                                                like.value("type").toString(), base,
                                                like.value("normalized").toBool());
            if (name == "POSITION")
            {
                // The spec requires POSITION bounds, and Cesium builds the model's bounding
                // volume from them
                float lo[3] = {0.0f, 0.0f, 0.0f};
                float hi[3] = {0.0f, 0.0f, 0.0f};
                for (quint32 v = 0; v < base; v++)
                {
                    float f[3];
                    std::memcpy(f, data.constData() + static_cast<qint64>(v) * 12, 12);
                    for (int c = 0; c < 3; c++)
                    {
                        lo[c] = (v == 0) ? f[c] : qMin(lo[c], f[c]);
                        hi[c] = (v == 0) ? f[c] : qMax(hi[c], f[c]);
                    }
                }
                QJsonArray accessorArray = root->value("accessors").toArray();
                QJsonObject positions = accessorArray.at(accessor).toObject();
                positions.insert("min", QJsonArray{lo[0], lo[1], lo[2]});
                positions.insert("max", QJsonArray{hi[0], hi[1], hi[2]});
                accessorArray.replace(accessor, positions);
                root->insert("accessors", accessorArray);
            }
            attributes.insert(name, accessor);
        }

        QByteArray indexData;
        const bool wide = base > 0xffff;
        indexData.resize(mergedIndices.size() * (wide ? 4 : 2));
        for (int i = 0; i < mergedIndices.size(); i++)
        {
            if (wide)
            {
                const quint32 v = mergedIndices.at(i);
                std::memcpy(indexData.data() + static_cast<qint64>(i) * 4, &v, 4);
            }
            else
            {
                const quint16 v = static_cast<quint16>(mergedIndices.at(i));
                std::memcpy(indexData.data() + static_cast<qint64>(i) * 2, &v, 2);
            }
        }
        const int indexView = appendBufferView(root, buffers, indexData);
        const int indexAccessor = appendAccessor(root, indexView,
                                                 wide ? ComponentUnsignedInt : ComponentUnsignedShort,
                                                 "SCALAR", mergedIndices.size(), false);

        QJsonObject primitive{
            {"attributes", attributes},
            {"indices", indexAccessor},
            {"mode", ModeTriangles}
        };
        if (first.contains("material")) {
            primitive.insert("material", first.value("material").toInt());
        }
        mergedPrimitives.append(primitive);
        mergedFrom += members.size();
        for (const Source &s : members) {
            consumed.insert(qMakePair(s.m_mesh, s.m_primitive));
        }
    }

    if (mergedPrimitives.isEmpty()) {
        return QString();
    }

    // Take the merged primitives out, and the mesh off any node left with nothing to draw.
    // The nodes stay for stripRedundantNodes() to clear, which also keeps any children in
    // place under the transforms they expect
    for (int mesh = 0; mesh < meshes.size(); mesh++)
    {
        QJsonObject meshObject = meshes.at(mesh).toObject();
        const QJsonArray primitives = meshObject.value("primitives").toArray();
        QJsonArray kept;
        for (int p = 0; p < primitives.size(); p++)
        {
            if (!consumed.contains(qMakePair(mesh, p))) {
                kept.append(primitives.at(p));
            }
        }
        if (kept.size() != primitives.size())
        {
            meshObject.insert("primitives", kept);
            meshes.replace(mesh, meshObject);
        }
    }
    QSet<int> emptied;
    for (int i = 0; i < nodes.size(); i++)
    {
        QJsonObject node = nodes.at(i).toObject();
        const int mesh = node.value("mesh").toInt(-1);
        if ((mesh >= 0) && meshes.at(mesh).toObject().value("primitives").toArray().isEmpty())
        {
            node.remove("mesh");
            nodes.replace(i, node);
            emptied.insert(i);
        }
    }

    // The merged geometry is in the scene's own space, so it hangs off the scene with no
    // transform of its own
    meshes.append(QJsonObject{{"name", "MergedStatic"}, {"primitives", mergedPrimitives}});
    nodes.append(QJsonObject{{"name", "MergedStatic"}, {"mesh", meshes.size() - 1}});
    sceneNodes.append(nodes.size() - 1);
    sceneObject.insert("nodes", sceneNodes);
    scenes.replace(scene, sceneObject);

    root->insert("nodes", nodes);
    root->insert("meshes", meshes);
    root->insert("scenes", scenes);

    QJsonArray bufferArray = root->value("buffers").toArray();
    if (!bufferArray.isEmpty())
    {
        QJsonObject buffer = bufferArray.at(0).toObject();
        buffer.insert("byteLength", buffers->at(0).size());
        bufferArray.replace(0, buffer);
        root->insert("buffers", bufferArray);
    }

    return QString("%1 primitives into %2, emptying %3 nodes")
        .arg(mergedFrom).arg(mergedPrimitives.size()).arg(emptied.size());
}
