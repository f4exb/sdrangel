///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QTimer>
#include <QDebug>

#include "dsp/samplesourcefifo.h"
#include "audio/audiofifo.h"

#include "audiooutputworker.h"

#define AUDIOOUTPUT_THROTTLE_MS 50

AudioOutputWorker::AudioOutputWorker(SampleSourceFifo* sampleFifo, AudioFifo *fifo, QObject* parent) :
    QObject(parent),
    m_running(false),
    m_samplerate(0),
    m_throttlems(AUDIOOUTPUT_THROTTLE_MS),
    m_maxThrottlems(50),
    m_throttleToggle(false),
    m_iqMapping(AudioOutputSettings::IQMapping::LR),
    m_buf(nullptr),
    m_samplesChunkSize(0),
    m_sampleFifo(sampleFifo),
    m_audioFifo(fifo)
{
    m_audioBuffer.resize(1<<14);
    m_audioBufferFill = 0;
}

AudioOutputWorker::~AudioOutputWorker()
{
}

void AudioOutputWorker::startWork()
{
    qDebug("AudioOutputWorker::startWork");
    m_running = true;
}

void AudioOutputWorker::stopWork()
{
    qDebug("AudioOutputWorker::stopWork");
    m_running = false;
}

void AudioOutputWorker::connectTimer(const QTimer& timer)
{
	qDebug() << "AudioOutputWorker::connectTimer";
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void AudioOutputWorker::setSamplerate(int samplerate)
{
	if (samplerate != m_samplerate)
	{
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

	    qDebug() << "AudioOutputWorker::setSamplerate:"
	            << " new:" << samplerate
	            << " old:" << m_samplerate
                << " m_sampleFifo size:" << m_sampleFifo->size()
                << " m_audioFifo size:" << m_audioFifo->size()
                << " sample i/q size" << sizeof(FixReal);

        // resize output buffer
        if (m_buf) {
            delete[] m_buf;
        }

        m_buf = new int16_t[samplerate*2];

        m_samplerate = samplerate;
        m_samplesChunkSize = (m_samplerate * m_throttlems) / 1000;

        if (wasRunning) {
            startWork();
        }
	}
}

void AudioOutputWorker::tick()
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

        if (iPart1Begin != iPart1End) {
            callbackPart(data, iPart1Begin, iPart1End);
        }

        if (iPart2Begin != iPart2End) {
            callbackPart(data, iPart2Begin, iPart2End);
        }

        // qDebug("AudioOutputWorker::tick: %d samples fill: %u", m_samplesChunkSize, m_audioFifo->fill());
    }
}

void AudioOutputWorker::callbackPart(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    for (unsigned int i = iBegin; i < iEnd; i++)
    {
        m_audioBuffer[m_audioBufferFill].l = m_iqMapping == AudioOutputSettings::LR ? data[i].m_real : data[i].m_imag;
        m_audioBuffer[m_audioBufferFill].r = m_iqMapping == AudioOutputSettings::LR ? data[i].m_imag : data[i].m_real;
        m_audioBufferFill++;

        if (m_audioBufferFill >= m_audioBuffer.size())
        {
            uint res = m_audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

            if (res != m_audioBufferFill)
            {
                qDebug("AudioOutputWorker::callbackPart: %u/%u audio samples written", res, m_audioBufferFill);
                m_audioFifo->clear();
            }

            m_audioBufferFill = 0;
        }
    }
}
