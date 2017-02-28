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

#include <dsp/basebandsamplesink.h>
#include <QMutex>
#include <vector>
#include "dsp/ncof.h"
#include "dsp/fftfilt.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#define ssbFftLen 1024

class ChannelAnalyzerNG : public BasebandSampleSink {
public:
    ChannelAnalyzerNG(BasebandSampleSink* m_sampleSink);
	virtual ~ChannelAnalyzerNG();

	void configure(MessageQueue* messageQueue,
			int channelSampleRate,
			Real Bandwidth,
			Real LowCutoff,
			int spanLog2,
			bool ssb);

	int getSampleRate() const {	return m_running.m_inputSampleRate; }
	Real getMagSq() const { return m_magsq; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

private:
	class MsgConfigureChannelAnalyzer : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
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
			return new MsgConfigureChannelAnalyzer(channelSampleRate, Bandwidth, LowCutoff, spanLog2, ssb);
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

	struct Config
	{
	    int m_frequency;
	    int m_inputSampleRate;
	    int m_channelSampleRate;

	    Config() :
	        m_frequency(0),
	        m_inputSampleRate(96000),
	        m_channelSampleRate(96000)
	    {}
	};

	Config m_config;
	Config m_running;

	Real m_Bandwidth;
	Real m_LowCutoff;
	int m_spanLog2;
	int m_undersampleCount;
	fftfilt::cmplx m_sum;
	bool m_usb;
	bool m_ssb;
	Real m_magsq;

	NCOF m_nco;
	fftfilt* SSBFilter;
	fftfilt* DSBFilter;

	BasebandSampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;
	QMutex m_settingsMutex;

	void apply(bool force = false);
};

#endif // INCLUDE_CHANALYZERNG_H
