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

#include "dsp/scopevis.h"

const unsigned int ChannelAnalyzerSink::m_ssbFftLen = 1024;
const unsigned int ChannelAnalyzerSink::m_corrFFTLen = 4*m_ssbFftLen;

ChannelAnalyzerSink::ChannelAnalyzerSink() :
    m_channelSampleRate(48000),
    m_channelFrequencyOffset(0),
    m_sinkSampleRate(48000),
    m_costasLoop(0.002, 2),
    m_scopeVis(nullptr)
{
	m_usb = true;
	m_magsq = 0;
	SSBFilter = new fftfilt(m_settings.m_lowCutoff / m_channelSampleRate, m_settings.m_bandwidth / m_channelSampleRate, m_ssbFftLen);
	DSBFilter = new fftfilt(m_settings.m_bandwidth / m_channelSampleRate, 2*m_ssbFftLen);
	RRCFilter = new fftfilt(m_settings.m_bandwidth / m_channelSampleRate, 2*m_ssbFftLen);
	m_corr = new fftcorr(2*m_corrFFTLen); // 8k for 4k effective samples
	m_pll.computeCoefficients(m_settings.m_pllBandwidth, m_settings.m_pllDampingFactor, m_settings.m_pllLoopGain);
        m_costasLoop.computeCoefficients(m_settings.m_pllBandwidth);

    applyChannelSettings(m_channelSampleRate, m_sinkSampleRate, m_channelFrequencyOffset, true);
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

	for (SampleVector::const_iterator it = begin; it < end; ++it)
	{
    	Complex ci;
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if (m_decimator.getDecim() == 1)
		{
            if (m_settings.m_rationalDownSample)
            {
                Complex cj;

                if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &cj))
                {
                    processOneSample(cj, sideband);
                    m_interpolatorDistanceRemain += m_interpolatorDistance;
                }
            }
            else
            {
    	        processOneSample(c, sideband);
            }
		}
		else
		{
            if (m_decimator.decimate(c, ci))
            {
                if (m_settings.m_rationalDownSample)
                {
                    Complex cj;

                    if (m_interpolator.decimate(&m_interpolatorDistanceRemain, ci, &cj))
                    {
                        processOneSample(cj, sideband);
                        m_interpolatorDistanceRemain += m_interpolatorDistance;
                    }
                }
                else
                {
                    processOneSample(ci, sideband);
                }
            }
        }
	}

    if (m_scopeVis)
    {
        std::vector<SampleVector::const_iterator> vbegin;
        vbegin.push_back(m_sampleBuffer.begin());
        m_scopeVis->feed(vbegin, m_sampleBuffer.end() - m_sampleBuffer.begin());
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
            // Use -fPLL to mix (exchange PLL real and image in the complex multiplication)
            if (m_settings.m_costasLoop)
            {
                m_costasLoop.feed(re, im);
                mix = si * std::conj(m_costasLoop.getComplex());
                feedOneSample(mix, m_costasLoop.getComplex());
            }
            else if (m_settings.m_fll)
            {
                m_fll.feed(re, im);
                mix = si * std::conj(m_fll.getComplex());
                feedOneSample(mix, m_fll.getComplex());
            }
            else
            {
                m_pll.feed(re, im);
                mix = si * std::conj(m_pll.getComplex());
                feedOneSample(mix, m_pll.getComplex());
            }
        }
        else
            feedOneSample(si, si);
    }
}

