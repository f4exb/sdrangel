///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016, 2019-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2021-2022 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021 Franco Venturi <fventuri@comcast.net>                      //
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
#include "sdrplayv3input.h"
#include "dsp/samplesinkfifo.h"
#include "util/poweroftwo.h"

#include <QDebug>

SDRPlayV3Thread::SDRPlayV3Thread(sdrplay_api_DeviceT* dev, SampleSinkFifo* sampleFifo, ReplayBuffer<qint16> *replayBuffer, SDRPlayV3Input *input, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_sampleCount(0),
    m_overloadCount(0),
    m_deviceLost(false),
    m_dev(dev),
    m_convertBuffer(SDRPLAYV3_INIT_NBSAMPLES),
    m_sampleFifo(sampleFifo),
    m_replayBuffer(replayBuffer),
    m_input(input),
    m_samplerate(2000000),
    m_log2Decim(0),
    m_fcPos(0),
    m_iqOrder(true),
    m_iqCount(0),
    m_rfChanged(0)
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

bool SDRPlayV3Thread::stopWork()
{
    sdrplay_api_ErrT err;
    bool ok = true;

    if (m_running)
    {
        // Stop the watchdog in run()
        m_watchdogMutex.lock();
        m_running = false;
        m_watchdogWaiter.wakeAll();
        m_watchdogMutex.unlock();

        if ((err = sdrplay_api_Uninit(m_dev->dev)) != sdrplay_api_Success)
        {
            qWarning() << "SDRPlayV3Thread::stopWork: sdrplay_api_Uninit error: " << sdrplay_api_GetErrorString(err);
            ok = false;
        }
    }

    wait();

    return ok;
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

// Samples are delivered by the API's callbacks, so this thread only runs a watchdog
// that reports when the API stops delivering samples (USB drop, API service stall)
void SDRPlayV3Thread::run()
{
    QMutexLocker watchdogLocker(&m_watchdogMutex);

    m_running = true;
    m_startWaiter.wakeAll();

    quint64 lastSampleCount = m_sampleCount;
    bool stalled = false;

    while (m_running)
    {
        m_watchdogWaiter.wait(&m_watchdogMutex, m_watchdogPeriodMs);

        if (!m_running) {
            break;
        }

        quint64 sampleCount = m_sampleCount;

        if (sampleCount == lastSampleCount)
        {
            if (!stalled)
            {
                qWarning() << "SDRPlayV3Thread::run: no samples received from the device for" << m_watchdogPeriodMs
                    << "ms. Check the USB connection and the SDRplay API service";
                stalled = true;

                if (!m_deviceLost)
                {
                    m_input->reportError(QString("No samples have been received from the device for %1 ms.\n\n"
                        "Check the USB connection. Restarting the SDRplay API service, then stopping and starting the device, may help.")
                        .arg(m_watchdogPeriodMs));
                }
            }
        }
        else if (stalled)
        {
            qInfo() << "SDRPlayV3Thread::run: samples are being received again";
            stalled = false;
        }

        lastSampleCount = sampleCount;
    }
}

void SDRPlayV3Thread::eventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params, void *cbContext)
{
    SDRPlayV3Thread* thread = (SDRPlayV3Thread*) cbContext;
    sdrplay_api_ErrT err;

    switch (eventId)
    {
    case sdrplay_api_PowerOverloadChange:
        if (params->powerOverloadParams.powerOverloadChangeType == sdrplay_api_Overload_Detected)
        {
            // Log the first one loudly, as they can repeat rapidly with a strong signal
            if (thread->m_overloadCount++ == 0) {
                qWarning() << "SDRPlayV3Thread::eventCallback: ADC overload detected. Consider reducing the gain";
            } else {
                qDebug() << "SDRPlayV3Thread::eventCallback: ADC overload detected:" << thread->m_overloadCount;
            }
            thread->m_input->reportOverload(true);
        }
        else
        {
            qDebug() << "SDRPlayV3Thread::eventCallback: ADC overload corrected";
            thread->m_input->reportOverload(false);
        }
        // The API requires overload events to be acknowledged, but rejects updates
        // once sdrplay_api_Uninit has been called (it sends overload corrected while stopping)
        if (thread->m_running && (err = sdrplay_api_Update(thread->m_dev->dev, tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck, sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success) {
            qWarning() << "SDRPlayV3Thread::eventCallback: could not acknowledge overload:" << sdrplay_api_GetErrorString(err);
        }
        break;
    case sdrplay_api_DeviceRemoved:
        // The API can send this event repeatedly, so only report the first removal or failure
        if (!thread->m_deviceLost.exchange(true))
        {
            qCritical() << "SDRPlayV3Thread::eventCallback: device removed. Samples will no longer be received";
            thread->m_input->reportDeviceLost();
            thread->m_input->reportError("The SDRplay API reports that the device has been removed. Samples will no longer be received.\n\n"
                "Check the USB connection, then stop and start the device.");
        }
        else
        {
            qDebug() << "SDRPlayV3Thread::eventCallback: device removed";
        }
        break;
    case sdrplay_api_DeviceFailure:
        if (!thread->m_deviceLost.exchange(true))
        {
            qCritical() << "SDRPlayV3Thread::eventCallback: device failure. Samples will no longer be received";
            thread->m_input->reportDeviceLost();
            thread->m_input->reportError("The SDRplay API reports that the device has failed. Samples will no longer be received.\n\n"
                "Check the USB connection. Restarting the SDRplay API service, then stopping and starting the device, may help.");
        }
        else
        {
            qDebug() << "SDRPlayV3Thread::eventCallback: device failure";
        }
        break;
    case sdrplay_api_RspDuoModeChange:
        qDebug() << "SDRPlayV3Thread::eventCallback: RSPduo mode change:" << (int) params->rspDuoModeParams.modeChangeType;
        break;
    default:
        // sdrplay_api_GainChange - could report params->gainParams.currGain
        break;
    }
}

void SDRPlayV3Thread::callbackHelper(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *ctx)
{
    (void) params;
    (void) reset;
    SDRPlayV3Thread* thread = (SDRPlayV3Thread*) ctx;

    thread->m_sampleCount += numSamples;

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

void SDRPlayV3Thread::callbackIQ(const qint16* inBuf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    // Save data to replay buffer
    m_replayBuffer->lock();
    bool replayEnabled = m_replayBuffer->getSize() > 0;
    if (replayEnabled) {
        m_replayBuffer->write(inBuf, len);
    }

    const qint16* buf = inBuf;
    qint32 remaining = len;

    while (remaining > 0)
    {
        // Choose between live data or replayed data
        if (replayEnabled && m_replayBuffer->useReplay()) {
            len = m_replayBuffer->read(remaining, buf);
        } else {
            len = remaining;
        }
        remaining -= len;

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
    }

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}

void SDRPlayV3Thread::callbackQI(const qint16* inBuf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    // Save data to replay buffer
    m_replayBuffer->lock();
    bool replayEnabled = m_replayBuffer->getSize() > 0;
    if (replayEnabled) {
        m_replayBuffer->write(inBuf, len);
    }

    const qint16* buf = inBuf;
    qint32 remaining = len;

    while (remaining > 0)
    {
        // Choose between live data or replayed data
        if (replayEnabled && m_replayBuffer->useReplay()) {
            len = m_replayBuffer->read(remaining, buf);
        } else {
            len = remaining;
        }
        remaining -= len;

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
    }

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}
