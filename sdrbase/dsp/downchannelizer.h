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

#include <list>
#include <vector>

#include "export.h"
#include "util/message.h"
#include "dsp/inthalfbandfiltereo.h"

#include "channelsamplesink.h"

#define DOWNCHANNELIZER_HB_FILTER_ORDER 48

class SDRBASE_API DownChannelizer : public ChannelSampleSink {
public:
	DownChannelizer(ChannelSampleSink* sampleSink);
	virtual ~DownChannelizer();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setDecimation(unsigned int log2Decim, unsigned int filterChainHash);         //!< Define channelizer with decimation factor and filter chain definition
    void setChannelization(int requestedSampleRate, qint64 requestedCenterFrequency); //!< Define channelizer with requested sample rate and center frequency (shift in the baseband)
    void setBasebandSampleRate(int basebandSampleRate, bool decim = false);           //!< decim: true => use direct decimation false => use channel configuration
	int getBasebandSampleRate() const { return m_basebandSampleRate; }
    int getChannelSampleRate() const { return m_channelSampleRate; }
	int getChannelFrequencyOffset() const { return m_channelFrequencyOffset; }

protected:
	struct FilterStage {
		enum Mode {
			ModeCenter,
			ModeLowerHalf,
			ModeUpperHalf
		};

#ifdef SDR_RX_SAMPLE_24BIT
        typedef bool (IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER, true>::*WorkFunction)(Sample* s);
        IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER, true>* m_filter;
#else
        typedef bool (IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER, true>::*WorkFunction)(Sample* s);
        IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER, true>* m_filter;
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
	ChannelSampleSink* m_sampleSink; //!< Demodulator
    int m_basebandSampleRate;
	int m_requestedOutputSampleRate;
	int m_requestedCenterFrequency;
	int m_channelSampleRate;
    int m_channelFrequencyOffset;
    unsigned int m_log2Decim;
    unsigned int m_filterChainHash;
	SampleVector m_sampleBuffer;

	void applyChannelization();
    void applyDecimation();
    static Real channelMinSpace(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd);
	Real createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd);
    double setFilterChain(const std::vector<unsigned int>& stageIndexes);
	void freeFilterChain();
	void debugFilterChain();
};

#endif // SDRBASE_DSP_DOWNCHANNELIZER_H
