///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "chirpchatmodsource.h"

const int ChirpChatModSource::m_levelNbSamples = 480; // every 10ms

ChirpChatModSource::ChirpChatModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_phaseIncrements(nullptr),
    m_repeatCount(0),
    m_active(false),
    m_modPhasor(0.0f),
	m_levelCalcCount(0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f)
{
	m_magsq = 0.0;

    initSF(m_settings.m_spreadFactor);
    initTest(m_settings.m_spreadFactor, m_settings.m_deBits);
    reset();
    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

ChirpChatModSource::~ChirpChatModSource()
{
    delete[] m_phaseIncrements;
}

void ChirpChatModSource::initSF(unsigned int sf)
{
    m_fftLength = 1 << sf;
    m_state = ChirpChatStateIdle;
    m_quarterSamples = (m_fftLength/4)*ChirpChatModSettings::oversampling;

    float halfAngle = M_PI/ChirpChatModSettings::oversampling;
    float phase = -halfAngle;

    if (m_phaseIncrements) {
        delete[] m_phaseIncrements;
    }

    m_phaseIncrements = new double[2*m_fftLength*ChirpChatModSettings::oversampling];
    phase = -halfAngle;

    for (unsigned int i = 0; i < m_fftLength*ChirpChatModSettings::oversampling; i++)
    {
        m_phaseIncrements[i] = phase;
        phase += (2*halfAngle) / (m_fftLength*ChirpChatModSettings::oversampling);
    }

    std::copy(
        m_phaseIncrements,
        m_phaseIncrements+m_fftLength*ChirpChatModSettings::oversampling,
        m_phaseIncrements+m_fftLength*ChirpChatModSettings::oversampling
    );
}

void ChirpChatModSource::initTest(unsigned int sf, unsigned int deBits)
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

void ChirpChatModSource::reset()
{
    m_chirp = 0;
    m_chirp0 = 0;
    m_sampleCounter = 0;
    m_fftCounter = 0;
    m_chirpCount = 0;
}

void ChirpChatModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void ChirpChatModSource::pullOne(Sample& sample)
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

void ChirpChatModSource::modulateSample()
{
    if (m_state == ChirpChatStateIdle)
    {
        m_modSample = Complex{0.0, 0.0};
        m_sampleCounter++;

        if (m_sampleCounter == m_quietSamples*ChirpChatModSettings::oversampling) // done with quiet time
        {
            m_chirp0 = 0;
            m_chirp = m_fftLength*ChirpChatModSettings::oversampling - 1;

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
        m_modPhasor += m_phaseIncrements[m_chirp]; // up chirps
        m_modSample = Complex(std::polar(0.891235351562 * SDR_TX_SCALED, m_modPhasor));
        m_fftCounter++;

        if (m_fftCounter == m_fftLength*ChirpChatModSettings::oversampling)
        {
            m_chirpCount++;
            m_fftCounter = 0;

            if (m_chirpCount == m_settings.m_preambleChirps)
            {
                m_chirpCount = 0;

                if (m_settings.hasSyncWord())
                {
                    m_chirp0 = ((m_settings.m_syncWord >> ((1-m_chirpCount)*4)) & 0xf)*8;
                    m_chirp = (m_chirp0 + m_fftLength)*ChirpChatModSettings::oversampling - 1;
                    m_state = ChirpChatStateSyncWord;
                }
                else
                {
                    m_sampleCounter = 0;
                    m_chirp0 = 0;
                    m_chirp = m_fftLength*ChirpChatModSettings::oversampling - 1;
                    m_state = ChirpChatStateSFD;
                }
            }
        }
    }
    else if (m_state == ChirpChatStateSyncWord)
    {
        m_modPhasor += m_phaseIncrements[m_chirp]; // up chirps
        m_modSample = Complex(std::polar(0.891235351562 * SDR_TX_SCALED, m_modPhasor));
        m_fftCounter++;

        if (m_fftCounter == m_fftLength*ChirpChatModSettings::oversampling)
        {
            m_chirpCount++;
            m_chirp0 = ((m_settings.m_syncWord >> ((1-m_chirpCount)*4)) & 0xf)*8;
            m_chirp = (m_chirp0 + m_fftLength)*ChirpChatModSettings::oversampling - 1;
            m_fftCounter = 0;

            if (m_chirpCount == 2)
            {
                m_sampleCounter = 0;
                m_chirpCount = 0;
                m_chirp0 = 0;
                m_chirp = m_fftLength*ChirpChatModSettings::oversampling - 1;
                m_state = ChirpChatStateSFD;
            }
        }
    }
    else if (m_state == ChirpChatStateSFD)
    {
        m_modPhasor -= m_phaseIncrements[m_chirp]; // down chirps
        m_modSample = Complex(std::polar(0.891235351562 * SDR_TX_SCALED, m_modPhasor));
        m_fftCounter++;
        m_sampleCounter++;

        if (m_fftCounter == m_fftLength*ChirpChatModSettings::oversampling)
        {
            m_chirp0 = 0;
            m_chirp = m_fftLength*ChirpChatModSettings::oversampling - 1;
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
            m_chirp0 = encodeSymbol(m_symbols[m_chirpCount]);
            m_chirp = (m_chirp0 + m_fftLength)*ChirpChatModSettings::oversampling - 1;
            m_state = ChirpChatStatePayload;
        }
    }
    else if (m_state == ChirpChatStatePayload)
    {
        m_modPhasor += m_phaseIncrements[m_chirp]; // up chirps
        m_modSample = Complex(std::polar(0.891235351562 * SDR_TX_SCALED, m_modPhasor));
        m_fftCounter++;

        if (m_fftCounter == m_fftLength*ChirpChatModSettings::oversampling)
        {
            m_chirpCount++;

            if (m_chirpCount == m_symbols.size())
            {
                reset();
                m_state = ChirpChatStateIdle;
            }
            else
            {
                m_chirp0 = encodeSymbol(m_symbols[m_chirpCount]);
                m_chirp = (m_chirp0 + m_fftLength)*ChirpChatModSettings::oversampling - 1;
                m_fftCounter = 0;
            }
        }
    }

    // limit phasor range to ]-pi,pi]
    if (m_modPhasor > M_PI) {
        m_modPhasor -= (2.0f * M_PI);
    }

    m_chirp++;

    if (m_chirp >= (m_chirp0 + m_fftLength)*ChirpChatModSettings::oversampling) {
        m_chirp = m_chirp0*ChirpChatModSettings::oversampling;
    }
}

unsigned short ChirpChatModSource::encodeSymbol(unsigned short symbol)
{
    if (m_settings.m_deBits == 0) {
        return symbol;
    }

    unsigned int deWidth = 1<<m_settings.m_deBits;
    unsigned int baseSymbol = symbol % (m_fftLength/deWidth); // symbols range control
    return deWidth*baseSymbol;
    // return deWidth*baseSymbol + (deWidth/2) - 1;
}

void ChirpChatModSource::calculateLevel(Real& sample)
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

void ChirpChatModSource::applySettings(const ChirpChatModSettings& settings, bool force)
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

void ChirpChatModSource::applyChannelSettings(int channelSampleRate, int bandwidth, int channelFrequencyOffset, bool force)
{
    qDebug() << "ChirpChatModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset
            << " bandwidth: " << bandwidth
            << " SR: " << bandwidth * ChirpChatModSettings::oversampling;

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
        m_interpolatorDistance = (Real) (bandwidth*ChirpChatModSettings::oversampling) / (Real) channelSampleRate;
        m_interpolator.create(16, bandwidth, bandwidth / 2.2);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_bandwidth = bandwidth;
    m_quietSamples = (bandwidth*m_settings.m_quietMillis) / 1000;
    m_state = ChirpChatStateIdle;
    reset();
}

void ChirpChatModSource::setSymbols(const std::vector<unsigned short>& symbols)
{
    m_symbols = symbols;
    qDebug("ChirpChatModSource::setSymbols: m_symbols: %lu", m_symbols.size());
    m_repeatCount = m_settings.m_messageRepeat;
    m_state = ChirpChatStateIdle; // first reset to idle
    reset();
    m_sampleCounter = m_quietSamples*ChirpChatModSettings::oversampling - 1; // start immediately
}
