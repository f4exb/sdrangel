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

#include "dsp/samplesinkfifo.h"

#define FCD_BLOCKSIZE 16384

RTLSDRThread::RTLSDRThread(rtlsdr_dev_t* dev, SampleSinkFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_convertBuffer(FCD_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_samplerate(288000),
	m_log2Decim(4),
	m_fcPos(0)
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

void RTLSDRThread::setLog2Decimation(unsigned int log2_decim)
{
	m_log2Decim = log2_decim;
}

void RTLSDRThread::setFcPos(int fcPos)
{
	m_fcPos = fcPos;
}

void RTLSDRThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

	while(m_running) {
		if((res = rtlsdr_read_async(m_dev, &RTLSDRThread::callbackHelper, this, 32, FCD_BLOCKSIZE)) < 0) {
			qCritical("RTLSDRThread: async error: %s", strerror(errno));
			break;
		}
	}

	m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void RTLSDRThread::callback(const quint8* buf, qint32 len)
{
	SampleVector::iterator it = m_convertBuffer.begin();

	if (m_log2Decim == 0)
	{
		m_decimators.decimate1(&it, buf, len);
	}
	else
	{
		if (m_fcPos == 0) // Infradyne
		{
			switch (m_log2Decim)
			{
			case 1:
				m_decimators.decimate2_inf(&it, buf, len);
				break;
			case 2:
				m_decimators.decimate4_inf(&it, buf, len);
				break;
			case 3:
				m_decimators.decimate8_inf(&it, buf, len);
				break;
			case 4:
				m_decimators.decimate16_inf(&it, buf, len);
				break;
            case 5:
                m_decimators.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_decimators.decimate64_inf(&it, buf, len);
                break;
			default:
				break;
			}
		}
		else if (m_fcPos == 1) // Supradyne
		{
			switch (m_log2Decim)
			{
			case 1:
				m_decimators.decimate2_sup(&it, buf, len);
				break;
			case 2:
				m_decimators.decimate4_sup(&it, buf, len);
				break;
			case 3:
				m_decimators.decimate8_sup(&it, buf, len);
				break;
			case 4:
				m_decimators.decimate16_sup(&it, buf, len);
				break;
            case 5:
                m_decimators.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_decimators.decimate64_sup(&it, buf, len);
                break;
			default:
				break;
			}
		}
		else // Centered
		{
			switch (m_log2Decim)
			{
			case 1:
				m_decimators.decimate2_cen(&it, buf, len);
				break;
			case 2:
				m_decimators.decimate4_cen(&it, buf, len);
				break;
			case 3:
				m_decimators.decimate8_cen(&it, buf, len);
				break;
			case 4:
				m_decimators.decimate16_cen(&it, buf, len);
				break;
            case 5:
                m_decimators.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_decimators.decimate64_cen(&it, buf, len);
                break;
			default:
				break;
			}
		}
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

