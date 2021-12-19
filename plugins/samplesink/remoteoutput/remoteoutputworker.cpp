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

#include <stdio.h>
#include <errno.h>
#include <algorithm>
#include <QDebug>

#include "dsp/samplesourcefifo.h"
#include "util/timeutil.h"
#include "remoteoutputworker.h"

RemoteOutputWorker::RemoteOutputWorker(SampleSourceFifo* sampleFifo, QObject* parent) :
	QObject(parent),
	m_running(false),
	m_samplesChunkSize(0),
	m_sampleFifo(sampleFifo),
	m_samplesCount(0),
	m_chunkCorrection(0),
    m_samplerate(0),
    m_throttlems(REMOTEOUTPUT_THROTTLE_MS),
    m_maxThrottlems(50),
    m_throttleToggle(false)
{
}

RemoteOutputWorker::~RemoteOutputWorker()
{
	if (m_running) {
		stopWork();
	}
}

void RemoteOutputWorker::startWork()
{
	qDebug() << "RemoteOutputWorker::startWork: ";
    m_udpSinkFEC.init();
	m_udpSinkFEC.startSender();
    m_maxThrottlems = 0;
    m_running = true;
}

void RemoteOutputWorker::stopWork()
{
	qDebug() << "RemoteOutputWorker::stopWork";
	m_running = false;
	m_udpSinkFEC.stopSender();
}

void RemoteOutputWorker::setSamplerate(int samplerate)
{
	if (samplerate != m_samplerate)
	{
	    qDebug() << "RemoteOutputWorker::setSamplerate:"
	            << " new:" << samplerate
	            << " old:" << m_samplerate;

	    bool wasRunning = false;

		if (m_running)
		{
			stopWork();
			wasRunning = true;
		}

		// resize sample FIFO
		if (m_sampleFifo)
        {
            unsigned int fifoRate = std::max(
                (unsigned int) samplerate,
                48000U);
            m_sampleFifo->resize(SampleSourceFifo::getSizePolicy(fifoRate));
		}

        m_samplerate = samplerate;
        m_samplesChunkSize = (m_samplerate * m_throttlems) / 1000;
        m_udpSinkFEC.setSampleRate(m_samplerate);

        if (wasRunning) {
            startWork();
        }
	}
}

void RemoteOutputWorker::connectTimer(const QTimer& timer)
{
	qDebug() << "RemoteOutputWorker::connectTimer";
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void RemoteOutputWorker::tick()
{
	if (m_running)
	{
        qint64 throttlems = m_elapsedTimer.restart();

        if (throttlems != m_throttlems)
        {
            m_throttlems = throttlems;
            m_samplesChunkSize = (m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000;
            m_samplesChunkSize = m_samplesChunkSize + m_chunkCorrection > 0 ? m_samplesChunkSize + m_chunkCorrection : m_samplesChunkSize;
            m_throttleToggle = !m_throttleToggle;
        }

        SampleVector::iterator readUntil;

        SampleVector& data = m_sampleFifo->getData();
        unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
        m_sampleFifo->read(m_samplesChunkSize, iPart1Begin, iPart1End, iPart2Begin, iPart2End);
        m_samplesCount += m_samplesChunkSize;

        if (iPart1Begin != iPart1End)
        {
            SampleVector::iterator beginRead = data.begin() + iPart1Begin;
            unsigned int partSize = iPart1End - iPart1Begin;
            m_udpSinkFEC.write(beginRead, partSize, true);
        }

        if (iPart2Begin != iPart2End)
        {
            SampleVector::iterator beginRead = data.begin() + iPart2Begin;
            unsigned int partSize = iPart2End - iPart2Begin;
            m_udpSinkFEC.write(beginRead, partSize, true);
        }
	}
}

uint32_t RemoteOutputWorker::getSamplesCount(uint64_t& ts_usecs) const
{
    ts_usecs = TimeUtil::nowus();
    return m_samplesCount;
}
