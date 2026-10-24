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

#include "animationmapper.h"

#include <QFile>
#include <QJsonArray>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

// The animation names the ADS-B demodulator plays (adsbdemodtrackerdecode.cpp)
const QString GearName = QStringLiteral("libxplanemp/controls/gear_ratio");
const QString FlapsName = QStringLiteral("libxplanemp/controls/flap_ratio");
const QString SlatsName = QStringLiteral("libxplanemp/controls/slat_ratio");
const QString EngineName = QStringLiteral("libxplanemp/engines/engine_rotation_angle_deg%1");

// GEAR ANIMATION POSITION, in keyframes: retracted at 0, extended with no load on it at
// 100, and the struts compressing as weight comes on beyond that - so the animations run
// to 120, 125, 200 and so on. Extended is keyframe 100 whatever the length: measured on
// the B788, A343 and DH8D, the wheels are lowest at 90-100 and rise again after it. A
// template's ANIM_LENGTH (50 on some) does not move it
constexpr double GearExtended = 100.0;
constexpr double DefaultGearLength = 200.0;
// The engine templates turn one revolution over the animation's length
constexpr double DefaultEngineLength = 100.0;

struct TemplateUse {
    QString m_name;
    QString m_component;
    int m_depth = 0;
    QHash<QString, QString> m_parameters;   // Direct children, upper case names
};

struct PartInfo {
    int m_depth = 0;
    QString m_name;
    QString m_animLength;
    QString m_variable;
};

int indexIn(const QString &text, const QRegularExpression &pattern, int fallback)
{
    const QRegularExpressionMatch match = pattern.match(text);
    return match.hasMatch() ? match.captured(1).toInt() : fallback;
}

double number(const QString &text, double fallback)
{
    bool ok = false;
    const double value = text.trimmed().toDouble(&ok);
    return (ok && (value > 0.0)) ? value : fallback;
}

void classifyTemplate(const TemplateUse &use, QHash<QString, MsfsAnimationRole> *roles)
{
    const QString name = use.m_name.toUpper();
    QString animation = use.m_parameters.value(QStringLiteral("ANIM_NAME")).trimmed();
    if (animation.isEmpty())
    {
        // Asobo's own aircraft use the gear templates' default names (CJ4, DA62, King Air)
        if (name == QStringLiteral("ASOBO_GEAR_CENTER_TEMPLATE")) {
            animation = QStringLiteral("GEAR_Center");
        } else if (name == QStringLiteral("ASOBO_GEAR_LEFT_TEMPLATE")) {
            animation = QStringLiteral("GEAR_Left");
        } else if (name == QStringLiteral("ASOBO_GEAR_RIGHT_TEMPLATE")) {
            animation = QStringLiteral("GEAR_Right");
        }
    }
    if (animation.isEmpty() || roles->contains(animation)) {
        return;
    }
    const double length = number(use.m_parameters.value(QStringLiteral("ANIM_LENGTH")), 0.0);
    MsfsAnimationRole role;
    if ((name == QStringLiteral("FSLTL_COMPGEAR_TEMPLATE"))
        || (name.startsWith(QStringLiteral("ASOBO_GEAR_"))
            && (name.contains(QStringLiteral("CENTER")) || name.contains(QStringLiteral("LEFT"))
                || name.contains(QStringLiteral("RIGHT")))
            && !name.contains(QStringLiteral("TIRE")) && !name.contains(QStringLiteral("STEER"))))
    {
        role.m_kind = MsfsAnimationRole::Gear;
        role.m_length = (length > 0.0) ? length : DefaultGearLength;
    }
    else if ((name == QStringLiteral("FSLTL_ENGINE_BLUR")) || (name == QStringLiteral("FSLTL_ENGINE_STATIC")))
    {
        // RPM_VAR is (A:ENG N1 RPM:2,degrees per second) or (A:GENERAL ENG RPM:2,...)
        static const QRegularExpression engineIndex(QStringLiteral("RPM\\s*:\\s*(\\d+)"),
                                                    QRegularExpression::CaseInsensitiveOption);
        role.m_kind = MsfsAnimationRole::Engine;
        role.m_engine = indexIn(use.m_parameters.value(QStringLiteral("RPM_VAR")), engineIndex, 1);
        role.m_length = (length > 0.0) ? length : DefaultEngineLength;
    }
    else if (name == QStringLiteral("ASOBO_ENGINE_PROPELLER_TEMPLATE"))
    {
        role.m_kind = MsfsAnimationRole::Engine;
        role.m_engine = qMax(1, use.m_parameters.value(QStringLiteral("ID")).trimmed().toInt());
        role.m_length = (length > 0.0) ? length : DefaultEngineLength;
    }
    else if ((name == QStringLiteral("ASOBO_GT_ANIM_CODE")) || (name == QStringLiteral("ASOBO_GT_ANIM")))
    {
        // Flaps and slats are custom code, but in components that say which they are -
        // and the TFS models' slats read the trailing edge flaps variable, so the
        // component comes first and the variable only where there is no component
        const QString component = use.m_component.toUpper();
        const QString code = use.m_parameters.value(QStringLiteral("ANIM_CODE")).toUpper();
        if (component.startsWith(QStringLiteral("SLAT"))) {
            role.m_kind = MsfsAnimationRole::Slats;
        } else if (component.startsWith(QStringLiteral("FLAP"))) {
            role.m_kind = MsfsAnimationRole::Flaps;
        } else if (code.contains(QStringLiteral("LEADING EDGE FLAPS"))) {
            role.m_kind = MsfsAnimationRole::Slats;
        } else if (code.contains(QStringLiteral("TRAILING EDGE FLAPS"))
                   && !component.contains(QStringLiteral("SPOILER"))) {
            role.m_kind = MsfsAnimationRole::Flaps;
        } else {
            return;
        }
        role.m_length = length;
    }
    else
    {
        return;
    }
    roles->insert(animation, role);
}

