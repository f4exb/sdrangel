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

#include "ddsdecoder.h"
#include "bc7decoder.h"

#include <QFile>
#include <QtEndian>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {

quint32 readU32(const QByteArray &data, qsizetype offset)
{
    return qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(data.constData() + offset));
}

quint16 readU16(const uchar *data)
{
    return qFromLittleEndian<quint16>(data);
}

struct Rgba {
    int m_r;
    int m_g;
    int m_b;
    int m_a;
};

Rgba color565(quint16 value)
{
    const int r = (value >> 11) & 31;
    const int g = (value >> 5) & 63;
    const int b = value & 31;
    return {(r * 255 + 15) / 31, (g * 255 + 31) / 63, (b * 255 + 15) / 31, 255};
}

void writePixel(QImage &image, int x, int y, const Rgba &pixel)
{
    if (x < 0 || y < 0 || x >= image.width() || y >= image.height()) {
        return;
    }
    uchar *destination = image.scanLine(y) + x * 4;
    destination[0] = static_cast<uchar>(std::clamp(pixel.m_r, 0, 255));
    destination[1] = static_cast<uchar>(std::clamp(pixel.m_g, 0, 255));
    destination[2] = static_cast<uchar>(std::clamp(pixel.m_b, 0, 255));
    destination[3] = static_cast<uchar>(std::clamp(pixel.m_a, 0, 255));
}

std::array<Rgba, 4> makeBc1Palette(const uchar *block, bool allowTransparency)
{
    const quint16 packed0 = readU16(block);
    const quint16 packed1 = readU16(block + 2);
    const Rgba c0 = color565(packed0);
    const Rgba c1 = color565(packed1);
    std::array<Rgba, 4> palette{c0, c1, {}, {}};

    if (!allowTransparency || packed0 > packed1)
    {
        palette[2] = {(2 * c0.m_r + c1.m_r) / 3, (2 * c0.m_g + c1.m_g) / 3,
                      (2 * c0.m_b + c1.m_b) / 3, 255};
        palette[3] = {(c0.m_r + 2 * c1.m_r) / 3, (c0.m_g + 2 * c1.m_g) / 3,
                      (c0.m_b + 2 * c1.m_b) / 3, 255};
    }
    else
    {
        palette[2] = {(c0.m_r + c1.m_r) / 2, (c0.m_g + c1.m_g) / 2, (c0.m_b + c1.m_b) / 2, 255};
        palette[3] = {0, 0, 0, 0};
    }
    return palette;
}

void decodeBc1Block(const uchar *block, QImage &image, int blockX, int blockY)
{
    const auto palette = makeBc1Palette(block, true);
    const quint32 indices = qFromLittleEndian<quint32>(block + 4);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            const int index = (indices >> (2 * (4 * y + x))) & 3;
            writePixel(image, blockX * 4 + x, blockY * 4 + y, palette[index]);
        }
    }
}

std::array<int, 8> makeUnsignedBc4Palette(const uchar *block)
{
    const int e0 = block[0];
    const int e1 = block[1];
    std::array<int, 8> values{e0, e1, 0, 0, 0, 0, 0, 0};
    if (e0 > e1)
    {
        for (int i = 1; i <= 6; ++i) {
            values[i + 1] = ((7 - i) * e0 + i * e1 + 3) / 7;
        }
    }
    else
    {
        for (int i = 1; i <= 4; ++i) {
            values[i + 1] = ((5 - i) * e0 + i * e1 + 2) / 5;
        }
        values[6] = 0;
        values[7] = 255;
    }
    return values;
}

std::array<int, 8> makeSignedBc4Palette(const uchar *block)
{
    auto signedEndpoint = [](uchar value) {
        const int endpoint = static_cast<qint8>(value);
        return endpoint == -128 ? -127 : endpoint;
    };
    const int e0 = signedEndpoint(block[0]);
    const int e1 = signedEndpoint(block[1]);
    std::array<int, 8> values{e0, e1, 0, 0, 0, 0, 0, 0};
    if (e0 > e1)
    {
        for (int i = 1; i <= 6; ++i) {
            values[i + 1] = ((7 - i) * e0 + i * e1) / 7;
        }
    }
    else
    {
        for (int i = 1; i <= 4; ++i) {
            values[i + 1] = ((5 - i) * e0 + i * e1) / 5;
        }
        values[6] = -127;
        values[7] = 127;
    }
    return values;
}

quint64 read48BitIndices(const uchar *block)
{
    quint64 indices = 0;
    for (int i = 0; i < 6; ++i) {
        indices |= static_cast<quint64>(block[2 + i]) << (8 * i);
    }
    return indices;
}

