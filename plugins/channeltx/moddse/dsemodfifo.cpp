///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "dsemodfifo.h"

DSEModFIFO::DSEModFIFO() :
    m_fifo(nullptr),
    m_size(0),
    m_writeIndex(0),
    m_readIndex(0),
    m_fifoEmpty(true)
{
}

DSEModFIFO::DSEModFIFO(uint32_t numSamples) :
    m_fifo(nullptr),
    m_size(0),
    m_writeIndex(0),
    m_readIndex(0),
    m_fifoEmpty(true)
{
    QMutexLocker mutexLocker(&m_mutex);
    create(numSamples);
}

DSEModFIFO::~DSEModFIFO()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_fifo)
    {
        delete[] m_fifo;
        m_fifo = nullptr;
    }

    m_size = 0;
}

void DSEModFIFO::setSize(uint32_t numSamples)
{
    QMutexLocker mutexLocker(&m_mutex);
    create(numSamples);
}

void DSEModFIFO::create(uint32_t numSamples)
{
    if (m_fifo)
    {
        delete[] m_fifo;
        m_fifo = nullptr;
    }

    m_writeIndex = 0;
    m_readIndex = 0;
    m_fifo = new int16_t[numSamples];
    m_size = numSamples;
}

void DSEModFIFO::clear()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_writeIndex = 0;
    m_readIndex = 0;
    m_fifoEmpty = true;
}

uint32_t DSEModFIFO::write(const int16_t* data, uint32_t numSamples)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_readIndex > m_writeIndex)
    {
        int rem = m_readIndex - m_writeIndex;
        int written = std::min(rem, (int) numSamples);
        std::copy(data, data + written, &m_fifo[m_writeIndex]);
        m_writeIndex += written;
        return written;
    }

    int remSize = m_size - m_writeIndex;
    int written = std::min(remSize, (int) numSamples);
    std::copy(data, data + written, &m_fifo[m_writeIndex]);
    m_writeIndex += written;
    m_writeIndex = m_writeIndex == (int) m_size ? 0 : m_writeIndex;

    if (written == (int) numSamples) {
        return written;
    }

    // wrap
    int remWrite = (int) numSamples - written;
    int remWritten = std::min(remWrite, m_readIndex);
    std::copy(data + written, data + written + remWritten, m_fifo);
    m_writeIndex = remWritten;
    return written + remWritten;
}

uint32_t DSEModFIFO::readOne(int16_t* data)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_readIndex == m_writeIndex)
    {
        m_fifoEmpty = true;
        *data = 0;
        return 0;
    }
    else
    {
        m_fifoEmpty = false;
    }

    *data = m_fifo[m_readIndex++];
    m_readIndex = m_readIndex == (int) m_size ? 0 : m_readIndex;
    return 1;
}

uint32_t DSEModFIFO::read(int16_t* data, uint32_t numSamples)
{
    QMutexLocker mutexLocker(&m_mutex);

    if ((uint32_t) fillUnlocked() < numSamples)   // the mutex is already held
    {
        m_fifoEmpty = true;
        std::fill(data, data + numSamples, 0);
        return 0;
    }

    m_fifoEmpty = false;

    for (uint32_t i = 0; i < numSamples; i++)
    {
        data[i] = m_fifo[m_readIndex++];
        m_readIndex = m_readIndex == (int) m_size ? 0 : m_readIndex;
    }

    return numSamples;
}

int DSEModFIFO::getFill() const
{
    // Read and written from the processor and the baseband threads, so under the same mutex as
    // read() and write(): a torn pair of indices reads as a fill of its own, and a stale zero
    // told an API transmission it had drained when it had not. Anything that already holds the
    // mutex calls fillUnlocked() instead - QMutex does not nest, and read() calling this one
    // deadlocked the baseband thread on its first call: no samples, no spectrum, no transmission
    QMutexLocker locker(&m_mutex);
    return fillUnlocked();
}

int DSEModFIFO::fillUnlocked() const
{
    if (m_readIndex > m_writeIndex) {
        return m_size - (m_readIndex - m_writeIndex);
    } else {
        return m_writeIndex - m_readIndex;
    }
}