// The older form: <PartInfo><Name>l_gear</Name><AnimLength>200</AnimLength>
// <Animation><Parameter><Sim><Variable>GEAR ANIMATION POSITION:1</Variable>...
void classifyPartInfo(const PartInfo &part, QHash<QString, MsfsAnimationRole> *roles)
{
    const QString animation = part.m_name.trimmed();
    if (animation.isEmpty() || roles->contains(animation)) {
        return;
    }
    const QString variable = part.m_variable.toUpper();
    MsfsAnimationRole role;
    if (variable.startsWith(QStringLiteral("GEAR ANIMATION POSITION"))) {
        role.m_kind = MsfsAnimationRole::Gear;
        role.m_length = number(part.m_animLength, DefaultGearLength);
    } else if (variable.startsWith(QStringLiteral("TRAILING EDGE FLAPS"))) {
        role.m_kind = MsfsAnimationRole::Flaps;
        role.m_length = number(part.m_animLength, 0.0);
    } else if (variable.startsWith(QStringLiteral("LEADING EDGE FLAPS"))) {
        role.m_kind = MsfsAnimationRole::Slats;
        role.m_length = number(part.m_animLength, 0.0);
    } else {
        return;
    }
    roles->insert(animation, role);
}

// glTF accessor access. The animation data in MSFS models is all float - checked over
// every FSLTL model - so other component types are left alone rather than converted
bool readFloats(const QJsonObject &root, const QVector<QByteArray> &buffers, int accessorIndex,
                int *components, QVector<float> *values)
{
    const QJsonArray accessors = root.value("accessors").toArray();
    if ((accessorIndex < 0) || (accessorIndex >= accessors.size())) {
        return false;
    }
    const QJsonObject accessor = accessors.at(accessorIndex).toObject();
    if (accessor.value("componentType").toInt() != 5126 || accessor.contains("sparse")) {
        return false;
    }
    const QString type = accessor.value("type").toString();
    const int n = (type == "SCALAR") ? 1 : (type == "VEC3") ? 3 : (type == "VEC4") ? 4 : 0;
    if (n == 0) {
        return false;
    }
    const QJsonObject view = root.value("bufferViews").toArray()
        .at(accessor.value("bufferView").toInt(-1)).toObject();
    const int bufferIndex = view.value("buffer").toInt(-1);
    if (view.isEmpty() || (bufferIndex < 0) || (bufferIndex >= buffers.size())) {
        return false;
    }
    const QByteArray &buffer = buffers.at(bufferIndex);
    // toDouble rather than toInteger, which Qt 5 doesn't have
    const qint64 count = static_cast<qint64>(accessor.value("count").toDouble());
    const qint64 stride = static_cast<qint64>(view.value("byteStride").toDouble(n * 4));
    const qint64 offset = static_cast<qint64>(view.value("byteOffset").toDouble()
                                              + accessor.value("byteOffset").toDouble());
    if ((count <= 0) || (offset + (count - 1) * stride + n * 4 > buffer.size())) {
        return false;
    }
    values->resize(count * n);
    for (qint64 i = 0; i < count; i++) {
        std::memcpy(values->data() + i * n, buffer.constData() + offset + i * stride, n * 4);
    }
    *components = n;
    return true;
}

