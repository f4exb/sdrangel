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

#include "plutosdr/deviceplutosdrbox.h"
#include "plutosdrinputsettings.h"
#include "plutosdrinputthread.h"

PlutoSDRInputThread::PlutoSDRInputThread(uint32_t blocksize, DevicePlutoSDRBox* plutoBox, SampleSinkFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_plutoBox(plutoBox),
    m_blockSize(blocksize),
    m_convertBuffer(1<<15),
    m_sampleFifo(sampleFifo),
    m_convertIt(m_convertBuffer.begin()),
    m_log2Decim(0),
    m_fcPos(PlutoSDRInputSettings::FC_POS_CENTER),
    m_phasor(0)
{
    m_buf = new qint16[blocksize*(sizeof(Sample)/sizeof(qint16))];
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
    m_running = true;
    m_startWaiter.wakeAll();

    while (m_running)
    {
        ssize_t nbytes_rx;
        char *p_dat, *p_end;
        std::ptrdiff_t p_inc;
        int is;

        // Refill RX buffer
        nbytes_rx = m_plutoBox->rxBufferRefill();
        if (nbytes_rx < 0) { qWarning("PlutoSDRInputThread::run: error refilling buf %d\n",(int) nbytes_rx); break; }

        // READ: Get pointers to RX buf and read IQ from RX buf port 0
        p_inc = m_plutoBox->rxBufferStep();
        p_end = m_plutoBox->rxBufferEnd();
        is = 0;

        for (p_dat = m_plutoBox->rxBufferFirst(); p_dat < p_end; p_dat += p_inc)
        {
            memcpy(&m_buf[2*is], p_dat, 2*sizeof(int16_t));
            is++;
        }

        m_sampleFifo->write((unsigned char *) m_buf, is*2*sizeof(int16_t));
    }

    m_running = false;
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void PlutoSDRInputThread::convert()
{
    if (m_log2Decim == 0)
    {
        m_decimators.decimate1(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
    }
    else
    {
        if (m_fcPos == 0) // Infra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimators.decimate2_inf(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 2:
                m_decimators.decimate4_inf(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 3:
                m_decimators.decimate8_inf(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 4:
                m_decimators.decimate16_inf(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 5:
                m_decimators.decimate32_inf(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 6:
                m_decimators.decimate64_inf(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
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
                m_decimators.decimate2_sup(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 2:
                m_decimators.decimate4_sup(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 3:
                m_decimators.decimate8_sup(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 4:
                m_decimators.decimate16_sup(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 5:
                m_decimators.decimate32_sup(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 6:
                m_decimators.decimate64_sup(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
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
                m_decimators.decimate2_cen(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 2:
                m_decimators.decimate4_cen(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 3:
                m_decimators.decimate8_cen(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 4:
                m_decimators.decimate16_cen(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 5:
                m_decimators.decimate32_cen(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            case 6:
                m_decimators.decimate64_cen(&m_convertIt, m_buf, 2*(1<<m_log2Decim));
                break;
            default:
                break;
            }
        }
    }

    ++m_convertIt;

    if (m_convertIt == m_convertBuffer.end())
    {
        m_sampleFifo->write(m_convertBuffer.begin(), m_convertBuffer.end());
        m_convertIt = m_convertBuffer.begin();
    }
}

