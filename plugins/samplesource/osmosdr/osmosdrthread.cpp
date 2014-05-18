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

#include <stdio.h>
#include <errno.h>
#include "osmosdrthread.h"
#include "dsp/samplefifo.h"

OsmoSDRThread::OsmoSDRThread(osmosdr_dev_t* dev, SampleFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_sampleFifo(sampleFifo)
{
}

OsmoSDRThread::~OsmoSDRThread()
{
	stopWork();
}

void OsmoSDRThread::startWork()
{
	m_startWaitMutex.lock();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void OsmoSDRThread::stopWork()
{
	m_running = false;
	wait();
}

void OsmoSDRThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

	//m_f = fopen("/tmp/samples.bin", "wb");

	while(m_running) {
		if((res = osmosdr_read_async(m_dev, &OsmoSDRThread::callbackHelper, this, 16, sizeof(Sample) * 8192 * 2)) < 0) {
			qCritical("OsmoSDRThread: async error: %s", strerror(errno));
			break;
		}
	}

	m_running = false;
}

void OsmoSDRThread::checkData(const quint8* buf, qint32 len)
{
	const Sample* s = (const Sample*)buf;
	len /= sizeof(Sample);

	while(len) {
		if((s->i != m_nextI) || (s->q != m_nextQ)) {
			qDebug("continuity error after %llu samples", m_samplePos);
			m_samplePos = 0;
			m_nextI = s->i - 1;
			m_nextQ = s->q + 1;
		} else {
			m_nextI--;
			m_nextQ++;
			m_samplePos++;
		}
		len--;
		s++;
	}
}

void OsmoSDRThread::callback(const quint8* buf, qint32 len)
{
	/*
	qDebug("%d", len);
*/
	/*
	for(int i = 0; i < len / 2; i += 2) {
		((qint16*)buf)[i] = sin((float)(cntr) * 1024* 2.0 * M_PI / 65536.0) * 32000.0;
		((qint16*)buf)[i + 1] = -cos((float)(cntr++) * 1024*2.0 * M_PI / 65536.0) * 32000.0;
	}
	*/

	//m_sampleFifo->write((SampleVector::const_iterator)((Sample*)buf), (SampleVector::const_iterator)((Sample*)(buf + len)));
	//fwrite(buf, 1, len, m_f);
	//checkData(buf, len);

	m_sampleFifo->write(buf, len);
	if(!m_running)
		osmosdr_cancel_async(m_dev);
}

void OsmoSDRThread::callbackHelper(unsigned char* buf, uint32_t len, void* ctx)
{
	OsmoSDRThread* thread = (OsmoSDRThread*)ctx;
	thread->callback(buf, len);
}
