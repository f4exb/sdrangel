///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <cctype>
#include <QDebug>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QVariant>

#include "dsp/basebandsamplesink.h"
#include "dsp/scopevis.h"
#include "ieee_802_15_4_modsource.h"
#include "util/crc.h"

MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_ModSource::MsgCloseUDP, Message)
MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_ModSource::MsgOpenUDP, Message)

IEEE_802_15_4_ModSource::IEEE_802_15_4_ModSource() :
    m_channelSampleRate(3000000),
    m_channelFrequencyOffset(0),
    m_spectrumRate(0),
    m_sinLUT(nullptr),
    m_scrambler(0x108, 0x1fe, 1),
    m_spectrumSink(nullptr),
    m_scopeSink(nullptr),
    m_specSampleBufferIndex(0),
    m_scopeSampleBufferIndex(0),
    m_magsq(0.0),
    m_levelCalcCount(0),
    m_peakLevel(0.0f),
    m_levelSum(0.0f),
    m_sampleIdx(0),
    m_chipsPerSymbol(15),
    m_bitsPerSymbol(1),
    m_chipRate(300000),
    m_state(idle),
    m_byteIdx(0),
    m_bitIdx(0),
    m_bitCount(0),
    m_udpSocket(nullptr)
{
    m_lowpass.create(301, m_channelSampleRate, 22000.0 / 2.0);
    m_pulseShapeI.create(1, 6, m_channelSampleRate/300000, true);
    m_pulseShapeQ.create(1, 6, m_channelSampleRate/300000, true);
    m_specSampleBuffer.resize(m_specSampleBufferSize);
    m_scopeSampleBuffer.resize(m_scopeSampleBufferSize);
    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

IEEE_802_15_4_ModSource::~IEEE_802_15_4_ModSource()
{
    closeUDP();
    delete[] m_sinLUT;
}

void IEEE_802_15_4_ModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void IEEE_802_15_4_ModSource::pullOne(Sample& sample)
{
    if (m_settings.m_channelMute)
    {
        sample.m_real = 0.0f;
        sample.m_imag = 0.0f;
        return;
    }

    // Calculate next sample
    modulateSample();

    // Shift to carrier frequency
    Complex ci = m_modSample;
    ci *= m_carrierNco.nextIQ();

    // Calculate power
    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();

    // Convert from float to fixed point
    sample.m_real = (FixReal) (ci.real() * SDR_TX_SCALEF);
    sample.m_imag = (FixReal) (ci.imag() * SDR_TX_SCALEF);
}

void IEEE_802_15_4_ModSource::sampleToSpectrum(Complex sample)
{
    if (m_spectrumSink && (m_settings.m_spectrumRate > 0))
    {
        Complex out;

        // Could use a simpler filter here, as currently m_spectrumRate is
        // always an integer multiple of m_channelSampleRate
        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, sample, &out))
        {
            Real r = std::real(out) * SDR_TX_SCALEF;
            Real i = std::imag(out) * SDR_TX_SCALEF;
            m_specSampleBuffer[m_specSampleBufferIndex++] = Sample(r, i);

            if (m_specSampleBufferIndex == m_specSampleBufferSize)
            {
                m_spectrumSink->feed(m_specSampleBuffer.begin(), m_specSampleBuffer.end(), false);
                m_specSampleBufferIndex = 0;
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
    }
}

void IEEE_802_15_4_ModSource::sampleToScope(Complex sample)
{
    if (m_scopeSink)
    {
        Real r = std::real(sample) * SDR_RX_SCALEF;
        Real i = std::imag(sample) * SDR_RX_SCALEF;
        m_scopeSampleBuffer[m_scopeSampleBufferIndex++] = Sample(r, i);

        if (m_scopeSampleBufferIndex == m_scopeSampleBufferSize)
        {
            std::vector<SampleVector::const_iterator> vbegin;
            vbegin.push_back(m_scopeSampleBuffer.begin());
            m_scopeSink->feed(vbegin, m_scopeSampleBufferSize);
            m_scopeSampleBufferIndex = 0;
        }
    }
}

void IEEE_802_15_4_ModSource::modulateSample()
{
    Real linearRampGain;
    Real i, q;

    if ((m_state == idle) || (m_state == wait))
    {
        Real audioMod = 0.0f;
        m_modSample.real(audioMod);
        m_modSample.imag(0);
        calculateLevel(audioMod);
        sampleToSpectrum(m_modSample);
        sampleToScope(m_modSample);
        if (m_state == wait)
        {
            m_waitCounter--;
            if (m_waitCounter == 0)
                initTX();
        }
    }
    else
    {

        if (m_sampleIdx == 0)
        {
            if (chipsValid())
                m_chips[m_chipOdd] = getChip();
            // Should we start ramping down power?
            if ((m_bitCount < m_settings.m_rampDownBits) || ((m_bitCount == 0) && !m_settings.m_rampDownBits))
            {
                m_state = ramp_down;
                if (m_settings.m_rampDownBits > 0)
                    m_powRamp = -m_settings.m_rampRange/(m_settings.m_rampDownBits * (Real)m_samplesPerChip);
            }
        }

        if (!m_settings.m_bbNoise)
        {
            if (m_settings.m_modulation == IEEE_802_15_4_ModSettings::BPSK)
            {
                // BPSK - Raised cosine pulse shaping
                if ((m_sampleIdx == 1) && (m_state != ramp_down))
                    i = m_pulseShapeI.filter(m_chips[0] ? 1.0f : -1.0f);
                else
                    i = m_pulseShapeI.filter(0.0f);
                q = 0.0f;
            }
            else
            {
                if (m_settings.m_pulseShaping == IEEE_802_15_4_ModSettings::SINE)
                {
                    // O-QPSK - Half-sine pulse shaping over 2 chips. Even chips on I, odd on Q. 1-chip out of phase.
                    i = (m_chips[0] ? 1.0f : -1.0f) * m_sinLUT[m_sampleIdx+(m_chipOdd ? m_samplesPerChip : 0)];
                    q = (m_chips[1] ? 1.0f : -1.0f) * m_sinLUT[m_sampleIdx+(m_chipOdd ? 0 : m_samplesPerChip)];
                }
                else
                {
                    // O-QPSK - Raised cosine pulse shaping. Even chips on I, odd on Q. 1-chip out of phase.
                    if ((m_sampleIdx == 1) && (m_state != ramp_down) && !m_chipOdd)
                        i = m_pulseShapeI.filter(m_chips[0] ? 1.0f : -1.0f);
                    else
                        i = m_pulseShapeI.filter(0.0f);
                    if ((m_sampleIdx == 1) && (m_state != ramp_down) && m_chipOdd)
                        q = m_pulseShapeQ.filter(m_chips[1] ? 1.0f : -1.0f);
                    else
                        q = m_pulseShapeQ.filter(0.0f);
                }
            }
        }
        else
        {
            i = (Real)rand()/((Real)RAND_MAX)-0.5; // Noise to test filter frequency response
            q = (Real)rand()/((Real)RAND_MAX)-0.5;
        }

        if (m_basebandFile.is_open())
            m_basebandFile << m_chips[0] << "," << m_chips[1] << "," << m_chipOdd << "," << i << "," << q << "," << (m_sampleIdx+(m_chipOdd ? m_samplesPerChip : 0)) << "," << (m_sampleIdx+(m_chipOdd ? 0 : m_samplesPerChip)) << "\n";

        m_sampleIdx++;
        if (m_sampleIdx >= m_samplesPerChip)
        {
            m_sampleIdx = 0;
            if (m_settings.m_modulation == IEEE_802_15_4_ModSettings::OQPSK)
                m_chipOdd = !m_chipOdd;
        }


        linearRampGain = powf(10.0f, m_pow/20.0f);

        m_modSample.real(m_linearGain * linearRampGain * i);
        m_modSample.imag(m_linearGain * linearRampGain * q);

        // Display baseband audio in spectrum analyser
        sampleToSpectrum(m_modSample);
        sampleToScope(m_modSample);

        // Apply low pass filter to limit RF BW
        m_modSample = m_lowpass.filter(m_modSample);

        // Ramp up/down power at start/end of frame
        if ((m_state == ramp_up) || (m_state == ramp_down))
        {
            m_pow += m_powRamp;
            if ((m_state == ramp_up) && (m_pow >= 0.0f))
            {
                // Finished ramp up, transmit at full gain
                m_state = tx;
                m_pow = 0.0f;
            }
            else if ((m_state == ramp_down) && (   (m_settings.m_rampRange == 0)
                                                || (m_settings.m_rampDownBits == 0)
                                                || (m_pow <= -(Real)m_settings.m_rampRange)
                                               ))
            {
                m_state = idle;
                // Do we need to retransmit the frame?
                if (m_settings.m_repeat)
                {
                    if (m_frameRepeatCount > 0)
                        m_frameRepeatCount--;
                    if ((m_frameRepeatCount == IEEE_802_15_4_ModSettings::infinitePackets) || (m_frameRepeatCount > 0))
                    {
                        if (m_settings.m_repeatDelay > 0.0f)
                        {
                            // Wait before retransmitting
                            m_state = wait;
                            m_waitCounter = m_settings.m_repeatDelay * m_channelSampleRate;
                        }
                        else
                        {
                            // Retransmit immediately
                            initTX();
                        }
                    }
                }
            }
        }

        Real s = std::real(m_modSample);
        calculateLevel(s);
    }
}

void IEEE_802_15_4_ModSource::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        m_rmsLevel = sqrt(m_levelSum / m_levelNbSamples);
        m_peakLevelOut = m_peakLevel;
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void IEEE_802_15_4_ModSource::applySettings(const IEEE_802_15_4_ModSettings& settings, bool force)
{
    // Only recreate filters if settings have changed
    if ((settings.m_lpfTaps != m_settings.m_lpfTaps) || (settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        qDebug() << "IEEE_802_15_4_ModSource::applySettings: Creating new lpf with taps " << settings.m_lpfTaps << " rfBW " << settings.m_rfBandwidth;
        m_lowpass.create(settings.m_lpfTaps, m_channelSampleRate, settings.m_rfBandwidth / 2.0);
    }
    if ((settings.m_spectrumRate != m_settings.m_spectrumRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) settings.m_spectrumRate;
        m_interpolator.create(48, settings.m_spectrumRate, settings.m_spectrumRate / 2.2, 3.0);
    }

    if (settings.m_modulation == IEEE_802_15_4_ModSettings::BPSK)
    {
        m_chipsPerSymbol = 15;
        m_bitsPerSymbol = 1;
    }
    else
    {
        m_bitsPerSymbol = 4;
        m_chipsPerSymbol = settings.m_subGHzBand ? 16 : 32;
    }

    m_chipRate = settings.m_bitRate * m_chipsPerSymbol / m_bitsPerSymbol;
    m_samplesPerChip = m_channelSampleRate / m_chipRate;
    qDebug() << "m_samplesPerChip: " << m_samplesPerChip;

    if (m_channelSampleRate % m_chipRate != 0) {
        qCritical("Sample rate is not an integer multiple of the chip rate");
    }

    if (m_samplesPerChip <= 2) {
        qCritical("Sample rate is not a high enough multiple of the chip rate");
    }

    if ((settings.m_pulseShaping != m_settings.m_pulseShaping)
        || (settings.m_beta != m_settings.m_beta)
        || (settings.m_symbolSpan != m_settings.m_symbolSpan)
        || (settings.m_bitRate != m_settings.m_bitRate)
        || (settings.m_modulation != m_settings.m_modulation)
        || (settings.m_subGHzBand != m_settings.m_subGHzBand)
        || force)
    {
        qDebug() << "IEEE_802_15_4_ModSource::applySettings: Recreating pulse shaping filter: "
                << " pulseShaping: " << m_settings.m_pulseShaping
                << " beta: " << settings.m_beta
                << " symbolSpan: " << settings.m_symbolSpan
                << " channelSampleRate:" << m_channelSampleRate
                << " subGHzBand: " << settings.m_subGHzBand
                << " bitRate:" << settings.m_bitRate
                << " chipRate:" << m_chipRate;

        if (settings.m_pulseShaping == IEEE_802_15_4_ModSettings::RC)
        {
            m_pulseShapeI.create(settings.m_beta, m_settings.m_symbolSpan, m_channelSampleRate/m_chipRate, true);
            m_pulseShapeQ.create(settings.m_beta, m_settings.m_symbolSpan, m_channelSampleRate/m_chipRate, true);
        }
        else
        {
             createHalfSine(m_channelSampleRate, m_chipRate);
        }
    }

    if ((settings.m_polynomial != m_settings.m_polynomial) || force) {
        m_scrambler.setPolynomial(settings.m_polynomial);
    }

    m_settings = settings;

    // Precalculate linear gain to save doing it in the loop
    m_linearGain = powf(10.0f,  m_settings.m_gain/20.0f);
}

void IEEE_802_15_4_ModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "IEEE_802_15_4_ModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset
            << " rfBandwidth: " << m_settings.m_rfBandwidth
            << " spectrumRate: " << m_settings.m_spectrumRate;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        qDebug() << "IEEE_802_15_4_ModSource::applyChannelSettings: Recreating filters";
        m_lowpass.create(m_settings.m_lpfTaps, channelSampleRate, m_settings.m_rfBandwidth / 2.0);
        qDebug() << "IEEE_802_15_4_ModSource::applyChannelSettings: Recreating pulse shaping filter: "
                << " pulseShaping: " << m_settings.m_pulseShaping
                << " beta: " << m_settings.m_beta
                << " symbolSpan: " << m_settings.m_symbolSpan
                << " channelSampleRate:" << channelSampleRate
                << " subGHzBand: " << m_settings.m_subGHzBand
                << " bitRate:" << m_settings.m_bitRate
                << " chipRate:" << m_chipRate;
        if (m_settings.m_pulseShaping == IEEE_802_15_4_ModSettings::RC)
        {
            m_pulseShapeI.create(m_settings.m_beta, m_settings.m_symbolSpan, channelSampleRate/m_chipRate, true);
            m_pulseShapeQ.create(m_settings.m_beta, m_settings.m_symbolSpan, channelSampleRate/m_chipRate, true);
        }
        else
            createHalfSine(channelSampleRate, m_chipRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || (m_spectrumRate != m_settings.m_spectrumRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_settings.m_spectrumRate;
        m_interpolator.create(48, m_settings.m_spectrumRate, m_settings.m_spectrumRate / 2.2, 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_spectrumRate = m_settings.m_spectrumRate;
    m_samplesPerChip = m_channelSampleRate / m_chipRate;
    qDebug() << "m_samplesPerChip: " << m_samplesPerChip;
}

// Half-sine pulse shaping for O-QPSK
void IEEE_802_15_4_ModSource::createHalfSine(int sampleRate, int chipRate)
{
    int samplesPerChip = sampleRate / chipRate;
    double tc = 1.0 / chipRate;

    delete[] m_sinLUT;
    m_sinLUT = new double[2*samplesPerChip];
    for (int i = 0; i < 2*samplesPerChip; i++)
    {
        double t=i/(double)sampleRate;
        m_sinLUT[i] = sin(M_PI*t/(2.0*tc));
    }
}

bool IEEE_802_15_4_ModSource::chipsValid()
{
    return (m_bitCount > 0) || (m_chipIdx < m_chipsPerSymbol);
}

// Symbol-to-chip mapping
int IEEE_802_15_4_ModSource::getChip()
{
    int chip = 0;

    if (m_chipIdx == 0)
        m_symbol = getSymbol();

    if (m_settings.m_bitRate <= 40000)
    {
        static const int chipsBpsk[2][15] = {
            {1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0},
            {0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1}
        };
        chip = chipsBpsk[m_symbol][m_chipIdx];
    }
    else if (m_settings.m_subGHzBand)
    {
        static const int chipsSubGHzOqpsk[16][16] = {
            {0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1},
            {0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1},
            {0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0},
            {1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0},
            {0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0},
            {1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1},
            {1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 1},
            {1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0},
            {0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0},
            {0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0},
            {0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 1},
            {1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1},
            {0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1},
            {1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0},
            {1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0},
            {1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1},
        };
        chip = chipsSubGHzOqpsk[m_symbol][m_chipIdx];
    }
    else
    {
        static const int chipsOqpsk[16][32] = {
            {1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0},
            {1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0},
            {0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0},
            {0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1},
            {0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1},
            {0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0},
            {1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1},
            {1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1},
            {1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1},
            {1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1},
            {0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1},
            {0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0},
            {0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1},
            {1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0},
            {1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0}
        };
        chip = chipsOqpsk[m_symbol][m_chipIdx];
    }

    m_chipIdx++;
    if (m_chipIdx >= m_chipsPerSymbol)
        m_chipIdx = 0;

    return chip;
}

int IEEE_802_15_4_ModSource::getSymbol()
{
    int symbol;

    if (m_bitCount > 0)
    {
        int mask = m_bitsPerSymbol == 1 ? 0x1 : 0xf;
        symbol = (m_bits[m_byteIdx] >> m_bitIdx) & mask;
        m_bitIdx += m_bitsPerSymbol;
        m_bitCount -= m_bitsPerSymbol;
        if (m_bitIdx == 8)
        {
            m_byteIdx++;
            m_bitIdx = 0;
        }
        if (m_settings.m_modulation == IEEE_802_15_4_ModSettings::BPSK)
        {
            // Differential encoding
            symbol = symbol ^ m_diffBit;
            m_diffBit = symbol;
        }
    }
    else
        symbol = 0;

    return symbol;
}

void IEEE_802_15_4_ModSource::initTX()
{
    m_sampleIdx = 0;
    m_chipOdd = false;
    m_chips[0] = 0;
    m_chips[1] = 0;
    m_chipIdx = 0;
    m_diffBit = 0;
    m_byteIdx = 0;
    m_bitIdx = 0;
    m_bitCount = m_bitCountTotal; // Reset to allow retransmission
    m_symbol = 0;
    if (m_settings.m_rampUpBits == 0)
    {
        m_state = tx;
        m_pow = 0.0f;
    }
    else
    {
        m_state = ramp_up;
        m_pow = -(Real)m_settings.m_rampRange;
        m_powRamp = m_settings.m_rampRange/(m_settings.m_rampUpBits * (Real)m_samplesPerChip);
    }
    m_scrambler.init();
}

void IEEE_802_15_4_ModSource::convert(const QString dataStr, QByteArray& data)
{
    // Convert string containing space separated list of hex values to binary
    QStringList list = dataStr.split(" ");

    for (int i = 0; i < list.size(); i++) {
        data.append(list[i].toInt(nullptr, 16));
    }
}

void IEEE_802_15_4_ModSource::addTxFrame(const QString& data)
{
    QByteArray ba;
    convert(data.trimmed(), ba);
    addTxFrame(ba);
}

void IEEE_802_15_4_ModSource::addTxFrame(const QByteArray& data)
{
    uint8_t *crcStart;
    uint8_t *p;
    uint8_t *pLength;
    crc16itut crc;
    uint16_t crcValue;

    // Create PHY frame
    p = m_bits;
    // Preamble
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    // SFD - start of frame delimiter
    *p++ = 0xa7;
    // PHR - length
    pLength = p;
    *p++ = 0;
    // PHY payload
    crcStart = p;
    // Data
    std::copy(data.data(), data.data() + data.length(), p);
    p += data.length();
    // MAC FCS
    crc.calculate(crcStart, p-crcStart);
    crcValue = crc.get();
    *p++ = crcValue & 0xff;
    *p++ = (crcValue >> 8);
    // Update length
    *pLength = p - pLength - 1;
    // Extra 0 to account for pulse shaping filter delay.
    // Should probably just be a few chips
    *p++ = 0x00;

    // Dump frame
    QByteArray qb((char *)m_bits, p-m_bits);

    // Save number of bits in frame
    m_bitCount = m_bitCountTotal = (p-&m_bits[0]) * 8;

    m_frameRepeatCount = m_settings.m_repeatCount;
    initTX();

    if (m_settings.m_writeToFile) {
        m_basebandFile.open("IEEE_802_15_4_Mod.csv", std::ofstream::out);
    } else if (m_basebandFile.is_open()) {
        m_basebandFile.close();
    }
}

void IEEE_802_15_4_ModSource::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool IEEE_802_15_4_ModSource::handleMessage(const Message& msg)
{
    if (MsgOpenUDP::match(msg))
    {
        qDebug("IEEE_802_15_4_ModSource::handleMessage: MsgOpenUDP");
        const MsgOpenUDP& cmd = (const MsgOpenUDP&) msg;
        openUDP(cmd.getUDPAddress(), cmd.getUDPPort());
        return true;
    }
    else if (MsgCloseUDP::match(msg))
    {
        qDebug("IEEE_802_15_4_ModSource::handleMessage: MsgCloseUDP");
        closeUDP();
        return true;
    }
    else
    {
        return false;
    }
}

void IEEE_802_15_4_ModSource::closeUDP()
{
    if (m_udpSocket != nullptr)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &IEEE_802_15_4_ModSource::udpRx);
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void IEEE_802_15_4_ModSource::openUDP(const QString& udpAddress, uint16_t udpPort)
{
    m_udpSocket = new QUdpSocket();

    if (m_udpSocket->bind(QHostAddress(udpAddress), udpPort))
    {
        connect(m_udpSocket, &QUdpSocket::readyRead, this, &IEEE_802_15_4_ModSource::udpRx);
        qDebug() << "IEEE_802_15_4_ModSource::openUDP: Listening for packets on "
            << udpAddress << ":"
            << udpPort;
            m_udpSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, IEEE_802_15_4_ModSettings::m_udpBufferSize);
    }
    else
    {
        qCritical() << "IEEE_802_15_4_Mod::openUDP: Failed to bind to port "
            << udpAddress << ":"
            << udpPort
            << ". Error: " << m_udpSocket->error();
    }
}

void IEEE_802_15_4_ModSource::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();
        qDebug() << "IEEE_802_15_4_ModSource::udpRx: " << data.toHex();

        if (m_settings.m_udpBytesFormat)
        {
            addTxFrame(data);
        }
        else
        {
            QString string = data.toHex(' ');
            addTxFrame(string);
        }
    }
}
