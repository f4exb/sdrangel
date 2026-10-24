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

#include "portholes.h"

#include <QHash>
#include <QJsonArray>
#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QSet>
#include <QVector3D>

#include <cmath>
#include <cstring>
#include <limits>

namespace {

// How lopsided the windows have to be, as the offset of their extent's middle from the
// centre plane, before one side is taken to be inset. The symmetric models are within
// 0.01 m; the least inset of the faulty ones, the 767, is 0.17 m
constexpr double InsetThreshold = 0.1;

constexpr int ComponentUnsignedByte = 5121;
constexpr int ComponentUnsignedShort = 5123;
constexpr int ComponentUnsignedInt = 5125;
constexpr int ComponentFloat = 5126;

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

// A copy of an accessor's description with new data, appended to buffers[0]
int appendAccessor(QJsonObject *root, QVector<QByteArray> *buffers, const QJsonObject &like,
                   const QByteArray &data, qint64 count, int componentType = 0)
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

    QJsonObject accessor{
        {"bufferView", views.size() - 1},
        {"componentType", componentType ? componentType : like.value("componentType").toInt()},
        {"count", count},
        {"type", like.value("type").toString()}
    };
    if (like.value("normalized").toBool()) {
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

QVector3D vec3(const QByteArray &data, qint64 i)
{
    float v[3];
    std::memcpy(v, data.constData() + i * 12, 12);
    return QVector3D(v[0], v[1], v[2]);
}

void putVec3(QByteArray *data, qint64 i, const QVector3D &v)
{
    const float f[3] = {v.x(), v.y(), v.z()};
    std::memcpy(data->data() + i * 12, f, 12);
}

QVector3D linear(const QMatrix4x4 &m, const QVector3D &v)
{
    return QVector3D(m(0, 0) * v.x() + m(0, 1) * v.y() + m(0, 2) * v.z(),
                     m(1, 0) * v.x() + m(1, 1) * v.y() + m(1, 2) * v.z(),
                     m(2, 0) * v.x() + m(2, 1) * v.y() + m(2, 2) * v.z());
}

struct Use {
    int m_node;
    int m_mesh;
    int m_primitive;
};

} // namespace

QString mirrorInsetPortholes(QJsonObject *root, QVector<QByteArray> *buffers)
{
    if (buffers->isEmpty()) {
        return QString();
    }
    QSet<int> portholes;
    const QJsonArray materials = root->value("materials").toArray();
    for (qsizetype i = 0; i < materials.size(); i++)
    {
        const QString code = materials.at(i).toObject().value("extras").toObject()
            .value("ASOBO_material_code").toString();
        if (code.compare(QStringLiteral("Porthole"), Qt::CaseInsensitive) == 0) {
            portholes.insert(static_cast<int>(i));
        }
    }
    if (portholes.isEmpty()) {
        return QString();
    }

    // Where every node is, at rest
    const QJsonArray nodes = root->value("nodes").toArray();
    QHash<int, QMatrix4x4> world;
    const QJsonArray scenes = root->value("scenes").toArray();
    const QJsonObject scene = scenes.at(root->value("scene").toInt(0)).toObject();
    for (const QJsonValue &rootNode : scene.value("nodes").toArray()) {
        worldMatrices(nodes, rootNode.toInt(-1), QMatrix4x4(), &world);
    }

    // The window primitives, and how many nodes draw each mesh - one drawn by several
    // nodes can't be changed for one without changing it for all
    QJsonArray meshes = root->value("meshes").toArray();
    QHash<int, int> meshUses;
    for (qsizetype i = 0; i < nodes.size(); i++)
    {
        const QJsonObject node = nodes.at(i).toObject();
        if (node.contains("mesh") && world.contains(static_cast<int>(i))) {
            meshUses[node.value("mesh").toInt()]++;
        }
    }
    QVector<Use> uses;
    for (qsizetype i = 0; i < nodes.size(); i++)
    {
        const QJsonObject node = nodes.at(i).toObject();
        const int mesh = node.value("mesh").toInt(-1);
        if ((mesh < 0) || (mesh >= meshes.size()) || !world.contains(static_cast<int>(i)) || node.contains("skin")) {
            continue;
        }
        const QJsonArray primitives = meshes.at(mesh).toObject().value("primitives").toArray();
        for (qsizetype p = 0; p < primitives.size(); p++)
        {
            const QJsonObject primitive = primitives.at(p).toObject();
            if (portholes.contains(primitive.value("material").toInt(-1))) {
                uses.append({static_cast<int>(i), mesh, static_cast<int>(p)});
            }
        }
    }

    // How far across the windows reach on each side of the fuselage's centre plane
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    for (const Use &use : uses)
    {
        const QJsonObject primitive = meshes.at(use.m_mesh).toObject().value("primitives").toArray()
            .at(use.m_primitive).toObject();
        QByteArray positions;
        int size = 0;
        qint64 count = 0;
        if (!readElements(*root, *buffers, primitive.value("attributes").toObject().value("POSITION").toInt(-1),
                          &positions, &size, &count) || (size != 12)) {
            continue;
        }
        const QMatrix4x4 &m = world[use.m_node];
        for (qint64 v = 0; v < count; v++)
        {
            const double x = m.map(vec3(positions, v)).x();
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
        }
    }
    if (minX > maxX) {
        return QString();
    }
    const double offset = (minX + maxX) / 2.0;
    if (std::abs(offset) < InsetThreshold) {
        return QString();
    }
    // The good side reaches further out
    const double good = (offset > 0.0) ? 1.0 : -1.0;

    int kept = 0;
    int dropped = 0;
    int mirrored = 0;
    int skipped = 0;
    for (const Use &use : uses)
    {
        QJsonObject mesh = meshes.at(use.m_mesh).toObject();
        QJsonArray primitives = mesh.value("primitives").toArray();
        QJsonObject primitive = primitives.at(use.m_primitive).toObject();
        const QJsonObject attributes = primitive.value("attributes").toObject();
        if ((meshUses.value(use.m_mesh) != 1) || (primitive.value("mode").toInt(4) != 4)
            || primitive.contains("targets") || attributes.contains("JOINTS_0"))
        {
            skipped++;
            continue;
        }
        const QJsonArray accessors = root->value("accessors").toArray();

        // Every attribute, so the mirrored vertices carry the same texture coordinates
        // and colours; positions, normals and tangents must be float to be reflected
        QHash<QString, QByteArray> data;
        QHash<QString, int> sizes;
        qint64 vertexCount = -1;
        bool usable = true;
        for (auto it = attributes.constBegin(); it != attributes.constEnd(); ++it)
        {
            QByteArray elements;
            int size = 0;
            qint64 count = 0;
            const int accessorIndex = it.value().toInt(-1);
            const QJsonObject accessor = accessors.at(accessorIndex).toObject();
            const bool geometric = (it.key() == "POSITION") || (it.key() == "NORMAL") || (it.key() == "TANGENT");
            if (!readElements(*root, *buffers, accessorIndex, &elements, &size, &count)
                || ((vertexCount >= 0) && (count != vertexCount))
                || (geometric && (accessor.value("componentType").toInt() != ComponentFloat)))
            {
                usable = false;
                break;
            }
            vertexCount = count;
            data.insert(it.key(), elements);
            sizes.insert(it.key(), size);
        }
        if (!usable || !data.contains("POSITION"))
        {
            skipped++;
            continue;
        }

        // Triangles, as vertex indices
        QVector<quint32> indices;
        if (primitive.contains("indices"))
        {
            QByteArray elements;
            int size = 0;
            qint64 count = 0;
            const QJsonObject accessor = accessors.at(primitive.value("indices").toInt()).toObject();
            const int type = accessor.value("componentType").toInt();
            if (!readElements(*root, *buffers, primitive.value("indices").toInt(), &elements, &size, &count))
            {
                skipped++;
                continue;
            }
            indices.resize(count);
            for (qint64 i = 0; i < count; i++)
            {
                if (type == ComponentUnsignedByte) {
                    indices[i] = static_cast<quint8>(elements[i]);
                } else if (type == ComponentUnsignedShort) {
                    quint16 v;
                    std::memcpy(&v, elements.constData() + i * 2, 2);
                    indices[i] = v;
                } else {
                    std::memcpy(&indices[i], elements.constData() + i * 4, 4);
                }
            }
        }
        else
        {
            indices.resize(vertexCount);
            for (qint64 i = 0; i < vertexCount; i++) {
                indices[i] = static_cast<quint32>(i);
            }
        }

        const QMatrix4x4 &m = world[use.m_node];
        bool invertible = false;
        const QMatrix4x4 inverse = m.inverted(&invertible);
        if (!invertible)
        {
            skipped++;
            continue;
        }
        const QMatrix3x3 normalMatrix = m.normalMatrix();  // Inverse transpose
        QMatrix4x4 normalMatrix4;
        const QMatrix4x4 normalInverse4 = m.transposed();   // Its inverse: the transpose of the linear part
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                normalMatrix4(r, c) = normalMatrix(r, c);
            }
        }
        const QByteArray &positions = data["POSITION"];

