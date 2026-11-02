///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "attribstrip.h"

#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

enum ComponentType {
    ComponentByte = 5120,
    ComponentUnsignedByte = 5121,
    ComponentShort = 5122,
    ComponentUnsignedShort = 5123,
    ComponentUnsignedInt = 5125,
    ComponentFloat = 5126
};

int componentSize(int componentType)
{
    switch (componentType)
    {
    case ComponentByte:
    case ComponentUnsignedByte:
        return 1;
    case ComponentShort:
    case ComponentUnsignedShort:
        return 2;
    case ComponentUnsignedInt:
    case ComponentFloat:
        return 4;
    default:
        return 0;
    }
}

int componentCount(const QString &type)
{
    if (type == QStringLiteral("SCALAR")) {
        return 1;
    } else if (type == QStringLiteral("VEC2")) {
        return 2;
    } else if (type == QStringLiteral("VEC3")) {
        return 3;
    } else if (type == QStringLiteral("VEC4")) {
        return 4;
    }
    return 0;
}

// Every TEXCOORD set any material reads. A textureInfo is any object carrying an "index",
// and its "texCoord" defaults to 0 when absent. KHR_texture_transform may override the set
// a texture samples, so its own texCoord counts too - walking the material recursively
// picks both up wherever an extension has put them.
void collectTexCoords(const QJsonValue &value, QSet<int> *used)
{
    if (value.isObject())
    {
        const QJsonObject object = value.toObject();

        if (object.contains(QStringLiteral("index"))) {
            used->insert(object.value(QStringLiteral("texCoord")).toInt(0));
        }
        for (auto it = object.begin(); it != object.end(); ++it) {
            collectTexCoords(it.value(), used);
        }
    }
    else if (value.isArray())
    {
        const QJsonArray array = value.toArray();

        for (const QJsonValue &entry : array) {
            collectTexCoords(entry, used);
        }
    }
}

// True when anything in this material needs a tangent frame. That is a normal map: the
// material's own normalTexture, or one an extension has added - KHR_materials_clearcoat has
// clearcoatNormalTexture - so the material is walked rather than only its known keys.
// KHR_materials_anisotropy counts whether or not it carries a texture, because the
// anisotropy direction is itself expressed in tangent space.
bool needsTangents(const QJsonValue &value)
{
    if (value.isObject())
    {
        const QJsonObject object = value.toObject();

        for (auto it = object.begin(); it != object.end(); ++it)
        {
            const QString key = it.key();

            if (key == QStringLiteral("KHR_materials_anisotropy")) {
                return true;
            }
            // normalTexture, clearcoatNormalTexture, and anything else named the same way,
            // but only where it really is a textureInfo pointing at a texture
            if (key.endsWith(QStringLiteral("ormalTexture"))
                && it.value().isObject()
                && it.value().toObject().contains(QStringLiteral("index"))) {
                return true;
            }
            if (needsTangents(it.value())) {
                return true;
            }
        }
    }
    else if (value.isArray())
    {
        const QJsonArray array = value.toArray();

        for (const QJsonValue &entry : array)
        {
            if (needsTangents(entry)) {
                return true;
            }
        }
    }
    return false;
}

