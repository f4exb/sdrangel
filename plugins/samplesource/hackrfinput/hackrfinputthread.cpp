///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include "hackrfinputthread.h"

#include <stdio.h>
#include <errno.h>
#include <algorithm>

#include "dsp/samplesinkfifo.h"

HackRFInputThread::HackRFInputThread(hackrf_device* dev, SampleSinkFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_convertBuffer(HACKRF_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_samplerate(10),
	m_log2Decim(0),
	m_fcPos(0),
    m_iqOrder(true)
{
    std::fill(m_buf, m_buf + 2*HACKRF_BLOCKSIZE, 0);
}

HackRFInputThread::~HackRFInputThread()
{
	stopWork();
}

void HackRFInputThread::startWork()
{
	m_startWaitMutex.lock();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void HackRFInputThread::stopWork()
{
	qDebug("HackRFThread::stopWork");
	m_running = false;
	wait();
}

void HackRFInputThread::setSamplerate(uint32_t samplerate)
{
	m_samplerate = samplerate;
}

void HackRFInputThread::setLog2Decimation(unsigned int log2_decim)
{
	m_log2Decim = log2_decim;
}

void HackRFInputThread::setFcPos(int fcPos)
{
	m_fcPos = fcPos;
}

void HackRFInputThread::run()
{
	hackrf_error rc;

    m_running = true;
    m_startWaiter.wakeAll();

    if (hackrf_is_streaming(m_dev) == HACKRF_TRUE)
    {
        qDebug("HackRFInputThread::run: HackRF is streaming already");
    }
    else
    {
        qDebug("HackRFInputThread::run: HackRF is not streaming");

        rc = (hackrf_error) hackrf_start_rx(m_dev, rx_callback, this);

        if (rc == HACKRF_SUCCESS)
        {
            qDebug("HackRFInputThread::run: started HackRF Rx");
        }
        else
        {
            qDebug("HackRFInputThread::run: failed to start HackRF Rx: %s", hackrf_error_name(rc));
        }
    }

    while ((m_running) && (hackrf_is_streaming(m_dev) == HACKRF_TRUE))
    {
        usleep(200000);
    }

    if (hackrf_is_streaming(m_dev) == HACKRF_TRUE)
    {
        rc = (hackrf_error) hackrf_stop_rx(m_dev);

        if (rc == HACKRF_SUCCESS)
        {
            qDebug("HackRFInputThread::run: stopped HackRF Rx");
        }
        else
        {
            qDebug("HackRFInputThread::run: failed to stop HackRF Rx: %s", hackrf_error_name(rc));
        }
    }

	m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void HackRFInputThread::callbackIQ(const qint8* buf, qint32 len)
{
	SampleVector::iterator it = m_convertBuffer.begin();

	if (m_log2Decim == 0)
	{
		m_decimatorsIQ.decimate1(&it, buf, len);
	}
	else
	{
		if (m_fcPos == 0) // Infra
		{
			switch (m_log2Decim)
			{
			case 1:
				m_decimatorsIQ.decimate2_inf(&it, buf, len);
				break;
			case 2:
				m_decimatorsIQ.decimate4_inf_txsync(&it, buf, len);
				break;
			case 3:
				m_decimatorsIQ.decimate8_inf_txsync(&it, buf, len);
				break;
			case 4:
				m_decimatorsIQ.decimate16_inf_txsync(&it, buf, len);
				break;
			case 5:
				m_decimatorsIQ.decimate32_inf_txsync(&it, buf, len);
				break;
			case 6:
				m_decimatorsIQ.decimate64_inf_txsync(&it, buf, len);
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
				m_decimatorsIQ.decimate2_sup(&it, buf, len);
				break;
			case 2:
				m_decimatorsIQ.decimate4_sup_txsync(&it, buf, len);
				break;
			case 3:
				m_decimatorsIQ.decimate8_sup_txsync(&it, buf, len);
				break;
			case 4:
				m_decimatorsIQ.decimate16_sup_txsync(&it, buf, len);
				break;
			case 5:
				m_decimatorsIQ.decimate32_sup_txsync(&it, buf, len);
				break;
			case 6:
				m_decimatorsIQ.decimate64_sup_txsync(&it, buf, len);
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

	m_sampleFifo->write(m_convertBuffer.begin(), it);
}

void HackRFInputThread::callbackQI(const qint8* buf, qint32 len)
{
	SampleVector::iterator it = m_convertBuffer.begin();

	if (m_log2Decim == 0)
	{
		m_decimatorsQI.decimate1(&it, buf, len);
	}
	else
	{
		if (m_fcPos == 0) // Infra
		{
			switch (m_log2Decim)
			{
			case 1:
				m_decimatorsQI.decimate2_inf(&it, buf, len);
				break;
			case 2:
				m_decimatorsQI.decimate4_inf_txsync(&it, buf, len);
				break;
			case 3:
				m_decimatorsQI.decimate8_inf_txsync(&it, buf, len);
				break;
			case 4:
				m_decimatorsQI.decimate16_inf_txsync(&it, buf, len);
				break;
			case 5:
				m_decimatorsQI.decimate32_inf_txsync(&it, buf, len);
				break;
			case 6:
				m_decimatorsQI.decimate64_inf_txsync(&it, buf, len);
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
				m_decimatorsQI.decimate2_sup(&it, buf, len);
				break;
			case 2:
				m_decimatorsQI.decimate4_sup_txsync(&it, buf, len);
				break;
			case 3:
				m_decimatorsQI.decimate8_sup_txsync(&it, buf, len);
				break;
			case 4:
				m_decimatorsQI.decimate16_sup_txsync(&it, buf, len);
				break;
			case 5:
				m_decimatorsQI.decimate32_sup_txsync(&it, buf, len);
				break;
			case 6:
				m_decimatorsQI.decimate64_sup_txsync(&it, buf, len);
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

	m_sampleFifo->write(m_convertBuffer.begin(), it);
}

int HackRFInputThread::rx_callback(hackrf_transfer* transfer)
{
    HackRFInputThread *thread = (HackRFInputThread *) transfer->rx_ctx;
	qint32 bytes_to_write = transfer->valid_length;

    if (thread->m_iqOrder) {
    	thread->callbackIQ((qint8 *) transfer->buffer, bytes_to_write);
    } else {
        thread->callbackQI((qint8 *) transfer->buffer, bytes_to_write);
    }

    return 0;
}
