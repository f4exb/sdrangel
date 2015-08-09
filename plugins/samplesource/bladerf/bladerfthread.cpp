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

void BladerfThread::decimate2_sup(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+1] - buf[pos+2];
		yimag = - buf[pos+0] - buf[pos+3];
		Sample s( xreal << 3, yimag << 3 );
		**it = s;
		(*it)++;
		xreal = buf[pos+6] - buf[pos+5];
		yimag = buf[pos+4] + buf[pos+7];
		Sample t( xreal << 3, yimag << 3 );
		**it = t;
		(*it)++;
	}
}

/*
void BladerfThread::decimate2_cen(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	for (int pos = 0; pos < len - 1; pos += 2) {
		Sample s(buf[pos+0] << 3, buf[pos+1] << 3);
		if (m_decimator2.workDecimateCenter(&s)) {
			**it = s;
			(*it)++;
		}
	}
}

void BladerfThread::decimate4_cen(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	for (int pos = 0; pos < len - 1; pos += 2) {
		Sample s(buf[pos+0] << 4, buf[pos+1] << 4);
		if (m_decimator2.workDecimateCenter(&s)) {
			if (m_decimator4.workDecimateCenter(&s)) {
				**it = s;
				(*it)++;
			}
		}
	}
}

void BladerfThread::decimate8_cen(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	for (int pos = 0; pos < len - 1; pos += 2) {
		Sample s(buf[pos+0] << 4, buf[pos+1] << 4);
		if (m_decimator2.workDecimateCenter(&s)) {
			if (m_decimator4.workDecimateCenter(&s)) {
				if (m_decimator8.workDecimateCenter(&s)) {
					**it = s;
					(*it)++;
				}
			}
		}
	}
}

void BladerfThread::decimate16_cen(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	for (int pos = 0; pos < len - 11; pos += 2) {
		Sample s(buf[pos+0] << 4, buf[pos+1] << 4);
		if (m_decimator2.workDecimateCenter(&s)) {
			if (m_decimator4.workDecimateCenter(&s)) {
				if (m_decimator8.workDecimateCenter(&s)) {
					if (m_decimator16.workDecimateCenter(&s)) {
						**it = s;
						(*it)++;
					}
				}
			}
		}
	}
}

void BladerfThread::decimate32_cen(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	for (int pos = 0; pos < len - 11; pos += 2) {
		Sample s(buf[pos+0] << 4, buf[pos+1] << 4);
		if (m_decimator2.workDecimateCenter(&s)) {
			if (m_decimator4.workDecimateCenter(&s)) {
				if (m_decimator8.workDecimateCenter(&s)) {
					if (m_decimator16.workDecimateCenter(&s)) {
						if (m_decimator32.workDecimateCenter(&s)) {
							**it = s;
							(*it)++;
						}
					}
				}
			}
		}
	}
}
*/

void BladerfThread::decimate2_cen(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	for (int pos = 0; pos < len - 3; pos += 4) {
		Sample s1(buf[pos+0] << 3, buf[pos+1] << 3);
		Sample s2(buf[pos+2] << 3, buf[pos+3] << 3);
		m_decimator2.myDecimate(&s1, &s2);
		**it = s2;
		(*it)++;
	}
}

void BladerfThread::decimate4_cen(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	for (int pos = 0; pos < len - 7; pos += 8) {
		Sample s1(buf[pos+0] << 4, buf[pos+1] << 4);
		Sample s2(buf[pos+2] << 4, buf[pos+3] << 4);
		Sample s3(buf[pos+4] << 4, buf[pos+5] << 4);
		Sample s4(buf[pos+6] << 4, buf[pos+7] << 4);
		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);
		m_decimator4.myDecimate(&s2, &s4);
		**it = s4;
		(*it)++;
	}
}

