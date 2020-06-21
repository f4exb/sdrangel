///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, 2018 Edouard Griffiths, F4EXB                             //
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#include "xtrx/devicextrx.h"
#include "xtrxinputsettings.h"
#include "xtrxinputthread.h"

XTRXInputThread::XTRXInputThread(struct xtrx_dev *dev, unsigned int nbChannels, unsigned int uniqueChannelIndex, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_nbChannels(nbChannels),
    m_uniqueChannelIndex(uniqueChannelIndex),
    m_iqOrder(true)
{
    qDebug("XTRXInputThread::XTRXInputThread: nbChannels: %u uniqueChannelIndex: %u", nbChannels, uniqueChannelIndex);
    m_channels = new Channel[2];

    for (unsigned int i = 0; i < 2; i++) {
        m_channels[i].m_convertBuffer.resize(DeviceXTRX::blockSize, Sample{0,0});
    }
}

XTRXInputThread::~XTRXInputThread()
{
    qDebug("XTRXInputThread::~XTRXInputThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_channels;
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


void XTRXInputThread::run()
{
    int res;

    m_running = true;
    m_startWaiter.wakeAll();

    unsigned int nbFifos = getNbFifos();

    if ((m_nbChannels != 0) && (nbFifos != 0))
    {
        xtrx_run_params params;
        xtrx_run_params_init(&params);

        params.dir = XTRX_RX;
        params.rx.chs = XTRX_CH_AB;
        params.rx.wfmt = XTRX_WF_16;
        params.rx.hfmt = XTRX_IQ_INT16;
        params.rx_stream_start = 2*DeviceXTRX::blockSize; // was 2*8192
        params.rx.paketsize = 2*DeviceXTRX::blockSize;

        if (m_nbChannels == 1)
        {
            qDebug("XTRXInputThread::run: SI mode for channel #%u", m_uniqueChannelIndex);
            params.rx.flags |= XTRX_RSP_SISO_MODE;

            if (m_uniqueChannelIndex == 1) {
                params.rx.flags |= XTRX_RSP_SWAP_AB;
            }
        }

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
        std::vector<std::vector<char>> buffMem(m_nbChannels, std::vector<char>(elemSize*DeviceXTRX::blockSize));
        std::vector<void *> buffs(m_nbChannels);

        for (std::size_t i = 0; i < m_nbChannels; i++) {
            buffs[i] = buffMem[i].data();
        }

        xtrx_recv_ex_info_t nfo;
        nfo.samples = DeviceXTRX::blockSize;
        nfo.buffer_count = m_nbChannels;
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

            if (m_nbChannels > 1)
            {
                callbackMI((const qint16*) buffs[0], (const qint16*) buffs[1], 2 * nfo.out_samples);
            }
            else
            {
                if (m_iqOrder) {
                    callbackSIIQ((const qint16*) buffs[0], 2 * nfo.out_samples);
                } else {
                    callbackSIQI((const qint16*) buffs[0], 2 * nfo.out_samples);
                }
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
    }
    else
    {
        qWarning("XTRXInputThread::run: no channels or FIFO allocated. Aborting");
    }

    m_running = false;
}

unsigned int XTRXInputThread::getNbFifos()
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

void XTRXInputThread::setLog2Decimation(unsigned int channel, unsigned int log2_decim)
{
    if (channel < 2) {
        m_channels[channel].m_log2Decim = log2_decim;
    }
}

unsigned int XTRXInputThread::getLog2Decimation(unsigned int channel) const
{
    if (channel < 2) {
        return m_channels[channel].m_log2Decim;
    } else {
        return 0;
    }
}

void XTRXInputThread::setFifo(unsigned int channel, SampleSinkFifo *sampleFifo)
{
    if (channel < 2) {
        m_channels[channel].m_sampleFifo = sampleFifo;
    }
}

SampleSinkFifo *XTRXInputThread::getFifo(unsigned int channel)
{
    if (channel < 2) {
        return m_channels[channel].m_sampleFifo;
    } else {
        return 0;
    }
}

void XTRXInputThread::callbackSIIQ(const qint16* buf, qint32 len)
{
    SampleVector::iterator it = m_channels[m_uniqueChannelIndex].m_convertBuffer.begin();

    if (m_channels[m_uniqueChannelIndex].m_log2Decim == 0)
    {
        m_channels[m_uniqueChannelIndex].m_decimatorsIQ.decimate1(&it, buf, len);
    }
    else
    {
        switch (m_channels[m_uniqueChannelIndex].m_log2Decim)
        {
        case 1:
            m_channels[m_uniqueChannelIndex].m_decimatorsIQ.decimate2_cen(&it, buf, len);
            break;
        case 2:
            m_channels[m_uniqueChannelIndex].m_decimatorsIQ.decimate4_cen(&it, buf, len);
            break;
        case 3:
            m_channels[m_uniqueChannelIndex].m_decimatorsIQ.decimate8_cen(&it, buf, len);
            break;
        case 4:
            m_channels[m_uniqueChannelIndex].m_decimatorsIQ.decimate16_cen(&it, buf, len);
            break;
        case 5:
            m_channels[m_uniqueChannelIndex].m_decimatorsIQ.decimate32_cen(&it, buf, len);
            break;
        case 6:
            m_channels[m_uniqueChannelIndex].m_decimatorsIQ.decimate64_cen(&it, buf, len);
            break;
        default:
            break;
        }
    }

    m_channels[m_uniqueChannelIndex].m_sampleFifo->write(m_channels[m_uniqueChannelIndex].m_convertBuffer.begin(), it);
}

void XTRXInputThread::callbackSIQI(const qint16* buf, qint32 len)
{
    SampleVector::iterator it = m_channels[m_uniqueChannelIndex].m_convertBuffer.begin();

    if (m_channels[m_uniqueChannelIndex].m_log2Decim == 0)
    {
        m_channels[m_uniqueChannelIndex].m_decimatorsQI.decimate1(&it, buf, len);
    }
    else
    {
        switch (m_channels[m_uniqueChannelIndex].m_log2Decim)
        {
        case 1:
            m_channels[m_uniqueChannelIndex].m_decimatorsQI.decimate2_cen(&it, buf, len);
            break;
        case 2:
            m_channels[m_uniqueChannelIndex].m_decimatorsQI.decimate4_cen(&it, buf, len);
            break;
        case 3:
            m_channels[m_uniqueChannelIndex].m_decimatorsQI.decimate8_cen(&it, buf, len);
            break;
        case 4:
            m_channels[m_uniqueChannelIndex].m_decimatorsQI.decimate16_cen(&it, buf, len);
            break;
        case 5:
            m_channels[m_uniqueChannelIndex].m_decimatorsQI.decimate32_cen(&it, buf, len);
            break;
        case 6:
            m_channels[m_uniqueChannelIndex].m_decimatorsQI.decimate64_cen(&it, buf, len);
            break;
        default:
            break;
        }
    }

    m_channels[m_uniqueChannelIndex].m_sampleFifo->write(m_channels[m_uniqueChannelIndex].m_convertBuffer.begin(), it);
}

void XTRXInputThread::callbackMI(const qint16* buf0, const qint16* buf1, qint32 len)
{
    unsigned int uniqueChannelIndex = m_uniqueChannelIndex;

    // channel 0
    m_uniqueChannelIndex = 0;

    if (m_iqOrder) {
        callbackSIIQ(buf0, len);
    } else {
        callbackSIQI(buf0, len);
    }

    // channel 1
    m_uniqueChannelIndex = 1;

    if (m_iqOrder) {
        callbackSIIQ(buf1, len);
    } else {
        callbackSIQI(buf1, len);
    }

    m_uniqueChannelIndex = uniqueChannelIndex;
}
