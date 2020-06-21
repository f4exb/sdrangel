///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <QtGlobal>
#include <algorithm>
#include "perseusthread.h"

PerseusThread *PerseusThread::m_this = 0;

PerseusThread::PerseusThread(perseus_descr* dev, SampleSinkFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_convertBuffer(PERSEUS_NBSAMPLES),
    m_sampleFifo(sampleFifo),
    m_log2Decim(0),
    m_iqOrder(true)
{
    m_this = this;
    std::fill(m_buf, m_buf + 2*PERSEUS_NBSAMPLES, 0);
}

PerseusThread::~PerseusThread()
{
    stopWork();
    m_this = 0;
}

void PerseusThread::startWork()
{
    qDebug("PerseusThread::startWork");
    m_startWaitMutex.lock();
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void PerseusThread::stopWork()
{
    qDebug("PerseusThread::stopWork");
    m_running = false;
    wait();
}

void PerseusThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

void PerseusThread::run()
{
	m_running = true;
	m_startWaiter.wakeAll();

	int rc = perseus_start_async_input(m_dev, PERSEUS_BLOCKSIZE, rx_callback, 0);

	if (rc < 0) {
		qCritical("PerseusThread::run: failed to start Perseus Rx: %s", perseus_errorstr());
	}
	else
	{
	    qDebug("PerseusThread::run: start Perseus Rx");
		while (m_running) {
			sleep(1);
		}
	}

	rc = perseus_stop_async_input(m_dev);

	if (rc < 0) {
		qCritical("PerseusThread::run: failed to stop Perseus Rx: %s", perseus_errorstr());
	} else {
		qDebug("PerseusThread::run: stopped Perseus Rx");
	}

	m_running = false;
}

void PerseusThread::callbackIQ(const uint8_t* buf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    switch (m_log2Decim)
    {
    case 0:
        m_decimators32IQ.decimate1(&it, (TripleByteLE<qint32>*) buf, len);
        break;
    case 1:
        m_decimators64IQ.decimate2_cen(&it, (TripleByteLE<qint64>*) buf, len);
        break;
    case 2:
        m_decimators64IQ.decimate4_cen(&it, (TripleByteLE<qint64>*) buf, len);
        break;
    default:
        break;
    }

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}

void PerseusThread::callbackQI(const uint8_t* buf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    switch (m_log2Decim)
    {
    case 0:
        m_decimators32QI.decimate1(&it, (TripleByteLE<qint32>*) buf, len);
        break;
    case 1:
        m_decimators64QI.decimate2_cen(&it, (TripleByteLE<qint64>*) buf, len);
        break;
    case 2:
        m_decimators64QI.decimate4_cen(&it, (TripleByteLE<qint64>*) buf, len);
        break;
    default:
        break;
    }

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}

int PerseusThread::rx_callback(void *buf, int buf_size, void *extra)
{
    (void) extra;
	qint32 nbIAndQ = buf_size / 3; // 3 bytes per I or Q

    if (m_this->m_iqOrder) {
    	m_this->callbackIQ((uint8_t*) buf, nbIAndQ);
    } else {
        m_this->callbackQI((uint8_t*) buf, nbIAndQ);
    }

    return 0;
}

