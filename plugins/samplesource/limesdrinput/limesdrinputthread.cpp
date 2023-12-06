///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <errno.h>
#include <algorithm>

#include "dsp/replaybuffer.h"
#include "limesdrinputsettings.h"
#include "limesdrinputthread.h"

LimeSDRInputThread::LimeSDRInputThread(lms_stream_t* stream, SampleSinkFifo* sampleFifo,
        ReplayBuffer<qint16> *replayBuffer, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_stream(stream),
    m_convertBuffer(DeviceLimeSDR::blockSize),
    m_sampleFifo(sampleFifo),
    m_replayBuffer(replayBuffer),
    m_log2Decim(0),
    m_iqOrder(true)
{
    std::fill(m_buf, m_buf + 2*DeviceLimeSDR::blockSize, 0);
}

LimeSDRInputThread::~LimeSDRInputThread()
{
    stopWork();
}

void LimeSDRInputThread::startWork()
{
    if (m_running) return; // return if running already

    if (LMS_StartStream(m_stream) < 0) {
        qCritical("LimeSDRInputThread::startWork: could not start stream");
    } else {
        usleep(50000);
        qDebug("LimeSDRInputThread::startWork: stream started");
    }

    m_startWaitMutex.lock();
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void LimeSDRInputThread::stopWork()
{
    if (!m_running) return; // return if not running

    m_running = false;
    wait();

    if (LMS_StopStream(m_stream) < 0) {
        qCritical("LimeSDRInputThread::stopWork: could not stop stream");
    } else {
        usleep(50000);
        qDebug("LimeSDRInputThread::stopWork: stream stopped");
    }
}

void LimeSDRInputThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

void LimeSDRInputThread::run()
{
    int res;

    lms_stream_meta_t metadata;          //Use metadata for additional control over sample receive function behaviour
    metadata.flushPartialPacket = false; //Do not discard data remainder when read size differs from packet size
    metadata.waitForTimestamp = false;   //Do not wait for specific timestamps

    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        if ((res = LMS_RecvStream(m_stream, (void *) m_buf, DeviceLimeSDR::blockSize, &metadata, 1000)) < 0)
        {
            qCritical("LimeSDRInputThread::run read error: %s", strerror(errno));
            break;
        }

        if (m_iqOrder) {
            callbackIQ(m_buf, 2 * res);
        } else {
            callbackQI(m_buf, 2 * res);
        }
    }

    m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void LimeSDRInputThread::callbackIQ(const qint16* inBuf, qint32 len)
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

        switch (m_log2Decim)
        {
        case 0:
            m_decimatorsIQ.decimate1(&it, buf, len);
            break;
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

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}

void LimeSDRInputThread::callbackQI(const qint16* inBuf, qint32 len)
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

        switch (m_log2Decim)
        {
        case 0:
            m_decimatorsQI.decimate1(&it, buf, len);
            break;
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

    m_replayBuffer->unlock();

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}
