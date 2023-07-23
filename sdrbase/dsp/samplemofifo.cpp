///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "samplemofifo.h"

const unsigned int SampleMOFifo::m_rwDivisor = 2;
const unsigned int SampleMOFifo::m_guardDivisor = 10;

SampleMOFifo::SampleMOFifo(QObject *parent) :
    QObject(parent),
    m_nbStreams(0)
{}

SampleMOFifo::SampleMOFifo(unsigned int nbStreams, unsigned int size, QObject *parent) :
    QObject(parent)
{
    init(nbStreams, size);
}

void SampleMOFifo::init(unsigned int nbStreams, unsigned int size)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_data.resize(nbStreams);
    m_vReadCount.resize(nbStreams);
    m_vReadHead.resize(nbStreams);
    m_vWriteHead.resize(nbStreams);
    m_nbStreams = nbStreams;
    resize(size);
}

void SampleMOFifo::resize(unsigned int size)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_size = size;
    m_lowGuard = m_size / m_guardDivisor;
    m_highGuard = m_size - (m_size/m_guardDivisor);
    m_midPoint = m_size / m_rwDivisor;

    for (unsigned int stream = 0; stream < m_nbStreams; stream++) {
        m_data[stream].resize(size);
    }

    reset();
}

void SampleMOFifo::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
	m_readCount = 0;
    m_readHead = 0;
    m_writeHead = m_midPoint;

    for (unsigned int stream = 0; stream < m_nbStreams; stream++)
    {
        m_vReadCount[stream] = 0;
        m_vReadHead[stream] = 0;
        m_vWriteHead[stream] = m_midPoint;
    }
}

SampleMOFifo::~SampleMOFifo()
{}

void SampleMOFifo::readSync(
    unsigned int amount,
    unsigned int& ipart1Begin, unsigned int& ipart1End, // first part offsets where to read
    unsigned int& ipart2Begin, unsigned int& ipart2End  // second part offsets
)
{
    QMutexLocker mutexLocker(&m_mutex);
    unsigned int spaceLeft = m_size - m_readHead;
    m_readCount = m_readCount + amount < m_size ? m_readCount + amount : m_size; // cannot exceed FIFO size

    if (amount <= spaceLeft)
    {
        ipart1Begin = m_readHead;
        ipart1End = m_readHead + amount;
        ipart2Begin = m_size;
        ipart2End = m_size;
        m_readHead += amount;
    }
    else
    {
        unsigned int remaining = (amount < m_size ? amount : m_size) - spaceLeft;
        ipart1Begin = m_readHead;
        ipart1End = m_size;
        ipart2Begin = 0;
        ipart2End = remaining;
        m_readHead = remaining;
    }

    emit dataReadSync();
}

void SampleMOFifo::writeSync(
    unsigned int amount,
    unsigned int& ipart1Begin, unsigned int& ipart1End, // first part offsets where to write
    unsigned int& ipart2Begin, unsigned int& ipart2End  // second part offsets
)
{
    QMutexLocker mutexLocker(&m_mutex);
    unsigned int rwDelta = m_writeHead >= m_readHead ? m_writeHead - m_readHead : m_size - (m_readHead - m_writeHead);

    if (rwDelta < m_lowGuard)
    {
        qWarning("SampleMOFifo::write: underrun (write too slow) using %d old samples", m_midPoint - m_lowGuard);
        m_writeHead = m_readHead + m_midPoint < m_size ? m_readHead + m_midPoint : m_readHead + m_midPoint - m_size;
    }
    else if (rwDelta > m_highGuard)
    {
        qWarning("SampleMOFifo::write: overrun (read too slow) dropping %d samples", m_highGuard - m_midPoint);
        m_writeHead = m_readHead + m_midPoint < m_size ? m_readHead + m_midPoint : m_readHead + m_midPoint - m_size;
    }

    unsigned int spaceLeft = m_size - m_writeHead;

    if (amount <= spaceLeft)
    {
        ipart1Begin = m_writeHead;
        ipart1End = m_writeHead + amount;
        ipart2Begin = m_size;
        ipart2End = m_size;
        m_writeHead += amount;
    }
    else
    {
        unsigned int remaining = (amount < m_size ? amount : m_size) - spaceLeft;
        ipart1Begin = m_writeHead;
        ipart1End = m_size;
        ipart2Begin = 0;
        ipart2End = remaining;
        m_writeHead = remaining;
    }

    m_readCount = amount < m_readCount ? m_readCount - amount : 0; // cannot be less than 0
}

