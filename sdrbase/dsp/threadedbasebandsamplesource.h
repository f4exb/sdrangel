///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#ifndef SDRBASE_DSP_THREADEDBASEBANDSAMPLESOURCE_H_
#define SDRBASE_DSP_THREADEDBASEBANDSAMPLESOURCE_H_

#include <dsp/basebandsamplesource.h>
#include <QMutex>

#include "samplesourcefifo.h"
#include "util/messagequeue.h"
#include "util/export.h"

class BasebandSampleSource;
class QThread;

/**
 * Because Qt is a piece of shit this class cannot be a nested protected class of ThreadedSampleSource
 * So let's make everything public
 */
class ThreadedBasebandSampleSourceFifo : public QObject {
	Q_OBJECT

public:
	ThreadedBasebandSampleSourceFifo(BasebandSampleSource* sampleSource);
	~ThreadedBasebandSampleSourceFifo();
	void readFromFifo(SampleVector::iterator& beginRead, unsigned int nbSamples);

	BasebandSampleSource* m_sampleSource;
	SampleSourceFifo m_sampleSourceFifo;

public slots:
	void handleFifoData();

private:
    unsigned int m_samplesChunkSize;
};

/**
 * This class is a wrapper for BasebandSampleSource that runs the BasebandSampleSource object in its own thread
 */
class SDRANGEL_API ThreadedBasebandSampleSource : public QObject {
	Q_OBJECT

public:
	ThreadedBasebandSampleSource(BasebandSampleSource* sampleSource, QObject *parent = 0);
	~ThreadedBasebandSampleSource();

	const BasebandSampleSource *getSource() const { return m_basebandSampleSource; }
	MessageQueue* getInputMessageQueue() { return m_basebandSampleSource->getInputMessageQueue(); }   //!< Return pointer to sample source's input message queue
	MessageQueue* getOutputMessageQueue() { return m_basebandSampleSource->getOutputMessageQueue(); } //!< Return pointer to sample source's output message queue

	void start(); //!< this thread start()
	void stop();  //!< this thread exit() and wait()

	bool handleSourceMessage(const Message& cmd);                         //!< Send message to source synchronously
	void pull(SampleVector::iterator& beginRead, unsigned int nbSamples); //!< Pull samples from source

	QString getSampleSourceObjectName() const;

protected:
	QThread *m_thread; //!< The thead object
	ThreadedBasebandSampleSourceFifo *m_threadedBasebandSampleSourceFifo;
	BasebandSampleSource* m_basebandSampleSource;
};

#endif /* SDRBASE_DSP_THREADEDBASEBANDSAMPLESOURCE_H_ */
