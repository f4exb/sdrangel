///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
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

#ifndef SDRBASE_DSP_UPCHANNELIZER_H_
#define SDRBASE_DSP_UPCHANNELIZER_H_

#include <QObject>
#include <algorithm>

#include "export.h"
#include "util/message.h"

#include "channelsamplesource.h"

#ifdef USE_SSE4_1
#include "dsp/inthalfbandfiltereo1.h"
#else
#include "dsp/inthalfbandfilterdb.h"
#endif

#define UPCHANNELIZER_HB_FILTER_ORDER 96

class SDRBASE_API UpChannelizer : public ChannelSampleSource {
public:
    UpChannelizer(ChannelSampleSource* sampleSource);
    virtual ~UpChannelizer();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples);

    void setInterpolation(unsigned int log2Interp, unsigned int filterChainHash);     //!< Define channelizer with interpolation factor and filter chain definition
    void setChannelization(int requestedSampleRate, qint64 requestedCenterFrequency); //!< Define channelizer with requested sample rate and center frequency (shift in the baseband)
    void setBasebandSampleRate(int basebandSampleRate, bool interp = false); //!< interp: true => use direct interpolation false => use channel configuration
    int getChannelSampleRate() const { return m_channelSampleRate; };
    int getChannelFrequencyOffset() const { return m_channelFrequencyOffset; }

protected:
    struct FilterStage {
        enum Mode {
            ModeCenter,
            ModeLowerHalf,
            ModeUpperHalf
        };

#ifdef USE_SSE4_1
        typedef bool (IntHalfbandFilterEO1<UPCHANNELIZER_HB_FILTER_ORDER>::*WorkFunction)(Sample* sIn, Sample *sOut);
        IntHalfbandFilterEO1<UPCHANNELIZER_HB_FILTER_ORDER>* m_filter;
#else
        typedef bool (IntHalfbandFilterDB<qint32, UPCHANNELIZER_HB_FILTER_ORDER>::*WorkFunction)(Sample* sIn, Sample *sOut);
        IntHalfbandFilterDB<qint32, UPCHANNELIZER_HB_FILTER_ORDER>* m_filter;
#endif
        WorkFunction m_workFunction;

        FilterStage(Mode mode);
        ~FilterStage();

        bool work(Sample* sampleIn, Sample *sampleOut) {
            return (m_filter->*m_workFunction)(sampleIn, sampleOut);
        }
    };

    typedef std::vector<FilterStage*> FilterStages;
    FilterStages m_filterStages;
    bool m_filterChainSetMode;
    std::vector<Sample> m_stageSamples;
    ChannelSampleSource* m_sampleSource; //!< Modulator
    int m_basebandSampleRate;
    int m_requestedInputSampleRate;
    int m_requestedCenterFrequency;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    unsigned int m_log2Interp;
    unsigned int m_filterChainHash;
    SampleVector m_sampleBuffer;
    Sample m_sampleIn;

    void applyChannelization();
    void applyInterpolation();
    static Real channelMinSpace(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd);
    Real createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd);
    double setFilterChain(const std::vector<unsigned int>& stageIndexes); //!< returns offset in ratio of sample rate
    void freeFilterChain();
};




#endif // SDRBASE_DSP_UPCHANNELIZER_H_
