///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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

SampleSourceFifo::SampleSourceFifo(uint32_t size, uint32_t samplesChunkSize) :
    m_size(size),
    m_samplesChunkSize(samplesChunkSize),
    m_i(0)
{
    assert(samplesChunkSize < m_size/4);
    m_data.resize(2*m_size);
}


SampleSourceFifo::~SampleSourceFifo()
{}

void SampleSourceFifo::read(SampleVector::iterator& begin, SampleVector::iterator& end)
{
    QMutexLocker mutexLocker(&m_mutex);
    end = m_data.begin() + m_size + m_i;
    begin = end - m_samplesChunkSize;
    emit dataRead();
}

void SampleSourceFifo::getReadIterator(SampleVector::iterator& beginRead)
{
    QMutexLocker mutexLocker(&m_mutex);
    beginRead = m_data.begin() + m_size + m_i - m_samplesChunkSize;
    emit dataRead();
}

void SampleSourceFifo::write(const Sample& sample)
{
    m_data[m_i] = sample;
    m_data[m_i+m_size] = sample;

    {
        QMutexLocker mutexLocker(&m_mutex);
        m_i = (m_i+1) % m_size;
    }
}

void SampleSourceFifo::getWriteIterator(SampleVector::iterator& writeAt)
{
    writeAt = m_data.begin() + m_i;
}

void SampleSourceFifo::bumpIndex()
{
    m_data[m_i+m_size] = m_data[m_i];
    m_i = (m_i+1) % m_size;

    {
        QMutexLocker mutexLocker(&m_mutex);
        m_i = (m_i+1) % m_size;
    }
}
