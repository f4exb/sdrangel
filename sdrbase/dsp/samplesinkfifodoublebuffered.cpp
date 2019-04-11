///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include "samplesinkfifodoublebuffered.h"

SampleSinkFifoDoubleBuffered::SampleSinkFifoDoubleBuffered(uint32_t size, uint32_t signalThreshold) :
    m_size(size),
    m_signalThreshold(signalThreshold),
    m_i(0),
    m_count(0),
    m_readIndex(0),
    m_readCount(0)
{
	assert(m_signalThreshold < m_size/2);
	m_data.resize(2*m_size);
}

SampleSinkFifoDoubleBuffered::~SampleSinkFifoDoubleBuffered()
{
}

void SampleSinkFifoDoubleBuffered::getWriteIterator(SampleVector::iterator& it1)
{
	it1 = m_data.begin() + m_i;
}

void SampleSinkFifoDoubleBuffered::bumpIndex(SampleVector::iterator& it1)
{
	m_data[m_i+m_size] = m_data[m_i];
	m_i = (m_i+1) % m_size;
	it1 = m_data.begin() + m_i;

	if (m_count < m_signalThreshold)
	{
		m_count++;
	}
	else
	{
		QMutexLocker mutexLocker(&m_mutex);
		m_readIndex = m_i + m_size - m_count;
		m_readCount = m_count;
		m_count = 0;
		emit dataReady();
	}
}

void SampleSinkFifoDoubleBuffered::read(SampleVector::iterator& begin, SampleVector::iterator& end)
{
	QMutexLocker mutexLocker(&m_mutex);

	begin = m_data.begin() + m_readIndex;
	end = begin + m_readCount;
}

