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

#include "ktx2writer.h"

#include <QByteArray>
#include <QSaveFile>
#include <QtEndian>

#ifdef MSFS_CONVERTER_WITH_ZSTD
#include <zstd.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

constexpr quint32 HeaderByteLength = 68;
constexpr quint32 LevelIndexEntryByteLength = 24;
constexpr quint32 DfdByteLength = 44;
constexpr quint32 UastcBlockByteLength = 16;
constexpr std::array<quint32, 4> UastcWeights{0, 21, 43, 64};
constexpr int Etc1Modifiers[8][4] = {
    {-8, -2, 2, 8}, {-17, -5, 5, 17}, {-29, -9, 9, 29}, {-42, -13, 13, 42}, {-60, -18, 18, 60}, {-80, -24, 24, 80}, {-106, -33, 33, 106}, {-183, -47, 47, 183}};
constexpr int EacModifiers[16][8] = {
    {-3, -6, -9, -15, 2, 5, 8, 14}, {-3, -7, -10, -13, 2, 6, 9, 12}, {-2, -5, -8, -13, 1, 4, 7, 12}, {-2, -4, -6, -13, 1, 3, 5, 12}, {-3, -6, -8, -12, 2, 5, 7, 11}, {-3, -7, -9, -11, 2, 6, 8, 10}, {-4, -7, -8, -11, 3, 6, 7, 10}, {-3, -5, -8, -11, 2, 4, 7, 10}, {-2, -6, -8, -10, 1, 5, 7, 9}, {-2, -5, -8, -10, 1, 4, 7, 9}, {-2, -4, -8, -10, 1, 3, 7, 9}, {-2, -5, -7, -10, 1, 4, 6, 9}, {-3, -4, -7, -10, 2, 3, 6, 9}, {-1, -2, -3, -10, 0, 1, 2, 9}, {-4, -6, -8, -9, 3, 5, 7, 8}, {-3, -5, -7, -9, 2, 4, 6, 8}};

using Rgba = std::array<quint8, 4>;
using PixelBlock = std::array<Rgba, 16>;

struct Etc1SolidHints {
    quint8 m_differential = 0;
    quint8 m_intensity = 0;
    quint8 m_selector = 0;
    Rgba m_base{};
};

struct Etc1BlockHints {
    quint8 m_flip = 0;
    quint8 m_differential = 0;
    quint8 m_intensity0 = 0;
    quint8 m_intensity1 = 0;
    quint8 m_bias = 13;
};

struct Etc1ChannelChoice {
    quint8 m_base = 0;
    quint16 m_squaredError = 0;
};

struct Etc1InverseTables {
    Etc1ChannelChoice m_choices[2][8][4][256]{};

    Etc1InverseTables()
    {
        for (int differential = 0; differential < 2; ++differential) {
            const int limit = differential ? 32 : 16;
            for (int intensity = 0; intensity < 8; ++intensity)
            {
                for (int selector = 0; selector < 4; ++selector)
                {
                    for (int color = 0; color < 256; ++color) {
                        int bestError = std::numeric_limits<int>::max();
                        for (int base = 0; base < limit; ++base)
                        {
                            const int expanded = differential ? ((base << 3) | (base >> 2))
                                                              : ((base << 4) | base);
                            const int decoded = std::clamp(
                                expanded + Etc1Modifiers[intensity][selector], 0, 255);
                            const int delta = decoded - color;
                            const int error = delta * delta;
                            if (error < bestError)
                            {
                                bestError = error;
                                m_choices[differential][intensity][selector][color] = {
                                    static_cast<quint8>(base), static_cast<quint16>(error)};
                            }
                        }
                    }
                }
            }
        }
    }
};

const Etc1InverseTables &etc1InverseTables()
{
    static const Etc1InverseTables tables;
    return tables;
}

void appendU8(QByteArray *output, quint8 value)
{
    output->append(static_cast<char>(value));
}

void appendU16(QByteArray *output, quint16 value)
{
    const quint16 littleEndian = qToLittleEndian(value);
    output->append(reinterpret_cast<const char *>(&littleEndian),
                   sizeof(littleEndian));
}

