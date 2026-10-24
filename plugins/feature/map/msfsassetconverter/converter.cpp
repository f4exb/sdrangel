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

#include "converter.h"
#include "animationmapper.h"
#include "nodestrip.h"
#include "hiddenparts.h"
#include "portholes.h"
#include "primmerge.h"

#include "ddsdecoder.h"
#include "fsltlresolver.h"
#include "ktx2writer.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>

namespace {

constexpr int ComponentByte = 5120;
constexpr int ComponentUnsignedByte = 5121;
constexpr int ComponentShort = 5122;
constexpr int ComponentUnsignedShort = 5123;
constexpr int ComponentUnsignedInt = 5125;
constexpr int ComponentFloat = 5126;
constexpr int ArrayBufferTarget = 34962;

enum class AttributeKind { Normal, Tangent, TexCoord, Color, Weights };

std::mutex textureLocksMutex;
std::map<QString, std::shared_ptr<std::mutex>> textureLocks;

std::shared_ptr<std::mutex> textureLock(const QString &fileName)
{
    const QString path = QFileInfo(fileName).absoluteFilePath();
    std::lock_guard<std::mutex> guard(textureLocksMutex);
    auto &lock = textureLocks[path];
    if (!lock) {
        lock = std::make_shared<std::mutex>();
    }
    return lock;
}

template <typename T>
T littleEndianValue(const QByteArray &bytes, qsizetype offset)
{
    return qFromLittleEndian<T>(
        reinterpret_cast<const uchar *>(bytes.constData() + offset));
}

bool validFileRange(quint64 offset, quint64 length, quint64 fileSize)
{
    return offset <= fileSize && length <= fileSize - offset;
}

bool reusableKtx2(const QString &fileName)
{
    QFile file(fileName);
    const quint64 fileSize = static_cast<quint64>(file.size());
    if (!file.open(QIODevice::ReadOnly) || fileSize < 104) {
        return false;
    }
    const QByteArray prefix = file.read(80 + 32 * 24);
    static const QByteArray identifier("\xABKTX 20\xBB\r\n\x1A\n", 12);
    if (prefix.size() < 80 || prefix.left(12) != identifier) {
        return false;
    }

    const quint32 width = littleEndianValue<quint32>(prefix, 20);
    const quint32 height = littleEndianValue<quint32>(prefix, 24);
    const quint32 faceCount = littleEndianValue<quint32>(prefix, 36);
    const quint32 levelCount = littleEndianValue<quint32>(prefix, 40);
    if (width == 0 || height == 0 || faceCount != 1 || levelCount == 0 ||
        levelCount > 32 || prefix.size() < 80 + levelCount * 24) {
        return false;
    }

    const quint32 dfdOffset = littleEndianValue<quint32>(prefix, 48);
    const quint32 dfdLength = littleEndianValue<quint32>(prefix, 52);
    const quint32 kvdOffset = littleEndianValue<quint32>(prefix, 56);
    const quint32 kvdLength = littleEndianValue<quint32>(prefix, 60);
    const quint64 sgdOffset = littleEndianValue<quint64>(prefix, 64);
    const quint64 sgdLength = littleEndianValue<quint64>(prefix, 72);
    if (dfdLength == 0 || !validFileRange(dfdOffset, dfdLength, fileSize) ||
        !validFileRange(kvdOffset, kvdLength, fileSize) ||
        !validFileRange(sgdOffset, sgdLength, fileSize)) {
        return false;
    }
    for (quint32 level = 0; level < levelCount; ++level)
    {
        const qsizetype offset = 80 + static_cast<qsizetype>(level) * 24;
        const quint64 byteOffset = littleEndianValue<quint64>(prefix, offset);
        const quint64 byteLength = littleEndianValue<quint64>(prefix, offset + 8);
        const quint64 uncompressedLength =
            littleEndianValue<quint64>(prefix, offset + 16);
        if (byteLength == 0 || uncompressedLength == 0 ||
            !validFileRange(byteOffset, byteLength, fileSize)) {
            return false;
        }
    }
    return true;
}

QByteArray fileSha256(const QString &fileName,
                      const QByteArray &context = QByteArray())
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(context);
    return hash.addData(&file) ? hash.result().toHex() : QByteArray();
}

bool writeKtxChecksum(const QString &fileName, QString *error)
{
    const QByteArray digest = fileSha256(fileName);
    if (digest.isEmpty())
    {
        *error = QStringLiteral("Cannot checksum generated KTX2 texture: %1")
                     .arg(fileName);
        return false;
    }
    QSaveFile checksum(fileName + QStringLiteral(".sha256"));
    if (!checksum.open(QIODevice::WriteOnly) ||
        checksum.write(digest) != digest.size() || !checksum.commit()) {
        *error = QStringLiteral("Cannot write KTX2 checksum: %1")
                     .arg(checksum.errorString());
        return false;
    }
    return true;
}

bool reusableCachedKtx2(const QString &fileName)
{
    if (!reusableKtx2(fileName)) {
        return false;
    }
    QFile checksum(fileName + QStringLiteral(".sha256"));
    if (!checksum.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray expected = checksum.read(65).trimmed();
    return expected.size() == 64 && fileSha256(fileName) == expected;
}

bool reusableCopy(const QString &fileName, const QString &sourceFile,
                  const QByteArray &context,
                  const QByteArray &sourceFingerprint)
{
    const QFileInfo cached(fileName);
    const QFileInfo source(sourceFile);
    if (!cached.isFile() || !cached.isReadable() || cached.size() <= 0 ||
        cached.size() != source.size()) {
        return false;
    }
    return fileSha256(fileName, context) == sourceFingerprint;
}

bool copyTextureAtomically(const QString &sourceFile,
                           const QString &destinationFile)
{
    QFile source(sourceFile);
    QSaveFile destination(destinationFile);
    if (!source.open(QIODevice::ReadOnly) ||
        !destination.open(QIODevice::WriteOnly)) {
        return false;
    }
    while (!source.atEnd()) {
        const QByteArray bytes = source.read(1024 * 1024);
        if (bytes.isEmpty() || destination.write(bytes) != bytes.size())
        {
            destination.cancelWriting();
            return false;
        }
    }
    return destination.commit();
}

void emitLog(const ConversionLog &log, const QString &message)
{
    if (log) {
        log(message);
    }
}

bool readFile(const QString &fileName, QByteArray *contents, QString *error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        *error =
            QStringLiteral("Cannot open %1: %2").arg(fileName, file.errorString());
        return false;
    }
    *contents = file.readAll();
    return true;
}

bool writeFile(const QString &fileName, const QByteArray &contents,
               QString *error)
{
    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        *error = QStringLiteral("Cannot create %1: %2")
                     .arg(fileName, file.errorString());
        return false;
    }
    if (file.write(contents) != contents.size() || !file.commit())
    {
        *error =
            QStringLiteral("Cannot write %1: %2").arg(fileName, file.errorString());
        return false;
    }
    return true;
}

