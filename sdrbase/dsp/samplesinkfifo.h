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

#ifndef INCLUDE_SAMPLEFIFO_H
#define INCLUDE_SAMPLEFIFO_H

#include <QObject>
#include <QMutex>
#include <QTime>
#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API SampleSinkFifo : public QObject {
	Q_OBJECT

private:
	QMutex m_mutex;
	QTime m_msgRateTimer;
	int m_suppressed;

	SampleVector m_data;

	uint m_size;
	uint m_fill;
	uint m_head;
	uint m_tail;

	void create(uint s);

public:
	SampleSinkFifo(QObject* parent = NULL);
	SampleSinkFifo(int size, QObject* parent = NULL);
	~SampleSinkFifo();

	bool setSize(int size);
	inline uint size() const { return m_size; }
	inline uint fill() { QMutexLocker mutexLocker(&m_mutex); uint fill = m_fill; return fill; }

	uint write(const quint8* data, uint count);
	uint write(SampleVector::const_iterator begin, SampleVector::const_iterator end);

	uint read(SampleVector::iterator begin, SampleVector::iterator end);

	uint readBegin(uint count,
		SampleVector::iterator* part1Begin, SampleVector::iterator* part1End,
		SampleVector::iterator* part2Begin, SampleVector::iterator* part2End);
	uint readCommit(uint count);

signals:
	void dataReady();
};

#endif // INCLUDE_SAMPLEFIFO_H
