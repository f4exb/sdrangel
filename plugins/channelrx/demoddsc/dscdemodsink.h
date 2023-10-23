///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_DSCDEMODSINK_H
#define INCLUDE_DSCDEMODSINK_H

#include <QVector>
#include <QMap>
#include <QDateTime>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "util/movingaverage.h"
#include "util/movingmaximum.h"
#include "util/messagequeue.h"
#include "util/dsc.h"

#include "dscdemodsettings.h"

class ChannelAPI;
class DSCDemod;
class ScopeVis;


class DSCDemodSink : public ChannelSampleSink {
public:
    DSCDemodSink(DSCDemod *dscDemod);
    ~DSCDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const DSCDemodSettings& settings, bool force = false);
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

    double getMagSq() const { return m_magsq; }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_magsqCount > 0)
        {
            m_magsq = m_magsqSum / m_magsqCount;
            m_magSqLevelStore.m_magsq = m_magsq;
            m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        }

        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;

        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }


private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

    struct PhasingPattern {
        unsigned int m_pattern;
        unsigned int m_offset;
    };

    ScopeVis* m_scopeSink;    // Scope GUI to display baseband waveform
    DSCDemod *m_dscDemod;
    DSCDemodSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    MessageQueue *m_messageQueueToChannel;

    MovingAverageUtil<Real, double, 16> m_movingAverage;

    Lowpass<Complex> m_lowpassComplex1;
    Lowpass<Complex> m_lowpassComplex2;
    MovingMaximum<Real> m_movMax1;
    MovingMaximum<Real> m_movMax2;

    static const int m_expLength = 600;
    static const int m_samplesPerBit = DSCDemodSettings::DSCDEMOD_CHANNEL_SAMPLE_RATE / DSCDemodSettings::DSCDEMOD_BAUD_RATE;
    Complex *m_exp;
    int m_expIdx;
    int m_bit;
    bool m_data;
    bool m_dataPrev;
    double m_clockCount;
    double m_clock;
    double m_int;
    double m_rssiMagSqSum;
    int m_rssiMagSqCount;

    unsigned int m_bits;
    int m_bitCount;
    bool m_gotSOP;
    int m_errorCount;
    int m_consecutiveErrors;
    QString m_messageBuffer;

    DSCDecoder m_dscDecoder;
    static const QList<PhasingPattern> m_phasingPatterns;

    ComplexVector m_sampleBuffer[DSCDemodSettings::m_scopeStreams];
    static const int m_sampleBufferSize = DSCDemodSettings::DSCDEMOD_CHANNEL_SAMPLE_RATE / 20;
    int m_sampleBufferIndex;

    void processOneSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    void sampleToScope(Complex sample, Real abs1Filt, Real abs2Filt, Real unbiasedData, Real biasedData);
    void init();
    void receiveBit(bool bit);
};

#endif // INCLUDE_DSCDEMODSINK_H

