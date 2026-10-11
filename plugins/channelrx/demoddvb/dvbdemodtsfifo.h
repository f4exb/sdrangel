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

#ifndef INCLUDE_DVBDEMODTSFIFO_H
#define INCLUDE_DVBDEMODTSFIFO_H

#include <atomic>
#include <cstdint>
#include <vector>

#include <QElapsedTimer>
#include <QMutex>
#include <QWaitCondition>

// Transport stream bytes from the demodulator to the video player, which may run in different threads.
// Data that doesn't fit is dropped, as the demodulator must never wait
// With needsReader, as the demodulator's is (in a server build nothing reads it), nothing's kept, and the buffer isn't
// allocated, until a reader attaches (attachReader(), or its first read()); it's detached by detachReader(), or when
// nothing's been read for a long while (a reader gone without detaching)
class DVBDemodTSFifo
{
public:
    explicit DVBDemodTSFifo(int size = 8 * 1024 * 1024, bool needsReader = false);

    void write(const uint8_t *data, int length);
    //! Whole 188-byte packets: as many as fit, the rest dropped
    void writePackets(const uint8_t *packets, int count);
    //! Read up to length bytes, waiting up to timeoutMs for some. Returns 0 on timeout
    int read(uint8_t *data, int length, int timeoutMs);
    void reset();
    int fill();
    uint64_t getOverflows();
    //! A reader (the video player) is about to read: written data's kept from now, emptied first. From any thread
    void attachReader();
    //! The reader's gone: nothing's kept, and the buffer's freed. From any thread
    void detachReader();
    //! Data's kept (a reader's attached, or it doesn't need one). Lock free, for the writer to skip what it'd drop
    bool isAccepting() const { return m_accepting.load(std::memory_order_relaxed); }

private:
    //! With needsReader: overflowing, and nothing read for this long, the reader's taken to be gone
    static constexpr qint64 m_readerGoneMs = 30000;

    void writeLocked(const uint8_t *data, int length);
    void attachLocked();
    void detachLocked();

    QMutex m_mutex;
    QWaitCondition m_dataAvailable;
    std::vector<uint8_t> m_buffer;
    size_t m_size;
    size_t m_readPos;
    size_t m_writePos;
    size_t m_fill;
    uint64_t m_overflows;   //!< Bytes dropped
    const bool m_needsReader;
    std::atomic<bool> m_accepting;
    QElapsedTimer m_sinceRead;  //!< With needsReader: since the reader last read
};

#endif // INCLUDE_DVBDEMODTSFIFO_H
