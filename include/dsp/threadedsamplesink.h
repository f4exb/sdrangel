#ifndef INCLUDE_THREADEDSAMPLESINK_H
#define INCLUDE_THREADEDSAMPLESINK_H

#include <QMutex>
#include "samplesink.h"
#include "dsp/samplefifo.h"
#include "util/messagequeue.h"
#include "util/export.h"

class QThread;
class SampleSink;

class SDRANGELOVE_API ThreadedSampleSink : public SampleSink {
	Q_OBJECT

public:
	ThreadedSampleSink(SampleSink* sampleSink);
	virtual ~ThreadedSampleSink();

	MessageQueue* getMessageQueue() { return &m_messageQueue; }

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst);
	void start();
	void stop();
	bool handleMessage(Message* cmd);

protected:
	QMutex m_mutex;
	QThread* m_thread;
	MessageQueue m_messageQueue;
	SampleFifo m_sampleFifo;
	SampleSink* m_sampleSink;

protected slots:
	void handleData();
	void handleMessages();
	void threadStarted();
	void threadFinished();
};

#endif // INCLUDE_THREADEDSAMPLESINK_H
