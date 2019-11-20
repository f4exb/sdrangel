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

#include <QDebug>

#include "wfmmodsource.h"

const int WFMModSource::m_rfFilterFFTLength = 1024;
const int WFMModSource::m_levelNbSamples = 480; // every 10ms

WFMModSource::WFMModSource() :
    m_channelSampleRate(384000),
    m_channelFrequencyOffset(0),
    m_modPhasor(0.0f),
    m_audioFifo(4800),
	m_levelCalcCount(0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f),
    m_ifstream(nullptr),
    m_audioSampleRate(48000)
{
    m_rfFilter = new fftfilt(-62500.0 / 384000.0, 62500.0 / 384000.0, m_rfFilterFFTLength);
    m_rfFilterBuffer = new Complex[m_rfFilterFFTLength];
    std::fill(m_rfFilterBuffer, m_rfFilterBuffer+m_rfFilterFFTLength, Complex{0,0});
    m_rfFilterBufferIndex = 0;
	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;
	m_magsq = 0.0;

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

WFMModSource::~WFMModSource()
{
    delete m_rfFilter;
    delete[] m_rfFilterBuffer;
}

void WFMModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void WFMModSource::pullOne(Sample& sample)
{
	if (m_settings.m_channelMute)
	{
		sample.m_real = 0.0f;
		sample.m_imag = 0.0f;
		return;
	}

	Complex ci, ri;
    fftfilt::cmplx *rf;
    int rf_out;
  	Real t;

	if ((m_settings.m_modAFInput == WFMModSettings::WFMModInputFile)
	   || (m_settings.m_modAFInput == WFMModSettings::WFMModInputAudio))
	{
        if (m_interpolatorDistance > 1.0f) // decimate
        {
            modulateAudio();

            while (!m_interpolator.decimate(&m_interpolatorDistanceRemain, m_modSample, &ri)) {
                modulateAudio();
            }
        }
        else // interpolate
        {
            if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_modSample, &ri)) {
                modulateAudio();
            }
        }

        t = ri.real();
	    m_interpolatorDistanceRemain += m_interpolatorDistance;
	}
	else
	{
	    pullAF(t);
        calculateLevel(t);
	}

    m_modPhasor += (m_settings.m_fmDeviation / (float) m_channelSampleRate) * t * M_PI * 2.0f;

    // limit phasor range to ]-pi,pi]
    if (m_modPhasor > M_PI) {
        m_modPhasor -= (2.0f * M_PI);
    }

    ci.real(cos(m_modPhasor) * 0.891235351562f * SDR_TX_SCALEF); // -1 dB
    ci.imag(sin(m_modPhasor) * 0.891235351562f * SDR_TX_SCALEF);

    // RF filtering
    rf_out = m_rfFilter->runFilt(ci, &rf);

    if (rf_out > 0)
    {
        memcpy((void *) m_rfFilterBuffer, (const void *) rf, rf_out*sizeof(Complex));
        m_rfFilterBufferIndex = 0;

    }

    ci = m_rfFilterBuffer[m_rfFilterBufferIndex] * m_carrierNco.nextIQ(); // shift to carrier frequency
    m_rfFilterBufferIndex++;

    double magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
	magsq /= (SDR_TX_SCALED*SDR_TX_SCALED);
	m_movingAverage(magsq);
	m_magsq = m_movingAverage.asDouble();

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();
}

void WFMModSource::modulateAudio()
{
    Real t;
    pullAF(t);
    calculateLevel(t);
    m_modSample.real(t);
    m_modSample.imag(0.0f);
}

void WFMModSource::prefetch(unsigned int nbSamples)
{
    unsigned int nbSamplesAudio = nbSamples * ((Real) m_audioSampleRate / (Real) m_channelSampleRate);
    pullAudio(nbSamplesAudio);
}

void WFMModSource::pullAudio(unsigned int nbSamplesAudio)
{
    if (nbSamplesAudio > m_audioBuffer.size()) {
        m_audioBuffer.resize(nbSamplesAudio);
    }

    m_audioFifo.read(reinterpret_cast<quint8*>(&m_audioBuffer[0]), nbSamplesAudio);
    m_audioBufferFill = 0;
}

