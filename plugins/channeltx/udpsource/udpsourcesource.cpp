///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017-2019 Edouard Griffiths, F4EXB                              //
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

#include <QDebug>

#include "dsp/dspcommands.h"
#include "dsp/basebandsamplesink.h"
#include "util/db.h"

#include "udpsourcesource.h"
#include "udpsourcemsg.h"

UDPSourceSource::UDPSourceSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_squelch(1e-6),
    m_spectrumSink(nullptr),
    m_spectrumChunkSize(2160),
    m_spectrumChunkCounter(0),
    m_magsq(1e-10),
    m_movingAverage(16, 1e-10),
    m_inMovingAverage(480, 1e-10),
    m_sampleRateSum(0),
    m_sampleRateAvgCounter(0),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
    m_levelNbSamples(480),
    m_squelchOpen(false),
    m_squelchOpenCount(0),
    m_squelchCloseCount(0),
    m_squelchThreshold(4800),
    m_modPhasor(0.0f),
    m_SSBFilterBufferIndex(0)
{
    m_SSBFilter = new fftfilt(m_settings.m_lowCutoff / m_settings.m_inputSampleRate, m_settings.m_rfBandwidth / m_settings.m_inputSampleRate, m_ssbFftLen);
    m_SSBFilterBuffer = new Complex[m_ssbFftLen>>1]; // filter returns data exactly half of its size
    m_magsq = 0.0;

    m_udpHandler.start();

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

UDPSourceSource::~UDPSourceSource()
{
    m_udpHandler.stop();
    delete m_SSBFilter;
    delete[] m_SSBFilterBuffer;
}

void UDPSourceSource::setUDPFeedbackMessageQueue(MessageQueue *messageQueue)
{
    m_udpHandler.setFeedbackMessageQueue(messageQueue);
}

void UDPSourceSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void UDPSourceSource::pullOne(Sample& sample)
{
    if (m_settings.m_channelMute)
    {
        sample.m_real = 0.0f;
        sample.m_imag = 0.0f;
        initSquelch(false);
        return;
    }

    Complex ci;

    if (m_interpolatorDistance > 1.0f) // decimate
    {
        modulateSample();

        while (!m_interpolator.decimate(&m_interpolatorDistanceRemain, m_modSample, &ci))
        {
            modulateSample();
        }
    }
    else
    {
        if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_modSample, &ci))
        {
            modulateSample();
        }
    }

    m_interpolatorDistanceRemain += m_interpolatorDistance;

    ci *= m_carrierNco.nextIQ(); // shift to carrier frequency
    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
    m_movingAverage.feed(magsq);
    m_magsq = m_movingAverage.average();

    sample.m_real = (FixReal) ci.real();
    sample.m_imag = (FixReal) ci.imag();
}

