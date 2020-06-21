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

#include "dsp/samplemififo.h"

#include "limesdrmithread.h"

LimeSDRMIThread::LimeSDRMIThread(lms_stream_t* stream0, lms_stream_t* stream1, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_stream0(stream0),
    m_stream1(stream1),
    m_sampleFifo(nullptr),
    m_iqOrder(true)
{
    qDebug("LimeSDRMIThread::LimeSDRMIThread");

    for (unsigned int i = 0; i < 2; i++) {
        m_convertBuffer[i].resize(DeviceLimeSDR::blockSize, Sample{0,0});
    }

    m_vBegin.push_back(m_convertBuffer[0].begin());
    m_vBegin.push_back(m_convertBuffer[1].begin());
}

LimeSDRMIThread::~LimeSDRMIThread()
{
    qDebug("LimeSDRMIThread::~LimeSDRMIThread");

    if (m_running) {
        stopWork();
    }
}

void LimeSDRMIThread::startWork()
{
    if (m_running) {
        return; // return if running already
    }

    int ret[2];

    ret[0] = LMS_StartStream(m_stream0);
    ret[1] = LMS_StartStream(m_stream1);

    if (ret[0] < 0)
    {
        qCritical("LimeSDRMIThread::startWork: could not start stream 0");
        return;
    }
    else
    {
        qDebug("LimeSDRMIThread::startWork: stream 0 started");
    }

    if (m_stream1)
    {
        if (ret[1] < 0)
        {
            qCritical("LimeSDRMIThread::startWork: could not start stream 1");
            return;
        }
        else
        {
            qDebug("LimeSDRMIThread::startWork: stream 0 started");
        }
    }

    usleep(50000);
    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void LimeSDRMIThread::stopWork()
{
    if (!m_running) {
        return; // return if not running
    }

    m_running = false;
    wait();
    int ret[2];

    ret[0] = LMS_StopStream(m_stream0);
    ret[1] = LMS_StopStream(m_stream1);

    if (ret[0] < 0) {
        qCritical("LimeSDRInputThread::stopWork: could not stop stream 0");
    } else {
        qDebug("LimeSDRInputThread::stopWork: stream 0 stopped");
    }

    if (m_stream1)
    {
        if (ret[1] < 0) {
            qCritical("LimeSDRInputThread::stopWork: could not stop stream 1");
        } else {
            qDebug("LimeSDRInputThread::stopWork: stream 1 stopped");
        }
    }

    usleep(50000);
}

void LimeSDRMIThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

unsigned int LimeSDRMIThread::getLog2Decimation() const
{
    return m_log2Decim;
}

void LimeSDRMIThread::run()
{
    lms_stream_meta_t metadata;       //Use metadata for additional control over sample receive function behaviour
    metadata.flushPartialPacket = false; //Do not discard data remainder when read size differs from packet size
    metadata.waitForTimestamp = false;   //Do not wait for specific timestamps
    int lengths[2];
    int res[2];

    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running && m_stream0)
    {
        res[0] = LMS_RecvStream(m_stream0, (void *) m_buf0, DeviceLimeSDR::blockSize, &metadata, 1000);

        if (res[0] < 0)
        {
            qCritical("LimeSDRMIThread::run read stream 0 error: %s", strerror(errno));
            break;
        }

        if (m_stream1)
        {
            res[1] = LMS_RecvStream(m_stream1, (void *) m_buf1, DeviceLimeSDR::blockSize, &metadata, 1000);

            if (res[1] < 0)
            {
                qCritical("LimeSDRMIThread::run read stream 1 error: %s", strerror(errno));
                break;
            }
        }
        else
        {
            std::fill(m_buf1, m_buf1 + 2*DeviceLimeSDR::blockSize, 0);
            res[1] = DeviceLimeSDR::blockSize;
        }

        if (m_iqOrder)
        {
            lengths[0] = channelCallbackIQ(m_buf0, 2*res[0], 0);
            lengths[1] = channelCallbackIQ(m_buf1, 2*res[1], 1);
        }
        else
        {
            lengths[0] = channelCallbackQI(m_buf0, 2*res[0], 0);
            lengths[1] = channelCallbackQI(m_buf1, 2*res[1], 1);
        }


        if (lengths[0] == lengths[1])
        {
            //qDebug("LimeSDRMIThread::run: writeSync %d samples", lengths[0]);
            m_sampleFifo->writeSync(m_vBegin, lengths[0]);
        }
        else
        {
            qWarning("LimeSDRMIThread::run: unequal channel lengths: [0]=%d [1]=%d", lengths[0], lengths[1]);
            m_sampleFifo->writeSync(m_vBegin, (std::min)(lengths[0], lengths[1]));
        }
    }

    m_running = false;
}

int LimeSDRMIThread::channelCallbackIQ(const qint16* buf, qint32 len, int channel)
{
    SampleVector::iterator it = m_convertBuffer[channel].begin();

    if (m_log2Decim == 0)
    {
        m_decimatorsIQ[channel].decimate1(&it, buf, len);
    }
    else
    {
        switch (m_log2Decim)
        {
        case 1:
            m_decimatorsIQ[channel].decimate2_cen(&it, buf, len);
            break;
        case 2:
            m_decimatorsIQ[channel].decimate4_cen(&it, buf, len);
            break;
        case 3:
            m_decimatorsIQ[channel].decimate8_cen(&it, buf, len);
            break;
        case 4:
            m_decimatorsIQ[channel].decimate16_cen(&it, buf, len);
            break;
        case 5:
            m_decimatorsIQ[channel].decimate32_cen(&it, buf, len);
            break;
        case 6:
            m_decimatorsIQ[channel].decimate64_cen(&it, buf, len);
            break;
        default:
            break;
        }
    }

    return it - m_convertBuffer[channel].begin();
}

int LimeSDRMIThread::channelCallbackQI(const qint16* buf, qint32 len, int channel)
{
    SampleVector::iterator it = m_convertBuffer[channel].begin();

    if (m_log2Decim == 0)
    {
        m_decimatorsQI[channel].decimate1(&it, buf, len);
    }
    else
    {
        switch (m_log2Decim)
        {
        case 1:
            m_decimatorsQI[channel].decimate2_cen(&it, buf, len);
            break;
        case 2:
            m_decimatorsQI[channel].decimate4_cen(&it, buf, len);
            break;
        case 3:
            m_decimatorsQI[channel].decimate8_cen(&it, buf, len);
            break;
        case 4:
            m_decimatorsQI[channel].decimate16_cen(&it, buf, len);
            break;
        case 5:
            m_decimatorsQI[channel].decimate32_cen(&it, buf, len);
            break;
        case 6:
            m_decimatorsQI[channel].decimate64_cen(&it, buf, len);
            break;
        default:
            break;
        }
    }

    return it - m_convertBuffer[channel].begin();
}
