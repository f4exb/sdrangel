///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SDRDAEMONSINKTHREAD_H
#define INCLUDE_SDRDAEMONSINKTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <QElapsedTimer>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <stdint.h>

#include "dsp/inthalfbandfilter.h"
#include "dsp/interpolators.h"

#include "udpsinkfec.h"

#define SDRDAEMONSINK_THROTTLE_MS 50

class SampleSourceFifo;

class SDRdaemonSinkThread : public QThread {
	Q_OBJECT

public:
	SDRdaemonSinkThread(SampleSourceFifo* sampleFifo, QObject* parent = 0);
	~SDRdaemonSinkThread();

	void startWork();
	void stopWork();

    void setCenterFrequency(uint64_t centerFrequency) { m_udpSinkFEC.setCenterFrequency(centerFrequency); }
	void setSamplerate(int samplerate);
    void setNbBlocksFEC(uint32_t nbBlocksFEC) { m_udpSinkFEC.setNbBlocksFEC(nbBlocksFEC); };
    void setTxDelay(uint32_t txDelay) { m_udpSinkFEC.setTxDelay(txDelay); };
    void setRemoteAddress(const QString& address, uint16_t port) { m_udpSinkFEC.setRemoteAddress(address, port); }

    bool isRunning() const { return m_running; }

    std::size_t getSamplesCount() const { return m_samplesCount; }
    void setSamplesCount(int samplesCount) { m_samplesCount = samplesCount; }

	void connectTimer(const QTimer& timer);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	unsigned int m_samplesChunkSize;
	SampleSourceFifo* m_sampleFifo;
    std::size_t m_samplesCount;

	int m_samplerate;
    int m_throttlems;
    int m_maxThrottlems;
    QElapsedTimer m_elapsedTimer;
    bool m_throttleToggle;

    UDPSinkFEC m_udpSinkFEC;

	void run();

private slots:
	void tick();
};

#endif // INCLUDE_SDRDAEMONSINKTHREAD_H
