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
#include "dsp/samplefifo.h"
#include "bladerfthread.h"



BladerfThread::BladerfThread(struct bladerf* dev, SampleFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_dev(dev),
	m_convertBuffer(BLADERF_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_samplerate(3072000),
	m_log2Decim(0)
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

void BladerfThread::decimate1(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	qint16 xreal, yimag;

	for (int pos = 0; pos < len; pos += 2) {
		xreal = buf[pos+0];
		yimag = buf[pos+1];
		Sample s( xreal * 16, yimag * 16 ); // shift by 4 bit positions (signed)
		**it = s;
		(*it)++;
	}
}

void BladerfThread::decimate2_u(SampleVector::iterator* it, const quint16* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3];
		yimag = buf[pos+1] + buf[pos+2] - 255;
		Sample s( xreal << 3, yimag << 3 );
		**it = s;
		(*it)++;
		xreal = buf[pos+7] - buf[pos+4];
		yimag = 255 - buf[pos+5] - buf[pos+6];
		Sample t( xreal << 3, yimag << 3 );
		**it = t;
		(*it)++;
	}
}

void BladerfThread::decimate2(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3];
		yimag = buf[pos+1] + buf[pos+2];
		Sample s( xreal << 3, yimag << 3 );
		**it = s;
		(*it)++;
		xreal = buf[pos+7] - buf[pos+4];
		yimag = - buf[pos+5] - buf[pos+6];
		Sample t( xreal << 3, yimag << 3 );
		**it = t;
		(*it)++;
	}
}

void BladerfThread::decimate4(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	qint16 xreal, yimag;
		for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
		yimag = buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];
		Sample s( xreal << 2, yimag << 2 ); // was shift 3
		**it = s;
		(*it)++;
	}
}

void BladerfThread::decimate8(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 15; pos += 8) {
		xreal = buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
		yimag = buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];
		Sample s1( xreal << 2, yimag << 2 ); // was shift 3
		pos += 8;
		xreal = buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4];
		yimag = buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6];
		Sample s2( xreal << 2, yimag << 2 ); // was shift 3

		m_decimator2.myDecimate(&s1, &s2);
		**it = s2;
		(*it)++;
	}
}

void BladerfThread::decimate16(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	// Offset tuning: 4x downsample and rotate, then
	// downsample 4x more. [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
	qint16 xreal[4], yimag[4];

	for (int pos = 0; pos < len - 31; ) {
		for (int i = 0; i < 4; i++) {
			xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << 2; // was shift 4
			yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << 2; // was shift 4
			pos += 8;
		}

		Sample s1( xreal[0], yimag[0] );
		Sample s2( xreal[1], yimag[1] );
		Sample s3( xreal[2], yimag[2] );
		Sample s4( xreal[3], yimag[3] );

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);

		m_decimator4.myDecimate(&s2, &s4);

		**it = s4;
		(*it)++;
	}
}

void BladerfThread::decimate32(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	qint16 xreal[8], yimag[8];

	for (int pos = 0; pos < len - 63; ) {
		for (int i = 0; i < 8; i++) {
			xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << 1;
			yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << 1;
			pos += 8;
		}

		Sample s1( xreal[0], yimag[0] );
		Sample s2( xreal[1], yimag[1] );
		Sample s3( xreal[2], yimag[2] );
		Sample s4( xreal[3], yimag[3] );
		Sample s5( xreal[4], yimag[4] );
		Sample s6( xreal[5], yimag[5] );
		Sample s7( xreal[6], yimag[6] );
		Sample s8( xreal[7], yimag[7] );

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);
		m_decimator2.myDecimate(&s5, &s6);
		m_decimator2.myDecimate(&s7, &s8);

		m_decimator4.myDecimate(&s2, &s4);
		m_decimator4.myDecimate(&s6, &s8);

		m_decimator8.myDecimate(&s4, &s8);

		**it = s8;
		(*it)++;
	}
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void BladerfThread::callback(const qint16* buf, qint32 len)
{
	SampleVector::iterator it = m_convertBuffer.begin();

	switch (m_log2Decim)
	{
	case 0:
		decimate1(&it, buf, len);
		break;
	case 1:
		decimate2(&it, buf, len);
		break;
	case 2:
		decimate4(&it, buf, len);
		break;
	case 3:
		decimate8(&it, buf, len);
		break;
	case 4:
		decimate16(&it, buf, len);
		break;
	case 5:
		decimate32(&it, buf, len);
		break;
	default:
		break;
	}

	m_sampleFifo->write(m_convertBuffer.begin(), it);
}