void decodeBc3Block(const uchar *block, QImage &image, int blockX, int blockY)
{
    const auto alpha = makeUnsignedBc4Palette(block);
    const quint64 alphaIndices = read48BitIndices(block);
    const auto colors = makeBc1Palette(block + 8, false);
    const quint32 colorIndices = qFromLittleEndian<quint32>(block + 12);

    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            const int pixel = 4 * y + x;
            Rgba value = colors[(colorIndices >> (2 * pixel)) & 3];
            value.m_a = alpha[(alphaIndices >> (3 * pixel)) & 7];
            writePixel(image, blockX * 4 + x, blockY * 4 + y, value);
        }
    }
}

void decodeBc2Block(const uchar *block, QImage &image, int blockX, int blockY)
{
    const auto colors = makeBc1Palette(block + 8, false);
    const quint32 colorIndices = qFromLittleEndian<quint32>(block + 12);
    const quint64 alphaValues = qFromLittleEndian<quint64>(block);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            const int pixel = 4 * y + x;
            Rgba value = colors[(colorIndices >> (2 * pixel)) & 3];
            value.m_a = static_cast<int>((alphaValues >> (4 * pixel)) & 15) * 17;
            writePixel(image, blockX * 4 + x, blockY * 4 + y, value);
        }
    }
}

void decodeBc5Block(const uchar *block, QImage &image, int blockX, int blockY,
                    bool signedValues, bool normalMap, bool flipNormalY)
{
    const quint64 redIndices = read48BitIndices(block);
    const quint64 greenIndices = read48BitIndices(block + 8);
    const auto signedRed = makeSignedBc4Palette(block);
    const auto signedGreen = makeSignedBc4Palette(block + 8);
    const auto unsignedRed = makeUnsignedBc4Palette(block);
    const auto unsignedGreen = makeUnsignedBc4Palette(block + 8);

    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            const int pixel = 4 * y + x;
            const int redIndex = (redIndices >> (3 * pixel)) & 7;
            const int greenIndex = (greenIndices >> (3 * pixel)) & 7;

            if (normalMap)
            {
                float nx = signedValues ? signedRed[redIndex] / 127.0F
                                        : unsignedRed[redIndex] / 127.5F - 1.0F;
                float ny = signedValues ? signedGreen[greenIndex] / 127.0F
                                        : unsignedGreen[greenIndex] / 127.5F - 1.0F;
                nx = std::clamp(nx, -1.0F, 1.0F);
                ny = std::clamp(ny, -1.0F, 1.0F);
                if (flipNormalY) {
                    ny = -ny;
                }
                const float nz = std::sqrt(std::max(0.0F, 1.0F - nx * nx - ny * ny));
                writePixel(image, blockX * 4 + x, blockY * 4 + y,
                           {static_cast<int>((nx * 0.5F + 0.5F) * 255.0F + 0.5F),
                            static_cast<int>((ny * 0.5F + 0.5F) * 255.0F + 0.5F),
                            static_cast<int>((nz * 0.5F + 0.5F) * 255.0F + 0.5F), 255});
            }
            else
            {
                const int red =
                    signedValues ? signedRed[redIndex] + 127 : unsignedRed[redIndex];
                const int green = signedValues ? signedGreen[greenIndex] + 127
                                               : unsignedGreen[greenIndex];
                writePixel(image, blockX * 4 + x, blockY * 4 + y, {red, green, 0, 255});
            }
        }
    }
}

bool decodeBc7ImageBlock(const uchar *block, QImage &image, int blockX,
                         int blockY)
{
    Bc7Rgba pixels[16];
    if (!decodeBc7Block(block, pixels)) {
        return false;
    }
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            const Bc7Rgba &pixel = pixels[y * 4 + x];
            writePixel(image, blockX * 4 + x, blockY * 4 + y,
                       {pixel.m_r, pixel.m_g, pixel.m_b, pixel.m_a});
        }
    }
    return true;
}

} // namespace