int appendFloats(QJsonObject *root, QVector<QByteArray> *buffers, const QVector<float> &values,
                 int components)
{
    QByteArray &buffer = (*buffers)[0];
    while (buffer.size() % 4) {
        buffer.append('\0');
    }
    const qint64 offset = buffer.size();
    buffer.append(reinterpret_cast<const char *>(values.constData()), values.size() * 4);

    QJsonArray views = root->value("bufferViews").toArray();
    views.append(QJsonObject{{"buffer", 0}, {"byteOffset", offset}, {"byteLength", values.size() * 4}});
    root->insert("bufferViews", views);

    QJsonObject accessor{
        {"bufferView", views.size() - 1},
        {"componentType", 5126},
        {"count", values.size() / components},
        {"type", (components == 1) ? "SCALAR" : (components == 3) ? "VEC3" : "VEC4"}
    };
    if (components == 1)
    {
        // Required for animation input
        const auto [low, high] = std::minmax_element(values.constBegin(), values.constEnd());
        accessor.insert("min", QJsonArray{*low});
        accessor.insert("max", QJsonArray{*high});
    }
    QJsonArray accessors = root->value("accessors").toArray();
    accessors.append(accessor);
    root->insert("accessors", accessors);
    return static_cast<int>(accessors.size() - 1);
}

// Keyframes up to and including `end`, with one made at `end` itself if it falls between
// two, so a gear animation stops at fully extended rather than carrying on into the
// compression the MSFS animation goes on to
void clipKeyframes(QVector<float> *times, QVector<float> *values, int components, double end,
                   bool step, bool rotation)
{
    const int count = times->size();
    int keep = 0;
    while ((keep < count) && ((*times)[keep] <= end)) {
        keep++;
    }
    if (keep == count) {
        return;
    }
    QVector<float> value(components);
    if (keep == 0) {
        // Starts after the end: hold its first pose
        std::copy(values->constBegin(), values->constBegin() + components, value.begin());
    }
    else if ((*times)[keep - 1] == static_cast<float>(end))
    {
        times->resize(keep);
        values->resize(keep * components);
        return;
    }
    else
    {
        const float t0 = (*times)[keep - 1];
        const float t1 = (*times)[keep];
        const float f = step ? 0.0f : static_cast<float>((end - t0) / (t1 - t0));
        const float *a = values->constData() + (keep - 1) * components;
        const float *b = values->constData() + keep * components;
        // Take the shorter way round between two quaternions
        float dot = 0.0f;
        for (int c = 0; c < components; c++) {
            dot += a[c] * b[c];
        }
        const float sign = (rotation && (dot < 0.0f)) ? -1.0f : 1.0f;
        float length = 0.0f;
        for (int c = 0; c < components; c++)
        {
            value[c] = a[c] + (sign * b[c] - a[c]) * f;
            length += value[c] * value[c];
        }
        if (rotation && (length > 0.0f))
        {
            length = std::sqrt(length);
            for (float &v : value) {
                v /= length;
            }
        }
    }
    times->resize(keep + 1);
    (*times)[keep] = static_cast<float>(end);
    values->resize(keep * components);
    values->append(value);
}

struct Group {
    MsfsAnimationRole m_role;
    QList<int> m_animations;
};

} // namespace

