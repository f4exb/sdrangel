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

#ifndef INCLUDE_BLADERFTHREAD_H
#define INCLUDE_BLADERFTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <libbladeRF.h>
#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"

#define BLADERF_BLOCKSIZE (1<<14)

class BladerfThread : public QThread {
	Q_OBJECT

public:
    BladerfThread(struct bladerf* dev, SampleSinkFifo* sampleFifo, QObject* parent = NULL);
	~BladerfThread();

	void startWork();
	void stopWork();
	void setLog2Decimation(unsigned int log2_decim);
	void setFcPos(int fcPos);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	struct bladerf* m_dev;
	qint16 m_buf[2*BLADERF_BLOCKSIZE];
	SampleVector m_convertBuffer;
    SampleSinkFifo* m_sampleFifo;

	unsigned int m_log2Decim;
	int m_fcPos;

	Decimators<qint16, SDR_SAMP_SZ, 12> m_decimators;

	void run();
	void callback(const qint16* buf, qint32 len);
};

#endif // INCLUDE_BLADERFTHREAD_H