        // Keep the good side's triangles, and add each again reflected through the centre
        // plane, wound the other way so that it still faces out
        QVector<quint32> newIndices;
        QHash<quint32, quint32> mirrorOf;   // Vertex to its reflection, among the new vertices
        QVector<quint32> mirroredVertices;
        for (qsizetype t = 0; t + 2 < indices.size(); t += 3)
        {
            const quint32 a = indices[t], b = indices[t + 1], c = indices[t + 2];
            if ((a >= vertexCount) || (b >= vertexCount) || (c >= vertexCount)) {
                continue;
            }
            const double centre = (m.map(vec3(positions, a)).x() + m.map(vec3(positions, b)).x()
                                   + m.map(vec3(positions, c)).x()) / 3.0;
            if (centre * good <= 0.0)
            {
                dropped++;
                continue;
            }
            newIndices << a << b << c;
            kept++;
            quint32 reflected[3];
            const quint32 corners[3] = {a, b, c};
            for (int k = 0; k < 3; k++)
            {
                auto found = mirrorOf.constFind(corners[k]);
                if (found == mirrorOf.constEnd())
                {
                    const quint32 added = static_cast<quint32>(vertexCount + mirroredVertices.size());
                    mirrorOf.insert(corners[k], added);
                    mirroredVertices.append(corners[k]);
                    reflected[k] = added;
                }
                else
                {
                    reflected[k] = found.value();
                }
            }
            newIndices << reflected[0] << reflected[2] << reflected[1];
            mirrored++;
        }

