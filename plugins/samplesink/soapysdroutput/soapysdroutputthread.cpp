///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <algorithm>

#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Errors.hpp>

#include "dsp/samplesourcefifo.h"

#include "soapysdroutputthread.h"

SoapySDROutputThread::SoapySDROutputThread(SoapySDR::Device* dev, unsigned int nbTxChannels, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dev(dev),
    m_sampleRate(0),
    m_nbChannels(nbTxChannels),
    m_interpolatorType(InterpolatorFloat)
{
    qDebug("SoapySDROutputThread::SoapySDROutputThread");
    m_channels = new Channel[nbTxChannels];
}

SoapySDROutputThread::~SoapySDROutputThread()
{
    qDebug("SoapySDROutputThread::~SoapySDROutputThread");

    if (m_running) {
        stopWork();
    }

    delete[] m_channels;
}

void SoapySDROutputThread::startWork()
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

void SoapySDROutputThread::stopWork()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    wait();
}

void SoapySDROutputThread::run()
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
        qDebug("SoapySDROutputThread::run: m_sampleRate: %u", m_sampleRate);
        for (const auto &it : channels) {
            m_dev->setSampleRate(SOAPY_SDR_TX, it, m_sampleRate);
        }

        // Determine sample format to be used
        double fullScale(0.0);
        std::string format = m_dev->getNativeStreamFormat(SOAPY_SDR_TX, channels.front(), fullScale);

        qDebug("SoapySDROutputThread::run: format: %s fullScale: %f", format.c_str(), fullScale);

        if ((format == "CS8") && (fullScale == 128.0)) { // 8 bit signed - native
            m_interpolatorType = Interpolator8;
        } else if ((format == "CS16") && (fullScale == 2048.0)) { // 12 bit signed - native
            m_interpolatorType = Interpolator12;
        } else if ((format == "CS16") && (fullScale == 32768.0)) { // 16 bit signed - native
            m_interpolatorType = Interpolator16;
        } else { // for other types make a conversion to float
            m_interpolatorType = InterpolatorFloat;
            format = "CF32";
        }

        unsigned int elemSize = SoapySDR::formatToSize(format); // sample (I+Q) size in bytes
        SoapySDR::Stream *stream = m_dev->setupStream(SOAPY_SDR_TX, format, channels);

        //allocate buffers for the stream read/write
        const unsigned int numElems = m_dev->getStreamMTU(stream); // number of samples (I+Q)
        std::vector<std::vector<char>> buffMem(m_nbChannels, std::vector<char>(elemSize*numElems));
        std::vector<void *> buffs(m_nbChannels);

        for (std::size_t i = 0; i < m_nbChannels; i++) {
            buffs[i] = buffMem[i].data();
        }

        m_dev->activateStream(stream);
        int flags(0);
        long long timeNs(0);
        float blockTime = ((float) numElems) / (m_sampleRate <= 0 ? 1024000 : m_sampleRate);
        long initialTtimeoutUs = 10000000 * blockTime; // 10 times the block time
        long timeoutUs = initialTtimeoutUs < 250000 ? 250000 : initialTtimeoutUs; // 250ms minimum

        qDebug("SoapySDROutputThread::run: numElems: %u elemSize: %u initialTtimeoutUs: %ld  timeoutUs: %ld",
                numElems, elemSize, initialTtimeoutUs, timeoutUs);
        qDebug("SoapySDROutputThread::run: start running loop");

        while (m_running)
        {
            int ret = m_dev->writeStream(stream, buffs.data(), numElems, flags, timeNs, timeoutUs);

            if (ret == SOAPY_SDR_TIMEOUT)
            {
                qWarning("SoapySDROutputThread::run: timeout: flags: %d timeNs: %lld timeoutUs: %ld", flags, timeNs, timeoutUs);
            }
            else if (ret == SOAPY_SDR_OVERFLOW)
            {
                qWarning("SoapySDROutputThread::run: overflow: flags: %d timeNs: %lld timeoutUs: %ld", flags, timeNs, timeoutUs);
            }
            else if (ret < 0)
            {
                qCritical("SoapySDROutputThread::run: Unexpected write stream error: %s", SoapySDR::errToStr(ret));
                break;
            }

            if (m_nbChannels > 1)
            {
                callbackMO(buffs, numElems); // size given in number of samples (1 item per sample)
            }
            else
            {
                switch (m_interpolatorType)
                {
                case Interpolator8:
                    callbackSO8((qint8*) buffs[0], numElems);
                    break;
                case Interpolator12:
                    callbackSO12((qint16*) buffs[0], numElems);
                    break;
                case Interpolator16:
                    callbackSO16((qint16*) buffs[0], numElems);
                    break;
                case InterpolatorFloat:
                default:
                    callbackSOIF((float*) buffs[0], numElems);
                    break;
                }
            }
        }

        qDebug("SoapySDROutputThread::run: stop running loop");
        m_dev->deactivateStream(stream);
        m_dev->closeStream(stream);

    }
    else
    {
        qWarning("SoapySDROutputThread::run: no channels or FIFO allocated. Aborting");
    }

    m_running = false;
}

