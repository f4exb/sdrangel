///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_REMOTEOUTPUTTHREAD_H
#define INCLUDE_REMOTEOUTPUTTHREAD_H

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <stdint.h>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <QElapsedTimer>

#include "dsp/inthalfbandfilter.h"
#include "dsp/interpolators.h"

#include "udpsinkfec.h"

#define REMOTEOUTPUT_THROTTLE_MS 50

class SampleSourceFifoDB;
struct timeval;

class RemoteOutputThread : public QThread {
	Q_OBJECT

public:
	RemoteOutputThread(SampleSourceFifoDB* sampleFifo, QObject* parent = 0);
	~RemoteOutputThread();

	void startWork();
	void stopWork();

	void setSamplerate(int samplerate);
    void setNbBlocksFEC(uint32_t nbBlocksFEC) { m_udpSinkFEC.setNbBlocksFEC(nbBlocksFEC); };
    void setTxDelay(float txDelay) { m_udpSinkFEC.setTxDelay(txDelay); };
    void setDataAddress(const QString& address, uint16_t port) { m_udpSinkFEC.setRemoteAddress(address, port); }

    bool isRunning() const { return m_running; }

    uint32_t getSamplesCount(uint64_t& ts_usecs) const;
    void setSamplesCount(int samplesCount) { m_samplesCount = samplesCount; }
    void setChunkCorrection(int chunkCorrection) { m_chunkCorrection = chunkCorrection; }

	void connectTimer(const QTimer& timer);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	volatile bool m_running;

	int m_samplesChunkSize;
	SampleSourceFifoDB* m_sampleFifo;
    uint32_t m_samplesCount;
    int m_chunkCorrection;

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

#endif // INCLUDE_REMOTEOUTPUTTHREAD_H
