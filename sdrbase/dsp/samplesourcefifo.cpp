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

#include <assert.h>
#include "samplesourcefifo.h"

SampleSourceFifo::SampleSourceFifo(uint32_t size, uint32_t samplesChunkSize) :
    m_size(size),
    m_samplesChunkSize(samplesChunkSize),
    m_init(false)
{
    assert(samplesChunkSize <= m_size/4);
    m_data.resize(2*m_size);
    init();
}


SampleSourceFifo::~SampleSourceFifo()
{}

void SampleSourceFifo::init()
{
    memset(&m_data[0], 0, sizeof(2*m_size*sizeof(Sample)));
    m_ir = 0;
    m_iw = m_samplesChunkSize*2;
    m_init = true;
}

void SampleSourceFifo::read(SampleVector::iterator& beginRead, unsigned int nbSamples)
{
    QMutexLocker mutexLocker(&m_mutex);

    assert(nbSamples < m_samplesChunkSize/2);

    beginRead = m_data.begin() + m_size + m_ir;
    m_ir = (m_ir + nbSamples) % m_size;

    int i_delta = m_iw - m_ir;

    if (m_init)
    {
        emit dataWrite();
        m_init = false;
    }
    else if (i_delta > 0)
    {
        if (i_delta < m_samplesChunkSize)
        {
            emit dataWrite();
        }
    }
    else
    {
        if (i_delta + m_size < m_samplesChunkSize)
        {
            emit dataWrite();
        }
    }
}

void SampleSourceFifo::write(const Sample& sample)
{
    m_data[m_iw] = sample;
    m_data[m_iw+m_size] = sample;

    {
        QMutexLocker mutexLocker(&m_mutex);
        m_iw = (m_iw+1) % m_size;
    }
}

void SampleSourceFifo::getWriteIterator(SampleVector::iterator& writeAt)
{
    writeAt = m_data.begin() + m_iw;
}

void SampleSourceFifo::bumpIndex()
{
    m_data[m_iw+m_size] = m_data[m_iw];
    m_iw = (m_iw+1) % m_size;

    {
        QMutexLocker mutexLocker(&m_mutex);
        m_iw = (m_iw+1) % m_size;
    }
}
