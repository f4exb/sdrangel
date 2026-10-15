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

#ifndef INCLUDE_AUDIOFILEREADER_H
#define INCLUDE_AUDIOFILEREADER_H

#include <QByteArray>
#include <QFile>
#include <QMutex>
#include <QString>
#include <QtGlobal>

#include "export.h"

/**
 * Reads audio frames from SDRangel's legacy F32LE raw format or from a WAV file.
 *
 * Raw files are interpreted as mono, 48 kHz, 32-bit little-endian float samples.
 * WAV files may contain one or two channels of 8/16/24/32-bit PCM or 32/64-bit
 * IEEE floating-point samples. Samples are returned as normalized floats and a
 * mono input is copied to both output channels.
 *
 * Files are accessed with QFile, so names returned by the Android file picker
 * can be opened. All methods are thread safe.
 */
class SDRBASE_API AudioFileReader
{
public:
    AudioFileReader();
    ~AudioFileReader();

    bool open(const QString& fileName);
    void close();

    bool readFrame(float& left, float& right);
    bool seek(quint64 frameIndex);
    bool rewind() { return seek(0); }

    bool isOpen() const;
    bool atEnd() const;
    quint32 getSampleRate() const;
    quint16 getChannelCount() const;
    quint64 getFrameCount() const;
    quint64 getPosition() const;
    QString getErrorString() const;

private:
    enum FileType
    {
        FileTypeNone,
        FileTypeRawFloat32,
        FileTypeWav
    };

    enum WavFormat
    {
        WavFormatPCM = 1,
        WavFormatIEEEFloat = 3,
        WavFormatExtensible = 0xfffe
    };

    mutable QMutex m_mutex;
    QFile m_file;
    FileType m_fileType;
    quint16 m_wavFormat;
    quint16 m_channelCount;
    quint16 m_bitsPerSample;
    quint16 m_blockAlign;
    quint32 m_sampleRate;
    quint64 m_dataOffset;
    quint64 m_dataSize;
    quint64 m_frameCount;
    quint64 m_position;
    QByteArray m_frameBuffer;
    QString m_errorString;

    void closeUnlocked();
    bool openRawUnlocked(quint64 fileSize);
    bool openWavUnlocked(quint64 fileSize);
    bool failUnlocked(const QString& error);
    bool seekUnlocked(quint64 frameIndex);
    bool readExact(char *data, qint64 size);
    float decodeSample(const uchar *data) const;
};

#endif // INCLUDE_AUDIOFILEREADER_H
