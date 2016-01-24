///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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
#include "dsp/samplefifo.h"
#include <QDebug>

#include "sdrdaemonthread.h"

const int SDRdaemonThread::m_rateDivider = 1000/SDRDAEMON_THROTTLE_MS;

SDRdaemonThread::SDRdaemonThread(std::ifstream *samplesStream, SampleFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_ifstream(samplesStream),
	m_buf(0),
	m_bufsize(0),
	m_chunksize(0),
	m_sampleFifo(sampleFifo),
	m_samplesCount(0),
	m_samplerate(0)
{
    assert(m_ifstream != 0);
}

SDRdaemonThread::~SDRdaemonThread()
{
	if (m_running) {
		stopWork();
	}

	if (m_buf != 0) {
		free(m_buf);
	}
}

void SDRdaemonThread::startWork()
{
	qDebug() << "SDRdaemonThread::startWork: ";
    
    if (m_ifstream->is_open())
    {
        qDebug() << "  - file stream open, starting...";
        m_startWaitMutex.lock();
        start();
        while(!m_running)
            m_startWaiter.wait(&m_startWaitMutex, 100);
        m_startWaitMutex.unlock();
    }
    else
    {
        qDebug() << "  - file stream closed, not starting.";
    }
}

void SDRdaemonThread::stopWork()
{
	qDebug() << "SDRdaemonThread::stopWork";
	m_running = false;
	wait();
}

void SDRdaemonThread::setSamplerate(int samplerate)
{
	qDebug() << "SDRdaemonThread::setSamplerate:"
			<< " new:" << samplerate
			<< " old:" << m_samplerate;

	if (samplerate != m_samplerate)
	{
		if (m_running) {
			stopWork();
		}

		m_samplerate = samplerate;
		m_chunksize = (m_samplerate / m_rateDivider)*4; // TODO: implement FF and slow motion here. 4 corresponds to live. 2 is half speed, 8 is doulbe speed
		m_bufsize = m_chunksize;

		if (m_buf == 0)	{
			qDebug() << "  - Allocate buffer";
			m_buf = (quint8*) malloc(m_bufsize);
		} else {
			qDebug() << "  - Re-allocate buffer";
			m_buf = (quint8*) realloc((void*) m_buf, m_bufsize);
		}

		qDebug() << "  - size: " << m_bufsize
				<< " #samples: " << (m_bufsize/4);
	}

	//m_samplerate = samplerate;
}

void SDRdaemonThread::run()
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

void SDRdaemonThread::connectTimer(const QTimer& timer)
{
	qDebug() << "SDRdaemonThread::connectTimer";
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void SDRdaemonThread::tick()
{
	if (m_running)
	{
		// read samples directly feeding the SampleFifo (no callback)
		m_ifstream->read(reinterpret_cast<char*>(m_buf), m_chunksize);
        
        if (m_ifstream->eof())
        {
            m_sampleFifo->write(m_buf, m_ifstream->gcount());
            // TODO: handle loop playback situation
    		m_ifstream->clear();
    		m_ifstream->seekg(0, std::ios::beg);
    		m_samplesCount = 0;
            //stopWork();
            //m_ifstream->close();
        }
        else
        {
            m_sampleFifo->write(m_buf, m_chunksize);
    		m_samplesCount += m_chunksize / 4;
        }
	}
}