void BladerfThread::decimate8_cen(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	for (int pos = 0; pos < len - 15; pos += 16) {
		Sample s1(buf[pos+0] << 4, buf[pos+1] << 4);
		Sample s2(buf[pos+2] << 4, buf[pos+3] << 4);
		Sample s3(buf[pos+4] << 4, buf[pos+5] << 4);
		Sample s4(buf[pos+6] << 4, buf[pos+7] << 4);
		Sample s5(buf[pos+8] << 4, buf[pos+9] << 4);
		Sample s6(buf[pos+10] << 4, buf[pos+11] << 4);
		Sample s7(buf[pos+12] << 4, buf[pos+13] << 4);
		Sample s8(buf[pos+14] << 4, buf[pos+15] << 4);
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

void BladerfThread::decimate16_cen(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	for (int pos = 0; pos < len - 31; pos += 32) {
		Sample s1(buf[pos+0] << 4, buf[pos+1] << 4);
		Sample s2(buf[pos+2] << 4, buf[pos+3] << 4);
		Sample s3(buf[pos+4] << 4, buf[pos+5] << 4);
		Sample s4(buf[pos+6] << 4, buf[pos+7] << 4);
		Sample s5(buf[pos+8] << 4, buf[pos+9] << 4);
		Sample s6(buf[pos+10] << 4, buf[pos+11] << 4);
		Sample s7(buf[pos+12] << 4, buf[pos+13] << 4);
		Sample s8(buf[pos+14] << 4, buf[pos+15] << 4);
		Sample s9(buf[pos+16] << 4, buf[pos+17] << 4);
		Sample s10(buf[pos+18] << 4, buf[pos+19] << 4);
		Sample s11(buf[pos+20] << 4, buf[pos+21] << 4);
		Sample s12(buf[pos+22] << 4, buf[pos+23] << 4);
		Sample s13(buf[pos+24] << 4, buf[pos+25] << 4);
		Sample s14(buf[pos+26] << 4, buf[pos+27] << 4);
		Sample s15(buf[pos+28] << 4, buf[pos+29] << 4);
		Sample s16(buf[pos+30] << 4, buf[pos+31] << 4);

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);
		m_decimator2.myDecimate(&s5, &s6);
		m_decimator2.myDecimate(&s7, &s8);
		m_decimator2.myDecimate(&s9, &s10);
		m_decimator2.myDecimate(&s11, &s12);
		m_decimator2.myDecimate(&s13, &s14);
		m_decimator2.myDecimate(&s15, &s16);

		m_decimator4.myDecimate(&s2, &s4);
		m_decimator4.myDecimate(&s6, &s8);
		m_decimator4.myDecimate(&s10, &s12);
		m_decimator4.myDecimate(&s14, &s16);

		m_decimator8.myDecimate(&s4, &s8);
		m_decimator8.myDecimate(&s12, &s16);

		m_decimator16.myDecimate(&s8, &s16);

		**it = s16;
		(*it)++;
	}
}

void BladerfThread::decimate32_cen(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	for (int pos = 0; pos < len - 63; pos += 64) {
		Sample s1(buf[pos+0] << 4, buf[pos+1] << 4);
		Sample s2(buf[pos+2] << 4, buf[pos+3] << 4);
		Sample s3(buf[pos+4] << 4, buf[pos+5] << 4);
		Sample s4(buf[pos+6] << 4, buf[pos+7] << 4);
		Sample s5(buf[pos+8] << 4, buf[pos+9] << 4);
		Sample s6(buf[pos+10] << 4, buf[pos+11] << 4);
		Sample s7(buf[pos+12] << 4, buf[pos+13] << 4);
		Sample s8(buf[pos+14] << 4, buf[pos+15] << 4);
		Sample s9(buf[pos+16] << 4, buf[pos+17] << 4);
		Sample s10(buf[pos+18] << 4, buf[pos+19] << 4);
		Sample s11(buf[pos+20] << 4, buf[pos+21] << 4);
		Sample s12(buf[pos+22] << 4, buf[pos+23] << 4);
		Sample s13(buf[pos+24] << 4, buf[pos+25] << 4);
		Sample s14(buf[pos+26] << 4, buf[pos+27] << 4);
		Sample s15(buf[pos+28] << 4, buf[pos+29] << 4);
		Sample s16(buf[pos+30] << 4, buf[pos+31] << 4);
		Sample s17(buf[pos+32] << 4, buf[pos+33] << 4);
		Sample s18(buf[pos+34] << 4, buf[pos+35] << 4);
		Sample s19(buf[pos+36] << 4, buf[pos+37] << 4);
		Sample s20(buf[pos+38] << 4, buf[pos+39] << 4);
		Sample s21(buf[pos+40] << 4, buf[pos+41] << 4);
		Sample s22(buf[pos+42] << 4, buf[pos+43] << 4);
		Sample s23(buf[pos+44] << 4, buf[pos+45] << 4);
		Sample s24(buf[pos+46] << 4, buf[pos+47] << 4);
		Sample s25(buf[pos+48] << 4, buf[pos+49] << 4);
		Sample s26(buf[pos+50] << 4, buf[pos+51] << 4);
		Sample s27(buf[pos+52] << 4, buf[pos+53] << 4);
		Sample s28(buf[pos+54] << 4, buf[pos+55] << 4);
		Sample s29(buf[pos+56] << 4, buf[pos+57] << 4);
		Sample s30(buf[pos+58] << 4, buf[pos+59] << 4);
		Sample s31(buf[pos+60] << 4, buf[pos+61] << 4);
		Sample s32(buf[pos+62] << 4, buf[pos+63] << 4);

		m_decimator2.myDecimate(&s1, &s2);
		m_decimator2.myDecimate(&s3, &s4);
		m_decimator2.myDecimate(&s5, &s6);
		m_decimator2.myDecimate(&s7, &s8);
		m_decimator2.myDecimate(&s9, &s10);
		m_decimator2.myDecimate(&s11, &s12);
		m_decimator2.myDecimate(&s13, &s14);
		m_decimator2.myDecimate(&s15, &s16);
		m_decimator2.myDecimate(&s17, &s18);
		m_decimator2.myDecimate(&s19, &s20);
		m_decimator2.myDecimate(&s21, &s22);
		m_decimator2.myDecimate(&s23, &s24);
		m_decimator2.myDecimate(&s25, &s26);
		m_decimator2.myDecimate(&s27, &s28);
		m_decimator2.myDecimate(&s29, &s30);
		m_decimator2.myDecimate(&s31, &s32);

		m_decimator4.myDecimate(&s2, &s4);
		m_decimator4.myDecimate(&s6, &s8);
		m_decimator4.myDecimate(&s10, &s12);
		m_decimator4.myDecimate(&s14, &s16);
		m_decimator4.myDecimate(&s18, &s20);
		m_decimator4.myDecimate(&s22, &s24);
		m_decimator4.myDecimate(&s26, &s28);
		m_decimator4.myDecimate(&s30, &s32);

		m_decimator8.myDecimate(&s4, &s8);
		m_decimator8.myDecimate(&s12, &s16);
		m_decimator8.myDecimate(&s20, &s24);
		m_decimator8.myDecimate(&s28, &s32);

		m_decimator16.myDecimate(&s8, &s16);
		m_decimator16.myDecimate(&s24, &s32);

		m_decimator32.myDecimate(&s16, &s32);

		**it = s32;
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

void BladerfThread::decimate4_sup(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	// Sup (USB):
	//            x  y   x  y   x   y  x   y  / x -> 1,-2,-5,6 / y -> -0,-3,4,7
	// [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
	// Inf (LSB):
	//            x  y   x  y   x   y  x   y  / x -> 0,-3,-4,7 / y -> 1,2,-5,-6
	// [ rotate:  0, 1, -3, 2, -4, -5, 7, -6]
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 7; pos += 8) {
		xreal = buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6];
		yimag = - buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7];
		//xreal = buf[pos+0] - buf[pos+3] - buf[pos+4] + buf[pos+7];
		//yimag = buf[pos+1] + buf[pos+2] - buf[pos+5] - buf[pos+6];
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

void BladerfThread::decimate8_sup(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	qint16 xreal, yimag;
	for (int pos = 0; pos < len - 15; pos += 8) {
		xreal = buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6];
		yimag = - buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7];
		Sample s1( xreal << 2, yimag << 2 ); // was shift 3
		pos += 8;
		xreal = buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6];
		yimag = - buf[pos+0] - buf[pos+3] + buf[pos+4] + buf[pos+7];
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

void BladerfThread::decimate16_sup(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	// Offset tuning: 4x downsample and rotate, then
	// downsample 4x more. [ rotate:  1, 0, -2, 3, -5, -4, 6, -7]
	qint16 xreal[4], yimag[4];

	for (int pos = 0; pos < len - 31; ) {
		for (int i = 0; i < 4; i++) {
			xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << 2; // was shift 4
			yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]) << 2; // was shift 4
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
			xreal[i] = (buf[pos+0] - buf[pos+3] + buf[pos+7] - buf[pos+4]) << 2;
			yimag[i] = (buf[pos+1] - buf[pos+5] + buf[pos+2] - buf[pos+6]) << 2;
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

void BladerfThread::decimate32_sup(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	qint16 xreal[8], yimag[8];

	for (int pos = 0; pos < len - 63; ) {
		for (int i = 0; i < 8; i++) {
			xreal[i] = (buf[pos+1] - buf[pos+2] - buf[pos+5] + buf[pos+6]) << 2;
			yimag[i] = (buf[pos+4] + buf[pos+7] - buf[pos+0] - buf[pos+3]) << 2;
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

	if (m_log2Decim == 0)
	{
		decimate1(&it, buf, len);
	}
	else
	{
		if (m_fcPos == 0) // Infra
		{
			switch (m_log2Decim)
			{
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
		}
		else if (m_fcPos == 1) // Supra
		{
			switch (m_log2Decim)
			{
			case 1:
				decimate2_sup(&it, buf, len);
				break;
			case 2:
				decimate4_sup(&it, buf, len);
				break;
			case 3:
				decimate8_sup(&it, buf, len);
				break;
			case 4:
				decimate16_sup(&it, buf, len);
				break;
			case 5:
				decimate32_sup(&it, buf, len);
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
				decimate2_cen(&it, buf, len);
				break;
			case 2:
				decimate4_cen(&it, buf, len);
				break;
			case 3:
				decimate8_cen(&it, buf, len);
				break;
			case 4:
				decimate16_cen(&it, buf, len);
				break;
			case 5:
				decimate32_cen(&it, buf, len);
				break;
			default:
				break;
			}
		}
	}


	m_sampleFifo->write(m_convertBuffer.begin(), it);
}