unsigned int SoapySDROutputThread::getNbFifos()
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

void SoapySDROutputThread::setLog2Interpolation(unsigned int channel, unsigned int log2_interp)
{
    if (channel < m_nbChannels) {
        m_channels[channel].m_log2Interp = log2_interp;
    }
}

unsigned int SoapySDROutputThread::getLog2Interpolation(unsigned int channel) const
{
    if (channel < m_nbChannels) {
        return m_channels[channel].m_log2Interp;
    } else {
        return 0;
    }
}

void SoapySDROutputThread::setFifo(unsigned int channel, SampleSourceFifo *sampleFifo)
{
    if (channel < m_nbChannels) {
        m_channels[channel].m_sampleFifo = sampleFifo;
    }
}

SampleSourceFifo *SoapySDROutputThread::getFifo(unsigned int channel)
{
    if (channel < m_nbChannels) {
        return m_channels[channel].m_sampleFifo;
    } else {
        return 0;
    }
}

void SoapySDROutputThread::callbackMO(std::vector<void *>& buffs, qint32 samplesPerChannel)
{
    for(unsigned int ichan = 0; ichan < m_nbChannels; ichan++)
    {
        if (m_channels[ichan].m_sampleFifo)
        {
            switch (m_interpolatorType)
            {
            case Interpolator8:
                callbackSO8((qint8*) buffs[ichan], samplesPerChannel, ichan);
                break;
            case Interpolator12:
                callbackSO12((qint16*) buffs[ichan], samplesPerChannel, ichan);
                break;
            case Interpolator16:
                callbackSO16((qint16*) buffs[ichan], samplesPerChannel, ichan);
                break;
            case InterpolatorFloat:
            default:
                // TODO
                break;
            }
        }
        else // no FIFO for this channel means channel is unused: fill with zeros
        {
            switch (m_interpolatorType)
            {
            case Interpolator8:
                std::fill((qint8*) buffs[ichan], (qint8*) buffs[ichan] + 2*samplesPerChannel, 0);
                break;
            case Interpolator12:
            case Interpolator16:
                std::fill((qint16*) buffs[ichan], (qint16*) buffs[ichan] + 2*samplesPerChannel, 0);
                break;
            case InterpolatorFloat:
            default:
                std::fill((float*) buffs[ichan], (float*) buffs[ichan] + 2*samplesPerChannel, 0.0f);
                break;
            }
        }
    }
}

//  Interpolate according to specified log2 (ex: log2=4 => decim=16). len is a number of samples (not a number of I or Q)

