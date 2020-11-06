///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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
#include "dsp/basebandsamplesink.h"
#include "testsinkworker.h"

TestSinkWorker::TestSinkWorker(SampleSourceFifo* sampleFifo, QObject* parent) :
	QObject(parent),
	m_running(false),
	m_bufsize(0),
	m_samplesChunkSize(0),
	m_sampleFifo(sampleFifo),
	m_samplesCount(0),
    m_samplerate(0),
    m_log2Interpolation(0),
    m_throttlems(TESTSINK_THROTTLE_MS),
    m_maxThrottlems(50),
    m_throttleToggle(false),
    m_buf(0)
{
}

TestSinkWorker::~TestSinkWorker()
{
	if (m_running) {
		stopWork();
	}

    if (m_buf) delete[] m_buf;
}

void TestSinkWorker::startWork()
{
	qDebug() << "TestSinkWorker::startWork: ";
    m_maxThrottlems = 0;
    m_elapsedTimer.start();
    m_running = true;
}

void TestSinkWorker::stopWork()
{
	qDebug() << "TestSinkWorker::stopWork";
	m_running = false;
}

void TestSinkWorker::setSamplerate(int samplerate)
{
	if (samplerate != m_samplerate)
	{
	    qDebug() << "TestSinkWorker::setSamplerate:"
	            << " new:" << samplerate
	            << " old:" << m_samplerate;

	    bool wasRunning = false;

		if (m_running)
		{
			stopWork();
			wasRunning = true;
		}

		// resize sample FIFO
		if (m_sampleFifo) {
		    m_sampleFifo->resize(SampleSourceFifo::getSizePolicy(samplerate));
		}

        // resize output buffer
        if (m_buf) delete[] m_buf;
        m_buf = new int16_t[samplerate*(1<<m_log2Interpolation)*2];

        m_samplerate = samplerate;
        m_samplesChunkSize = (m_samplerate * m_throttlems) / 1000;

        if (wasRunning) {
            startWork();
        }
	}
}

void TestSinkWorker::setLog2Interpolation(int log2Interpolation)
{
    if ((log2Interpolation < 0) || (log2Interpolation > 6)) {
        return;
    }

    if (log2Interpolation != m_log2Interpolation)
    {
        qDebug() << "TestSinkWorker::setLog2Interpolation:"
                << " new:" << log2Interpolation
                << " old:" << m_log2Interpolation;

        bool wasRunning = false;

        if (m_running)
        {
            stopWork();
            wasRunning = true;
        }

        // resize output buffer
        if (m_buf) delete[] m_buf;
        m_buf = new int16_t[m_samplerate*(1<<log2Interpolation)*2];

        m_log2Interpolation = log2Interpolation;

        if (wasRunning) {
            startWork();
        }
    }
}

void TestSinkWorker::connectTimer(const QTimer& timer)
{
	qDebug() << "TestSinkWorker::connectTimer";
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void TestSinkWorker::tick()
{
	if (m_running)
	{
        qint64 throttlems = m_elapsedTimer.restart();

        if (throttlems != m_throttlems)
        {
            m_throttlems = throttlems;
            m_samplesChunkSize = (m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000;
            m_throttleToggle = !m_throttleToggle;
        }

        unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
        SampleVector& data = m_sampleFifo->getData();
        m_sampleFifo->read(m_samplesChunkSize, iPart1Begin, iPart1End, iPart2Begin, iPart2End);
        m_samplesCount += m_samplesChunkSize;

        if (iPart1Begin != iPart1End) {
            callbackPart(data, iPart1Begin, iPart1End);
        }

        if (iPart2Begin != iPart2End) {
            callbackPart(data, iPart2Begin, iPart2End);
        }

	}
}

void TestSinkWorker::callbackPart(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    SampleVector::iterator beginRead = data.begin() + iBegin;
    unsigned int chunkSize = iEnd - iBegin;

    if (m_log2Interpolation == 0)
    {
        m_interpolators.interpolate1(&beginRead, m_buf, 2*chunkSize);
        feedSpectrum(m_buf, 2*chunkSize);
    }
    else
    {

        switch (m_log2Interpolation)
        {
        case 1:
            m_interpolators.interpolate2_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interpolation)*2);
            break;
        case 2:
            m_interpolators.interpolate4_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interpolation)*2);
            break;
        case 3:
            m_interpolators.interpolate8_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interpolation)*2);
            break;
        case 4:
            m_interpolators.interpolate16_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interpolation)*2);
            break;
        case 5:
            m_interpolators.interpolate32_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interpolation)*2);
            break;
        case 6:
            m_interpolators.interpolate64_cen(&beginRead, m_buf, chunkSize*(1<<m_log2Interpolation)*2);
            break;
        default:
            break;
        }

        feedSpectrum(m_buf, 2*chunkSize*(1<<m_log2Interpolation));
    }
}

void TestSinkWorker::feedSpectrum(int16_t *buf, unsigned int bufSize)
{
    if (!m_spectrumSink) {
        return;
    }

    m_samplesVector.allocate(bufSize/2);
    Sample16 *s16Buf = (Sample16*) buf;

    std::transform(
        s16Buf,
        s16Buf + (bufSize/2),
        m_samplesVector.m_vector.begin(),
        [](Sample16 s) -> Sample {
            return Sample{s.m_real, s.m_imag};
        }
    );

    m_spectrumSink->feed(m_samplesVector.m_vector.begin(), m_samplesVector.m_vector.begin() + (bufSize/2), false);
}