void appendU32(QByteArray *output, quint32 value)
{
    const quint32 littleEndian = qToLittleEndian(value);
    output->append(reinterpret_cast<const char *>(&littleEndian),
                   sizeof(littleEndian));
}

void appendU64(QByteArray *output, quint64 value)
{
    const quint64 littleEndian = qToLittleEndian(value);
    output->append(reinterpret_cast<const char *>(&littleEndian),
                   sizeof(littleEndian));
}

void appendKeyValue(QByteArray *output, const QByteArray &key,
                    const QByteArray &value)
{
    const quint32 payloadLength =
        static_cast<quint32>(key.size() + 1 + value.size() + 1);
    appendU32(output, payloadLength);
    output->append(key);
    output->append('\0');
    output->append(value);
    output->append('\0');
    while (output->size() % 4 != 0) {
        output->append('\0');
    }
}

QByteArray createDfd(bool srgb, bool hasAlpha)
{
    QByteArray dfd;
    dfd.reserve(DfdByteLength);
    appendU32(&dfd, DfdByteLength);
    appendU16(&dfd, 0);           // KHR_DF_VENDORID_KHRONOS
    appendU16(&dfd, 0);           // KHR_DF_KHR_DESCRIPTORTYPE_BASICFORMAT
    appendU16(&dfd, 2);           // KHR_DF_VERSIONNUMBER_1_3
    appendU16(&dfd, 40);          // Descriptor block size
    appendU8(&dfd, 166);          // KHR_DF_MODEL_UASTC
    appendU8(&dfd, srgb ? 1 : 0); // BT709 for sRGB, unspecified for linear
    appendU8(&dfd, srgb ? 2 : 1); // sRGB or linear transfer
    appendU8(&dfd, 0);            // Straight alpha
    appendU8(&dfd, 3);            // Four texels in X, stored as dimension minus one
    appendU8(&dfd, 3);            // Four texels in Y
    appendU8(&dfd, 0);
    appendU8(&dfd, 0);
    appendU8(&dfd, UastcBlockByteLength);
    for (int plane = 1; plane < 8; ++plane) {
        appendU8(&dfd, 0);
    }

    appendU16(&dfd, 0);
    appendU8(&dfd, 127);              // One sample covers the complete 128-bit block
    appendU8(&dfd, hasAlpha ? 3 : 4); // UASTC RGBA or RGB
    for (int position = 0; position < 4; ++position) {
        appendU8(&dfd, 0);
    }
    appendU32(&dfd, 0);
    appendU32(&dfd, 0xFFFFFFFFU);
    return dfd;
}

void writeBits(quint8 *block, int *bitOffset, quint32 value, int bitCount)
{
    for (int bit = 0; bit < bitCount; ++bit)
    {
        if ((value & (quint32(1) << bit)) != 0) {
            block[*bitOffset / 8] |= quint8(1U << (*bitOffset % 8));
        }
        ++*bitOffset;
    }
}

quint8 interpolate(quint8 low, quint8 high, quint32 weight)
{
    return static_cast<quint8>((low * (64U - weight) + high * weight + 32U) >>
                               6U);
}

quint8 clampByte(int value)
{
    return static_cast<quint8>(std::clamp(value, 0, 255));
}

quint64 rgbError(const Rgba &left, const Rgba &right)
{
    quint64 error = 0;
    for (int channel = 0; channel < 3; ++channel)
    {
        const qint64 delta = qint64(left[channel]) - right[channel];
        error += static_cast<quint64>(delta * delta);
    }
    return error;
}

