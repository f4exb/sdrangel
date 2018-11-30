
///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/samplesinkfifo.h"
#include "airspyhfthread.h"

AirspyHFThread *AirspyHFThread::m_this = 0;

AirspyHFThread::AirspyHFThread(airspyhf_device_t* dev, SampleSinkFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_convertBuffer(AIRSPYHF_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_samplerate(10),
	m_log2Decim(0)
{
    memset((char*) m_buf, 0, 2*AIRSPYHF_BLOCKSIZE*sizeof(qint16));
	m_this = this;
}

AirspyHFThread::~AirspyHFThread()
{
	stopWork();
	m_this = 0;
}

void AirspyHFThread::startWork()
{
	m_startWaitMutex.lock();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void AirspyHFThread::stopWork()
{
	qDebug("AirspyThread::stopWork");
	m_running = false;
	wait();
}

void AirspyHFThread::setSamplerate(uint32_t samplerate)
{
	m_samplerate = samplerate;
}

void AirspyHFThread::setLog2Decimation(unsigned int log2_decim)
{
	m_log2Decim = log2_decim;
}

void AirspyHFThread::run()
{
    airspyhf_error rc;

	m_running = true;
	m_startWaiter.wakeAll();

	rc = (airspyhf_error) airspyhf_start(m_dev, rx_callback, 0);

	if (rc != AIRSPYHF_SUCCESS)
	{
		qCritical("AirspyHFFThread::run: failed to start Airspy HF Rx");
	}
	else
	{
		while ((m_running) && (airspyhf_is_streaming(m_dev) != 0))
		{
			sleep(1);
		}
	}

	rc = (airspyhf_error) airspyhf_stop(m_dev);

	if (rc == AIRSPYHF_SUCCESS) {
		qDebug("AirspyHFFThread::run: stopped Airspy HF Rx");
	} else {
		qDebug("AirspyHFFThread::run: failed to stop Airspy HF Rx");
	}

	m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void AirspyHFThread::callback(const float* buf, qint32 len)
{
	SampleVector::iterator it = m_convertBuffer.begin();

    switch (m_log2Decim)
    {
    case 0:
        m_decimators.decimate1(&it, buf, len);
        break;
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

	m_sampleFifo->write(m_convertBuffer.begin(), it);
}


int AirspyHFThread::rx_callback(airspyhf_transfer_t* transfer)
{
	qint32 nbIAndQ = transfer->sample_count * 2;
	m_this->callback((float *) transfer->samples, nbIAndQ);
	return 0;
}
