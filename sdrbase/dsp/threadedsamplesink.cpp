#include <QThread>
#include "dsp/threadedsamplesink.h"
#include "util/message.h"

ThreadedSampleSink::ThreadedSampleSink(SampleSink* sampleSink) :
	m_thread(new QThread),
	m_sampleSink(sampleSink)
{
	moveToThread(m_thread);
	connect(m_thread, SIGNAL(started()), this, SLOT(threadStarted()));
	connect(m_thread, SIGNAL(finished()), this, SLOT(threadFinished()));

	m_messageQueue.moveToThread(m_thread);
	connect(&m_messageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()));

	m_sampleFifo.moveToThread(m_thread);
	connect(&m_sampleFifo, SIGNAL(dataReady()), this, SLOT(handleData()));
	m_sampleFifo.setSize(262144);

	sampleSink->moveToThread(m_thread);
}

ThreadedSampleSink::~ThreadedSampleSink()
{
	m_thread->exit();
	m_thread->wait();
	delete m_thread;
}

void ThreadedSampleSink::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst)
{
	Q_UNUSED(firstOfBurst);
	m_sampleFifo.write(begin, end);
}

void ThreadedSampleSink::start()
{
	m_thread->start();
}

void ThreadedSampleSink::stop()
{
	m_thread->exit();
	m_thread->wait();
	m_sampleFifo.readCommit(m_sampleFifo.fill());
}

bool ThreadedSampleSink::handleMessage(Message* cmd)
{
	// called from other thread
	m_messageQueue.submit(cmd);
	return true;
}

void ThreadedSampleSink::handleData()
{
	bool firstOfBurst = true;

	while((m_sampleFifo.fill() > 0) && (m_messageQueue.countPending() == 0)) {
		SampleVector::iterator part1begin;
		SampleVector::iterator part1end;
		SampleVector::iterator part2begin;
		SampleVector::iterator part2end;

		size_t count = m_sampleFifo.readBegin(m_sampleFifo.fill(), &part1begin, &part1end, &part2begin, &part2end);

		// first part of FIFO data
		if(part1begin != part1end) {
			// handle data
			if(m_sampleSink != NULL)
				m_sampleSink->feed(part1begin, part1end, firstOfBurst);
			firstOfBurst = false;
		}
		// second part of FIFO data (used when block wraps around)
		if(part2begin != part2end) {
			// handle data
			if(m_sampleSink != NULL)
				m_sampleSink->feed(part1begin, part1end, firstOfBurst);
			firstOfBurst = false;
		}

		// adjust FIFO pointers
		m_sampleFifo.readCommit(count);
	}
}

void ThreadedSampleSink::handleMessages()
{
	Message* message;
	while((message = m_messageQueue.accept()) != NULL) {
		qDebug("CMD: %s", message->getIdentifier());
		if(m_sampleSink != NULL) {
			if(!m_sampleSink->handleMessage(message))
				message->completed();
		} else {
			message->completed();
		}
	}
}

void ThreadedSampleSink::threadStarted()
{
	if(m_sampleSink != NULL)
		m_sampleSink->start();
}

void ThreadedSampleSink::threadFinished()
{
	if(m_sampleSink != NULL)
		m_sampleSink->stop();
}
