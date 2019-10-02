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

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void SampleMIFifo::init(unsigned int nbStreams, unsigned int size)
{
    m_nbStreams = nbStreams;
    m_size = size;
	m_fill = 0;
	m_head = 0;
	m_tail = 0;
	m_data.resize(nbStreams);
    m_vfill.resize(nbStreams);
    m_vhead.resize(nbStreams);
    m_vtail.resize(nbStreams);

    for (unsigned int stream = 0; stream < nbStreams; stream++)
    {
        m_data[stream].resize(size);
        m_vfill[stream] = 0;
        m_vhead[stream] = 0;
        m_vtail[stream] = 0;
    }
}

SampleMIFifo::SampleMIFifo(QObject* parent) :
	QObject(parent)
{
	m_suppressed = -1;
	m_size = 0;
	m_fill = 0;
	m_head = 0;
	m_tail = 0;
}

SampleMIFifo::SampleMIFifo(unsigned int nbStreams, unsigned int size, QObject* parent) :
	QObject(parent)
{
	init(nbStreams, size);

	m_suppressed = -1;

    for (unsigned int stream = 0; stream < nbStreams; stream++)
    {
        m_vsuppressed[stream] = -1;
        m_vmsgRateTimer.push_back(QTime());
    }
}

unsigned int SampleMIFifo::writeSync(const quint8* data, unsigned int count)
{
	QMutexLocker mutexLocker(&m_mutex);
	unsigned int total;
	unsigned int remaining;
	unsigned int len;
    unsigned int byteCount = count;
	count /= sizeof(Sample);

	total = std::min(count, m_size - m_fill);

	if (total < count)
    {
		if (m_suppressed < 0)
        {
			m_suppressed = 0;
			m_msgRateTimer.start();
			qCritical("SampleMIFifo: overflow - dropping %u samples", count - total);
		}
        else
        {
			if (m_msgRateTimer.elapsed() > 2500)
            {
				qCritical("SampleMIFifo: %u messages dropped", m_suppressed);
				qCritical("SampleMIFifo: overflow - dropping %u samples", count - total);
				m_suppressed = -1;
			}
            else
            {
				m_suppressed++;
			}
		}
	}

	remaining = total;
	std::vector<const Sample*> vbegin;
    vbegin.resize(m_nbStreams);

    for (unsigned int stream = 0; stream < m_nbStreams; stream++) {
        vbegin[stream] = (const Sample*) &data[stream*byteCount];
    }

    while (remaining > 0)
    {
		len = std::min(remaining, m_size - m_tail);

        for (unsigned int stream = 0; stream < m_nbStreams; stream++)
        {
            std::copy(vbegin[stream], vbegin[stream] + len, m_data[stream].begin() + m_tail);
            vbegin[stream] += len;
        }

		m_tail += len;
		m_tail %= m_size;
		m_fill += len;
		remaining -= len;
	}

	if (m_fill > 0) {
		emit dataSyncReady();
    }

	return total;
}

unsigned int SampleMIFifo::writeSync(std::vector<SampleVector::const_iterator> vbegin, unsigned int count)
{
    if ((vbegin.size() != m_nbStreams)) {
        return 0;
    }

	QMutexLocker mutexLocker(&m_mutex);
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
			qCritical("SampleMIFifo::writeSync: overflow - dropping %u samples", count - total);
		}
        else
        {
			if (m_msgRateTimer.elapsed() > 2500)
            {
				qCritical("SampleMIFifo::writeSync: %u messages dropped", m_suppressed);
				qCritical("SampleMIFifo::writeSync: overflow - dropping %u samples", count - total);
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

        for (unsigned int stream = 0; stream < m_nbStreams; stream++)
        {
            std::copy(vbegin[stream], vbegin[stream] + len, m_data[stream].begin() + m_tail);
            vbegin[stream] += len;
        }

		m_tail += len;
		m_tail %= m_size;
		m_fill += len;
		remaining -= len;
	}

	if (m_fill > 0) {
		emit dataSyncReady();
    }

	return total;
}

