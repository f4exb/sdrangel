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

#ifndef INCLUDE_CHANALYZERSINK_H
#define INCLUDE_CHANALYZERSINK_H

#include "dsp/channelsamplesink.h"
#include "dsp/interpolator.h"
#include "dsp/ncof.h"
#include "dsp/fftcorr.h"
#include "dsp/fftfilt.h"
#include "dsp/phaselockcomplex.h"
#include "dsp/freqlockcomplex.h"
#include "audio/audiofifo.h"

#include "util/movingaverage.h"

#include "chanalyzersettings.h"

class BasebandSampleSink;

class ChannelAnalyzerSink : public ChannelSampleSink {
public:
    ChannelAnalyzerSink();
	~ChannelAnalyzerSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
	void applySettings(const ChannelAnalyzerSettings& settings, bool force = false);

	double getMagSq() const { return m_magsq; }
	double getMagSqAvg() const { return (double) m_channelPowerAvg; }
	bool isPllLocked() const { return m_settings.m_pll && m_pll.locked(); }
    Real getPllFrequency() const;
	Real getPllDeltaPhase() const { return m_pll.getDeltaPhi(); }
    Real getPllPhase() const { return m_pll.getPhiHat(); }
    void setSampleSink(BasebandSampleSink* sampleSink) { m_sampleSink = sampleSink; }

    static const unsigned int m_corrFFTLen;
    static const unsigned int m_ssbFftLen;

private:
    int m_channelSampleRate;
	int m_channelFrequencyOffset;
    ChannelAnalyzerSettings m_settings;

	bool m_usb;
	double m_magsq;

	NCOF m_nco;
	PhaseLockComplex m_pll;
	FreqLockComplex m_fll;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

	fftfilt* SSBFilter;
	fftfilt* DSBFilter;
	fftfilt* RRCFilter;
	fftcorr* m_corr;

	SampleVector m_sampleBuffer;
	MovingAverageUtil<double, double, 480> m_channelPowerAvg;

    BasebandSampleSink* m_sampleSink;

	void setFilters(int sampleRate, float bandwidth, float lowCutoff);
	void processOneSample(Complex& c, fftfilt::cmplx *sideband);

	inline void feedOneSample(const fftfilt::cmplx& s, const fftfilt::cmplx& pll)
	{
	    switch (m_settings.m_inputType)
	    {
	        case ChannelAnalyzerSettings::InputPLL:
            {
                if (m_settings.m_ssb & !m_usb) { // invert spectrum for LSB
                    m_sampleBuffer.push_back(Sample(pll.imag()*SDR_RX_SCALEF, pll.real()*SDR_RX_SCALEF));
                } else {
                    m_sampleBuffer.push_back(Sample(pll.real()*SDR_RX_SCALEF, pll.imag()*SDR_RX_SCALEF));
                }
            }
	            break;
	        case ChannelAnalyzerSettings::InputAutoCorr:
	        {
                std::complex<float> a = m_corr->run(s/SDR_RX_SCALEF, 0);

                if (m_settings.m_ssb & !m_usb) { // invert spectrum for LSB
                    m_sampleBuffer.push_back(Sample(a.imag(), a.real()));
                } else {
                    m_sampleBuffer.push_back(Sample(a.real(), a.imag()));
                }
	        }
	            break;
            case ChannelAnalyzerSettings::InputSignal:
            default:
            {
                if (m_settings.m_ssb & !m_usb) { // invert spectrum for LSB
                    m_sampleBuffer.push_back(Sample(s.imag(), s.real()));
                } else {
                    m_sampleBuffer.push_back(Sample(s.real(), s.imag()));
                }
            }
                break;
	    }
	}
};

#endif // INCLUDE_CHANALYZERSINK_H
