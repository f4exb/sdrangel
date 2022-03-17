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
#include <QElapsedTimer>
#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API SampleSinkFifo : public QObject {
	Q_OBJECT

private:
	QElapsedTimer m_msgRateTimer;
	int m_suppressed;
	SampleVector m_data;
	int m_total;
	unsigned int m_writtenSignalCount;
	unsigned int m_writtenSignalRateDivider;
	QMutex m_mutex;

	unsigned int m_size;
	unsigned int m_fill;
	unsigned int m_head;
	unsigned int m_tail;
	QString m_label;

	void create(unsigned int s);

public:
	SampleSinkFifo(QObject* parent = nullptr);
	SampleSinkFifo(int size, QObject* parent = nullptr);
    SampleSinkFifo(const SampleSinkFifo& other);
	~SampleSinkFifo();

	bool setSize(int size);
    void reset();
	void setWrittenSignalRateDivider(unsigned int divider);
	inline unsigned int size() { QMutexLocker mutexLocker(&m_mutex); unsigned int size = m_size; return size; }
	inline unsigned int fill() { QMutexLocker mutexLocker(&m_mutex); unsigned int fill = m_fill; return fill; }

	unsigned int write(const quint8* data, unsigned int count);
	unsigned int write(SampleVector::const_iterator begin, SampleVector::const_iterator end);

	unsigned int read(SampleVector::iterator begin, SampleVector::iterator end);

	unsigned int readBegin(unsigned int count,
		SampleVector::iterator* part1Begin, SampleVector::iterator* part1End,
		SampleVector::iterator* part2Begin, SampleVector::iterator* part2End);
	unsigned int readCommit(unsigned int count);
	void setLabel(const QString& label) { m_label = label; }
    static unsigned int getSizePolicy(unsigned int sampleRate);

signals:
	void dataReady();
	void written(int nsamples, qint64 timestamp);
	void overflow(int nsamples);
	void underflow(int nsamples);
};

#endif // INCLUDE_SAMPLEFIFO_H
