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

#include <stdio.h>
#include <errno.h>
#include <algorithm>

#include "airspyworker.h"

#include "dsp/samplesinkfifo.h"

AirspyWorker::AirspyWorker(struct airspy_device* dev, SampleSinkFifo* sampleFifo, QObject* parent) :
	QObject(parent),
	m_running(false),
	m_dev(dev),
	m_convertBuffer(AIRSPY_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_samplerate(10),
	m_log2Decim(0),
	m_fcPos(0),
    m_iqOrder(true)
{
	std::fill(m_buf, m_buf + 2*AIRSPY_BLOCKSIZE, 0);
}

AirspyWorker::~AirspyWorker()
{
	stopWork();
}

bool AirspyWorker::startWork()
{
	airspy_error rc;

	rc = (airspy_error) airspy_start_rx(m_dev, rx_callback, this);

	if (rc == AIRSPY_SUCCESS)
	{
    	m_running = (airspy_is_streaming(m_dev) == AIRSPY_TRUE);
	}
    else
    {
		qCritical("AirspyWorker::run: failed to start Airspy Rx: %s", airspy_error_name(rc));
        m_running = false;
    }

    return m_running;
}

void AirspyWorker::stopWork()
{
	airspy_error rc = (airspy_error) airspy_stop_rx(m_dev);

	if (rc == AIRSPY_SUCCESS) {
		qDebug("AirspyWorker::run: stopped Airspy Rx");
	} else {
		qDebug("AirspyWorker::run: failed to stop Airspy Rx: %s", airspy_error_name(rc));
	}

	m_running = false;
}

void AirspyWorker::setSamplerate(uint32_t samplerate)
{
	m_samplerate = samplerate;
}

void AirspyWorker::setLog2Decimation(unsigned int log2_decim)
{
	m_log2Decim = log2_decim;
}

void AirspyWorker::setFcPos(int fcPos)
{
	m_fcPos = fcPos;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void AirspyWorker::callbackIQ(const qint16* buf, qint32 len)
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
		else if (m_fcPos == 1) // Supra
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

void AirspyWorker::callbackQI(const qint16* buf, qint32 len)
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
		else if (m_fcPos == 1) // Supra
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

int AirspyWorker::rx_callback(airspy_transfer_t* transfer)
{
    AirspyWorker *worker = (AirspyWorker*) transfer->ctx;
	qint32 bytes_to_write = transfer->sample_count * sizeof(qint16);

    if (worker->m_iqOrder) {
    	worker->callbackIQ((qint16 *) transfer->samples, bytes_to_write);
    } else {
        worker->callbackQI((qint16 *) transfer->samples, bytes_to_write);
    }

    return 0;
}
