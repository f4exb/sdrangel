///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <QDebug>

#include "dsp/samplesourcefifo.h"
#include "filesinkthread.h"

FileSinkThread::FileSinkThread(std::ofstream *samplesStream, SampleSinkFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_ofstream(samplesStream),
	m_buf(0),
	m_bufsize(0),
	m_samplesChunkSize(0),
	m_sampleFifo(sampleFifo),
    m_samplerate(0),
    m_throttlems(FILESINK_THROTTLE_MS),
    m_throttleToggle(false)
{
    assert(m_ofstream != 0);
}

FileSinkThread::~FileSinkThread()
{
	if (m_running) {
		stopWork();
	}

	if (m_buf != 0) {
		free(m_buf);
	}
}

void FileSinkThread::startWork()
{
	qDebug() << "FileSinkThread::startWork: ";

    if (m_ofstream->is_open())
    {
        qDebug() << "FileSinkThread::startWork: file stream open, starting...";
        m_startWaitMutex.lock();
        m_elapsedTimer.start();
        start();
        while(!m_running)
            m_startWaiter.wait(&m_startWaitMutex, 100);
        m_startWaitMutex.unlock();
    }
    else
    {
        qDebug() << "FileSinkThread::startWork: file stream closed, not starting.";
    }
}

void FileSinkThread::stopWork()
{
	qDebug() << "FileSinkThread::stopWork";
	m_running = false;
	wait();
}

void FileSinkThread::setSamplerate(int samplerate)
{
	qDebug() << "FileSinkThread::setSamplerate:"
			<< " new:" << samplerate
			<< " old:" << m_samplerate;

	if (samplerate != m_samplerate)
	{
		if (m_running) {
			stopWork();
		}

		m_samplerate = samplerate;
        m_samplesChunkSize = (m_samplerate * m_throttlems) / 1000;
	}
}

void FileSinkThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

	while(m_running) // actual work is in the tick() function
	{
		sleep(1);
	}

	m_running = false;
}

void FileSinkThread::connectTimer(const QTimer& timer)
{
	qDebug() << "FileSinkThread::connectTimer";
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void FileSinkThread::tick()
{
	if (m_running)
	{
        qint64 throttlems = m_elapsedTimer.restart();

        if (throttlems != m_throttlems)
        {
            m_throttlems = throttlems;
            m_samplesChunkSize = (m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000;
            m_throttleToggle = !m_throttleToggle;
        }

        SampleVector::iterator beginRead;

        m_sampleFifo->read(beginRead, m_samplesChunkSize);
        m_ofstream->write(reinterpret_cast<char*>(*beginRead), m_samplesChunkSize*4);
	}
}
