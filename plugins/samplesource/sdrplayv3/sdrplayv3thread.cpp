///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include <chrono>
#include <thread>
#include "sdrplayv3thread.h"
#include "dsp/samplesinkfifo.h"
#include "util/poweroftwo.h"

#include <QDebug>

SDRPlayV3Thread::SDRPlayV3Thread(sdrplay_api_DeviceT* dev, SampleSinkFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_convertBuffer(SDRPLAYV3_INIT_NBSAMPLES),
    m_sampleFifo(sampleFifo),
    m_samplerate(2000000),
    m_log2Decim(0),
    m_fcPos(0),
    m_iqOrder(true),
    m_iqCount(0)
{
}

SDRPlayV3Thread::~SDRPlayV3Thread()
{
    stopWork();
}

void SDRPlayV3Thread::startWork()
{
    sdrplay_api_ErrT err;
    sdrplay_api_CallbackFnsT cbFns;

    cbFns.StreamACbFn = &SDRPlayV3Thread::callbackHelper;
    cbFns.StreamBCbFn = &SDRPlayV3Thread::callbackHelper;
    cbFns.EventCbFn = &SDRPlayV3Thread::eventCallback;

    if ((err = sdrplay_api_Init(m_dev->dev, &cbFns, this)) != sdrplay_api_Success)
    {
        qCritical() << "SDRPlayV3Thread::run: sdrplay_api_Init error: " << sdrplay_api_GetErrorString(err);
        m_running = false;
        return;
    }

    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void SDRPlayV3Thread::stopWork()
{
    sdrplay_api_ErrT err;

    if (m_running)
    {
        m_running = false;

        if ((err = sdrplay_api_Uninit(m_dev->dev)) != sdrplay_api_Success) {
            qWarning() << "SDRPlayV3Thread::callbackHelper: sdrplay_api_Uninit error: " << sdrplay_api_GetErrorString(err);
        }
    }

    wait();
}

void SDRPlayV3Thread::setSamplerate(int samplerate)
{
    m_samplerate = samplerate;
}

void SDRPlayV3Thread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

void SDRPlayV3Thread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

void SDRPlayV3Thread::resetRfChanged()
{
    m_rfChanged = 0;
}

bool SDRPlayV3Thread::waitForRfChanged()
{
    for (unsigned int i = 0; i < m_rfChangedTimeout && m_rfChanged == 0; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return m_rfChanged != 0;
}

// Don't really need a thread here - just using same structure as other plugins
void SDRPlayV3Thread::run()
{
    m_running = true;
    m_startWaiter.wakeAll();
}

void SDRPlayV3Thread::eventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext)
{
    // Could possibly report params->gainParams.currGain for eventId == sdrplay_api_GainChange
    // or indicate ADC overload for eventId ==  sdrplay_api_PowerOverloadChange
    (void) eventId;
    (void) tuner;
    (void) params;
    (void) cbContext;
}

void SDRPlayV3Thread::callbackHelper(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *ctx)
{
    (void) params;
    (void) reset;
    SDRPlayV3Thread* thread = (SDRPlayV3Thread*) ctx;

    if (params->rfChanged)
        thread->m_rfChanged = params->rfChanged;

    if (thread->m_running)
    {
        // Interleave samples
        for (int i = 0; i < (int)numSamples; i++)
        {
            thread->m_iq[thread->m_iqCount+i*2] = xi[i];
            thread->m_iq[thread->m_iqCount+i*2+1] = xq[i];
        }
        thread->m_iqCount += numSamples * 2;

        if (thread->m_iqCount > 8192) {
            qCritical() << "SDRPlayV3Thread::callbackHelper: IQ buffer too small: " << numSamples;
        }

        // Decimators require length to be a power of 2
        int iqLen = lowerPowerOfTwo(thread->m_iqCount);

        if (thread->m_iqOrder) {
            thread->callbackIQ(thread->m_iq, iqLen);
        } else {
            thread->callbackQI(thread->m_iq, iqLen);
        }

        // Shuffle buffer up
        int iqRemaining = thread->m_iqCount - iqLen;
        memmove(thread->m_iq, &thread->m_iq[iqLen], iqRemaining * sizeof(qint16));
        thread->m_iqCount = iqRemaining;
    }
}

void SDRPlayV3Thread::callbackIQ(const qint16* buf, qint32 len)
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
}

void SDRPlayV3Thread::callbackQI(const qint16* buf, qint32 len)
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
}
