///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <errno.h>

#include "limesdroutputthread.h"
#include "limesdroutputsettings.h"

LimeSDROutputThread::LimeSDROutputThread(lms_stream_t* stream, SampleSourceFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_stream(stream),
    m_sampleFifo(sampleFifo),
    m_log2Interp(0),
    m_fcPos(LimeSDROutputSettings::FC_POS_CENTER)
{
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

void LimeSDROutputThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
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
        callback(m_buf, LIMESDROUTPUT_BLOCKSIZE);

        res = LMS_SendStream(m_stream, (void *) m_buf, LIMESDROUTPUT_BLOCKSIZE, &metadata, 1000000);

        if (res < 0)
        {
            qCritical("LimeSDROutputThread::run write error: %s", strerror(errno));
            break;
        }
        else if (res != LIMESDROUTPUT_BLOCKSIZE)
        {
            qDebug("LimeSDROutputThread::run written %d/%d samples", res, LIMESDROUTPUT_BLOCKSIZE);
        }
    }

    m_running = false;
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16)
void LimeSDROutputThread::callback(qint16* buf, qint32 len)
{
    SampleVector::iterator beginRead;
    m_sampleFifo->readAdvance(beginRead, len/(1<<m_log2Interp));
    beginRead -= len;

    if (m_log2Interp == 0)
    {
        m_interpolators.interpolate1(&beginRead, buf, len*2);
    }
    else
    {
        switch (m_log2Interp)
        {
        case 1:
            m_interpolators.interpolate2_cen(&beginRead, buf, len*2);
            break;
        case 2:
            m_interpolators.interpolate4_cen(&beginRead, buf, len*2);
            break;
        case 3:
            m_interpolators.interpolate8_cen(&beginRead, buf, len*2);
            break;
        case 4:
            m_interpolators.interpolate16_cen(&beginRead, buf, len*2);
            break;
        case 5:
            m_interpolators.interpolate32_cen(&beginRead, buf, len*2);
            break;
        case 6:
            m_interpolators.interpolate64_cen(&beginRead, buf, len*2);
            break;
        default:
            break;
        }
    }
}

