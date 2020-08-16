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

#ifndef INCLUDE_AIRSPYHFWORKER_H
#define INCLUDE_AIRSPYHFWORKER_H

#include <dsp/decimatorsfi.h>
#include <QObject>
#include <libairspyhf/airspyhf.h>

#include "dsp/samplesinkfifo.h"

#define AIRSPYHF_BLOCKSIZE (1<<17)

class AirspyHFWorker : public QObject {
	Q_OBJECT

public:
	AirspyHFWorker(airspyhf_device_t* dev, SampleSinkFifo* sampleFifo, QObject* parent = 0);
	~AirspyHFWorker();

	bool startWork();
	void stopWork();
	void setSamplerate(uint32_t samplerate);
	void setLog2Decimation(unsigned int log2_decim);
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }

private:
	bool m_running;

	airspyhf_device_t* m_dev;
	qint16 m_buf[2*AIRSPYHF_BLOCKSIZE];
	SampleVector m_convertBuffer;
	SampleSinkFifo* m_sampleFifo;

	int m_samplerate;
	unsigned int m_log2Decim;
    bool m_iqOrder;

	DecimatorsFI<true> m_decimatorsIQ;
	DecimatorsFI<false> m_decimatorsQI;

	void callbackIQ(const float* buf, qint32 len);
	void callbackQI(const float* buf, qint32 len);
	static int rx_callback(airspyhf_transfer_t* transfer);
};

#endif // INCLUDE_AIRSPYHFWORKER_H
