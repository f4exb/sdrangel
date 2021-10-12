///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "png.h"

#include <QDebug>
#include <QString>
#include <QBuffer>
#include <QFile>

PNG::PNG()
{
}

PNG::PNG(QByteArray data) :
    m_bytes(data),
    m_width(0),
    m_height(0)
{
    int idx = findChunk("IHDR");
    if (idx >= 0)
    {
        m_width = getInt(idx + 8);
        m_height = getInt(idx + 12);
    }
    else
    {
        qDebug() << "PNG: No IHDR found";
    }
}

void PNG::appendSignature()
{
    m_bytes.append(m_signature);
}

void PNG::appendEnd()
{
    QByteArray ba;
    appendChunk("IEND", ba);
}

void PNG::appendChunk(const char *type, QByteArray chunk)
{
    appendInt(chunk.size());
    appendInt(typeStringToInt(type));
    m_bytes.append(chunk);
    appendInt(crc(type, chunk)); // CRC type and data, but not length
}

void PNG::append(QByteArray data)
{
    m_bytes.append(data);
}

void PNG::appendInt(QByteArray& ba, quint32 value)
{
    // Network byte order
    ba.append((value >> 24) & 0xff);
    ba.append((value >> 16) & 0xff);
    ba.append((value >> 8) & 0xff);
    ba.append((value) & 0xff);
}

void PNG::appendShort(QByteArray& ba, quint16 value)
{
    // Network byte order
    ba.append((value >> 8) & 0xff);
    ba.append((value) & 0xff);
}

void PNG::appendInt(quint32 value)
{
    appendInt(m_bytes, value);
}

qint32 PNG::getInt(int index)
{
    qint32 v = 0;
    for (int i = 0; i < 4; i++) {
        v |= (m_bytes[index+i] & 0xff) << ((3-i)*8);
    }
    return v;
}

qint32 PNG::crc(const char *type, const QByteArray data)
{
    m_crc.init();
    m_crc.calculate((const uint8_t *)type, 4);
    m_crc.calculate((const uint8_t *)data.data(), data.size());
    return m_crc.get();
}

qint32 PNG::typeStringToInt(const char *header)
{
    quint32 v = 0;
    for (int i = 0; i < 4; i++) {
        v |= header[i] << ((3-i)*8);
    }
    return v;
}

QString PNG::intToTypeString(quint32 type)
{
    QString s;
    for (int i = 0; i < 4; i++)
    {
        char c = (type >> ((3-i)*8)) & 0xff;
        s.append(c);
    }
    return s;
}

// Animation control chunk for APNGs (loops=0 is infinite)
void PNG::appendacTL(int frames, quint32 loops)
{
    QByteArray ba;
    appendInt(ba, frames);
    appendInt(ba, loops);
    appendChunk("acTL", ba);
}

// Frame control chunk for APNGs
void PNG::appendfcTL(quint32 seqNo, quint32 width, quint32 height, int fps, quint32 xOffset, quint32 yOffset)
{
    QByteArray ba;
    appendInt(ba, seqNo);
    appendInt(ba, width);
    appendInt(ba, height);
    appendInt(ba, xOffset);
    appendInt(ba, yOffset);
    appendShort(ba, 1);
    appendShort(ba, fps);
    ba.append((char)0); // No disposal
    ba.append((char)0); // Overwrite previous image
    appendChunk("fcTL", ba);
}

// Animation frame data
void PNG::appendfdAT(quint32 seqNo, const QByteArray& data)
{
    QByteArray ba;
    appendInt(ba, seqNo);
    ba.append(data);
    appendChunk("fdAT", ba);
}

QByteArray PNG::data()
{
    return m_bytes;
}

bool PNG::checkSignature()
{
    return m_bytes.startsWith(m_signature);
}

int PNG::findChunk(const char *type, int startIndex)
{
    if ((startIndex == 0) && !checkSignature())
    {
        qDebug() << "PNG::findChunk - PNG signature not found";
        return -1;
    }
    int i = startIndex == 0 ? m_signature.size() : startIndex;
    qint32 typeInt = typeStringToInt(type);
    while (i < m_bytes.size())
    {
        qint32 chunkType = getInt(i+4);
        if (typeInt == chunkType) {
            return i;
        }
        qint32 length = getInt(i);
        i += 12 + length;
    }
    return -1;
}

// Get chunk including length, type data and CRC
QByteArray PNG::getChunk(const char *type)
{
    int start = findChunk(type);
    if (start >= 0)
    {
        quint32 length = getInt(start);
        return m_bytes.mid(start, length + 12);
    }
    return QByteArray();
}

// Get all chunks with same type
QByteArray PNG::getChunks(const char *type)
{
    int start = 0;
    QByteArray bytes;

    while ((start = findChunk(type, start)) != -1)
    {
        quint32 length = getInt(start);
        QByteArray chunk = m_bytes.mid(start, length + 12);
        bytes.append(chunk);
        start += chunk.size();
    }
    return bytes;
}

// Get data from chunk
QList<QByteArray> PNG::getChunkData(const char *type)
{
    int start = 0;
    QList<QByteArray> chunks;

    while ((start = findChunk(type, start)) != -1)
    {
        quint32 length = getInt(start);
        QByteArray chunk = m_bytes.mid(start + 8, length);
        chunks.append(chunk);
        start += length + 12;
    }

    return chunks;
}

quint32 PNG::getWidth() const
{
    return m_width;
}

quint32 PNG::getHeight() const
{
    return m_height;
}

bool APNG::addImage(const QImage& image, int fps)
{
    if (!m_ended)
    {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::ReadWrite);
        if (image.save(&buffer, "PNG"))
        {
            PNG pngIn(ba);
            if (m_frame == 0)
            {
                m_png.append(pngIn.getChunk("IHDR"));
                m_png.appendacTL(m_frames);
                m_png.appendfcTL(m_seqNo++, pngIn.getWidth(), pngIn.getHeight(), fps);
                // PNGs can contain multiple IDAT chunks, typically each limited to 8kB
                m_png.append(pngIn.getChunks("IDAT"));
            }
            else
            {
                m_png.appendfcTL(m_seqNo++, pngIn.getWidth(), pngIn.getHeight(), fps);
                QList<QByteArray> data = pngIn.getChunkData("IDAT");
                for (int i = 0; i < data.size(); i++) {
                    m_png.appendfdAT(m_seqNo++, data[i]);
                }
            }
            m_frame++;
            return true;
        }
        else
        {
            qDebug() << "APNG::addImage - Failed to save image to PNG";
            return false;
        }
    }
    else
    {
        qDebug() << "APNG::addImage - Call to addImage after IEND added";
        return false;
    }
}

bool APNG::save(const QString& fileName)
{
    if (!m_ended)
    {
        if (m_frame != m_frames) {
            qDebug() << "APNG::save - " << m_frame << " frames added out of expected " << m_frames;
        }
        m_png.appendEnd();
        m_ended = true;
    }
    QFile animFile(fileName);
    if (animFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        animFile.write(m_png.data());
        animFile.close();
        return true;
    }
    else
    {
        return false;
    }
}
