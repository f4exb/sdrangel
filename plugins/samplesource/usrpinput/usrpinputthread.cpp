///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <uhd/types/stream_cmd.hpp>

#include "usrpinputsettings.h"
#include "usrpinputthread.h"

USRPInputThread::USRPInputThread(uhd::rx_streamer::sptr stream, size_t bufSamples, SampleSinkFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_stream(stream),
    m_bufSamples(bufSamples),
    m_convertBuffer(bufSamples),
    m_sampleFifo(sampleFifo),
    m_log2Decim(0)
{
    // *2 as samples are I+Q
    m_buf = new qint16[2*bufSamples];
    std::fill(m_buf, m_buf + 2*bufSamples, 0);
}

USRPInputThread::~USRPInputThread()
{
    stopWork();
    delete m_buf;
}

void USRPInputThread::issueStreamCmd(bool start)
{
    uhd::stream_cmd_t stream_cmd(start ? uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS : uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    stream_cmd.num_samps = size_t(0);
    stream_cmd.stream_now = true;
    stream_cmd.time_spec = uhd::time_spec_t();

    m_stream->issue_stream_cmd(stream_cmd);
    qDebug() << "USRPInputThread::issueStreamCmd " << (start ? "start" : "stop");
}

void USRPInputThread::startWork()
{
    if (m_running) return; // return if running already

    try
    {
        // Start streaming
        issueStreamCmd(true);

        // Reset stats
        m_packets = 0;
        m_overflows = 0;
        m_timeouts = 0;

        qDebug("USRPInputThread::startWork: stream started");
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInputThread::startWork: exception: " << e.what();
    }

    m_startWaitMutex.lock();
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void USRPInputThread::stopWork()
{
    if (!m_running) return; // return if not running

    m_running = false;
    wait();

    try
    {
        uhd::rx_metadata_t md;

        // Stop streaming
        issueStreamCmd(false);

        // Clear out any data left in the stream, otherwise we'll get an
        // exception 'recv buffer smaller than vrt packet offset' when restarting
        md.end_of_burst = false;
        md.error_code = uhd::rx_metadata_t::ERROR_CODE_NONE;
        while (!md.end_of_burst && (md.error_code != uhd::rx_metadata_t::ERROR_CODE_TIMEOUT))
        {
            try
            {
                md.reset();
                m_stream->recv(m_buf, m_bufSamples, md);
            }
            catch (std::exception& e)
            {
                qDebug() << "USRPInputThread::stopWork: exception ignored while flushing buffers: " << e.what();
            }
        }

        qDebug("USRPInputThread::stopWork: stream stopped");
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInputThread::stopWork: exception: " << e.what();
    }
}

void USRPInputThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

void USRPInputThread::run()
{
    uhd::rx_metadata_t md;

    m_running = true;
    m_startWaiter.wakeAll();

    try
    {
        while (m_running)
        {
            md.reset();
            const size_t samples_received = m_stream->recv(m_buf, m_bufSamples, md);

            m_packets++;
            if (samples_received != m_bufSamples)
            {
                qDebug("USRPInputThread::run - received %ld/%ld samples", samples_received, m_bufSamples);
            }
            if (md.error_code ==  uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
            {
                qDebug("USRPInputThread::run - timeout - ending thread");
                m_timeouts++;
                // Restart streaming
                issueStreamCmd(false);
                issueStreamCmd(true);
                qDebug("USRPInputThread::run - timeout - restarting");
            }
            else if (md.error_code ==  uhd::rx_metadata_t::ERROR_CODE_OVERFLOW)
            {
                qDebug("USRPInputThread::run - overflow");
                m_overflows++;
            }
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_LATE_COMMAND)
                qDebug("USRPInputThread::run - late command error");
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_BROKEN_CHAIN)
                qDebug("USRPInputThread::run - broken chain error");
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_ALIGNMENT)
                qDebug("USRPInputThread::run - alignment error");
            else if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_BAD_PACKET)
                qDebug("USRPInputThread::run - bad packet error");

            if (samples_received > 0)
                callbackIQ(m_buf, 2 * samples_received);
        }
    }
    catch (std::exception& e)
    {
        qDebug() << "USRPInputThread::run: exception: " << e.what();
    }

    m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void USRPInputThread::callbackIQ(const qint16* buf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

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

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}

void USRPInputThread::getStreamStatus(bool& active, quint32& overflows, quint32& timeouts)
{
    //qDebug() << "USRPInputThread::getStreamStatus " << m_packets << " " << m_overflows << " " << m_timeouts;
    active = m_packets > 0;
    overflows = m_overflows;
    timeouts = m_timeouts;
}
