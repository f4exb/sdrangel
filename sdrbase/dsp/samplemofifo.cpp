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

void SampleMOFifo::init(unsigned int nbStreams, unsigned int size)
{
    m_nbStreams = nbStreams;
    m_size = size;
	m_readCount = 0;
    m_readHead = size/2;
    m_writeHead = 0;
    m_data.resize(nbStreams);
    m_vReadCount.clear();
    m_vReadHead.clear();
    m_vWriteHead.clear();

    for (unsigned int stream = 0; stream < nbStreams; stream++)
    {
        m_data[stream].resize(size);
        m_vReadCount.push_back(0);
        m_vReadHead.push_back(size/2);
        m_vWriteHead.push_back(0);
    }
}

SampleMOFifo::SampleMOFifo(QObject *parent) :
    QObject(parent),
    m_nbStreams(0),
    m_size(0),
    m_readCount(0)
{}

SampleMOFifo::SampleMOFifo(unsigned int nbStreams, unsigned int size, QObject *parent) :
    QObject(parent)
{
    init(nbStreams, size);
}

SampleMOFifo::~SampleMOFifo()
{}

void SampleMOFifo::reset()
{
	m_readCount = 0;
    m_readHead = m_size/2;
    m_writeHead = 0;

    for (unsigned int stream = 0; stream < m_nbStreams; stream++)
    {
        m_data[stream].resize(m_size);
        m_vReadCount[stream] = 0;
        m_vReadHead[stream] = m_size/2;
        m_vWriteHead[stream] = 0;
    }
}

void SampleMOFifo::readSync(
    unsigned int amount,
    unsigned int& ipart1Begin, unsigned int& ipart1End,
    unsigned int& ipart2Begin, unsigned int& ipart2End
)
{
    QMutexLocker mutexLocker(&m_mutex);
    unsigned int spaceLeft = m_size - m_readHead;
    m_readCount += amount;

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
        unsigned int remaining = amount - spaceLeft;
        ipart1Begin = m_readHead;
        ipart1End = m_size;
        ipart2Begin = 0;
        ipart2End = remaining;
        m_readHead = remaining;
    }

    emit dataSyncRead();
}

void SampleMOFifo::writeSync(const std::vector<SampleVector::const_iterator>& vbegin, unsigned int amount)
{
    QMutexLocker mutexLocker(&m_mutex);
    unsigned int spaceLeft = m_size - m_writeHead;
    m_readCount = amount < m_readCount ? m_readCount - amount : 0;

    if (amount <= spaceLeft)
    {
        for (unsigned int stream = 0; stream < m_nbStreams; stream++) {
            std::copy(vbegin[stream], vbegin[stream] + amount, m_data[stream].begin() + m_writeHead);
        }

        m_writeHead += amount;
    }
    else
    {
        unsigned int remaining = amount - spaceLeft;

        for (unsigned int stream = 0; stream < m_nbStreams; stream++)
        {
            std::copy(vbegin[stream], vbegin[stream] + spaceLeft, m_data[stream].begin() + m_writeHead);
            std::copy(vbegin[stream] + spaceLeft, vbegin[stream] + amount, m_data[stream].begin());
        }

        m_writeHead = remaining;
    }
}

void SampleMOFifo::readAsync(
    unsigned int amount,
    unsigned int& ipart1Begin, unsigned int& ipart1End,
    unsigned int& ipart2Begin, unsigned int& ipart2End,
    unsigned int stream
)
{
    if (stream >= m_nbStreams) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);
    unsigned int spaceLeft = m_size - m_vReadHead[stream];
    m_vReadCount[stream] += amount;

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
        unsigned int remaining = amount - spaceLeft;
        ipart1Begin = m_vReadHead[stream];
        ipart1End = m_size;
        ipart2Begin = 0;
        ipart2End = remaining;
        m_readHead = remaining;
    }

    emit dataAsyncRead(stream);
}

void SampleMOFifo::writeAsync(const SampleVector::const_iterator& begin, unsigned int amount, unsigned int stream)
{
    QMutexLocker mutexLocker(&m_mutex);
    unsigned int spaceLeft = m_size - m_vWriteHead[stream];
    m_vReadCount[stream] = amount < m_vReadCount[stream] ? m_vReadCount[stream] - amount : 0;

    if (amount <= spaceLeft)
    {
        std::copy(begin, begin + amount, m_data[stream].begin() + m_vWriteHead[stream]);
        m_vWriteHead[stream] += amount;
    }
    else
    {
        unsigned int remaining = amount - spaceLeft;

        for (unsigned int stream = 0; stream < m_nbStreams; stream++)
        {
            std::copy(begin, begin + spaceLeft, m_data[stream].begin() + m_vWriteHead[stream]);
            std::copy(begin + spaceLeft, begin + amount, m_data[stream].begin());
            m_vWriteHead[stream] = remaining;
        }
    }
}
