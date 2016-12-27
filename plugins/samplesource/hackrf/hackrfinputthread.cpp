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

#include "../../../sdrbase/dsp/samplesinkfifo.h"
#include "hackrfinputthread.h"

HackRFInputThread *HackRFInputThread::m_this = 0;

HackRFInputThread::HackRFInputThread(hackrf_device* dev, SampleSinkFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_convertBuffer(HACKRF_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_samplerate(10),
	m_log2Decim(0),
	m_fcPos(0)
{
	m_this = this;
}

HackRFInputThread::~HackRFInputThread()
{
	stopWork();
	m_this = 0;
}

void HackRFInputThread::startWork()
{
	//m_startWaitMutex.lock();
	m_running = true;
	start();
	/*
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();*/
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

	//m_running = true;
	m_startWaiter.wakeAll();

	rc = (hackrf_error) hackrf_start_rx(m_dev, rx_callback, NULL);

	if (rc != HACKRF_SUCCESS)
	{
		qCritical("HackRFThread::run: failed to start HackRF Rx: %s", hackrf_error_name(rc));
	}
	else
	{
		while ((m_running) && (hackrf_is_streaming(m_dev) == HACKRF_TRUE))
		{
			sleep(1);
		}
	}

	rc = (hackrf_error) hackrf_stop_rx(m_dev);

	if (rc == HACKRF_SUCCESS)
	{
		qDebug("HackRFThread::run: stopped HackRF Rx");
	}
	else
	{
		qDebug("HackRFThread::run: failed to stop HackRF Rx: %s", hackrf_error_name(rc));
	}

	//m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void HackRFInputThread::callback(const qint8* buf, qint32 len)
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
			case 6:
				m_decimators.decimate64_inf(&it, buf, len);
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
			case 6:
				m_decimators.decimate64_sup(&it, buf, len);
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
			case 6:
				m_decimators.decimate64_cen(&it, buf, len);
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
	qint32 bytes_to_write = transfer->valid_length;
	m_this->callback((qint8 *) transfer->buffer, bytes_to_write);
	return 0;
}
