///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <algorithm>

#include <QDebug>

#include <uhd/types/metadata.hpp>

#include "dsp/samplemofifo.h"
#include "usrp/deviceusrpshared.h"
#include "usrp/deviceusrp.h"

#include "usrpmothread.h"

USRPMOThread::USRPMOThread(uhd::usrp::multi_usrp::sptr device, uhd::tx_streamer::sptr stream, size_t bufSamples,
        int nbChannels, SampleMOFifo *sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_packets(0),
    m_underflows(0),
    m_droppedPackets(0),
    m_device(device),
    m_stream(stream),
    m_bufSamples(bufSamples),
    m_nbChannels(std::min(nbChannels, m_maxChannels)),
    m_sampleFifo(sampleFifo),
    m_log2Interp(0),
    m_detached(false)
{
    setObjectName("USRPMO");

    for (int channel = 0; channel < m_nbChannels; channel++) {
        // *2 as samples are I+Q
        m_buf.push_back(std::vector<qint16>(2 * m_bufSamples, 0));
    }

    for (int channel = 0; channel < m_nbChannels; channel++) {
        m_bufPtrs.push_back(m_buf[channel].data());
    }

    // Executes in the thread this object was created in (the device engine thread), not in run()
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &USRPMOThread::handleInputMessages);
}

USRPMOThread::~USRPMOThread()
{
    stopWork();
}

void USRPMOThread::startWork()
{
    if (m_running || !m_stream) {
        return;
    }

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

void USRPMOThread::stopWork()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    wait();

    try
    {
        // Get message indicating underflow, so it doesn't appear if we restart
        uhd::async_metadata_t md;
        m_stream->recv_async_msg(md);
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPMOThread::stopWork: exception: " << e.what();
    }
}

// Release references to stream and device, so they're destroyed when the device is closed,
// even if this object is deleted later. Thread must be stopped.
void USRPMOThread::releaseStream()
{
    m_stream = nullptr;
    m_device = nullptr;
}

// Stop handling requests, waiting for any request being handled to complete.
// Used when stopping from a thread other than the device engine thread (E.g. when USRPMIMO is being destroyed)
void USRPMOThread::detach()
{
    QMutexLocker mutexLocker(&m_configMutex);
    m_detached = true;
}

void USRPMOThread::handleInputMessages()
{
    QMutexLocker mutexLocker(&m_configMutex);
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (!m_detached)
        {
            if (DeviceUSRPShared::MsgResizeSampleFifo::match(*message)) {
                resizeSampleFifo(((const DeviceUSRPShared::MsgResizeSampleFifo&) *message).getSize());
            } else {
                qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
            }
        }

        delete message;
    }
}

// Resize the sample FIFO. This executes in the device engine thread, which writes to the FIFO,
// so the only other thread that accesses it is the streaming thread, which is paused while it is resized.
void USRPMOThread::resizeSampleFifo(unsigned int size)
{
    bool wasRunning = m_running;

    if (wasRunning) {
        stopWork();
    }

    m_sampleFifo->resize(size);
    qDebug("USRPMOThread::resizeSampleFifo: %u", size);

    if (wasRunning) {
        startWork();
    }
}

void USRPMOThread::run()
{
    uhd::tx_metadata_t md;
    md.end_of_burst = false;

    // Higher priority, so streaming isn't delayed by other threads
    DeviceUSRP::setStreamingThreadPriority();

    if (m_nbChannels > 1)
    {
        // Channels in a multi-channel stream need to be started at the same time, so they are aligned
        try
        {
            md.start_of_burst = true;
            md.has_time_spec = true;
            md.time_spec = m_device->get_time_now() + uhd::time_spec_t(0.05);
        }
        catch (std::exception& e)
        {
            qWarning() << "USRPMOThread::run: could not get time: " << e.what();
            md.start_of_burst = false;
            md.has_time_spec = false;
        }
    }
    else
    {
        md.start_of_burst = false;
        md.has_time_spec = false;
    }

    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        unsigned int writtenSamples = callback();

        try
        {
            // First packet may not be sent until its time, so allow for that in timeout
            const size_t sentSamples = m_stream->send(m_bufPtrs, writtenSamples, md, md.has_time_spec ? 1.0 : DeviceUSRP::m_streamTimeout);
            m_packets++;

            if (sentSamples != writtenSamples) {
                qDebug("USRPMOThread::run: sent %zu/%u samples", sentSamples, writtenSamples);
            }
        }
        catch (std::exception& e)
        {
            qDebug() << "USRPMOThread::run: exception: " << e.what();
        }

        md.start_of_burst = false;
        md.has_time_spec = false;
    }

    // End the burst, so the stream can be restarted cleanly
    try
    {
        md.end_of_burst = true;
        m_stream->send(m_bufPtrs, 0, md);
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPMOThread::run: exception sending end of burst: " << e.what();
    }

    m_running = false;
}

