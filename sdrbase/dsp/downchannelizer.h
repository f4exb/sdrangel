///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2019 F4EXB                                                 //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_DSP_DOWNCHANNELIZER_H
#define SDRBASE_DSP_DOWNCHANNELIZER_H

#include <dsp/basebandsamplesink.h>
#include <list>
#include <vector>
#include <QMutex>
#include "export.h"
#include "util/message.h"
#include "dsp/inthalfbandfiltereo.h"

#define DOWNCHANNELIZER_HB_FILTER_ORDER 48

class MessageQueue;

class SDRBASE_API DownChannelizer : public BasebandSampleSink {
	Q_OBJECT
public:
    class SDRBASE_API MsgChannelizerNotification : public Message {
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

    class SDRBASE_API MsgSetChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        MsgSetChannelizer(unsigned int log2Decim, unsigned int filterChainHash) :
            Message(),
            m_log2Decim(log2Decim),
            m_filterChainHash(filterChainHash)
        { }

        unsigned int getLog2Decim() const { return m_log2Decim; }
        unsigned int getFilterChainHash() const { return m_filterChainHash; }

    private:
        unsigned int m_log2Decim;
        unsigned int m_filterChainHash;
    };

	DownChannelizer(BasebandSampleSink* sampleSink);
	virtual ~DownChannelizer();

	void configure(MessageQueue* messageQueue, int sampleRate, int centerFrequency);
    void set(MessageQueue* messageQueue, unsigned int log2Decim, unsigned int filterChainHash);
	int getInputSampleRate() const { return m_inputSampleRate; }
	int getRequestedCenterFrequency() const { return m_requestedCenterFrequency; }

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
        typedef bool (IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER>::*WorkFunction)(Sample* s);
        IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER>* m_filter;
#else
        typedef bool (IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER>::*WorkFunction)(Sample* s);
        IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER>* m_filter;
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
    bool m_filterChainSetMode;
	BasebandSampleSink* m_sampleSink; //!< Demodulator
	int m_inputSampleRate;
	int m_requestedOutputSampleRate;
	int m_requestedCenterFrequency;
	int m_currentOutputSampleRate;
	int m_currentCenterFrequency;
	SampleVector m_sampleBuffer;
	QMutex m_mutex;

	void applyConfiguration();
    void applySetting(unsigned int log2Decim, unsigned int filterChainHash);
	bool signalContainsChannel(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd) const;
	Real createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd);
    void setFilterChain(const std::vector<unsigned int>& stageIndexes);
	void freeFilterChain();
	void debugFilterChain();

signals:
	void inputSampleRateChanged();
};

#endif // SDRBASE_DSP_DOWNCHANNELIZER_H
