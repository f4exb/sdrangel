///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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

#include "plutosdr/deviceplutosdrbox.h"
#include "dsp/samplemififo.h"

#include "plutosdrmimosettings.h"
#include "plutosdrmithread.h"

PlutoSDRMIThread::PlutoSDRMIThread(DevicePlutoSDRBox* plutoBox, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_plutoBox(plutoBox),
    m_sampleFifo(nullptr),
    m_iqOrder(true)
{
    qDebug("PlutoSDRMIThread::PlutoSDRMIThread");
    m_buf[0] = new qint16[2*m_plutoSDRBlockSizeSamples];
    m_buf[1] = new qint16[2*m_plutoSDRBlockSizeSamples];

    for (unsigned int i = 0; i < 2; i++) {
        m_convertBuffer[i].resize(m_plutoSDRBlockSizeSamples, Sample{0,0});
    }
}

PlutoSDRMIThread::~PlutoSDRMIThread()
{
    qDebug("PlutoSDRMIThread::~PlutoSDRMIThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_buf[0];
    delete[] m_buf[1];
}

void PlutoSDRMIThread::startWork()
{
    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void PlutoSDRMIThread::stopWork()
{
    m_running = false;
    wait();
}

void PlutoSDRMIThread::setLog2Decimation(unsigned int log2Decim)
{
    m_log2Decim = log2Decim;
}

unsigned int PlutoSDRMIThread::getLog2Decimation() const
{
    return m_log2Decim;
}

void PlutoSDRMIThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

int PlutoSDRMIThread::getFcPos() const
{
    return m_fcPos;
}

void PlutoSDRMIThread::run()
{
    std::ptrdiff_t p_inc = m_plutoBox->rxBufferStep();
    int sampleSize = 2*m_plutoBox->getRxSampleBytes(); // I/Q sample size in bytes
    int nbChan = p_inc / sampleSize; // number of I/Q channels

    qDebug("PlutoSDRMOThread::run: nbChan: %d", nbChan);
    qDebug("PlutoSDRMOThread::run: I+Q bytes %d", sampleSize);
    qDebug("PlutoSDRMIThread::run: rxBufferStep: %ld bytes", p_inc);
    qDebug("PlutoSDRMIThread::run: Rx all samples size is %ld bytes", m_plutoBox->getRxSampleSize());
    qDebug("PlutoSDRMIThread::run: Tx all samples size is %ld bytes", m_plutoBox->getTxSampleSize());
    qDebug("PlutoSDRMIThread::run: nominal nbytes_rx is %ld bytes with 1 refill", m_plutoSDRBlockSizeSamples*p_inc);

    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        ssize_t nbytes_rx;
        char *p_dat, *p_end;
        int ihs; // half sample index (I then Q to make a sample)

        // Refill RX buffer
        nbytes_rx = m_plutoBox->rxBufferRefill();

        if (nbytes_rx != m_plutoSDRBlockSizeSamples*p_inc)
        {
            qWarning("PlutoSDRMIThread::run: error refilling buf %d / %ld", (int) nbytes_rx, (int)  m_plutoSDRBlockSizeSamples*p_inc);
            usleep(200000);
            continue;
        }

        // READ: Get pointers to RX buf and read IQ from RX buf port 0
        p_dat = m_plutoBox->rxBufferFirst();
        p_end = m_plutoBox->rxBufferEnd();
        ihs = 0;

        // p_inc is 8 on a char* buffer therefore each iteration processes a couple of IQ samples,
        // I and Q each being two bytes
        // conversion is not needed as samples are little endian

        for (; p_dat < p_end; p_dat += p_inc, ihs += 2)
        {
            m_buf[0][ihs]   = *(((int16_t *) p_dat) + 0);
            m_buf[0][ihs+1] = *(((int16_t *) p_dat) + 1);

            if (nbChan == 1)
            {
                m_buf[1][ihs]   = 0;
                m_buf[1][ihs+1] = 0;
            }
            else if (nbChan == 2)
            {
                m_buf[1][ihs]   = *(((int16_t *) p_dat) + 2);
                m_buf[1][ihs+1] = *(((int16_t *) p_dat) + 3);
            }
        }

        std::vector<SampleVector::const_iterator> vbegin;
        int lengths[2];

        for (unsigned int channel = 0; channel < 2; channel++)
        {
            if (m_iqOrder) {
                lengths[channel] = channelCallbackIQ(m_buf[channel], 2*m_plutoSDRBlockSizeSamples, channel);
            } else {
                lengths[channel] = channelCallbackQI(m_buf[channel], 2*m_plutoSDRBlockSizeSamples, channel);
            }

            vbegin.push_back(m_convertBuffer[channel].begin());
        }

        if (lengths[0] == lengths[1])
        {
            m_sampleFifo->writeSync(vbegin, lengths[0]);
        }
        else
        {
            qWarning("PlutoSDRMIThread::run: unequal channel lengths: [0]=%d [1]=%d", lengths[0], lengths[1]);
            m_sampleFifo->writeSync(vbegin, (std::min)(lengths[0], lengths[1]));
        }
    }

    m_running = false;
}

int PlutoSDRMIThread::channelCallbackIQ(const qint16* buf, qint32 len, int channel)
{
    SampleVector::iterator it = m_convertBuffer[channel].begin();

    if (m_log2Decim == 0)
    {
        m_decimatorsIQ[channel].decimate1(&it, buf, len);
    }
    else
    {
        if (m_fcPos == 0) // Infra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsIQ[channel].decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_decimatorsIQ[channel].decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_decimatorsIQ[channel].decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_decimatorsIQ[channel].decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_decimatorsIQ[channel].decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_decimatorsIQ[channel].decimate64_inf(&it, buf, len);
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
                m_decimatorsIQ[channel].decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_decimatorsIQ[channel].decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_decimatorsIQ[channel].decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_decimatorsIQ[channel].decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_decimatorsIQ[channel].decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_decimatorsIQ[channel].decimate64_sup(&it, buf, len);
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
                m_decimatorsIQ[channel].decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_decimatorsIQ[channel].decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_decimatorsIQ[channel].decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_decimatorsIQ[channel].decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_decimatorsIQ[channel].decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_decimatorsIQ[channel].decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    return it - m_convertBuffer[channel].begin();
}

int PlutoSDRMIThread::channelCallbackQI(const qint16* buf, qint32 len, int channel)
{
    SampleVector::iterator it = m_convertBuffer[channel].begin();

    if (m_log2Decim == 0)
    {
        m_decimatorsQI[channel].decimate1(&it, buf, len);
    }
    else
    {
        if (m_fcPos == 0) // Infra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimatorsQI[channel].decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_decimatorsQI[channel].decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_decimatorsQI[channel].decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_decimatorsQI[channel].decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_decimatorsQI[channel].decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_decimatorsQI[channel].decimate64_inf(&it, buf, len);
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
                m_decimatorsQI[channel].decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_decimatorsQI[channel].decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_decimatorsQI[channel].decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_decimatorsQI[channel].decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_decimatorsQI[channel].decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_decimatorsQI[channel].decimate64_sup(&it, buf, len);
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
                m_decimatorsQI[channel].decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_decimatorsQI[channel].decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_decimatorsQI[channel].decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_decimatorsQI[channel].decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_decimatorsQI[channel].decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_decimatorsQI[channel].decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    return it - m_convertBuffer[channel].begin();
}