// Read and interpolate samples for all channels.
// Returns number of samples written to each channel buffer, which may be less than the buffer size,
// when the buffer size is not divisible by the interpolation factor.
unsigned int USRPMOThread::callback()
{
    // Read once, as it can be changed from another thread, and the same value must be used for the whole buffer
    const unsigned int log2Interp = m_log2Interp;
    const unsigned int interpolationFactor = 1U << log2Interp;
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;

    m_sampleFifo->readSync(m_bufSamples / interpolationFactor, iPart1Begin, iPart1End, iPart2Begin, iPart2End);

    if (iPart1Begin != iPart1End) {
        callbackPart(iPart1Begin, iPart1End - iPart1Begin, 0, log2Interp);
    }

    if (iPart2Begin != iPart2End) {
        callbackPart(iPart2Begin, iPart2End - iPart2Begin, (iPart1End - iPart1Begin) * interpolationFactor, log2Interp);
    }

    return ((iPart1End - iPart1Begin) + (iPart2End - iPart2Begin)) * interpolationFactor;
}

// Interpolate nSamples from FIFO starting at iBegin, into each channel buffer, starting at sample offset
void USRPMOThread::callbackPart(unsigned int iBegin, unsigned int nSamples, unsigned int offset, unsigned int log2Interp)
{
    const int len = 2 * nSamples * (1 << log2Interp); // Number of I and Q values output

    for (int channel = 0; channel < m_nbChannels; channel++)
    {
        SampleVector::iterator begin = m_sampleFifo->getData(channel).begin() + iBegin;
        qint16 *buf = &m_buf[channel][2 * offset];

        switch (log2Interp)
        {
        case 0:
            m_interpolators[channel].interpolate1(&begin, buf, len);
            break;
        case 1:
            m_interpolators[channel].interpolate2_cen(&begin, buf, len);
            break;
        case 2:
            m_interpolators[channel].interpolate4_cen(&begin, buf, len);
            break;
        case 3:
            m_interpolators[channel].interpolate8_cen(&begin, buf, len);
            break;
        case 4:
            m_interpolators[channel].interpolate16_cen(&begin, buf, len);
            break;
        case 5:
            m_interpolators[channel].interpolate32_cen(&begin, buf, len);
            break;
        case 6:
            m_interpolators[channel].interpolate64_cen(&begin, buf, len);
            break;
        default:
            break;
        }
    }
}

// Called from GUI thread
void USRPMOThread::getStreamStatus(bool& active, quint32& underflows, quint32& droppedPackets)
{
    uhd::async_metadata_t md;

    // Don't wait for messages (default timeout is 0.1s), and drain all pending messages
    try
    {
        while (m_stream && m_stream->recv_async_msg(md, 0.0))
        {
            if ((md.event_code & uhd::async_metadata_t::EVENT_CODE_UNDERFLOW)
                || (md.event_code & uhd::async_metadata_t::EVENT_CODE_UNDERFLOW_IN_PACKET)) {
                m_underflows++;
            }
            if ((md.event_code & uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR)
                || (md.event_code & uhd::async_metadata_t::EVENT_CODE_SEQ_ERROR_IN_BURST)) {
                m_droppedPackets++;
            }
        }
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPMOThread::getStreamStatus: exception: " << e.what();
    }

    active = m_packets > 0;
    underflows = m_underflows;
    droppedPackets = m_droppedPackets;
}
