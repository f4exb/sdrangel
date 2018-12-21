///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include <QDebug>
#include <stdio.h>
#include <errno.h>
#include <chrono>
#include <thread>

#include "dsp/samplesinkfifo.h"
#include "audio/audiofifo.h"

#include "fcdprothread.h"

FCDProThread::FCDProThread(SampleSinkFifo* sampleFifo, AudioFifo *fcdFIFO, QObject* parent) :
	QThread(parent),
	m_fcdFIFO(fcdFIFO),
	m_running(false),
	m_convertBuffer(fcd_traits<Pro>::convBufSize), // nb samples
	m_sampleFifo(sampleFifo)
{
	start();
}

FCDProThread::~FCDProThread()
{
}

void FCDProThread::startWork()
{
	m_startWaitMutex.lock();

	start();

	while(!m_running)
	{
		m_startWaiter.wait(&m_startWaitMutex, 100);
	}

	m_startWaitMutex.unlock();
}

void FCDProThread::stopWork()
{
	m_running = false;
	wait();
}

void FCDProThread::run()
{
    m_running = true;
    qDebug("FCDProThread::run: start running loop");

    while (m_running)
    {
        work(fcd_traits<Pro>::convBufSize);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }

    qDebug("FCDProThread::run: running loop stopped");
    m_running = false;
}

void FCDProThread::work(unsigned int n_items)
{
    uint32_t nbRead = m_fcdFIFO->read((unsigned char *) m_buf, n_items); // number of samples
    SampleVector::iterator it = m_convertBuffer.begin();
    m_decimators.decimate1(&it, m_buf, 2*nbRead);
    m_sampleFifo->write(m_convertBuffer.begin(), it);
}
