
///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include "dsp/samplesinkfifo.h"
#include "airspyhfworker.h"

AirspyHFWorker::AirspyHFWorker(airspyhf_device_t* dev, SampleSinkFifo* sampleFifo, ReplayBuffer<float> *replayBuffer, QObject* parent) :
	QObject(parent),
	m_dev(dev),
	m_convertBuffer(AIRSPYHF_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
    m_replayBuffer(replayBuffer),
	m_samplerate(10),
	m_log2Decim(0),
    m_iqOrder(true)
{
    std::fill(m_buf, m_buf + 2*AIRSPYHF_BLOCKSIZE, 0);
}

AirspyHFWorker::~AirspyHFWorker()
{
	stopWork();
}

void AirspyHFWorker::startWork()
{
    qDebug("AirspyHFWorker::startWork");
	airspyhf_error rc = (airspyhf_error) airspyhf_start(m_dev, rx_callback, this);

	if (rc != AIRSPYHF_SUCCESS) {
		qCritical("AirspyHFWorker::run: failed to start Airspy HF Rx");
	}
}

void AirspyHFWorker::stopWork()
{
	qDebug("AirspyHFWorker::stopWork");
	airspyhf_error rc = (airspyhf_error) airspyhf_stop(m_dev);

	if (rc == AIRSPYHF_SUCCESS) {
		qDebug("AirspyHFWorker::run: stopped Airspy HF Rx");
	} else {
		qDebug("AirspyHFWorker::run: failed to stop Airspy HF Rx");
	}
}

void AirspyHFWorker::setSamplerate(uint32_t samplerate)
{
	m_samplerate = samplerate;
}

void AirspyHFWorker::setLog2Decimation(unsigned int log2_decim)
{
	m_log2Decim = log2_decim;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void AirspyHFWorker::callbackIQ(const float* inBuf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    // Save data to replay buffer
    m_replayBuffer->lock();
    bool replayEnabled = m_replayBuffer->getSize() > 0;
    if (replayEnabled) {
        m_replayBuffer->write(inBuf, len);
    }

    const float* buf = inBuf;
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

        switch (m_log2Decim)
        {
        case 0:
            m_decimatorsIQ.decimate1(&it, buf, len);
            break;
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
        case 7:
            m_decimatorsIQ.decimate128_cen(&it, buf, len);
            break;
        case 8:
            m_decimatorsIQ.decimate256_cen(&it, buf, len);
            break;
        default:
            break;
        }
    }

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}

void AirspyHFWorker::callbackQI(const float* inBuf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

   // Save data to replay buffer
    m_replayBuffer->lock();
    bool replayEnabled = m_replayBuffer->getSize() > 0;
    if (replayEnabled) {
        m_replayBuffer->write(inBuf, len);
    }

    const float* buf = inBuf;
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

        switch (m_log2Decim)
        {
        case 0:
            m_decimatorsQI.decimate1(&it, buf, len);
            break;
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
        case 7:
            m_decimatorsQI.decimate128_cen(&it, buf, len);
            break;
        case 8:
            m_decimatorsQI.decimate256_cen(&it, buf, len);
            break;
        default:
            break;
        }
    }

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}

int AirspyHFWorker::rx_callback(airspyhf_transfer_t* transfer)
{
    //qDebug("AirspyHFWorker::rx_callback");
    AirspyHFWorker *worker = (AirspyHFWorker*) transfer->ctx;
	qint32 nbIAndQ = transfer->sample_count * 2;

    if (worker->m_iqOrder) {
    	worker->callbackIQ((float *) transfer->samples, nbIAndQ);
    } else {
        worker->callbackQI((float *) transfer->samples, nbIAndQ);
    }

    return 0;
}
