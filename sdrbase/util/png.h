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

#ifndef INCLUDE_PNG_H
#define INCLUDE_PNG_H

#include <QByteArray>
#include <QImage>

#include "export.h"

#include "util/crc.h"

// PNG (Portable Network Graphics) utility code
// Contains just enough functionality to support assembling APNG files (Animated PNGs)
// from multiple PNGs (which can be created using Qt)
class SDRBASE_API PNG {
public:

    PNG();
    PNG(QByteArray data);
    void appendSignature();
    void appendEnd();
    void appendChunk(const char *type, QByteArray chunk);
    void append(QByteArray data);
    void appendInt(QByteArray& ba, quint32 value);
    void appendShort(QByteArray& ba, quint16 value);
    void appendInt(quint32 value);
    qint32 getInt(int index);
    qint32 crc(const char *type, const QByteArray data);
    qint32 typeStringToInt(const char *header);
    QString intToTypeString(quint32 type);
    void appendacTL(int frames, quint32 loops=0);
    void appendfcTL(quint32 seqNo, quint32 width, quint32 height, int fps, quint32 xOffset=0, quint32 yOffset=0);
    void appendfdAT(quint32 seqNo, const QByteArray& data);
    QByteArray data();
    bool checkSignature();
    int findChunk(const char *type, int startIndex=0);
    QByteArray getChunk(const char *type);
    QByteArray getChunks(const char *type);
    QList<QByteArray> getChunkData(const char *type);
    quint32 getWidth() const;
    quint32 getHeight() const;

private:

    QByteArray m_signature = QByteArrayLiteral("\x89\x50\x4e\x47\x0d\x0a\x1a\x0a");
    QByteArray m_bytes;
    crc32 m_crc;
    quint32 m_width;
    quint32 m_height;

};

// Animated PNG
class SDRBASE_API APNG {

public:

    APNG(int frames) :
        m_frames(frames),
        m_frame(0),
        m_seqNo(0),
        m_ended(false)
    {
        m_png.appendSignature();
    }

    bool addImage(const QImage& image, int fps=5);
    bool save(const QString& fileName);

private:

    PNG m_png;
    int m_frames;       //!< Total number of frames in animation
    int m_frame;        //!< Current frame number
    int m_seqNo;        //!< Chunk sequence number
    bool m_ended;       //!< IEND chunk has added

};

#endif // INCLUDE_PNG_H