unsigned int SampleMIFifo::readSync(unsigned int count,
    std::vector<SampleVector::const_iterator*> vpart1Begin, std::vector<SampleVector::const_iterator*> vpart1End,
    std::vector<SampleVector::const_iterator*> vpart2Begin, std::vector<SampleVector::const_iterator*> vpart2End)
{
    if ((vpart1Begin.size() != m_nbStreams)
     || (vpart2Begin.size() != m_nbStreams)
     || (vpart1End.size() != m_nbStreams)
     || (vpart2End.size() != m_nbStreams))
    {
        return 0;
    }

	QMutexLocker mutexLocker(&m_mutex);
	unsigned int total;
	unsigned int remaining;
	unsigned int len;
	unsigned int head = m_head;

	total = std::min(count, m_fill);

    if (total < count) {
		qCritical("SampleMIFifo::readSync: underflow - missing %u samples", count - total);
    }

	remaining = total;

    if (remaining > 0)
    {
		len = std::min(remaining, m_size - head);

        for (unsigned int stream = 0; stream < m_nbStreams; stream++)
        {
            *vpart1Begin[stream] = m_data[stream].begin() + head;
            *vpart1End[stream] = m_data[stream].begin() + head + len;
        }

		head += len;
		head %= m_size;
		remaining -= len;
	}
    else
    {
        for (unsigned int stream = 0; stream < m_nbStreams; stream++)
        {
    		*vpart1Begin[stream] = m_data[stream].end();
	    	*vpart1End[stream] = m_data[stream].end();
        }
	}

    if (remaining > 0)
    {
		len = std::min(remaining, m_size - head);

        for (unsigned int stream = 0; stream < m_nbStreams; stream++)
        {
            *vpart2Begin[stream] = m_data[stream].begin() + head;
            *vpart2End[stream] = m_data[stream].begin() + head + len;
        }
	}
    else
    {
        for (unsigned int stream = 0; stream < m_nbStreams; stream++)
        {
            *vpart2Begin[stream] = m_data[stream].end();
            *vpart2End[stream] = m_data[stream].end();
        }
	}

	return total;
}

unsigned int SampleMIFifo::readSync(unsigned int count,
    int& ipart1Begin, int& ipart1End,
    int& ipart2Begin, int& ipart2End)
{
	QMutexLocker mutexLocker(&m_mutex);
	unsigned int total;
	unsigned int remaining;
	unsigned int len;
	unsigned int head = m_head;

	total = std::min(count, m_fill);

    if (total < count) {
		qCritical("SampleMIFifo::readSync: underflow - missing %u samples", count - total);
    }

	remaining = total;

    if (remaining > 0)
    {
		len = std::min(remaining, m_size - head);
        ipart1Begin = head;
        ipart1End =head + len;
		head += len;
		head %= m_size;
		remaining -= len;
	}
    else
    {
  		ipart1Begin = m_size;
	    ipart1End = m_size;
	}

    if (remaining > 0)
    {
		len = std::min(remaining, m_size - head);
        ipart2Begin = head;
        ipart2End = head + len;
	}
    else
    {
  		ipart2Begin = m_size;
	    ipart2End = m_size;
	}

	return total;
}

unsigned int SampleMIFifo::readCommitSync(unsigned int count)
{
	QMutexLocker mutexLocker(&m_mutex);

	if (count > m_fill)
    {
		qCritical("SampleMIFifo::readCommitSync: cannot commit more than available samples");
		count = m_fill;
	}

    m_head = (m_head + count) % m_size;
	m_fill -= count;

	return count;
}

unsigned int SampleMIFifo::writeAsync(const quint8* data, unsigned int count, unsigned int stream)
{
    if (stream >= m_nbStreams) {
        return 0;
    }

	QMutexLocker mutexLocker(&m_mutex);
	unsigned int total;
	unsigned int remaining;
	unsigned int len;
	const Sample* begin = (const Sample*) data;
	count /= sizeof(Sample);
	total = std::min(count, m_size - m_vfill[stream]);

	if (total < count)
    {
		if (m_vsuppressed[stream] < 0)
        {
			m_vsuppressed[stream] = 0;
			m_msgRateTimer.start();
			qCritical("SampleSinkFifo::writeAsync[%u]: overflow - dropping %u samples", stream, count - total);
		}
        else
        {
			if (m_msgRateTimer.elapsed() > 2500)
            {
				qCritical("SampleSinkFifo::writeAsync[%u]: %u messages dropped", stream, m_vsuppressed[stream]);
				qCritical("SampleSinkFifo::writeAsync[%u]: overflow - dropping %u samples", stream, count - total);
				m_vsuppressed[stream] = -1;
			}
            else
            {
				m_vsuppressed[stream]++;
			}
		}
	}

	remaining = total;

    while (remaining > 0)
    {
		len = std::min(remaining, m_size - m_vtail[stream]);
		std::copy(begin, begin + len, m_data[stream].begin() + m_vtail[stream]);
		begin += len;
		m_vtail[stream] += len;
		m_vtail[stream] %= m_size;
		m_vfill[stream] += len;
		remaining -= len;
	}

	if (m_vfill[stream] > 0) {
		emit dataAsyncReady(stream);
    }

	return total;
}

