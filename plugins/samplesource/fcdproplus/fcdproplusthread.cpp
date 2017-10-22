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

FCDProPlusThread::FCDProPlusThread(SampleSinkFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	fcd_handle(NULL),
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
	if ( !OpenSource(fcd_traits<ProPlus>::alsaDeviceName) )
	{
		qCritical() << "FCDThread::run: cannot open FCD sound card";
		return;
	}
	// TODO: fallback to original fcd

	m_running = true;

	while(m_running)
	{
		if (work(fcd_traits<ProPlus>::convBufSize) < 0)
		{
			break;
		}
	}

	CloseSource();
}

bool FCDProPlusThread::OpenSource(const char* cardname)
{
	bool fail = false;
	snd_pcm_hw_params_t* params;
	//fcd_rate = FCDPP_RATE;
	//fcd_channels =2;
	//fcd_format = SND_PCM_SFMT_U16_LE;
	snd_pcm_stream_t fcd_stream = SND_PCM_STREAM_CAPTURE;

	if (fcd_handle)
	{
		return false;
	}

	if (snd_pcm_open(&fcd_handle, cardname, fcd_stream, 0) < 0)
	{
		qCritical("FCDThread::OpenSource: cannot open %s", cardname);
		return false;
	}

	snd_pcm_hw_params_alloca(&params);

	if (snd_pcm_hw_params_any(fcd_handle, params) < 0)
	{
		qCritical("FCDProPlusThread::OpenSource: snd_pcm_hw_params_any failed");
		fail = true;
	}
	else if (snd_pcm_hw_params(fcd_handle, params) < 0)
	{
		qCritical("FCDProPlusThread::OpenSource: snd_pcm_hw_params failed");
		fail = true;
		// TODO: check actual samplerate, may be crippled firmware
	}
	else
	{
		if (snd_pcm_start(fcd_handle) < 0)
		{
			qCritical("FCDProPlusThread::OpenSource: snd_pcm_start failed");
			fail = true;
		}
	}

	if (fail)
	{
		snd_pcm_close( fcd_handle );
		return false;
	}
	else
	{
		qDebug("FCDProPlusThread::OpenSource: Funcube stream started");
	}

	return true;
}

void FCDProPlusThread::CloseSource()
{
	if (fcd_handle)
	{
		snd_pcm_close( fcd_handle );
	}

	fcd_handle = NULL;
}

int FCDProPlusThread::work(int n_items)
{
	int l;
	SampleVector::iterator it;
	void *out;

	it = m_convertBuffer.begin();
	out = (void *)&it[0];
	l = snd_pcm_mmap_readi(fcd_handle, out, (snd_pcm_uframes_t)n_items);
	if (l > 0)
		m_sampleFifo->write(it, it + l);
	if (l == -EPIPE) {
		qDebug("FCDProPlusThread::work: Overrun detected");
		return 0;
	}
	return l;
}


