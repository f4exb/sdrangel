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

#include "samplemififo.h"

void SampleMIFifo::init(unsigned int nbStreams, unsigned int size)
{
    QMutexLocker mutexLocker(&m_mutex);
    m_nbStreams = nbStreams;
    m_size = size;
	m_fill = 0;
	m_head = 0;
    m_data.resize(nbStreams);
    m_vFill.clear();
    m_vHead.clear();

    for (unsigned int stream = 0; stream < nbStreams; stream++)
    {
        m_data[stream].resize(size);
        m_vFill.push_back(0);
        m_vHead.push_back(0);
    }
}

void SampleMIFifo::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
	m_fill = 0;
	m_head = 0;
    for (unsigned int stream = 0; stream < m_nbStreams; stream++)
    {
        m_vFill[stream] = 0;
        m_vHead[stream] = 0;
    }
}

SampleMIFifo::SampleMIFifo(QObject *parent) :
    QObject(parent),
    m_nbStreams(0),
    m_size(0),
    m_fill(0),
    m_head(0),
    m_mutex(QMutex::Recursive)
{
}

SampleMIFifo::SampleMIFifo(unsigned int nbStreams, unsigned int size, QObject *parent) :
    QObject(parent),
    m_mutex(QMutex::Recursive)
{
    init(nbStreams, size);
}

SampleMIFifo::~SampleMIFifo()
{
    qDebug("SampleMIFifo::~SampleMIFifo: m_fill: %u", m_fill);
    qDebug("SampleMIFifo::~SampleMIFifo: m_head: %u", m_head);

    for (unsigned int stream = 0; stream < m_data.size(); stream++)
    {
        qDebug("SampleMIFifo::~SampleMIFifo: m_data[%u] size: %lu", stream, m_data[stream].size());
        qDebug("SampleMIFifo::~SampleMIFifo: m_vFill[%u] %u", stream, m_vFill[stream]);
        qDebug("SampleMIFifo::~SampleMIFifo: m_vHead[%u] %u", stream, m_vHead[stream]);
    }
}

void SampleMIFifo::writeSync(const quint8* data, unsigned int count)
{
    QMutexLocker mutexLocker(&m_mutex);
    unsigned int spaceLeft = m_size - m_fill;
    unsigned int size = count / sizeof(Sample);

    if (size > m_size)
    {
        qWarning("SampleMIFifo::writeSync: input size %u greater that FIFO size %u: truncating input", size, m_size);
        size = m_size;
        count = m_size * sizeof(Sample);
    }

    for (unsigned int stream = 0; stream < m_data.size(); stream++)
    {
        if (size <= spaceLeft)
        {
            std::copy(&data[stream*count], &data[stream*count] + count, m_data[stream].begin() + m_fill);
            m_fill += size;
        }
        else
        {
            unsigned int remaining = size - spaceLeft;
            unsigned int bytesLeft = spaceLeft*sizeof(Sample);
            std::copy(&data[stream*count], &data[stream*count] + bytesLeft, m_data[stream].begin() + m_fill);
            std::copy(&data[stream*count] + bytesLeft, &data[stream*count] + count, m_data[stream].begin());
            m_fill = remaining;
        }
    }

    emit dataSyncReady();
}

void SampleMIFifo::writeSync(const std::vector<SampleVector::const_iterator>& vbegin, unsigned int size)
{
    if ((m_data.size() == 0) || (m_data.size() != vbegin.size())) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);
    unsigned int spaceLeft = m_size - m_fill;

    if (size > m_size)
    {
        qWarning("SampleMIFifo::writeSync: input size %u greater that FIFO size %u: truncating input", size, m_size);
        size = m_size;
    }

    if (size <= spaceLeft)
    {
        for (unsigned int stream = 0; stream < m_data.size(); stream++) {
            std::copy(vbegin[stream], vbegin[stream] + size, m_data[stream].begin() + m_fill);
        }

        m_fill += size;
    }
    else
    {
        unsigned int remaining = size - spaceLeft;

        for (unsigned int stream = 0; stream < m_data.size(); stream++)
        {
            std::copy(vbegin[stream], vbegin[stream] + spaceLeft, m_data[stream].begin() + m_fill);
            std::copy(vbegin[stream] + spaceLeft, vbegin[stream] + size, m_data[stream].begin());
        }

        m_fill = remaining;
    }

    emit dataSyncReady();
}

void SampleMIFifo::readSync(
    std::vector<SampleVector::const_iterator*> vpart1Begin, std::vector<SampleVector::const_iterator*> vpart1End,
    std::vector<SampleVector::const_iterator*> vpart2Begin, std::vector<SampleVector::const_iterator*> vpart2End
)
{
    if (m_data.size() == 0) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);
    vpart1Begin.resize(m_nbStreams);
    vpart1End.resize(m_nbStreams);
    vpart2Begin.resize(m_nbStreams);
    vpart2End.resize(m_nbStreams);

    if (m_head < m_fill)
    {
        for (unsigned int stream = 0; stream < m_data.size(); stream++)
        {
            *vpart1Begin[stream] = m_data[stream].begin() + m_head;
            *vpart1End[stream] = m_data[stream].begin() + m_fill;
            *vpart2Begin[stream] = m_data[stream].end();
            *vpart2End[stream] = m_data[stream].end();
        }
    }
    else
    {
        for (unsigned int stream = 0; stream < m_data.size(); stream++)
        {
            *vpart1Begin[stream] = m_data[stream].begin() + m_head;
            *vpart1End[stream] = m_data[stream].end();
            *vpart2Begin[stream] = m_data[stream].begin();
            *vpart2End[stream] = m_data[stream].begin() + m_fill;
        }
    }

    m_head = m_fill;
}

