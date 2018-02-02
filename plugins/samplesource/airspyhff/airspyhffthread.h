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

#ifndef INCLUDE_AIRSPYHFFTHREAD_H
#define INCLUDE_AIRSPYHFFTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <libairspyhf/airspyhf.h>

#include "dsp/samplesinkfifo.h"
#include "dsp/decimatorsf.h"

#define AIRSPYHFF_BLOCKSIZE (1<<17)

class AirspyHFFThread : public QThread {
	Q_OBJECT

public:
	AirspyHFFThread(airspyhf_device_t* dev, SampleSinkFifo* sampleFifo, QObject* parent = 0);
	~AirspyHFFThread();

	void startWork();
	void stopWork();
	void setSamplerate(uint32_t samplerate);
	void setLog2Decimation(unsigned int log2_decim);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	airspyhf_device_t* m_dev;
	qint16 m_buf[2*AIRSPYHFF_BLOCKSIZE];
	SampleVector m_convertBuffer;
	SampleSinkFifo* m_sampleFifo;

	int m_samplerate;
	unsigned int m_log2Decim;
	static AirspyHFFThread *m_this;

	DecimatorsF m_decimators;

	void run();
	void callback(const float* buf, qint32 len);
	static int rx_callback(airspyhf_transfer_t* transfer);
};

#endif // INCLUDE_AIRSPYHFFTHREAD_H
