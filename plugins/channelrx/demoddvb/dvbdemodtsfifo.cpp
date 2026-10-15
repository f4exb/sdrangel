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

#include "dvbdemodtsfifo.h"

DVBDemodTSFifo::DVBDemodTSFifo(int size, bool needsReader) :
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

void DVBDemodTSFifo::attachLocked()
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

void DVBDemodTSFifo::detachLocked()
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

void DVBDemodTSFifo::attachReader()
{
    QMutexLocker locker(&m_mutex);
    attachLocked();
}

void DVBDemodTSFifo::detachReader()
{
    QMutexLocker locker(&m_mutex);
    detachLocked();
    m_dataAvailable.wakeAll();
}

void DVBDemodTSFifo::write(const uint8_t *data, int length)
{
    if (!isAccepting() || (length <= 0)) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    writeLocked(data, length);
}

void DVBDemodTSFifo::writePackets(const uint8_t *packets, int count)
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

void DVBDemodTSFifo::writeLocked(const uint8_t *data, int length)
{
    if (m_buffer.empty()) {
        return; // Detached meanwhile
    }

    size_t free = m_buffer.size() - m_fill;

    // Dropped whole, so the stream stays in packets
    if ((size_t) length > free)
    {
        m_overflows += length;
        PROFILER_ADD("DVBDemod TS FIFO overflow (count = writes dropping data)", 0)

        if (m_needsReader && m_sinceRead.isValid() && (m_sinceRead.elapsed() > m_readerGoneMs))
        {
            qDebug("DVBDemodTSFifo::writeLocked: nothing read for %lld s: no reader", (long long) (m_sinceRead.elapsed() / 1000));
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

int DVBDemodTSFifo::read(uint8_t *data, int length, int timeoutMs)
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

void DVBDemodTSFifo::reset()
{
    QMutexLocker locker(&m_mutex);
    m_readPos = 0;
    m_writePos = 0;
    m_fill = 0;
}

int DVBDemodTSFifo::fill()
{
    QMutexLocker locker(&m_mutex);
    return (int) m_fill;
}

uint64_t DVBDemodTSFifo::getOverflows()
{
    QMutexLocker locker(&m_mutex);
    return m_overflows;
}
