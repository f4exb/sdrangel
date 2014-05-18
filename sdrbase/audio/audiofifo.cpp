///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include <string.h>
#include <QTime>
#include "audio/audiofifo.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

AudioFifo::AudioFifo() :
	m_fifo(NULL)
{
	m_size = 0;
	m_fill = 0;
	m_head = 0;
	m_tail = 0;
}

AudioFifo::AudioFifo(uint sampleSize, uint numSamples) :
	m_fifo(NULL)
{
	QMutexLocker mutexLocker(&m_mutex);

	create(sampleSize, numSamples);
}

AudioFifo::~AudioFifo()
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_fifo != NULL) {
		delete[] m_fifo;
		m_fifo = NULL;
	}

	m_writeWaitCondition.wakeOne();
	m_readWaitCondition.wakeOne();

	m_size = 0;
}

bool AudioFifo::setSize(uint sampleSize, uint numSamples)
{
	QMutexLocker mutexLocker(&m_mutex);

	return create(sampleSize, numSamples);
}

uint AudioFifo::write(const quint8* data, uint numSamples, int timeout)
{
	QTime time;
	uint total;
	uint remaining;
	uint copyLen;

	if(m_fifo == NULL)
		return 0;

	time.start();
	m_mutex.lock();

	if(timeout == 0)
		total = MIN(numSamples, m_size - m_fill);
	else total = numSamples;

	remaining = total;
	while(remaining > 0) {
		if(isFull()) {
			if(time.elapsed() < timeout) {
				m_writeWaitLock.lock();
				m_mutex.unlock();
				int ms = timeout - time.elapsed();
				if(ms < 1)
					ms = 1;
				bool ok = m_writeWaitCondition.wait(&m_writeWaitLock, ms);
				m_writeWaitLock.unlock();
				if(!ok)
					return total - remaining;
				m_mutex.lock();
				if(m_fifo == NULL) {
					m_mutex.unlock();
					return 0;
				}
			} else {
				m_mutex.unlock();
				return total - remaining;
			}
		}
		copyLen = MIN(remaining, m_size - m_fill);
		copyLen = MIN(copyLen, m_size - m_tail);
		memcpy(m_fifo + (m_tail * m_sampleSize), data, copyLen * m_sampleSize);
		m_tail += copyLen;
		m_tail %= m_size;
		m_fill += copyLen;
		data += copyLen * m_sampleSize;
		remaining -= copyLen;
		m_readWaitCondition.wakeOne();
	}

	m_mutex.unlock();
	return total;
}

uint AudioFifo::read(quint8* data, uint numSamples, int timeout)
{
	QTime time;
	uint total;
	uint remaining;
	uint copyLen;

	if(m_fifo == NULL)
		return 0;

	time.start();
	m_mutex.lock();

	if(timeout == 0)
		total = MIN(numSamples, m_fill);
	else total = numSamples;

	remaining = total;
	while(remaining > 0) {
		if(isEmpty()) {
			if(time.elapsed() < timeout) {
				m_readWaitLock.lock();
				m_mutex.unlock();
				int ms = timeout - time.elapsed();
				if(ms < 1)
					ms = 1;
				bool ok = m_readWaitCondition.wait(&m_readWaitLock, ms);
				m_readWaitLock.unlock();
				if(!ok)
					return total - remaining;
				m_mutex.lock();
				if(m_fifo == NULL) {
					m_mutex.unlock();
					return 0;
				}
			} else {
				m_mutex.unlock();
				return total - remaining;
			}
		}

		copyLen = MIN(remaining, m_fill);
		copyLen = MIN(copyLen, m_size - m_head);
		memcpy(data, m_fifo + (m_head * m_sampleSize), copyLen * m_sampleSize);
		m_head += copyLen;
		m_head %= m_size;
		m_fill -= copyLen;
		data += copyLen * m_sampleSize;
		remaining -= copyLen;
		m_writeWaitCondition.wakeOne();
	}

	m_mutex.unlock();
	return total;
}

uint AudioFifo::drain(uint numSamples)
{
	QMutexLocker mutexLocker(&m_mutex);

	if(numSamples > m_fill)
		numSamples = m_fill;
	m_head = (m_head + numSamples) % m_size;
	m_fill -= numSamples;

	m_writeWaitCondition.wakeOne();
	return numSamples;
}

void AudioFifo::clear()
{
	QMutexLocker mutexLocker(&m_mutex);

	m_fill = 0;
	m_head = 0;
	m_tail = 0;

	m_writeWaitCondition.wakeOne();
}

bool AudioFifo::create(uint sampleSize, uint numSamples)
{
	if(m_fifo != NULL) {
		delete[] m_fifo;
		m_fifo = NULL;
	}

	m_sampleSize = sampleSize;
	m_size = 0;
	m_fill = 0;
	m_head = 0;
	m_tail = 0;

	if((m_fifo = new qint8[numSamples * m_sampleSize]) == NULL) {
		qDebug("out of memory");
		return false;
	}

	m_size = numSamples;
	return true;
}
