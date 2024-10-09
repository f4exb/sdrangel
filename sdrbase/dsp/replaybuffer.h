///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE								         //
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

#ifndef INCLUDE_REPLAYBUFFER_H
#define INCLUDE_REPLAYBUFFER_H

#include <QDebug>
#include <QMutex>
#include <QFileInfo>

#include <vector>

#include "dsp/wavfilerecord.h"

// Circular buffer for storing and replaying IQ samples
// lock/unlock should be called manually before write/read/getSize
// (so it only needs to be locked once for write then multiple reads).
template <typename T>
class ReplayBuffer {

public:
	ReplayBuffer() :
        m_data(1000000*2, 0),
        m_write(0),
        m_read(0),
        m_readOffset(0),
        m_count(0)
    {
    }

    bool useReplay() const
    {
        return (m_readOffset > 0) || m_loop;
    }

    void setSize(float lengthInSeconds, int sampleRate)
    {
         QMutexLocker locker(&m_mutex);
         unsigned int newSize = lengthInSeconds * sampleRate * 2;
         unsigned int oldSize = m_data.size();

         if (newSize == oldSize) {
             return;
         }

         // Save most recent data
         if (m_write >= newSize)
         {
             memmove(&m_data[0], &m_data[m_write-newSize], newSize);
             m_write = 0;
             m_count = newSize;
             m_data.resize(newSize);
         }
         else if (newSize < oldSize)
         {
             memmove(&m_data[m_write], &m_data[oldSize-(newSize-m_write)], newSize-m_write);
             m_count = std::min(m_count, newSize);
             m_data.resize(newSize);
         }
         else
         {
             m_data.resize(newSize);
             memmove(&m_data[newSize-(oldSize-m_write)], &m_data[m_write], oldSize-m_write);
         }
    }

    // lock()/unlock() should be called before/after calling this function
    int getSize() const {
        return m_data.size();
    }

    void setLoop(bool loop) {
        m_loop = loop;
    }

    bool getLoop() const {
        return m_loop;
    }

    // Copy count samples into circular buffer (1 I and Q pair should have count = 2)
    // When loop is set, samples aren't copied, but write pointer is still updated
    // lock()/unlock() should be called before/after calling this function
    void write(const T* data, unsigned int count)
    {
        unsigned int totalLen = count;
        while (totalLen > 0)
        {
            unsigned int len = std::min((unsigned int)m_data.size() - m_write, totalLen);
            if (!m_loop) {
                memcpy(&m_data[m_write], data, len * sizeof(T));
            }
            m_write += len;
            if (m_write >= m_data.size()) {
                m_write = 0;
            }
            m_count += len;
            if (m_count > m_data.size()) {
                m_count = m_data.size();
            }
            data += len;
            totalLen -= len;
        }
    }

    // Get pointer to count samples - actual number available is returned
    // lock()/unlock() should be called before/after calling this function
    unsigned int read(unsigned int count, const T*& ptr)
    {
        unsigned int totalLen = count;
        unsigned int len = std::min((unsigned int)m_data.size() - m_read, totalLen);
        ptr = &m_data[m_read];
        m_read += len;
        if (m_read >= m_data.size()) {
            m_read = 0;
        }
        return len;
    }

    void setReadOffset(unsigned int offset)
    {
        QMutexLocker locker(&m_mutex);
        m_readOffset = offset;
        offset = std::min(offset, (unsigned int)(m_data.size() - 1));
        int read = m_write - offset;
        while (read < 0) {
            read += m_data.size();
        }
        m_read = (unsigned int) read;
    }

    unsigned int getReadOffset()
    {
        return m_readOffset;
    }

    // Save buffer to .wav file
    void save(const QString& filename, quint32 sampleRate, quint64 centerFrequency)
    {
        QMutexLocker locker(&m_mutex);

        WavFileRecord wavFile(sampleRate, centerFrequency);
        QString baseName = filename;
        QFileInfo fileInfo(baseName);
        QString suffix = fileInfo.suffix();
        if (!suffix.isEmpty()) {
            baseName.chop(suffix.length() + 1);
        }
        wavFile.setFileName(baseName);

        wavFile.startRecording();
        int offset = m_write + m_data.size() - m_count;
        for (unsigned int i = 0; i < m_count; i += 2)
        {
            int idx = (i + offset) % m_data.size();
            qint16 l = conv(m_data[idx]);
            qint16 r = conv(m_data[idx+1]);
            wavFile.write(l, r);
        }
        wavFile.stopRecording();
    }

    void clear()
    {
        QMutexLocker locker(&m_mutex);
        std::fill(m_data.begin(), m_data.end(), 0);
        m_count = 0;
    }

    void lock()
    {
        m_mutex.lock();
    }

    void unlock()
    {
        m_mutex.unlock();
    }

private:
    std::vector<T> m_data;
    unsigned int m_write;       // Write index
    unsigned int m_read;        // Read index
    unsigned int m_readOffset;
    unsigned int m_count;       // Count of number of valid samples in the buffer
    bool m_loop;
    QMutex m_mutex;

    qint16 conv(quint8 data) const
    {
        return (data - 128) << 8;
    }

    qint16 conv(qint16 data) const
    {
        return data;
    }

    qint16 conv(qint32 data) const
    {
        return data >> 16;
    }

    qint16 conv(float data) const
    {
        return (qint16)(data * SDR_RX_SCALEF);
    }
};

#endif // INCLUDE_REPLAYBUFFER_H
