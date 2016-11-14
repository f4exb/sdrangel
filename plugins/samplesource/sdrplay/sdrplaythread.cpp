///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <stdio.h>
#include <errno.h>
#include "sdrplaythread.h"
#include "dsp/samplesinkfifo.h"

SDRPlayThread *SDRPlayThread::m_this = 0;

SDRPlayThread::SDRPlayThread(SampleSinkFifo* sampleFifo, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_convertBuffer(SDRPLAY_INIT_NBSAMPLES),
    m_sampleFifo(sampleFifo),
    m_log2Decim(0),
    m_fcPos(0)
{
    m_this = this;
}

SDRPlayThread::~SDRPlayThread()
{
    stopWork();
    m_this = 0;
}

void SDRPlayThread::startWork()
{
    m_startWaitMutex.lock();
    start();
    while(!m_running)
        m_startWaiter.wait(&m_startWaitMutex, 100);
    m_startWaitMutex.unlock();
}

void SDRPlayThread::stopWork()
{
    m_running = false;
    wait();
}

void SDRPlayThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

void SDRPlayThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

void SDRPlayThread::streamCallback(
        short *xi,
        short *xq,
        unsigned int firstSampleNum,
        int grChanged,
        int rfChanged,
        int fsChanged,
        unsigned int numSamples,
        unsigned int reset,
        void *cbContext)
{
    qDebug("SDRPlayThread::streamCallback");
    m_this->callback(xi, xq, numSamples);
}

void SDRPlayThread::callback(short *xi, short *xq, unsigned int numSamples)
{
    qDebug("SDRPlayThread::callback");
    if (m_convertBuffer.size() < numSamples)
    {
        m_convertBuffer.resize(numSamples);
    }

    SampleVector::iterator it = m_convertBuffer.begin();

    if (m_log2Decim == 0)
    {
        m_decimators.decimate1(&it, xi, xq, numSamples);
    }
    else
    {
        if (m_fcPos == 0) // Infra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimators.decimate2_inf(&it, xi, xq, numSamples);
                break;
            case 2:
                m_decimators.decimate4_inf(&it, xi, xq, numSamples);
                break;
            case 3:
                m_decimators.decimate8_inf(&it, xi, xq, numSamples);
                break;
            case 4:
                m_decimators.decimate16_inf(&it, xi, xq, numSamples);
                break;
            case 5:
                m_decimators.decimate32_inf(&it, xi, xq, numSamples);
                break;
            case 6:
                m_decimators.decimate64_inf(&it, xi, xq, numSamples);
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
                m_decimators.decimate2_sup(&it, xi, xq, numSamples);
                break;
            case 2:
                m_decimators.decimate4_sup(&it, xi, xq, numSamples);
                break;
            case 3:
                m_decimators.decimate8_sup(&it, xi, xq, numSamples);
                break;
            case 4:
                m_decimators.decimate16_sup(&it, xi, xq, numSamples);
                break;
            case 5:
                m_decimators.decimate32_sup(&it, xi, xq, numSamples);
                break;
            case 6:
                m_decimators.decimate64_sup(&it, xi, xq, numSamples);
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
                m_decimators.decimate2_cen(&it, xi, xq, numSamples);
                break;
            case 2:
                m_decimators.decimate4_cen(&it, xi, xq, numSamples);
                break;
            case 3:
                m_decimators.decimate8_cen(&it, xi, xq, numSamples);
                break;
            case 4:
                m_decimators.decimate16_cen(&it, xi, xq, numSamples);
                break;
            case 5:
                m_decimators.decimate32_cen(&it, xi, xq, numSamples);
                break;
            case 6:
                m_decimators.decimate64_cen(&it, xi, xq, numSamples);
                break;
            default:
                break;
            }
        }
    }

    m_sampleFifo->write(m_convertBuffer.begin(), it);
}

void SDRPlayThread::run()
{
    int res;

    m_running = true;
    m_startWaiter.wakeAll();

    while(m_running)
    {
        sleep(1);
    }

    m_running = false;
}