float halfToFloat(quint16 half)
{
    const quint32 sign = static_cast<quint32>(half & 0x8000U) << 16;
    quint32 exponent = (half >> 10) & 0x1FU;
    quint32 mantissa = half & 0x03FFU;
    quint32 bits;

    if (exponent == 0)
    {
        if (mantissa == 0) {
            bits = sign;
        }
        else
        {
            int shift = 0;
            while ((mantissa & 0x0400U) == 0)
            {
                mantissa <<= 1;
                ++shift;
            }
            mantissa &= 0x03FFU;
            const quint32 floatExponent = static_cast<quint32>(127 - 14 - shift);
            bits = sign | (floatExponent << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        bits = sign | 0x7F800000U | (mantissa << 13);
    }
    else
    {
        exponent += 127 - 15;
        bits = sign | (exponent << 23) | (mantissa << 13);
    }

    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

float floatFromLittleEndian(const uchar *bytes)
{
    const quint32 bits = qFromLittleEndian<quint32>(bytes);
    float value;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void appendFloat(QByteArray *destination, float value)
{
    quint32 bits;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    bits = qToLittleEndian(bits);
    destination->append(reinterpret_cast<const char *>(&bits), sizeof(bits));
}

int componentCount(const QString &type)
{
    if (type == "SCALAR") {
        return 1;
    }
    if (type == "VEC2") {
        return 2;
    }
    if (type == "VEC3") {
        return 3;
    }
    if (type == "VEC4") {
        return 4;
    }
    return 0;
}

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
    case ComponentFloat:
        return 4;
    default:
        return 0;
    }
}

bool identifyConversions(const QJsonArray &meshes, const QJsonArray &accessors,
                         QHash<int, AttributeKind> *conversions,
                         QString *error)
{
    for (const QJsonValue &meshValue : meshes) {
        const QJsonArray primitives =
            meshValue.toObject().value("primitives").toArray();
        for (const QJsonValue &primitiveValue : primitives) {
            const QJsonObject attributes =
                primitiveValue.toObject().value("attributes").toObject();
            for (auto it = attributes.begin(); it != attributes.end(); ++it)
            {
                AttributeKind kind;
                if (it.key() == "NORMAL") {
                    kind = AttributeKind::Normal;
                } else if (it.key() == "TANGENT") {
                    kind = AttributeKind::Tangent;
                } else if (it.key().startsWith("TEXCOORD_")) {
                    kind = AttributeKind::TexCoord;
                } else if (it.key().startsWith("COLOR_")) {
                    kind = AttributeKind::Color;
                } else if (it.key().startsWith("WEIGHTS_")) {
                    kind = AttributeKind::Weights;
                } else {
                    continue;
                }
                const int accessorIndex = it.value().toInt(-1);
                if (accessorIndex < 0)
                {
                    *error =
                        QStringLiteral("Invalid accessor for attribute %1").arg(it.key());
                    return false;
                }
                // Weights are only rewritten when they are not already what glTF asks
                // for, so the models that are right are left untouched - see convertAccessor
                if ((kind == AttributeKind::Weights) &&
                    (accessorIndex < accessors.size()) &&
                    (accessors.at(accessorIndex).toObject().value("type").toString() ==
                     QLatin1String("VEC4"))) {
                    continue;
                }
                if (conversions->contains(accessorIndex) &&
                    conversions->value(accessorIndex) != kind) {
                    *error = QStringLiteral(
                                 "Accessor %1 is reused with incompatible semantics")
                                 .arg(accessorIndex);
                    return false;
                }
                conversions->insert(accessorIndex, kind);
            }
        }
    }
    return true;
}

bool convertAccessor(int accessorIndex, AttributeKind kind,
                     QJsonArray *accessors, QJsonArray *bufferViews,
                     QVector<QByteArray> *buffers, QString *error)
{
    QJsonObject accessor = accessors->at(accessorIndex).toObject();
    if (accessor.contains("sparse"))
    {
        *error = QStringLiteral(
                     "Sparse accessor %1 is not supported for ASOBO expansion")
                     .arg(accessorIndex);
        return false;
    }
    const int viewIndex = accessor.value("bufferView").toInt(-1);
    if (viewIndex < 0 || viewIndex >= bufferViews->size())
    {
        *error = QStringLiteral("Accessor %1 has no usable bufferView")
                     .arg(accessorIndex);
        return false;
    }
    const QJsonObject view = bufferViews->at(viewIndex).toObject();
    const int bufferIndex = view.value("buffer").toInt(0);
    if (bufferIndex < 0 || bufferIndex >= buffers->size())
    {
        *error = QStringLiteral("Accessor %1 references invalid buffer %2")
                     .arg(accessorIndex)
                     .arg(bufferIndex);
        return false;
    }

    const int sourceComponents =
        componentCount(accessor.value("type").toString());
    const int sourceComponentType = accessor.value("componentType").toInt();
    const int sourceComponentSize = componentSize(sourceComponentType);
    const int count = accessor.value("count").toInt(-1);
    if (sourceComponents <= 0 || sourceComponentSize <= 0 || count < 0)
    {
        *error = QStringLiteral("Accessor %1 has an unsupported source layout")
                     .arg(accessorIndex);
        return false;
    }

    const int packedSize = sourceComponents * sourceComponentSize;
    const int stride = view.value("byteStride").toInt(packedSize);
    const qsizetype viewStart =
        static_cast<qsizetype>(view.value("byteOffset").toDouble());
    const qsizetype viewLength =
        static_cast<qsizetype>(view.value("byteLength").toDouble(-1));
    const qsizetype accessorOffset =
        static_cast<qsizetype>(accessor.value("byteOffset").toDouble());
    const qsizetype start = viewStart + accessorOffset;
    const QByteArray &sourceBuffer = buffers->at(bufferIndex);
    const quint64 relativeEnd =
        count == 0
            ? static_cast<quint64>(accessorOffset)
            : static_cast<quint64>(accessorOffset) +
                  static_cast<quint64>(count - 1) * static_cast<quint64>(stride) +
                  packedSize;
    if (viewStart < 0 || viewLength < 0 || accessorOffset < 0 ||
        stride < packedSize || relativeEnd > static_cast<quint64>(viewLength) ||
        static_cast<quint64>(viewStart) + static_cast<quint64>(viewLength) >
            static_cast<quint64>(sourceBuffer.size())) {
        *error = QStringLiteral("Accessor %1 exceeds its source bufferView")
                     .arg(accessorIndex);
        return false;
    }

    int outputComponents = sourceComponents;
    if (kind == AttributeKind::Normal)
    {
        if (sourceComponentType != ComponentByte || sourceComponents != 4)
        {
            *error = QStringLiteral("NORMAL accessor %1 is not ASOBO BYTE VEC4")
                         .arg(accessorIndex);
            return false;
        }
        outputComponents = 3;
    }
    else if (kind == AttributeKind::Tangent)
    {
        if (sourceComponentType != ComponentByte || sourceComponents != 4)
        {
            *error = QStringLiteral("TANGENT accessor %1 is not ASOBO BYTE VEC4")
                         .arg(accessorIndex);
            return false;
        }
        outputComponents = 4;
    }
    else if (kind == AttributeKind::TexCoord)
    {
        if (sourceComponentType != ComponentShort || sourceComponents != 2)
        {
            *error = QStringLiteral("TEXCOORD accessor %1 is not ASOBO SHORT VEC2")
                         .arg(accessorIndex);
            return false;
        }
        outputComponents = 2;
    }
    else if (kind == AttributeKind::Weights)
    {
        // glTF requires WEIGHTS_0 to be VEC4. Asobo's exporter writes a SCALAR accessor
        // for a primitive whose vertices each have a single full influence, and Cesium
        // declares the attribute from that type - "in float a_weights_0" - then swizzles
        // it .x/.y/.z/.w in the skinning loop. The vertex shader then fails to compile and
        // the map stops rendering altogether, not just that model. The missing influences
        // are written as zero, which is what a weight that was never given means
        if ((sourceComponentType != ComponentFloat) &&
            (sourceComponentType != ComponentUnsignedByte) &&
            (sourceComponentType != ComponentUnsignedShort))
        {
            *error = QStringLiteral("WEIGHTS accessor %1 is not a float or normalized "
                                    "integer vector")
                         .arg(accessorIndex);
            return false;
        }
        outputComponents = 4;
    } else if (kind == AttributeKind::Color) {
        const bool byteColor = sourceComponentType == ComponentByte ||
                               sourceComponentType == ComponentUnsignedByte;
        if ((!byteColor && sourceComponentType != ComponentUnsignedShort) ||
            (sourceComponents != 3 && sourceComponents != 4)) {
            *error = QStringLiteral(
                         "COLOR accessor %1 is not an ASOBO byte or ushort vector")
                         .arg(accessorIndex);
            return false;
        }
    }

    QByteArray converted;
    converted.reserve(count * outputComponents * static_cast<int>(sizeof(float)));
    const uchar *source =
        reinterpret_cast<const uchar *>(sourceBuffer.constData());
    for (int element = 0; element < count; ++element) {
        const uchar *input =
            source + start + static_cast<qsizetype>(element) * stride;
        for (int component = 0; component < outputComponents; ++component)
        {
            float value;
            if (kind == AttributeKind::Normal || kind == AttributeKind::Tangent) {
                value = std::max(-1.0F, static_cast<qint8>(input[component]) / 127.0F);
            } else if (kind == AttributeKind::Weights) {
                if (component >= sourceComponents) {
                    value = 0.0F;   // An influence the exporter did not write
                } else if (sourceComponentType == ComponentFloat) {
                    value = floatFromLittleEndian(input + component * 4);
                } else if (sourceComponentSize == 1) {
                    value = input[component] / 255.0F;
                } else {
                    value = qFromLittleEndian<quint16>(input + component * 2) / 65535.0F;
                }
            } else if (kind == AttributeKind::Color && sourceComponentSize == 1) {
                // Some stock-derived MSFS models declare packed colours as BYTE even
                // though their raw values use the full unsigned 0..255 range.
                value = input[component] / 255.0F;
            } else {
                value = halfToFloat(qFromLittleEndian<quint16>(input + component * 2));
            }
            if (!std::isfinite(value))
            {
                *error =
                    QStringLiteral("Accessor %1 contains a non-finite converted value")
                        .arg(accessorIndex);
                return false;
            }
            appendFloat(&converted, value);
        }
    }

    QByteArray &destinationBuffer = (*buffers)[bufferIndex];
    while ((destinationBuffer.size() & 3) != 0) {
        destinationBuffer.append('\0');
    }
    const qsizetype destinationOffset = destinationBuffer.size();
    destinationBuffer.append(converted);

    QJsonObject newView;
    newView.insert("buffer", bufferIndex);
    newView.insert("byteOffset", static_cast<double>(destinationOffset));
    newView.insert("byteLength", converted.size());
    newView.insert("target", ArrayBufferTarget);
    const int newViewIndex = bufferViews->size();
    bufferViews->append(newView);

    accessor.insert("bufferView", newViewIndex);
    accessor.insert("byteOffset", 0);
    accessor.insert("componentType", ComponentFloat);
    accessor.insert("type", QStringLiteral("VEC%1").arg(outputComponents));
    accessor.remove("normalized");
    accessor.remove("min");
    accessor.remove("max");
    (*accessors)[accessorIndex] = accessor;
    return true;
}

bool flipTriangleWinding(const QJsonArray &meshes, const QJsonArray &accessors,
                         const QJsonArray &bufferViews,
                         QVector<QByteArray> *buffers, int *flippedTriangles,
                         QString *error)
{
    QSet<int> convertedIndexAccessors;
    for (const QJsonValue &meshValue : meshes)
    {
        for (const QJsonValue &primitiveValue :
             meshValue.toObject().value("primitives").toArray()) {
            const QJsonObject primitive = primitiveValue.toObject();
            if (primitive.value("mode").toInt(4) != 4) {
                continue;
            }
            const int accessorIndex = primitive.value("indices").toInt(-1);
            if (accessorIndex < 0)
            {
                *error = QStringLiteral(
                    "Prototype cannot reverse a non-indexed triangle primitive");
                return false;
            }
            if (convertedIndexAccessors.contains(accessorIndex)) {
                continue;
            }
            if (accessorIndex >= accessors.size())
            {
                *error =
                    QStringLiteral("Primitive references missing index accessor %1")
                        .arg(accessorIndex);
                return false;
            }

            const QJsonObject accessor = accessors.at(accessorIndex).toObject();
            if (accessor.contains("sparse"))
            {
                *error = QStringLiteral(
                             "Sparse index accessor %1 cannot be modified in place")
                             .arg(accessorIndex);
                return false;
            }
            const int viewIndex = accessor.value("bufferView").toInt(-1);
            if (viewIndex < 0 || viewIndex >= bufferViews.size())
            {
                *error = QStringLiteral("Index accessor %1 has no usable bufferView")
                             .arg(accessorIndex);
                return false;
            }
            const QJsonObject view = bufferViews.at(viewIndex).toObject();
            const int bufferIndex = view.value("buffer").toInt(0);
            if (bufferIndex < 0 || bufferIndex >= buffers->size())
            {
                *error = QStringLiteral("Index accessor %1 references invalid buffer")
                             .arg(accessorIndex);
                return false;
            }
            const int componentType = accessor.value("componentType").toInt();
            int bytesPerIndex = 0;
            if (componentType == ComponentUnsignedByte) {
                bytesPerIndex = 1;
            } else if (componentType == ComponentUnsignedShort) {
                bytesPerIndex = 2;
            } else if (componentType == ComponentUnsignedInt) {
                bytesPerIndex = 4;
            }
            else
            {
                *error = QStringLiteral(
                             "Index accessor %1 has unsupported component type %2")
                             .arg(accessorIndex)
                             .arg(componentType);
                return false;
            }
            const int count = accessor.value("count").toInt(-1);
            if (count < 0 || count % 3 != 0)
            {
                *error =
                    QStringLiteral("Triangle index accessor %1 has invalid count %2")
                        .arg(accessorIndex)
                        .arg(count);
                return false;
            }
            const int stride = view.value("byteStride").toInt(bytesPerIndex);
            const qsizetype viewStart =
                static_cast<qsizetype>(view.value("byteOffset").toDouble());
            const qsizetype viewLength =
                static_cast<qsizetype>(view.value("byteLength").toDouble(-1));
            const qsizetype accessorOffset =
                static_cast<qsizetype>(accessor.value("byteOffset").toDouble());
            const qsizetype start = viewStart + accessorOffset;
            QByteArray &buffer = (*buffers)[bufferIndex];
            const quint64 relativeEnd = count == 0
                                            ? static_cast<quint64>(accessorOffset)
                                            : static_cast<quint64>(accessorOffset) +
                                                  static_cast<quint64>(count - 1) *
                                                      static_cast<quint64>(stride) +
                                                  bytesPerIndex;
            if (viewStart < 0 || viewLength < 0 || accessorOffset < 0 ||
                stride < bytesPerIndex ||
                relativeEnd > static_cast<quint64>(viewLength) ||
                static_cast<quint64>(viewStart) + static_cast<quint64>(viewLength) >
                    static_cast<quint64>(buffer.size())) {
                *error =
                    QStringLiteral("Index accessor %1 exceeds its source bufferView")
                        .arg(accessorIndex);
                return false;
            }

            char *data = buffer.data();
            for (int index = 0; index < count; index += 3)
            {
                char *second =
                    data + start + static_cast<qsizetype>(index + 1) * stride;
                char *third = data + start + static_cast<qsizetype>(index + 2) * stride;
                for (int byte = 0; byte < bytesPerIndex; ++byte) {
                    std::swap(second[byte], third[byte]);
                }
            }
            *flippedTriangles += count / 3;
            convertedIndexAccessors.insert(accessorIndex);
        }
    }
    return true;
}

int sanitizeExtras(QJsonValue *value)
{
    int removed = 0;
    if (value->isArray())
    {
        QJsonArray array = value->toArray();
        for (qsizetype i = 0; i < array.size(); ++i)
        {
            QJsonValue child = array.at(i);
            removed += sanitizeExtras(&child);
            array[i] = child;
        }
        *value = array;
    }
    else if (value->isObject())
    {
        QJsonObject object = value->toObject();
        const auto keys = object.keys();
        for (const QString &key : keys)
        {
            if (key == "extras" && !object.value(key).isObject())
            {
                object.remove(key);
                ++removed;
            }
            else
            {
                QJsonValue child = object.value(key);
                removed += sanitizeExtras(&child);
                object.insert(key, child);
            }
        }
        *value = object;
    }
    return removed;
}

void stripExtensions(QJsonValue *value, const QSet<QString> &names)
{
    if (value->isArray())
    {
        QJsonArray array = value->toArray();
        for (qsizetype i = 0; i < array.size(); ++i)
        {
            QJsonValue child = array.at(i);
            stripExtensions(&child, names);
            array[i] = child;
        }
        *value = array;
    }
    else if (value->isObject())
    {
        QJsonObject object = value->toObject();
        const auto keys = object.keys();
        for (const QString &key : keys)
        {
            QJsonValue child = object.value(key);
            if (key == "extensions" && child.isObject())
            {
                QJsonObject extensions = child.toObject();
                for (const QString &name : names) {
                    extensions.remove(name);
                }
                if (extensions.isEmpty())
                {
                    object.remove(key);
                    continue;
                }
                child = extensions;
            }
            stripExtensions(&child, names);
            object.insert(key, child);
        }
        *value = object;
    }
}

QJsonArray withoutExtensions(const QJsonArray &array,
                             const QSet<QString> &names)
{
    QJsonArray filtered;
    for (const QJsonValue &value : array)
    {
        if (!names.contains(value.toString())) {
            filtered.append(value);
        }
    }
    return filtered;
}

QJsonArray withExtension(QJsonArray array, const QString &name)
{
    for (const QJsonValue &value : array)
    {
        if (value.toString() == name) {
            return array;
        }
    }
    array.append(name);
    return array;
}

int textureSource(const QJsonObject &texture)
{
    const QJsonObject dds = texture.value("extensions")
                                .toObject()
                                .value("MSFT_texture_dds")
                                .toObject();
    return dds.contains("source") ? dds.value("source").toInt(-1)
                                  : texture.value("source").toInt(-1);
}

QString legacyKtxNameForDds(const QString &uri, qsizetype imageIndex,
                            const QByteArray &fingerprint)
{
    QString name = QFileInfo(uri).fileName();
    if (name.endsWith(".dds", Qt::CaseInsensitive)) {
        name.chop(4);
    }
    return QStringLiteral("%1_%2_%3.ktx2")
        .arg(name)
        .arg(imageIndex)
        .arg(QString::fromLatin1(fingerprint.left(12)));
}

QString cachedKtxName(const QByteArray &fingerprint)
{
    return QString::fromLatin1(fingerprint) + QStringLiteral(".ktx2");
}

QString cachedImageName(const QString &sourceFile,
                        const QByteArray &fingerprint)
{
    const QString suffix = QFileInfo(sourceFile).suffix().toLower();
    return QString::fromLatin1(fingerprint) +
           (suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix);
}

bool safeRelativePath(const QString &uri, QString *decoded, QString *error)
{
    const QUrl url = QUrl::fromEncoded(uri.toUtf8(), QUrl::StrictMode);
    *decoded = url.path(QUrl::FullyDecoded);
    const QString clean = QDir::cleanPath(*decoded);
    if (!url.isValid() || !url.scheme().isEmpty() || !url.authority().isEmpty() ||
        QDir::isAbsolutePath(*decoded) || clean == ".." ||
        clean.startsWith("../") || clean.contains(':')) {
        *error = QStringLiteral("Unsafe external URI is not allowed: %1").arg(uri);
        return false;
    }
    *decoded = clean;
    return true;
}

QString containedFile(const QDir &root, const QString &relative)
{
    const QFileInfo candidate(root.filePath(relative));
    if (!candidate.isFile())
        return {};
    const QString rootPath = QFileInfo(root.absolutePath()).canonicalFilePath();
    const QString filePath = candidate.canonicalFilePath();
    if (rootPath.isEmpty() || filePath.isEmpty())
        return {};
    const QString prefix = QDir::toNativeSeparators(rootPath) + QDir::separator();
    const Qt::CaseSensitivity pathCase =
#ifdef Q_OS_WIN
        Qt::CaseInsensitive;
#else
        Qt::CaseSensitive;
#endif
    if (!QDir::toNativeSeparators(filePath).startsWith(prefix, pathCase)) {
        return {};
    }
    return filePath;
}

QString findTextureFile(const QString &uri, const QDir &modelDirectory,
                        const QStringList &textureDirectories, QString *error)
{
    QString relative;
    if (!safeRelativePath(uri, &relative, error)) {
        return {};
    }
    for (const QString &directory : textureDirectories)
    {
        const QString candidate = containedFile(QDir(directory), relative);
        if (!candidate.isEmpty())
            return candidate;
    }
    return containedFile(modelDirectory, relative);
}

// Qt's smooth scale works in premultiplied alpha, so a texel with zero alpha comes back
// with its colour gone - black - and that black bleeds into its neighbours too. These
// liveries are drawn with an OPAQUE material, which never applies the alpha, so the black
// is simply what is seen: six B744F freighters lost their whole airframe and kept only
// their decals, their albedo being a sheet 90% alpha-0 over perfectly good colour.
// Scaling the colour and the alpha separately keeps the colour lying under a transparent
// texel, which is the colour an opaque material goes on to draw
QImage scaleKeepingColourUnderAlpha(const QImage &source, int width, int height)
{
    const QImage straight = source.convertToFormat(QImage::Format_RGBA8888);
    const QImage colour = straight.convertToFormat(QImage::Format_RGB888)
                              .scaled(width, height, Qt::IgnoreAspectRatio,
                                      Qt::SmoothTransformation);
    QImage alpha(straight.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < straight.height(); ++y)
    {
        const uchar *in = straight.constScanLine(y);
        uchar *out = alpha.scanLine(y);
        for (int x = 0; x < straight.width(); ++x) {
            out[x] = in[x * 4 + 3];
        }
    }
    alpha = alpha.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage result(width, height, QImage::Format_RGBA8888);
    for (int y = 0; y < height; ++y)
    {
        const uchar *c = colour.constScanLine(y);
        const uchar *a = alpha.constScanLine(y);
        uchar *out = result.scanLine(y);
        for (int x = 0; x < width; ++x)
        {
            out[x * 4 + 0] = c[x * 3 + 0];
            out[x * 4 + 1] = c[x * 3 + 1];
            out[x * 4 + 2] = c[x * 3 + 2];
            out[x * 4 + 3] = a[x];
        }
    }
    return result;
}

// The mip chain already holds every smaller size, so a maximum size is applied by
// dropping the levels above it rather than rescaling: level 1 of a 4096 texture is
// exactly the 2048 image the GPU would have sampled from it anyway
int capTextureLevels(QVector<QImage> *levels, int maximumSize)
{
    if ((maximumSize <= 0) || levels->isEmpty()) {
        return 0;
    }
    int dropped = 0;
    while ((levels->size() > 1) &&
           (std::max(levels->first().width(), levels->first().height()) > maximumSize))
    {
        levels->removeFirst();
        ++dropped;
    }
    // A source with no mip levels to drop to is scaled instead, so the cap always holds
    QImage &top = (*levels)[0];
    const int longest = std::max(top.width(), top.height());
    if (longest > maximumSize)
    {
        top = scaleKeepingColourUnderAlpha(
            top, std::max(1, top.width() * maximumSize / longest),
            std::max(1, top.height() * maximumSize / longest));
        ++dropped;
    }
    return dropped;
}

QByteArray fileFingerprint(const QString &fileName, const QByteArray &context,
                           QString *error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        *error =
            QStringLiteral("Cannot hash %1: %2").arg(fileName, file.errorString());
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(context);
    if (!hash.addData(&file))
    {
        *error = QStringLiteral("Cannot hash %1").arg(fileName);
        return {};
    }
    return hash.result().toHex();
}

bool cancelled(const ConversionOptions &options, QString *error)
{
    if (options.m_isCancelled && options.m_isCancelled())
    {
        *error = QStringLiteral("Conversion cancelled");
        return true;
    }
    return false;
}

bool writeNeutralTexture(const ConversionOptions &options,
                         const QDir &outputDirectory,
                         const QDir &sharedTextureDirectory,
                         UastcTextureSemantic semantic, bool srgb, QRgb pixel,
                         QString *outputUri, bool *reused, QString *error)
{
    QByteArray identity = QByteArrayLiteral("MSFSAssetConverter-neutral-v2\0") +
                          QByteArray::number(static_cast<int>(semantic)) + ':' +
                          QByteArray::number(srgb) + ':' +
                          QByteArray::number(qRed(pixel)) + ':' +
                          QByteArray::number(qGreen(pixel)) + ':' +
                          QByteArray::number(qBlue(pixel)) + ':' +
                          QByteArray::number(qAlpha(pixel)) + ':' +
                          QByteArray::number(options.m_zstdSupercompression);
    const QByteArray fingerprint =
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex();
    const QString outputName = cachedKtxName(fingerprint);
    QString outputPath = outputDirectory.filePath(outputName);
    *outputUri = outputName;
    if (!options.m_sharedTextureDirectory.isEmpty())
    {
        outputPath = sharedTextureDirectory.absoluteFilePath(outputName);
        *outputUri = QDir::fromNativeSeparators(
            outputDirectory.relativeFilePath(outputPath));
    }

    const std::shared_ptr<std::mutex> outputLock = textureLock(outputPath);
    const std::lock_guard<std::mutex> outputGuard(*outputLock);
    *reused = reusableCachedKtx2(outputPath);
    if (*reused) {
        return true;
    }

    // 4x4, one whole block, rather than 1x1. Cesium transcodes UASTC to a GPU block
    // format, and a 1x1 base level is not whole blocks: in QtWebEngine it failed with
    // GL_INVALID_OPERATION "Invalid compressed image size"
    QImage neutral(4, 4, QImage::Format_RGBA8888);
    neutral.fill(pixel);
    if (!writeUastcKtx2({neutral}, outputPath, srgb, semantic, error,
                        options.m_isCancelled, options.m_zstdSupercompression)) {
        return false;
    }
    if (!writeKtxChecksum(outputPath, error) ||
        !reusableCachedKtx2(outputPath)) {
        *error = QStringLiteral("Generated neutral KTX2 texture failed validation: %1")
                     .arg(outputPath);
        return false;
    }
    return true;
}

// MSFS's Porthole shader blends its base color by alpha, so the window decal
// quads are declared OPAQUE. Core glTF ignores alpha for OPAQUE materials, which
// shows the quad's transparent white surround as a pale rectangle round each
// window.
int blendPortholeMaterials(QJsonObject *root)
{
    QJsonArray materials = root->value("materials").toArray();
    int count = 0;
    for (qsizetype i = 0; i < materials.size(); ++i)
    {
        QJsonObject material = materials.at(i).toObject();
        const QString code = material.value("extras")
                                 .toObject()
                                 .value("ASOBO_material_code")
                                 .toString();
        if (code.compare(QStringLiteral("Porthole"), Qt::CaseInsensitive) != 0 ||
            material.contains("alphaMode")) {
            continue;
        }
        material.insert("alphaMode", QStringLiteral("BLEND"));
        materials[i] = material;
        ++count;
    }
    if (count > 0) {
        root->insert("materials", materials);
    }
    return count;
}

// Asobo's "optimized" assets give every primitive of a mesh the one index accessor for the
// whole mesh, and say which part of it each draws only in their extras:
// ASOBO_primitive {StartIndex, PrimitiveCount (triangles), BaseVertexIndex}. Core glTF
// knows nothing of that, so each primitive drew every triangle of the mesh - the CJ4 had its
// placard atlas and registration over the whole airframe. Every Asobo aircraft in FSLTL is
// built this way, and the Superjet. Gives each such primitive an accessor for its own range,
// with the base vertex added in where there is one. Returns how many primitives were split
int splitSharedIndices(QJsonObject *root, QVector<QByteArray> *buffers, QString *warning)
{
    QJsonArray meshes = root->value("meshes").toArray();
    QJsonArray accessors = root->value("accessors").toArray();
    QJsonArray bufferViews = root->value("bufferViews").toArray();
    QHash<QString, int> made;   // Range to the accessor made for it, so none is flipped twice
    int split = 0;
    for (qsizetype m = 0; m < meshes.size(); m++)
    {
        QJsonObject mesh = meshes.at(m).toObject();
        QJsonArray primitives = mesh.value("primitives").toArray();
        bool changed = false;
        for (qsizetype p = 0; p < primitives.size(); p++)
        {
            QJsonObject primitive = primitives.at(p).toObject();
            const QJsonObject asobo = primitive.value("extras").toObject().value("ASOBO_primitive").toObject();
            const int accessorIndex = primitive.value("indices").toInt(-1);
            if (asobo.isEmpty() || !asobo.contains("PrimitiveCount") || (accessorIndex < 0)
                || (accessorIndex >= accessors.size())) {
                continue;
            }
            const QJsonObject accessor = accessors.at(accessorIndex).toObject();
            const qint64 total = static_cast<qint64>(accessor.value("count").toDouble());
            const qint64 start = static_cast<qint64>(asobo.value("StartIndex").toDouble());
            const qint64 count = static_cast<qint64>(asobo.value("PrimitiveCount").toDouble()) * 3;
            const qint64 base = static_cast<qint64>(asobo.value("BaseVertexIndex").toDouble());
            if ((start == 0) && (count == total) && (base == 0)) {
                continue;   // Already its own
            }
            const int componentType = accessor.value("componentType").toInt();
            const int bytes = (componentType == ComponentUnsignedByte) ? 1
                            : (componentType == ComponentUnsignedShort) ? 2
                            : (componentType == ComponentUnsignedInt) ? 4 : 0;
            if ((bytes == 0) || (start < 0) || (count <= 0) || (start + count > total) || accessor.contains("sparse"))
            {
                *warning = QStringLiteral("index range %1+%2 of %3 in mesh %4 left as it is")
                    .arg(start).arg(count).arg(total).arg(mesh.value("name").toString());
                continue;
            }
            const QString key = QStringLiteral("%1/%2/%3/%4").arg(accessorIndex).arg(start).arg(count).arg(base);
            int newIndex = made.value(key, -1);
            if (newIndex < 0)
            {
                const QJsonObject view = bufferViews.at(accessor.value("bufferView").toInt(-1)).toObject();
                const int stride = view.value("byteStride").toInt(bytes);
                if (base == 0)
                {
                    // The same data, from further in
                    QJsonObject range = accessor;
                    range.remove("min");
                    range.remove("max");
                    range.insert("byteOffset", accessor.value("byteOffset").toDouble() + static_cast<double>(start) * stride);
                    range.insert("count", count);
                    accessors.append(range);
                }
                else
                {
                    // The base vertex has to be added in, so a copy, as 32 bit
                    const int bufferIndex = view.value("buffer").toInt(-1);
                    if ((bufferIndex < 0) || (bufferIndex >= buffers->size())) {
                        continue;
                    }
                    const QByteArray &source = buffers->at(bufferIndex);
                    const qint64 offset = static_cast<qint64>(view.value("byteOffset").toDouble()
                                                              + accessor.value("byteOffset").toDouble());
                    if (offset + (start + count - 1) * stride + bytes > source.size()) {
                        continue;
                    }
                    QByteArray data(count * 4, '\0');
                    for (qint64 i = 0; i < count; i++)
                    {
                        quint32 value = 0;
                        std::memcpy(&value, source.constData() + offset + (start + i) * stride, bytes);
                        value = qFromLittleEndian(value) + static_cast<quint32>(base);
                        value = qToLittleEndian(value);
                        std::memcpy(data.data() + i * 4, &value, 4);
                    }
                    QByteArray &target = (*buffers)[0];
                    while (target.size() % 4) {
                        target.append('\0');
                    }
                    const qint64 targetOffset = target.size();
                    target.append(data);
                    bufferViews.append(QJsonObject{{"buffer", 0}, {"byteOffset", targetOffset}, {"byteLength", data.size()}});
                    accessors.append(QJsonObject{{"bufferView", bufferViews.size() - 1}, {"componentType", ComponentUnsignedInt},
                                                 {"count", count}, {"type", "SCALAR"}});
                }
                newIndex = static_cast<int>(accessors.size() - 1);
                made.insert(key, newIndex);
            }
            primitive.insert("indices", newIndex);
            primitives[p] = primitive;
            changed = true;
            split++;
        }
        if (changed)
        {
            mesh.insert("primitives", primitives);
            meshes[m] = mesh;
        }
    }
    root->insert("meshes", meshes);
    root->insert("accessors", accessors);
    root->insert("bufferViews", bufferViews);
    return split;
}

} // namespace

bool convertMsfsAsset(const ConversionOptions &options,
                      ConversionResult *result, QString *error,
                      const ConversionLog &log)
{
    *result = {};
    const QFileInfo requestedInput(options.m_inputGltf);
    if (!requestedInput.exists() || !requestedInput.isFile())
    {
        *error = QStringLiteral("Input does not exist: %1").arg(options.m_inputGltf);
        return false;
    }
    QFileInfo inputInfo = requestedInput;
    QString outputBaseName = inputInfo.completeBaseName();
    QStringList textureDirectories;
    QString modelXml;
    if (requestedInput.fileName().compare(QStringLiteral("aircraft.cfg"),
                                          Qt::CaseInsensitive) == 0) {
        ResolvedFsltlAircraft aircraft;
        if (!resolveFsltlAircraft(requestedInput.absoluteFilePath(),
                                  options.m_flightSimIndex, options.m_lodIndex,
                                  &aircraft, error)) {
            return false;
        }
        inputInfo = QFileInfo(aircraft.m_gltfFile);
        modelXml = aircraft.m_modelXml;
        outputBaseName = aircraft.m_outputBaseName;
        textureDirectories = aircraft.m_textureDirectories;
        result->m_skippedFallbackDirectories = aircraft.m_warnings.size();
        emitLog(log, QStringLiteral("Resolved aircraft hierarchy: %1")
                         .arg(aircraft.m_aircraftHierarchy.join(" -> ")));
        emitLog(log, QStringLiteral("Selected LOD %1 model: %2")
                         .arg(options.m_lodIndex)
                         .arg(inputInfo.absoluteFilePath()));
        emitLog(log, QStringLiteral("Texture search order: %1")
                         .arg(textureDirectories.join(" -> ")));
        for (const QString &warning : aircraft.m_warnings) {
            emitLog(log, QStringLiteral("WARNING: %1").arg(warning));
        }
    }
    QDir outputDirectory(options.m_outputDirectory);
    if (!outputDirectory.exists() &&
        !QDir().mkpath(outputDirectory.absolutePath())) {
        *error = QStringLiteral("Cannot create output directory: %1")
                     .arg(outputDirectory.absolutePath());
        return false;
    }
    QDir sharedTextureDirectory(options.m_sharedTextureDirectory);
    if (!options.m_sharedTextureDirectory.isEmpty() &&
        !sharedTextureDirectory.exists() &&
        !QDir().mkpath(sharedTextureDirectory.absolutePath())) {
        *error = QStringLiteral("Cannot create shared texture directory: %1")
                     .arg(sharedTextureDirectory.absolutePath());
        return false;
    }

    QByteArray jsonBytes;
    if (!readFile(inputInfo.absoluteFilePath(), &jsonBytes, error)) {
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(jsonBytes, &parseError);
    if (document.isNull() || !document.isObject())
    {
        *error = QStringLiteral("Invalid glTF JSON at byte %1: %2")
                     .arg(parseError.offset)
                     .arg(parseError.errorString());
        return false;
    }
    QJsonObject root = document.object();
    emitLog(log, QStringLiteral("Loaded %1").arg(inputInfo.fileName()));

    QJsonArray bufferObjects = root.value("buffers").toArray();
    QVector<QByteArray> buffers;
    buffers.reserve(bufferObjects.size());
    for (qsizetype i = 0; i < bufferObjects.size(); ++i)
    {
        if (cancelled(options, error))
            return false;
        const QString uri = bufferObjects.at(i).toObject().value("uri").toString();
        if (uri.isEmpty() || uri.startsWith("data:"))
        {
            *error =
                QStringLiteral("Prototype only supports external binary buffers");
            return false;
        }
        QString relative;
        if (!safeRelativePath(uri, &relative, error))
            return false;
        QByteArray bytes;
        const QString bufferFile = containedFile(inputInfo.dir(), relative);
        if (bufferFile.isEmpty())
        {
            *error = QStringLiteral(
                         "Buffer URI does not resolve inside the model directory: %1")
                         .arg(uri);
            return false;
        }
        if (!readFile(bufferFile, &bytes, error)) {
            return false;
        }
        buffers.append(std::move(bytes));
    }

    QString splitWarning;
    const int splitPrimitives = splitSharedIndices(&root, &buffers, &splitWarning);
    if (splitPrimitives > 0) {
        emitLog(log, QStringLiteral("Gave %1 primitives sharing an index accessor their own range")
                         .arg(splitPrimitives));
    }
    if (!splitWarning.isEmpty()) {
        emitLog(log, QStringLiteral("WARNING: %1").arg(splitWarning));
    }

    QJsonArray accessors = root.value("accessors").toArray();
    QJsonArray bufferViews = root.value("bufferViews").toArray();
    QHash<int, AttributeKind> conversions;
    if (!identifyConversions(root.value("meshes").toArray(), accessors, &conversions,
                             error)) {
        return false;
    }
    QList<int> accessorIndices = conversions.keys();
    std::sort(accessorIndices.begin(), accessorIndices.end());
    for (int accessorIndex : accessorIndices)
    {
        if (cancelled(options, error))
            return false;
        if (accessorIndex >= accessors.size())
        {
            *error = QStringLiteral("Mesh references missing accessor %1")
                         .arg(accessorIndex);
            return false;
        }
        if (!convertAccessor(accessorIndex, conversions.value(accessorIndex),
                             &accessors, &bufferViews, &buffers, error)) {
            return false;
        }
        ++result->m_convertedAccessors;
    }
    root.insert("accessors", accessors);
    root.insert("bufferViews", bufferViews);
    emitLog(log, QStringLiteral("Expanded %1 packed vertex accessors to float32")
                     .arg(result->m_convertedAccessors));

    if (!flipTriangleWinding(root.value("meshes").toArray(), accessors,
                             bufferViews, &buffers, &result->m_flippedTriangles,
                             error)) {
        return false;
    }
    emitLog(log,
            QStringLiteral("Reversed %1 triangle windings for glTF front faces")
                .arg(result->m_flippedTriangles));

    // Gear, flaps, slats and engines under the names the ADS-B demodulator plays. Only
    // the model XML says which MSFS animation is which, so a bare glTF keeps its own
    if (!modelXml.isEmpty())
    {
        QHash<QString, MsfsAnimationRole> roles;
        QString animationError;
        if (!readMsfsAnimationRoles(modelXml, &roles, &animationError)) {
            emitLog(log, QStringLiteral("WARNING: animations not mapped: %1").arg(animationError));
        } else {
            for (const QString &made : mapMsfsAnimations(roles, &root, &buffers)) {
                emitLog(log, QStringLiteral("Animation %1").arg(made));
            }
        }
    }

    // Whatever is left is either played or dead weight, and what is played was baked at a
    // keyframe per simulation frame
    const QString unplayed = dropUnplayedAnimations(&root);
    if (!unplayed.isEmpty()) {
        emitLog(log, QStringLiteral("Unplayed animations: %1").arg(unplayed));
    }
    const QString thinned = decimateAnimations(&root, &buffers, options.m_animationTolerance);
    if (!thinned.isEmpty()) {
        emitLog(log, QStringLiteral("Thinned animation %1").arg(thinned));
    }

    // What MSFS only shows some of the time, and Cesium would show all of it
    const QString hidden = dropHiddenParts(&root, modelXml.isEmpty() ? QSet<QString>()
                                                                     : readMsfsNeverVisibleNodes(modelXml));
    if (!hidden.isEmpty()) {
        emitLog(log, QStringLiteral("Hidden parts: %1").arg(hidden));
    }

    // Last of the node passes, so it also clears up what dropHiddenParts left behind
    const QString stripped = stripRedundantNodes(&root);
    if (!stripped.isEmpty()) {
        emitLog(log, QStringLiteral("Stripped %1").arg(stripped));
    }

    const int portholes = blendPortholeMaterials(&root);
    if (portholes > 0) {
        emitLog(log, QStringLiteral("Set alpha blending on %1 porthole materials")
                         .arg(portholes));
    }
    const QString mirroredWindows = mirrorInsetPortholes(&root, &buffers);
    if (!mirroredWindows.isEmpty()) {
        emitLog(log, QStringLiteral("Cabin %1").arg(mirroredWindows));
    }

    // After the passes that look primitives up by material, since this concatenates them.
    // Fewer draw commands, and it empties the nodes the merged geometry came from, so the
    // node strip runs once more to clear those away
    const QString mergedPrimitives = mergeStaticPrimitives(&root, &buffers);
    if (!mergedPrimitives.isEmpty())
    {
        emitLog(log, QStringLiteral("Merged %1").arg(mergedPrimitives));
        const QString restripped = stripRedundantNodes(&root);
        if (!restripped.isEmpty()) {
            emitLog(log, QStringLiteral("Stripped %1").arg(restripped));
        }
    }

    QJsonArray textures = root.value("textures").toArray();
    QSet<int> normalImages;
    QSet<int> linearImages;
    QSet<int> colorImages;
    QSet<int> emissiveImages;
    QSet<int> opaqueBaseColorImages;
    QSet<int> transparentBaseColorImages;
    for (const QJsonValue &materialValue : root.value("materials").toArray())
    {
        const QJsonObject material = materialValue.toObject();
        const int normalTexture =
            material.value("normalTexture").toObject().value("index").toInt(-1);
        if (normalTexture >= 0 && normalTexture < textures.size()) {
            const int imageIndex =
                textureSource(textures.at(normalTexture).toObject());
            if (imageIndex >= 0)
            {
                normalImages.insert(imageIndex);
                linearImages.insert(imageIndex);
            }
        }
        const QJsonObject pbr = material.value("pbrMetallicRoughness").toObject();
        const int baseColorTexture =
            pbr.value("baseColorTexture").toObject().value("index").toInt(-1);
        if (baseColorTexture >= 0 && baseColorTexture < textures.size()) {
            const int imageIndex =
                textureSource(textures.at(baseColorTexture).toObject());
            if (imageIndex >= 0)
            {
                colorImages.insert(imageIndex);
                const QString alphaMode =
                    material.value("alphaMode").toString(QStringLiteral("OPAQUE"));
                if (alphaMode.compare(QStringLiteral("OPAQUE"),
                                      Qt::CaseInsensitive) == 0) {
                    opaqueBaseColorImages.insert(imageIndex);
                } else {
                    transparentBaseColorImages.insert(imageIndex);
                }
            }
        }
        const int emissiveTexture =
            material.value("emissiveTexture").toObject().value("index").toInt(-1);
        if (emissiveTexture >= 0 && emissiveTexture < textures.size()) {
            const int imageIndex =
                textureSource(textures.at(emissiveTexture).toObject());
            if (imageIndex >= 0)
            {
                colorImages.insert(imageIndex);
                emissiveImages.insert(imageIndex);
            }
        }
        const QList<int> linearTextures{
            pbr.value("metallicRoughnessTexture")
                .toObject()
                .value("index")
                .toInt(-1),
            material.value("occlusionTexture").toObject().value("index").toInt(-1)};
        for (int textureIndex : linearTextures)
        {
            if (textureIndex >= 0 && textureIndex < textures.size()) {
                const int imageIndex =
                    textureSource(textures.at(textureIndex).toObject());
                if (imageIndex >= 0) {
                    linearImages.insert(imageIndex);
                }
            }
        }
    }
    for (int imageIndex : colorImages)
    {
        if (linearImages.contains(imageIndex))
        {
            *error = QStringLiteral(
                         "Image %1 is reused by both color and linear/normal texture "
                         "slots; "
                         "production conversion requires separate image sources")
                         .arg(imageIndex);
            return false;
        }
    }

    QJsonArray images = root.value("images").toArray();
    QSet<int> convertedImages;
    for (qsizetype i = 0; i < images.size(); ++i)
    {
        if (cancelled(options, error))
            return false;
        QJsonObject imageObject = images.at(i).toObject();
        const QString uri = imageObject.value("uri").toString();
        if (uri.isEmpty() || uri.startsWith(QStringLiteral("data:"),
                                            Qt::CaseInsensitive)) {
            continue;
        }
        bool ddsSource = uri.endsWith(".dds", Qt::CaseInsensitive);
        QString sourceTexture =
            findTextureFile(uri, inputInfo.dir(), textureDirectories, error);
        if (sourceTexture.isEmpty() && error->isEmpty() && !ddsSource)
        {
            // Some MSFS glTF files expose the fallback image as foo.png even though
            // only foo.png.DDS is shipped alongside the aircraft.
            sourceTexture = findTextureFile(uri + QStringLiteral(".DDS"),
                                            inputInfo.dir(), textureDirectories,
                                            error);
            ddsSource = !sourceTexture.isEmpty();
        }
        if (sourceTexture.isEmpty())
        {
            if (!error->isEmpty()) {
                return false;
            }

            const int imageIndex = static_cast<int>(i);
            // An opaque colour texture that can't be found is normally a broken livery,
            // and failing it lets ADS-B use a livery that works. But the Asobo aircraft
            // FSLTL reuses also draw small parts - wipers, propeller blur, a far LOD's
            // atlas - from the sim's shared texture folders, which their texture.cfg
            // falls back to and FSLTL does not ship. Where one of those was missing, that
            // is where the texture would have been, so the part is drawn plain grey
            // rather than the King Air 350 and C152 not being converted at all
            const bool missingFromFallback = opaqueBaseColorImages.contains(imageIndex)
                && (result->m_skippedFallbackDirectories > 0);
            if (opaqueBaseColorImages.contains(imageIndex) && !missingFromFallback)
            {
                *error = QStringLiteral(
                             "Cannot resolve required opaque base-color texture through "
                             "the fallback hierarchy: %1")
                             .arg(uri);
                return false;
            }

            const QString upperUri = uri.toUpper();
            const bool isNormal = normalImages.contains(imageIndex) ||
                                  upperUri.contains(QStringLiteral("NORMAL")) ||
                                  upperUri.contains(QStringLiteral("_NORM"));
            const bool isLinear = linearImages.contains(imageIndex) || isNormal ||
                                  upperUri.contains(QStringLiteral("OCCLUSION")) ||
                                  upperUri.contains(QStringLiteral("ROUGHNESS")) ||
                                  upperUri.contains(QStringLiteral("METALLIC")) ||
                                  upperUri.contains(QStringLiteral("_COMP")) ||
                                  upperUri.contains(QStringLiteral("_MASK"));
            const bool srgb = !isLinear;
            const UastcTextureSemantic semantic =
                isNormal ? UastcTextureSemantic::Normal
                         : (srgb ? UastcTextureSemantic::Color
                                 : UastcTextureSemantic::Data);
            QRgb neutralPixel = qRgba(255, 255, 255, 255);
            if (isNormal) {
                neutralPixel = qRgba(128, 128, 255, 255);
            } else if (isLinear) {
                // Neutral glTF ORM: full occlusion, full roughness, no metallic.
                neutralPixel = qRgba(255, 255, 0, 255);
            } else if (emissiveImages.contains(imageIndex)) {
                neutralPixel = qRgba(0, 0, 0, 255);
            } else if (missingFromFallback) {
                neutralPixel = qRgba(128, 128, 128, 255);
            } else if (transparentBaseColorImages.contains(imageIndex)) {
                // Missing BLEND/MASK base colors are normally optional overlays.
                neutralPixel = qRgba(255, 255, 255, 0);
            }

            QString neutralUri;
            bool reused = false;
            if (!writeNeutralTexture(options, outputDirectory,
                                     sharedTextureDirectory, semantic, srgb,
                                     neutralPixel, &neutralUri, &reused, error)) {
                return false;
            }
            imageObject.insert("uri", neutralUri);
            imageObject.remove("mimeType");
            images[i] = imageObject;
            convertedImages.insert(imageIndex);
            ++result->m_convertedTextures;
            ++result->m_synthesizedTextures;
            if (reused) {
                ++result->m_reusedTextures;
            }
            emitLog(log,
                    QStringLiteral("WARNING: Replaced missing optional texture %1 "
                                   "with a neutral 4x4 KTX2 texture")
                        .arg(uri));
            continue;
        }
        if (!ddsSource)
        {
            const QByteArray copyContext =
                QByteArrayLiteral("MSFSAssetConverter-image-copy-v1");
            const QByteArray fingerprint = fileFingerprint(
                sourceTexture, copyContext, error);
            if (fingerprint.isEmpty())
                return false;
            const QString outputName = cachedImageName(sourceTexture, fingerprint);
            QString outputPath = outputDirectory.filePath(outputName);
            QString outputUri = outputName;
            if (!options.m_sharedTextureDirectory.isEmpty())
            {
                outputPath = sharedTextureDirectory.absoluteFilePath(outputName);
                outputUri = QDir::fromNativeSeparators(
                    outputDirectory.relativeFilePath(outputPath));
            }
            const std::shared_ptr<std::mutex> outputLock = textureLock(outputPath);
            const std::lock_guard<std::mutex> outputGuard(*outputLock);
            if (reusableCopy(outputPath, sourceTexture, copyContext, fingerprint))
            {
                ++result->m_reusedTextures;
                emitLog(log, QStringLiteral("Reused cached image for %1").arg(uri));
            }
            else if (!copyTextureAtomically(sourceTexture, outputPath))
            {
                *error = QStringLiteral("Cannot copy external image %1 to %2")
                             .arg(sourceTexture, outputPath);
                return false;
            } else {
                emitLog(log, QStringLiteral("Copied external image %1").arg(uri));
            }
            imageObject.insert("uri", outputUri);
            images[i] = imageObject;
            continue;
        }

        const bool isNormal = normalImages.contains(static_cast<int>(i));
        const bool srgb = !linearImages.contains(static_cast<int>(i));
        const UastcTextureSemantic semantic =
            isNormal
                ? UastcTextureSemantic::Normal
                : (srgb ? UastcTextureSemantic::Color : UastcTextureSemantic::Data);
        const QByteArray fingerprint =
            fileFingerprint(sourceTexture,
                            // v3: a capped texture no longer loses the colour under
                            // its transparent texels, so everything v2 cached has to
                            // be encoded again
                            QByteArrayLiteral("MSFSAssetConverter-UASTC-v3\0") +
                                QByteArray::number(static_cast<int>(semantic)) +
                                QByteArray::number(options.m_flipDirectXNormalY) +
                                QByteArray::number(options.m_zstdSupercompression) +
                                QByteArray::number(options.m_maximumTextureSize),
                            error);
        if (fingerprint.isEmpty())
            return false;
        const QString outputName = cachedKtxName(fingerprint);
        QString outputPath = outputDirectory.filePath(outputName);
        QString outputUri = outputName;
        std::shared_ptr<std::mutex> sharedOutputLock;
        std::unique_lock<std::mutex> outputGuard;
        if (!options.m_sharedTextureDirectory.isEmpty())
        {
            outputPath = sharedTextureDirectory.absoluteFilePath(outputName);
            outputUri = QDir::fromNativeSeparators(
                QDir(outputDirectory.absolutePath()).relativeFilePath(outputPath));
            sharedOutputLock = textureLock(outputPath);
            outputGuard = std::unique_lock<std::mutex>(*sharedOutputLock);
            const QString legacyOutput = outputDirectory.filePath(
                legacyKtxNameForDds(uri, i, fingerprint));
            QString checksumError;
            if (!reusableCachedKtx2(outputPath) && reusableKtx2(legacyOutput) &&
                copyTextureAtomically(legacyOutput, outputPath) &&
                writeKtxChecksum(outputPath, &checksumError)) {
                emitLog(
                    log,
                    QStringLiteral("Copied existing KTX2 texture into the shared cache for %1")
                        .arg(uri));
            }
        }
        if (cancelled(options, error))
            return false;

        if (reusableCachedKtx2(outputPath))
        {
            ++result->m_reusedTextures;
            emitLog(log, QStringLiteral("Reused cached KTX2 texture for %1")
                             .arg(uri));
        }
        else
        {
            DdsDecodeResult decoded;
            if (!decodeDds(sourceTexture, isNormal, options.m_flipDirectXNormalY,
                           &decoded, error)) {
                return false;
            }
            const int droppedLevels =
                capTextureLevels(&decoded.m_levels, options.m_maximumTextureSize);
            if (droppedLevels > 0)
            {
                ++result->m_downscaledTextures;
                emitLog(log, QStringLiteral("Limited %1 to %2 by dropping %3 mip level(s)")
                                 .arg(uri)
                                 .arg(options.m_maximumTextureSize)
                                 .arg(droppedLevels));
            }
            emitLog(
                log,
                QStringLiteral("Writing %1 DDS mip levels from %2 as %3 UASTC KTX2")
                    .arg(decoded.m_levels.size())
                    .arg(uri,
                         srgb ? QStringLiteral("sRGB") : QStringLiteral("linear")));
            if (!writeUastcKtx2(decoded.m_levels, outputPath, srgb, semantic, error,
                                options.m_isCancelled,
                                options.m_zstdSupercompression)) {
                return false;
            }
            if (!writeKtxChecksum(outputPath, error) ||
                !reusableCachedKtx2(outputPath)) {
                *error = QStringLiteral("Generated KTX2 texture failed validation: %1")
                             .arg(outputPath);
                return false;
            }
            result->m_preservedMipLevels += decoded.m_levels.size();
            emitLog(log, QStringLiteral("Converted %1 from %2 (%3, %4 mip levels)%5")
                             .arg(uri, QFileInfo(sourceTexture).absolutePath(),
                                  decoded.m_format)
                             .arg(decoded.m_levels.size())
                             .arg(isNormal
                                      ? QStringLiteral(" as an OpenGL normal map")
                                      : QString()));
        }
        imageObject.insert("uri", outputUri);
        imageObject.remove("mimeType");
        images[i] = imageObject;
        convertedImages.insert(static_cast<int>(i));
        ++result->m_convertedTextures;
    }
    root.insert("images", images);

    for (qsizetype i = 0; i < textures.size(); ++i)
    {
        QJsonObject texture = textures.at(i).toObject();
        const int source = textureSource(texture);
        if (convertedImages.contains(source))
        {
            texture.remove("source");
            QJsonObject extensions = texture.value("extensions").toObject();
            extensions.insert("KHR_texture_basisu", QJsonObject{{"source", source}});
            texture.insert("extensions", extensions);
        } else if (source >= 0) {
            texture.insert("source", source);
        }
        textures[i] = texture;
    }
    root.insert("textures", textures);

    const QSet<QString> removedExtensions{
        "ASOBO_asset_optimized", "ASOBO_macro_light",
        "ASOBO_normal_map_convention", "MSFT_texture_dds"};
    const QJsonArray usedBeforeRemoval = root.value("extensionsUsed").toArray();
    if (usedBeforeRemoval.contains(QStringLiteral("ASOBO_macro_light")))
    {
        const QString message =
            QStringLiteral("ASOBO_macro_light is not representable in core glTF; "
                           "its payload will be dropped");
        if (options.m_strictExtensions)
        {
            *error = message;
            return false;
        }
        emitLog(log, QStringLiteral("WARNING: %1").arg(message));
    }
    root.insert("extensionsUsed",
                withoutExtensions(root.value("extensionsUsed").toArray(),
                                  removedExtensions));
    root.insert("extensionsRequired",
                withoutExtensions(root.value("extensionsRequired").toArray(),
                                  removedExtensions));
    QJsonValue rootValue(root);
    stripExtensions(&rootValue, removedExtensions);
    result->m_removedExtras = sanitizeExtras(&rootValue);
    root = rootValue.toObject();
    if (!convertedImages.isEmpty())
    {
        root.insert("extensionsUsed",
                    withExtension(root.value("extensionsUsed").toArray(),
                                  QStringLiteral("KHR_texture_basisu")));
        root.insert("extensionsRequired",
                    withExtension(root.value("extensionsRequired").toArray(),
                                  QStringLiteral("KHR_texture_basisu")));
    }
    if (root.value("extensionsUsed").toArray().isEmpty()) {
        root.remove("extensionsUsed");
    }
    if (root.value("extensionsRequired").toArray().isEmpty()) {
        root.remove("extensionsRequired");
    }
    emitLog(log, QStringLiteral("Removed unsupported MSFS extension payloads and "
                                "%1 non-object extras")
                     .arg(result->m_removedExtras));

    const QString baseName =
        outputBaseName + options.m_outputNameSuffix + QStringLiteral("_cesium") +
        options.m_outputFileSuffix;
    for (qsizetype i = 0; i < buffers.size(); ++i)
    {
        if (cancelled(options, error))
            return false;
        const QByteArray hash =
            QCryptographicHash::hash(buffers.at(i), QCryptographicHash::Sha256)
                .toHex()
                .left(12);
        const QString outputBufferName = QStringLiteral("%1_%2_%3.bin")
                                             .arg(baseName)
                                             .arg(i)
                                             .arg(QString::fromLatin1(hash));
        if (!writeFile(outputDirectory.filePath(outputBufferName), buffers.at(i),
                       error)) {
            return false;
        }
        QJsonObject bufferObject = bufferObjects.at(i).toObject();
        bufferObject.insert("uri", outputBufferName);
        bufferObject.insert("byteLength", buffers.at(i).size());
        bufferObjects[i] = bufferObject;
    }
    root.insert("buffers", bufferObjects);

    QJsonObject asset = root.value("asset").toObject();
    const QString generator = asset.value("generator").toString();
    asset.insert("generator", generator.isEmpty()
                                  ? QStringLiteral("MSFSAssetConverter")
                                  : generator + "; MSFSAssetConverter");
    root.insert("asset", asset);

    result->m_outputGltf = outputDirectory.filePath(baseName + ".gltf");
    if (!writeFile(result->m_outputGltf,
                   QJsonDocument(root).toJson(QJsonDocument::Indented), error)) {
        return false;
    }
    emitLog(log, QStringLiteral("Wrote %1").arg(result->m_outputGltf));
    return true;
}
