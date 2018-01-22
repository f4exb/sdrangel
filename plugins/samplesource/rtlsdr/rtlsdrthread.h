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

#ifndef INCLUDE_RTLSDRTHREAD_H
#define INCLUDE_RTLSDRTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <rtl-sdr.h>

#include "dsp/samplesinkfifo.h"
#include "dsp/decimatorsu.h"

class RTLSDRThread : public QThread {
	Q_OBJECT

public:
	RTLSDRThread(rtlsdr_dev_t* dev, SampleSinkFifo* sampleFifo, QObject* parent = NULL);
	~RTLSDRThread();

	void startWork();
	void stopWork();
	void setSamplerate(int samplerate);
	void setLog2Decimation(unsigned int log2_decim);
	void setFcPos(int fcPos);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	rtlsdr_dev_t* m_dev;
	SampleVector m_convertBuffer;
	SampleSinkFifo* m_sampleFifo;

	int m_samplerate;
	unsigned int m_log2Decim;
	int m_fcPos;

#ifdef SDR_RX_SAMPLE_24BIT
    DecimatorsU<qint64, quint8, SDR_RX_SAMP_SZ, 8, 127> m_decimators;
#else
	DecimatorsU<qint32, quint8, SDR_RX_SAMP_SZ, 8, 127> m_decimators;
#endif

	void run();
	void callback(const quint8* buf, qint32 len);

	static void callbackHelper(unsigned char* buf, uint32_t len, void* ctx);
};

#endif // INCLUDE_RTLSDRTHREAD_H
