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

#include "bladerf2/devicebladerf2shared.h"
#include "dsp/samplemofifo.h"

#include "bladerf2mothread.h"

BladeRF2MOThread::BladeRF2MOThread(struct bladerf* dev, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_log2Interp(0)
{
    qDebug("BladeRF2MOThread::BladeRF2MOThread");
    m_buf = new qint16[2*DeviceBladeRF2::blockSize*2];
}

BladeRF2MOThread::~BladeRF2MOThread()
{
    qDebug("BladeRF2MOThread::~BladeRF2MOThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_buf;
}

void BladeRF2MOThread::startWork()
{
    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void BladeRF2MOThread::stopWork()
{
    m_running = false;
    wait();
}

void BladeRF2MOThread::run()
{
    int res;

    m_running = true;
    m_startWaiter.wakeAll();

    int status;

    status = bladerf_sync_config(m_dev, BLADERF_TX_X2, BLADERF_FORMAT_SC16_Q11, 128, 16384, 32, 1500);

    if (status < 0)
    {
        qCritical("BladeRF2MOThread::run: cannot configure streams: %s", bladerf_strerror(status));
    }
    else
    {
        qDebug("BladeRF2MOThread::run: start running loop");

        while (m_running)
        {
            callback(m_buf, DeviceBladeRF2::blockSize);
            res = bladerf_sync_tx(m_dev, m_buf, DeviceBladeRF2::blockSize*2, 0, 1500);

            if (res < 0)
            {
                qCritical("BladeRF2MOThread::run sync Rx error: %s", bladerf_strerror(res));
                break;
            }
        }

        qDebug("BladeRF2MOThread::run: stop running loop");
    }

    m_running = false;
}

void BladeRF2MOThread::setLog2Interpolation(unsigned int log2Interp)
{
    qDebug("BladeRF2MOThread::setLog2Interpolation: %u", log2Interp);
    m_log2Interp = log2Interp;
}

unsigned int BladeRF2MOThread::getLog2Interpolation() const
{
    return m_log2Interp;
}

void BladeRF2MOThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

int BladeRF2MOThread::getFcPos() const
{
    return m_fcPos;
}

void BladeRF2MOThread::callback(qint16* buf, qint32 samplesPerChannel)
{
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    m_sampleFifo->readSync(samplesPerChannel/(1<<m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

    if (iPart1Begin != iPart1End)
    {
        callbackPart(buf, (iPart1End - iPart1Begin)*(1<<m_log2Interp), iPart1Begin);
    }

    if (iPart2Begin != iPart2End)
    {
        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_log2Interp);
        callbackPart(buf + 2*shift, (iPart2End - iPart2Begin)*(1<<m_log2Interp), iPart2Begin);
    }

    int status = bladerf_interleave_stream_buffer(BLADERF_TX_X2, BLADERF_FORMAT_SC16_Q11 , samplesPerChannel*2, (void *) buf);

    if (status < 0)
    {
        qCritical("BladeRF2MOThread::callback: cannot interleave buffer: %s", bladerf_strerror(status));
        return;
    }
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16). len is a number of samples (not a number of I or Q)
void BladeRF2MOThread::callbackPart(qint16* buf, qint32 nSamples, int iBegin)
{
    for (unsigned int channel = 0; channel < 2; channel++)
    {
        SampleVector::iterator begin = m_sampleFifo->getData(channel).begin() + iBegin;

        if (m_log2Interp == 0)
        {
            m_interpolators[channel].interpolate1(&begin, &buf[channel*2*nSamples], 2*nSamples);
        }
        else
        {
            if (m_fcPos == 0) // Infra
            {
                switch (m_log2Interp)
                {
                case 1:
                    m_interpolators[channel].interpolate2_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_inf(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                default:
                    break;
                }
            }
            else if (m_fcPos == 1) // Supra
            {
                switch (m_log2Interp)
                {
                case 1:
                    m_interpolators[channel].interpolate2_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_sup(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                default:
                    break;
                }
            }
            else if (m_fcPos == 2) // Center
            {
                switch (m_log2Interp)
                {
                case 1:
                    m_interpolators[channel].interpolate2_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 2:
                    m_interpolators[channel].interpolate4_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 3:
                    m_interpolators[channel].interpolate8_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 4:
                    m_interpolators[channel].interpolate16_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 5:
                    m_interpolators[channel].interpolate32_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                case 6:
                    m_interpolators[channel].interpolate64_cen(&begin, &buf[channel*2*nSamples], 2*nSamples);
                    break;
                default:
                    break;
                }
            }
        }
    }
}
