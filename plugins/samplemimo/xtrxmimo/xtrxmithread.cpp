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

#include <errno.h>
#include <chrono>
#include <thread>

#include "dsp/samplemififo.h"

#include "xtrxmithread.h"

XTRXMIThread::XTRXMIThread(struct xtrx_dev *dev, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_sampleFifo(nullptr),
    m_iqOrder(true)
{
    qDebug("XTRXMIThread::XTRXMIThread");

    for (unsigned int i = 0; i < 2; i++) {
        m_channels[i].m_convertBuffer.resize(DeviceXTRX::blockSize, Sample{0,0});
    }

    m_vBegin.push_back(m_channels[0].m_convertBuffer.begin());
    m_vBegin.push_back(m_channels[1].m_convertBuffer.begin());
}

XTRXMIThread::~XTRXMIThread()
{
    qDebug("XTRXMIThread::~XTRXMIThread");

    if (m_running) {
        stopWork();
    }
}

void XTRXMIThread::startWork()
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

void XTRXMIThread::stopWork()
{
    if (!m_running) {
        return; // return if not running
    }

    m_running = false;
    wait();
}

void XTRXMIThread::run()
{
    int res;
    int lengths[2];

    m_running = true;
    m_startWaiter.wakeAll();

    xtrx_run_params params;
    xtrx_run_params_init(&params);

    params.dir = XTRX_RX;
    params.rx.chs = XTRX_CH_AB;
    params.rx.wfmt = XTRX_WF_16;
    params.rx.hfmt = XTRX_IQ_INT16;
    params.rx_stream_start = 2*DeviceXTRX::blockSize; // was 2*8192
    params.rx.paketsize = 2*DeviceXTRX::blockSize;

    res = xtrx_run_ex(m_dev, &params);

    if (res != 0)
    {
        qCritical("XTRXInputThread::run: could not start stream err:%d", res);
        m_running = false;
    }
    else
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        qDebug("XTRXInputThread::run: stream started");
    }

    const unsigned int elemSize = 4; // XTRX uses 4 byte I+Q samples
    std::vector<std::vector<char>> buffMem(2, std::vector<char>(elemSize*DeviceXTRX::blockSize));
    std::vector<void *> buffs(2);

    for (std::size_t i = 0; i < 2; i++) {
        buffs[i] = buffMem[i].data();
    }

    xtrx_recv_ex_info_t nfo;
    nfo.samples = DeviceXTRX::blockSize;
    nfo.buffer_count = 2;
    nfo.buffers = (void* const*) buffs.data();
    nfo.flags = RCVEX_DONT_INSER_ZEROS | RCVEX_DROP_OLD_ON_OVERFLOW;

    while (m_running)
    {
        res = xtrx_recv_sync_ex(m_dev, &nfo);

        if (res < 0)
        {
            qCritical("XTRXInputThread::run read error: %d", res);
            qDebug("XTRXInputThread::run: out_samples: %u out_events: %u", nfo.out_samples, nfo.out_events);
            break;
        }

        if (nfo.out_events & RCVEX_EVENT_OVERFLOW) {
            qDebug("XTRXInputThread::run: overflow");
        }

        if (m_iqOrder)
        {
            lengths[0] = callbackSIIQ(0, (const qint16*) buffs[0], 2*nfo.out_samples);
            lengths[1] = callbackSIIQ(1, (const qint16*) buffs[1], 2*nfo.out_samples);
        }
        else
        {
            lengths[0] = callbackSIQI(0, (const qint16*) buffs[0], 2*nfo.out_samples);
            lengths[1] = callbackSIQI(1, (const qint16*) buffs[1], 2*nfo.out_samples);
        }


        if (lengths[0] == lengths[1])
        {
            m_sampleFifo->writeSync(m_vBegin, lengths[0]);
        }
        else
        {
            qWarning("XTRXMIThread::run: unequal channel lengths: [0]=%d [1]=%d", lengths[0], lengths[1]);
            m_sampleFifo->writeSync(m_vBegin, (std::min)(lengths[0], lengths[1]));
        }
    }

    res = xtrx_stop(m_dev, XTRX_RX);

    if (res != 0)
    {
        qCritical("XTRXInputThread::run: could not stop stream");
    }
    else
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        qDebug("XTRXInputThread::run: stream stopped");
    }

    m_running = false;
}

void XTRXMIThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

unsigned int XTRXMIThread::getLog2Decimation() const
{
    return m_log2Decim;
}

int XTRXMIThread::callbackSIIQ(unsigned int channel, const qint16* buf, qint32 len)
{
    SampleVector::iterator it = m_channels[channel].m_convertBuffer.begin();

    if (m_log2Decim == 0)
    {
        m_channels[channel].m_decimatorsIQ.decimate1(&it, buf, len);
    }
    else
    {
        switch (m_log2Decim)
        {
        case 1:
            m_channels[channel].m_decimatorsIQ.decimate2_cen(&it, buf, len);
            break;
        case 2:
            m_channels[channel].m_decimatorsIQ.decimate4_cen(&it, buf, len);
            break;
        case 3:
            m_channels[channel].m_decimatorsIQ.decimate8_cen(&it, buf, len);
            break;
        case 4:
            m_channels[channel].m_decimatorsIQ.decimate16_cen(&it, buf, len);
            break;
        case 5:
            m_channels[channel].m_decimatorsIQ.decimate32_cen(&it, buf, len);
            break;
        case 6:
            m_channels[channel].m_decimatorsIQ.decimate64_cen(&it, buf, len);
            break;
        default:
            break;
        }
    }

    return it - m_channels[channel].m_convertBuffer.begin();
}

int XTRXMIThread::callbackSIQI(unsigned int channel, const qint16* buf, qint32 len)
{
    SampleVector::iterator it = m_channels[channel].m_convertBuffer.begin();

    if (m_log2Decim == 0)
    {
        m_channels[channel].m_decimatorsQI.decimate1(&it, buf, len);
    }
    else
    {
        switch (m_log2Decim)
        {
        case 1:
            m_channels[channel].m_decimatorsQI.decimate2_cen(&it, buf, len);
            break;
        case 2:
            m_channels[channel].m_decimatorsQI.decimate4_cen(&it, buf, len);
            break;
        case 3:
            m_channels[channel].m_decimatorsQI.decimate8_cen(&it, buf, len);
            break;
        case 4:
            m_channels[channel].m_decimatorsQI.decimate16_cen(&it, buf, len);
            break;
        case 5:
            m_channels[channel].m_decimatorsQI.decimate32_cen(&it, buf, len);
            break;
        case 6:
            m_channels[channel].m_decimatorsQI.decimate64_cen(&it, buf, len);
            break;
        default:
            break;
        }
    }

    return it - m_channels[channel].m_convertBuffer.begin();
}

