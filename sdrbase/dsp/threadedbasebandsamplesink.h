///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_THREADEDSAMPLESINK_H
#define INCLUDE_THREADEDSAMPLESINK_H

#include <dsp/basebandsamplesink.h>
#include <QMutex>

#include "samplesinkfifo.h"
#include "util/messagequeue.h"
#include "util/export.h"

class BasebandSampleSink;
class QThread;

/**
 * Because Qt is a piece of shit this class cannot be a nested protected class of ThreadedSampleSink
 * So let's make everything public
 */
class ThreadedBasebandSampleSinkFifo : public QObject {
	Q_OBJECT

public:
	ThreadedBasebandSampleSinkFifo(BasebandSampleSink* sampleSink, std::size_t size = 1<<18);
	~ThreadedBasebandSampleSinkFifo();
	void writeToFifo(SampleVector::const_iterator& begin, SampleVector::const_iterator& end);

	BasebandSampleSink* m_sampleSink;
	SampleSinkFifo m_sampleFifo;

public slots:
	void handleFifoData();
};

/**
 * This class is a wrapper for SampleSink that runs the SampleSink object in its own thread
 */
class SDRANGEL_API ThreadedBasebandSampleSink : public QObject {
	Q_OBJECT

public:
	ThreadedBasebandSampleSink(BasebandSampleSink* sampleSink, QObject *parent = 0);
	~ThreadedBasebandSampleSink();

	const BasebandSampleSink *getSink() const { return m_basebandSampleSink; }

	void start(); //!< this thread start()
	void stop();  //!< this thread exit() and wait()

	bool handleSinkMessage(const Message& cmd); //!< Send message to sink synchronously
	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly); //!< Feed sink with samples

	QString getSampleSinkObjectName() const;

protected:

	QThread *m_thread; //!< The thead object
	ThreadedBasebandSampleSinkFifo *m_threadedBasebandSampleSinkFifo;
	BasebandSampleSink* m_basebandSampleSink;
};

#endif // INCLUDE_THREADEDSAMPLESINK_H
