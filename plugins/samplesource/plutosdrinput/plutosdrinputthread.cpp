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
#include "plutosdrinputsettings.h"
#include "plutosdrinputthread.h"

#include "iio.h"

PlutoSDRInputThread::PlutoSDRInputThread(uint32_t blocksizeSamples, DevicePlutoSDRBox* plutoBox, SampleSinkFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_plutoBox(plutoBox),
    m_blockSizeSamples(blocksizeSamples),
    m_convertBuffer(blocksizeSamples),
    m_convertIt(m_convertBuffer.begin()),
    m_sampleFifo(sampleFifo),
    m_log2Decim(0),
    m_fcPos(PlutoSDRInputSettings::FC_POS_CENTER),
    m_phasor(0)
{
    m_buf     = new qint16[blocksizeSamples*2]; // (I,Q) -> 2 * int16_t
    m_bufConv = new qint16[blocksizeSamples*2]; // (I,Q) -> 2 * int16_t
}

PlutoSDRInputThread::~PlutoSDRInputThread()
{
    stopWork();
    delete[] m_buf;
}

void PlutoSDRInputThread::startWork()
{
    if (m_running) return; // return if running already

    m_startWaitMutex.lock();
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void PlutoSDRInputThread::stopWork()
{
    if (!m_running) return; // return if not running

    m_running = false;
    wait();
}

void PlutoSDRInputThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

void PlutoSDRInputThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

void PlutoSDRInputThread::run()
{
    std::ptrdiff_t p_inc = m_plutoBox->rxBufferStep();

    qDebug("PlutoSDRInputThread::run: rxBufferStep: %ld bytes", p_inc);
    qDebug("PlutoSDRInputThread::run: Rx sample size is %ld bytes", m_plutoBox->getRxSampleSize());
    qDebug("PlutoSDRInputThread::run: Tx sample size is %ld bytes", m_plutoBox->getTxSampleSize());
    qDebug("PlutoSDRInputThread::run: nominal nbytes_rx is %d bytes with 2 refills", m_blockSizeSamples*2);

    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        ssize_t nbytes_rx;
        char *p_dat, *p_end;
        int ihs; // half sample index (I then Q to make a sample)

        // Refill RX buffer
        nbytes_rx = m_plutoBox->rxBufferRefill();

        if (nbytes_rx != m_blockSizeSamples*2)
        {
            qWarning("PlutoSDRInputThread::run: error refilling buf (1) %d / %d\n",(int) nbytes_rx, (int)  m_blockSizeSamples*2);
            usleep(200000);
            continue;
        }

        // READ: Get pointers to RX buf and read IQ from RX buf port 0
        p_end = m_plutoBox->rxBufferEnd();
        ihs = 0;

        // p_inc is 2 on a char* buffer therefore each iteration processes only the I or Q sample
        // I and Q samples are processed one after the other
        // conversion is not needed as samples are little endian

        for (p_dat = m_plutoBox->rxBufferFirst(); p_dat < p_end; p_dat += p_inc)
        {
            m_buf[ihs] = *((int16_t *) p_dat);
//            iio_channel_convert(m_plutoBox->getRxChannel0(), (void *) &m_bufConv[ihs], (const void *) &m_buf[ihs]);
            ihs++;
        }

        // Refill RX buffer again - we still need twice more samples to complete since they come as I followed by Q
        nbytes_rx = m_plutoBox->rxBufferRefill();

        if (nbytes_rx != m_blockSizeSamples*2)
        {
            qWarning("PlutoSDRInputThread::run: error refilling buf (2) %d / %d\n",(int) nbytes_rx, (int)  m_blockSizeSamples*2);
            usleep(200000);
            continue;
        }

        // READ: Get pointers to RX buf and read IQ from RX buf port 0
        p_end = m_plutoBox->rxBufferEnd();

        // p_inc is 2 on a char* buffer therefore each iteration processes only the I or Q sample
        // I and Q samples are processed one after the other
        // conversion is not needed as samples are little endian

        for (p_dat = m_plutoBox->rxBufferFirst(); p_dat < p_end; p_dat += p_inc)
        {
            m_buf[ihs] = *((int16_t *) p_dat);
//            iio_channel_convert(m_plutoBox->getRxChannel0(), (void *) &m_bufConv[ihs], (const void *) &m_buf[ihs]);
            ihs++;
        }

        //m_sampleFifo->write((unsigned char *) m_buf, ihs*sizeof(int16_t));
        convert(m_buf, 2*m_blockSizeSamples); // size given in number of int16_t (I and Q interleaved)
    }

    m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void PlutoSDRInputThread::convert(const qint16* buf, qint32 len)
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