bool readMsfsAnimationRoles(const QString &xmlFile, QHash<QString, MsfsAnimationRole> *roles,
                            QString *error)
{
    roles->clear();
    QFile file(xmlFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        *error = QStringLiteral("Cannot open %1: %2").arg(xmlFile, file.errorString());
        return false;
    }
    QXmlStreamReader xml(&file);
    QStringList elements;
    QStringList components;     // Component IDs enclosing the current element
    QVector<TemplateUse> uses;  // UseTemplates enclosing the current element
    QVector<PartInfo> parts;
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.isStartElement())
        {
            const QString name = xml.name().toString();
            elements.append(name);
            const int depth = elements.size();
            if (name.compare(QStringLiteral("Component"), Qt::CaseInsensitive) == 0) {
                components.append(xml.attributes().value(QStringLiteral("ID")).toString());
            } else if (name.compare(QStringLiteral("UseTemplate"), Qt::CaseInsensitive) == 0) {
                uses.append({xml.attributes().value(QStringLiteral("Name")).toString(),
                             components.isEmpty() ? QString() : components.last(), depth, {}});
            } else if (name.compare(QStringLiteral("PartInfo"), Qt::CaseInsensitive) == 0) {
                parts.append({depth, {}, {}, {}});
            }
        }
        else if (xml.isCharacters() && !xml.isWhitespace() && !elements.isEmpty())
        {
            const QString &element = elements.last();
            const int depth = elements.size();
            if (!uses.isEmpty() && (depth == uses.last().m_depth + 1)) {
                uses.last().m_parameters[element.toUpper()] += xml.text().toString();
            }
            if (!parts.isEmpty())
            {
                PartInfo &part = parts.last();
                if ((depth == part.m_depth + 1) && (element.compare(QStringLiteral("Name"), Qt::CaseInsensitive) == 0)) {
                    part.m_name += xml.text().toString();
                } else if ((depth == part.m_depth + 1) && (element.compare(QStringLiteral("AnimLength"), Qt::CaseInsensitive) == 0)) {
                    part.m_animLength += xml.text().toString();
                } else if (element.compare(QStringLiteral("Variable"), Qt::CaseInsensitive) == 0) {
                    part.m_variable += xml.text().toString();
                }
            }
        }
        else if (xml.isEndElement() && !elements.isEmpty())
        {
            const QString name = elements.takeLast();
            if (name.compare(QStringLiteral("Component"), Qt::CaseInsensitive) == 0) {
                if (!components.isEmpty()) {
                    components.removeLast();
                }
            } else if (name.compare(QStringLiteral("UseTemplate"), Qt::CaseInsensitive) == 0) {
                if (!uses.isEmpty()) {
                    classifyTemplate(uses.takeLast(), roles);
                }
            } else if (name.compare(QStringLiteral("PartInfo"), Qt::CaseInsensitive) == 0) {
                if (!parts.isEmpty()) {
                    classifyPartInfo(parts.takeLast(), roles);
                }
            }
        }
    }
    if (xml.hasError())
    {
        *error = QStringLiteral("Invalid model XML %1: %2").arg(xmlFile, xml.errorString());
        return false;
    }
    return true;
}