void SampleMIFifo::readSync(
        std::vector<unsigned int>& vpart1Begin, std::vector<unsigned int>& vpart1End,
        std::vector<unsigned int>& vpart2Begin, std::vector<unsigned int>& vpart2End
)
{
    if (m_data.size() == 0) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);
    vpart1Begin.resize(m_nbStreams);
    vpart1End.resize(m_nbStreams);
    vpart2Begin.resize(m_nbStreams);
    vpart2End.resize(m_nbStreams);


    if (m_head < m_fill)
    {
        for (unsigned int stream = 0; stream < m_data.size(); stream++)
        {
            vpart1Begin[stream] = m_head;
            vpart1End[stream] = m_fill;
            vpart2Begin[stream] = 0;
            vpart2End[stream] = 0;
        }
    }
    else
    {
        for (unsigned int stream = 0; stream < m_data.size(); stream++)
        {
            vpart1Begin[stream] = m_head;
            vpart1End[stream] = m_size;
            vpart2Begin[stream] = 0;
            vpart2End[stream] = m_fill;
        }
    }

    m_head = m_fill;
}

void SampleMIFifo::readSync(
    unsigned int& ipart1Begin, unsigned int& ipart1End,
    unsigned int& ipart2Begin, unsigned int& ipart2End
)
{
    if (m_data.size() == 0) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);

    if (m_head < m_fill)
    {
        ipart1Begin = m_head;
        ipart1End = m_fill;
        ipart2Begin  = 0;
        ipart2End = 0;
    }
    else
    {
        ipart1Begin = m_head;
        ipart1End = m_size;
        ipart2Begin = 0;
        ipart2End = m_fill;
    }

    m_head = m_fill;
}

void SampleMIFifo::writeAsync(const quint8* data, unsigned int count, unsigned int stream)
{
    if (stream >= m_nbStreams) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);
    unsigned int spaceLeft = m_size - m_vFill[stream];
    unsigned int size = count / sizeof(Sample);

    if (size > m_size)
    {
        qWarning("SampleMIFifo::writeAsync: input size %u greater that FIFO size %u: truncating input", size, m_size);
        size = m_size;
        count = m_size * sizeof(Sample);
    }

    if (size <= spaceLeft)
    {
        std::copy(&data[stream*count], &data[stream*count] + count, m_data[stream].begin() + m_vFill[stream]);
        m_vFill[stream] += size;
    }
    else
    {
        unsigned int remaining = size - spaceLeft;
        unsigned int bytesLeft = spaceLeft * sizeof(Sample);
        std::copy(&data[stream*count], &data[stream*count] + bytesLeft, m_data[stream].begin() + m_vFill[stream]);
        std::copy(&data[stream*count] + bytesLeft, &data[stream*count] + count,  m_data[stream].begin());
        m_vFill[stream] = remaining;
    }

    emit dataAsyncReady(stream);
}

void SampleMIFifo::writeAsync(const SampleVector::const_iterator& begin, unsigned int size, unsigned int stream)
{
    if (stream >= m_nbStreams) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);
    unsigned int spaceLeft = m_size < m_vFill[stream] ? 0 : m_size - m_vFill[stream];

    if (size > m_size)
    {
        qWarning("SampleMIFifo::writeAsync: input size %u greater that FIFO size %u: truncating input", size, m_size);
        size = m_size;
    }

    if (size <= spaceLeft)
    {
        std::copy(begin, begin + size, m_data[stream].begin() + m_vFill[stream]);
        m_vFill[stream] += size;
    }
    else
    {
        unsigned int remaining = size - spaceLeft;
        std::copy(begin, begin + spaceLeft, m_data[stream].begin() + m_vFill[stream]);
        std::copy(begin + spaceLeft, begin + size,  m_data[stream].begin());
        m_vFill[stream] = remaining;
    }

    emit dataAsyncReady(stream);
}

void SampleMIFifo::readAsync(
		SampleVector::const_iterator* part1Begin, SampleVector::const_iterator* part1End,
		SampleVector::const_iterator* part2Begin, SampleVector::const_iterator* part2End,
        unsigned int stream)
{
    if (stream >= m_nbStreams) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);

    if (m_vHead[stream] < m_vFill[stream])
    {
        *part1Begin = m_data[stream].begin() + m_vHead[stream];
        *part1End   = m_data[stream].begin() + m_vFill[stream];
        *part2Begin = m_data[stream].begin();
        *part2End   = m_data[stream].begin();
    }
    else
    {
        *part1Begin = m_data[stream].begin() + m_vHead[stream];
        *part1End   = m_data[stream].end();
        *part2Begin = m_data[stream].begin();
        *part2End   = m_data[stream].begin() + m_vFill[stream];
    }

    m_vHead[stream] = m_vFill[stream];
}

void SampleMIFifo::readAsync(
    unsigned int& ipart1Begin, unsigned int& ipart1End,
    unsigned int& ipart2Begin, unsigned int& ipart2End,
    unsigned int stream)
{
    if (stream >= m_data.size()) {
        return;
    }

    QMutexLocker mutexLocker(&m_mutex);

    if (m_vHead[stream] < m_vFill[stream])
    {
        ipart1Begin = m_vHead[stream];
        ipart1End   = m_vFill[stream];
        ipart2Begin = m_size;
        ipart2End   = m_size;
    }
    else
    {
        ipart1Begin = m_vHead[stream];
        ipart1End   = m_size;
        ipart2Begin = 0;
        ipart2End   = m_vFill[stream];
    }

    m_vHead[stream] = m_vFill[stream];
}
