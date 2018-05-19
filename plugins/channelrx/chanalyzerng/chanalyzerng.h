///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_CHANALYZERNG_H
#define INCLUDE_CHANALYZERNG_H

#include <QMutex>
#include <vector>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "dsp/interpolator.h"
#include "dsp/ncof.h"
#include "dsp/fftfilt.h"
#include "dsp/phaselockcomplex.h"
#include "dsp/freqlockcomplex.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#include "chanalyzerngsettings.h"

#define ssbFftLen 1024

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class ChannelAnalyzerNG : public BasebandSampleSink, public ChannelSinkAPI {
public:
    class MsgConfigureChannelAnalyzer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ChannelAnalyzerNGSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureChannelAnalyzer* create(const ChannelAnalyzerNGSettings& settings, bool force)
        {
            return new MsgConfigureChannelAnalyzer(settings, force);
        }

    private:
        ChannelAnalyzerNGSettings m_settings;
        bool m_force;

        MsgConfigureChannelAnalyzer(const ChannelAnalyzerNGSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConfigureChannelAnalyzerOld : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int  getChannelSampleRate() const { return m_channelSampleRate; }
        Real getBandwidth() const { return m_Bandwidth; }
        Real getLoCutoff() const { return m_LowCutoff; }
        int  getSpanLog2() const { return m_spanLog2; }
        bool getSSB() const { return m_ssb; }
        bool getPLL() const { return m_pll; }
        bool getFLL() const { return m_fll; }
        unsigned int getPLLPSKOrder() const { return m_pllPskOrder; }

        static MsgConfigureChannelAnalyzerOld* create(
                int channelSampleRate,
                Real Bandwidth,
                Real LowCutoff,
                int spanLog2,
                bool ssb,
                bool pll,
                bool fll,
				unsigned int pllPskOrder)
        {
            return new MsgConfigureChannelAnalyzerOld(
                    channelSampleRate,
                    Bandwidth,
                    LowCutoff,
                    spanLog2,
                    ssb,
                    pll,
                    fll,
					pllPskOrder);
        }

    private:
        int  m_channelSampleRate;
        Real m_Bandwidth;
        Real m_LowCutoff;
        int  m_spanLog2;
        bool m_ssb;
        bool m_pll;
        bool m_fll;
        unsigned int m_pllPskOrder;

        MsgConfigureChannelAnalyzerOld(
                int channelSampleRate,
                Real Bandwidth,
                Real LowCutoff,
                int spanLog2,
                bool ssb,
                bool pll,
                bool fll,
				unsigned int pllPskOrder) :
            Message(),
            m_channelSampleRate(channelSampleRate),
            m_Bandwidth(Bandwidth),
            m_LowCutoff(LowCutoff),
            m_spanLog2(spanLog2),
            m_ssb(ssb),
            m_pll(pll),
            m_fll(fll),
			m_pllPskOrder(pllPskOrder)
        { }
    };

    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        int getCenterFrequency() const { return m_centerFrequency; }

        static MsgConfigureChannelizer* create(int sampleRate, int centerFrequency)
        {
            return new MsgConfigureChannelizer(sampleRate, centerFrequency);
        }

    private:
        int  m_sampleRate;
        int  m_centerFrequency;

        MsgConfigureChannelizer(int sampleRate, int centerFrequency) :
            Message(),
            m_sampleRate(sampleRate),
            m_centerFrequency(centerFrequency)
        { }
    };

    class MsgReportChannelSampleRateChanged : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgReportChannelSampleRateChanged* create()
        {
            return new MsgReportChannelSampleRateChanged();
        }

    private:

        MsgReportChannelSampleRateChanged() :
            Message()
        { }
    };

    ChannelAnalyzerNG(DeviceSourceAPI *deviceAPI);
	virtual ~ChannelAnalyzerNG();
	virtual void destroy() { delete this; }
	void setSampleSink(BasebandSampleSink* sampleSink) { m_sampleSink = sampleSink; }

	void configure(MessageQueue* messageQueue,
			int channelSampleRate,
			Real Bandwidth,
			Real LowCutoff,
			int spanLog2,
			bool ssb,
			bool pll,
			bool fll,
			unsigned int pllPskOrder);

	DownChannelizer *getChannelizer() { return m_channelizer; }
	int getInputSampleRate() const { return m_running.m_inputSampleRate; }
    int getChannelSampleRate() const { return m_running.m_channelSampleRate; }
	double getMagSq() const { return m_magsq; }
	bool isPllLocked() const { return m_running.m_pll && m_pll.locked(); }
    Real getPllFrequency() const { return m_pll.getFreq(); }
	Real getPllDeltaPhase() const { return m_pll.getDeltaPhi(); }
    Real getPllPhase() const { return m_pll.getPhiHat(); }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = objectName(); }
    virtual qint64 getCenterFrequency() const { return m_running.m_frequency; }

    virtual QByteArray serialize() const { return QByteArray(); }
    virtual bool deserialize(const QByteArray& data __attribute__((unused))) { return false; }

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:

	struct Config
	{
	    int m_frequency;
	    int m_inputSampleRate;
	    int m_channelSampleRate;
	    Real m_Bandwidth;
	    Real m_LowCutoff;
	    int m_spanLog2;
	    bool m_ssb;
	    bool m_pll;
	    bool m_fll;
	    unsigned int m_pllPskOrder;

	    Config() :
	        m_frequency(0),
	        m_inputSampleRate(96000),
	        m_channelSampleRate(96000),
	        m_Bandwidth(5000),
	        m_LowCutoff(300),
	        m_spanLog2(3),
	        m_ssb(false),
	        m_pll(false),
	        m_fll(false),
			m_pllPskOrder(1)
	    {}
	};

	Config m_config;
	Config m_running;

	DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;
    ChannelAnalyzerNGSettings m_settings;

    int m_inputSampleRate;
    int m_inputFrequencyOffset;
	int m_undersampleCount;
	fftfilt::cmplx m_sum;
	bool m_usb;
	double m_magsq;
	bool m_useInterpolator;

	NCOF m_nco;
	PhaseLockComplex m_pll;
	FreqLockComplex m_fll;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

	fftfilt* SSBFilter;
	fftfilt* DSBFilter;

	BasebandSampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;
	QMutex m_settingsMutex;

	void apply(bool force = false);
	void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force);
	void applySettings(const ChannelAnalyzerNGSettings& settings, bool force = false);
	void setFilters(int sampleRate, float bandwidth, float lowCutoff);

	void processOneSample(Complex& c, fftfilt::cmplx *sideband)
	{
	    int n_out;
	    int decim = 1<<m_running.m_spanLog2;

        if (m_running.m_ssb)
        {
            n_out = SSBFilter->runSSB(c, &sideband, m_usb);
        }
        else
        {
            n_out = DSBFilter->runDSB(c, &sideband);
        }

        for (int i = 0; i < n_out; i++)
        {
            // Downsample by 2^(m_scaleLog2 - 1) for SSB band spectrum display
            // smart decimation with bit gain using float arithmetic (23 bits significand)

            m_sum += sideband[i];

            if (!(m_undersampleCount++ & (decim - 1))) // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)
            {
                m_sum /= decim;
                Real re = m_sum.real() / SDR_RX_SCALED;
                Real im = m_sum.imag() / SDR_RX_SCALED;
                m_magsq = re*re + im*im;
                Real mixI = 1.0f;
                Real mixQ = 0.0f;

                if (m_running.m_pll)
                {
                    if (m_running.m_fll)
                    {
                        m_fll.feed(re, im);
                        // Use -fPLL to mix (exchange PLL real and image in the complex multiplication)
                        mixI = m_sum.real() * m_fll.getImag() - m_sum.imag() * m_fll.getReal();
                        mixQ = m_sum.real() * m_fll.getReal() + m_sum.imag() * m_fll.getImag();
//                        mixI = m_fll.getReal() * SDR_RX_SCALED;
//                        mixQ = m_fll.getImag() * SDR_RX_SCALED;
                    }
                    else
                    {
                        m_pll.feed(re, im);
                        // Use -fPLL to mix (exchange PLL real and image in the complex multiplication)
                        mixI = m_sum.real() * m_pll.getImag() - m_sum.imag() * m_pll.getReal();
                        mixQ = m_sum.real() * m_pll.getReal() + m_sum.imag() * m_pll.getImag();
                    }

                    if (m_running.m_ssb & !m_usb)
                    { // invert spectrum for LSB
                        m_sampleBuffer.push_back(Sample(mixQ, mixI));
                    }
                    else
                    {
                        m_sampleBuffer.push_back(Sample(mixI, mixQ));
                    }
                }
                else
                {
                    if (m_running.m_ssb & !m_usb)
                    { // invert spectrum for LSB
                        m_sampleBuffer.push_back(Sample(m_sum.imag(), m_sum.real()));
                    }
                    else
                    {
                        m_sampleBuffer.push_back(Sample(m_sum.real(), m_sum.imag()));
                    }
                }

                m_sum = 0;
            }
        }
	}
};

#endif // INCLUDE_CHANALYZERNG_H
