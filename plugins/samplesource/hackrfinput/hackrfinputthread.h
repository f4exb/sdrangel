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

#ifndef INCLUDE_HACKRFINPUTTHREAD_H
#define INCLUDE_HACKRFINPUTTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <libhackrf/hackrf.h>

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"

#define HACKRF_BLOCKSIZE (1<<17)

class HackRFInputThread : public QThread {
	Q_OBJECT

public:
	HackRFInputThread(hackrf_device* dev, SampleSinkFifo* sampleFifo, QObject* parent = NULL);
	~HackRFInputThread();

	void startWork();
	void stopWork();
	void setSamplerate(uint32_t samplerate);
	void setLog2Decimation(unsigned int log2_decim);
	void setFcPos(int fcPos);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	hackrf_device* m_dev;
	qint16 m_buf[2*HACKRF_BLOCKSIZE];
	SampleVector m_convertBuffer;
	SampleSinkFifo* m_sampleFifo;

	int m_samplerate;
	unsigned int m_log2Decim;
	int m_fcPos;

#ifdef SDR_RX_SAMPLE_24BIT
    Decimators<qint64, qint8, SDR_RX_SAMP_SZ, 8> m_decimators;
#else
	Decimators<qint32, qint8, SDR_RX_SAMP_SZ, 8> m_decimators;
#endif

	void run();
	void callback(const qint8* buf, qint32 len);
	static int rx_callback(hackrf_transfer* transfer);
};

#endif // INCLUDE_HACKRFINPUTTHREAD_H
