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

#include "nodestrip.h"

#include <QJsonArray>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <array>

namespace {

// Column-major 4x4, the way glTF stores a node matrix, in double - a folded chain
// multiplies several of them together and float would show at the tail of the result
using Mat4 = std::array<double, 16>;

// a, then b: the transform of a point by a and then by b
Mat4 multiply(const Mat4 &a, const Mat4 &b)
{
    Mat4 r{};
    for (int c = 0; c < 4; c++)
    {
        for (int row = 0; row < 4; row++)
        {
            double sum = 0.0;
            for (int k = 0; k < 4; k++) {
                sum += a[k * 4 + row] * b[c * 4 + k];
            }
            r[c * 4 + row] = sum;
        }
    }
    return r;
}

Mat4 nodeMatrix(const QJsonObject &node)
{
    if (node.contains("matrix"))
    {
        const QJsonArray m = node.value("matrix").toArray();
        if (m.size() == 16)
        {
            Mat4 r{};
            for (int i = 0; i < 16; i++) {
                r[i] = m.at(i).toDouble();
            }
            return r;
        }
    }
    double tx = 0.0, ty = 0.0, tz = 0.0;
    if (node.contains("translation"))
    {
        const QJsonArray t = node.value("translation").toArray();
        tx = t.at(0).toDouble();
        ty = t.at(1).toDouble();
        tz = t.at(2).toDouble();
    }
    double qx = 0.0, qy = 0.0, qz = 0.0, qw = 1.0;
    if (node.contains("rotation"))
    {
        const QJsonArray q = node.value("rotation").toArray();   // xyzw
        qx = q.at(0).toDouble();
        qy = q.at(1).toDouble();
        qz = q.at(2).toDouble();
        qw = q.at(3).toDouble();
    }
    double sx = 1.0, sy = 1.0, sz = 1.0;
    if (node.contains("scale"))
    {
        const QJsonArray s = node.value("scale").toArray();
        sx = s.at(0).toDouble();
        sy = s.at(1).toDouble();
        sz = s.at(2).toDouble();
    }
    const double r00 = 1.0 - 2.0 * (qy * qy + qz * qz);
    const double r01 = 2.0 * (qx * qy - qz * qw);
    const double r02 = 2.0 * (qx * qz + qy * qw);
    const double r10 = 2.0 * (qx * qy + qz * qw);
    const double r11 = 1.0 - 2.0 * (qx * qx + qz * qz);
    const double r12 = 2.0 * (qy * qz - qx * qw);
    const double r20 = 2.0 * (qx * qz - qy * qw);
    const double r21 = 2.0 * (qy * qz + qx * qw);
    const double r22 = 1.0 - 2.0 * (qx * qx + qy * qy);
    // Scale is applied first, so it multiplies the column it belongs to
    Mat4 m{};
    m[0]  = r00 * sx; m[1]  = r10 * sx; m[2]  = r20 * sx; m[3]  = 0.0;
    m[4]  = r01 * sy; m[5]  = r11 * sy; m[6]  = r21 * sy; m[7]  = 0.0;
    m[8]  = r02 * sz; m[9]  = r12 * sz; m[10] = r22 * sz; m[11] = 0.0;
    m[12] = tx;       m[13] = ty;       m[14] = tz;       m[15] = 1.0;
    return m;
}

void setMatrix(QJsonObject *node, const Mat4 &m)
{
    node->remove("translation");
    node->remove("rotation");
    node->remove("scale");
    QJsonArray a;
    for (int i = 0; i < 16; i++) {
        a.append(m[i]);
    }
    node->insert("matrix", a);
}

// Anything beyond a name, children and a transform means something is attached that we
// may not know about - a mesh, a camera, a light from an extension - so the node stays
bool carriesSomething(const QJsonObject &node)
{
    static const QSet<QString> transformOnly{
        QStringLiteral("name"), QStringLiteral("children"),
        QStringLiteral("translation"), QStringLiteral("rotation"),
        QStringLiteral("scale"), QStringLiteral("matrix")
    };
    const QStringList keys = node.keys();
    for (const QString &key : keys)
    {
        if (!transformOnly.contains(key)) {
            return true;
        }
    }
    return false;
}

} // namespace

