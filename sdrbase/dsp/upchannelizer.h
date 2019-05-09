///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#include <dsp/basebandsamplesource.h>
#include <vector>
#include <QMutex>
#include "export.h"
#include "util/message.h"
#ifdef USE_SSE4_1
#include "dsp/inthalfbandfiltereo1.h"
#else
#include "dsp/inthalfbandfilterdb.h"
#endif

#define UPCHANNELIZER_HB_FILTER_ORDER 96

class MessageQueue;

class SDRBASE_API UpChannelizer : public BasebandSampleSource {
    Q_OBJECT
public:
    class SDRBASE_API MsgChannelizerNotification : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        MsgChannelizerNotification(int basebandSampleRate, int samplerate, qint64 frequencyOffset) :
            Message(),
            m_basebandSampleRate(basebandSampleRate),
            m_sampleRate(samplerate),
            m_frequencyOffset(frequencyOffset)
        { }

        int getBasebandSampleRate() const { return m_basebandSampleRate; }
        int getSampleRate() const { return m_sampleRate; }
        qint64 getFrequencyOffset() const { return m_frequencyOffset; }

        static MsgChannelizerNotification* create(int basebandSampleRate, int samplerate, qint64 frequencyOffset)
        {
            return new MsgChannelizerNotification(basebandSampleRate, samplerate, frequencyOffset);
        }

    private:
        int m_basebandSampleRate;
        int m_sampleRate;
        qint64 m_frequencyOffset;
    };

    class SDRBASE_API MsgSetChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        MsgSetChannelizer(unsigned int log2Interp, unsigned int filterChainHash) :
            Message(),
            m_log2interp(log2Interp),
            m_filterChainHash(filterChainHash)
        { }

        unsigned int getLog2Interp() const { return m_log2interp; }
        unsigned int getFilterChainHash() const { return m_filterChainHash; }

    private:
        unsigned int m_log2interp;
        unsigned int m_filterChainHash;
    };

    UpChannelizer(BasebandSampleSource* sampleSink);
    virtual ~UpChannelizer();

    void configure(MessageQueue* messageQueue, int sampleRate, int centerFrequency);
    void set(MessageQueue* messageQueue, unsigned int log2Interp, unsigned int filterChainHash);
    int getOutputSampleRate() const { return m_outputSampleRate; }

    virtual void start();
    virtual void stop();
    virtual void pull(Sample& sample);
    virtual void pullAudio(int nbSamples) { if (m_sampleSource) m_sampleSource->pullAudio(nbSamples); }

    virtual bool handleMessage(const Message& cmd);

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

        bool work(Sample* sampleIn, Sample *sampleOut)
        {
            return (m_filter->*m_workFunction)(sampleIn, sampleOut);
        }
    };
    typedef std::vector<FilterStage*> FilterStages;
    FilterStages m_filterStages;
    bool m_filterChainSetMode;
    std::vector<Sample> m_stageSamples;
    BasebandSampleSource* m_sampleSource; //!< Modulator
    int m_outputSampleRate;
    int m_requestedInputSampleRate;
    int m_requestedCenterFrequency;
    int m_currentInputSampleRate;
    int m_currentCenterFrequency;
    SampleVector m_sampleBuffer;
    Sample m_sampleIn;
    QMutex m_mutex;

    void applyConfiguration();
    void applySetting(unsigned int log2Decim, unsigned int filterChainHash);
    bool signalContainsChannel(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd) const;
    Real createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd);
    double setFilterChain(const std::vector<unsigned int>& stageIndexes); //!< returns offset in ratio of sample rate
    void freeFilterChain();

signals:
    void outputSampleRateChanged();
};

#endif /* SDRBASE_DSP_UPCHANNELIZER_H_ */
