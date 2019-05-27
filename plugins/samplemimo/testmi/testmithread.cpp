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

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <errno.h>

#include "dsp/samplesinkfifo.h"

#include "testmithread.h"

#define TESTMI_BLOCKSIZE 16384

MESSAGE_CLASS_DEFINITION(TestMIThread::MsgStartStop, Message)

TestMIThread::TestMIThread(SampleSinkFifo* sampleFifo, int streamIndex, QObject* parent) :
	QThread(parent),
	m_running(false),
    m_buf(0),
    m_bufsize(0),
    m_chunksize(0),
	m_convertBuffer(TESTMI_BLOCKSIZE),
	m_sampleFifo(sampleFifo),
    m_streamIndex(streamIndex),
	m_frequencyShift(0),
	m_toneFrequency(440),
	m_modulation(TestMIStreamSettings::ModulationNone),
	m_amModulation(0.5f),
	m_fmDeviationUnit(0.0f),
	m_fmPhasor(0.0f),
    m_pulseWidth(150),
    m_pulseSampleCount(0),
    m_pulsePatternCount(0),
    m_pulsePatternCycle(8),
    m_pulsePatternPlaces(3),
	m_samplerate(48000),
	m_log2Decim(4),
	m_fcPos(0),
	m_bitSizeIndex(0),
	m_bitShift(8),
	m_amplitudeBits(127),
	m_dcBias(0.0f),
	m_iBias(0.0f),
	m_qBias(0.0f),
	m_phaseImbalance(0.0f),
	m_amplitudeBitsDC(0),
	m_amplitudeBitsI(127),
	m_amplitudeBitsQ(127),
	m_frequency(435*1000),
	m_fcPosShift(0),
    m_throttlems(TESTMI_THROTTLE_MS),
    m_throttleToggle(false),
    m_mutex(QMutex::Recursive)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

TestMIThread::~TestMIThread()
{
}

