///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_CHANALYZER_H
#define INCLUDE_CHANALYZER_H

#include <QMutex>
#include <vector>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "dsp/ncof.h"
#include "dsp/fftfilt.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#define ssbFftLen 1024

class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class ChannelAnalyzer : public BasebandSampleSink, public ChannelSinkAPI {
public:
    class MsgConfigureChannelAnalyzer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Real getBandwidth() const { return m_Bandwidth; }
        Real getLoCutoff() const { return m_LowCutoff; }
        int  getSpanLog2() const { return m_spanLog2; }
        bool getSSB() const { return m_ssb; }

        static MsgConfigureChannelAnalyzer* create(Real Bandwidth,
                Real LowCutoff,
                int spanLog2,
                bool ssb)
        {
            return new MsgConfigureChannelAnalyzer(Bandwidth, LowCutoff, spanLog2, ssb);
        }

    private:
        Real m_Bandwidth;
        Real m_LowCutoff;
        int  m_spanLog2;
        bool m_ssb;

        MsgConfigureChannelAnalyzer(Real Bandwidth,
                Real LowCutoff,
                int spanLog2,
                bool ssb) :
            Message(),
            m_Bandwidth(Bandwidth),
            m_LowCutoff(LowCutoff),
            m_spanLog2(spanLog2),
            m_ssb(ssb)
        { }
    };

    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getCenterFrequency() const { return m_centerFrequency; }

        static MsgConfigureChannelizer* create(int centerFrequency)
        {
            return new MsgConfigureChannelizer(centerFrequency);
        }

    private:
        int  m_centerFrequency;

        MsgConfigureChannelizer(int centerFrequency) :
            Message(),
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

	ChannelAnalyzer(DeviceSourceAPI *deviceAPI);
	virtual ~ChannelAnalyzer();
	void setSampleSink(BasebandSampleSink* sampleSink) { m_sampleSink = sampleSink; }

	void configure(MessageQueue* messageQueue,
			Real Bandwidth,
			Real LowCutoff,
			int spanLog2,
			bool ssb);

	int getSampleRate() const {	return m_sampleRate; }
	Real getMagSq() const { return m_magsq; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	virtual int getDeltaFrequency() const { return m_frequency; }
	virtual void getIdentifier(QString& id) { id = objectName(); }
	virtual void getTitle(QString& title) { title = objectName(); }

    static const QString m_channelID;

private slots:
    void channelSampleRateChanged();

private:
    DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

	Real m_Bandwidth;
	Real m_LowCutoff;
	int m_spanLog2;
	int m_undersampleCount;
	fftfilt::cmplx m_sum;
	int m_sampleRate;
	int m_frequency;
	bool m_usb;
	bool m_ssb;
	Real m_magsq;

	NCOF m_nco;
	fftfilt* SSBFilter;
	fftfilt* DSBFilter;

	BasebandSampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;
	QMutex m_settingsMutex;
};

#endif // INCLUDE_CHANALYZER_H