unsigned int SampleMIFifo::writeAsync(SampleVector::const_iterator begin, unsigned int count, unsigned int stream)
{
    if (stream >= m_nbStreams) {
        return 0;
    }

	QMutexLocker mutexLocker(&m_mutex);
	unsigned int total;
	unsigned int remaining;
	unsigned int len;
	total = std::min(count, m_size - m_vfill[stream]);

    if (total < count)
    {
		if (m_vsuppressed[stream] < 0)
        {
			m_vsuppressed[stream] = 0;
			m_vmsgRateTimer[stream].start();
			qCritical("SampleSinkFifo::writeAsync[%u]: overflow - dropping %u samples", stream, count - total);
		}
        else
        {
			if (m_vmsgRateTimer[stream].elapsed() > 2500)
            {
				qCritical("SampleSinkFifo::writeAsync[%u]: %u messages dropped", stream, m_vsuppressed[stream]);
				qCritical("SampleSinkFifo::writeAsync[%u]: overflow - dropping %u samples", stream, count - total);
				m_vsuppressed[stream] = -1;
			}
            else
            {
				m_vsuppressed[stream]++;
			}
		}
	}

	remaining = total;

	while (remaining > 0)
    {
		len = std::min(remaining, m_size - m_vtail[stream]);
		std::copy(begin, begin + len, m_data[stream].begin() + m_vtail[stream]);
		begin += len;
        m_vtail[stream] += len;
		m_vtail[stream] %= m_size;
		m_vfill[stream] += len;
		remaining -= len;
	}

	if (m_vfill[stream] > 0) {
		emit dataAsyncReady(stream);
    }

	return total;
}

unsigned int SampleMIFifo::readAsync(unsigned int count,
    SampleVector::const_iterator* part1Begin, SampleVector::const_iterator* part1End,
    SampleVector::const_iterator* part2Begin, SampleVector::const_iterator* part2End,
    unsigned int stream)
{
    if (stream >= m_nbStreams) {
        return 0;
    }

	QMutexLocker mutexLocker(&m_mutex);
	unsigned int total;
	unsigned int remaining;
	unsigned int len;
	unsigned int head = m_vhead[stream];
	total = std::min(count, m_vfill[stream]);

	if (total < count) {
		qCritical("SampleSinkFifo::readAsync[%u]: underflow - missing %u samples", stream, count - total);
    }

	remaining = total;

	if (remaining > 0)
    {
		len = std::min(remaining, m_size - head);
		*part1Begin = m_data[stream].begin() + head;
		*part1End = m_data[stream].begin() + head + len;
		head += len;
		head %= m_size;
		remaining -= len;
	}
    else
    {
		*part1Begin = m_data[stream].end();
		*part1End = m_data[stream].end();
	}

	if (remaining > 0)
    {
		len = std::min(remaining, m_size - head);
		*part2Begin = m_data[stream].begin() + head;
		*part2End = m_data[stream].begin() + head + len;
	}
    else
    {
		*part2Begin = m_data[stream].end();
		*part2End = m_data[stream].end();
	}

	return total;
}

unsigned int SampleMIFifo::readCommitAsync(unsigned int count, unsigned int stream)
{
    if (stream >= m_nbStreams) {
        return 0;
    }

	QMutexLocker mutexLocker(&m_mutex);

	if (count > m_vfill[stream])
    {
		qCritical("SampleSinkFifo::readCommitAsync[%u]: cannot commit more than available samples", stream);
		count = m_vfill[stream];
	}

	m_vhead[stream] = (m_vhead[stream] + count) % m_size;
	m_vfill[stream] -= count;

	return count;
}
