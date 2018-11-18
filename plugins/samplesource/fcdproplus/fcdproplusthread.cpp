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
#include "fcdproplusthread.h"

#include "dsp/samplesinkfifo.h"
#include "fcdtraits.h"

FCDProPlusThread::FCDProPlusThread(SampleSinkFifo* sampleFifo, snd_pcm_t *fcd_handle, QObject* parent) :
	QThread(parent),
	m_fcd_handle(fcd_handle),
	m_running(false),
	m_convertBuffer(fcd_traits<ProPlus>::convBufSize),
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
		if (work(fcd_traits<ProPlus>::convBufSize) < 0) {
			break;
		}
	}

	qDebug("FCDThread::run: running loop stopped");
}

int FCDProPlusThread::work(int n_items)
{
	int l;
	SampleVector::iterator it;
	void *out;

	it = m_convertBuffer.begin();
	out = (void *)&it[0];

	l = snd_pcm_mmap_readi(m_fcd_handle, out, (snd_pcm_uframes_t)n_items);

	if (l > 0)
	{
	    m_sampleFifo->write(it, it + l);
	}
	else
	{
	    if (l == -EPIPE) {
	        qDebug("FCDProPlusThread::work: Overrun detected");
	    } else if (l < 0) {
	        qDebug("FCDProPlusThread::work: snd_pcm_mmap_readi failed with code %d", l);
	    } else {
	        qDebug("FCDProPlusThread::work: snd_pcm_mmap_readi empty");
	    }

	    return 0;
	}

	return l;
}
