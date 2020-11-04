///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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
#include "fileoutputworker.h"

FileOutputWorker::FileOutputWorker(std::ofstream *samplesStream, SampleSourceFifo* sampleFifo, QObject* parent) :
	QObject(parent),
	m_running(false),
	m_ofstream(samplesStream),
	m_bufsize(0),
	m_samplesChunkSize(0),
	m_sampleFifo(sampleFifo),
	m_samplesCount(0),
    m_samplerate(0),
    m_log2Interpolation(0),
    m_throttlems(FILEOUTPUT_THROTTLE_MS),
    m_maxThrottlems(50),
    m_throttleToggle(false),
    m_buf(nullptr)
{
}

FileOutputWorker::~FileOutputWorker()
{
	if (m_running) {
		stopWork();
	}

    if (m_buf) delete[] m_buf;
}

void FileOutputWorker::startWork()
{
	qDebug() << "FileOutputWorker::startWork: ";

    if (m_ofstream->is_open())
    {
        qDebug() << "FileOutputWorker::startWork: file stream open, starting...";
        m_maxThrottlems = 0;
        m_elapsedTimer.start();
        m_running = true;
    }
    else
    {
        qDebug() << "FileOutputWorker::startWork: file stream closed, not starting.";
        m_running = false;
    }
}

void FileOutputWorker::stopWork()
{
	m_running = false;
}

void FileOutputWorker::setSamplerate(int samplerate)
{
	if (samplerate != m_samplerate)
	{
	    qDebug() << "FileOutputWorker::setSamplerate:"
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
		    m_sampleFifo->resize(SampleSourceFifo::getSizePolicy(samplerate)); // 1s buffer
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

void FileOutputWorker::setLog2Interpolation(int log2Interpolation)
{
    if ((log2Interpolation < 0) || (log2Interpolation > 6))
    {
        return;
    }

    if (log2Interpolation != m_log2Interpolation)
    {
        qDebug() << "FileOutputWorker::setLog2Interpolation:"
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

void FileOutputWorker::connectTimer(const QTimer& timer)
{
	qDebug() << "FileOutputWorker::connectTimer";
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void FileOutputWorker::tick()
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

void FileOutputWorker::callbackPart(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    SampleVector::iterator beginRead = data.begin() + iBegin;
    unsigned int chunkSize = iEnd - iBegin;

    if (m_log2Interpolation == 0)
    {
        m_ofstream->write(reinterpret_cast<char*>(&(*beginRead)), chunkSize*sizeof(Sample));
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

        m_ofstream->write(reinterpret_cast<char*>(m_buf), chunkSize*(1<<m_log2Interpolation)*2*sizeof(int16_t));
    }
}
