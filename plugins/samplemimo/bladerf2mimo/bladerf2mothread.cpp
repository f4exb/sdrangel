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
#include "dsp/samplesourcefifo.h"

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

void BladeRF2MOThread::setLog2Interpolation(unsigned int log2_interp)
{
    m_log2Interp = log2_interp;
}

unsigned int BladeRF2MOThread::getLog2Interpolation() const
{
    return m_log2Interp;
}

void BladeRF2MOThread::setFifo(unsigned int channel, SampleSourceFifo *sampleFifo)
{
    if (channel < 2) {
        m_sampleFifo[channel] = sampleFifo;
    }
}

SampleSourceFifo *BladeRF2MOThread::getFifo(unsigned int channel)
{
    if (channel < 2) {
        return m_sampleFifo[channel];
    } else {
        return nullptr;
    }
}

void BladeRF2MOThread::callback(qint16* buf, qint32 samplesPerChannel)
{
    for (unsigned int channel = 0; channel < 2; channel++)
    {
        if (m_sampleFifo[channel]) {
           channelCallback(&buf[2*samplesPerChannel*channel], samplesPerChannel, channel);
        } else {
            std::fill(&buf[2*samplesPerChannel*channel], &buf[2*samplesPerChannel*channel]+2*samplesPerChannel, 0); // fill with zero samples
        }
    }

    int status = bladerf_interleave_stream_buffer(BLADERF_TX_X2, BLADERF_FORMAT_SC16_Q11 , samplesPerChannel*2, (void *) buf);

    if (status < 0)
    {
        qCritical("BladeRF2MOThread::callback: cannot interleave buffer: %s", bladerf_strerror(status));
        return;
    }
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16). len is a number of samples (not a number of I or Q)
void BladeRF2MOThread::channelCallback(qint16* buf, qint32 len, unsigned int channel)
{
    if (m_sampleFifo[channel])
    {
        float bal = m_sampleFifo[channel]->getRWBalance();

        if (bal < -0.25) {
            qDebug("BladeRF2MOThread::channelCallback: read lags: %f", bal);
        } else if (bal > 0.25) {
            qDebug("BladeRF2MOThread::channelCallback: read leads: %f", bal);
        }

        SampleVector::iterator beginRead;
        m_sampleFifo[channel]->readAdvance(beginRead, len/(1<<m_log2Interp));
        beginRead -= len;

        if (m_log2Interp == 0)
        {
            m_interpolators[channel].interpolate1(&beginRead, buf, len*2);
        }
        else
        {
            switch (m_log2Interp)
            {
            case 1:
                m_interpolators[channel].interpolate2_cen(&beginRead, buf, len*2);
                break;
            case 2:
                m_interpolators[channel].interpolate4_cen(&beginRead, buf, len*2);
                break;
            case 3:
                m_interpolators[channel].interpolate8_cen(&beginRead, buf, len*2);
                break;
            case 4:
                m_interpolators[channel].interpolate16_cen(&beginRead, buf, len*2);
                break;
            case 5:
                m_interpolators[channel].interpolate32_cen(&beginRead, buf, len*2);
                break;
            case 6:
                m_interpolators[channel].interpolate64_cen(&beginRead, buf, len*2);
                break;
            default:
                break;
            }
        }
    }
    else
    {
        std::fill(buf, buf+2*len, 0);
    }
}
