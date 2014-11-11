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
#include "rtlsdrthread.h"
#include "dsp/samplefifo.h"

#define BLOCKSIZE 16384

RTLSDRThread::RTLSDRThread(rtlsdr_dev_t* dev, SampleFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_convertBuffer(BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_samplerate(288000)
{
}

RTLSDRThread::~RTLSDRThread()
{
	stopWork();
}

void RTLSDRThread::startWork()
{
	m_startWaitMutex.lock();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void RTLSDRThread::stopWork()
{
	m_running = false;
	wait();
}

void RTLSDRThread::setSamplerate(int samplerate)
{
	m_samplerate = samplerate;
}

void RTLSDRThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

	while(m_running) {
		if((res = rtlsdr_read_async(m_dev, &RTLSDRThread::callbackHelper, this, 32, BLOCKSIZE)) < 0) {
			qCritical("RTLSDRThread: async error: %s", strerror(errno));
			break;
		}
	}

	m_running = false;
}

void RTLSDRThread::decimate2(SampleVector::iterator* it, const quint8* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3];
		yimag = buf[pos+1] + buf[pos+2] - 255;
		Sample s( xreal << 3, yimag << 3 );
		**it = s;
		(*it)++;
		xreal = buf[pos+7] - buf[pos+4];
		yimag = 255 - buf[pos+5] - buf[pos+6];
		Sample t( xreal << 3, yimag << 3 );
		**it = t;
		(*it)++;
	}
}
void RTLSDRThread::decimate4(SampleVector::iterator* it, const quint8* buf, qint32 len)
{
	qint16 xreal, yimag;
		for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
		yimag = buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];
		Sample s( xreal << 4, yimag << 4 );
		**it = s;
		(*it)++;
	}
}

void RTLSDRThread::decimate8(SampleVector::iterator* it, const quint8* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 15; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
		yimag = buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];
		pos += 8;
		xreal += buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
		yimag += buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];

		Sample s( xreal << 3, yimag << 3 );
		**it = s;
		(*it)++;
	}
}

void RTLSDRThread::decimate16(SampleVector::iterator* it, const quint8* buf, qint32 len)
{
	// Offset tuning: 4x downsample and rotate, then
	// downsample 4x more. [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
	qint16 xreal, yimag;
	for (int step = 0; step < len - 31; step +=32) {
		xreal = yimag = 0;
		for (int pos = step; pos < step + 32; pos += 8) {
			xreal += buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
			yimag += buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];
		}
		Sample s( xreal << 3, yimag << 3 );
		**it = s;
		(*it)++;
	}
}


/*
  Decimate everything by 16x, except 288kHz
  TODO : no offset tuning for direct sampling
*/

void RTLSDRThread::callback(const quint8* buf, qint32 len)
{
	qint16 xreal, yimag, phase;
	SampleVector::iterator it = m_convertBuffer.begin();

	int mode = 0;
	if (m_samplerate < 800000)
		mode = 2;
	// if (directsampling) mode++;

	switch(mode) {
		case 0:
			decimate16(&it, buf, len);
			break;
		case 1:
			// no_rotate16(&it, buf, len);
			break;

		case 2:
			decimate4(&it, buf, len);
			break;

		case 3:
			// norotate4(&it, buf, len);
			break;

		default:
			decimate16(&it, buf, len);
			break;
	}

	m_sampleFifo->write(m_convertBuffer.begin(), it);

	if(!m_running)
		rtlsdr_cancel_async(m_dev);
}

void RTLSDRThread::callbackHelper(unsigned char* buf, uint32_t len, void* ctx)
{
	RTLSDRThread* thread = (RTLSDRThread*)ctx;
	thread->callback(buf, len);
}
