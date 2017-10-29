///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <unistd.h>

#include "plutosdr/deviceplutosdrbox.h"
#include "plutosdroutputsettings.h"
#include "iio.h"
#include "plutosdroutputthread.h"

PlutoSDROutputThread::PlutoSDROutputThread(uint32_t blocksizeSamples, DevicePlutoSDRBox* plutoBox, SampleSourceFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_plutoBox(plutoBox),
    m_blockSizeSamples(blocksizeSamples),
    m_sampleFifo(sampleFifo),
    m_log2Interp(0)
{
    m_buf = new qint16[blocksizeSamples*2];
//    m_bufConv = new qint16[blocksizeSamples*(sizeof(Sample)/sizeof(qint16))];
}

PlutoSDROutputThread::~PlutoSDROutputThread()
{
    stopWork();
    delete[] m_buf;
}

void PlutoSDROutputThread::startWork()
{
    if (m_running) return; // return if running already

    m_startWaitMutex.lock();
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void PlutoSDROutputThread::stopWork()
{
    if (!m_running) return; // return if not running

    m_running = false;
    wait();
}

void PlutoSDROutputThread::setLog2Interpolation(unsigned int log2_interp)
{
    m_log2Interp = log2_interp;
}

void PlutoSDROutputThread::run()
{
    std::ptrdiff_t p_inc = m_plutoBox->txBufferStep();

    qDebug("PlutoSDROutputThread::run: txBufferStep: %ld bytes", p_inc);
    qDebug("PlutoSDROutputThread::run: Rx sample size is %ld bytes", m_plutoBox->getRxSampleSize());
    qDebug("PlutoSDROutputThread::run: Tx sample size is %ld bytes", m_plutoBox->getTxSampleSize());
    qDebug("PlutoSDROutputThread::run: nominal nbytes_tx is %d bytes", m_blockSizeSamples*4);

    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        ssize_t nbytes_tx;
        char *p_dat, *p_end;
        int ihs; // half sample index (I then Q to make a sample)

        convert(m_buf, 2*m_blockSizeSamples); // size given in number of int16_t (I and Q interleaved)

        // WRITE: Get pointers to TX buf and write IQ to TX buf port 0
        p_end = m_plutoBox->txBufferEnd();
        ihs = 0;

        // p_inc is 2 on a char* buffer therefore each iteration processes only the I or Q sample
        // I and Q samples are processed one after the other
        // conversion is not needed as samples are little endian

        for (p_dat = m_plutoBox->txBufferFirst(), ihs = 0; p_dat < p_end; p_dat += p_inc, ihs += 2)
        {
            m_plutoBox->txChannelConvert((int16_t*) p_dat, &m_buf[ihs]);
            //*((int16_t*)p_dat) = m_buf[ihs] << 4;
        }

        // Schedule TX buffer for sending
        nbytes_tx = m_plutoBox->txBufferPush();

        if (nbytes_tx != 4*m_blockSizeSamples)
        {
            qDebug("PlutoSDROutputThread::run: error pushing buf %d / %d\n", (int) nbytes_tx, (int) 4*m_blockSizeSamples);
            usleep(200000);
            continue;
        }
    }

    m_running = false;
}

// Decimate according to specified log2 (ex: log2=4 => decim=16)
// len is in half samples (I or Q) thus the size up to which buf is filled
// SampleVector contains full (I, Q) samples
void PlutoSDROutputThread::convert(qint16* buf, qint32 len)
{
    // pull samples from baseband generator
    SampleVector::iterator beginRead;
    m_sampleFifo->readAdvance(beginRead, len/(2*(1<<m_log2Interp)));
    beginRead -= len/2;

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

