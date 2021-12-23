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

#ifndef INCLUDE_REMOTEOUTPUTWORKER_H
#define INCLUDE_REMOTEOUTPUTWORKER_H

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <stdint.h>

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>

#include "dsp/inthalfbandfilter.h"
#include "dsp/interpolators.h"

#include "udpsinkfec.h"

#define REMOTEOUTPUT_THROTTLE_MS 50

class SampleSourceFifo;
struct timeval;

class RemoteOutputWorker : public QObject {
	Q_OBJECT

public:
	RemoteOutputWorker(SampleSourceFifo* sampleFifo, QObject* parent = 0);
	~RemoteOutputWorker();

	void startWork();
	void stopWork();

    void setDeviceIndex(uint32_t deviceIndex) { m_udpSinkFEC.setDeviceIndex(deviceIndex); }
	void setSamplerate(int samplerate);
    void setNbBlocksFEC(uint32_t nbBlocksFEC) { m_udpSinkFEC.setNbBlocksFEC(nbBlocksFEC); };
    void setNbTxBytes(uint32_t nbTxBytes) { m_udpSinkFEC.setNbTxBytes(nbTxBytes); };
    void setDataAddress(const QString& address, uint16_t port) { m_udpSinkFEC.setRemoteAddress(address, port); }

    bool isRunning() const { return m_running; }

    uint32_t getSamplesCount() const { return m_samplesCount; }
    uint32_t getSamplesCount(uint64_t& ts_usecs) const;
    void setChunkCorrection(int chunkCorrection) { m_chunkCorrection = chunkCorrection; }

	void connectTimer(const QTimer& timer);

private:
	volatile bool m_running;

	int m_samplesChunkSize;
	SampleSourceFifo* m_sampleFifo;
    uint32_t m_samplesCount;
    int m_chunkCorrection;

	int m_samplerate;
    int m_throttlems;
    int m_maxThrottlems;
    QElapsedTimer m_elapsedTimer;
    bool m_throttleToggle;

    UDPSinkFEC m_udpSinkFEC;

private slots:
	void tick();
};

#endif // INCLUDE_REMOTEOUTPUTTHREAD_H
