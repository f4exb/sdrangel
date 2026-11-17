///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016, 2019-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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
#include <cmath>

#include "sdrplaythread.h"
#include "dsp/samplesinkfifo.h"

SDRPlayThread::SDRPlayThread(mirisdr_dev_t* dev, SampleSinkFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_startState(StartState::Stopped),
    m_running(false),
    m_dev(dev),
    m_convertBuffer(SDRPLAY_INIT_NBSAMPLES),
    m_floatConvertBuffer(SDRPLAY_INIT_NBSAMPLES),
    m_sampleFifo(sampleFifo),
    m_samplerate(288000),
    m_log2Decim(0),
    m_fcPos(0),
    m_iqOrder(true)
{
    setObjectName("SDRPlay");
}

SDRPlayThread::~SDRPlayThread()
{
    stopWork();
}

bool SDRPlayThread::startWork()
{
    QMutexLocker locker(&m_startWaitMutex);
    m_startState = StartState::WaitingForSamples;
    m_running.store(true);
    start();

    while (m_startState == StartState::WaitingForSamples)
    {
        if (!m_startWaiter.wait(&m_startWaitMutex, 5000))
        {
            qCritical("SDRPlayThread::startWork: timed out waiting for the first sample buffer");
            m_startState = StartState::Failed;
            m_running.store(false);
            locker.unlock();
            mirisdr_cancel_async(m_dev);
            wait();
            return false;
        }
    }

    const bool started = m_startState == StartState::Running;
    locker.unlock();

    if (!started) {
        wait();
    }

    return started;
}

void SDRPlayThread::stopWork()
{
    m_running.store(false);
    if (isRunning()) {
        mirisdr_cancel_async(m_dev);
    }
    wait();

    QMutexLocker locker(&m_startWaitMutex);
    m_startState = StartState::Stopped;
    m_startWaiter.wakeAll();
}

void SDRPlayThread::setSamplerate(int samplerate)
{
    m_samplerate = samplerate;
}

void SDRPlayThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

void SDRPlayThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

void SDRPlayThread::run()
{
    int res;

    bool readError = false;

    while (m_running.load())
    {
        if ((res = mirisdr_read_async(m_dev, &SDRPlayThread::callbackHelper, this, 32, SDRPLAY_INIT_NBSAMPLES)) < 0)
        {
            if (m_running.load()) {
                qCritical("SDRPlayThread::run: async read error: rc %d: %s", res, strerror(errno));
                readError = true;
            }
            break;
        }
    }

    m_running.store(false);

    QMutexLocker locker(&m_startWaitMutex);
    if (readError || m_startState == StartState::WaitingForSamples) {
        m_startState = StartState::Failed;
    } else if (m_startState != StartState::Failed) {
        m_startState = StartState::Stopped;
    }
    m_startWaiter.wakeAll();
}

void SDRPlayThread::reportFirstSamples()
{
    QMutexLocker locker(&m_startWaitMutex);
    if (m_startState == StartState::WaitingForSamples)
    {
        m_startState = StartState::Running;
        m_startWaiter.wakeAll();
    }
}

void SDRPlayThread::callbackHelper(unsigned char* buf, uint32_t len, void* ctx)
{
    SDRPlayThread* thread = (SDRPlayThread*) ctx;
    thread->reportFirstSamples();
    mirisdr_buffer_info_t info;
    const qint16 *samples = reinterpret_cast<const qint16*>(buf);
    qint32 sampleWords = len / sizeof(qint16);

    if (mirisdr_get_buffer_info(thread->m_dev, &info) == 0 && info.type == MIRISDR_SAMPLE_F32)
    {
        const float *floatSamples = reinterpret_cast<const float*>(buf);
        sampleWords = len / sizeof(float);

        if (thread->m_floatConvertBuffer.size() < static_cast<size_t>(sampleWords)) {
            thread->m_floatConvertBuffer.resize(sampleWords);
        }

        for (qint32 i = 0; i < sampleWords; ++i) {
            const float sample = std::max(-1.0f, std::min(1.0f, floatSamples[i]));
            thread->m_floatConvertBuffer[i] = static_cast<qint16>(std::lrint(sample * 32767.0f));
        }

        samples = thread->m_floatConvertBuffer.data();
    }

    if (thread->m_iqOrder) {
        thread->callbackIQ(samples, sampleWords);
    } else {
        thread->callbackQI(samples, sampleWords);
    }

}

void SDRPlayThread::callbackIQ(const qint16* buf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    if (m_log2Decim == 0)
    {
        m_decimatorsIQ.decimate1(&it, buf, len);
    }
    else
    {
        if (m_fcPos == 0) // Infradyne
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsIQ.decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_decimatorsIQ.decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_decimatorsIQ.decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_decimatorsIQ.decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_decimatorsIQ.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_decimatorsIQ.decimate64_inf(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_fcPos == 1) // Supradyne
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsIQ.decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_decimatorsIQ.decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_decimatorsIQ.decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_decimatorsIQ.decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_decimatorsIQ.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_decimatorsIQ.decimate64_sup(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else // Centered
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsIQ.decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_decimatorsIQ.decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_decimatorsIQ.decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_decimatorsIQ.decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_decimatorsIQ.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_decimatorsIQ.decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    m_sampleFifo->write(m_convertBuffer.begin(), it);

    if(!m_running.load())
    {
        mirisdr_cancel_async(m_dev);
    }
}

void SDRPlayThread::callbackQI(const qint16* buf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    if (m_log2Decim == 0)
    {
        m_decimatorsQI.decimate1(&it, buf, len);
    }
    else
    {
        if (m_fcPos == 0) // Infradyne
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsQI.decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_decimatorsQI.decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_decimatorsQI.decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_decimatorsQI.decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_decimatorsQI.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_decimatorsQI.decimate64_inf(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_fcPos == 1) // Supradyne
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsQI.decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_decimatorsQI.decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_decimatorsQI.decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_decimatorsQI.decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_decimatorsQI.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_decimatorsQI.decimate64_sup(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else // Centered
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsQI.decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_decimatorsQI.decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_decimatorsQI.decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_decimatorsQI.decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_decimatorsQI.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_decimatorsQI.decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    m_sampleFifo->write(m_convertBuffer.begin(), it);

    if(!m_running.load())
    {
        mirisdr_cancel_async(m_dev);
    }
}
