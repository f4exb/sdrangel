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
	m_decimation(1)
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

void RTLSDRThread::setDecimation(int decimation)
{
	m_decimation = decimation;
}

void RTLSDRThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

	while(m_running) {
		if((res = rtlsdr_read_async(m_dev, &RTLSDRThread::callbackHelper, this, 16, 2 * BLOCKSIZE)) < 0) {
			qCritical("RTLSDRThread: async error: %s", strerror(errno));
			break;
		}
	}

	m_running = false;
}

void RTLSDRThread::decimate2(SampleVector::iterator* it, const quint8* buf, qint32 len)
{
	for(int pos = 0; pos < len; pos += 2) {
		Sample s((((qint8)buf[pos]) - 128) << 8, (((qint8)buf[pos + 1]) - 128) << 8);
		if(m_decimator2.workDecimateCenter(&s)) {
			**it = s;
			++(*it);
		}
	}
}

void RTLSDRThread::decimate4(SampleVector::iterator* it, const quint8* buf, qint32 len)
{
	for(int pos = 0; pos < len; pos += 2) {
		Sample s((((qint8)buf[pos]) - 128) << 8, (((qint8)buf[pos + 1]) - 128) << 8);
		if(m_decimator2.workDecimateCenter(&s)) {
			if(m_decimator4.workDecimateCenter(&s)) {
				**it = s;
				++(*it);
			}
		}
	}
}

void RTLSDRThread::decimate8(SampleVector::iterator* it, const quint8* buf, qint32 len)
{
	for(int pos = 0; pos < len; pos += 2) {
		Sample s((((qint8)buf[pos]) - 128) << 8, (((qint8)buf[pos + 1]) - 128) << 8);
		if(m_decimator2.workDecimateCenter(&s)) {
			if(m_decimator4.workDecimateCenter(&s)) {
				if(m_decimator8.workDecimateCenter(&s)) {
					**it = s;
					++(*it);
				}
			}
		}
	}
}

void RTLSDRThread::decimate16(SampleVector::iterator* it, const quint8* buf, qint32 len)
{
	for(int pos = 0; pos < len; pos += 2) {
		Sample s((((qint8)buf[pos]) - 128) << 8, (((qint8)buf[pos + 1]) - 128) << 8);
		if(m_decimator2.workDecimateCenter(&s)) {
			if(m_decimator4.workDecimateCenter(&s)) {
				if(m_decimator8.workDecimateCenter(&s)) {
					if(m_decimator16.workDecimateCenter(&s)) {
						**it = s;
						++(*it);
					}
				}
			}
		}
	}
}

void RTLSDRThread::callback(const quint8* buf, qint32 len)
{
	SampleVector::iterator it = m_convertBuffer.begin();

	switch(m_decimation) {
		case 0: // 1:1 = no decimation
			for(int pos = 0; pos < len; pos += 2) {
				*it = Sample((((qint8)buf[pos]) - 128) << 8, (((qint8)buf[pos + 1]) - 128) << 8);
				++it;
			}
			break;

		case 1: // 1:2
			decimate2(&it, buf, len);
			break;

		case 2: // 1:4
			decimate4(&it, buf, len);
			break;

		case 3: // 1:8
			decimate8(&it, buf, len);
			break;

		case 4: // 1:16
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