void TestMIThread::startWork()
{
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    m_timer.start(50);
	m_startWaitMutex.lock();
	m_elapsedTimer.start();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void TestMIThread::stopWork()
{
	m_running = false;
	wait();
    m_timer.stop();
    disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void TestMIThread::setSamplerate(int samplerate)
{
    QMutexLocker mutexLocker(&m_mutex);

	m_samplerate = samplerate;
    m_chunksize = 4 * ((m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000);
    m_throttleToggle = !m_throttleToggle;
	m_nco.setFreq(m_frequencyShift, m_samplerate);
	m_toneNco.setFreq(m_toneFrequency, m_samplerate);
}

void TestMIThread::setLog2Decimation(unsigned int log2_decim)
{
	m_log2Decim = log2_decim;
}

void TestMIThread::setFcPos(int fcPos)
{
	m_fcPos = fcPos;
}

void TestMIThread::setBitSize(quint32 bitSizeIndex)
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

void TestMIThread::setAmplitudeBits(int32_t amplitudeBits)
{
    m_amplitudeBits = amplitudeBits;
    m_amplitudeBitsDC = m_dcBias * amplitudeBits;
    m_amplitudeBitsI = (1.0f + m_iBias) * amplitudeBits;
    m_amplitudeBitsQ = (1.0f + m_qBias) * amplitudeBits;
}

void TestMIThread::setDCFactor(float dcFactor)
{
    m_dcBias = dcFactor;
    m_amplitudeBitsDC = m_dcBias * m_amplitudeBits;
}

void TestMIThread::setIFactor(float iFactor)
{
    m_iBias = iFactor;
    m_amplitudeBitsI = (1.0f + m_iBias) * m_amplitudeBits;
}

void TestMIThread::setQFactor(float iFactor)
{
    m_qBias = iFactor;
    m_amplitudeBitsQ = (1.0f + m_qBias) * m_amplitudeBits;
}

void TestMIThread::setPhaseImbalance(float phaseImbalance)
{
    m_phaseImbalance = phaseImbalance;
}

void TestMIThread::setFrequencyShift(int shift)
{
    m_nco.setFreq(shift, m_samplerate);
}

void TestMIThread::setToneFrequency(int toneFrequency)
{
    m_toneNco.setFreq(toneFrequency, m_samplerate);
}

void TestMIThread::setModulation(TestMIStreamSettings::Modulation modulation)
{
    m_modulation = modulation;
}

void TestMIThread::setAMModulation(float amModulation)
{
    m_amModulation = amModulation < 0.0f ? 0.0f : amModulation > 1.0f ? 1.0f : amModulation;
}

void TestMIThread::setFMDeviation(float deviation)
{
    float fmDeviationUnit = deviation / (float) m_samplerate;
    m_fmDeviationUnit = fmDeviationUnit < 0.0f ? 0.0f : fmDeviationUnit > 0.5f ? 0.5f : fmDeviationUnit;
    qDebug("TestMIThread::setFMDeviation: m_fmDeviationUnit: %f", m_fmDeviationUnit);
}

void TestMIThread::startStop(bool start)
{
    MsgStartStop *msg = MsgStartStop::create(start);
    m_inputMessageQueue.push(msg);
}

void TestMIThread::run()
{
    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running) // actual work is in the tick() function
    {
        sleep(1);
    }

    m_running = false;
}

void TestMIThread::setBuffers(quint32 chunksize)
{
    if (chunksize > m_bufsize)
    {
        m_bufsize = chunksize;

        if (m_buf == 0)
        {
            qDebug() << "TestMIThread::setBuffer: Allocate buffer:    "
                    << " size: " << m_bufsize << " bytes"
                    << " #samples: " << (m_bufsize/4);
            m_buf = (qint16*) malloc(m_bufsize);
        }
        else
        {
            qDebug() << "TestMIThread::setBuffer: Re-allocate buffer: "
                    << " size: " << m_bufsize << " bytes"
                    << " #samples: " << (m_bufsize/4);
            free(m_buf);
            m_buf = (qint16*) malloc(m_bufsize);
        }

        m_convertBuffer.resize(chunksize/4);
    }
}

void TestMIThread::generate(quint32 chunksize)
{
    int n = chunksize / 2;
    setBuffers(chunksize);

    for (int i = 0; i < n-1;)
    {
        switch (m_modulation)
        {
        case TestMIStreamSettings::ModulationAM:
        {
            Complex c = m_nco.nextIQ();
            Real t, re, im;
            pullAF(t);
            t = (t*m_amModulation + 1.0f)*0.5f;
            re = c.real()*t;
            im = c.imag()*t + m_phaseImbalance*re;
            m_buf[i++] = (int16_t) (re * (float) m_amplitudeBitsI) + m_amplitudeBitsDC;
            m_buf[i++] = (int16_t) (im * (float) m_amplitudeBitsQ);
        }
        break;
        case TestMIStreamSettings::ModulationFM:
        {
            Complex c = m_nco.nextIQ();
            Real t, re, im;
            pullAF(t);
            m_fmPhasor += m_fmDeviationUnit * t;
            m_fmPhasor = m_fmPhasor < -1.0f ? -m_fmPhasor - 1.0f  : m_fmPhasor > 1.0f ? m_fmPhasor - 1.0f : m_fmPhasor;
            re =  c.real()*cos(m_fmPhasor*M_PI) - c.imag()*sin(m_fmPhasor*M_PI);
            im = (c.real()*sin(m_fmPhasor*M_PI) + c.imag()*cos(m_fmPhasor*M_PI)) + m_phaseImbalance*re;
            m_buf[i++] = (int16_t) (re * (float) m_amplitudeBitsI) + m_amplitudeBitsDC;
            m_buf[i++] = (int16_t) (im * (float) m_amplitudeBitsQ);
        }
        break;
        case TestMIStreamSettings::ModulationPattern0: // binary pattern
        {
            if (m_pulseSampleCount < m_pulseWidth) // sync pattern: 0
            {
                m_buf[i++] = m_amplitudeBitsDC;
                m_buf[i++] = 0;
            }
            else if (m_pulseSampleCount < 2*m_pulseWidth) // sync pattern: 1
            {
                m_buf[i++] = (int16_t) (m_amplitudeBitsI + m_amplitudeBitsDC);
                m_buf[i++] = (int16_t) (m_phaseImbalance * (float) m_amplitudeBitsQ);
            }
            else if (m_pulseSampleCount < 3*m_pulseWidth) // sync pattern: 0
            {
                m_buf[i++] = m_amplitudeBitsDC;
                m_buf[i++] = 0;
            }
            else if (m_pulseSampleCount < (3+m_pulsePatternPlaces)*m_pulseWidth) // binary pattern
            {
                uint32_t patPulseSampleCount = m_pulseSampleCount - 3*m_pulseWidth;
                uint32_t patPulseIndex = patPulseSampleCount / m_pulseWidth;
                float patFigure = (m_pulsePatternCount & (1<<patPulseIndex)) != 0 ? 0.3 : 0.0; // make binary pattern ~-10dB vs sync pattern
                m_buf[i++] = (int16_t) (patFigure * (float) m_amplitudeBitsI) + m_amplitudeBitsDC;
                m_buf[i++] = (int16_t) (patFigure * (float) m_phaseImbalance * m_amplitudeBitsQ);
            }

            if (m_pulseSampleCount < (4+m_pulsePatternPlaces)*m_pulseWidth - 1)
            {
                m_pulseSampleCount++;
            }
            else
            {
                if (m_pulsePatternCount < m_pulsePatternCycle - 1) {
                    m_pulsePatternCount++;
                } else {
                    m_pulsePatternCount = 0;
                }

                m_pulseSampleCount = 0;
            }
        }
        break;
        case TestMIStreamSettings::ModulationPattern1: // sawtooth pattern
        {
            Real re, im;
            re = (float) (m_pulseWidth - m_pulseSampleCount) / (float) m_pulseWidth;
            im = m_phaseImbalance*re;
            m_buf[i++] = (int16_t) (re * (float) m_amplitudeBitsI) + m_amplitudeBitsDC;
            m_buf[i++] = (int16_t) (im * (float) m_amplitudeBitsQ);

            if (m_pulseSampleCount < m_pulseWidth - 1) {
                m_pulseSampleCount++;
            } else {
                m_pulseSampleCount = 0;
            }
        }
        break;
        case TestMIStreamSettings::ModulationPattern2: // 50% duty cycle square pattern
        {
            if (m_pulseSampleCount < m_pulseWidth) // 1
            {
                m_buf[i++] = (int16_t) (m_amplitudeBitsI + m_amplitudeBitsDC);
                m_buf[i++] = (int16_t) (m_phaseImbalance * (float) m_amplitudeBitsQ);
            } else { // 0
                m_buf[i++] = m_amplitudeBitsDC;
                m_buf[i++] = 0;
            }

            if (m_pulseSampleCount < 2*m_pulseWidth - 1) {
                m_pulseSampleCount++;
            } else {
                m_pulseSampleCount = 0;
            }
        }
        break;
        case TestMIStreamSettings::ModulationNone:
        default:
        {
            Complex c = m_nco.nextIQ(m_phaseImbalance);
            m_buf[i++] = (int16_t) (c.real() * (float) m_amplitudeBitsI) + m_amplitudeBitsDC;
            m_buf[i++] = (int16_t) (c.imag() * (float) m_amplitudeBitsQ);
        }
        break;
        }
    }

    callback(m_buf, n);
}

void TestMIThread::pullAF(Real& afSample)
{
    afSample = m_toneNco.next();
}

//  call appropriate conversion (decimation) routine depending on the number of sample bits
void TestMIThread::callback(const qint16* buf, qint32 len)
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

void TestMIThread::tick()
{
    if (m_running)
    {
        qint64 throttlems = m_elapsedTimer.restart();

        if ((throttlems > 45) && (throttlems < 55) && (throttlems != m_throttlems))
        {
            QMutexLocker mutexLocker(&m_mutex);
            m_throttlems = throttlems;
            m_chunksize = 4 * ((m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000);
            m_throttleToggle = !m_throttleToggle;
        }

        generate(m_chunksize);
    }
}

void TestMIThread::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgStartStop::match(*message))
        {
            MsgStartStop* notif = (MsgStartStop*) message;
            qDebug("TestMIThread::handleInputMessages: MsgStartStop: %s", notif->getStartStop() ? "start" : "stop");

            if (notif->getStartStop()) {
                startWork();
            } else {
                stopWork();
            }

            delete message;
        }
    }
}

void TestMIThread::setPattern0()
{
    m_pulseWidth = 150;
    m_pulseSampleCount = 0;
    m_pulsePatternCount = 0;
    m_pulsePatternCycle = 8;
    m_pulsePatternPlaces = 3;
}

void TestMIThread::setPattern1()
{
    m_pulseWidth = 1000;
    m_pulseSampleCount = 0;
}

void TestMIThread::setPattern2()
{
    m_pulseWidth = 1000;
    m_pulseSampleCount = 0;
}
