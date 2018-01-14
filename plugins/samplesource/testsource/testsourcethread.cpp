///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <stdio.h>
#include <errno.h>
#include "testsourcethread.h"

#include "dsp/samplesinkfifo.h"

#define TESTSOURCE_BLOCKSIZE 16384

TestSourceThread::TestSourceThread(SampleSinkFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
    m_buf(0),
    m_bufsize(0),
    m_chunksize(0),
	m_convertBuffer(TESTSOURCE_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
	m_frequencyShift(0),
	m_samplerate(48000),
	m_log2Decim(4),
	m_fcPos(0),
	m_bitSizeIndex(0),
	m_bitShift(8),
	m_amplitudeBits(127),
	m_dcBias(0.0f),
	m_iBias(0.0f),
	m_qBias(0.0f),
	m_amplitudeBitsDC(0),
	m_amplitudeBitsI(127),
	m_amplitudeBitsQ(127),
	m_frequency(435*1000),
	m_fcPosShift(0),
    m_throttlems(TESTSOURCE_THROTTLE_MS),
    m_throttleToggle(false),
    m_mutex(QMutex::Recursive)
{
}

TestSourceThread::~TestSourceThread()
{
	stopWork();
}

void TestSourceThread::startWork()
{
	m_startWaitMutex.lock();
	m_elapsedTimer.start();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void TestSourceThread::stopWork()
{
	m_running = false;
	wait();
}

void TestSourceThread::setSamplerate(int samplerate)
{
    QMutexLocker mutexLocker(&m_mutex);

	m_samplerate = samplerate;
    m_chunksize = 4 * ((m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000);
    m_throttleToggle = !m_throttleToggle;
	m_nco.setFreq(m_frequencyShift, m_samplerate);
}

void TestSourceThread::setLog2Decimation(unsigned int log2_decim)
{
	m_log2Decim = log2_decim;
}

void TestSourceThread::setFcPos(int fcPos)
{
	m_fcPos = fcPos;
}

void TestSourceThread::setBitSize(quint32 bitSizeIndex)
{
    switch (bitSizeIndex)
    {
    case 0:
        m_bitShift = 7;
        m_bitSizeIndex = 0;
        break;
    case 1:
        m_bitShift = 11;
        m_bitSizeIndex = 1;
        break;
    case 2:
    default:
        m_bitShift = 15;
        m_bitSizeIndex = 2;
        break;
    }
}

void TestSourceThread::setAmplitudeBits(int32_t amplitudeBits)
{
    m_amplitudeBits = amplitudeBits;
    m_amplitudeBitsDC = m_dcBias * amplitudeBits;
    m_amplitudeBitsI = (1.0f + m_iBias) * amplitudeBits;
    m_amplitudeBitsQ = (1.0f + m_qBias) * amplitudeBits;
}

void TestSourceThread::setDCFactor(float dcFactor)
{
    m_dcBias = dcFactor;
    m_amplitudeBitsDC = m_dcBias * m_amplitudeBits;
}

void TestSourceThread::setIFactor(float iFactor)
{
    m_iBias = iFactor;
    m_amplitudeBitsI = (1.0f + m_iBias) * m_amplitudeBits;
}

void TestSourceThread::setQFactor(float iFactor)
{
    m_qBias = iFactor;
    m_amplitudeBitsQ = (1.0f + m_qBias) * m_amplitudeBits;
}

void TestSourceThread::setFrequencyShift(int shift)
{
    m_nco.setFreq(shift, m_samplerate);
}

void TestSourceThread::run()
{
    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running) // actual work is in the tick() function
    {
        sleep(1);
    }

    m_running = false;
}

void TestSourceThread::setBuffers(quint32 chunksize)
{
    if (chunksize > m_bufsize)
    {
        m_bufsize = chunksize;

        if (m_buf == 0)
        {
            qDebug() << "TestSourceThread::setBuffer: Allocate buffer:    "
                    << " size: " << m_bufsize << " bytes"
                    << " #samples: " << (m_bufsize/4);
            m_buf = (qint16*) malloc(m_bufsize);
        }
        else
        {
            qDebug() << "TestSourceThread::setBuffer: Re-allocate buffer: "
                    << " size: " << m_bufsize << " bytes"
                    << " #samples: " << (m_bufsize/4);
            free(m_buf);
            m_buf = (qint16*) malloc(m_bufsize);
        }

        m_convertBuffer.resize(chunksize/4);
    }
}

void TestSourceThread::generate(quint32 chunksize)
{
    int n = chunksize / 2;
    setBuffers(chunksize);

    for (int i = 0; i < n-1;)
    {
        Complex c = m_nco.nextIQ();
        m_buf[i++] = (int16_t) (c.real() * (float) m_amplitudeBitsI) + m_amplitudeBitsDC;
        m_buf[i++] = (int16_t) (c.imag() * (float) m_amplitudeBitsQ);
    }

    callback(m_buf, n);
}

//  call appropriate conversion (decimation) routine depending on the number of sample bits
void TestSourceThread::callback(const qint16* buf, qint32 len)
{
	SampleVector::iterator it = m_convertBuffer.begin();

	switch (m_bitSizeIndex)
	{
	case 0: // 8 bit samples
	    convert_8(&it, buf, len);
	    break;
    case 1: // 12 bit samples
        convert_12(&it, buf, len);
        break;
    case 2: // 16 bit samples
    default:
        convert_16(&it, buf, len);
        break;
	}

	m_sampleFifo->write(m_convertBuffer.begin(), it);
}

void TestSourceThread::connectTimer(const QTimer& timer)
{
    qDebug() << "TestSourceThread::connectTimer";
    connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void TestSourceThread::tick()
{
    if (m_running)
    {
        qint64 throttlems = m_elapsedTimer.restart();

        if (throttlems != m_throttlems)
        {
            QMutexLocker mutexLocker(&m_mutex);
            m_throttlems = throttlems;
            m_chunksize = 4 * ((m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000);
            m_throttleToggle = !m_throttleToggle;
        }

        generate(m_chunksize);
    }
}