// True when every element of the accessor is opaque white, so dropping it cannot change
// what is drawn. Read from the buffer rather than taken from the accessor's min/max, which
// these models do not carry. Anything that cannot be read with certainty answers false, so
// an attribute is kept whenever there is doubt.
bool isWhite(const QJsonObject &root, const QVector<QByteArray> &buffers, int accessorIndex)
{
    const QJsonArray accessors = root.value(QStringLiteral("accessors")).toArray();

    if ((accessorIndex < 0) || (accessorIndex >= accessors.size())) {
        return false;
    }
    const QJsonObject accessor = accessors.at(accessorIndex).toObject();
    // A sparse accessor's values live elsewhere, so the dense range below is not all of it
    if (accessor.contains(QStringLiteral("sparse"))) {
        return false;
    }
    const int componentType = accessor.value(QStringLiteral("componentType")).toInt();
    const int size = componentSize(componentType);
    const int components = componentCount(accessor.value(QStringLiteral("type")).toString());
    const int element = size * components;

    if ((element == 0) || (components < 3)) {
        return false;
    }
    const QJsonArray views = root.value(QStringLiteral("bufferViews")).toArray();
    const int viewIndex = accessor.value(QStringLiteral("bufferView")).toInt(-1);

    if ((viewIndex < 0) || (viewIndex >= views.size())) {
        return false;
    }
    const QJsonObject view = views.at(viewIndex).toObject();
    const int bufferIndex = view.value(QStringLiteral("buffer")).toInt(-1);

    if ((bufferIndex < 0) || (bufferIndex >= buffers.size())) {
        return false;
    }
    const QByteArray &buffer = buffers.at(bufferIndex);
    const qint64 count = static_cast<qint64>(accessor.value(QStringLiteral("count")).toDouble());
    const qint64 stride = static_cast<qint64>(
        view.value(QStringLiteral("byteStride")).toDouble(element));
    const qint64 offset = static_cast<qint64>(
        view.value(QStringLiteral("byteOffset")).toDouble()
        + accessor.value(QStringLiteral("byteOffset")).toDouble());

    if ((count <= 0) || (stride < element)
        || (offset < 0) || (offset + (count - 1) * stride + element > buffer.size())) {
        return false;
    }
    const bool normalized = accessor.value(QStringLiteral("normalized")).toBool();
    const uchar *bytes = reinterpret_cast<const uchar *>(buffer.constData());

    for (qint64 i = 0; i < count; i++)
    {
        const uchar *element0 = bytes + offset + i * stride;

        // Every component, alpha included. glTF multiplies COLOR_n into the base colour
        // with its alpha, so a VEC4 element that is white in RGB but not opaque still
        // changes the result; a VEC3 accessor simply has no fourth component to check
        for (int c = 0; c < components; c++)
        {
            const uchar *p = element0 + c * size;

            switch (componentType)
            {
            case ComponentFloat:
            {
                float v;
                memcpy(&v, p, sizeof(v));
                if (!std::isfinite(v) || (std::fabs(v - 1.0f) > 1e-4f)) {
                    return false;
                }
                break;
            }
            case ComponentUnsignedByte:
                if (!normalized || (*p != 0xFF)) {
                    return false;
                }
                break;
            case ComponentUnsignedShort:
            {
                quint16 v;
                memcpy(&v, p, sizeof(v));
                if (!normalized || (v != 0xFFFF)) {
                    return false;
                }
                break;
            }
            default:
                // Signed and integer colours are rare and not worth guessing at
                return false;
            }
        }
    }
    return true;
}

} // namespace

