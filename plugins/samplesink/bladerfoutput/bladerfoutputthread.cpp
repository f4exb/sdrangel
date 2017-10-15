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
#include "bladerfoutputthread.h"



BladerfOutputThread::BladerfOutputThread(struct bladerf* dev, SampleSourceFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_sampleFifo(sampleFifo),
	m_log2Interp(0),
	m_fcPos(0)
{
}

BladerfOutputThread::~BladerfOutputThread()
{
	stopWork();
}

void BladerfOutputThread::startWork()
{
	m_startWaitMutex.lock();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void BladerfOutputThread::stopWork()
{
	m_running = false;
	wait();
}

void BladerfOutputThread::setLog2Interpolation(unsigned int log2_interp)
{
	m_log2Interp = log2_interp;
}

void BladerfOutputThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

	while (m_running)
	{
        callback(m_buf, BLADERFOUTPUT_BLOCKSIZE);

        if((res = bladerf_sync_tx(m_dev, m_buf, BLADERFOUTPUT_BLOCKSIZE, NULL, 10000)) < 0)
		{
			qCritical("BladerdOutputThread:run: sync error: %s", strerror(errno));
			break;
		}
	}

	m_running = false;
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16)
void BladerfOutputThread::callback(qint16* buf, qint32 len)
{
    SampleVector::iterator beginRead;
    m_sampleFifo->readAdvance(beginRead, len/(1<<m_log2Interp));
    beginRead -= len;

    if (m_log2Interp == 0)
	{
		m_interpolators.interpolate1(&beginRead, buf, len*2);
	}
	else
	{
        switch (m_log2Interp)
        {
        case 1:
            m_interpolators.interpolate2_cen(&beginRead, buf, len*2);
            break;
        case 2:
            m_interpolators.interpolate4_cen(&beginRead, buf, len*2);
            break;
        case 3:
            m_interpolators.interpolate8_cen(&beginRead, buf, len*2);
            break;
        case 4:
            m_interpolators.interpolate16_cen(&beginRead, buf, len*2);
            break;
        case 5:
            m_interpolators.interpolate32_cen(&beginRead, buf, len*2);
            break;
        case 6:
            m_interpolators.interpolate64_cen(&beginRead, buf, len*2);
            break;
        default:
            break;
        }
	}
}