void WFMModSource::pullAF(Real& sample)
{
    switch (m_settings.m_modAFInput)
    {
    case WFMModSettings::WFMModInputTone:
        sample = m_toneNco.next() * m_settings.m_volumeFactor;
        break;
    case WFMModSettings::WFMModInputFile:
        // sox f4exb_call.wav --encoding float --endian little f4exb_call.raw
        // ffplay -f f32le -ar 48k -ac 1 f4exb_call.raw
        if (m_ifstream && m_ifstream->is_open())
        {
            if (m_ifstream->eof())
            {
            	if (m_settings.m_playLoop)
            	{
                    m_ifstream->clear();
                    m_ifstream->seekg(0, std::ios::beg);
            	}
            }

            if (m_ifstream->eof())
            {
            	sample = 0.0f;
            }
            else
            {
                Real s;
            	m_ifstream->read(reinterpret_cast<char*>(&s), sizeof(Real));
            	sample = s * m_settings.m_volumeFactor;
            }
        }
        else
        {
            sample = 0.0f;
        }
        break;
    case WFMModSettings::WFMModInputAudio:
        {
            if (m_audioBufferFill < m_audioBuffer.size())
            {
                sample = ((m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r) / 65536.0f) * m_settings.m_volumeFactor;
                m_audioBufferFill++;
            }
            else
            {
                unsigned int size = m_audioBuffer.size();
                qDebug("WFMModSource::pullAF: starve audio samples: size: %u", size);
                sample = ((m_audioBuffer[size-1].l + m_audioBuffer[size-1].r) / 65536.0f) * m_settings.m_volumeFactor;
            }
        }
        break;
    case WFMModSettings::WFMModInputCWTone:
        Real fadeFactor;

        if (m_cwKeyer.getSample())
        {
            m_cwKeyer.getCWSmoother().getFadeSample(true, fadeFactor);
            sample = m_toneNco.next() * m_settings.m_volumeFactor * fadeFactor;
        }
        else
        {
            if (m_cwKeyer.getCWSmoother().getFadeSample(false, fadeFactor))
            {
                sample = m_toneNco.next() * m_settings.m_volumeFactor * fadeFactor;
            }
            else
            {
                sample = 0.0f;
                m_toneNco.setPhase(0);
            }
        }
        break;
    case WFMModSettings::WFMModInputNone:
    default:
        sample = 0.0f;
        break;
    }
}

void WFMModSource::calculateLevel(const Real& sample)
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

void WFMModSource::applyAudioSampleRate(unsigned int sampleRate)
{
    qDebug("WFMModSource::applyAudioSampleRate: %d", sampleRate);

    m_interpolatorDistanceRemain = 0;
    m_interpolatorConsumed = false;
    m_interpolatorDistance = (Real) sampleRate / (Real) m_channelSampleRate;
    m_interpolator.create(48, sampleRate, m_settings.m_rfBandwidth / 2.2, 3.0);

    m_audioSampleRate = sampleRate;
}

void WFMModSource::applySettings(const WFMModSettings& settings, bool force)
{
    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_audioSampleRate / (Real) m_channelSampleRate;
        m_interpolator.create(48, m_audioSampleRate, settings.m_afBandwidth / 2.2, 3.0);
    }

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        Real lowCut = -(settings.m_rfBandwidth / 2.2) / m_channelSampleRate;
        Real hiCut  = (settings.m_rfBandwidth / 2.2) / m_channelSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
    }

    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        m_toneNco.setFreq(settings.m_toneFrequency, m_channelSampleRate);
    }

    m_settings = settings;
}

void WFMModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "WFMModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force) {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_audioSampleRate / (Real) channelSampleRate;
        m_interpolator.create(48, m_audioSampleRate, m_settings.m_afBandwidth / 2.2, 3.0);
        Real lowCut = -(m_settings.m_rfBandwidth / 2.0) / channelSampleRate;
        Real hiCut  = (m_settings.m_rfBandwidth / 2.0) / channelSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_toneNco.setFreq(m_settings.m_toneFrequency, channelSampleRate);
        m_cwKeyer.setSampleRate(channelSampleRate);
        m_cwKeyer.reset();
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}