QString stripUnusedAttributes(QJsonObject *root, const QVector<QByteArray> &buffers)
{
    QJsonArray meshes = root->value(QStringLiteral("meshes")).toArray();

    if (meshes.isEmpty()) {
        return QString();
    }

    QSet<int> usedTexCoords;
    collectTexCoords(root->value(QStringLiteral("materials")), &usedTexCoords);

    // An accessor is shared between primitives, so each one is judged once and the answer
    // reused - reading a whole colour buffer per primitive would be the slowest part of
    // the converter on a model with hundreds of them
    QHash<int, bool> whiteCache;
    // Materials are shared between primitives too, and this walks a whole material subtree
    QHash<int, bool> tangentCache;
    const QJsonArray materials = root->value(QStringLiteral("materials")).toArray();
    int droppedTexCoords = 0;
    int droppedColours = 0;
    int keptColours = 0;
    int droppedTangents = 0;
    int keptTangents = 0;
    QSet<int> droppedSets;

    for (qsizetype m = 0; m < meshes.size(); m++)
    {
        QJsonObject mesh = meshes.at(m).toObject();
        QJsonArray primitives = mesh.value(QStringLiteral("primitives")).toArray();
        bool meshChanged = false;

        for (qsizetype p = 0; p < primitives.size(); p++)
        {
            QJsonObject primitive = primitives.at(p).toObject();
            QJsonObject attributes = primitive.value(QStringLiteral("attributes")).toObject();
            bool changed = false;

            // A primitive with no material is drawn with glTF's default, which has no
            // textures at all; one whose index does not resolve is malformed, and keeps
            // everything it has
            const int materialIndex = primitive.value(QStringLiteral("material")).toInt(-1);
            bool materialNeedsTangents;

            if (materialIndex < 0) {
                materialNeedsTangents = false;
            } else if (materialIndex >= materials.size()) {
                materialNeedsTangents = true;
            } else {
                if (!tangentCache.contains(materialIndex)) {
                    tangentCache.insert(materialIndex,
                                        needsTangents(materials.at(materialIndex)));
                }
                materialNeedsTangents = tangentCache.value(materialIndex);
            }

            const QStringList names = attributes.keys();
            for (const QString &name : names)
            {
                const int accessorIndex = attributes.value(name).toInt(-1);

                if (name.startsWith(QStringLiteral("TEXCOORD_")))
                {
                    bool ok = false;
                    const int set = name.mid(9).toInt(&ok);

                    if (ok && !usedTexCoords.contains(set))
                    {
                        attributes.remove(name);
                        droppedTexCoords++;
                        droppedSets.insert(set);
                        changed = true;
                    }
                }
                else if (name == QStringLiteral("TANGENT"))
                {
                    if (materialNeedsTangents)
                    {
                        keptTangents++;
                    }
                    else
                    {
                        attributes.remove(name);
                        droppedTangents++;
                        changed = true;
                    }
                }
                else if (name.startsWith(QStringLiteral("COLOR_")))
                {
                    if (!whiteCache.contains(accessorIndex)) {
                        whiteCache.insert(accessorIndex, isWhite(*root, buffers, accessorIndex));
                    }
                    if (whiteCache.value(accessorIndex))
                    {
                        attributes.remove(name);
                        droppedColours++;
                        changed = true;
                    }
                    else
                    {
                        keptColours++;
                    }
                }
            }

            // A morph target holds deltas for the attributes the primitive has, so a target
            // for an attribute that has gone would have nothing to apply to
            if (changed && primitive.contains(QStringLiteral("targets")))
            {
                QJsonArray targets = primitive.value(QStringLiteral("targets")).toArray();

                for (qsizetype t = 0; t < targets.size(); t++)
                {
                    QJsonObject target = targets.at(t).toObject();

                    for (const QString &name : target.keys())
                    {
                        if (!attributes.contains(name)
                            && (name.startsWith(QStringLiteral("TEXCOORD_"))
                                || name.startsWith(QStringLiteral("COLOR_"))
                                || (name == QStringLiteral("TANGENT")))) {
                            target.remove(name);
                        }
                    }
                    targets[t] = target;
                }
                primitive.insert(QStringLiteral("targets"), targets);
            }

            if (changed)
            {
                primitive.insert(QStringLiteral("attributes"), attributes);
                primitives[p] = primitive;
                meshChanged = true;
            }
        }

        if (meshChanged)
        {
            mesh.insert(QStringLiteral("primitives"), primitives);
            meshes[m] = mesh;
        }
    }

    if ((droppedTexCoords == 0) && (droppedColours == 0) && (droppedTangents == 0)) {
        return QString();
    }
    root->insert(QStringLiteral("meshes"), meshes);

    QStringList parts;
    if (droppedTexCoords > 0)
    {
        QStringList sets;
        QList<int> sorted = droppedSets.values();
        std::sort(sorted.begin(), sorted.end());
        for (int set : sorted) {
            sets.append(QString::number(set));
        }
        parts.append(QStringLiteral("%1 unreferenced TEXCOORD_%2")
                         .arg(droppedTexCoords)
                         .arg(sets.join(QStringLiteral("/"))));
    }
    if (droppedColours > 0) {
        parts.append(QStringLiteral("%1 all-white COLOR").arg(droppedColours));
    }
    if (droppedTangents > 0) {
        parts.append(QStringLiteral("%1 TANGENT with no normal map").arg(droppedTangents));
    }
    if (keptColours > 0) {
        parts.append(QStringLiteral("kept %1 COLOR that is used").arg(keptColours));
    }
    if (keptTangents > 0) {
        parts.append(QStringLiteral("kept %1 TANGENT that is used").arg(keptTangents));
    }
    return QStringLiteral("Dropped ") + parts.join(QStringLiteral(", "));
}
