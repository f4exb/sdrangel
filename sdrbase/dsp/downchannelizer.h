///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_DSP_DOWNCHANNELIZER_H
#define SDRBASE_DSP_DOWNCHANNELIZER_H

#include <dsp/basebandsamplesink.h>
#include <list>
#include <QMutex>
#include "util/export.h"
#include "util/message.h"
#ifdef SDR_RX_SAMPLE_24BIT
#include "dsp/inthalfbandfilterdb.h"
#else
#ifdef USE_SSE4_1
#include "dsp/inthalfbandfiltereo1.h"
#else
#include "dsp/inthalfbandfilterdb.h"
#endif
#endif

#define DOWNCHANNELIZER_HB_FILTER_ORDER 48

class MessageQueue;

class SDRANGEL_API DownChannelizer : public BasebandSampleSink {
	Q_OBJECT
public:
	class SDRANGEL_API MsgChannelizerNotification : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		MsgChannelizerNotification(int samplerate, qint64 frequencyOffset) :
			Message(),
			m_sampleRate(samplerate),
			m_frequencyOffset(frequencyOffset)
		{ }

		int getSampleRate() const { return m_sampleRate; }
		qint64 getFrequencyOffset() const { return m_frequencyOffset; }

        static MsgChannelizerNotification* create(int samplerate, qint64 frequencyOffset)
        {
            return new MsgChannelizerNotification(samplerate, frequencyOffset);
        }

	private:
		int m_sampleRate;
		qint64 m_frequencyOffset;
	};

	DownChannelizer(BasebandSampleSink* sampleSink);
	virtual ~DownChannelizer();

	void configure(MessageQueue* messageQueue, int sampleRate, int centerFrequency);
	int getInputSampleRate() const { return m_inputSampleRate; }

	virtual void start();
	virtual void stop();
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual bool handleMessage(const Message& cmd);

protected:
	struct FilterStage {
		enum Mode {
			ModeCenter,
			ModeLowerHalf,
			ModeUpperHalf
		};

#ifdef SDR_RX_SAMPLE_24BIT
        typedef bool (IntHalfbandFilterDB<qint64, DOWNCHANNELIZER_HB_FILTER_ORDER>::*WorkFunction)(Sample* s);
        IntHalfbandFilterDB<qint64, DOWNCHANNELIZER_HB_FILTER_ORDER>* m_filter;
#else
#ifdef USE_SSE4_1
		typedef bool (IntHalfbandFilterEO1<DOWNCHANNELIZER_HB_FILTER_ORDER>::*WorkFunction)(Sample* s);
		IntHalfbandFilterEO1<DOWNCHANNELIZER_HB_FILTER_ORDER>* m_filter;
#else
		typedef bool (IntHalfbandFilterDB<qint32, DOWNCHANNELIZER_HB_FILTER_ORDER>::*WorkFunction)(Sample* s);
		IntHalfbandFilterDB<qint32, DOWNCHANNELIZER_HB_FILTER_ORDER>* m_filter;
#endif
#endif
		WorkFunction m_workFunction;
		Mode m_mode;
		bool m_sse;

		FilterStage(Mode mode);
		~FilterStage();

		bool work(Sample* sample)
		{
			return (m_filter->*m_workFunction)(sample);
		}
	};
	typedef std::list<FilterStage*> FilterStages;
	FilterStages m_filterStages;
	BasebandSampleSink* m_sampleSink; //!< Demodulator
	int m_inputSampleRate;
	int m_requestedOutputSampleRate;
	int m_requestedCenterFrequency;
	int m_currentOutputSampleRate;
	int m_currentCenterFrequency;
	SampleVector m_sampleBuffer;
	QMutex m_mutex;

	void applyConfiguration();
	bool signalContainsChannel(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd) const;
	Real createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd);
	void freeFilterChain();
	void debugFilterChain();

signals:
	void inputSampleRateChanged();
};

#endif // SDRBASE_DSP_DOWNCHANNELIZER_H
