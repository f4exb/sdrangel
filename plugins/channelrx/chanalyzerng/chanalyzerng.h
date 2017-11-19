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
#include "audio/audiofifo.h"
#include "util/message.h"

#define ssbFftLen 1024

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class ChannelAnalyzerNG : public BasebandSampleSink, public ChannelSinkAPI {
public:
    class MsgConfigureChannelAnalyzer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int  getChannelSampleRate() const { return m_channelSampleRate; }
        Real getBandwidth() const { return m_Bandwidth; }
        Real getLoCutoff() const { return m_LowCutoff; }
        int  getSpanLog2() const { return m_spanLog2; }
        bool getSSB() const { return m_ssb; }

        static MsgConfigureChannelAnalyzer* create(
                int channelSampleRate,
                Real Bandwidth,
                Real LowCutoff,
                int spanLog2,
                bool ssb)
        {
            return new MsgConfigureChannelAnalyzer(
                    channelSampleRate,
                    Bandwidth,
                    LowCutoff,
                    spanLog2,
                    ssb);
        }

    private:
        int  m_channelSampleRate;
        Real m_Bandwidth;
        Real m_LowCutoff;
        int  m_spanLog2;
        bool m_ssb;

        MsgConfigureChannelAnalyzer(
                int channelSampleRate,
                Real Bandwidth,
                Real LowCutoff,
                int spanLog2,
                bool ssb) :
            Message(),
            m_channelSampleRate(channelSampleRate),
            m_Bandwidth(Bandwidth),
            m_LowCutoff(LowCutoff),
            m_spanLog2(spanLog2),
            m_ssb(ssb)
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
	void setSampleSink(BasebandSampleSink* sampleSink) { m_sampleSink = sampleSink; }

	void configure(MessageQueue* messageQueue,
			int channelSampleRate,
			Real Bandwidth,
			Real LowCutoff,
			int spanLog2,
			bool ssb);

	DownChannelizer *getChannelizer() { return m_channelizer; }
	int getInputSampleRate() const { return m_running.m_inputSampleRate; }
    int getChannelSampleRate() const { return m_running.m_channelSampleRate; }
	double getMagSq() const { return m_magsq; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	virtual int getDeltaFrequency() const { return m_running.m_frequency; }
	virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = objectName(); }

    static const QString m_channelID;

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

	    Config() :
	        m_frequency(0),
	        m_inputSampleRate(96000),
	        m_channelSampleRate(96000),
	        m_Bandwidth(5000),
	        m_LowCutoff(300),
	        m_spanLog2(3),
	        m_ssb(false)
	    {}
	};

	Config m_config;
	Config m_running;

	DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

	int m_undersampleCount;
	fftfilt::cmplx m_sum;
	bool m_usb;
	double m_magsq;
	bool m_useInterpolator;

	NCOF m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

	fftfilt* SSBFilter;
	fftfilt* DSBFilter;

	BasebandSampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;
	QMutex m_settingsMutex;

	void apply(bool force = false);

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
                m_magsq = (m_sum.real() * m_sum.real() + m_sum.imag() * m_sum.imag())/ (1<<30);

                if (m_running.m_ssb & !m_usb)
                { // invert spectrum for LSB
                    //m_sampleBuffer.push_back(Sample(m_sum.imag() * 32768.0, m_sum.real() * 32768.0));
                    m_sampleBuffer.push_back(Sample(m_sum.imag(), m_sum.real()));
                }
                else
                {
                    //m_sampleBuffer.push_back(Sample(m_sum.real() * 32768.0, m_sum.imag() * 32768.0));
                    m_sampleBuffer.push_back(Sample(m_sum.real(), m_sum.imag()));
                }

                m_sum = 0;
            }
        }
	}
};

#endif // INCLUDE_CHANALYZERNG_H