void SampleMOFifo::readAsync(
    unsigned int amount,
    unsigned int& ipart1Begin, unsigned int& ipart1End,
    unsigned int& ipart2Begin, unsigned int& ipart2End,
    unsigned int stream
)
{
    QMutexLocker mutexLocker(&m_mutex);
    unsigned int spaceLeft = m_size - m_vReadHead[stream];
    m_vReadCount[stream] = m_vReadCount[stream] + amount < m_size ? m_vReadCount[stream] + amount : m_size; // cannot exceed FIFO size

    if (amount <= spaceLeft)
    {
        ipart1Begin = m_vReadHead[stream];
        ipart1End = m_vReadHead[stream] + amount;
        ipart2Begin = m_size;
        ipart2End = m_size;
        m_vReadHead[stream] += amount;
    }
    else
    {
        unsigned int remaining = (amount < m_size ? amount : m_size) - spaceLeft;
        ipart1Begin = m_vReadHead[stream];
        ipart1End = m_size;
        ipart2Begin = 0;
        ipart2End = remaining;
        m_vReadHead[stream] = remaining;
    }

    emit dataReadAsync(stream);
}

void SampleMOFifo::writeAsync( //!< in place write with given amount
    unsigned int amount,
    unsigned int& ipart1Begin, unsigned int& ipart1End,
    unsigned int& ipart2Begin, unsigned int& ipart2End,
    unsigned int stream
)
{
    QMutexLocker mutexLocker(&m_mutex);
    unsigned int rwDelta = m_vWriteHead[stream] >= m_vReadHead[stream] ?
        m_vWriteHead[stream] - m_vReadHead[stream] : m_size - (m_vReadHead[stream] - m_vWriteHead[stream]);

    if (rwDelta < m_lowGuard)
    {
        qWarning("SampleMOFifo::write: underrun on stream %u (write too slow) using %d old samples", stream, m_midPoint - m_lowGuard);
        m_vWriteHead[stream] = m_vReadHead[stream] + m_midPoint < m_size ?
            m_vReadHead[stream] + m_midPoint : m_vReadHead[stream] + m_midPoint - m_size;
    }
    else if (rwDelta > m_highGuard)
    {
        qWarning("SampleMOFifo::write: overrun on stream %u (read too slow) dropping %d samples", stream, m_highGuard - m_midPoint);
        m_vWriteHead[stream] = m_vReadHead[stream] + m_midPoint < m_size ?
            m_vReadHead[stream] + m_midPoint : m_vReadHead[stream] + m_midPoint - m_size;
    }

    unsigned int spaceLeft = m_size - m_vWriteHead[stream];

    if (amount <= spaceLeft)
    {
        ipart1Begin = m_vWriteHead[stream];
        ipart1End = m_vWriteHead[stream] + amount;
        ipart2Begin = m_size;
        ipart2End = m_size;
        m_vWriteHead[stream] += amount;
    }
    else
    {
        unsigned int remaining = (amount < m_size ? amount : m_size) - spaceLeft;
        ipart1Begin = m_vWriteHead[stream];
        ipart1End = m_size;
        ipart2Begin = 0;
        ipart2End = remaining;
        m_vWriteHead[stream] = remaining;
    }

    m_vReadCount[stream] = amount < m_vReadCount[stream] ? m_vReadCount[stream] - amount : 0; // cannot be less than 0
}

unsigned int SampleMOFifo::getSizePolicy(unsigned int sampleRate)
{
    return (sampleRate/100)*64; // .64s
}
