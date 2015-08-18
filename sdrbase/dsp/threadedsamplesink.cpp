#include <QThread>
#include <QDebug>
#include "dsp/threadedsamplesink.h"
#include "dsp/dspcommands.h"
#include "util/message.h"

ThreadedSampleSink::ThreadedSampleSink(SampleSink* sampleSink) :
	m_sampleSink(sampleSink)
{
	moveToThread(this);
}

ThreadedSampleSink::~ThreadedSampleSink()
{
	wait();
}

void ThreadedSampleSink::start()
{
	qDebug() << "ThreadedSampleSink::start";
	DSPPing cmd;
	QThread::start();
	m_syncMessenger.sendWait(cmd);
}

void ThreadedSampleSink::stop()
{
	qDebug() << "ThreadedSampleSink::stop";
	exit();
	wait();
}

void ThreadedSampleSink::run()
{
	qDebug() << "ThreadedSampleSink::run";
	connect(&m_syncMessenger, SIGNAL(messageSent()), this, SLOT(handleSynchronousMessages()), Qt::QueuedConnection);
	m_syncMessenger.done(); // Release start() that is waiting in calling trhead
	exec();
}

void ThreadedSampleSink::feed(SampleVector::const_iterator& begin, SampleVector::const_iterator& end, bool positiveOnly)
{
	m_sampleSink->feed(begin, end, positiveOnly);
}

bool ThreadedSampleSink::sendWaitSink(Message& cmd)
{
	m_syncMessenger.sendWait(cmd);
}

void ThreadedSampleSink::handleSynchronousMessages()
{
	qDebug() << "ThreadedSampleSink::handleSynchronousMessages";
	Message *message = m_syncMessenger.getMessage();
	qDebug() << "  - message: " << message->getIdentifier();
	m_sampleSink->handleMessage(*message); // just delegate to the sink
	m_syncMessenger.done();
}

QString ThreadedSampleSink::getSampleSinkObjectName() const
{
	return m_sampleSink->objectName();
}


/*
void ThreadedSampleSink::handleData()
{
	bool positiveOnly = false;

	while((m_sampleFifo.fill() > 0) && (m_messageQueue.countPending() == 0)) {
		SampleVector::iterator part1begin;
		SampleVector::iterator part1end;
		SampleVector::iterator part2begin;
		SampleVector::iterator part2end;

		size_t count = m_sampleFifo.readBegin(m_sampleFifo.fill(), &part1begin, &part1end, &part2begin, &part2end);

		// first part of FIFO data
		if(count > 0) {
			// handle data
			if(m_sampleSink != NULL)
				m_sampleSink->feed(part1begin, part1end, positiveOnly);
			m_sampleFifo.readCommit(part1end - part1begin);
		}
		// second part of FIFO data (used when block wraps around)
		if(part2begin != part2end) {
			// handle data
			if(m_sampleSink != NULL)
				m_sampleSink->feed(part2begin, part2end, positiveOnly);
			m_sampleFifo.readCommit(part2end - part2begin);
		}
	}
}*/