void ChannelAnalyzerSink::applyChannelSettings(int channelSampleRate, int sinkSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "ChannelAnalyzerSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " sinkSampleRate: " << sinkSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;
    bool doApplySampleRate = false;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate)
     || (m_sinkSampleRate != sinkSampleRate) || force)
    {
        m_interpolator.create(16, sinkSampleRate, sinkSampleRate / 4.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) sinkSampleRate / (Real) m_settings.m_rationalDownSamplerRate;

        int decim = channelSampleRate / sinkSampleRate;
        m_decimator.setLog2Decim(0);

        for (int i = 0; i < 7; i++) // find log2 between 0 and 6
        {
            if ((decim & 1) == 1)
            {
                qDebug() << "ChannelAnalyzerSink::applyChannelSettings: log2decim: " << i;
                m_decimator.setLog2Decim(i);
                break;
            }

            decim >>= 1;
        }

        doApplySampleRate = true;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_sinkSampleRate = sinkSampleRate;

    if (doApplySampleRate) {
        applySampleRate();
    }
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
            << " m_rcc: " << settings.m_rrc
            << " m_rrcRolloff: " << settings.m_rrcRolloff / 100.0
            << " m_bandwidth: " << settings.m_bandwidth
            << " m_lowCutoff: " << settings.m_lowCutoff
            << " m_log2Decim: " << settings.m_log2Decim
            << " m_rationalDownSample: " << settings.m_rationalDownSample
            << " m_rationalDownSamplerRate: " << settings.m_rationalDownSamplerRate
            << " m_ssb: " << settings.m_ssb
            << " m_pll: " << settings.m_pll
            << " m_fll: " << settings.m_fll
            << " m_costasLoop: " << settings.m_costasLoop
            << " m_pllPskOrder: " << settings.m_pllPskOrder
            << " m_pllBandwidth: " << settings.m_pllBandwidth
            << " m_pllDampingFactor: " << settings.m_pllDampingFactor
            << " m_pllLoopGain: " << settings.m_pllLoopGain
            << " m_inputType: " << (int) settings.m_inputType;
    bool doApplySampleRate = false;

    if ((settings.m_bandwidth != m_settings.m_bandwidth) ||
        (settings.m_lowCutoff != m_settings.m_lowCutoff) ||
        (settings.m_rrcRolloff != m_settings.m_rrcRolloff) || force)
    {
        doApplySampleRate = true;
    }

    if (settings.m_pll != m_settings.m_pll || force)
    {
        if (settings.m_pll)
        {
            m_pll.reset();
            m_fll.reset();
            m_costasLoop.reset();
        }
    }

    if (settings.m_fll != m_settings.m_fll || force)
    {
        if (settings.m_fll) {
            m_fll.reset();
        }
    }

    if (settings.m_costasLoop != m_settings.m_costasLoop || force)
    {
        if (settings.m_costasLoop) {
            m_costasLoop.reset();
        }
    }

    if (settings.m_pllPskOrder != m_settings.m_pllPskOrder || force)
    {
        if (settings.m_pllPskOrder < 32) {
            m_pll.setPskOrder(settings.m_pllPskOrder);
        }
        if (settings.m_pllPskOrder < 16) {
            m_costasLoop.setPskOrder(settings.m_pllPskOrder);
        }
    }

    if ((settings.m_pllBandwidth != m_settings.m_pllBandwidth)
        || (settings.m_pllDampingFactor != m_settings.m_pllDampingFactor)
        || (settings.m_pllLoopGain != m_settings.m_pllLoopGain)
        || force)
    {
        m_pll.computeCoefficients(settings.m_pllBandwidth, settings.m_pllDampingFactor, settings.m_pllLoopGain);
        m_costasLoop.computeCoefficients(settings.m_pllBandwidth);
    }

    if ((settings.m_rationalDownSample != m_settings.m_rationalDownSample) ||
        (settings.m_rationalDownSamplerRate != m_settings.m_rationalDownSamplerRate) || force)
    {
        m_interpolator.create(16, m_sinkSampleRate, m_sinkSampleRate / 4.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_sinkSampleRate / (Real) settings.m_rationalDownSamplerRate;
        doApplySampleRate = true;
    }

    m_settings = settings;

    qDebug() << "ChannelAnalyzerSink::applySettings:"
        << " m_rationalDownSample: " << settings.m_rationalDownSample;

    if (doApplySampleRate) {
        applySampleRate();
    }
}

bool ChannelAnalyzerSink::isPllLocked() const
{
    if (m_settings.m_pll)
        return m_pll.locked();
    else
        return false;
}

Real ChannelAnalyzerSink::getPllFrequency() const
{
    if (m_settings.m_costasLoop)
        return m_costasLoop.getFreq();
    else if (m_settings.m_fll)
        return m_fll.getFreq();
    else if (m_settings.m_pll)
        return m_pll.getFreq();
    else
        return 0.0;
}

Real ChannelAnalyzerSink::getPllPhase() const
{
    if (m_settings.m_costasLoop)
        return m_costasLoop.getPhiHat();
    else if (m_settings.m_pll)
        return m_pll.getPhiHat();
    else
        return 0.0f;
}

Real ChannelAnalyzerSink::getPllDeltaPhase() const
{
    if (m_settings.m_pll)
        return m_pll.getDeltaPhi();
    else
        return 0.0f;
}

int ChannelAnalyzerSink::getActualSampleRate()
{
    if (m_settings.m_rationalDownSample) {
        return m_settings.m_rationalDownSamplerRate;
    } else {
        return m_sinkSampleRate;
    }
}

void ChannelAnalyzerSink::applySampleRate()
{
    int sampleRate = getActualSampleRate();
    qDebug("ChannelAnalyzerSink::applySampleRate: sampleRate: %d m_interpolatorDistance: %f", sampleRate, m_interpolatorDistance);
    setFilters(sampleRate, m_settings.m_bandwidth, m_settings.m_lowCutoff);
    m_pll.setSampleRate(sampleRate);
    m_fll.setSampleRate(sampleRate);
    m_costasLoop.setSampleRate(sampleRate);
    RRCFilter->create_rrc_filter(m_settings.m_bandwidth / (float) sampleRate, m_settings.m_rrcRolloff / 100.0);
}