void SoapySDROutputThread::callbackSO8(qint8* buf, qint32 len, unsigned int channel)
{
    if (m_channels[channel].m_sampleFifo)
    {
        SampleVector& data = m_channels[channel].m_sampleFifo->getData();
        unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
        m_channels[channel].m_sampleFifo->read(len/(1<<m_channels[channel].m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

        if (iPart1Begin != iPart1End) {
            callbackPart8(buf, data, iPart1Begin, iPart1End, channel);
        }

        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_channels[channel].m_log2Interp);

        if (iPart2Begin != iPart2End) {
            callbackPart8(buf + 2*shift, data, iPart2Begin, iPart2End, channel);
        }
    }
    else
    {
        std::fill(buf, buf+2*len, 0);
    }
}

void SoapySDROutputThread::callbackPart8(qint8* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd, unsigned int channel)
{
    SampleVector::iterator beginRead = data.begin() + iBegin;
    int len = 2*(iEnd - iBegin)*(1<<m_channels[channel].m_log2Interp);

    if (m_channels[channel].m_log2Interp == 0)
    {
        m_channels[channel].m_interpolators8.interpolate1(&beginRead, buf, len);
    }
    else
    {
        switch (m_channels[channel].m_log2Interp)
        {
        case 1:
            m_channels[channel].m_interpolators8.interpolate2_cen(&beginRead, buf, len);
            break;
        case 2:
            m_channels[channel].m_interpolators8.interpolate4_cen(&beginRead, buf, len);
            break;
        case 3:
            m_channels[channel].m_interpolators8.interpolate8_cen(&beginRead, buf, len);
            break;
        case 4:
            m_channels[channel].m_interpolators8.interpolate16_cen(&beginRead, buf, len);
            break;
        case 5:
            m_channels[channel].m_interpolators8.interpolate32_cen(&beginRead, buf, len);
            break;
        case 6:
            m_channels[channel].m_interpolators8.interpolate64_cen(&beginRead, buf, len);
            break;
        default:
            break;
        }
    }
}

void SoapySDROutputThread::callbackSO12(qint16* buf, qint32 len, unsigned int channel)
{
    if (m_channels[channel].m_sampleFifo)
    {
        SampleVector& data = m_channels[channel].m_sampleFifo->getData();
        unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
        m_channels[channel].m_sampleFifo->read(len/(1<<m_channels[channel].m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

        if (iPart1Begin != iPart1End) {
            callbackPart12(buf, data, iPart1Begin, iPart1End, channel);
        }

        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_channels[channel].m_log2Interp);

        if (iPart2Begin != iPart2End) {
            callbackPart12(buf + 2*shift, data, iPart2Begin, iPart2End, channel);
        }
    }
    else
    {
        std::fill(buf, buf+2*len, 0);
    }
}

void SoapySDROutputThread::callbackPart12(qint16* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd, unsigned int channel)
{
    SampleVector::iterator beginRead = data.begin() + iBegin;
    int len = 2*(iEnd - iBegin)*(1<<m_channels[channel].m_log2Interp);

    if (m_channels[channel].m_log2Interp == 0)
    {
        m_channels[channel].m_interpolators12.interpolate1(&beginRead, buf, len);
    }
    else
    {
        switch (m_channels[channel].m_log2Interp)
        {
        case 1:
            m_channels[channel].m_interpolators12.interpolate2_cen(&beginRead, buf, len);
            break;
        case 2:
            m_channels[channel].m_interpolators12.interpolate4_cen(&beginRead, buf, len);
            break;
        case 3:
            m_channels[channel].m_interpolators12.interpolate8_cen(&beginRead, buf, len);
            break;
        case 4:
            m_channels[channel].m_interpolators12.interpolate16_cen(&beginRead, buf, len);
            break;
        case 5:
            m_channels[channel].m_interpolators12.interpolate32_cen(&beginRead, buf, len);
            break;
        case 6:
            m_channels[channel].m_interpolators12.interpolate64_cen(&beginRead, buf, len);
            break;
        default:
            break;
        }
    }
}

void SoapySDROutputThread::callbackSO16(qint16* buf, qint32 len, unsigned int channel)
{
    if (m_channels[channel].m_sampleFifo)
    {
        SampleVector& data = m_channels[channel].m_sampleFifo->getData();
        unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
        m_channels[channel].m_sampleFifo->read(len/(1<<m_channels[channel].m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

        if (iPart1Begin != iPart1End) {
            callbackPart16(buf, data, iPart1Begin, iPart1End, channel);
        }

        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_channels[channel].m_log2Interp);

        if (iPart2Begin != iPart2End) {
            callbackPart16(buf + 2*shift, data, iPart2Begin, iPart2End, channel);
        }
    }
    else
    {
        std::fill(buf, buf+2*len, 0);
    }
}

void SoapySDROutputThread::callbackPart16(qint16* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd, unsigned int channel)
{
    SampleVector::iterator beginRead = data.begin() + iBegin;
    int len = 2*(iEnd - iBegin)*(1<<m_channels[channel].m_log2Interp);

    if (m_channels[channel].m_log2Interp == 0)
    {
        m_channels[channel].m_interpolators16.interpolate1(&beginRead, buf, len);
    }
    else
    {
        switch (m_channels[channel].m_log2Interp)
        {
        case 1:
            m_channels[channel].m_interpolators16.interpolate2_cen(&beginRead, buf, len);
            break;
        case 2:
            m_channels[channel].m_interpolators16.interpolate4_cen(&beginRead, buf, len);
            break;
        case 3:
            m_channels[channel].m_interpolators16.interpolate8_cen(&beginRead, buf, len);
            break;
        case 4:
            m_channels[channel].m_interpolators16.interpolate16_cen(&beginRead, buf, len);
            break;
        case 5:
            m_channels[channel].m_interpolators16.interpolate32_cen(&beginRead, buf, len);
            break;
        case 6:
            m_channels[channel].m_interpolators16.interpolate64_cen(&beginRead, buf, len);
            break;
        default:
            break;
        }
    }
}

void SoapySDROutputThread::callbackSOIF(float* buf, qint32 len, unsigned int channel)
{
    if (m_channels[channel].m_sampleFifo)
    {
        SampleVector& data = m_channels[channel].m_sampleFifo->getData();
        unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
        m_channels[channel].m_sampleFifo->read(len/(1<<m_channels[channel].m_log2Interp), iPart1Begin, iPart1End, iPart2Begin, iPart2End);

        if (iPart1Begin != iPart1End) {
            callbackPartF(buf, data, iPart1Begin, iPart1End, channel);
        }

        unsigned int shift = (iPart1End - iPart1Begin)*(1<<m_channels[channel].m_log2Interp);

        if (iPart2Begin != iPart2End) {
            callbackPartF(buf + 2*shift, data, iPart2Begin, iPart2End, channel);
        }
    }
    else
    {
        std::fill(buf, buf+2*len, 0.0f);
    }
}

void SoapySDROutputThread::callbackPartF(float* buf, SampleVector& data, unsigned int iBegin, unsigned int iEnd, unsigned int channel)
{
    SampleVector::iterator beginRead = data.begin() + iBegin;
    int len = 2*(iEnd - iBegin)*(1<<m_channels[channel].m_log2Interp);

    if (m_channels[channel].m_log2Interp == 0)
    {
        m_channels[channel].m_interpolatorsIF.interpolate1(&beginRead, buf, len);
    }
    else
    {
        switch (m_channels[channel].m_log2Interp)
        {
        case 1:
            m_channels[channel].m_interpolatorsIF.interpolate2_cen(&beginRead, buf, len);
            break;
        case 2:
            m_channels[channel].m_interpolatorsIF.interpolate4_cen(&beginRead, buf, len);
            break;
        case 3:
            m_channels[channel].m_interpolatorsIF.interpolate8_cen(&beginRead, buf, len);
            break;
        case 4:
            m_channels[channel].m_interpolatorsIF.interpolate16_cen(&beginRead, buf, len);
            break;
        case 5:
            m_channels[channel].m_interpolatorsIF.interpolate32_cen(&beginRead, buf, len);
            break;
        case 6:
            m_channels[channel].m_interpolatorsIF.interpolate64_cen(&beginRead, buf, len);
            break;
        default:
            break;
        }
    }
}
