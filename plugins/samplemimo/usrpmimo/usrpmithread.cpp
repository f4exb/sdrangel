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

#include <uhd/types/stream_cmd.hpp>
#include <uhd/types/metadata.hpp>

#include "dsp/samplemififo.h"
#include "usrp/deviceusrp.h"

#include "usrpmithread.h"

USRPMIThread::USRPMIThread(uhd::usrp::multi_usrp::sptr device, uhd::rx_streamer::sptr stream, size_t bufSamples,
        int nbChannels, SampleMIFifo *sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_started(false),
    m_runStarted(false),
    m_packets(0),
    m_overflows(0),
    m_timeouts(0),
    m_device(device),
    m_stream(stream),
    m_bufSamples(bufSamples),
    m_nbChannels(std::min(nbChannels, m_maxChannels)),
    m_sampleFifo(sampleFifo),
    m_log2Decim(0)
{
    setObjectName("USRPMI");

    for (int channel = 0; channel < m_nbChannels; channel++)
    {
        // *2 as samples are I+Q
        m_buf.push_back(std::vector<qint16>(2 * m_bufSamples, 0));
        m_convertBuffer[channel].resize(m_bufSamples, Sample{0, 0});
    }

    for (int channel = 0; channel < m_nbChannels; channel++) {
        m_bufPtrs.push_back(m_buf[channel].data());
    }
}

USRPMIThread::~USRPMIThread()
{
    stopWork();
}

void USRPMIThread::issueStreamCmd(bool start)
{
    uhd::stream_cmd_t streamCmd(start ? uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS : uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    streamCmd.num_samps = 0;

    if (start && (m_nbChannels > 1))
    {
        // Channels in a multi-channel stream need to be started at the same time, so they are aligned
        streamCmd.stream_now = false;
        // Allow enough time for the command to reach the device, even if the device is being reconfigured
        streamCmd.time_spec = m_device->get_time_now() + uhd::time_spec_t(0.1);
    }
    else
    {
        streamCmd.stream_now = true;
    }

    m_stream->issue_stream_cmd(streamCmd);
    qDebug() << "USRPMIThread::issueStreamCmd:" << (start ? "start" : "stop");
}

void USRPMIThread::startWork()
{
    if (m_started) {
        return;
    }

    m_packets = 0;
    m_overflows = 0;
    m_timeouts = 0;

    try
    {
        issueStreamCmd(true);
    }
    catch (std::exception& e)
    {
        qWarning() << "USRPMIThread::startWork: exception: " << e.what();
    }

    // Wait for run() to start. Use a separate flag to m_running, as run() may exit immediately on error
    m_runStarted = false;
    m_started = true;
    m_startWaitMutex.lock();
    start();

    while (!m_runStarted) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void USRPMIThread::stopWork()
{
    if (!m_started) {
        return;
    }

    m_started = false;
    m_running = false;
    wait();

    try
    {
        issueStreamCmd(false);
        flush();
    }
    catch (std::exception& e)
    {
        qWarning() << "USRPMIThread::stopWork: exception: " << e.what();
    }
}

// Clear out any data left in the stream, otherwise we'll get an
// exception 'recv buffer smaller than vrt packet offset' when restarting
void USRPMIThread::flush()
{
    uhd::rx_metadata_t md;
    const int maxFlushes = 1000; // In case device has been disconnected

    md.end_of_burst = false;
    md.error_code = uhd::rx_metadata_t::ERROR_CODE_NONE;

    for (int i = 0; (i < maxFlushes) && !md.end_of_burst && (md.error_code != uhd::rx_metadata_t::ERROR_CODE_TIMEOUT); i++)
    {
        try
        {
            md.reset();
            m_stream->recv(m_bufPtrs, m_bufSamples, md);
        }
        catch (std::exception& e)
        {
            qDebug() << "USRPMIThread::flush: exception: " << e.what();
            break;
        }
    }
}

void USRPMIThread::run()
{
    uhd::rx_metadata_t md;
    // First samples won't arrive until the timed start
    double timeout = 1.0;

    // Higher priority, so streaming isn't delayed by other threads
    DeviceUSRP::setStreamingThreadPriority();

    // Hold mutex, so wake can't occur between startWork() checking m_runStarted and waiting
    m_startWaitMutex.lock();
    m_running = true;
    m_runStarted = true;
    m_startWaiter.wakeAll();
    m_startWaitMutex.unlock();

    try
    {
        while (m_running)
        {
            md.reset();
            const size_t samplesReceived = m_stream->recv(m_bufPtrs, m_bufSamples, md, timeout);
            timeout = DeviceUSRP::m_streamTimeout;

            m_packets++;

            if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
            {
                qDebug("USRPMIThread::run: timeout - restarting");
                m_timeouts++;
                issueStreamCmd(false);
                issueStreamCmd(true);
                timeout = 1.0;
            }
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW)
            {
                m_overflows++;
            }
            else if ((md.error_code == uhd::rx_metadata_t::ERROR_CODE_ALIGNMENT)
                || (md.error_code == uhd::rx_metadata_t::ERROR_CODE_LATE_COMMAND))
            {
                // Channels lose alignment when the device is reconfigured while streaming
                // (E.g. sample rate or clock source change, which resets the AD9361 on B2xx).
                // UHD can't recover from this itself, so restart with a timed start to realign them.
                // If the timed start command arrived late, streaming won't have started, so also restart,
                // rather than waiting for a timeout
                qDebug() << "USRPMIThread::run:" << QString::fromStdString(md.strerror()) << "- restarting";
                issueStreamCmd(false);
                flush();
                issueStreamCmd(true);
                timeout = 1.0;
                continue;
            }
            else if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE)
            {
                qDebug() << "USRPMIThread::run: error: " << QString::fromStdString(md.strerror());
            }

            if (samplesReceived > 0)
            {
                std::vector<SampleVector::const_iterator> vbegin;
                int length = 0;
                // Read once, as it can be changed from another thread, and the same value must be used for all channels
                const unsigned int log2Decim = m_log2Decim;

                for (int channel = 0; channel < m_nbChannels; channel++)
                {
                    int channelLength = channelCallback(m_buf[channel].data(), 2 * samplesReceived, channel, log2Decim);
                    length = channel == 0 ? channelLength : std::min(length, channelLength);
                    vbegin.push_back(m_convertBuffer[channel].begin());
                }

                m_sampleFifo->writeSync(vbegin, length);
            }
        }
    }
    catch (std::exception& e)
    {
        qWarning() << "USRPMIThread::run: exception: " << e.what();
    }

    m_running = false;
}

// Decimate according to specified log2 (ex: log2=4 => decim=16). len is number of I and Q values
int USRPMIThread::channelCallback(const qint16* buf, qint32 len, int channel, unsigned int log2Decim)
{
    SampleVector::iterator it = m_convertBuffer[channel].begin();

    switch (log2Decim)
    {
    case 0:
        m_decimators[channel].decimate1(&it, buf, len);
        break;
    case 1:
        m_decimators[channel].decimate2_cen(&it, buf, len);
        break;
    case 2:
        m_decimators[channel].decimate4_cen(&it, buf, len);
        break;
    case 3:
        m_decimators[channel].decimate8_cen(&it, buf, len);
        break;
    case 4:
        m_decimators[channel].decimate16_cen(&it, buf, len);
        break;
    case 5:
        m_decimators[channel].decimate32_cen(&it, buf, len);
        break;
    case 6:
        m_decimators[channel].decimate64_cen(&it, buf, len);
        break;
    default:
        break;
    }

    return it - m_convertBuffer[channel].begin();
}

void USRPMIThread::getStreamStatus(bool& active, quint32& overflows, quint32& timeouts)
{
    active = m_packets > 0;
    overflows = m_overflows;
    timeouts = m_timeouts;
}
