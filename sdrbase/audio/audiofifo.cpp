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

#include <string.h>
#include <QTime>
#include "dsp/dsptypes.h"
#include "audio/audiofifo.h"
#include "audio/audionetsink.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

AudioFifo::AudioFifo() :
	m_fifo(nullptr),
	m_sampleSize(sizeof(AudioSample))
{
	m_size = 0;
	m_fill = 0;
	m_head = 0;
	m_tail = 0;
}

AudioFifo::AudioFifo(uint32_t numSamples) :
	m_fifo(nullptr),
    m_sampleSize(sizeof(AudioSample))
{
	QMutexLocker mutexLocker(&m_mutex);

	create(numSamples);
}

AudioFifo::~AudioFifo()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_fifo)
	{
		delete[] m_fifo;
		m_fifo = nullptr;
	}

	m_size = 0;
}

bool AudioFifo::setSize(uint32_t numSamples)
{
	QMutexLocker mutexLocker(&m_mutex);

	return create(numSamples);
}

bool AudioFifo::setSampleSize(uint32_t sampleSize, uint32_t numSamples)
{
	QMutexLocker mutexLocker(&m_mutex);
    m_sampleSize = sampleSize;
	return create(numSamples);
}

uint32_t AudioFifo::write(const quint8* data, uint32_t numSamples)
{
	uint32_t total;
	uint32_t remaining;
	uint32_t copyLen;

	if (!m_fifo) {
		return 0;
	}

	m_mutex.lock();

	total = MIN(numSamples, m_size - m_fill);
	remaining = total;

	while (remaining != 0)
	{
		if (isFull())
		{
			m_mutex.unlock();

			if (total - remaining > 0) {
				emit dataReady();
			}

			return total - remaining; // written so far
		}

		copyLen = MIN(remaining, m_size - m_fill);
		copyLen = MIN(copyLen, m_size - m_tail);
		memcpy(m_fifo + (m_tail * m_sampleSize), data, copyLen * m_sampleSize);
		m_tail += copyLen;
		m_tail %= m_size;
		m_fill += copyLen;
		data += copyLen * m_sampleSize;
		remaining -= copyLen;
	}

	m_mutex.unlock();

	emit dataReady();

	if (total < numSamples)
	{
		qCritical("AudioFifo::write: (%s) overflow %u samples",
			qPrintable(m_label), numSamples - total);
		emit overflow(numSamples - total);
	}

	return total;
}

uint32_t AudioFifo::read(quint8* data, uint32_t numSamples)
{
	uint32_t total;
	uint32_t remaining;
	uint32_t copyLen;

	if (!m_fifo) {
		return 0;
	}

	m_mutex.lock();

	total = MIN(numSamples, m_fill);
	remaining = total;

	while (remaining != 0)
	{
		if (isEmpty())
		{
			m_mutex.unlock();
			return total - remaining; // read so far
		}

		copyLen = MIN(remaining, m_fill);
		copyLen = MIN(copyLen, m_size - m_head);
		memcpy(data, m_fifo + (m_head * m_sampleSize), copyLen * m_sampleSize);

		m_head += copyLen;
		m_head %= m_size;
		m_fill -= copyLen;
		data += copyLen * m_sampleSize;
		remaining -= copyLen;
	}

	m_mutex.unlock();
	return total;
}

bool AudioFifo::readOne(quint8* data)
{
	if ((!m_fifo) || isEmpty()) {
		return false;
	}

    memcpy(data, m_fifo + (m_head * m_sampleSize), m_sampleSize);
    m_head += 1;
    m_head %= m_size;
    m_fill -= 1;
    return true;
}

uint AudioFifo::drain(uint32_t numSamples)
{
	QMutexLocker mutexLocker(&m_mutex);

	if(numSamples > m_fill)
	{
		numSamples = m_fill;
	}

	m_head = (m_head + numSamples) % m_size;
	m_fill -= numSamples;

	return numSamples;
}

void AudioFifo::clear()
{
	QMutexLocker mutexLocker(&m_mutex);

	m_fill = 0;
	m_head = 0;
	m_tail = 0;
}

bool AudioFifo::create(uint32_t numSamples)
{
	if (m_fifo)
	{
		delete[] m_fifo;
		m_fifo = nullptr;
	}

	m_fill = 0;
	m_head = 0;
	m_tail = 0;

	m_fifo = new qint8[numSamples * m_sampleSize];
	m_size = numSamples;

	return true;
}
