///////////////////////////////////////////////////////////////////////////////////
 // Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include "maincore.h"
#include "samplesinkfifo.h"

//#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void SampleSinkFifo::create(unsigned int s)
{
	m_fill = 0;
	m_head = 0;
	m_tail = 0;

	m_data.resize(s);
	m_size = m_data.size();
}

void SampleSinkFifo::reset()
{
	QMutexLocker mutexLocker(&m_mutex);
	m_suppressed = -1;
	m_fill = 0;
	m_head = 0;
	m_tail = 0;
}

SampleSinkFifo::SampleSinkFifo(QObject* parent) :
	QObject(parent),
	m_data(),
	m_total(0),
	m_writtenSignalCount(0),
	m_writtenSignalRateDivider(1),
	m_mutex(QMutex::Recursive)
{
	m_suppressed = -1;
	m_size = 0;
	m_fill = 0;
	m_head = 0;
	m_tail = 0;
}

SampleSinkFifo::SampleSinkFifo(int size, QObject* parent) :
	QObject(parent),
	m_data(),
	m_total(0),
	m_writtenSignalCount(0),
	m_writtenSignalRateDivider(1),
	m_mutex(QMutex::Recursive)
{
	m_suppressed = -1;
	create(size);
}

SampleSinkFifo::SampleSinkFifo(const SampleSinkFifo& other) :
    QObject(other.parent()),
    m_data(other.m_data),
	m_total(0),
	m_writtenSignalCount(0),
	m_writtenSignalRateDivider(1),
	m_mutex(QMutex::Recursive)
{
  	m_suppressed = -1;
	m_size = m_data.size();
	m_fill = 0;
	m_head = 0;
	m_tail = 0;
}

SampleSinkFifo::~SampleSinkFifo()
{
	QMutexLocker mutexLocker(&m_mutex);
	m_size = 0;
}

bool SampleSinkFifo::setSize(int size)
{
	QMutexLocker mutexLocker(&m_mutex);
	create(size);
	return m_data.size() == (unsigned int)size;
}

void SampleSinkFifo::setWrittenSignalRateDivider(unsigned int divider)
{
	QMutexLocker mutexLocker(&m_mutex);
	m_writtenSignalRateDivider = divider;
}

unsigned int SampleSinkFifo::write(const quint8* data, unsigned int count)
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_size == 0) {
		return 0;
	}

	unsigned int total;
	unsigned int remaining;
	unsigned int len;
	const Sample* begin = (const Sample*)data;
	count /= sizeof(Sample);

	total = std::min(count, m_size - m_fill);

    if (total < count)
    {
		if (m_suppressed < 0)
        {
			m_suppressed = 0;
			m_msgRateTimer.start();
			qCritical("SampleSinkFifo::write: (%s) overflow - dropping %u samples",
				qPrintable(m_label), count - total);
			emit overflow(count - total);
		}
        else
        {
			if (m_msgRateTimer.elapsed() > 2500)
            {
				qCritical("SampleSinkFifo::write: (%s) %u messages dropped", qPrintable(m_label), m_suppressed);
				qCritical("SampleSinkFifo::write: (%s) overflow - dropping %u samples",
					qPrintable(m_label), count - total);
				emit overflow(count - total);
				m_suppressed = -1;
			}
            else
            {
				m_suppressed++;
			}
		}
	}

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

	if (m_fill > 0) {
		emit dataReady();
    }

	m_total += total;

	if (++m_writtenSignalCount >= m_writtenSignalRateDivider)
	{
		emit written(m_total, MainCore::instance()->getElapsedNsecs());
		m_total = 0;
		m_writtenSignalCount = 0;
	}

	return total;
}

unsigned int SampleSinkFifo::write(SampleVector::const_iterator begin, SampleVector::const_iterator end)
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_size == 0) {
		return 0;
	}

	unsigned int count = end - begin;
	unsigned int total;
	unsigned int remaining;
	unsigned int len;

	total = std::min(count, m_size - m_fill);

    if (total < count)
    {
		if (m_suppressed < 0)
        {
			m_suppressed = 0;
			m_msgRateTimer.start();
			qCritical("SampleSinkFifo::write: (%s) overflow - dropping %u samples",
				qPrintable(m_label), count - total);
			emit overflow(count - total);
		}
        else
        {
			if (m_msgRateTimer.elapsed() > 2500)
            {
				qCritical("SampleSinkFifo::write: (%s) %u messages dropped", qPrintable(m_label), m_suppressed);
				qCritical("SampleSinkFifo::write: (%s) overflow - dropping %u samples",
					qPrintable(m_label), count - total);
				emit overflow(count - total);
				m_suppressed = -1;
			}
            else
            {
				m_suppressed++;
			}
		}
	}

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

	if (m_fill > 0) {
		emit dataReady();
    }

	m_total += total;

	if (++m_writtenSignalCount >= m_writtenSignalRateDivider)
	{
		emit written(m_total, MainCore::instance()->getElapsedNsecs());
		m_total = 0;
		m_writtenSignalCount = 0;
	}

	return total;
}

unsigned int SampleSinkFifo::read(SampleVector::iterator begin, SampleVector::iterator end)
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_size == 0) {
		return 0;
	}

	unsigned int count = end - begin;
	unsigned int total;
	unsigned int remaining;
	unsigned int len;

	total = std::min(count, m_fill);

    if (total < count)
	{
		qCritical("SampleSinkFifo::read: (%s) underflow - missing %u samples",
			qPrintable(m_label), count - total);
		emit underflow(count - total);
    }

	remaining = total;

    while (remaining > 0)
    {
		len = std::min(remaining, m_size - m_head);
		std::copy(m_data.begin() + m_head, m_data.begin() + m_head + len, begin);
		m_head += len;
		m_head %= m_size;
		m_fill -= len;
		begin += len;
		remaining -= len;
	}

	return total;
}

unsigned int SampleSinkFifo::readBegin(unsigned int count,
	SampleVector::iterator* part1Begin, SampleVector::iterator* part1End,
	SampleVector::iterator* part2Begin, SampleVector::iterator* part2End)
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_size == 0) {
		return 0;
	}

	unsigned int total;
	unsigned int remaining;
	unsigned int len;
	unsigned int head = m_head;

	total = std::min(count, m_fill);

    if (total < count)
	{
		qCritical("SampleSinkFifo::readBegin: (%s) underflow - missing %u samples",
			qPrintable(m_label), count - total);
		emit underflow(count - total);
    }

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

unsigned int SampleSinkFifo::readCommit(unsigned int count)
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_size == 0) {
		return 0;
	}

	if (count > m_fill)
    {
		qCritical("SampleSinkFifo::readCommit: (%s) cannot commit more than available samples", qPrintable(m_label));
		count = m_fill;
	}

    m_head = (m_head + count) % m_size;
	m_fill -= count;

	return count;
}

unsigned int SampleSinkFifo::getSizePolicy(unsigned int sampleRate)
{
    return (sampleRate/100)*64; // .64s
}
