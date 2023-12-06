///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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
#include "rtlsdrthread.h"

#include "dsp/samplesinkfifo.h"

#define FCD_BLOCKSIZE 16384

RTLSDRThread::RTLSDRThread(rtlsdr_dev_t* dev, SampleSinkFifo* sampleFifo, ReplayBuffer<quint8> *replayBuffer, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_convertBuffer(FCD_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_replayBuffer(replayBuffer),
	m_samplerate(288000),
	m_log2Decim(4),
	m_fcPos(0),
    m_iqOrder(true)
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
//  Len is total samples (i.e. one I and Q pair will have len=2)
void RTLSDRThread::callbackIQ(const quint8* inBuf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    // Save data to replay buffer
    m_replayBuffer->lock();
    bool replayEnabled = m_replayBuffer->getSize() > 0;
    if (replayEnabled) {
        m_replayBuffer->write(inBuf, len);
    }

    const quint8* buf = inBuf;
    qint32 remaining = len;

    while (remaining > 0)
    {
        // Choose between live data or replayed data
        if (replayEnabled && m_replayBuffer->useReplay()) {
            len = m_replayBuffer->read(remaining, buf);
        } else {
            len = remaining;
        }
        remaining -= len;

        if (m_log2Decim == 0)
        {
            m_decimatorsIQ.decimate1(&it, buf, len);
        }
        else
        {
            if (m_fcPos == 0) // Infradyne
            {
                switch (m_log2Decim)
                {
                case 1:
                    m_decimatorsIQ.decimate2_inf(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsIQ.decimate4_inf(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsIQ.decimate8_inf(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsIQ.decimate16_inf(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsIQ.decimate32_inf(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsIQ.decimate64_inf(&it, buf, len);
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
                    m_decimatorsIQ.decimate2_sup(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsIQ.decimate4_sup(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsIQ.decimate8_sup(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsIQ.decimate16_sup(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsIQ.decimate32_sup(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsIQ.decimate64_sup(&it, buf, len);
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
                    m_decimatorsIQ.decimate2_cen(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsIQ.decimate4_cen(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsIQ.decimate8_cen(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsIQ.decimate16_cen(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsIQ.decimate32_cen(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsIQ.decimate64_cen(&it, buf, len);
                    break;
                default:
                    break;
                }
            }
        }
    }

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);

    if(!m_running)
        rtlsdr_cancel_async(m_dev);
}

void RTLSDRThread::callbackQI(const quint8* inBuf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    // Save data to replay buffer
    m_replayBuffer->lock();
    bool replayEnabled = m_replayBuffer->getSize() > 0;
    if (replayEnabled) {
        m_replayBuffer->write(inBuf, len);
    }

    const quint8* buf = inBuf;
    qint32 remaining = len;

    while (remaining > 0)
    {
        // Choose between live data or replayed data
        if (replayEnabled && m_replayBuffer->useReplay()) {
            len = m_replayBuffer->read(remaining, buf);
        } else {
            len = remaining;
        }
        remaining -= len;

        if (m_log2Decim == 0)
        {
            m_decimatorsQI.decimate1(&it, buf, len);
        }
        else
        {
            if (m_fcPos == 0) // Infradyne
            {
                switch (m_log2Decim)
                {
                case 1:
                    m_decimatorsQI.decimate2_inf(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsQI.decimate4_inf(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsQI.decimate8_inf(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsQI.decimate16_inf(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsQI.decimate32_inf(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsQI.decimate64_inf(&it, buf, len);
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
                    m_decimatorsQI.decimate2_sup(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsQI.decimate4_sup(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsQI.decimate8_sup(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsQI.decimate16_sup(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsQI.decimate32_sup(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsQI.decimate64_sup(&it, buf, len);
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
                    m_decimatorsQI.decimate2_cen(&it, buf, len);
                    break;
                case 2:
                    m_decimatorsQI.decimate4_cen(&it, buf, len);
                    break;
                case 3:
                    m_decimatorsQI.decimate8_cen(&it, buf, len);
                    break;
                case 4:
                    m_decimatorsQI.decimate16_cen(&it, buf, len);
                    break;
                case 5:
                    m_decimatorsQI.decimate32_cen(&it, buf, len);
                    break;
                case 6:
                    m_decimatorsQI.decimate64_cen(&it, buf, len);
                    break;
                default:
                    break;
                }
            }
        }
    }

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);

    if(!m_running)
        rtlsdr_cancel_async(m_dev);
}

void RTLSDRThread::callbackHelper(unsigned char* buf, uint32_t len, void* ctx)
{
	RTLSDRThread* thread = (RTLSDRThread*) ctx;

    if (thread->m_iqOrder) {
    	thread->callbackIQ(buf, len);
    } else {
        thread->callbackQI(buf, len);
    }
}