QStringList mapMsfsAnimations(const QHash<QString, MsfsAnimationRole> &roles, QJsonObject *root,
                              QVector<QByteArray> *buffers)
{
    QStringList made;
    QJsonArray animations = root->value("animations").toArray();
    if (roles.isEmpty() || animations.isEmpty() || buffers->isEmpty()) {
        return made;
    }

    // Which animations make each libxplanemp one. An animation exported once per node
    // carries the node after the name: custom_anim_SIM_ON_GROUND_06_node30_anim_...
    QMap<QString, Group> groups;
    for (qsizetype i = 0; i < animations.size(); i++)
    {
        const QString name = animations.at(i).toObject().value("name").toString();
        auto exact = roles.constFind(name);
        const MsfsAnimationRole *role = (exact != roles.constEnd()) ? &exact.value() : nullptr;
        for (auto it = roles.constBegin(); !role && (it != roles.constEnd()); ++it)
        {
            if (name.startsWith(it.key() + QStringLiteral("_node"))) {
                role = &it.value();
            }
        }
        if (!role) {
            continue;
        }
        QString target;
        switch (role->m_kind)
        {
        case MsfsAnimationRole::Gear: target = GearName; break;
        case MsfsAnimationRole::Flaps: target = FlapsName; break;
        case MsfsAnimationRole::Slats: target = SlatsName; break;
        case MsfsAnimationRole::Engine: target = EngineName.arg(role->m_engine); break;
        }
        Group &group = groups[target];
        if (group.m_animations.isEmpty()) {
            group.m_role = *role;
        }
        group.m_animations.append(static_cast<int>(i));
    }

    QSet<int> replaced;
    QJsonArray added;
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it)
    {
        const Group &group = it.value();
        const MsfsAnimationRole &role = group.m_role;

        // Where the animation is fully deployed, as a keyframe time. A flap or slat
        // animation runs from retracted to fully out, so its last keyframe is that
        double end = 0.0;
        double scale = 1.0;
        for (int index : group.m_animations)
        {
            for (const QJsonValue &sampler : animations.at(index).toObject().value("samplers").toArray())
            {
                const QJsonObject input = root->value("accessors").toArray()
                    .at(sampler.toObject().value("input").toInt(-1)).toObject();
                end = qMax(end, input.value("max").toArray().at(0).toDouble());
            }
        }
        if (role.m_kind == MsfsAnimationRole::Gear) {
            end = qMin(end, GearExtended);  // A gear animation shorter than 100 ends extended
        } else if (role.m_kind == MsfsAnimationRole::Engine) {
            scale = 360.0 / role.m_length;  // One degree per second, as the demod expects
        }
        if (role.m_kind != MsfsAnimationRole::Engine)
        {
            if (end <= 0.0) {
                continue;
            }
            scale = 1.0 / end;
        }

        QJsonArray samplers;
        QJsonArray channels;
        QSet<QString> targets;
        for (int index : group.m_animations)
        {
            const QJsonObject animation = animations.at(index).toObject();
            const QJsonArray sourceSamplers = animation.value("samplers").toArray();
            for (const QJsonValue &channelValue : animation.value("channels").toArray())
            {
                const QJsonObject channel = channelValue.toObject();
                const QJsonObject target = channel.value("target").toObject();
                const QString key = QStringLiteral("%1/%2").arg(target.value("node").toInt(-1))
                    .arg(target.value("path").toString());
                if (targets.contains(key)) {
                    continue;   // Two animations moving one part: the first wins
                }
                const QJsonObject sampler = sourceSamplers.at(channel.value("sampler").toInt(-1)).toObject();
                const QString interpolation = sampler.value("interpolation").toString(QStringLiteral("LINEAR"));
                if (interpolation == QStringLiteral("CUBICSPLINE")) {
                    continue;
                }
                QVector<float> times;
                QVector<float> values;
                int timeComponents = 0;
                int components = 0;
                if (!readFloats(*root, *buffers, sampler.value("input").toInt(-1), &timeComponents, &times)
                    || !readFloats(*root, *buffers, sampler.value("output").toInt(-1), &components, &values)
                    || (timeComponents != 1) || (values.size() != times.size() * components)) {
                    continue;
                }
                if (role.m_kind != MsfsAnimationRole::Engine)
                {
                    clipKeyframes(&times, &values, components, end, interpolation == QStringLiteral("STEP"),
                                  target.value("path").toString() == QStringLiteral("rotation"));
                }
                for (float &t : times) {
                    t = static_cast<float>(t * scale);
                }
                targets.insert(key);
                samplers.append(QJsonObject{
                    {"input", appendFloats(root, buffers, times, 1)},
                    {"output", appendFloats(root, buffers, values, components)},
                    {"interpolation", interpolation}
                });
                channels.append(QJsonObject{{"sampler", samplers.size() - 1}, {"target", target}});
            }
        }
        if (channels.isEmpty()) {
            continue;
        }
        for (int index : group.m_animations) {
            replaced.insert(index);
        }
        added.append(QJsonObject{{"name", it.key()}, {"samplers", samplers}, {"channels", channels}});
        made.append(QStringLiteral("%1 from %2 animation%3 (%4 channels)")
            .arg(it.key())
            .arg(group.m_animations.size())
            .arg(group.m_animations.size() == 1 ? QString() : QStringLiteral("s"))
            .arg(channels.size()));
    }

    // Kept: everything not replaced, wheels and steering and spoilers among them
    QJsonArray result;
    for (qsizetype i = 0; i < animations.size(); i++)
    {
        if (!replaced.contains(static_cast<int>(i))) {
            result.append(animations.at(i));
        }
    }
    for (const QJsonValue &animation : added) {
        result.append(animation);
    }
    root->insert("animations", result);
    return made;
}
