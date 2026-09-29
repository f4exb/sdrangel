///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-26 Jon Beniston, M7RCE <jon@beniston.com>                  //
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

#include <QDebug>

#include "dsp/samplesourcefifo.h"

#include "usrpoutputthread.h"

USRPOutputThread::USRPOutputThread(uhd::tx_streamer::sptr stream, size_t bufSamples, SampleSourceFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_packets(0),
    m_underflows(0),
    m_droppedPackets(0),
    m_stream(stream),
    m_bufSamples(bufSamples),
    m_sampleFifo(sampleFifo),
    m_log2Interp(0)
{
    setObjectName("USRPOutput");
    // *2 as samples are I+Q
    m_buf = new qint16[2 * bufSamples];
    std::fill(m_buf, m_buf + 2 * bufSamples, 0);
}

USRPOutputThread::~USRPOutputThread()
{
    stopWork();
    delete[] m_buf;
}

void USRPOutputThread::startWork()
{
    if (m_running) return; // return if running already

    // Reset stats
    m_packets = 0;
    m_underflows = 0;
    m_droppedPackets = 0;

    m_startWaitMutex.lock();
    start();
    while (!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }
    m_startWaitMutex.unlock();
}

void USRPOutputThread::stopWork()
{
    uhd::async_metadata_t md;

    if (!m_running) return; // return if not running

    m_running = false;
    wait();

    try
    {
        // Get message indicating underflow, so it doesn't appear if we restart
        m_stream->recv_async_msg(md);
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPOutputThread::stopWork: exception: " << e.what();
    }

    qDebug("USRPOutputThread::stopWork: stream stopped");
}

void USRPOutputThread::setLog2Interpolation(unsigned int log2_interp)
{
    m_log2Interp = log2_interp;
}

void USRPOutputThread::run()
{
    uhd::tx_metadata_t md;
    md.start_of_burst = false;
    md.end_of_burst   = false;

    m_running = true;
    m_startWaiter.wakeAll();

    qDebug("USRPOutputThread::run");

    while (m_running)
    {
        qint32 writtenSamples = callback(m_buf, m_bufSamples);

        try
        {
            const size_t sentSamples = m_stream->send(m_buf, writtenSamples, md);
            m_packets++;
            if (sentSamples != writtenSamples) {
                qDebug("USRPOutputThread::run sent %ld/%ld samples", sentSamples, writtenSamples);
            }
        }
        catch (std::exception& e)
        {
            qDebug() << "USRPOutputThread::run: exception: " << e.what();
        }
    }

    m_running = false;
}

// Interpolate according to specified log2 (ex: log2=4 => interpolate=16)
// Returns the number of samples written to buf, which may be less than len, when the TX buffer is not divisible by the interpolation factor.
qint32 USRPOutputThread::callback(qint16* buf, qint32 len)
{
    const unsigned int interpolationFactor = 1U << m_log2Interp;
    SampleVector& data = m_sampleFifo->getData();
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;

    // Truncation is intentional here: reading more would overrun the fixed TX buffer when len is not divisible by interpolation factor.
    m_sampleFifo->read(static_cast<unsigned int>(len)/interpolationFactor, iPart1Begin, iPart1End, iPart2Begin, iPart2End);

    if (iPart1Begin != iPart1End) {
        callbackPart(buf, data, iPart1Begin, iPart1End);
    }

    const unsigned int shift = (iPart1End - iPart1Begin) * interpolationFactor;

    if (iPart2Begin != iPart2End) {
        callbackPart(buf + 2 * shift, data, iPart2Begin, iPart2End);
    }

    return ((iPart1End - iPart1Begin) + (iPart2End - iPart2Begin)) * interpolationFactor;
}

void USRPOutputThread::callbackPart(qint16* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    SampleVector::iterator beginRead = data.begin() + iBegin;
    const int len = 2 * (iEnd - iBegin) * (1 << m_log2Interp);

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

void USRPOutputThread::getStreamStatus(bool& active, quint32& underflows, quint32& droppedPackets)
{
    uhd::async_metadata_t md;

    if (m_stream->recv_async_msg(md))
    {
        if ((md.event_code & uhd::async_metadata_t::event_code_t::EVENT_CODE_UNDERFLOW)
            || (md.event_code & uhd::async_metadata_t::event_code_t::EVENT_CODE_UNDERFLOW_IN_PACKET)) {
            m_underflows++;
        }
        if ((md.event_code & uhd::async_metadata_t::event_code_t::EVENT_CODE_SEQ_ERROR)
            || (md.event_code & uhd::async_metadata_t::event_code_t::EVENT_CODE_SEQ_ERROR_IN_BURST)) {
            m_droppedPackets++;
        }
    }
    //qDebug() << "USRPOutputThread::getStreamStatus " << m_packets << " " << m_underflows << " " << m_droppedPackets;
    active = m_packets > 0;
    underflows = m_underflows;
    droppedPackets = m_droppedPackets;
}