bool decodeDds(const QString &fileName, bool normalMap, bool flipNormalY,
               DdsDecodeResult *result, QString *error)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        *error =
            QStringLiteral("Cannot open %1: %2").arg(fileName, file.errorString());
        return false;
    }
    constexpr qint64 MaximumDdsBytes = 1024LL * 1024 * 1024;
    if (file.size() > MaximumDdsBytes)
    {
        *error = QStringLiteral("DDS file exceeds the 1 GiB safety limit: %1")
                     .arg(fileName);
        return false;
    }
    const QByteArray data = file.readAll();
    if (data.size() < 128 || data.left(4) != QByteArrayLiteral("DDS ") ||
        readU32(data, 4) != 124 || readU32(data, 76) != 32) {
        *error = QStringLiteral("%1 is not a supported DDS file").arg(fileName);
        return false;
    }

    const int height = static_cast<int>(readU32(data, 12));
    const int width = static_cast<int>(readU32(data, 16));
    const int mipLevels = std::max(1, static_cast<int>(readU32(data, 28)));
    const quint32 pixelFormatFlags = readU32(data, 80);
    const quint32 caps2 = readU32(data, 112);
    if ((caps2 & (0x200U | 0x200000U)) != 0)
    {
        *error =
            QStringLiteral("DDS cubemaps and volume textures are not supported: %1")
                .arg(fileName);
        return false;
    }
    const QByteArray fourCc = data.mid(84, 4);
    int blockSize = 0;
    qsizetype sourceOffset = 128;
    enum class Format {
        Bc1,
        Bc2,
        Bc3,
        Bc5Unsigned,
        Bc5Signed,
        Bc7,
        Rgba32
    } format;

    if (fourCc == "DXT1")
    {
        format = Format::Bc1;
        blockSize = 8;
    }
    else if (fourCc == "DXT3")
    {
        format = Format::Bc2;
        blockSize = 16;
    }
    else if (fourCc == "DXT5")
    {
        format = Format::Bc3;
        blockSize = 16;
    }
    else if (fourCc == "ATI2" || fourCc == "BC5U")
    {
        format = Format::Bc5Unsigned;
        blockSize = 16;
    }
    else if (fourCc == "BC5S")
    {
        format = Format::Bc5Signed;
        blockSize = 16;
    }
    else if (fourCc == "DX10")
    {
        if (data.size() < 148)
        {
            *error = QStringLiteral("Truncated DDS DX10 header in %1").arg(fileName);
            return false;
        }
        const quint32 dxgiFormat = readU32(data, 128);
        const quint32 resourceDimension = readU32(data, 132);
        const quint32 miscFlag = readU32(data, 136);
        const quint32 arraySize = readU32(data, 140);
        if (resourceDimension != 3 || arraySize != 1 || (miscFlag & 0x4U) != 0)
        {
            *error = QStringLiteral("Only a single 2D DDS texture is supported in %1")
                         .arg(fileName);
            return false;
        }
        sourceOffset = 148;
        switch (dxgiFormat)
        {
        case 71: // DXGI_FORMAT_BC1_UNORM
        case 72: // DXGI_FORMAT_BC1_UNORM_SRGB
            format = Format::Bc1;
            blockSize = 8;
            break;
        case 74: // DXGI_FORMAT_BC2_UNORM
        case 75: // DXGI_FORMAT_BC2_UNORM_SRGB
            format = Format::Bc2;
            blockSize = 16;
            break;
        case 77: // DXGI_FORMAT_BC3_UNORM
        case 78: // DXGI_FORMAT_BC3_UNORM_SRGB
            format = Format::Bc3;
            blockSize = 16;
            break;
        case 83: // DXGI_FORMAT_BC5_UNORM
            format = Format::Bc5Unsigned;
            blockSize = 16;
            break;
        case 84: // DXGI_FORMAT_BC5_SNORM
            format = Format::Bc5Signed;
            blockSize = 16;
            break;
        case 98: // DXGI_FORMAT_BC7_UNORM
        case 99: // DXGI_FORMAT_BC7_UNORM_SRGB
            format = Format::Bc7;
            blockSize = 16;
            break;
        default:
            *error = QStringLiteral("Unsupported DDS DXGI format %1 in %2")
                         .arg(dxgiFormat)
                         .arg(fileName);
            return false;
        }
    }
    else if ((pixelFormatFlags & 0x40U) != 0 && readU32(data, 88) == 32)
    {
        format = Format::Rgba32;
        blockSize = 0;
    }
    else
    {
        *error = QStringLiteral("Unsupported DDS FourCC '%1' in %2")
                     .arg(QString::fromLatin1(fourCc), fileName);
        return false;
    }

    if (width <= 0 || height <= 0 || width > 32768 || height > 32768 ||
        mipLevels > 16) {
        *error = QStringLiteral("Invalid DDS dimensions %1x%2 in %3")
                     .arg(width)
                     .arg(height)
                     .arg(fileName);
        return false;
    }
    result->m_levels.clear();
    result->m_levels.reserve(mipLevels);
    const quint32 redMask = readU32(data, 92);
    const quint32 greenMask = readU32(data, 96);
    const quint32 blueMask = readU32(data, 100);
    const quint32 alphaMask = readU32(data, 104);
    auto unpackMasked = [](quint32 value, quint32 mask, int defaultValue) {
        if (mask == 0) {
            return defaultValue;
        }
        int shift = 0;
        while (((mask >> shift) & 1U) == 0) {
            ++shift;
        }
        const quint32 maximum = mask >> shift;
        return static_cast<int>(((value & mask) >> shift) * 255U / maximum);
    };
    for (int level = 0; level < mipLevels; ++level)
    {
        const int levelWidth = std::max(1, width >> level);
        const int levelHeight = std::max(1, height >> level);
        const int blocksWide = (levelWidth + 3) / 4;
        const int blocksHigh = (levelHeight + 3) / 4;
        const qsizetype levelBytes =
            format == Format::Rgba32
                ? static_cast<qsizetype>(levelWidth) * levelHeight * 4
                : static_cast<qsizetype>(blocksWide) * blocksHigh * blockSize;
        if (sourceOffset > data.size() || levelBytes > data.size() - sourceOffset)
        {
            *error = QStringLiteral("DDS mip %1 (%2x%3) is truncated in %4")
                         .arg(level)
                         .arg(levelWidth)
                         .arg(levelHeight)
                         .arg(fileName);
            return false;
        }

        QImage image(levelWidth, levelHeight, QImage::Format_RGBA8888);
        if (image.isNull())
        {
            *error = QStringLiteral("Cannot allocate DDS mip %1 (%2x%3)")
                         .arg(level)
                         .arg(levelWidth)
                         .arg(levelHeight);
            return false;
        }

        const uchar *source =
            reinterpret_cast<const uchar *>(data.constData() + sourceOffset);
        if (format == Format::Rgba32)
        {
            for (int y = 0; y < levelHeight; ++y)
            {
                for (int x = 0; x < levelWidth; ++x)
                {
                    const quint32 packed = qFromLittleEndian<quint32>(
                        source + (static_cast<qsizetype>(y) * levelWidth + x) * 4);
                    writePixel(image, x, y,
                               {unpackMasked(packed, redMask, 0),
                                unpackMasked(packed, greenMask, 0),
                                unpackMasked(packed, blueMask, 0),
                                unpackMasked(packed, alphaMask, 255)});
                }
            }
        }
        else
        {
            for (int by = 0; by < blocksHigh; ++by)
            {
                for (int bx = 0; bx < blocksWide; ++bx) {
                    const uchar *block = source + (by * blocksWide + bx) * blockSize;
                    switch (format)
                    {
                    case Format::Bc1:
                        decodeBc1Block(block, image, bx, by);
                        break;
                    case Format::Bc2:
                        decodeBc2Block(block, image, bx, by);
                        break;
                    case Format::Bc3:
                        decodeBc3Block(block, image, bx, by);
                        break;
                    case Format::Bc5Unsigned:
                        decodeBc5Block(block, image, bx, by, false, normalMap, flipNormalY);
                        break;
                    case Format::Bc5Signed:
                        decodeBc5Block(block, image, bx, by, true, normalMap, flipNormalY);
                        break;
                    case Format::Bc7:
                        if (!decodeBc7ImageBlock(block, image, bx, by))
                        {
                            *error = QStringLiteral(
                                         "Invalid BC7 block in DDS mip %1 at (%2,%3) in %4")
                                         .arg(level)
                                         .arg(bx)
                                         .arg(by)
                                         .arg(fileName);
                            return false;
                        }
                        break;
                    case Format::Rgba32:
                        break;
                    }
                }
            }
        }
        if (normalMap && flipNormalY && format != Format::Bc5Unsigned &&
            format != Format::Bc5Signed) {
            for (int y = 0; y < image.height(); ++y) {
                uchar *row = image.scanLine(y);
                for (int x = 0; x < image.width(); ++x) {
                    row[x * 4 + 1] = static_cast<uchar>(255 - row[x * 4 + 1]);
                }
            }
        }
        result->m_levels.append(std::move(image));
        sourceOffset += levelBytes;
    }

    result->m_format =
        fourCc == "DX10"
            ? QStringLiteral("DX10/%1").arg(readU32(data, 128))
            : (format == Format::Rgba32 ? QStringLiteral("RGBA32")
                                        : QString::fromLatin1(fourCc));
    return true;
}
