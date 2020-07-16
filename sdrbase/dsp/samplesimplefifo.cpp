///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "samplesimplefifo.h"

void SampleSimpleFifo::create(unsigned int s)
{
	m_size = 0;
	m_fill = 0;
	m_head = 0;
	m_tail = 0;

	m_data.resize(s);
	m_size = m_data.size();
}

void SampleSimpleFifo::reset()
{
	m_fill = 0;
	m_head = 0;
	m_tail = 0;
}

SampleSimpleFifo::SampleSimpleFifo() :
	m_data()
{
	m_size = 0;
	m_fill = 0;
	m_head = 0;
	m_tail = 0;
}

SampleSimpleFifo::SampleSimpleFifo(int size) :
	m_data()
{
	create(size);
}

SampleSimpleFifo::SampleSimpleFifo(const SampleSimpleFifo& other) :
    m_data(other.m_data)
{
	m_size = m_data.size();
	m_fill = 0;
	m_head = 0;
	m_tail = 0;
}

SampleSimpleFifo::~SampleSimpleFifo()
{
	m_size = 0;
}

bool SampleSimpleFifo::setSize(int size)
{
	create(size);
	return m_data.size() == (unsigned int) size;
}

unsigned int SampleSimpleFifo::write(SampleVector::const_iterator begin, SampleVector::const_iterator end)
{
	unsigned int count = end - begin;
	unsigned int total;
	unsigned int remaining;
	unsigned int len;

	total = count;
	remaining = total;

    while (remaining > 0)
    {
		len = std::min(remaining, m_size - m_tail);
		std::copy(begin, begin + len, m_data.begin() + m_tail);
		m_tail += len;
		m_tail %= m_size;
		m_fill += len;
		begin += len;
		remaining -= len;
	}

    if (m_fill >= m_size) // has wrapped around at least once
    {
        m_head = m_tail;
        m_fill = m_size;
    }

    return m_fill;
}

unsigned int SampleSimpleFifo::readBegin(unsigned int count,
	SampleVector::iterator* part1Begin, SampleVector::iterator* part1End,
	SampleVector::iterator* part2Begin, SampleVector::iterator* part2End)
{
	unsigned int total;
	unsigned int remaining;
	unsigned int len;
	unsigned int head = m_head;

	total = count;
	remaining = total;

    if (remaining > 0)
    {
		len = std::min(remaining, m_size - head);
		*part1Begin = m_data.begin() + head;
		*part1End = m_data.begin() + head + len;
		head += len;
		head %= m_size;
		remaining -= len;
	}
    else
    {
		*part1Begin = m_data.end();
		*part1End = m_data.end();
	}

    if (remaining > 0)
    {
		len = std::min(remaining, m_size - head);
		*part2Begin = m_data.begin() + head;
		*part2End = m_data.begin() + head + len;
	}
    else
    {
		*part2Begin = m_data.end();
		*part2End = m_data.end();
	}

	return total;
}

unsigned int SampleSimpleFifo::readCommit(unsigned int count)
{
	if (count > m_fill)
    {
		qCritical("SampleSinkFifo::readCommit: cannot commit more than available samples");
		count = m_fill;
	}

    m_head = (m_head + count) % m_size;
	m_fill -= count;

	return count;
}