        // The vertex data: the originals, then the reflections
        QJsonObject newAttributes;
        const qint64 newCount = vertexCount + mirroredVertices.size();
        for (auto it = data.constBegin(); it != data.constEnd(); ++it)
        {
            const int size = sizes[it.key()];
            QByteArray out = it.value();
            out.resize(newCount * size);
            for (qsizetype i = 0; i < mirroredVertices.size(); i++)
            {
                const qint64 from = mirroredVertices[i];
                const qint64 to = vertexCount + i;
                std::memcpy(out.data() + to * size, it.value().constData() + from * size, size);
                if (it.key() == "POSITION")
                {
                    QVector3D p = m.map(vec3(it.value(), from));
                    p.setX(-p.x());
                    putVec3(&out, to, inverse.map(p));
                }
                else if (it.key() == "NORMAL")
                {
                    QVector3D n = linear(normalMatrix4, vec3(it.value(), from));
                    n.setX(-n.x());
                    putVec3(&out, to, linear(normalInverse4, n).normalized());
                }
                else if (it.key() == "TANGENT")
                {
                    float t[4];
                    std::memcpy(t, it.value().constData() + from * 16, 16);
                    QVector3D d = linear(m, QVector3D(t[0], t[1], t[2]));
                    d.setX(-d.x());
                    d = linear(inverse, d).normalized();
                    const float f[4] = {d.x(), d.y(), d.z(), -t[3]};  // A reflection swaps handedness
                    std::memcpy(out.data() + to * 16, f, 16);
                }
            }
            const QJsonObject like = accessors.at(attributes.value(it.key()).toInt()).toObject();
            const int accessorIndex = appendAccessor(root, buffers, like, out, newCount);
            if (it.key() == "POSITION")
            {
                // Required for POSITION
                QVector3D low(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
                QVector3D high = -low;
                for (qint64 v = 0; v < newCount; v++)
                {
                    const QVector3D p = vec3(out, v);
                    low = QVector3D(std::min(low.x(), p.x()), std::min(low.y(), p.y()), std::min(low.z(), p.z()));
                    high = QVector3D(std::max(high.x(), p.x()), std::max(high.y(), p.y()), std::max(high.z(), p.z()));
                }
                QJsonArray all = root->value("accessors").toArray();
                QJsonObject accessor = all.at(accessorIndex).toObject();
                accessor.insert("min", QJsonArray{low.x(), low.y(), low.z()});
                accessor.insert("max", QJsonArray{high.x(), high.y(), high.z()});
                all[accessorIndex] = accessor;
                root->insert("accessors", all);
            }
            newAttributes.insert(it.key(), accessorIndex);
        }
        QByteArray indexData(newIndices.size() * 4, '\0');
        std::memcpy(indexData.data(), newIndices.constData(), indexData.size());
        const QJsonObject indexLike{{"type", "SCALAR"}, {"componentType", ComponentUnsignedInt}};
        primitive.insert("attributes", newAttributes);
        primitive.insert("indices", appendAccessor(root, buffers, indexLike, indexData, newIndices.size()));
        primitives[use.m_primitive] = primitive;
        mesh.insert("primitives", primitives);
        meshes[use.m_mesh] = mesh;
    }
    root->insert("meshes", meshes);

    return QStringLiteral("windows on the %1 side were inset (offset %2 m): %3 triangles replaced by %4 "
                          "reflected from the %5 side%6")
        .arg(good > 0.0 ? "-x" : "+x")
        .arg(offset, 0, 'f', 2)
        .arg(dropped)
        .arg(mirrored)
        .arg(good > 0.0 ? "+x" : "-x")
        .arg(skipped ? QStringLiteral(", %1 primitives left alone").arg(skipped) : QString());
}
