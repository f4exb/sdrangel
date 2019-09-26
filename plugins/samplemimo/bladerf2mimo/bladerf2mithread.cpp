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
#include "dsp/samplemififo.h"

#include "bladerf2mithread.h"

BladeRF2MIThread::BladeRF2MIThread(struct bladerf* dev, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_sampleFifo(nullptr)
{
    qDebug("BladeRF2MIThread::BladeRF2MIThread");
    m_buf = new qint16[2*DeviceBladeRF2::blockSize*2];

    for (unsigned int i = 0; i < 2; i++) {
        m_convertBuffer[i].resize(DeviceBladeRF2::blockSize, Sample{0,0});
    }
}

BladeRF2MIThread::~BladeRF2MIThread()
{
    qDebug("BladeRF2MIThread::~BladeRF2MIThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_buf;
}

void BladeRF2MIThread::startWork()
{
    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void BladeRF2MIThread::stopWork()
{
    m_running = false;
    wait();
}

void BladeRF2MIThread::setLog2Decimation(unsigned int log2_decim)
{
    m_log2Decim = log2_decim;
}

unsigned int BladeRF2MIThread::getLog2Decimation() const
{
    return m_log2Decim;
}

void BladeRF2MIThread::setFcPos(int fcPos)
{
    m_fcPos = fcPos;
}

int BladeRF2MIThread::getFcPos() const
{
    return m_fcPos;
}

void BladeRF2MIThread::run()
{
    int res;

    m_running = true;
    m_startWaiter.wakeAll();

    int status = bladerf_sync_config(m_dev, BLADERF_RX_X2, BLADERF_FORMAT_SC16_Q11, 64, 8192, 32, 10000);

    if (status < 0)
    {
        qCritical("BladeRF2MIThread::run: cannot configure streams: %s", bladerf_strerror(status));
    }
    else
    {
        qDebug("BladeRF2MIThread::run: start running loop");

        while (m_running)
        {
            res = bladerf_sync_rx(m_dev, m_buf, DeviceBladeRF2::blockSize*2, nullptr, 1500);

            if (res < 0)
            {
                qCritical("BladeRF2MIThread::run sync Rx error: %s", bladerf_strerror(res));
                break;
            }

            callback(m_buf, DeviceBladeRF2::blockSize);
        }

        qDebug("BladeRF2MIThread::run: stop running loop");
        m_running = false;
    }
}

void BladeRF2MIThread::callback(const qint16* buf, qint32 samplesPerChannel)
{
    int status = bladerf_deinterleave_stream_buffer(BLADERF_RX_X2, BLADERF_FORMAT_SC16_Q11 , samplesPerChannel*2, (void *) buf);

    if (status < 0)
    {
        qCritical("BladeRF2MIThread::callback: cannot de-interleave buffer: %s", bladerf_strerror(status));
        return;
    }

    std::vector<SampleVector::const_iterator> vbegin;

    for (unsigned int channel = 0; channel < 2; channel++)
    {
        channelCallback(&buf[2*samplesPerChannel*channel], 2*samplesPerChannel, channel);
        vbegin.push_back(m_convertBuffer[channel].begin());
    }

    m_sampleFifo->writeSync(vbegin, samplesPerChannel/(1<<m_log2Decim));
}

void BladeRF2MIThread::channelCallback(const qint16* buf, qint32 len, int channel)
{
    SampleVector::iterator it = m_convertBuffer[channel].begin();

    if (m_log2Decim == 0)
    {
        m_decimators[channel].decimate1(&it, buf, len);
    }
    else
    {
        if (m_fcPos == 0) // Infra
        {
            switch (m_log2Decim)
            {
            case 1:
                m_decimators[channel].decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_decimators[channel].decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_decimators[channel].decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_decimators[channel].decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_decimators[channel].decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_decimators[channel].decimate64_inf(&it, buf, len);
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
                m_decimators[channel].decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_decimators[channel].decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_decimators[channel].decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_decimators[channel].decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_decimators[channel].decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_decimators[channel].decimate64_sup(&it, buf, len);
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
                m_decimators[channel].decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_decimators[channel].decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_decimators[channel].decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_decimators[channel].decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_decimators[channel].decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_decimators[channel].decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }
}