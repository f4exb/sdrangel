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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <QMutexLocker>
#include <QtEndian>

#include "audiofilereader.h"

namespace {

bool chunkIdEquals(const char *id, const char *expected)
{
    return std::memcmp(id, expected, 4) == 0;
}

}

AudioFileReader::AudioFileReader() :
    m_fileType(FileTypeNone),
    m_wavFormat(0),
    m_channelCount(0),
    m_bitsPerSample(0),
    m_blockAlign(0),
    m_sampleRate(0),
    m_dataOffset(0),
    m_dataSize(0),
    m_frameCount(0),
    m_position(0)
{
}

AudioFileReader::~AudioFileReader()
{
    close();
}

bool AudioFileReader::open(const QString& fileName)
{
    QMutexLocker locker(&m_mutex);
    closeUnlocked();
    m_errorString.clear();

    m_file.setFileName(fileName);

    if (!m_file.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
        return failUnlocked(QStringLiteral("Unable to open audio file: %1").arg(m_file.errorString()));
    }

    const qint64 size = m_file.size();

    if (size < 0) {
        return failUnlocked(QStringLiteral("Unable to determine audio file size"));
    }

    const quint64 fileSize = static_cast<quint64>(size);
    char signature[12] = {};
    const bool hasSignature = (fileSize >= sizeof(signature)) && readExact(signature, sizeof(signature));

    if (!m_file.seek(0)) {
        return failUnlocked(QStringLiteral("Unable to seek in audio file"));
    }

    if (hasSignature &&
        (chunkIdEquals(signature, "RIFF") || chunkIdEquals(signature, "RF64")) &&
        chunkIdEquals(signature + 8, "WAVE"))
    {
        return openWavUnlocked(fileSize);
    }

    if (fileName.endsWith(QStringLiteral(".raw"), Qt::CaseInsensitive)) {
        return openRawUnlocked(fileSize);
    }

    return failUnlocked(QStringLiteral("Audio file is neither WAV nor F32LE raw data"));
}

void AudioFileReader::close()
{
    QMutexLocker locker(&m_mutex);
    closeUnlocked();
}

void AudioFileReader::closeUnlocked()
{
    if (m_file.isOpen()) {
        m_file.close();
    }

    m_fileType = FileTypeNone;
    m_wavFormat = 0;
    m_channelCount = 0;
    m_bitsPerSample = 0;
    m_blockAlign = 0;
    m_sampleRate = 0;
    m_dataOffset = 0;
    m_dataSize = 0;
    m_frameCount = 0;
    m_position = 0;
    m_frameBuffer.clear();
}

bool AudioFileReader::failUnlocked(const QString& error)
{
    m_errorString = error;
    closeUnlocked();
    return false;
}

bool AudioFileReader::readExact(char *data, qint64 size)
{
    return m_file.read(data, size) == size;
}

bool AudioFileReader::openRawUnlocked(quint64 fileSize)
{
    m_fileType = FileTypeRawFloat32;
    m_wavFormat = WavFormatIEEEFloat;
    m_channelCount = 1;
    m_bitsPerSample = 32;
    m_blockAlign = 4;
    m_sampleRate = 48000;
    m_dataOffset = 0;
    m_dataSize = fileSize - (fileSize % m_blockAlign);
    m_frameCount = m_dataSize / m_blockAlign;
    m_position = 0;
    m_frameBuffer.resize(m_blockAlign);
    return true;
}