void UDPSourceSource::modulateSample()
{
    if (m_settings.m_sampleFormat == UDPSourceSettings::FormatSnLE) // Linear I/Q transponding
    {
        Sample s;

        m_udpHandler.readSample(s);

        uint64_t magsq = s.m_real * s.m_real + s.m_imag * s.m_imag;
        m_inMovingAverage.feed(magsq/(SDR_TX_SCALED*SDR_TX_SCALED));
        m_inMagsq = m_inMovingAverage.average();

        calculateSquelch(m_inMagsq);

        if (m_squelchOpen)
        {
            m_modSample.real(s.m_real * m_settings.m_gainOut);
            m_modSample.imag(s.m_imag * m_settings.m_gainOut);
            calculateLevel(m_modSample);
        }
        else
        {
            m_modSample.real(0.0f);
            m_modSample.imag(0.0f);
        }
    }
    else if (m_settings.m_sampleFormat == UDPSourceSettings::FormatNFM)
    {
        qint16 t;
        readMonoSample(t);

        m_inMovingAverage.feed((t*t)/1073741824.0);
        m_inMagsq = m_inMovingAverage.average();

        calculateSquelch(m_inMagsq);

        if (m_squelchOpen)
        {
            m_modPhasor += (m_settings.m_fmDeviation / m_settings.m_inputSampleRate) * (t / SDR_TX_SCALEF) * M_PI * 2.0f;
            m_modSample.real(cos(m_modPhasor) * 0.3162292f * SDR_TX_SCALEF * m_settings.m_gainOut);
            m_modSample.imag(sin(m_modPhasor) * 0.3162292f * SDR_TX_SCALEF * m_settings.m_gainOut);
            calculateLevel(m_modSample);
        }
        else
        {
            m_modSample.real(0.0f);
            m_modSample.imag(0.0f);
        }
    }
    else if (m_settings.m_sampleFormat == UDPSourceSettings::FormatAM)
    {
        qint16 t;
        readMonoSample(t);
        m_inMovingAverage.feed((t*t)/(SDR_TX_SCALED*SDR_TX_SCALED));
        m_inMagsq = m_inMovingAverage.average();

        calculateSquelch(m_inMagsq);

        if (m_squelchOpen)
        {
            m_modSample.real(((t / SDR_TX_SCALEF)*m_settings.m_amModFactor*m_settings.m_gainOut + 1.0f) * (SDR_TX_SCALEF/2)); // modulate and scale zero frequency carrier
            m_modSample.imag(0.0f);
            calculateLevel(m_modSample);
        }
        else
        {
            m_modSample.real(0.0f);
            m_modSample.imag(0.0f);
        }
    }
    else if ((m_settings.m_sampleFormat == UDPSourceSettings::FormatLSB) || (m_settings.m_sampleFormat == UDPSourceSettings::FormatUSB))
    {
        qint16 t;
        Complex c, ci;
        fftfilt::cmplx *filtered;
        int n_out = 0;

        readMonoSample(t);
        m_inMovingAverage.feed((t*t)/(SDR_TX_SCALED*SDR_TX_SCALED));
        m_inMagsq = m_inMovingAverage.average();

        calculateSquelch(m_inMagsq);

        if (m_squelchOpen)
        {
            ci.real((t / SDR_TX_SCALEF) * m_settings.m_gainOut);
            ci.imag(0.0f);

            n_out = m_SSBFilter->runSSB(ci, &filtered, (m_settings.m_sampleFormat == UDPSourceSettings::FormatUSB));

            if (n_out > 0)
            {
                memcpy((void *) m_SSBFilterBuffer, (const void *) filtered, n_out*sizeof(Complex));
                m_SSBFilterBufferIndex = 0;
            }

            c = m_SSBFilterBuffer[m_SSBFilterBufferIndex];
            m_modSample.real(m_SSBFilterBuffer[m_SSBFilterBufferIndex].real() * SDR_TX_SCALEF);
            m_modSample.imag(m_SSBFilterBuffer[m_SSBFilterBufferIndex].imag() * SDR_TX_SCALEF);
            m_SSBFilterBufferIndex++;

            calculateLevel(m_modSample);
        }
        else
        {
            m_modSample.real(0.0f);
            m_modSample.imag(0.0f);
        }
    }
    else
    {
        m_modSample.real(0.0f);
        m_modSample.imag(0.0f);
        initSquelch(false);
    }

    if (m_spectrumSink)
    {
        Sample s;
        s.m_real = (FixReal) m_modSample.real();
        s.m_imag = (FixReal) m_modSample.imag();
        m_sampleBuffer.push_back(s);
        m_spectrumChunkCounter++;

        if (m_spectrumChunkCounter == m_spectrumChunkSize)
        {
            m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), false);
            m_sampleBuffer.clear();
            m_spectrumChunkCounter = 0;
        }
    }
}

