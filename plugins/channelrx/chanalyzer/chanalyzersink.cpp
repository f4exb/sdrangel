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

#include "chanalyzersink.h"

#include <QTime>
#include <QDebug>
#include <stdio.h>

#include "dsp/basebandsamplesink.h"

const unsigned int ChannelAnalyzerSink::m_ssbFftLen = 1024;
const unsigned int ChannelAnalyzerSink::m_corrFFTLen = 4*m_ssbFftLen;

ChannelAnalyzerSink::ChannelAnalyzerSink() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_sampleSink(nullptr)
{
	m_usb = true;
	m_magsq = 0;
	m_interpolatorDistance = 1.0f;
	m_interpolatorDistanceRemain = 0.0f;
	SSBFilter = new fftfilt(m_settings.m_lowCutoff / m_channelSampleRate, m_settings.m_bandwidth / m_channelSampleRate, m_ssbFftLen);
	DSBFilter = new fftfilt(m_settings.m_bandwidth / m_channelSampleRate, 2*m_ssbFftLen);
	RRCFilter = new fftfilt(m_settings.m_bandwidth / m_channelSampleRate, 2*m_ssbFftLen);
	m_corr = new fftcorr(2*m_corrFFTLen); // 8k for 4k effective samples
	m_pll.computeCoefficients(0.002f, 0.5f, 10.0f); // bandwidth, damping factor, loop gain

    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
	applySettings(m_settings, true);
}

ChannelAnalyzerSink::~ChannelAnalyzerSink()
{
    delete SSBFilter;
    delete DSBFilter;
    delete RRCFilter;
    delete m_corr;
}

void ChannelAnalyzerSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	fftfilt::cmplx *sideband = 0;
	Complex ci;

	for (SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if (m_settings.m_rationalDownSample)
		{
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci, sideband);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
		}
		else
		{
	        processOneSample(c, sideband);
		}
	}

	if (m_sampleSink) {
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), m_settings.m_ssb); // m_ssb = positive only
	}

	m_sampleBuffer.clear();
}

void ChannelAnalyzerSink::processOneSample(Complex& c, fftfilt::cmplx *sideband)
{
    int n_out;

    if (m_settings.m_ssb)
    {
        n_out = SSBFilter->runSSB(c, &sideband, m_usb);
    }
    else
    {
        if (m_settings.m_rrc) {
            n_out = RRCFilter->runFilt(c, &sideband);
        } else {
            n_out = DSBFilter->runDSB(c, &sideband);
        }
    }

    for (int i = 0; i < n_out; i++)
    {
        fftfilt::cmplx si = sideband[i];
        Real re = si.real() / SDR_RX_SCALEF;
        Real im = si.imag() / SDR_RX_SCALEF;
        m_magsq = re*re + im*im;
        m_channelPowerAvg(m_magsq);
        std::complex<float> mix;

        if (m_settings.m_pll)
        {
            if (m_settings.m_fll)
            {
                m_fll.feed(re, im);
                // Use -fPLL to mix (exchange PLL real and image in the complex multiplication)
                mix = si * std::conj(m_fll.getComplex());
            }
            else
            {
                m_pll.feed(re, im);
                // Use -fPLL to mix (exchange PLL real and image in the complex multiplication)
                mix = si * std::conj(m_pll.getComplex());
            }
        }

        feedOneSample(m_settings.m_pll ? mix : si, m_settings.m_fll ? m_fll.getComplex() : m_pll.getComplex());
    }
}

void ChannelAnalyzerSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "ChannelAnalyzerSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, channelSampleRate / 2.2f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) m_settings.m_rationalDownSamplerRate;

        int sinkSampleRate = m_settings.m_rationalDownSample ? m_settings.m_rationalDownSamplerRate : channelSampleRate;
        setFilters(sinkSampleRate, m_settings.m_bandwidth, m_settings.m_lowCutoff);
        m_pll.setSampleRate(sinkSampleRate);
        m_fll.setSampleRate(sinkSampleRate);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void ChannelAnalyzerSink::setFilters(int sampleRate, float bandwidth, float lowCutoff)
{
    qDebug("ChannelAnalyzerSink::setFilters: sampleRate: %d bandwidth: %f lowCutoff: %f",
            sampleRate, bandwidth, lowCutoff);

    if (bandwidth < 0)
    {
        bandwidth = -bandwidth;
        lowCutoff = -lowCutoff;
        m_usb = false;
    }
    else
    {
        m_usb = true;
    }

    if (bandwidth < 100.0f)
    {
        bandwidth = 100.0f;
        lowCutoff = 0;
    }

    SSBFilter->create_filter(lowCutoff / sampleRate, bandwidth / sampleRate);
    DSBFilter->create_dsb_filter(bandwidth / sampleRate);
    RRCFilter->create_rrc_filter(bandwidth / sampleRate, m_settings.m_rrcRolloff / 100.0);
}

void ChannelAnalyzerSink::applySettings(const ChannelAnalyzerSettings& settings, bool force)
{
    qDebug() << "ChannelAnalyzerSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rationalDownSample: " << settings.m_rationalDownSample
            << " m_rationalDownSamplerRate: " << settings.m_rationalDownSamplerRate
            << " m_rcc: " << settings.m_rrc
            << " m_rrcRolloff: " << settings.m_rrcRolloff / 100.0
            << " m_bandwidth: " << settings.m_bandwidth
            << " m_lowCutoff: " << settings.m_lowCutoff
            << " m_log2Decim: " << settings.m_log2Decim
            << " m_ssb: " << settings.m_ssb
            << " m_pll: " << settings.m_pll
            << " m_fll: " << settings.m_fll
            << " m_pllPskOrder: " << settings.m_pllPskOrder
            << " m_inputType: " << (int) settings.m_inputType;

    if ((settings.m_rationalDownSamplerRate != m_settings.m_rationalDownSamplerRate) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, m_channelSampleRate / 2.2);
        m_interpolatorDistanceRemain = 0.0f;
        m_interpolatorDistance =  (Real) m_channelSampleRate / (Real) settings.m_rationalDownSamplerRate;
    }

    if ((settings.m_rationalDownSample != m_settings.m_rationalDownSample) || force)
    {
        int sinkSampleRate = settings.m_rationalDownSample ? settings.m_rationalDownSamplerRate : m_channelSampleRate;

        setFilters(sinkSampleRate, settings.m_bandwidth, settings.m_lowCutoff);
        m_pll.setSampleRate(sinkSampleRate);
        m_fll.setSampleRate(sinkSampleRate);
    }

    if ((settings.m_bandwidth != m_settings.m_bandwidth) ||
        (settings.m_lowCutoff != m_settings.m_lowCutoff)|| force)
    {
        int sinkSampleRate = settings.m_rationalDownSample ? settings.m_rationalDownSamplerRate : m_channelSampleRate;
        setFilters(sinkSampleRate, settings.m_bandwidth, settings.m_lowCutoff);
    }

    if ((settings.m_rrcRolloff != m_settings.m_rrcRolloff) || force)
    {
        float sinkSampleRate = settings.m_rationalDownSample ? (float) settings.m_rationalDownSamplerRate : (float) m_channelSampleRate;
        RRCFilter->create_rrc_filter(settings.m_bandwidth / sinkSampleRate, settings.m_rrcRolloff / 100.0);
    }

    if (settings.m_pll != m_settings.m_pll || force)
    {
        if (settings.m_pll)
        {
            m_pll.reset();
            m_fll.reset();
        }
    }

    if (settings.m_fll != m_settings.m_fll || force)
    {
        if (settings.m_fll) {
            m_fll.reset();
        }
    }

    if (settings.m_pllPskOrder != m_settings.m_pllPskOrder || force)
    {
        if (settings.m_pllPskOrder < 32) {
            m_pll.setPskOrder(settings.m_pllPskOrder);
        }
    }

    m_settings = settings;
}

Real ChannelAnalyzerSink::getPllFrequency() const
{
    if (m_settings.m_fll) {
        return m_fll.getFreq();
    } else if (m_settings.m_pll) {
        return m_pll.getFreq();
    } else {
        return 0.0;
    }
}