quint64 pixelError(const Rgba &left, const Rgba &right,
                   UastcTextureSemantic semantic)
{
    if (semantic == UastcTextureSemantic::Normal)
    {
        auto normal = [](const Rgba &pixel) {
            std::array<double, 3> value{pixel[0] / 127.5 - 1.0,
                                        pixel[1] / 127.5 - 1.0,
                                        pixel[2] / 127.5 - 1.0};
            const double length = std::sqrt(
                value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
            if (length > 1.0e-8)
            {
                for (double &component : value) {
                    component /= length;
                }
            }
            return value;
        };
        const auto leftNormal = normal(left);
        const auto rightNormal = normal(right);
        const double dot = std::clamp(leftNormal[0] * rightNormal[0] +
                                          leftNormal[1] * rightNormal[1] +
                                          leftNormal[2] * rightNormal[2],
                                      -1.0, 1.0);
        const qint64 alphaDelta = qint64(left[3]) - right[3];
        return static_cast<quint64>(std::lround((1.0 - dot) * 260100.0)) +
               static_cast<quint64>(alphaDelta * alphaDelta);
    }

    constexpr std::array<quint64, 4> ColorWeights{3, 6, 1, 4};
    quint64 error = 0;
    for (int channel = 0; channel < 4; ++channel)
    {
        const qint64 delta = qint64(left[channel]) - right[channel];
        const quint64 weight =
            semantic == UastcTextureSemantic::Color ? ColorWeights[channel] : 1;
        error += static_cast<quint64>(delta * delta) * weight;
    }
    return error;
}

quint64 chooseSelectors(const PixelBlock &pixels, const Rgba &low,
                        const Rgba &high, std::array<quint8, 16> *selectors,
                        UastcTextureSemantic semantic)
{
    std::array<Rgba, 4> palette{};
    for (std::size_t selector = 0; selector < palette.size(); ++selector)
    {
        for (int channel = 0; channel < 4; ++channel) {
            palette[selector][channel] =
                interpolate(low[channel], high[channel], UastcWeights[selector]);
        }
    }

    quint64 totalError = 0;
    for (std::size_t pixel = 0; pixel < pixels.size(); ++pixel)
    {
        quint64 bestError = std::numeric_limits<quint64>::max();
        quint8 bestSelector = 0;
        for (std::size_t selector = 0; selector < palette.size(); ++selector) {
            const quint64 error =
                pixelError(pixels[pixel], palette[selector], semantic);
            if (error < bestError)
            {
                bestError = error;
                bestSelector = static_cast<quint8>(selector);
            }
        }
        (*selectors)[pixel] = bestSelector;
        totalError += bestError;
    }
    return totalError;
}

void optimizeEndpoints(const PixelBlock &pixels,
                       const std::array<quint8, 16> &selectors, Rgba *low,
                       Rgba *high)
{
    double lowLow = 0.0;
    double lowHigh = 0.0;
    double highHigh = 0.0;
    for (const quint8 selector : selectors)
    {
        const double highWeight = UastcWeights[selector] / 64.0;
        const double lowWeight = 1.0 - highWeight;
        lowLow += lowWeight * lowWeight;
        lowHigh += lowWeight * highWeight;
        highHigh += highWeight * highWeight;
    }
    const double determinant = lowLow * highHigh - lowHigh * lowHigh;
    if (std::abs(determinant) < 1.0e-8) {
        return;
    }

    for (int channel = 0; channel < 4; ++channel)
    {
        double lowTarget = 0.0;
        double highTarget = 0.0;
        for (std::size_t pixel = 0; pixel < pixels.size(); ++pixel)
        {
            const double highWeight = UastcWeights[selectors[pixel]] / 64.0;
            const double lowWeight = 1.0 - highWeight;
            lowTarget += lowWeight * pixels[pixel][channel];
            highTarget += highWeight * pixels[pixel][channel];
        }
        const double optimizedLow =
            (lowTarget * highHigh - highTarget * lowHigh) / determinant;
        const double optimizedHigh =
            (highTarget * lowLow - lowTarget * lowHigh) / determinant;
        (*low)[channel] =
            static_cast<quint8>(std::clamp(std::lround(optimizedLow), 0L, 255L));
        (*high)[channel] =
            static_cast<quint8>(std::clamp(std::lround(optimizedHigh), 0L, 255L));
    }
}

Etc1SolidHints makeEtc1SolidHints(const Rgba &color)
{
    const Etc1InverseTables &tables = etc1InverseTables();
    Etc1SolidHints best;
    quint64 bestError = std::numeric_limits<quint64>::max();
    for (quint8 differential = 0; differential < 2; ++differential)
    {
        for (quint8 intensity = 0; intensity < 8; ++intensity)
        {
            for (quint8 selector = 0; selector < 4; ++selector)
            {
                Rgba bases{};
                quint64 error = 0;
                for (int channel = 0; channel < 3; ++channel)
                {
                    const Etc1ChannelChoice &choice =
                        tables.m_choices[differential][intensity][selector][color[channel]];
                    bases[channel] = choice.m_base;
                    error += choice.m_squaredError;
                }
                if (error < bestError)
                {
                    bestError = error;
                    best.m_differential = differential;
                    best.m_intensity = intensity;
                    best.m_selector = selector;
                    best.m_base = bases;
                }
            }
        }
    }
    return best;
}

Etc1BlockHints makeEtc1BlockHints(const PixelBlock &pixels)
{
    Etc1BlockHints best;
    quint64 bestError = std::numeric_limits<quint64>::max();
    for (quint8 flip = 0; flip < 2; ++flip)
    {
        quint64 totalError = 0;
        std::array<quint8, 2> intensities{};
        for (int subset = 0; subset < 2; ++subset)
        {
            std::array<int, 3> sums{};
            std::array<int, 8> indices{};
            int count = 0;
            for (int y = 0; y < 4; ++y)
            {
                for (int x = 0; x < 4; ++x)
                {
                    const int pixelSubset = flip ? y / 2 : x / 2;
                    if (pixelSubset != subset) {
                        continue;
                    }
                    const int index = y * 4 + x;
                    indices[count++] = index;
                    for (int channel = 0; channel < 3; ++channel) {
                        sums[channel] += pixels[index][channel];
                    }
                }
            }

            Rgba base{};
            for (int channel = 0; channel < 3; ++channel)
            {
                const int packed = (sums[channel] * 15 + 1020) / (8 * 255);
                base[channel] = static_cast<quint8>((packed << 4) | packed);
            }

            quint64 bestSubsetError = std::numeric_limits<quint64>::max();
            quint8 bestIntensity = 0;
            for (quint8 intensity = 0; intensity < 8; ++intensity) {
                quint64 error = 0;
                for (int i = 0; i < count; ++i)
                {
                    quint64 bestPixelError = std::numeric_limits<quint64>::max();
                    for (int selector = 0; selector < 4; ++selector)
                    {
                        Rgba decoded{};
                        for (int channel = 0; channel < 3; ++channel) {
                            decoded[channel] =
                                clampByte(base[channel] + Etc1Modifiers[intensity][selector]);
                        }
                        bestPixelError =
                            std::min(bestPixelError, rgbError(pixels[indices[i]], decoded));
                    }
                    error += bestPixelError;
                }
                if (error < bestSubsetError)
                {
                    bestSubsetError = error;
                    bestIntensity = intensity;
                }
            }
            intensities[subset] = bestIntensity;
            totalError += bestSubsetError;
        }
        if (totalError < bestError)
        {
            bestError = totalError;
            best.m_flip = flip;
            best.m_intensity0 = intensities[0];
            best.m_intensity1 = intensities[1];
        }
    }
    return best;
}

quint8 makeEacAlphaHints(const PixelBlock &pixels)
{
    int minimum = 255;
    int maximum = 0;
    for (const Rgba &pixel : pixels)
    {
        minimum = std::min(minimum, static_cast<int>(pixel[3]));
        maximum = std::max(maximum, static_cast<int>(pixel[3]));
    }
    if (minimum == maximum) {
        return 0x1D; // Multiplier 1 and the reference solid-alpha table 13.
    }

    quint64 bestError = std::numeric_limits<quint64>::max();
    quint8 bestTable = 0;
    quint8 bestMultiplier = 1;
    for (quint8 table = 0; table < 16; ++table)
    {
        const int modifierRange = EacModifiers[table][7] - EacModifiers[table][3];
        const int estimatedMultiplier = static_cast<int>(
            std::lround(double(maximum - minimum) / modifierRange));
        for (int candidate = estimatedMultiplier - 1;
             candidate <= estimatedMultiplier + 1; ++candidate) {
            const int multiplier = std::clamp(candidate, 1, 15);
            const double blend = double(-EacModifiers[table][3]) / modifierRange;
            const int base = std::clamp(
                static_cast<int>(std::lround(minimum + (maximum - minimum) * blend)),
                0, 255);
            quint64 error = 0;
            for (const Rgba &pixel : pixels)
            {
                int bestPixelError = std::numeric_limits<int>::max();
                for (int selector = 0; selector < 8; ++selector)
                {
                    const int decoded = std::clamp(
                        base + EacModifiers[table][selector] * multiplier, 0, 255);
                    const int delta = decoded - pixel[3];
                    bestPixelError = std::min(bestPixelError, delta * delta);
                }
                error += static_cast<quint64>(bestPixelError);
            }
            if (error < bestError)
            {
                bestError = error;
                bestTable = table;
                bestMultiplier = static_cast<quint8>(multiplier);
            }
        }
    }
    return static_cast<quint8>((bestMultiplier << 4) | bestTable);
}

QByteArray encodeSolidBlock(const Rgba &color)
{
    const Etc1SolidHints hints = makeEtc1SolidHints(color);
    quint8 block[UastcBlockByteLength]{};
    int bitOffset = 0;
    // UASTC mode 8 (solid RGBA) is the five-bit LSB-first Huffman code 0x17.
    writeBits(block, &bitOffset, 0x17, 5);
    for (const quint8 channel : color) {
        writeBits(block, &bitOffset, channel, 8);
    }
    writeBits(block, &bitOffset, hints.m_differential, 1);
    writeBits(block, &bitOffset, hints.m_intensity, 3);
    writeBits(block, &bitOffset, hints.m_selector, 2);
    for (int channel = 0; channel < 3; ++channel) {
        writeBits(block, &bitOffset, hints.m_base[channel], 5);
    }
    return QByteArray(reinterpret_cast<const char *>(block), sizeof(block));
}

QByteArray encodeInterpolatedBlock(const PixelBlock &pixels, quint64 solidError,
                                   UastcTextureSemantic semantic)
{
    Rgba low = pixels[0];
    Rgba high = pixels[0];
    quint64 greatestDistance = 0;
    for (std::size_t first = 0; first < pixels.size(); ++first)
    {
        for (std::size_t second = first + 1; second < pixels.size(); ++second) {
            const quint64 distance =
                pixelError(pixels[first], pixels[second], semantic);
            if (distance > greatestDistance)
            {
                greatestDistance = distance;
                low = pixels[first];
                high = pixels[second];
            }
        }
    }
    if (greatestDistance == 0) {
        return {};
    }

    std::array<quint8, 16> selectors{};
    for (int iteration = 0; iteration < 4; ++iteration)
    {
        chooseSelectors(pixels, low, high, &selectors, semantic);
        optimizeEndpoints(pixels, selectors, &low, &high);
    }
    const quint64 interpolatedError =
        chooseSelectors(pixels, low, high, &selectors, semantic);
    if (interpolatedError >= solidError) {
        return {};
    }

    // The first texel is UASTC's anchor and stores one fewer selector bit. Make
    // its top bit zero by reversing the palette without changing any colors.
    if ((selectors[0] & 2U) != 0) {
        std::swap(low, high);
        for (quint8 &selector : selectors) {
            selector = 3U - selector;
        }
    }

    PixelBlock decodedPixels{};
    for (std::size_t pixel = 0; pixel < pixels.size(); ++pixel)
    {
        for (int channel = 0; channel < 4; ++channel) {
            decodedPixels[pixel][channel] = interpolate(
                low[channel], high[channel], UastcWeights[selectors[pixel]]);
        }
    }
    const Etc1BlockHints etc1 = makeEtc1BlockHints(decodedPixels);
    const quint8 eacAlpha = makeEacAlphaHints(decodedPixels);

    quint8 block[UastcBlockByteLength]{};
    int bitOffset = 0;
    writeBits(block, &bitOffset, 0x0D, 5); // UASTC mode 14
    writeBits(block, &bitOffset, 0, 2);    // BC1 hints
    writeBits(block, &bitOffset, etc1.m_flip, 1);
    writeBits(block, &bitOffset, etc1.m_differential, 1);
    writeBits(block, &bitOffset, etc1.m_intensity0, 3);
    writeBits(block, &bitOffset, etc1.m_intensity1, 3);
    writeBits(block, &bitOffset, etc1.m_bias, 5);
    writeBits(block, &bitOffset, eacAlpha, 8);
    for (int channel = 0; channel < 4; ++channel)
    {
        writeBits(block, &bitOffset, low[channel], 8);
        writeBits(block, &bitOffset, high[channel], 8);
    }
    writeBits(block, &bitOffset, selectors[0], 1);
    for (std::size_t pixel = 1; pixel < selectors.size(); ++pixel) {
        writeBits(block, &bitOffset, selectors[pixel], 2);
    }
    return QByteArray(reinterpret_cast<const char *>(block), sizeof(block));
}

QByteArray encodeUastcRow(const QImage &image, int blockY,
                          UastcTextureSemantic semantic)
{
    QByteArray bytes;
    const int blocksWide = (image.width() + 3) / 4;
    bytes.reserve(static_cast<qsizetype>(blocksWide) * UastcBlockByteLength);

    for (int blockX = 0; blockX < blocksWide; ++blockX)
    {
        PixelBlock pixels{};
        quint32 sums[4]{};
        for (int localY = 0; localY < 4; ++localY)
        {
            const int y = qMin(blockY * 4 + localY, image.height() - 1);
            const auto *row =
                reinterpret_cast<const quint8 *>(image.constScanLine(y));
            for (int localX = 0; localX < 4; ++localX)
            {
                const int x = qMin(blockX * 4 + localX, image.width() - 1);
                Rgba &pixel = pixels[localY * 4 + localX];
                for (int channel = 0; channel < 4; ++channel)
                {
                    pixel[channel] = row[x * 4 + channel];
                    sums[channel] += pixel[channel];
                }
            }
        }

        Rgba average{};
        for (int channel = 0; channel < 4; ++channel) {
            average[channel] = static_cast<quint8>((sums[channel] + 8U) / 16U);
        }
        quint64 solidError = 0;
        for (const Rgba &pixel : pixels) {
            solidError += pixelError(pixel, average, semantic);
        }

        const QByteArray interpolated =
            encodeInterpolatedBlock(pixels, solidError, semantic);
        bytes.append(interpolated.isEmpty() ? encodeSolidBlock(average)
                                            : interpolated);
    }
    return bytes;
}

bool writeBytes(QIODevice *destination, const QByteArray &bytes,
                QString *error)
{
    if (destination->write(bytes) != bytes.size())
    {
        *error = QStringLiteral("Cannot write KTX2 data: %1")
                     .arg(destination->errorString());
        return false;
    }
    return true;
}

bool writePadding(QIODevice *destination, quint64 targetOffset,
                  QString *error)
{
    static const QByteArray zeros(16, '\0');
    while (static_cast<quint64>(destination->pos()) < targetOffset)
    {
        const quint64 remaining =
            targetOffset - static_cast<quint64>(destination->pos());
        const qsizetype count =
            static_cast<qsizetype>(std::min<quint64>(remaining, zeros.size()));
        if (!writeBytes(destination, zeros.left(count), error)) {
            return false;
        }
    }
    return true;
}

} // namespace

