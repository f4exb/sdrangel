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

#include <algorithm>
#include <chrono>
#include <thread>

#include "dsp/samplemofifo.h"

#include "xtrxmothread.h"

XTRXMOThread::XTRXMOThread(struct xtrx_dev *dev, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_sampleFifo(nullptr)
{
    qDebug("XTRXMOThread::XTRXMOThread");
    m_buf = new qint16[2*DeviceXTRX::blockSize*2];
    std::fill(m_buf, m_buf + 2*DeviceXTRX::blockSize*2, 0);
}

XTRXMOThread::~XTRXMOThread()
{
    qDebug("XTRXMOThread::~XTRXMOThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_buf;
}

void XTRXMOThread::startWork()
{
    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void XTRXMOThread::stopWork()
{
    m_running = false;
    wait();
}

void XTRXMOThread::setLog2Interpolation(unsigned int log2Interp)
{
    qDebug("XTRXMOThread::setLog2Interpolation: %u", log2Interp);
    m_log2Interp = log2Interp;
}

unsigned int XTRXMOThread::getLog2Interpolation() const
{
    return m_log2Interp;
}

void XTRXMOThread::run()
{
    int res;

    m_running = true;
    m_startWaiter.wakeAll();

    xtrx_run_params params;
    xtrx_run_params_init(&params);

    params.dir = XTRX_TX;
    params.tx_repeat_buf = 0;
    params.tx.paketsize = 2*DeviceXTRX::blockSize;
    params.tx.chs = XTRX_CH_AB;
    params.tx.wfmt = XTRX_WF_16;
    params.tx.hfmt = XTRX_IQ_INT16;
    params.tx.flags |= XTRX_RSP_SWAP_IQ;

    res = xtrx_run_ex(m_dev, &params);

    if (res != 0)
    {
        qCritical("XTRXMOThread::run: could not start stream err:%d", res);
        m_running = false;
    }
    else
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        qDebug("XTRXMOThread::run: stream started");
    }

    qint16 buf0[2*DeviceXTRX::blockSize]; // I+Q = 2x16 bit samples
    qint16 buf1[2*DeviceXTRX::blockSize];
    std::vector<void *> buffs(2);
    master_ts ts = 4096*1024;

    buffs[0] = &buf0;
    buffs[1] = &buf1;

    xtrx_send_ex_info_t nfo;
    nfo.samples = DeviceXTRX::blockSize;
    nfo.buffer_count = 2;
    nfo.buffers = (void* const*) buffs.data();
    nfo.flags = XTRX_TX_DONT_BUFFER; // | XTRX_TX_SEND_ZEROS;
    nfo.timeout = 0;
    nfo.out_txlatets = 0;
    nfo.ts = ts;

    while (m_running)
    {
        callback(buf0, buf1, nfo.samples);
        res = xtrx_send_sync_ex(m_dev, &nfo);

        if (res < 0)
        {
            qCritical("XTRXMOThread::run send error: %d", res);
            qDebug("XTRXMOThread::run: out_samples: %u out_flags: %u", nfo.out_samples, nfo.out_flags);
            break;
        }

        if (nfo.out_flags & XTRX_TX_DISCARDED_TO) {
            qDebug("XTRXMOThread::run: underrun");
        }

        if (nfo.out_txlatets) {
            qDebug("XTRXMOThread::run: out_txlatets: %lu", nfo.out_txlatets);
        }

        nfo.ts += DeviceXTRX::blockSize;
    }

    res = xtrx_stop(m_dev, XTRX_TX);

    if (res != 0)
    {
        qCritical("XTRXMOThread::run: could not stop stream");
    }
    else
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        qDebug("XTRXMOThread::run: stream stopped");
    }

    m_running = false;
}

void XTRXMOThread::callback(qint16* buf0, qint16* buf1, qint32 samplesPerChannel)
{
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    m_sampleFifo->readSync(samplesPerChannel/(1<<m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

    if (iPart1Begin != iPart1End)
    {
        callbackPart(buf0, buf1, (iPart1End - iPart1Begin)*(1<<m_log2Interp), iPart1Begin);
    }

    if (iPart2Begin != iPart2End)
    {
        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_log2Interp);
        callbackPart(buf0 + 2*shift, buf1 + 2*shift, (iPart2End - iPart2Begin)*(1<<m_log2Interp), iPart2Begin);
    }
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16). len is a number of samples (not a number of I or Q)
void XTRXMOThread::callbackPart(qint16* buf0, qint16* buf1, qint32 nSamples, int iBegin)
{
    for (unsigned int channel = 0; channel < 2; channel++)
    {
        SampleVector::iterator begin = m_sampleFifo->getData(channel).begin() + iBegin;

        if (m_log2Interp == 0)
        {
            m_interpolators[channel].interpolate1(
                &begin,
                channel == 0 ? buf0 : buf1,
                2*nSamples);
        }
        else
        {
            switch (m_log2Interp)
            {
            case 1:
                m_interpolators[channel].interpolate2_cen(
                    &begin,
                    channel == 0 ? buf0 : buf1,
                    2*nSamples);
                break;
            case 2:
                m_interpolators[channel].interpolate4_cen(
                    &begin,
                    channel == 0 ? buf0 : buf1,
                    2*nSamples);
                break;
            case 3:
                m_interpolators[channel].interpolate8_cen(
                    &begin,
                    channel == 0 ? buf0 : buf1,
                    2*nSamples);
                break;
            case 4:
                m_interpolators[channel].interpolate16_cen(
                    &begin,
                    channel == 0 ? buf0 : buf1,
                    2*nSamples);
                break;
            case 5:
                m_interpolators[channel].interpolate32_cen(
                    &begin,
                    channel == 0 ? buf0 : buf1,
                    2*nSamples);
                break;
            case 6:
                m_interpolators[channel].interpolate64_cen(
                    &begin,
                    channel == 0 ? buf0 : buf1,
                    2*nSamples);
                break;
            default:
                break;
            }
        }
    }
}
