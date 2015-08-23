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

#include <QMutex>
#include "samplesink.h"
#include "dsp/samplefifo.h"
#include "util/messagequeue.h"
#include "util/export.h"

class SampleSink;
class QThread;

/**
 * Because Qt is a piece of shit this class cannot be a nested protected class of ThreadedSampleSink
 * So let's make everything public
 */
class ThreadedSampleFifo : public QObject {
	Q_OBJECT

public:
	ThreadedSampleFifo(SampleSink* sampleSink, std::size_t size = 1<<18);
	~ThreadedSampleFifo();
	void writeToFifo(SampleVector::const_iterator& begin, SampleVector::const_iterator& end);

	SampleSink* m_sampleSink;
	SampleFifo m_sampleFifo;

public slots:
	void handleFifoData();
};

/**
 * This class is a wrapper for SampleSink that runs the SampleSink object in its own thread
 */
class SDRANGELOVE_API ThreadedSampleSink : public QObject {
	Q_OBJECT

public:
	ThreadedSampleSink(SampleSink* sampleSink, QObject *parent = 0);
	~ThreadedSampleSink();

	const SampleSink *getSink() const { return m_sampleSink; }
	MessageQueue* getInputMessageQueue() { return m_sampleSink->getInputMessageQueue(); } //!< Return pointer to sample sink's input message queue
	MessageQueue* getOutputMessageQueue() { return m_sampleSink->getOutputMessageQueue(); } //!< Return pointer to sample sink's output message queue

	void start(); //!< this thread start()
	void stop();  //!< this thread exit() and wait()

	bool handleSinkMessage(Message& cmd); //!< Send message to sink synchronously
	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly); //!< Feed sink with samples

	QString getSampleSinkObjectName() const;

protected:

	QThread *m_thread; //!< The thead object
	ThreadedSampleFifo *m_threadedSampleFifo;
	SampleSink* m_sampleSink;
};

#endif // INCLUDE_THREADEDSAMPLESINK_H