bool AudioFileReader::openWavUnlocked(quint64 fileSize)
{
    char riffHeader[12];

    if (!readExact(riffHeader, sizeof(riffHeader))) {
        return failUnlocked(QStringLiteral("WAV file has an incomplete RIFF header"));
    }

    const bool rf64 = chunkIdEquals(riffHeader, "RF64");
    bool haveFormat = false;
    bool haveData = false;
    quint64 rf64DataSize = 0;
    quint64 dataOffset = 0;
    quint64 dataSize = 0;

    while (static_cast<quint64>(m_file.pos()) + 8 <= fileSize)
    {
        char chunkHeader[8];

        if (!readExact(chunkHeader, sizeof(chunkHeader))) {
            break;
        }

        const quint32 chunkSize = qFromLittleEndian<quint32>(chunkHeader + 4);
        const quint64 payloadOffset = static_cast<quint64>(m_file.pos());
        const quint64 payloadSize = chunkSize;

        if (chunkIdEquals(chunkHeader, "ds64"))
        {
            uchar ds64[28];

            if ((chunkSize < sizeof(ds64)) || !readExact(reinterpret_cast<char *>(ds64), sizeof(ds64))) {
                return failUnlocked(QStringLiteral("WAV file has an invalid RF64 ds64 chunk"));
            }

            rf64DataSize = qFromLittleEndian<quint64>(ds64 + 8);
        }
        else if (chunkIdEquals(chunkHeader, "fmt "))
        {
            if (chunkSize < 16) {
                return failUnlocked(QStringLiteral("WAV format chunk is too short"));
            }

            const quint32 formatBytes = std::min<quint32>(chunkSize, 40);
            QByteArray formatData(formatBytes, 0);

            if (!readExact(formatData.data(), formatBytes)) {
                return failUnlocked(QStringLiteral("WAV format chunk is incomplete"));
            }

            const uchar *format = reinterpret_cast<const uchar *>(formatData.constData());
            m_wavFormat = qFromLittleEndian<quint16>(format);
            m_channelCount = qFromLittleEndian<quint16>(format + 2);
            m_sampleRate = qFromLittleEndian<quint32>(format + 4);
            m_blockAlign = qFromLittleEndian<quint16>(format + 12);
            m_bitsPerSample = qFromLittleEndian<quint16>(format + 14);

            if (m_wavFormat == WavFormatExtensible)
            {
                if (formatBytes < 40) {
                    return failUnlocked(QStringLiteral("WAV extensible format chunk is incomplete"));
                }

                // First two bytes of the sub-format GUID are the format code
                m_wavFormat = qFromLittleEndian<quint16>(format + 24);
            }

            haveFormat = true;
        }
        else if (chunkIdEquals(chunkHeader, "data"))
        {
            dataOffset = payloadOffset;
            dataSize = (rf64 && chunkSize == std::numeric_limits<quint32>::max()) ? rf64DataSize : chunkSize;
            haveData = true;
        }

        if (haveFormat && haveData) {
            break;
        }

        if (payloadSize > fileSize - std::min(payloadOffset, fileSize)) {
            return failUnlocked(QStringLiteral("WAV chunk extends beyond the end of the file"));
        }

        // Chunks are padded to an even size
        const quint64 nextChunk = payloadOffset + payloadSize + (payloadSize & 1U);

        if ((nextChunk > fileSize) || !m_file.seek(static_cast<qint64>(nextChunk))) {
            return failUnlocked(QStringLiteral("WAV chunk padding extends beyond the end of the file"));
        }
    }

    if (!haveFormat || !haveData) {
        return failUnlocked(QStringLiteral("WAV file is missing a format or data chunk"));
    }

    if ((m_wavFormat != WavFormatPCM) && (m_wavFormat != WavFormatIEEEFloat)) {
        return failUnlocked(QStringLiteral("WAV encoding is not PCM or IEEE float"));
    }

    if ((m_channelCount < 1) || (m_channelCount > 2)) {
        return failUnlocked(QStringLiteral("WAV file must contain one or two channels"));
    }

    if (m_sampleRate == 0) {
        return failUnlocked(QStringLiteral("WAV file has an invalid sample rate"));
    }

    const bool validPCM = (m_wavFormat == WavFormatPCM) &&
        ((m_bitsPerSample == 8) || (m_bitsPerSample == 16) ||
         (m_bitsPerSample == 24) || (m_bitsPerSample == 32));
    const bool validFloat = (m_wavFormat == WavFormatIEEEFloat) &&
        ((m_bitsPerSample == 32) || (m_bitsPerSample == 64));

    if (!validPCM && !validFloat) {
        return failUnlocked(QStringLiteral("WAV sample size is unsupported"));
    }

    const quint16 bytesPerSample = m_bitsPerSample / 8;
    const quint16 minimumBlockAlign = m_channelCount * bytesPerSample;

    if (m_blockAlign < minimumBlockAlign) {
        return failUnlocked(QStringLiteral("WAV block alignment is invalid"));
    }

    if (dataOffset > fileSize) {
        return failUnlocked(QStringLiteral("WAV data offset is outside the file"));
    }

    dataSize = std::min(dataSize, fileSize - dataOffset);
    dataSize -= dataSize % m_blockAlign;

    m_fileType = FileTypeWav;
    m_dataOffset = dataOffset;
    m_dataSize = dataSize;
    m_frameCount = dataSize / m_blockAlign;
    m_position = 0;
    m_frameBuffer.resize(m_blockAlign);

    if (!m_file.seek(static_cast<qint64>(m_dataOffset))) {
        return failUnlocked(QStringLiteral("Unable to seek to WAV data"));
    }

    return true;
}

