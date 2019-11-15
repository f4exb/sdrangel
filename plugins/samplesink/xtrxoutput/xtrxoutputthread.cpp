///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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
#include <chrono>
#include <thread>

#include "xtrx/devicextrx.h"
#include "dsp/samplesourcefifo.h"
#include "xtrxoutputthread.h"


XTRXOutputThread::XTRXOutputThread(struct xtrx_dev *dev, unsigned int nbChannels, unsigned int uniqueChannelIndex, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_nbChannels(nbChannels),
    m_uniqueChannelIndex(uniqueChannelIndex)
{
    qDebug("XTRXOutputThread::XTRXOutputThread: nbChannels: %u uniqueChannelIndex: %u", nbChannels, uniqueChannelIndex);
    m_channels = new Channel[2];
}

XTRXOutputThread::~XTRXOutputThread()
{
    qDebug("XTRXOutputThread::~XTRXOutputThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_channels;
}

void XTRXOutputThread::startWork()
{
    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void XTRXOutputThread::stopWork()
{
    m_running = false;
    wait();
}

unsigned int XTRXOutputThread::getNbFifos()
{
    unsigned int fifoCount = 0;

    for (unsigned int i = 0; i < 2; i++)
    {
        if (m_channels[i].m_sampleFifo) {
            fifoCount++;
        }
    }

    return fifoCount;
}

void XTRXOutputThread::setLog2Interpolation(unsigned int channel, unsigned int log2_interp)
{
    if (channel < 2) {
        m_channels[channel].m_log2Interp = log2_interp;
    }
}

unsigned int XTRXOutputThread::getLog2Interpolation(unsigned int channel) const
{
    if (channel < 2) {
        return m_channels[channel].m_log2Interp;
    } else {
        return 0;
    }
}

void XTRXOutputThread::setFifo(unsigned int channel, SampleSourceFifo *sampleFifo)
{
    if (channel < 2) {
        m_channels[channel].m_sampleFifo = sampleFifo;
    }
}

SampleSourceFifo *XTRXOutputThread::getFifo(unsigned int channel)
{
    if (channel < 2) {
        return m_channels[channel].m_sampleFifo;
    } else {
        return 0;
    }
}

void XTRXOutputThread::run()
{
    int res;

    m_running = true;
    m_startWaiter.wakeAll();

    unsigned int nbFifos = getNbFifos();

    if ((m_nbChannels != 0) && (nbFifos != 0))
    {
        xtrx_run_params params;
        xtrx_run_params_init(&params);

        params.dir = XTRX_TX;
        params.tx_repeat_buf = 0;
        params.tx.paketsize = 2*DeviceXTRX::blockSize;
        params.tx.chs = XTRX_CH_AB;
        params.tx.wfmt = XTRX_WF_16;
        params.tx.hfmt = XTRX_IQ_INT16;
        params.tx.flags |= XTRX_RSP_SWAP_IQ;

        if (m_nbChannels == 1)
        {
            qDebug("XTRXOutputThread::run: SO mode for channel #%u", m_uniqueChannelIndex);
            params.tx.flags |= XTRX_RSP_SISO_MODE;

            if (m_uniqueChannelIndex == 1) {
                params.tx.flags |= XTRX_RSP_SWAP_AB;
            }
        }

        res = xtrx_run_ex(m_dev, &params);

        if (res != 0)
        {
            qCritical("XTRXOutputThread::run: could not start stream err:%d", res);
            m_running = false;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            qDebug("XTRXOutputThread::run: stream started");
        }

        const unsigned int elemSize = 4; // XTRX uses 4 byte I+Q samples
        std::vector<std::vector<char>> buffMem(m_nbChannels, std::vector<char>(elemSize*DeviceXTRX::blockSize));
        std::vector<void *> buffs(m_nbChannels);
        master_ts ts = 4096*1024;

        for (std::size_t i = 0; i < m_nbChannels; i++) {
            buffs[i] = buffMem[i].data();
        }

        xtrx_send_ex_info_t nfo;
        nfo.samples = DeviceXTRX::blockSize;
        nfo.buffer_count = m_nbChannels;
        nfo.buffers = (void* const*) buffs.data();
        nfo.flags = XTRX_TX_DONT_BUFFER; // | XTRX_TX_SEND_ZEROS;
        nfo.timeout = 0;
        nfo.out_txlatets = 0;
        nfo.ts = ts;

        while (m_running)
        {
//            if (m_nbChannels > 1) {
//                callbackMO((qint16*) buffs[0], (qint16*) buffs[1], nfo.samples);
//            } else {
//                callbackSO((qint16*) buffs[0], nfo.samples);
//            }

            callbackSO((qint16*) buffs[0], nfo.samples);
            res = xtrx_send_sync_ex(m_dev, &nfo);

            if (res < 0)
            {
                qCritical("XTRXOutputThread::run send error: %d", res);
                qDebug("XTRXOutputThread::run: out_samples: %u out_flags: %u", nfo.out_samples, nfo.out_flags);
                break;
            }

            if (nfo.out_flags & XTRX_TX_DISCARDED_TO) {
                qDebug("XTRXOutputThread::run: underrun");
            }

            if (nfo.out_txlatets) {
                qDebug("XTRXOutputThread::run: out_txlatets: %lu", nfo.out_txlatets);
            }

            nfo.ts += DeviceXTRX::blockSize;
        }

        res = xtrx_stop(m_dev, XTRX_TX);

        if (res != 0)
        {
            qCritical("XTRXOutputThread::run: could not stop stream");
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            qDebug("XTRXOutputThread::run: stream stopped");
        }
    }
    else
    {
        qWarning("XTRXOutputThread::run: no channels or FIFO allocated. Aborting");
    }

    m_running = false;
}

void XTRXOutputThread::callbackPart(qint16* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    SampleVector::iterator beginRead = data.begin() + iBegin;
    int len = 2*(iEnd - iBegin)*(1<<m_channels[m_uniqueChannelIndex].m_log2Interp);

    if (m_channels[m_uniqueChannelIndex].m_log2Interp == 0)
    {
        m_channels[m_uniqueChannelIndex].m_interpolators.interpolate1(&beginRead, buf, len*2);
    }
    else
    {
        switch (m_channels[m_uniqueChannelIndex].m_log2Interp)
        {
        case 1:
            m_channels[m_uniqueChannelIndex].m_interpolators.interpolate2_cen(&beginRead, buf, len*2);
            break;
        case 2:
            m_channels[m_uniqueChannelIndex].m_interpolators.interpolate4_cen(&beginRead, buf, len*2);
            break;
        case 3:
            m_channels[m_uniqueChannelIndex].m_interpolators.interpolate8_cen(&beginRead, buf, len*2);
            break;
        case 4:
            m_channels[m_uniqueChannelIndex].m_interpolators.interpolate16_cen(&beginRead, buf, len*2);
            break;
        case 5:
            m_channels[m_uniqueChannelIndex].m_interpolators.interpolate32_cen(&beginRead, buf, len*2);
            break;
        case 6:
            m_channels[m_uniqueChannelIndex].m_interpolators.interpolate64_cen(&beginRead, buf, len*2);
            break;
        default:
            break;
        }
    }
}

void XTRXOutputThread::callbackSO(qint16* buf, qint32 len)
{
    if (m_channels[m_uniqueChannelIndex].m_sampleFifo)
    {
        SampleVector& data = m_channels[m_uniqueChannelIndex].m_sampleFifo->getData();
        unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
        m_channels[m_uniqueChannelIndex].m_sampleFifo->read(len/(1<<m_channels[m_uniqueChannelIndex].m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

        if (iPart1Begin != iPart1End) {
            callbackPart(buf, data, iPart1Begin, iPart1End);
        }

        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_channels[m_uniqueChannelIndex].m_log2Interp);

        if (iPart2Begin != iPart2End) {
            callbackPart(buf + 2*shift, data, iPart2Begin, iPart2End);
        }
    }
    else
    {
        std::fill(buf, buf+2*len, 0);
    }
}

void XTRXOutputThread::callbackMO(qint16* buf0, qint16* buf1, qint32 len)
{
    unsigned int uniqueChannelIndex = m_uniqueChannelIndex;

    // channel 0
    m_uniqueChannelIndex = 0;
    callbackSO(buf0, len);
    // channel 1
    m_uniqueChannelIndex = 1;
    callbackSO(buf1, len);

    m_uniqueChannelIndex = uniqueChannelIndex;
}
