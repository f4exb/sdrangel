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

#ifndef INCLUDE_SAMPLEMIFIFO_H
#define INCLUDE_SAMPLEMIFIFO_H

#include <QObject>
#include <QMutex>
#include <QTime>
#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API SampleMIFifo : public QObject {
	Q_OBJECT

public:
	SampleMIFifo(QObject* parent = nullptr);
	SampleMIFifo(unsigned int nbStreams, unsigned int size, QObject* parent = nullptr);

	void init(unsigned int nbStreams, unsigned int size);
	inline unsigned int size() const { return m_size; }

    inline unsigned int fillSync()
    {
        QMutexLocker mutexLocker(&m_mutex);
        unsigned int fill = m_fill;
        return fill;
    }

    inline unsigned int fillAsync(unsigned int stream)
    {
        if (stream >= m_nbStreams) {
            return 0;
        }

        QMutexLocker mutexLocker(&m_mutex);
        unsigned int fill = m_vfill[stream];
        return fill;
    }

    const std::vector<SampleVector>& getData() { return m_data; }
    unsigned int getNbStreams() const { return m_nbStreams; }

	unsigned int writeSync(const quint8* data, unsigned int count); //!< de-interleaved data in input with count bytes for each stream
	unsigned int writeSync(std::vector<SampleVector::const_iterator> vbegin, unsigned int count);
	unsigned int readSync(unsigned int count,
		std::vector<SampleVector::const_iterator*> vpart1Begin, std::vector<SampleVector::const_iterator*> vpart1End,
		std::vector<SampleVector::const_iterator*> vpart2Begin, std::vector<SampleVector::const_iterator*> vpart2End);
	unsigned int readSync(unsigned int count,
		int& ipart1Begin, int& ipart1End,
		int& ipart2Begin, int& ipart2End);
	unsigned int readCommitSync(unsigned int count);

	unsigned int writeAsync(const quint8* data, unsigned int count, unsigned int stream);
	unsigned int writeAsync(SampleVector::const_iterator begin, unsigned int count, unsigned int stream);
	unsigned int readAsync(unsigned int count,
		SampleVector::const_iterator* part1Begin, SampleVector::const_iterator* part1End,
		SampleVector::const_iterator* part2Begin, SampleVector::const_iterator* part2End,
        unsigned int stream);
	unsigned int readCommitAsync(unsigned int count, unsigned int stream);

signals:
	void dataSyncReady();
	void dataAsyncReady(int streamIndex);

private:
	QMutex m_mutex;

	std::vector<SampleVector> m_data;
    unsigned int m_nbStreams;
	unsigned int m_size;

    // Synchronous
	int m_suppressed;
	QTime m_msgRateTimer;
	unsigned int m_fill;
	unsigned int m_head;
	unsigned int m_tail;

    // Asynchronous
	std::vector<int> m_vsuppressed;
	std::vector<QTime> m_vmsgRateTimer;
	std::vector<unsigned int> m_vfill;
	std::vector<unsigned int> m_vhead;
	std::vector<unsigned int> m_vtail;
};

#endif // INCLUDE_SAMPLEMIFIFO_H
