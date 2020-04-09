///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/samplesourcefifo.h"

#include "limesdroutputthread.h"
#include "limesdroutputsettings.h"

LimeSDROutputThread::LimeSDROutputThread(lms_stream_t* stream, SampleSourceFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_stream(stream),
    m_sampleFifo(sampleFifo),
    m_log2Interp(0)
{
    std::fill(m_buf, m_buf + 2*DeviceLimeSDR::blockSize, 0);
}

LimeSDROutputThread::~LimeSDROutputThread()
{
    stopWork();
}

void LimeSDROutputThread::startWork()
{
    if (m_running) return; // return if running already

    if (LMS_StartStream(m_stream) < 0) {
        qCritical("LimeSDROutputThread::startWork: could not start stream");
    } else {
        usleep(50000);
        qDebug("LimeSDROutputThread::startWork: stream started");
    }

    m_startWaitMutex.lock();
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void LimeSDROutputThread::stopWork()
{
    if (!m_running) return; // return if not running

    m_running = false;
    wait();

    if (LMS_StopStream(m_stream) < 0) {
        qCritical("LimeSDROutputThread::stopWork: could not stop stream");
    } else {
        usleep(50000);
        qDebug("LimeSDROutputThread::stopWork: stream stopped");
    }
}

void LimeSDROutputThread::setLog2Interpolation(unsigned int log2_interp)
{
    m_log2Interp = log2_interp;
}

void LimeSDROutputThread::run()
{
    int res;

    lms_stream_meta_t metadata;          //Use metadata for additional control over sample receive function behaviour
    metadata.flushPartialPacket = false; //Do not discard data remainder when read size differs from packet size
    metadata.waitForTimestamp = false;   //Do not wait for specific timestamps

    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        callback(m_buf, DeviceLimeSDR::blockSize);

        res = LMS_SendStream(m_stream, (void *) m_buf, DeviceLimeSDR::blockSize, &metadata, 1000000);

        if (res < 0)
        {
            qCritical("LimeSDROutputThread::run write error: %s", strerror(errno));
            break;
        }
        else if (res != DeviceLimeSDR::blockSize)
        {
            qDebug("LimeSDROutputThread::run written %d/%d samples", res, DeviceLimeSDR::blockSize);
        }
    }

    m_running = false;
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16)
void LimeSDROutputThread::callback(qint16* buf, qint32 len)
{
    SampleVector& data = m_sampleFifo->getData();
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    m_sampleFifo->read(len/(1<<m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

    if (iPart1Begin != iPart1End) {
        callbackPart(buf, data, iPart1Begin, iPart1End);
    }

    unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_log2Interp);

    if (iPart2Begin != iPart2End) {
        callbackPart(buf + 2*shift, data, iPart2Begin, iPart2End);
    }
}

void LimeSDROutputThread::callbackPart(qint16* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    SampleVector::iterator beginRead = data.begin() + iBegin;
    int len = 2*(iEnd - iBegin)*(1<<m_log2Interp);

    if (m_log2Interp == 0)
    {
        m_interpolators.interpolate1(&beginRead, buf, len);
    }
    else
    {
        switch (m_log2Interp)
        {
        case 1:
            m_interpolators.interpolate2_cen(&beginRead, buf, len);
            break;
        case 2:
            m_interpolators.interpolate4_cen(&beginRead, buf, len);
            break;
        case 3:
            m_interpolators.interpolate8_cen(&beginRead, buf, len);
            break;
        case 4:
            m_interpolators.interpolate16_cen(&beginRead, buf, len);
            break;
        case 5:
            m_interpolators.interpolate32_cen(&beginRead, buf, len);
            break;
        case 6:
            m_interpolators.interpolate64_cen(&beginRead, buf, len);
            break;
        default:
            break;
        }
    }
}
