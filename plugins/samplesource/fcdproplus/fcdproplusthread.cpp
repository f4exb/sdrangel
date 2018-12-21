///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2018 Edouard Griffiths, F4EXB                              //
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

#include "fcdproplusthread.h"

FCDProPlusThread::FCDProPlusThread(SampleSinkFifo* sampleFifo, AudioFifo *fcdFIFO, QObject* parent) :
	QThread(parent),
	m_fcdFIFO(fcdFIFO),
	m_running(false),
	m_convertBuffer(fcd_traits<ProPlus>::convBufSize), // nb samples
	m_sampleFifo(sampleFifo)
{
	start();
}

FCDProPlusThread::~FCDProPlusThread()
{
}

void FCDProPlusThread::startWork()
{
	m_startWaitMutex.lock();

	start();

	while(!m_running)
	{
		m_startWaiter.wait(&m_startWaitMutex, 100);
	}

	m_startWaitMutex.unlock();
}

void FCDProPlusThread::stopWork()
{
	m_running = false;
	wait();
}

void FCDProPlusThread::run()
{
	m_running = true;
	qDebug("FCDThread::run: start running loop");

	while (m_running)
	{
	    work(fcd_traits<ProPlus>::convBufSize);
	    std::this_thread::sleep_for(std::chrono::microseconds(100));
	}

	qDebug("FCDThread::run: running loop stopped");
	m_running = false;
}

void FCDProPlusThread::work(unsigned int n_items)
{
    uint32_t nbRead = m_fcdFIFO->read((unsigned char *) m_buf, n_items); // number of samples
    SampleVector::iterator it = m_convertBuffer.begin();
    m_decimators.decimate1(&it, m_buf, 2*nbRead);
    m_sampleFifo->write(m_convertBuffer.begin(), it);
}
