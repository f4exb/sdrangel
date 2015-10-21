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
#include "dsp/samplefifo.h"
#include "bladerfthread.h"



BladerfThread::BladerfThread(struct bladerf* dev, SampleFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_convertBuffer(BLADERF_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_samplerate(10),
	m_log2Decim(0),
	m_fcPos(0)
{
}

BladerfThread::~BladerfThread()
{
	stopWork();
}

void BladerfThread::startWork()
{
	m_startWaitMutex.lock();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void BladerfThread::stopWork()
{
	m_running = false;
	wait();
}

void BladerfThread::setSamplerate(int samplerate)
{
	m_samplerate = samplerate;
}

void BladerfThread::setLog2Decimation(unsigned int log2_decim)
{
	m_log2Decim = log2_decim;
}

void BladerfThread::setFcPos(int fcPos)
{
	m_fcPos = fcPos;
}

void BladerfThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

	while(m_running) {
		if((res = bladerf_sync_rx(m_dev, m_buf, BLADERF_BLOCKSIZE, NULL, 10000)) < 0) {
			qCritical("BladerfThread: sync error: %s", strerror(errno));
			break;
		}

		callback(m_buf, 2 * BLADERF_BLOCKSIZE);
	}

	m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void BladerfThread::callback(const qint16* buf, qint32 len)
{
	SampleVector::iterator it = m_convertBuffer.begin();

	if (m_log2Decim == 0)
	{
		m_decimators.decimate1(&it, buf, len);
	}
	else
	{
		if (m_fcPos == 0) // Infra
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
			default:
				break;
			}
		}
		else if (m_fcPos == 1) // Supra
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
			default:
				break;
			}
		}
		else if (m_fcPos == 2) // Center
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
			default:
				break;
			}
		}
	}


	m_sampleFifo->write(m_convertBuffer.begin(), it);
}
