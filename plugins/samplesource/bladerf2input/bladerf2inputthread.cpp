///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "bladerf2inputthread.h"

Bladerf2InputThread::Bladerf2InputThread(struct bladerf* dev, unsigned int nbRxChannels, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_nbChannels(nbRxChannels)
{
    m_channels = new Channel[nbRxChannels];
    m_buf = new qint16[2*DeviceBladeRF2::blockSize*nbRxChannels];
}

Bladerf2InputThread::~Bladerf2InputThread()
{
    if (m_running) {
        stopWork();
    }

    delete[] m_buf;
    delete[] m_channels;
}

void Bladerf2InputThread::startWork()
{
    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void Bladerf2InputThread::stopWork()
{
    m_running = false;
    wait();
}

void Bladerf2InputThread::run()
{
    int res;

    m_running = true;
    m_startWaiter.wakeAll();

    unsigned int nbFifos = getNbFifos();

    if (nbFifos > 0)
    {
        int status;

        if (nbFifos > 1) {
            status = bladerf_sync_config(m_dev, BLADERF_RX_X2, BLADERF_FORMAT_SC16_Q11, 64, 8192, 32, 10000);
        } else {
            status = bladerf_sync_config(m_dev, BLADERF_RX_X1, BLADERF_FORMAT_SC16_Q11, 64, 8192, 32, 10000);
        }

        if (status < 0)
        {
            qCritical("Bladerf2InputThread::run: cannot configure streams: %s", bladerf_strerror(status));
        }
        else
        {
            while (m_running)
            {
                res = bladerf_sync_rx(m_dev, m_buf, DeviceBladeRF2::blockSize, NULL, 10000);

                if (res < 0)
                {
                    qCritical("BladerfThread::run sync Rx error: %s", bladerf_strerror(res));
                    break;
                }

                if (nbFifos > 1) {
                    callbackMI(m_buf, DeviceBladeRF2::blockSize);
                } else {
                    callbackSI(m_buf, 2*DeviceBladeRF2::blockSize);
                }
            }
        }
    }
    else
    {
        qWarning("Bladerf2InputThread::run: no sample FIFOs registered. Aborting");
    }


    m_running = false;
}

unsigned int Bladerf2InputThread::getNbFifos()
{
    unsigned int fifoCount = 0;

    for (int i = 0; i < m_nbChannels; i++)
    {
        if (m_channels[i].m_sampleFifo) {
            fifoCount++;
        }
    }

    return fifoCount;
}

void Bladerf2InputThread::setLog2Decimation(unsigned int channel, unsigned int log2_decim)
{
    if ((channel >= 0) && (channel < m_nbChannels)) {
        m_channels[channel].m_log2Decim = log2_decim;
    }
}

void Bladerf2InputThread::setFcPos(unsigned int channel, int fcPos)
{
    if ((channel >= 0) && (channel < m_nbChannels)) {
        m_channels[channel].m_fcPos = fcPos;
    }
}

void Bladerf2InputThread::setFifo(unsigned int channel, SampleSinkFifo *sampleFifo)
{
    if ((channel >= 0) && (channel < m_nbChannels)) {
        m_channels[channel].m_sampleFifo = sampleFifo;
    }
}

SampleSinkFifo *Bladerf2InputThread::getFifo(unsigned int channel)
{
    if ((channel >= 0) && (channel < m_nbChannels)) {
        return m_channels[channel].m_sampleFifo;
    } else {
        return 0;
    }
}

void Bladerf2InputThread::callbackMI(const qint16* buf, qint32 samplesPerChannel)
{
    // TODO: write a set of decimators that can take interleaved samples in input directly
    int status = bladerf_deinterleave_stream_buffer(BLADERF_RX_X2, BLADERF_FORMAT_SC16_Q11 , samplesPerChannel*m_nbChannels, (void *) buf);

    if (status < 0)
    {
        qCritical("Bladerf2InputThread::callbackMI: cannot de-interleave buffer: %s", bladerf_strerror(status));
        return;
    }

    for (unsigned int channel = 0; channel < m_nbChannels; channel++)
    {
        if (m_channels[channel].m_sampleFifo) {
            callbackSI(&buf[2*samplesPerChannel*channel], 2*samplesPerChannel, channel);
        }
    }
}

void Bladerf2InputThread::callbackSI(const qint16* buf, qint32 len, unsigned int channel)
{
    SampleVector::iterator it = m_channels[channel].m_convertBuffer.begin();

    if (m_channels[channel].m_log2Decim == 0)
    {
        m_channels[channel].m_decimators.decimate1(&it, buf, len);
    }
    else
    {
        if (m_channels[channel].m_fcPos == 0) // Infra
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimators.decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators.decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators.decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators.decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators.decimate64_inf(&it, buf, len);
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
                m_channels[channel].m_decimators.decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators.decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators.decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators.decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators.decimate64_sup(&it, buf, len);
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
                m_channels[channel].m_decimators.decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators.decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators.decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators.decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators.decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    m_channels[channel].m_sampleFifo->write(m_channels[channel].m_convertBuffer.begin(), it);
}

