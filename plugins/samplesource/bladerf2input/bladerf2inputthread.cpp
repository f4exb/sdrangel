///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/samplesinkfifo.h"

#include "bladerf2inputthread.h"

BladeRF2InputThread::BladeRF2InputThread(struct bladerf* dev, unsigned int nbRxChannels, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_nbChannels(nbRxChannels),
    m_iqOrder(true)
{
    qDebug("BladeRF2InputThread::BladeRF2InputThread");
    m_channels = new Channel[nbRxChannels];

    for (unsigned int i = 0; i < nbRxChannels; i++) {
        m_channels[i].m_convertBuffer.resize(DeviceBladeRF2::blockSize, Sample{0,0});
    }

    m_buf = new qint16[2*DeviceBladeRF2::blockSize*nbRxChannels];
}

BladeRF2InputThread::~BladeRF2InputThread()
{
    qDebug("BladeRF2InputThread::~BladeRF2InputThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_buf;
    delete[] m_channels;
}

void BladeRF2InputThread::startWork()
{
    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void BladeRF2InputThread::stopWork()
{
    m_running = false;
    wait();
}

void BladeRF2InputThread::run()
{
    int res;

    m_running = true;
    m_startWaiter.wakeAll();

    unsigned int nbFifos = getNbFifos();

    if ((m_nbChannels > 0) && (nbFifos > 0))
    {
        int status;

        if (m_nbChannels > 1) {
            status = bladerf_sync_config(m_dev, BLADERF_RX_X2, BLADERF_FORMAT_SC16_Q11, 64, 8192, 32, 10000);
        } else {
            status = bladerf_sync_config(m_dev, BLADERF_RX_X1, BLADERF_FORMAT_SC16_Q11, 64, 8192, 32, 10000);
        }

        if (status < 0)
        {
            qCritical("BladeRF2InputThread::run: cannot configure streams: %s", bladerf_strerror(status));
        }
        else
        {
            qDebug("BladeRF2InputThread::run: start running loop");
            while (m_running)
            {
                if (m_nbChannels > 1) {
                    res = bladerf_sync_rx(m_dev, m_buf, DeviceBladeRF2::blockSize*m_nbChannels, NULL, 10000);
                } else {
                    res = bladerf_sync_rx(m_dev, m_buf, DeviceBladeRF2::blockSize, NULL, 10000);
                }

                if (res < 0)
                {
                    qCritical("BladeRF2InputThread::run sync Rx error: %s", bladerf_strerror(res));
                    break;
                }

                if (m_nbChannels > 1)
                {
                    callbackMI(m_buf, DeviceBladeRF2::blockSize);
                }
                else
                {
                    if (m_iqOrder) {
                        callbackSIIQ(m_buf, 2*DeviceBladeRF2::blockSize);
                    } else {
                        callbackSIQI(m_buf, 2*DeviceBladeRF2::blockSize);
                    }
                }
            }
            qDebug("BladeRF2InputThread::run: stop running loop");
        }
    }
    else
    {
        qWarning("BladeRF2InputThread::run: no channels or FIFO allocated. Aborting");
    }


    m_running = false;
}

unsigned int BladeRF2InputThread::getNbFifos()
{
    unsigned int fifoCount = 0;

    for (unsigned int i = 0; i < m_nbChannels; i++)
    {
        if (m_channels[i].m_sampleFifo) {
            fifoCount++;
        }
    }

    return fifoCount;
}

void BladeRF2InputThread::setLog2Decimation(unsigned int channel, unsigned int log2_decim)
{
    if (channel < m_nbChannels) {
        m_channels[channel].m_log2Decim = log2_decim;
    }
}

unsigned int BladeRF2InputThread::getLog2Decimation(unsigned int channel) const
{
    if (channel < m_nbChannels) {
        return m_channels[channel].m_log2Decim;
    } else {
        return 0;
    }
}

void BladeRF2InputThread::setFcPos(unsigned int channel, int fcPos)
{
    if (channel < m_nbChannels) {
        m_channels[channel].m_fcPos = fcPos;
    }
}

int BladeRF2InputThread::getFcPos(unsigned int channel) const
{
    if (channel < m_nbChannels) {
        return m_channels[channel].m_fcPos;
    } else {
        return 0;
    }
}

void BladeRF2InputThread::setFifo(unsigned int channel, SampleSinkFifo *sampleFifo)
{
    if (channel < m_nbChannels) {
        m_channels[channel].m_sampleFifo = sampleFifo;
    }
}

SampleSinkFifo *BladeRF2InputThread::getFifo(unsigned int channel)
{
    if (channel < m_nbChannels) {
        return m_channels[channel].m_sampleFifo;
    } else {
        return 0;
    }
}

void BladeRF2InputThread::callbackMI(const qint16* buf, qint32 samplesPerChannel)
{
    // TODO: write a set of decimators that can take interleaved samples in input directly
    int status = bladerf_deinterleave_stream_buffer(BLADERF_RX_X2, BLADERF_FORMAT_SC16_Q11 , samplesPerChannel*m_nbChannels, (void *) buf);

    if (status < 0)
    {
        qCritical("BladeRF2InputThread::callbackMI: cannot de-interleave buffer: %s", bladerf_strerror(status));
        return;
    }

    for (unsigned int channel = 0; channel < m_nbChannels; channel++)
    {
        if (m_channels[channel].m_sampleFifo)
        {
            if (m_iqOrder) {
                callbackSIIQ(&buf[2*samplesPerChannel*channel], 2*samplesPerChannel, channel);
            } else {
                callbackSIQI(&buf[2*samplesPerChannel*channel], 2*samplesPerChannel, channel);
            }
        }
    }
}

void BladeRF2InputThread::callbackSIIQ(const qint16* buf, qint32 len, unsigned int channel)
{
    SampleVector::iterator it = m_channels[channel].m_convertBuffer.begin();

    if (m_channels[channel].m_log2Decim == 0)
    {
        m_channels[channel].m_decimatorsIQ.decimate1(&it, buf, len);
    }
    else
    {
        if (m_channels[channel].m_fcPos == 0) // Infra
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimatorsIQ.decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimatorsIQ.decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimatorsIQ.decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimatorsIQ.decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimatorsIQ.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimatorsIQ.decimate64_inf(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_channels[channel].m_fcPos == 1) // Supra
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimatorsIQ.decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimatorsIQ.decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimatorsIQ.decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimatorsIQ.decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimatorsIQ.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimatorsIQ.decimate64_sup(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_channels[channel].m_fcPos == 2) // Center
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimatorsIQ.decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimatorsIQ.decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimatorsIQ.decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimatorsIQ.decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimatorsIQ.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimatorsIQ.decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    m_channels[channel].m_sampleFifo->write(m_channels[channel].m_convertBuffer.begin(), it);
}

void BladeRF2InputThread::callbackSIQI(const qint16* buf, qint32 len, unsigned int channel)
{
    SampleVector::iterator it = m_channels[channel].m_convertBuffer.begin();

    if (m_channels[channel].m_log2Decim == 0)
    {
        m_channels[channel].m_decimatorsQI.decimate1(&it, buf, len);
    }
    else
    {
        if (m_channels[channel].m_fcPos == 0) // Infra
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimatorsQI.decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimatorsQI.decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimatorsQI.decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimatorsQI.decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimatorsQI.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimatorsQI.decimate64_inf(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_channels[channel].m_fcPos == 1) // Supra
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimatorsQI.decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimatorsQI.decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimatorsQI.decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimatorsQI.decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimatorsQI.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimatorsQI.decimate64_sup(&it, buf, len);
                break;
            default:
                break;
            }
        }
        else if (m_channels[channel].m_fcPos == 2) // Center
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimatorsQI.decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimatorsQI.decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimatorsQI.decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimatorsQI.decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimatorsQI.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimatorsQI.decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    m_channels[channel].m_sampleFifo->write(m_channels[channel].m_convertBuffer.begin(), it);
}