bool AudioFileReader::readFrame(float& left, float& right)
{
    QMutexLocker locker(&m_mutex);

    if (!m_file.isOpen() || (m_position >= m_frameCount)) {
        return false;
    }

    if (!readExact(m_frameBuffer.data(), m_blockAlign))
    {
        m_position = m_frameCount;
        return false;
    }

    const uchar *frame = reinterpret_cast<const uchar *>(m_frameBuffer.constData());
    const quint16 bytesPerSample = m_bitsPerSample / 8;
    left = decodeSample(frame);
    right = m_channelCount == 2 ? decodeSample(frame + bytesPerSample) : left;
    ++m_position;
    return true;
}

float AudioFileReader::decodeSample(const uchar *data) const
{
    if (m_wavFormat == WavFormatIEEEFloat)
    {
        if (m_bitsPerSample == 32)
        {
            const quint32 bits = qFromLittleEndian<quint32>(data);
            float sample;
            std::memcpy(&sample, &bits, sizeof(sample));
            return std::isfinite(sample) ? sample : 0.0f;
        }

        const quint64 bits = qFromLittleEndian<quint64>(data);
        double sample;
        std::memcpy(&sample, &bits, sizeof(sample));
        return std::isfinite(sample) ? static_cast<float>(sample) : 0.0f;
    }

    switch (m_bitsPerSample)
    {
    case 8:
        return (static_cast<int>(data[0]) - 128) / 128.0f;
    case 16:
        return qFromLittleEndian<qint16>(data) / 32768.0f;
    case 24:
    {
        qint32 sample = static_cast<qint32>(data[0]) |
            (static_cast<qint32>(data[1]) << 8) |
            (static_cast<qint32>(data[2]) << 16);

        if (sample & 0x00800000) {
            sample |= ~0x00ffffff;
        }

        return sample / 8388608.0f;
    }
    case 32:
        return qFromLittleEndian<qint32>(data) / 2147483648.0f;
    default:
        return 0.0f;
    }
}

bool AudioFileReader::seek(quint64 frameIndex)
{
    QMutexLocker locker(&m_mutex);
    return seekUnlocked(frameIndex);
}

bool AudioFileReader::seekUnlocked(quint64 frameIndex)
{
    if (!m_file.isOpen()) {
        return false;
    }

    frameIndex = std::min(frameIndex, m_frameCount);
    const quint64 byteOffset = m_dataOffset + frameIndex * m_blockAlign;

    if (!m_file.seek(static_cast<qint64>(byteOffset))) {
        return false;
    }

    m_position = frameIndex;
    return true;
}

bool AudioFileReader::isOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_file.isOpen();
}

bool AudioFileReader::atEnd() const
{
    QMutexLocker locker(&m_mutex);
    return !m_file.isOpen() || (m_position >= m_frameCount);
}

quint32 AudioFileReader::getSampleRate() const
{
    QMutexLocker locker(&m_mutex);
    return m_sampleRate;
}

quint16 AudioFileReader::getChannelCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_channelCount;
}

quint64 AudioFileReader::getFrameCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_frameCount;
}

quint64 AudioFileReader::getPosition() const
{
    QMutexLocker locker(&m_mutex);
    return m_position;
}

QString AudioFileReader::getErrorString() const
{
    QMutexLocker locker(&m_mutex);
    return m_errorString;
}
