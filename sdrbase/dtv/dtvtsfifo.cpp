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
#include <cstring>

#include <QDebug>

#include "util/profiler.h"

#include "dtv/dtvtsfifo.h"

DTVTSFifo::DTVTSFifo(int size, bool needsReader) :
    m_size((size_t) std::max(188, size)),
    m_readPos(0),
    m_writePos(0),
    m_fill(0),
    m_overflows(0),
    m_needsReader(needsReader),
    m_accepting(!needsReader)
{
    if (!needsReader) {
        m_buffer.resize(m_size);
    }
}

void DTVTSFifo::attachLocked()
{
    if (m_buffer.size() != m_size) {
        m_buffer.resize(m_size);
    }

    m_readPos = 0;
    m_writePos = 0;
    m_fill = 0;
    m_sinceRead.start();
    m_accepting = true;
}

void DTVTSFifo::detachLocked()
{
    if (!m_needsReader) {
        return;
    }

    m_accepting = false;
    m_readPos = 0;
    m_writePos = 0;
    m_fill = 0;
    std::vector<uint8_t>().swap(m_buffer);
}

void DTVTSFifo::attachReader()
{
    QMutexLocker locker(&m_mutex);
    attachLocked();
}

void DTVTSFifo::detachReader()
{
    QMutexLocker locker(&m_mutex);
    detachLocked();
    m_dataAvailable.wakeAll();
}

void DTVTSFifo::write(const uint8_t *data, int length)
{
    if (!isAccepting() || (length <= 0)) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    writeLocked(data, length);
}

void DTVTSFifo::writePackets(const uint8_t *packets, int count)
{
    if (!isAccepting() || (count <= 0)) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    const int fits = (int) std::min((size_t) count, (m_buffer.size() - std::min(m_fill, m_buffer.size())) / 188);

    // As many as fit, the rest dropped (and counted) whole
    if (fits > 0) {
        writeLocked(packets, fits * 188);
    }

    if (fits < count) {
        writeLocked(packets + fits * 188, (count - fits) * 188);
    }
}

void DTVTSFifo::writeLocked(const uint8_t *data, int length)
{
    if (m_buffer.empty()) {
        return; // Detached meanwhile
    }

    size_t free = m_buffer.size() - m_fill;

    // Dropped whole, so the stream stays in packets
    if ((size_t) length > free)
    {
        m_overflows += length;
        PROFILER_ADD("DTV TS FIFO overflow (count = writes dropping data)", 0)

        if (m_needsReader && m_sinceRead.isValid() && (m_sinceRead.elapsed() > m_readerGoneMs))
        {
            qDebug("DTVTSFifo::writeLocked: nothing read for %lld s: no reader", (long long) (m_sinceRead.elapsed() / 1000));
            detachLocked();
        }

        return;
    }

    size_t first = std::min((size_t) length, m_buffer.size() - m_writePos);
    memcpy(&m_buffer[m_writePos], data, first);
    memcpy(&m_buffer[0], data + first, length - first);
    m_writePos = (m_writePos + length) % m_buffer.size();
    m_fill += length;
    m_dataAvailable.wakeAll();
}

int DTVTSFifo::read(uint8_t *data, int length, int timeoutMs)
{
    QMutexLocker locker(&m_mutex);

    // The reader's here (as the video player is, without attaching first)
    if (!m_accepting) {
        attachLocked();
    }

    m_sinceRead.start();

    if (m_fill == 0)
    {
        m_dataAvailable.wait(&m_mutex, timeoutMs);

        if (m_fill == 0) {
            return 0;
        }
    }

    length = (int) std::min((size_t) length, m_fill);
    size_t first = std::min((size_t) length, m_buffer.size() - m_readPos);
    memcpy(data, &m_buffer[m_readPos], first);
    memcpy(data + first, &m_buffer[0], length - first);
    m_readPos = (m_readPos + length) % m_buffer.size();
    m_fill -= length;

    return length;
}

void DTVTSFifo::reset()
{
    QMutexLocker locker(&m_mutex);
    m_readPos = 0;
    m_writePos = 0;
    m_fill = 0;
}

int DTVTSFifo::fill()
{
    QMutexLocker locker(&m_mutex);
    return (int) m_fill;
}

uint64_t DTVTSFifo::getOverflows()
{
    QMutexLocker locker(&m_mutex);
    return m_overflows;
}
