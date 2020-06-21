///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_FCDTHREAD_H
#define INCLUDE_FCDTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "dsp/samplesinkfifo.h"
#include "dsp/decimators.h"
#include "fcdtraits.h"

class AudioFifo;

class FCDProPlusThread : public QThread {
	Q_OBJECT

public:
	FCDProPlusThread(SampleSinkFifo* sampleFifo, AudioFifo *fcdFIFO, QObject* parent = nullptr);
	~FCDProPlusThread();

	void startWork();
	void stopWork();
	void setLog2Decimation(unsigned int log2_decim);
	void setFcPos(int fcPos);
    void setIQOrder(bool iqOrder) { m_iqOrder = iqOrder; }

private:
	AudioFifo* m_fcdFIFO;

	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;
	unsigned int m_log2Decim;
	int m_fcPos;
    bool m_iqOrder;

	qint16 m_buf[fcd_traits<ProPlus>::convBufSize*2]; // stereo (I, Q)
	SampleVector m_convertBuffer;
	SampleSinkFifo* m_sampleFifo;
	Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, true> m_decimatorsIQ;
	Decimators<qint32, qint16, SDR_RX_SAMP_SZ, 16, false> m_decimatorsQI;

	void run();
	void workIQ(unsigned int n_items);
	void workQI(unsigned int n_items);
};
#endif // INCLUDE_FCDTHREAD_H
