///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#include "xtrxinputsettings.h"
#include "xtrxinputthread.h"

XTRXInputThread::XTRXInputThread(DeviceXTRXShared* shared,
                                 SampleSinkFifo* sampleFifo,
                                 QObject* parent) :
    QThread(parent),
    m_running(false),
    m_convertBuffer(XTRX_BLOCKSIZE),
    m_sampleFifo(sampleFifo),
    m_log2Decim(0),
    m_fcPos(XTRXInputSettings::FC_POS_CENTER),
    m_shared(shared)
{
}

XTRXInputThread::~XTRXInputThread()
{
    stopWork();
}

void XTRXInputThread::startWork()
{
    if (m_running) {
        return; // return if running already
    }

    m_startWaitMutex.lock();
    start();

    while (!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void XTRXInputThread::stopWork()
{
    if (!m_running) {
        return; // return if not running
    }

    m_running = false;
    wait();
}

void XTRXInputThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

void XTRXInputThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

void XTRXInputThread::run()
{
    int res;

    m_running = true;
    m_startWaiter.wakeAll();

    xtrx_run_params params;
    xtrx_run_params_init(&params);

    params.dir = XTRX_RX;
    params.rx.chs = XTRX_CH_AB;
    params.rx.wfmt = XTRX_WF_16;
    params.rx.hfmt = XTRX_IQ_INT16;
    params.rx.flags |= XTRX_RSP_SISO_MODE;
    params.rx_stream_start = 2*8192;

    // TODO: replace this
    if (m_shared->m_channel == XTRX_CH_B) {
        params.rx.flags |= XTRX_RSP_SWAP_AB;
    }

    res = xtrx_run_ex(m_shared->m_deviceParams->getDevice(), &params);

    if (res != 0)
    {
        qCritical("XTRXInputThread::run: could not start stream err:%d", res);
        m_running = false;
    }
    else
    {
        usleep(50000);
        qDebug("XTRXInputThread::run: stream started");
    }

    void* buffers[1] = { m_buf };
    xtrx_recv_ex_info_t nfo;
    nfo.samples = XTRX_BLOCKSIZE;
    nfo.buffer_count = 1;
    nfo.buffers = (void* const*)buffers;
    nfo.flags = RCVEX_DONT_INSER_ZEROS | RCVEX_DROP_OLD_ON_OVERFLOW;


    while (m_running)
    {
        //if ((res = LMS_RecvStream(m_stream, (void *) m_buf, XTRX_BLOCKSIZE, &metadata, 1000)) < 0)
        res = xtrx_recv_sync_ex(m_shared->m_deviceParams->getDevice(), &nfo);

        if (res < 0)
        {
            qCritical("XTRXInputThread::run read error: %d", res);
            break;
        }

        callback(m_buf, 2 * nfo.out_samples);
    }

    res = xtrx_stop(m_shared->m_deviceParams->getDevice(), XTRX_RX);

    if (res != 0)
    {
        qCritical("XTRXInputThread::run: could not stop stream");
    }
    else
    {
        usleep(50000);
        qDebug("XTRXInputThread::run: stream stopped");
    }

    m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void XTRXInputThread::callback(const qint16* buf, qint32 len)
{
    SampleVector::iterator it = m_convertBuffer.begin();

    if (m_log2Decim == 0)
    {
        m_decimators.decimate1(&it, buf, len);
    }
    else
    {
        if (m_fcPos == 0) // Infra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimators.decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_decimators.decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_decimators.decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_decimators.decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_decimators.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_decimators.decimate64_inf(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_fcPos == 1) // Supra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimators.decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_decimators.decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_decimators.decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_decimators.decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_decimators.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_decimators.decimate64_sup(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_fcPos == 2) // Center
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimators.decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_decimators.decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_decimators.decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_decimators.decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_decimators.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_decimators.decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}
