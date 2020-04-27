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

#include "dsp/samplemofifo.h"

#include "limesdrmothread.h"

LimeSDRMOThread::LimeSDRMOThread(lms_stream_t* stream0, lms_stream_t* stream1, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_stream0(stream0),
    m_stream1(stream1),
    m_sampleFifo(nullptr)
{
    qDebug("LimeSDRMOThread::LimeSDRMOThread");
    m_buf = new qint16[2*DeviceLimeSDR::blockSize*2];
    std::fill(m_buf, m_buf + 2*DeviceLimeSDR::blockSize*2, 0);
}

LimeSDRMOThread::~LimeSDRMOThread()
{
    qDebug("LimeSDRMOThread::~LimeSDRMOThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_buf;
}

void LimeSDRMOThread::startWork()
{
    if (m_running) {
        return; // return if running already
    }

    int ret[2];

    ret[0] = LMS_StartStream(m_stream0);
    ret[1] = LMS_StartStream(m_stream1);

    if (ret[0] < 0)
    {
        qCritical("LimeSDROutputThread::startWork: could not start stream 0");
        return;
    }
    else
    {
        qDebug("LimeSDROutputThread::startWork: stream 0 started");
    }

    if (m_stream1)
    {
        if (ret[1] < 0)
        {
            qCritical("LimeSDROutputThread::startWork: could not start stream 1");
            LMS_StopStream(m_stream0);
            return;
        }
        else
        {
            qDebug("LimeSDROutputThread::startWork: stream 1 started");
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

void LimeSDRMOThread::stopWork()
{
    if (!m_running) {
        return; // return if not running
    }

    m_running = false;
    wait();
    int ret[2];

    ret[0] = LMS_StopStream(m_stream0);
    ret[1] = LMS_StopStream(m_stream1);

    if (ret[0] < 0)
    {
        qCritical("LimeSDROutputThread::stopWork: could not stop stream 0");
    } else {
        qDebug("LimeSDROutputThread::stopWork: stream 0 stopped");
    }

    if (m_stream1)
    {
        if (ret[1] < 0)
        {
            qCritical("LimeSDROutputThread::stopWork: could not stop stream 1");
        } else {
            qDebug("LimeSDROutputThread::stopWork: stream 1 stopped");
        }
    }

    usleep(50000);
}

void LimeSDRMOThread::setLog2Interpolation(unsigned int log2Interp)
{
    qDebug("LimeSDRMOThread::setLog2Interpolation: %u", log2Interp);
    m_log2Interp = log2Interp;
}

unsigned int LimeSDRMOThread::getLog2Interpolation() const
{
    return m_log2Interp;
}

void LimeSDRMOThread::run()
{
    lms_stream_meta_t metadata;          //Use metadata for additional control over sample receive function behaviour
    metadata.flushPartialPacket = false; //Do not discard data remainder when read size differs from packet size
    metadata.waitForTimestamp = false;   //Do not wait for specific timestamps

    m_running = true;
    m_startWaiter.wakeAll();
    int res[2];

    while (m_running)
    {
        callback(m_buf, DeviceLimeSDR::blockSize);

        res[0] = LMS_SendStream(m_stream0, (void *) &m_buf[0], DeviceLimeSDR::blockSize, &metadata, 1000000);

        if (res[0] < 0)
        {
            qCritical("LimeSDROutputThread::run stream 0 write error: %s", strerror(errno));
            break;
        }
        else if (res[0] != DeviceLimeSDR::blockSize)
        {
            qDebug("LimeSDROutputThread::run stream 0 written %d/%u samples", res[0], DeviceLimeSDR::blockSize);
        }

        if (m_stream1)
        {
            res[1] = LMS_SendStream(m_stream1, (void *) &m_buf[2*DeviceLimeSDR::blockSize], DeviceLimeSDR::blockSize, &metadata, 1000000);

            if (res[1] < 0)
            {
                qCritical("LimeSDROutputThread::run stream 1 write error: %s", strerror(errno));
                break;
            }
            else if (res[1] != DeviceLimeSDR::blockSize)
            {
                qDebug("LimeSDROutputThread::run stream 1 written %d/%u samples", res[1], DeviceLimeSDR::blockSize);
            }
        }
    }

    m_running = false;
}

void LimeSDRMOThread::callback(qint16* buf, qint32 samplesPerChannel)
{
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    m_sampleFifo->readSync(samplesPerChannel/(1<<m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

    if (iPart1Begin != iPart1End)
    {
        callbackPart(buf, (iPart1End - iPart1Begin)*(1<<m_log2Interp), iPart1Begin);
    }

    if (iPart2Begin != iPart2End)
    {
        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_log2Interp);
        callbackPart(buf + 2*shift, (iPart2End - iPart2Begin)*(1<<m_log2Interp), iPart2Begin);
    }
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16). len is a number of samples (not a number of I or Q)
void LimeSDRMOThread::callbackPart(qint16* buf, qint32 nSamples, int iBegin)
{
    for (unsigned int channel = 0; channel < 2; channel++)
    {
        SampleVector::iterator begin = m_sampleFifo->getData(channel).begin() + iBegin;

        if (m_log2Interp == 0)
        {
            m_interpolators[channel].interpolate1(&begin, &buf[channel*2*nSamples], 2*nSamples);
        }
        else
        {
            switch (m_log2Interp)
            {
            case 1:
                m_interpolators[channel].interpolate2_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                break;
            case 2:
                m_interpolators[channel].interpolate4_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                break;
            case 3:
                m_interpolators[channel].interpolate8_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                break;
            case 4:
                m_interpolators[channel].interpolate16_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                break;
            case 5:
                m_interpolators[channel].interpolate32_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                break;
            case 6:
                m_interpolators[channel].interpolate64_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                break;
            default:
                break;
            }
        }
    }
}
