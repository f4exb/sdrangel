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

#include "samplesourcefifo.h"

const unsigned int SampleSourceFifo::m_rwDivisor = 2;
const unsigned int SampleSourceFifo::m_guardDivisor = 10;

SampleSourceFifo::SampleSourceFifo(QObject *parent) :
    QObject(parent)
{}

SampleSourceFifo::SampleSourceFifo(unsigned int size, QObject *parent) :
    QObject(parent)
{
    resize(size);
}

void SampleSourceFifo::resize(unsigned int size)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_size = size;
    m_lowGuard = m_size / m_guardDivisor;
    m_highGuard = m_size - (m_size/m_guardDivisor);
    m_midPoint = m_size / m_rwDivisor;
	m_readCount = 0;
    m_readHead = 0;
    m_writeHead = m_midPoint;
    m_data.resize(size);
}

void SampleSourceFifo::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
	m_readCount = 0;
    m_readHead = 0;
    m_writeHead = m_midPoint;
}

SampleSourceFifo::~SampleSourceFifo()
{}

void SampleSourceFifo::read(
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

    emit dataRead();
}

void SampleSourceFifo::write(
    unsigned int amount,
    unsigned int& ipart1Begin, unsigned int& ipart1End, // first part offsets where to write
    unsigned int& ipart2Begin, unsigned int& ipart2End  // second part offsets
)
{
    QMutexLocker mutexLocker(&m_mutex);
    unsigned int rwDelta = m_writeHead >= m_readHead ? m_writeHead - m_readHead : m_size - (m_readHead - m_writeHead);

    if (rwDelta < m_lowGuard)
    {
        qWarning("SampleSourceFifo::write: underrun (write too slow) using %d old samples", m_midPoint - m_lowGuard);
        m_writeHead = m_readHead + m_midPoint < m_size ? m_readHead + m_midPoint : m_readHead + m_midPoint - m_size;
    }
    else if (rwDelta > m_highGuard)
    {
        qWarning("SampleSourceFifo::write: overrun (read too slow) dropping %d samples", m_highGuard - m_midPoint);
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

unsigned int SampleSourceFifo::getSizePolicy(unsigned int sampleRate)
{
    return (sampleRate/100)*64; // .64s
}