bool writeUastcKtx2(const QVector<QImage> &levels, const QString &outputFile,
                    bool srgb, UastcTextureSemantic semantic, QString *error,
                    const std::function<bool()> &isCancelled,
                    bool zstdSupercompression)
{
#ifndef MSFS_CONVERTER_WITH_ZSTD
    if (zstdSupercompression)
    {
        *error = QStringLiteral(
            "This build does not include optional Zstandard supercompression");
        return false;
    }
#endif
    if (levels.isEmpty() || levels.first().isNull())
    {
        *error = QStringLiteral("Cannot write an empty KTX2 mip chain");
        return false;
    }
    if (levels.size() > 32)
    {
        *error = QStringLiteral("KTX2 mip chain has an unreasonable level count");
        return false;
    }

    QVector<quint64> levelLengths;
    QVector<quint64> uncompressedLevelLengths;
    levelLengths.reserve(levels.size());
    uncompressedLevelLengths.reserve(levels.size());
    int expectedWidth = levels.first().width();
    int expectedHeight = levels.first().height();
    for (qsizetype level = 0; level < levels.size(); ++level)
    {
        if (isCancelled && isCancelled())
        {
            *error = QStringLiteral("Conversion cancelled");
            return false;
        }
        const QImage &image = levels.at(level);
        if (image.isNull() || image.width() != expectedWidth ||
            image.height() != expectedHeight) {
            *error = QStringLiteral("KTX2 mip %1 is %2x%3; expected %4x%5")
                         .arg(level)
                         .arg(image.width())
                         .arg(image.height())
                         .arg(expectedWidth)
                         .arg(expectedHeight);
            return false;
        }
        const quint64 blocksWide = (static_cast<quint64>(image.width()) + 3) / 4;
        const quint64 blocksHigh = (static_cast<quint64>(image.height()) + 3) / 4;
        const quint64 byteLength = blocksWide * blocksHigh * UastcBlockByteLength;
        levelLengths.append(byteLength);
        uncompressedLevelLengths.append(byteLength);
        expectedWidth = qMax(1, expectedWidth / 2);
        expectedHeight = qMax(1, expectedHeight / 2);
    }

    QVector<QByteArray> compressedLevels;
#ifdef MSFS_CONVERTER_WITH_ZSTD
    if (zstdSupercompression) {
        compressedLevels.reserve(levels.size());
        for (qsizetype level = 0; level < levels.size(); ++level)
        {
            if (isCancelled && isCancelled())
            {
                *error = QStringLiteral("Conversion cancelled");
                return false;
            }
            const QImage image =
                levels.at(level).convertToFormat(QImage::Format_RGBA8888);
            QByteArray uncompressed;
            uncompressed.reserve(
                static_cast<qsizetype>(uncompressedLevelLengths[level]));
            const int blocksHigh = (image.height() + 3) / 4;
            for (int blockY = 0; blockY < blocksHigh; ++blockY) {
                uncompressed.append(encodeUastcRow(image, blockY, semantic));
            }
            QByteArray compressed;
            compressed.resize(static_cast<qsizetype>(
                ZSTD_compressBound(static_cast<size_t>(uncompressed.size()))));
            const size_t compressedSize = ZSTD_compress(
                compressed.data(), static_cast<size_t>(compressed.size()),
                uncompressed.constData(), static_cast<size_t>(uncompressed.size()),
                9);
            if (ZSTD_isError(compressedSize))
            {
                *error =
                    QStringLiteral("Zstandard compression failed: %1")
                        .arg(QString::fromLatin1(ZSTD_getErrorName(compressedSize)));
                return false;
            }
            compressed.resize(static_cast<qsizetype>(compressedSize));
            levelLengths[level] = static_cast<quint64>(compressed.size());
            compressedLevels.append(std::move(compressed));
        }
    }
#endif

    bool hasAlpha = false;
    for (const QImage &source : levels) {
        const QImage image = source.convertToFormat(QImage::Format_RGBA8888);
        for (int y = 0; y < image.height() && !hasAlpha; ++y) {
            const auto *row =
                reinterpret_cast<const quint8 *>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x)
            {
                if (row[x * 4 + 3] != 255)
                {
                    hasAlpha = true;
                    break;
                }
            }
        }
        if (hasAlpha) {
            break;
        }
    }
    const QByteArray dfd = createDfd(srgb, hasAlpha);
    QByteArray kvd;
    appendKeyValue(&kvd, QByteArrayLiteral("KTXorientation"),
                   QByteArrayLiteral("rd"));
    appendKeyValue(&kvd, QByteArrayLiteral("KTXwriter"),
                   QByteArrayLiteral("MSFSAssetConverter embedded UASTC writer"));

    const quint32 levelCount = static_cast<quint32>(levels.size());
    const quint32 dfdOffset =
        12 + HeaderByteLength + levelCount * LevelIndexEntryByteLength;
    const quint32 kvdOffset = dfdOffset + static_cast<quint32>(dfd.size());
    quint64 dataOffset = kvdOffset + static_cast<quint32>(kvd.size());
    if (!zstdSupercompression) {
        dataOffset = (dataOffset + 15U) & ~quint64(15U);
    }

    QVector<quint64> levelOffsets(levelLengths.size());
    quint64 nextOffset = dataOffset;
    for (qsizetype level = levelLengths.size(); level-- > 0;)
    {
        if (!zstdSupercompression) {
            nextOffset = (nextOffset + 15U) & ~quint64(15U);
        }
        levelOffsets[level] = nextOffset;
        if (levelLengths.at(level) >
            std::numeric_limits<quint64>::max() - nextOffset) {
            *error = QStringLiteral("KTX2 output size overflows 64 bits");
            return false;
        }
        nextOffset += levelLengths.at(level);
    }

    QByteArray prefix;
    prefix.reserve(static_cast<qsizetype>(dataOffset));
    prefix.append("\xABKTX 20\xBB\r\n\x1A\n", 12);
    appendU32(&prefix, 0); // UASTC is described by the DFD, not a Vulkan format
    appendU32(&prefix, 1); // typeSize
    appendU32(&prefix, static_cast<quint32>(levels.first().width()));
    appendU32(&prefix, static_cast<quint32>(levels.first().height()));
    appendU32(&prefix, 0); // pixelDepth
    appendU32(&prefix, 0); // layerCount
    appendU32(&prefix, 1); // faceCount
    appendU32(&prefix, levelCount);
    appendU32(&prefix, zstdSupercompression ? 2U : 0U);
    appendU32(&prefix, dfdOffset);
    appendU32(&prefix, static_cast<quint32>(dfd.size()));
    appendU32(&prefix, kvdOffset);
    appendU32(&prefix, static_cast<quint32>(kvd.size()));
    appendU64(&prefix, 0); // No supercompression global data
    appendU64(&prefix, 0);

    for (qsizetype level = 0; level < levelLengths.size(); ++level)
    {
        appendU64(&prefix, levelOffsets.at(level));
        appendU64(&prefix, levelLengths.at(level));
        appendU64(&prefix, uncompressedLevelLengths.at(level));
    }
    prefix.append(dfd);
    prefix.append(kvd);
    while (static_cast<quint64>(prefix.size()) < dataOffset) {
        prefix.append('\0');
    }

    QSaveFile destination(outputFile);
    if (!destination.open(QIODevice::WriteOnly))
    {
        *error = QStringLiteral("Cannot write KTX2 texture: %1 (%2)")
                     .arg(outputFile, destination.errorString());
        return false;
    }
    if (!writeBytes(&destination, prefix, error)) {
        return false;
    }
    for (qsizetype level = levels.size(); level-- > 0;)
    {
        if (!writePadding(&destination, levelOffsets.at(level), error)) {
            return false;
        }
        if (zstdSupercompression)
        {
            if (!writeBytes(&destination, compressedLevels.at(level), error)) {
                return false;
            }
        }
        else
        {
            const QImage image =
                levels.at(level).convertToFormat(QImage::Format_RGBA8888);
            const int blocksHigh = (image.height() + 3) / 4;
            for (int blockY = 0; blockY < blocksHigh; ++blockY)
            {
                if (isCancelled && isCancelled())
                {
                    destination.cancelWriting();
                    *error = QStringLiteral("Conversion cancelled");
                    return false;
                }
                if (!writeBytes(&destination, encodeUastcRow(image, blockY, semantic),
                                error)) {
                    return false;
                }
            }
        }
    }
    if (!destination.commit())
    {
        *error = QStringLiteral("Cannot commit KTX2 texture: %1 (%2)")
                     .arg(outputFile, destination.errorString());
        return false;
    }
    return true;
}
