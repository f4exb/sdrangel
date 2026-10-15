///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include <QStringList>

#include "meshcoremodsource.h"

const int MeshcoreModSource::m_levelNbSamples = 480; // every 10ms

MeshcoreModSource::MeshcoreModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_bandwidth(MeshcoreModSettings::bandwidths[5]),
    m_phaseIncrements(nullptr),
    m_repeatCount(0),
    m_txFrameToken(0U),
    m_active(false),
    m_modPhasor(0.0f),
    m_levelCalcCount(0),
    m_rmsLevel(0.0f),
    m_peakLevelOut(0.0f),
    m_peakLevel(0.0f),
    m_levelSum(0.0f)
{
	m_magsq = 0.0;

    initSF(m_settings.m_spreadFactor);
    initTest(m_settings.m_spreadFactor, m_settings.m_deBits);
    reset();
    applySettings(m_settings, true);
    applyChannelSettings(
        m_channelSampleRate,
        MeshcoreModSettings::bandwidths[m_settings.m_bandwidthIndex],
        m_channelFrequencyOffset,
        true
    );
}

MeshcoreModSource::~MeshcoreModSource()
{
    delete[] m_phaseIncrements;
}

void MeshcoreModSource::initSF(unsigned int sf)
{
    m_fftLength = 1 << sf;
    m_state = ChirpChatStateIdle;
    m_quarterSamples = (m_fftLength/4)*MeshcoreModSettings::oversampling;

    float halfAngle = M_PI/MeshcoreModSettings::oversampling;
    float phase = -halfAngle;

    if (m_phaseIncrements) {
        delete[] m_phaseIncrements;
    }

    m_phaseIncrements = new double[2*m_fftLength*MeshcoreModSettings::oversampling];
    phase = -halfAngle;

    for (unsigned int i = 0; i < m_fftLength*MeshcoreModSettings::oversampling; i++)
    {
        m_phaseIncrements[i] = phase;
        phase += (2*halfAngle) / (m_fftLength*MeshcoreModSettings::oversampling);
    }

    std::copy(
        m_phaseIncrements,
        m_phaseIncrements+m_fftLength*MeshcoreModSettings::oversampling,
        m_phaseIncrements+m_fftLength*MeshcoreModSettings::oversampling
    );
}

void MeshcoreModSource::initTest(unsigned int sf, unsigned int deBits)
{
    unsigned int fftLength = 1<<sf;
    unsigned int symbolRange = fftLength/(1<<deBits);
    m_symbols.clear();

    for (unsigned int seq = 0; seq < 1; seq++)
    {
        for (unsigned int symbol = 0; symbol < symbolRange; symbol += symbolRange/4)
        {
            m_symbols.push_back(symbol);
            m_symbols.push_back(symbol+1);
        }
    }
}

void MeshcoreModSource::reset()
{
    m_chirp = 0;
    m_chirp0 = 0;
    m_sampleCounter = 0;
    m_fftCounter = 0;
    m_chirpCount = 0;
}

void MeshcoreModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void MeshcoreModSource::pullOne(Sample& sample)
{
	if (m_settings.m_channelMute)
	{
		sample.m_real = 0.0f;
		sample.m_imag = 0.0f;
        m_magsq = 0.0;
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

    if (!(m_state == ChirpChatStateIdle))
    {
        double magsq = std::norm(ci);
        magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
        m_movingAverage(magsq);
        m_magsq = m_movingAverage.asDouble();
    }

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();
}

void MeshcoreModSource::modulateSample()
{
    if (m_state == ChirpChatStateIdle)
    {
        m_modSample = Complex{0.0, 0.0};
        m_sampleCounter++;

        if (m_sampleCounter == m_quietSamples*MeshcoreModSettings::oversampling) // done with quiet time
        {
            m_chirp0 = 0;
            m_chirp = m_chirp0*MeshcoreModSettings::oversampling;

            if (m_symbols.size() != 0) // some payload to transmit
            {
                if (m_settings.m_messageRepeat == 0) // infinite
                {
                    m_state = ChirpChatStatePreamble;
                    m_active = true;
                }
                else
                {
                    if (m_repeatCount != 0)
                    {
                        m_repeatCount--;
                        m_state = ChirpChatStatePreamble;
                        m_active = true;
                    }
                    else
                    {
                        m_active = false;
                    }
                }
            }
            else
            {
                m_active = false;
            }
        }
    }
    else if (m_state == ChirpChatStatePreamble)
    {
        m_modPhasor += (m_settings.m_invertRamps ? -1 : 1) * m_phaseIncrements[m_chirp]; // preamble chirps
        m_modSample = Complex(std::polar(0.891235351562 * SDR_TX_SCALED, m_modPhasor));
        m_fftCounter++;

        if (m_fftCounter == m_fftLength*MeshcoreModSettings::oversampling)
        {
            m_chirpCount++;
            m_fftCounter = 0;

            if (m_chirpCount == m_settings.m_preambleChirps)
            {
                m_chirpCount = 0;

                if (m_settings.hasSyncWord())
                {
                    m_chirp0 = ((m_settings.m_syncWord >> ((1-m_chirpCount)*4)) & 0xf)*8;
                    // Standard LoRa: chirp index walks phaseIncrements[chirp0*os .. (chirp0+N)*os - 1].
                    // Old init `(chirp0+N)*os - 1` triggered immediate wrap on first m_chirp++,
                    // causing first sample to read phaseInc[N*os-1] (high freq) before ramping
                    // up from phaseInc[0]. ~0.25 FFT-bin freq drift, breaking demod header CRC.
                    m_chirp = m_chirp0*MeshcoreModSettings::oversampling;
                    m_state = ChirpChatStateSyncWord;
                }
                else
                {
                    m_sampleCounter = 0;
                    m_chirp0 = 0;
                    m_chirp = m_chirp0*MeshcoreModSettings::oversampling;
                    m_state = ChirpChatStateSFD;
                }
            }
        }
    }
    else if (m_state == ChirpChatStateSyncWord)
    {
        m_modPhasor += (m_settings.m_invertRamps ? -1 : 1) * m_phaseIncrements[m_chirp]; // sync chirps same orientation as preamble
        m_modSample = Complex(std::polar(0.891235351562 * SDR_TX_SCALED, m_modPhasor));
        m_fftCounter++;

        if (m_fftCounter == m_fftLength*MeshcoreModSettings::oversampling)
        {
            m_chirpCount++;
            m_fftCounter = 0;

            if (m_chirpCount >= 2)
            {
                m_sampleCounter = 0;
                m_chirpCount = 0;
                m_chirp0 = 0;
                m_chirp = m_chirp0*MeshcoreModSettings::oversampling;
                m_state = ChirpChatStateSFD;
            }
            else
            {
                m_chirp0 = ((m_settings.m_syncWord >> ((1-m_chirpCount)*4)) & 0xf)*8;
                m_chirp = m_chirp0*MeshcoreModSettings::oversampling;
            }
        }
    }
    else if (m_state == ChirpChatStateSFD)
    {
        m_modPhasor -= (m_settings.m_invertRamps ? -1 : 1) * m_phaseIncrements[m_chirp]; // SFD chirps
        m_modSample = Complex(std::polar(0.891235351562 * SDR_TX_SCALED, m_modPhasor));
        m_fftCounter++;
        m_sampleCounter++;

        if (m_fftCounter == m_fftLength*MeshcoreModSettings::oversampling)
        {
            m_chirp0 = 0;
            m_chirp = m_chirp0*MeshcoreModSettings::oversampling;
            m_fftCounter = 0;
        }

        if (m_sampleCounter == m_quarterSamples)
        {
            m_chirpCount++;
            m_sampleCounter = 0;
        }

        if (m_chirpCount == m_settings.getNbSFDFourths())
        {
            m_fftCounter = 0;
            m_chirpCount = 0;
            m_chirp0 = encodeSymbol(m_symbols[m_chirpCount], MeshcoreModSettings::m_hasHeader && (m_chirpCount < 8U));
            m_txFrameToken++;
            m_chirp = m_chirp0*MeshcoreModSettings::oversampling;
            m_state = ChirpChatStatePayload;
        }
    }
    else if (m_state == ChirpChatStatePayload)
    {
        m_modPhasor += (m_settings.m_invertRamps ? -1 : 1) * m_phaseIncrements[m_chirp]; // payload chirps
        m_modSample = Complex(std::polar(0.891235351562 * SDR_TX_SCALED, m_modPhasor));
        m_fftCounter++;

        if (m_fftCounter == m_fftLength*MeshcoreModSettings::oversampling)
        {
            m_chirpCount++;

            if (m_chirpCount == m_symbols.size())
            {
                reset();
                m_state = ChirpChatStateIdle;
            }
            else
            {
                m_chirp0 = encodeSymbol(m_symbols[m_chirpCount], MeshcoreModSettings::m_hasHeader && (m_chirpCount < 8U));
                m_chirp = m_chirp0*MeshcoreModSettings::oversampling;
                m_fftCounter = 0;
            }
        }
    }

    // limit phasor range to ]-pi,pi]
    if (m_modPhasor > M_PI) {
        m_modPhasor -= (2.0f * M_PI);
    }

    m_chirp++;

    if (m_chirp >= (m_chirp0 + m_fftLength)*MeshcoreModSettings::oversampling) {
        m_chirp = m_chirp0*MeshcoreModSettings::oversampling;
    }
}

unsigned short MeshcoreModSource::encodeSymbol(unsigned short symbol, bool headerSymbol) const
{
    auto deBits = static_cast<unsigned int>(std::max(0, m_settings.m_deBits));

    if (headerSymbol && deBits < 2U) {
        deBits = 2U;
    }

    const unsigned int deWidth = 1U << deBits;
    const unsigned int symbolRange = std::max(1U, m_fftLength / std::max(1U, deWidth));
    const unsigned int baseSymbol = symbol % symbolRange;
    const unsigned int rawSymbol = (deWidth * baseSymbol + 1U) % m_fftLength; // match demod evalSymbol shift (raw_bin - 1)
    return static_cast<unsigned short>(rawSymbol);
}

void MeshcoreModSource::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(m_peakLevel, std::fabs(sample));
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

void MeshcoreModSource::applySettings(const MeshcoreModSettings& settings, bool force)
{
    if ((settings.m_spreadFactor != m_settings.m_spreadFactor)
     || (settings.m_deBits != m_settings.m_deBits)
     || (settings.m_preambleChirps != m_settings.m_preambleChirps)|| force)
    {
        initSF(settings.m_spreadFactor);
        initTest(settings.m_spreadFactor, settings.m_deBits);
        reset();
    }

    if ((settings.m_quietMillis != m_settings.m_quietMillis) || force)
    {
        m_quietSamples = (m_bandwidth*settings.m_quietMillis) / 1000;
        reset();
    }

    if ((settings.m_messageRepeat != m_settings.m_messageRepeat) || force) {
        m_repeatCount = settings.m_messageRepeat;
    }

    m_settings = settings;
}

void MeshcoreModSource::applyChannelSettings(int channelSampleRate, int bandwidth, int channelFrequencyOffset, bool force)
{
    qWarning() << "MeshcoreModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset
            << " bandwidth: " << bandwidth
            << " SR: " << bandwidth * MeshcoreModSettings::oversampling;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate)
     || (bandwidth != m_bandwidth) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) (bandwidth*MeshcoreModSettings::oversampling) / (Real) channelSampleRate;
        m_interpolator.create(16, bandwidth, bandwidth / 2.2);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_bandwidth = bandwidth;
    m_quietSamples = (bandwidth*m_settings.m_quietMillis) / 1000;
    m_state = ChirpChatStateIdle;
    reset();
}

void MeshcoreModSource::setSymbols(const std::vector<unsigned short>& symbols)
{
    m_symbols = symbols;
    qDebug("MeshcoreModSource::setSymbols: m_symbols: %lu", m_symbols.size());
    m_repeatCount = m_settings.m_messageRepeat;
    m_state = ChirpChatStateIdle; // first reset to idle
    reset();
    m_sampleCounter = m_quietSamples*MeshcoreModSettings::oversampling - 1; // start immediately
}
