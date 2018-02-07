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

#ifndef PLUGINS_SAMPLESOURCE_PERSEUS_PERSEUSTHREAD_H_
#define PLUGINS_SAMPLESOURCE_PERSEUS_PERSEUSTHREAD_H_

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include "perseus-sdr.h"

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"

#define PERSEUS_NBSAMPLES 1024   // Number of I/Q samples in each callback from Perseus
#define PERSEUS_BLOCKSIZE 6*PERSEUS_NBSAMPLES // Perseus sends 2*3 bytes samples

class PerseusThread : public QThread {
	Q_OBJECT

public:
	PerseusThread(perseus_descr* dev, SampleSinkFifo* sampleFifo, QObject* parent = 0);
	~PerseusThread();

	void startWork();
	void stopWork();
	void setSamplerate(uint32_t samplerate);
	void setLog2Decimation(unsigned int log2_decim);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	perseus_descr* m_dev;
	qint32 m_buf[2*PERSEUS_NBSAMPLES];
	SampleVector m_convertBuffer;
	SampleSinkFifo* m_sampleFifo;

	int m_samplerate;
	unsigned int m_log2Decim;
	static PerseusThread *m_this;

	Decimators<qint64, TripleByteLE<qint32>, SDR_RX_SAMP_SZ, 16> m_decimators32;
    Decimators<qint64, TripleByteLE<qint64>, SDR_RX_SAMP_SZ, 16> m_decimators64;

	void run();
	void callback(const uint8_t* buf, qint32 len); // inner call back
	static int rx_callback(void *buf, int buf_size, void *extra); // call back from Perseus
};

#endif /* PLUGINS_SAMPLESOURCE_PERSEUS_PERSEUSTHREAD_H_ */
