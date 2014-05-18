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
#include "dsp/samplefifo.h"
#include "dsp/inthalfbandfilter.h"

class RTLSDRThread : public QThread {
	Q_OBJECT

public:
	RTLSDRThread(rtlsdr_dev_t* dev, SampleFifo* sampleFifo, QObject* parent = NULL);
	~RTLSDRThread();

	void startWork();
	void stopWork();

	void setDecimation(int decimation);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	rtlsdr_dev_t* m_dev;
	SampleVector m_convertBuffer;
	SampleFifo* m_sampleFifo;

	int m_decimation;

	IntHalfbandFilter m_decimator2;
	IntHalfbandFilter m_decimator4;
	IntHalfbandFilter m_decimator8;
	IntHalfbandFilter m_decimator16;

	void run();

	void decimate2(SampleVector::iterator* it, const quint8* buf, qint32 len);
	void decimate4(SampleVector::iterator* it, const quint8* buf, qint32 len);
	void decimate8(SampleVector::iterator* it, const quint8* buf, qint32 len);
	void decimate16(SampleVector::iterator* it, const quint8* buf, qint32 len);
	void callback(const quint8* buf, qint32 len);

	static void callbackHelper(unsigned char* buf, uint32_t len, void* ctx);
};

#endif // INCLUDE_RTLSDRTHREAD_H
