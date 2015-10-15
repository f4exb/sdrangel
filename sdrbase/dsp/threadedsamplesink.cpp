#include <QThread>
#include <QDebug>
#include "dsp/threadedsamplesink.h"
#include "dsp/dspcommands.h"
#include "util/message.h"

ThreadedSampleFifo::ThreadedSampleFifo(SampleSink *sampleSink, std::size_t size) :
	m_sampleSink(sampleSink)
{
	connect(&m_sampleFifo, SIGNAL(dataReady()), this, SLOT(handleFifoData()));
	m_sampleFifo.setSize(size);
}

ThreadedSampleFifo::~ThreadedSampleFifo()
{
	m_sampleFifo.readCommit(m_sampleFifo.fill());
}

void ThreadedSampleFifo::writeToFifo(SampleVector::const_iterator& begin, SampleVector::const_iterator& end)
{
	m_sampleFifo.write(begin, end);
}

void ThreadedSampleFifo::handleFifoData() // FIXME: Fixed? Move it to the new threadable sink class
{
	bool positiveOnly = false;

	while ((m_sampleFifo.fill() > 0) && (m_sampleSink->getInputMessageQueue()->size() == 0))
	{
		SampleVector::iterator part1begin;
		SampleVector::iterator part1end;
		SampleVector::iterator part2begin;
		SampleVector::iterator part2end;

		std::size_t count = m_sampleFifo.readBegin(m_sampleFifo.fill(), &part1begin, &part1end, &part2begin, &part2end);

		// first part of FIFO data

		if (count > 0)
		{
			// handle data
			if(m_sampleSink != NULL)
			{
				m_sampleSink->feed(part1begin, part1end, positiveOnly);
			}

			m_sampleFifo.readCommit(part1end - part1begin);
		}

		// second part of FIFO data (used when block wraps around)

		if(part2begin != part2end)
		{
			// handle data
			if(m_sampleSink != NULL)
			{
				m_sampleSink->feed(part2begin, part2end, positiveOnly);
			}

			m_sampleFifo.readCommit(part2end - part2begin);
		}
	}
}

ThreadedSampleSink::ThreadedSampleSink(SampleSink* sampleSink, QObject *parent) :
	m_sampleSink(sampleSink)
{
	QString name = "ThreadedSampleSink(" + m_sampleSink->objectName() + ")";
	setObjectName(name);

	qDebug() << "ThreadedSampleSink::ThreadedSampleSink: " << name;

	m_thread = new QThread(parent);
	m_threadedSampleFifo = new ThreadedSampleFifo(m_sampleSink);
	//moveToThread(m_thread); // FIXME: Fixed? the intermediate FIFO should be handled within the sink. Define a new type of sink that is compatible with threading
	m_sampleSink->moveToThread(m_thread);
	m_threadedSampleFifo->moveToThread(m_thread);
	//m_sampleFifo.moveToThread(m_thread);
	//connect(&m_sampleFifo, SIGNAL(dataReady()), this, SLOT(handleData()));
	//m_sampleFifo.setSize(262144);

	qDebug() << "ThreadedSampleSink::ThreadedSampleSink: thread: " << thread() << " m_thread: " << m_thread;
}

ThreadedSampleSink::~ThreadedSampleSink()
{
	delete m_threadedSampleFifo; // Valgrind memcheck
	delete m_thread;
}

void ThreadedSampleSink::start()
{
	qDebug() << "ThreadedSampleSink::start";
	m_thread->start();
	m_sampleSink->start();
}

void ThreadedSampleSink::stop()
{
	qDebug() << "ThreadedSampleSink::stop";
	m_sampleSink->stop();
	m_thread->exit();
	m_thread->wait();
}

void ThreadedSampleSink::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
	//m_sampleSink->feed(begin, end, positiveOnly);
	//m_sampleFifo.write(begin, end);
	m_threadedSampleFifo->writeToFifo(begin, end);
}

bool ThreadedSampleSink::handleSinkMessage(Message& cmd)
{
	return m_sampleSink->handleMessage(cmd);
}

QString ThreadedSampleSink::getSampleSinkObjectName() const
{
	return m_sampleSink->objectName();
}
