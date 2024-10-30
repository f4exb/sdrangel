///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#ifndef INCLUDE_NAVTEXDEMODSINK_H
#define INCLUDE_NAVTEXDEMODSINK_H

#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "util/movingaverage.h"
#include "util/movingmaximum.h"
#include "util/messagequeue.h"
#include "util/navtex.h"

#include "navtexdemodsettings.h"

class ChannelAPI;
class NavtexDemod;
class ScopeVis;

class NavtexDemodSink : public ChannelSampleSink {
public:
    NavtexDemodSink();
    ~NavtexDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const NavtexDemodSettings& settings, bool force = false);
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

    ScopeVis* m_scopeSink;    // Scope GUI to display baseband waveform
    NavtexDemodSettings m_settings;
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
    static const int m_samplesPerBit = NavtexDemodSettings::NAVTEXDEMOD_CHANNEL_SAMPLE_RATE / NavtexDemodSettings::NAVTEXDEMOD_BAUD_RATE;
    Complex *m_exp;
    int m_expIdx;
    int m_bit;
    bool m_data;
    bool m_dataPrev;
    int m_clockCount;
    bool m_clock;
    double m_rssiMagSqSum;
    int m_rssiMagSqCount;

    unsigned short m_bits;
    int m_bitCount;
    bool m_gotSOP;
    int m_errorCount;
    int m_consecutiveErrors;
    QString m_messageBuffer;

    SitorBDecoder m_sitorBDecoder;

    SampleVector m_sampleBuffer;
    static const int m_sampleBufferSize = NavtexDemodSettings::NAVTEXDEMOD_CHANNEL_SAMPLE_RATE / 20;
    int m_sampleBufferIndex;

    void processOneSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    void sampleToScope(Complex sample);
    void eraseChars(int n);
    void init();
    void receiveBit(bool bit);
};

#endif // INCLUDE_NAVTEXDEMODSINK_H

