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

#ifndef SDRBASE_DSP_UPSAMPLECHANNELIZER_H_
#define SDRBASE_DSP_UPSAMPLECHANNELIZER_H_

#include <QObject>
#include <QMutex>
#include <algorithm>

#include "export.h"
#include "channelsamplesource.h"

#ifdef USE_SSE4_1
#include "dsp/inthalfbandfiltereo1.h"
#else
#include "dsp/inthalfbandfilterdb.h"
#endif

#define UPSAMPLECHANNELIZER_HB_FILTER_ORDER 96

class SDRBASE_API UpSampleChannelizer : public ChannelSampleSource {
public:
    UpSampleChannelizer(ChannelSampleSource* sampleSource);
    virtual ~UpSampleChannelizer();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);

protected:
    struct FilterStage {
        enum Mode {
            ModeCenter,
            ModeLowerHalf,
            ModeUpperHalf
        };

#ifdef USE_SSE4_1
        typedef bool (IntHalfbandFilterEO1<UPSAMPLECHANNELIZER_HB_FILTER_ORDER>::*WorkFunction)(Sample* sIn, Sample *sOut);
        IntHalfbandFilterEO1<UPSAMPLECHANNELIZER_HB_FILTER_ORDER>* m_filter;
#else
        typedef bool (IntHalfbandFilterDB<qint32, UPSAMPLECHANNELIZER_HB_FILTER_ORDER>::*WorkFunction)(Sample* sIn, Sample *sOut);
        IntHalfbandFilterDB<qint32, UPSAMPLECHANNELIZER_HB_FILTER_ORDER>* m_filter;
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
    int m_outputSampleRate;
    int m_requestedInputSampleRate;
    int m_requestedCenterFrequency;
    int m_currentInputSampleRate;
    int m_currentCenterFrequency;
    SampleVector m_sampleBuffer;
    Sample m_sampleIn;
    QMutex m_mutex;

    void applyConfiguration();
    void applySetting(unsigned int log2Interp, unsigned int filterChainHash);
    bool signalContainsChannel(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd) const;
    Real createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd);
    double setFilterChain(const std::vector<unsigned int>& stageIndexes); //!< returns offset in ratio of sample rate
    void freeFilterChain();

signals:
    void outputSampleRateChanged();
};




#endif // SDRBASE_DSP_UPSAMPLECHANNELIZER_H_
