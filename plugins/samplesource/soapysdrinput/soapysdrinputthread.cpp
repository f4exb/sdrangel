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

#include <vector>
#include <algorithm>

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Errors.hpp>

#include "dsp/samplesinkfifo.h"
#include "soapysdr/devicesoapysdr.h"

#include "soapysdrinputthread.h"

SoapySDRInputThread::SoapySDRInputThread(SoapySDR::Device* dev, unsigned int nbRxChannels, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_sampleRate(0),
    m_nbChannels(nbRxChannels),
    m_decimatorType(DecimatorFloat)
{
    qDebug("SoapySDRInputThread::SoapySDRInputThread");
    m_channels = new Channel[nbRxChannels];
}

SoapySDRInputThread::~SoapySDRInputThread()
{
    qDebug("SoapySDRInputThread::~SoapySDRInputThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_channels;
}

void SoapySDRInputThread::startWork()
{
    if (m_running) {
        return;
    }

    m_startWaitMutex.lock();
    start();

    while(!m_running) {
        m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void SoapySDRInputThread::stopWork()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    wait();
}

void SoapySDRInputThread::run()
{
    m_running = true;
    m_startWaiter.wakeAll();

    unsigned int nbFifos = getNbFifos();

    if ((m_nbChannels > 0) && (nbFifos > 0))
    {
        // build channels list
        std::vector<std::size_t> channels(m_nbChannels);
        std::iota(channels.begin(), channels.end(), 0); // Fill with 0, 1, ..., m_nbChannels-1.

        //initialize the sample rate for all channels
        for (const auto &it : channels) {
            m_dev->setSampleRate(SOAPY_SDR_RX, it, m_sampleRate);
        }

        // Determine sample format to be used
        double fullScale(0.0);
        std::string format = m_dev->getNativeStreamFormat(SOAPY_SDR_RX, channels.front(), fullScale);

        qDebug("SoapySDRInputThread::run: format: %s fullScale: %f", format.c_str(), fullScale);

        if ((format == "CS8") && (fullScale == 128.0)) { // 8 bit signed - native
            m_decimatorType = Decimator8;
        } else if ((format == "CS16") && (fullScale == 2048.0)) { // 12 bit signed - native
            m_decimatorType = Decimator12;
        } else if ((format == "CS16") && (fullScale == 32768.0)) { // 16 bit signed - native
            m_decimatorType = Decimator16;
        } else { // for other types make a conversion to float
            m_decimatorType = DecimatorFloat;
            format = "CF32";
        }

        unsigned int elemSize = SoapySDR::formatToSize(format); // sample (I+Q) size in bytes
        SoapySDR::Stream *stream = m_dev->setupStream(SOAPY_SDR_RX, format, channels);

        //allocate buffers for the stream read/write
        const unsigned int numElems = m_dev->getStreamMTU(stream); // number of samples (I+Q)
        std::vector<std::vector<char>> buffMem(m_nbChannels, std::vector<char>(elemSize*numElems));
        std::vector<void *> buffs(m_nbChannels);

        for (std::size_t i = 0; i < m_nbChannels; i++) {
            buffs[i] = buffMem[i].data();
        }

        for (unsigned int i = 0; i < m_nbChannels; i++) {
            m_channels[i].m_convertBuffer.resize(numElems, Sample{0,0});
        }

        m_dev->activateStream(stream);
        int flags(0);
        long long timeNs(0);
        float blockTime = ((float) numElems) / (m_sampleRate <= 0 ? 1024000 : m_sampleRate);
        long initialTtimeoutUs = 10000000 * blockTime; // 10 times the block time
        long timeoutUs = initialTtimeoutUs < 250000 ? 250000 : initialTtimeoutUs; // 250ms minimum

        qDebug("SoapySDRInputThread::run: numElems: %u elemSize: %u initialTtimeoutUs: %ld timeoutUs: %ld",
                numElems, elemSize, initialTtimeoutUs, timeoutUs);
        qDebug("SoapySDRInputThread::run: start running loop");

        while (m_running)
        {
            int ret = m_dev->readStream(stream, buffs.data(), numElems, flags, timeNs, timeoutUs);

            if (ret == SOAPY_SDR_TIMEOUT)
            {
                qWarning("SoapySDRInputThread::run: timeout: flags: %d timeNs: %lld timeoutUs: %ld", flags, timeNs, timeoutUs);
            }
            else if (ret == SOAPY_SDR_OVERFLOW)
            {
                qWarning("SoapySDRInputThread::run: overflow: flags: %d timeNs: %lld timeoutUs: %ld", flags, timeNs, timeoutUs);
            }
            else if (ret < 0)
            {
                qCritical("SoapySDRInputThread::run: Unexpected read stream error: %s", SoapySDR::errToStr(ret));
                break;
            }

            if (m_nbChannels > 1)
            {
                callbackMI(buffs, numElems*2); // size given in number of I or Q samples (2 items per sample)
            }
            else
            {
                switch (m_decimatorType)
                {
                case Decimator8:
                    callbackSI8((const qint8*) buffs[0], numElems*2);
                    break;
                case Decimator12:
                    callbackSI12((const qint16*) buffs[0], numElems*2);
                    break;
                case Decimator16:
                    callbackSI16((const qint16*) buffs[0], numElems*2);
                    break;
                case DecimatorFloat:
                default:
                    callbackSIF((const float*) buffs[0], numElems*2);
                }
            }
        }

        qDebug("SoapySDRInputThread::run: stop running loop");
        m_dev->deactivateStream(stream);
        m_dev->closeStream(stream);
    }
    else
    {
        qWarning("SoapySDRInputThread::run: no channels or FIFO allocated. Aborting");
    }

    m_running = false;
}

unsigned int SoapySDRInputThread::getNbFifos()
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

void SoapySDRInputThread::setLog2Decimation(unsigned int channel, unsigned int log2_decim)
{
    if (channel < m_nbChannels) {
        m_channels[channel].m_log2Decim = log2_decim;
    }
}

unsigned int SoapySDRInputThread::getLog2Decimation(unsigned int channel) const
{
    if (channel < m_nbChannels) {
        return m_channels[channel].m_log2Decim;
    } else {
        return 0;
    }
}

void SoapySDRInputThread::setFcPos(unsigned int channel, int fcPos)
{
    if (channel < m_nbChannels) {
        m_channels[channel].m_fcPos = fcPos;
    }
}

int SoapySDRInputThread::getFcPos(unsigned int channel) const
{
    if (channel < m_nbChannels) {
        return m_channels[channel].m_fcPos;
    } else {
        return 0;
    }
}

void SoapySDRInputThread::setFifo(unsigned int channel, SampleSinkFifo *sampleFifo)
{
    if (channel < m_nbChannels) {
        m_channels[channel].m_sampleFifo = sampleFifo;
    }
}

SampleSinkFifo *SoapySDRInputThread::getFifo(unsigned int channel)
{
    if (channel < m_nbChannels) {
        return m_channels[channel].m_sampleFifo;
    } else {
        return 0;
    }
}

void SoapySDRInputThread::callbackMI(std::vector<void *>& buffs, qint32 samplesPerChannel)
{
    for(unsigned int ichan = 0; ichan < m_nbChannels; ichan++)
    {
        switch (m_decimatorType)
        {
        case Decimator8:
            callbackSI8((const qint8*) buffs[ichan], samplesPerChannel, ichan);
            break;
        case Decimator12:
            callbackSI12((const qint16*) buffs[ichan], samplesPerChannel, ichan);
            break;
        case Decimator16:
            callbackSI16((const qint16*) buffs[ichan], samplesPerChannel, ichan);
            break;
        case DecimatorFloat:
        default:
            callbackSIF((const float*) buffs[ichan], samplesPerChannel, ichan);
        }
    }
}

void SoapySDRInputThread::callbackSI8(const qint8* buf, qint32 len, unsigned int channel)
{
    SampleVector::iterator it = m_channels[channel].m_convertBuffer.begin();

    if (m_channels[channel].m_log2Decim == 0)
    {
        m_channels[channel].m_decimators8.decimate1(&it, buf, len);
    }
    else
    {
        if (m_channels[channel].m_fcPos == 0) // Infra
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimators8.decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators8.decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators8.decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators8.decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators8.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators8.decimate64_inf(&it, buf, len);
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
                m_channels[channel].m_decimators8.decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators8.decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators8.decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators8.decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators8.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators8.decimate64_sup(&it, buf, len);
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
                m_channels[channel].m_decimators8.decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators8.decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators8.decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators8.decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators8.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators8.decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    m_channels[channel].m_sampleFifo->write(m_channels[channel].m_convertBuffer.begin(), it);
}

void SoapySDRInputThread::callbackSI12(const qint16* buf, qint32 len, unsigned int channel)
{
    SampleVector::iterator it = m_channels[channel].m_convertBuffer.begin();

    if (m_channels[channel].m_log2Decim == 0)
    {
        m_channels[channel].m_decimators12.decimate1(&it, buf, len);
    }
    else
    {
        if (m_channels[channel].m_fcPos == 0) // Infra
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimators12.decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators12.decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators12.decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators12.decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators12.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators12.decimate64_inf(&it, buf, len);
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
                m_channels[channel].m_decimators12.decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators12.decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators12.decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators12.decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators12.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators12.decimate64_sup(&it, buf, len);
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
                m_channels[channel].m_decimators12.decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators12.decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators12.decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators12.decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators12.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators12.decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    m_channels[channel].m_sampleFifo->write(m_channels[channel].m_convertBuffer.begin(), it);
}

void SoapySDRInputThread::callbackSI16(const qint16* buf, qint32 len, unsigned int channel)
{
    SampleVector::iterator it = m_channels[channel].m_convertBuffer.begin();

    if (m_channels[channel].m_log2Decim == 0)
    {
        m_channels[channel].m_decimators16.decimate1(&it, buf, len);
    }
    else
    {
        if (m_channels[channel].m_fcPos == 0) // Infra
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimators16.decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators16.decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators16.decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators16.decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators16.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators16.decimate64_inf(&it, buf, len);
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
                m_channels[channel].m_decimators16.decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators16.decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators16.decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators16.decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators16.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators16.decimate64_sup(&it, buf, len);
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
                m_channels[channel].m_decimators16.decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimators16.decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimators16.decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimators16.decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimators16.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimators16.decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    m_channels[channel].m_sampleFifo->write(m_channels[channel].m_convertBuffer.begin(), it);
}

void SoapySDRInputThread::callbackSIF(const float* buf, qint32 len, unsigned int channel)
{
    SampleVector::iterator it = m_channels[channel].m_convertBuffer.begin();

    if (m_channels[channel].m_log2Decim == 0)
    {
        m_channels[channel].m_decimatorsFloat.decimate1(&it, buf, len);
    }
    else
    {
        if (m_channels[channel].m_fcPos == 0) // Infra
        {
            switch (m_channels[channel].m_log2Decim)
            {
            case 1:
                m_channels[channel].m_decimatorsFloat.decimate2_inf(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimatorsFloat.decimate4_inf(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimatorsFloat.decimate8_inf(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimatorsFloat.decimate16_inf(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimatorsFloat.decimate32_inf(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimatorsFloat.decimate64_inf(&it, buf, len);
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
                m_channels[channel].m_decimatorsFloat.decimate2_sup(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimatorsFloat.decimate4_sup(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimatorsFloat.decimate8_sup(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimatorsFloat.decimate16_sup(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimatorsFloat.decimate32_sup(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimatorsFloat.decimate64_sup(&it, buf, len);
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
                m_channels[channel].m_decimatorsFloat.decimate2_cen(&it, buf, len);
                break;
            case 2:
                m_channels[channel].m_decimatorsFloat.decimate4_cen(&it, buf, len);
                break;
            case 3:
                m_channels[channel].m_decimatorsFloat.decimate8_cen(&it, buf, len);
                break;
            case 4:
                m_channels[channel].m_decimatorsFloat.decimate16_cen(&it, buf, len);
                break;
            case 5:
                m_channels[channel].m_decimatorsFloat.decimate32_cen(&it, buf, len);
                break;
            case 6:
                m_channels[channel].m_decimatorsFloat.decimate64_cen(&it, buf, len);
                break;
            default:
                break;
            }
        }
    }

    m_channels[channel].m_sampleFifo->write(m_channels[channel].m_convertBuffer.begin(), it);
}