QString stripRedundantNodes(QJsonObject *root)
{
    QJsonArray nodes = root->value("nodes").toArray();
    const qsizetype count = nodes.size();
    if (count == 0) {
        return QString();
    }

    const QJsonArray animations = root->value("animations").toArray();
    QSet<int> animated;
    for (const QJsonValue &animationValue : animations)
    {
        for (const QJsonValue &channelValue : animationValue.toObject().value("channels").toArray())
        {
            const int node = channelValue.toObject().value("target").toObject()
                .value("node").toInt(-1);
            if (node >= 0) {
                animated.insert(node);
            }
        }
    }
    QSet<int> jointed;
    for (const QJsonValue &skinValue : root->value("skins").toArray())
    {
        const QJsonObject skin = skinValue.toObject();
        for (const QJsonValue &joint : skin.value("joints").toArray()) {
            jointed.insert(joint.toInt(-1));
        }
        if (skin.contains("skeleton")) {
            jointed.insert(skin.value("skeleton").toInt(-1));
        }
    }

    QVector<bool> keep(count, true);
    QVector<QList<int>> children(count);
    for (qsizetype i = 0; i < count; i++)
    {
        for (const QJsonValue &child : nodes.at(i).toObject().value("children").toArray())
        {
            const int c = child.toInt(-1);
            if ((c >= 0) && (c < count)) {
                children[i].append(c);
            }
        }
    }

    const auto pinned = [&](int i) {
        return carriesSomething(nodes.at(i).toObject())
            || animated.contains(i) || jointed.contains(i);
    };
    const auto survivingChildren = [&](int i) {
        QList<int> out;
        for (int c : children[i])
        {
            if (keep[c]) {
                out.append(c);
            }
        }
        return out;
    };

    // Empty leaves, over and over: dropping one can leave its parent a leaf as well
    int removed = 0;
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (qsizetype i = 0; i < count; i++)
        {
            const int node = static_cast<int>(i);
            if (!keep[i] || pinned(node) || !survivingChildren(node).isEmpty()) {
                continue;
            }
            keep[i] = false;
            removed++;
            changed = true;
        }
    }

    // Pass-throughs: fold the transform into the one child left under it. Which child
    // each folded node became has to be recorded as it happens - the children list still
    // holds the ones already removed, so it can't be worked out again afterwards
    QVector<int> foldedInto(count, -1);
    int folded = 0;
    for (qsizetype i = 0; i < count; i++)
    {
        const int node = static_cast<int>(i);
        if (!keep[i] || pinned(node)) {
            continue;
        }
        const QList<int> kids = survivingChildren(node);
        if (kids.size() != 1) {
            continue;
        }
        const int child = kids.first();
        if (animated.contains(child)) {
            continue;   // animation would overwrite the transform we folded in
        }
        QJsonObject childNode = nodes.at(child).toObject();
        setMatrix(&childNode, multiply(nodeMatrix(nodes.at(i).toObject()), nodeMatrix(childNode)));
        nodes[child] = childNode;
        keep[i] = false;
        foldedInto[i] = child;
        folded++;
        // whoever pointed at this node now points at the child instead
        for (qsizetype p = 0; p < count; p++)
        {
            for (int &c : children[p])
            {
                if (c == node) {
                    c = child;
                }
            }
        }
    }

    if ((removed == 0) && (folded == 0)) {
        return QString();
    }

    // Renumber what is left, and everything that refers to a node by index
    QVector<int> renumbered(count, -1);
    QJsonArray out;
    for (qsizetype i = 0; i < count; i++)
    {
        if (keep[i])
        {
            renumbered[i] = static_cast<int>(out.size());
            out.append(nodes.at(i));
        }
    }
    for (qsizetype i = 0; i < count; i++)
    {
        if (!keep[i]) {
            continue;
        }
        QJsonObject node = out.at(renumbered[i]).toObject();
        QJsonArray kids;
        for (int c : children[i])
        {
            if (keep[c]) {
                kids.append(renumbered[c]);
            }
        }
        if (kids.isEmpty()) {
            node.remove("children");
        } else {
            node.insert("children", kids);
        }
        out[renumbered[i]] = node;
    }
    root->insert("nodes", out);

    // A scene root that was folded away is now the child it was folded into
    QJsonArray scenes = root->value("scenes").toArray();
    for (qsizetype s = 0; s < scenes.size(); s++)
    {
        QJsonObject scene = scenes.at(s).toObject();
        QJsonArray roots;
        for (const QJsonValue &rootValue : scene.value("nodes").toArray())
        {
            int i = rootValue.toInt(-1);
            // A folded root is now the child it went into, which may itself have been
            // folded further down; a root removed as an empty leaf has no replacement
            while ((i >= 0) && (i < count) && !keep[i]) {
                i = foldedInto[i];
            }
            if ((i >= 0) && (i < count) && (renumbered[i] >= 0)) {
                roots.append(renumbered[i]);
            }
        }
        scene.insert("nodes", roots);
        scenes[s] = scene;
    }
    if (!scenes.isEmpty()) {
        root->insert("scenes", scenes);
    }

    if (!animations.isEmpty())
    {
        QJsonArray newAnimations;
        for (const QJsonValue &animationValue : animations)
        {
            QJsonObject animation = animationValue.toObject();
            QJsonArray channels;
            for (const QJsonValue &channelValue : animation.value("channels").toArray())
            {
                QJsonObject channel = channelValue.toObject();
                QJsonObject target = channel.value("target").toObject();
                const int i = target.value("node").toInt(-1);
                if ((i < 0) || (i >= count) || (renumbered[i] < 0)) {
                    continue;   // a channel with no node is to be ignored anyway
                }
                target.insert("node", renumbered[i]);
                channel.insert("target", target);
                channels.append(channel);
            }
            animation.insert("channels", channels);
            newAnimations.append(animation);
        }
        root->insert("animations", newAnimations);
    }

    QJsonArray skins = root->value("skins").toArray();
    if (!skins.isEmpty())
    {
        for (qsizetype s = 0; s < skins.size(); s++)
        {
            QJsonObject skin = skins.at(s).toObject();
            QJsonArray joints;
            for (const QJsonValue &joint : skin.value("joints").toArray())
            {
                const int i = joint.toInt(-1);
                if ((i >= 0) && (i < count) && (renumbered[i] >= 0)) {
                    joints.append(renumbered[i]);
                }
            }
            skin.insert("joints", joints);
            if (skin.contains("skeleton"))
            {
                const int i = skin.value("skeleton").toInt(-1);
                if ((i >= 0) && (i < count) && (renumbered[i] >= 0)) {
                    skin.insert("skeleton", renumbered[i]);
                } else {
                    skin.remove("skeleton");
                }
            }
            skins[s] = skin;
        }
        root->insert("skins", skins);
    }

    return QStringLiteral("%1 nodes to %2 (%3 empty removed, %4 pass-through%5 folded)")
        .arg(count).arg(out.size()).arg(removed).arg(folded)
        .arg(folded == 1 ? QString() : QStringLiteral("s"));
}
