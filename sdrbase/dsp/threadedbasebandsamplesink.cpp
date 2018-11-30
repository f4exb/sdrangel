#include "threadedbasebandsamplesink.h"

#include <QThread>
#include <QDebug>
#include "dsp/dspcommands.h"
#include "util/message.h"

ThreadedBasebandSampleSinkFifo::ThreadedBasebandSampleSinkFifo(BasebandSampleSink *sampleSink, std::size_t size) :
	m_sampleSink(sampleSink)
{
	connect(&m_sampleFifo, SIGNAL(dataReady()), this, SLOT(handleFifoData()));
	m_sampleFifo.setSize(size);
}

ThreadedBasebandSampleSinkFifo::~ThreadedBasebandSampleSinkFifo()
{
	m_sampleFifo.readCommit(m_sampleFifo.fill());
}

void ThreadedBasebandSampleSinkFifo::writeToFifo(SampleVector::const_iterator& begin, SampleVector::const_iterator& end)
{
	m_sampleFifo.write(begin, end);
}

void ThreadedBasebandSampleSinkFifo::handleFifoData() // FIXME: Fixed? Move it to the new threadable sink class
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

ThreadedBasebandSampleSink::ThreadedBasebandSampleSink(BasebandSampleSink* sampleSink, QObject *parent) :
	m_basebandSampleSink(sampleSink)
{
	QString name = "ThreadedBasebandSampleSink(" + m_basebandSampleSink->objectName() + ")";
	setObjectName(name);

	qDebug() << "ThreadedBasebandSampleSink::ThreadedBasebandSampleSink: " << name;

	m_thread = new QThread(parent);
	m_threadedBasebandSampleSinkFifo = new ThreadedBasebandSampleSinkFifo(m_basebandSampleSink);
	//moveToThread(m_thread); // FIXME: Fixed? the intermediate FIFO should be handled within the sink. Define a new type of sink that is compatible with threading
	m_basebandSampleSink->moveToThread(m_thread);
	m_threadedBasebandSampleSinkFifo->moveToThread(m_thread);
	BasebandSampleSink::MsgThreadedSink *msg = BasebandSampleSink::MsgThreadedSink::create(m_thread); // inform of the new thread
	m_basebandSampleSink->handleMessage(*msg);
	delete msg;
	//m_sampleFifo.moveToThread(m_thread);
	//connect(&m_sampleFifo, SIGNAL(dataReady()), this, SLOT(handleData()));
	//m_sampleFifo.setSize(262144);

	qDebug() << "ThreadedBasebandSampleSink::ThreadedBasebandSampleSink: thread: " << thread() << " m_thread: " << m_thread;
}

ThreadedBasebandSampleSink::~ThreadedBasebandSampleSink()
{
    if (m_thread->isRunning()) {
        stop();
    }

    delete m_threadedBasebandSampleSinkFifo; // Valgrind memcheck
	delete m_thread;
}

void ThreadedBasebandSampleSink::start()
{
	qDebug() << "ThreadedBasebandSampleSink::start";
	m_thread->start();
	m_basebandSampleSink->start();
}

void ThreadedBasebandSampleSink::stop()
{
	qDebug() << "ThreadedBasebandSampleSink::stop";
	m_basebandSampleSink->stop();
	m_thread->exit();
	m_thread->wait();
}

void ThreadedBasebandSampleSink::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly __attribute__((unused)))
{
	//m_sampleSink->feed(begin, end, positiveOnly);
	//m_sampleFifo.write(begin, end);
	m_threadedBasebandSampleSinkFifo->writeToFifo(begin, end);
}

bool ThreadedBasebandSampleSink::handleSinkMessage(const Message& cmd)
{
	return m_basebandSampleSink->handleMessage(cmd);
}

QString ThreadedBasebandSampleSink::getSampleSinkObjectName() const
{
	return m_basebandSampleSink->objectName();
}
