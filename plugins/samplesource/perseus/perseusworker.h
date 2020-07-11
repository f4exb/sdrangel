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

#ifndef PLUGINS_SAMPLESOURCE_PERSEUS_PERSEUSWORKER_H_
#define PLUGINS_SAMPLESOURCE_PERSEUS_PERSEUSWORKER_H_

#include <QObject>
#include "perseus-sdr.h"

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"

#define PERSEUS_NBSAMPLES 2048   // Number of I/Q samples in each callback from Perseus
#define PERSEUS_BLOCKSIZE 6*PERSEUS_NBSAMPLES // Perseus sends 2*3 bytes samples

class PerseusWorker : public QObject {
	Q_OBJECT

public:
	PerseusWorker(perseus_descr* dev, SampleSinkFifo* sampleFifo, QObject* parent = 0);
	~PerseusWorker();

	void startWork();
	void stopWork();
	void setLog2Decimation(unsigned int log2_decim);
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }

private:
	volatile bool m_running;

	perseus_descr* m_dev;
	qint32 m_buf[2*PERSEUS_NBSAMPLES];
	SampleVector m_convertBuffer;
	SampleSinkFifo* m_sampleFifo;

	unsigned int m_log2Decim;
    bool m_iqOrder;
	static PerseusWorker *m_this;

	Decimators<qint32, TripleByteLE<qint32>, SDR_RX_SAMP_SZ, 24, true> m_decimators32IQ; // for no decimation (accumulator is int32)
    Decimators<qint32, TripleByteLE<qint64>, SDR_RX_SAMP_SZ, 24, true> m_decimators64IQ; // for actual decimation (accumulator is int64)
	Decimators<qint32, TripleByteLE<qint32>, SDR_RX_SAMP_SZ, 24, false> m_decimators32QI; // for no decimation (accumulator is int32)
    Decimators<qint32, TripleByteLE<qint64>, SDR_RX_SAMP_SZ, 24, false> m_decimators64QI; // for actual decimation (accumulator is int64)

	void callbackIQ(const uint8_t* buf, qint32 len); // inner call back
    void callbackQI(const uint8_t* buf, qint32 len);
	static int rx_callback(void *buf, int buf_size, void *extra); // call back from Perseus
};

#endif /* PLUGINS_SAMPLESOURCE_PERSEUS_PERSEUSWORKER_H_ */
