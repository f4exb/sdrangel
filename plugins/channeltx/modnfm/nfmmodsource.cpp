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

#include <QDebug>

#include "nfmmodsource.h"

const int NFMModSource::m_levelNbSamples = 480; // every 10ms
const float NFMModSource::m_preemphasis = 120.0e-6; // 120us

NFMModSource::NFMModSource() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_modPhasor(0.0f),
    m_audioFifo(4800),
    m_feedbackAudioFifo(48000),
	m_levelCalcCount(0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f),
    m_ifstream(nullptr),
    m_preemphasisFilter(m_preemphasis*48000),
    m_audioSampleRate(48000)
{
	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

	m_feedbackAudioBuffer.resize(1<<14);
	m_feedbackAudioBufferFill = 0;

	m_magsq = 0.0;

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

NFMModSource::~NFMModSource()
{
}

void NFMModSource::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    std::for_each(
        begin,
        begin + nbSamples,
        [this](Sample& s) {
            pullOne(s);
        }
    );
}

void NFMModSource::pullOne(Sample& sample)
{
	if (m_settings.m_channelMute)
	{
		sample.m_real = 0.0f;
		sample.m_imag = 0.0f;
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
	m_movingAverage(magsq);
	m_magsq = m_movingAverage.asDouble();

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();
}

void NFMModSource::prefetch(unsigned int nbSamples)
{
    unsigned int nbSamplesAudio = nbSamples * ((Real) m_audioSampleRate / (Real) m_channelSampleRate);
    pullAudio(nbSamplesAudio);
}

void NFMModSource::pullAudio(unsigned int nbSamplesAudio)
{
    if (nbSamplesAudio > m_audioBuffer.size())
    {
        m_audioBuffer.resize(nbSamplesAudio);
    }

    m_audioFifo.read(reinterpret_cast<quint8*>(&m_audioBuffer[0]), nbSamplesAudio);
    m_audioBufferFill = 0;
}

void NFMModSource::modulateSample()
{
	Real t0, t;

    pullAF(t0);
    m_preemphasisFilter.process(t0, t);

    if (m_settings.m_feedbackAudioEnable) {
        pushFeedback(t * m_settings.m_feedbackVolumeFactor * 16384.0f);
    }

    calculateLevel(t);
    m_audioBufferFill++;

    if (m_settings.m_ctcssOn)
    {
        m_modPhasor += (m_settings.m_fmDeviation / (float) m_audioSampleRate) * (0.85f * m_bandpass.filter(t) + 0.15f * 189.0f * m_ctcssNco.next()) * (M_PI / 189.0f);
    }
    else
    {
        // 378 = 302 * 1.25; 302 = number of filter taps (established experimentally) and 189 = 378/2 for 2*PI
        m_modPhasor += (m_settings.m_fmDeviation / (float) m_audioSampleRate) * m_bandpass.filter(t) * (M_PI / 189.0f);
    }

    // limit phasor range to ]-pi,pi]
    if (m_modPhasor > M_PI) {
        m_modPhasor -= (2.0f * M_PI);
    }

    m_modSample.real(cos(m_modPhasor) * 0.891235351562f * SDR_TX_SCALEF); // -1 dB
    m_modSample.imag(sin(m_modPhasor) * 0.891235351562f * SDR_TX_SCALEF);
}

void NFMModSource::pullAF(Real& sample)
{
    switch (m_settings.m_modAFInput)
    {
    case NFMModSettings::NFMModInputTone:
        sample = m_toneNco.next();
        break;
    case NFMModSettings::NFMModInputFile:
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
            	m_ifstream->read(reinterpret_cast<char*>(&sample), sizeof(Real));
            	sample *= m_settings.m_volumeFactor;
            }
        }
        else
        {
            sample = 0.0f;
        }
        break;
    case NFMModSettings::NFMModInputAudio:
        sample = ((m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r) / 65536.0f) * m_settings.m_volumeFactor;
        break;
    case NFMModSettings::NFMModInputCWTone:
        Real fadeFactor;

        if (m_cwKeyer.getSample())
        {
            m_cwKeyer.getCWSmoother().getFadeSample(true, fadeFactor);
            sample = m_toneNco.next() * fadeFactor;
        }
        else
        {
            if (m_cwKeyer.getCWSmoother().getFadeSample(false, fadeFactor))
            {
                sample = m_toneNco.next() * fadeFactor;
            }
            else
            {
                sample = 0.0f;
                m_toneNco.setPhase(0);
            }
        }
        break;
    case NFMModSettings::NFMModInputNone:
    default:
        sample = 0.0f;
        break;
    }
}