void UDPSourceSource::calculateLevel(Real sample)
{
    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        m_rmsLevel = m_levelSum > 0.0 ? sqrt(m_levelSum / m_levelNbSamples) : 0.0;
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void UDPSourceSource::calculateLevel(Complex sample)
{
    Real t = std::abs(sample);

    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), t);
        m_levelSum += (t * t);
        m_levelCalcCount++;
    }
    else
    {
        m_rmsLevel = m_levelSum > 0.0 ? sqrt((m_levelSum/(SDR_TX_SCALED*SDR_TX_SCALED)) / m_levelNbSamples) : 0.0;
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void UDPSourceSource::resetReadIndex()
{
    m_udpHandler.resetReadIndex();
}

void UDPSourceSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "UDPSourceSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if (((channelSampleRate != m_channelSampleRate) && (!m_settings.m_autoRWBalance)) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_settings.m_inputSampleRate / (Real) channelSampleRate;
        m_interpolator.create(48, m_settings.m_inputSampleRate, m_settings.m_rfBandwidth / 2.2, 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void UDPSourceSource::applySettings(const UDPSourceSettings& settings, bool force)
{
    qDebug() << "UDPSourceSource::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_sampleFormat: " << settings.m_sampleFormat
            << " m_inputSampleRate: " << settings.m_inputSampleRate
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_lowCutoff: " << settings.m_lowCutoff
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_amModFactor: " << settings.m_amModFactor
            << " m_udpAddressStr: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " m_multicastAddress: " << settings.m_multicastAddress
            << " m_multicastJoin: " << settings.m_multicastJoin
            << " m_channelMute: " << settings.m_channelMute
            << " m_gainIn: " << settings.m_gainIn
            << " m_gainOut: " << settings.m_gainOut
            << " m_squelchGate: " << settings.m_squelchGate
            << " m_squelch: " << settings.m_squelch << "dB"
            << " m_squelchEnabled: " << settings.m_squelchEnabled
            << " m_autoRWBalance: " << settings.m_autoRWBalance
            << " m_stereoInput: " << settings.m_stereoInput
            << " force: " << force;

    if((settings.m_rfBandwidth != m_settings.m_rfBandwidth) ||
       (settings.m_lowCutoff != m_settings.m_lowCutoff) ||
       (settings.m_inputSampleRate != m_settings.m_inputSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) settings.m_inputSampleRate / (Real) m_channelSampleRate;
        m_interpolator.create(48, settings.m_inputSampleRate, settings.m_rfBandwidth / 2.2, 3.0);
        m_actualInputSampleRate = settings.m_inputSampleRate;
        m_udpHandler.resetReadIndex();
        m_sampleRateSum = 0.0;
        m_sampleRateAvgCounter = 0;
        m_spectrumChunkSize = settings.m_inputSampleRate * 0.05; // 50 ms chunk
        m_spectrumChunkCounter = 0;
        m_levelNbSamples = settings.m_inputSampleRate * 0.01; // every 10 ms
        m_levelCalcCount = 0;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_udpHandler.resizeBuffer(settings.m_inputSampleRate);
        m_inMovingAverage.resize(settings.m_inputSampleRate * 0.01, 1e-10); // 10 ms
        m_squelchThreshold = settings.m_inputSampleRate * settings.m_squelchGate;
        initSquelch(m_squelchOpen);
        m_SSBFilter->create_filter(settings.m_lowCutoff / settings.m_inputSampleRate, settings.m_rfBandwidth / settings.m_inputSampleRate);
    }

    if ((settings.m_squelch != m_settings.m_squelch) || force)
    {
        m_squelch = CalcDb::powerFromdB(settings.m_squelch);
    }

    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force)
    {
        m_squelchThreshold = m_channelSampleRate * settings.m_squelchGate;
        initSquelch(m_squelchOpen);
    }

    if ((settings.m_udpAddress != m_settings.m_udpAddress) ||
        (settings.m_udpPort != m_settings.m_udpPort) ||
        (settings.m_multicastAddress != m_settings.m_multicastAddress) ||
        (settings.m_multicastJoin != m_settings.m_multicastJoin) || force)
    {
        m_udpHandler.configureUDPLink(settings.m_udpAddress, settings.m_udpPort, settings.m_multicastAddress, settings.m_multicastJoin);
    }

    if ((settings.m_channelMute != m_settings.m_channelMute) || force)
    {
        if (!settings.m_channelMute) {
            m_udpHandler.resetReadIndex();
        }
    }

    if ((settings.m_autoRWBalance != m_settings.m_autoRWBalance) || force)
    {
        m_udpHandler.setAutoRWBalance(settings.m_autoRWBalance);

        if (!settings.m_autoRWBalance)
        {
            m_interpolatorDistanceRemain = 0;
            m_interpolatorConsumed = false;
            m_interpolatorDistance = (Real) settings.m_inputSampleRate / (Real) m_channelSampleRate;
            m_interpolator.create(48, settings.m_inputSampleRate, settings.m_rfBandwidth / 2.2, 3.0);
            m_actualInputSampleRate = settings.m_inputSampleRate;
            m_udpHandler.resetReadIndex();
        }
    }

    m_settings = settings;
}

void UDPSourceSource::sampleRateCorrection(float rawDeltaRatio, float correctionFactor)
{
    float newSampleRate = m_actualInputSampleRate + correctionFactor * m_actualInputSampleRate;

    // exclude values too way out nominal sample rate (20%)
    if ((newSampleRate < m_settings.m_inputSampleRate * 1.2) && (newSampleRate >  m_settings.m_inputSampleRate * 0.8))
    {
        m_actualInputSampleRate = newSampleRate;

        if ((rawDeltaRatio > -0.05) && (rawDeltaRatio < 0.05))
        {
            if (m_sampleRateAvgCounter < m_sampleRateAverageItems)
            {
                m_sampleRateSum += m_actualInputSampleRate;
                m_sampleRateAvgCounter++;
            }
        }
        else
        {
            m_sampleRateSum = 0.0;
            m_sampleRateAvgCounter = 0;
        }

        if (m_sampleRateAvgCounter == m_sampleRateAverageItems)
        {
            float avgRate = m_sampleRateSum / m_sampleRateAverageItems;
            qDebug("UDPSourceSource::sampleRateCorrection: corr: %+.6f new rate: %.0f: avg rate: %.0f",
                    correctionFactor,
                    m_actualInputSampleRate,
                    avgRate);
            m_actualInputSampleRate = avgRate;
            m_sampleRateSum = 0.0;
            m_sampleRateAvgCounter = 0;
        }

        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_actualInputSampleRate / (Real) m_channelSampleRate;
    }
}