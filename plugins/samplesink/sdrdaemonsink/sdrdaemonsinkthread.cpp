///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <algorithm>
#include <QDebug>

#include "dsp/samplesourcefifo.h"
#include "sdrdaemonsinkthread.h"

SDRdaemonSinkThread::SDRdaemonSinkThread(SampleSourceFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_samplesChunkSize(0),
	m_sampleFifo(sampleFifo),
	m_samplesCount(0),
	m_chunkCorrection(0),
    m_samplerate(0),
    m_throttlems(SDRDAEMONSINK_THROTTLE_MS),
    m_maxThrottlems(50),
    m_throttleToggle(false)
{
}

SDRdaemonSinkThread::~SDRdaemonSinkThread()
{
	if (m_running) {
		stopWork();
	}
}

void SDRdaemonSinkThread::startWork()
{
	qDebug() << "SDRdaemonSinkThread::startWork: ";
	m_udpSinkFEC.start();
    m_maxThrottlems = 0;
    m_startWaitMutex.lock();
    m_elapsedTimer.start();
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void SDRdaemonSinkThread::stopWork()
{
	qDebug() << "SDRdaemonSinkThread::stopWork";
	m_running = false;
	wait();
	m_udpSinkFEC.stop();
}

void SDRdaemonSinkThread::setSamplerate(int samplerate)
{
	if (samplerate != m_samplerate)
	{
	    qDebug() << "SDRdaemonSinkThread::setSamplerate:"
	            << " new:" << samplerate
	            << " old:" << m_samplerate;

	    bool wasRunning = false;

		if (m_running)
		{
			stopWork();
			wasRunning = true;
		}

		// resize sample FIFO
		if (m_sampleFifo) {
		    m_sampleFifo->resize(samplerate); // 1s buffer
		}

        m_samplerate = samplerate;
        m_samplesChunkSize = (m_samplerate * m_throttlems) / 1000;
        m_udpSinkFEC.setSampleRate(m_samplerate);

        if (wasRunning) {
            startWork();
        }
	}
}

void SDRdaemonSinkThread::run()
{
	m_running = true;
	m_startWaiter.wakeAll();

	while(m_running) // actual work is in the tick() function
	{
		sleep(1);
	}

	m_running = false;
}

void SDRdaemonSinkThread::connectTimer(const QTimer& timer)
{
	qDebug() << "SDRdaemonSinkThread::connectTimer";
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void SDRdaemonSinkThread::tick()
{
	if (m_running)
	{
        qint64 throttlems = m_elapsedTimer.restart();

        if (throttlems != m_throttlems)
        {
            m_throttlems = throttlems;
            m_samplesChunkSize = (m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000;
            m_samplesChunkSize = m_samplesChunkSize + m_chunkCorrection > 0 ? m_samplesChunkSize + m_chunkCorrection : m_samplesChunkSize;
            m_throttleToggle = !m_throttleToggle;
        }

        SampleVector::iterator readUntil;

        m_sampleFifo->readAdvance(readUntil, m_samplesChunkSize); // pull samples
        SampleVector::iterator beginRead = readUntil - m_samplesChunkSize;
        m_samplesCount += m_samplesChunkSize;

        m_udpSinkFEC.write(beginRead, m_samplesChunkSize);
	}
}

uint32_t SDRdaemonSinkThread::getSamplesCount(struct timeval& tv) const
{
    gettimeofday(&tv, 0);
    return m_samplesCount;
}