void NFMModSource::pushFeedback(Real sample)
{
    Complex c(sample, sample);
    Complex ci;

    if (m_feedbackInterpolatorDistance < 1.0f) // interpolate
    {
        while (!m_feedbackInterpolator.interpolate(&m_feedbackInterpolatorDistanceRemain, c, &ci))
        {
            processOneSample(ci);
            m_feedbackInterpolatorDistanceRemain += m_feedbackInterpolatorDistance;
        }
    }
    else // decimate
    {
        if (m_feedbackInterpolator.decimate(&m_feedbackInterpolatorDistanceRemain, c, &ci))
        {
            processOneSample(ci);
            m_feedbackInterpolatorDistanceRemain += m_feedbackInterpolatorDistance;
        }
    }
}

void NFMModSource::processOneSample(Complex& ci)
{
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].l = ci.real();
    m_feedbackAudioBuffer[m_feedbackAudioBufferFill].r = ci.imag();
    ++m_feedbackAudioBufferFill;

    if (m_feedbackAudioBufferFill >= m_feedbackAudioBuffer.size())
    {
        uint res = m_feedbackAudioFifo.write((const quint8*)&m_feedbackAudioBuffer[0], m_feedbackAudioBufferFill);

        if (res != m_feedbackAudioBufferFill)
        {
            qDebug("NFMModSource::pushFeedback: %u/%u audio samples written m_feedbackInterpolatorDistance: %f",
                res, m_feedbackAudioBufferFill, m_feedbackInterpolatorDistance);
            m_feedbackAudioFifo.clear();
        }

        m_feedbackAudioBufferFill = 0;
    }
}

void NFMModSource::calculateLevel(Real& sample)
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

void NFMModSource::applyAudioSampleRate(unsigned int sampleRate)
{
    qDebug("NFMModSource::applyAudioSampleRate: %u", sampleRate);

    m_interpolatorDistanceRemain = 0;
    m_interpolatorConsumed = false;
    m_interpolatorDistance = (Real) sampleRate / (Real) m_channelSampleRate;
    m_interpolator.create(48, sampleRate, m_settings.m_rfBandwidth / 2.2, 3.0);
    m_lowpass.create(301, sampleRate, 250.0);
    m_bandpass.create(301, sampleRate, 300.0, m_settings.m_afBandwidth);
    m_toneNco.setFreq(m_settings.m_toneFrequency, sampleRate);
    m_ctcssNco.setFreq(NFMModSettings::getCTCSSFreq(m_settings.m_ctcssIndex), sampleRate);
    m_cwKeyer.setSampleRate(sampleRate);
    m_cwKeyer.reset();
    m_preemphasisFilter.configure(m_preemphasis*sampleRate);
    m_audioSampleRate = sampleRate;
    applyFeedbackAudioSampleRate(m_feedbackAudioSampleRate);
}

void NFMModSource::applyFeedbackAudioSampleRate(unsigned int sampleRate)
{
    qDebug("NFMModSource::applyFeedbackAudioSampleRate: %u", sampleRate);

    m_feedbackInterpolatorDistanceRemain = 0;
    m_feedbackInterpolatorConsumed = false;
    m_feedbackInterpolatorDistance = (Real) sampleRate / (Real) m_audioSampleRate;
    Real cutoff = std::min(sampleRate, m_audioSampleRate) / 2.2f;
    m_feedbackInterpolator.create(48, sampleRate, cutoff, 3.0);
    m_feedbackAudioSampleRate = sampleRate;
}

void NFMModSource::applySettings(const NFMModSettings& settings, bool force)
{
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth)
     || (settings.m_afBandwidth != m_settings.m_afBandwidth) || force)
    {
        m_settings.m_rfBandwidth = settings.m_rfBandwidth;
        m_settings.m_afBandwidth = settings.m_afBandwidth;
        applyAudioSampleRate(m_audioSampleRate);
    }

    if ((settings.m_toneFrequency != m_settings.m_toneFrequency) || force) {
        m_toneNco.setFreq(settings.m_toneFrequency, m_audioSampleRate);
    }

    if ((settings.m_ctcssIndex != m_settings.m_ctcssIndex) || force) {
        m_ctcssNco.setFreq(NFMModSettings::getCTCSSFreq(settings.m_ctcssIndex), m_audioSampleRate);
    }

    m_settings = settings;
}

void NFMModSource::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "NFMModSource::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset)
     || (channelSampleRate != m_channelSampleRate) || force)
    {
        m_carrierNco.setFreq(channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_audioSampleRate / (Real) channelSampleRate;
        m_interpolator.create(48, m_audioSampleRate, m_settings.m_rfBandwidth / 2.2, 3.0);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}