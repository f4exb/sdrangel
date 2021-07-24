///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_RADIOCLOCKSINK_H
#define INCLUDE_RADIOCLOCKSINK_H

#include <QVector>
#include <QDateTime>

#include "dsp/channelsamplesink.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/fftfactory.h"
#include "dsp/fftengine.h"
#include "dsp/fftwindow.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"
#include "util/messagequeue.h"

#include "radioclocksettings.h"

#include <vector>
#include <iostream>
#include <fstream>

class ChannelAPI;
class RadioClock;
class ScopeVis;

class RadioClockSink : public ChannelSampleSink {
public:
    RadioClockSink(RadioClock *radioClock);
    ~RadioClockSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setScopeSink(ScopeVis* scopeSink);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const RadioClockSettings& settings, bool force = false);
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

    ScopeVis* m_scopeSink;    // Scope GUI to display debug waveforms
    RadioClock *m_radioClock;
    RadioClockSettings m_settings;
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

    MovingAverageUtil<Real, double, 80> m_movingAverage;    //!< Moving average has sharpest step response of LPFs

    MovingAverageUtil<Real, double, 10*RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE> m_thresholdMovingAverage;  // Average over 10 seconds (because VVWB markers are 80% off)

    int m_data;             //!< Demod data before clocking
    int m_prevData;         //!< Previous value of m_data
    int m_sample;           //!< For scope, indicates when data is sampled

    int m_lowCount;         //!< Number of consecutive 0 samples
    int m_highCount;        //!< Number of consecutive 1 samples
    int m_periodCount;      //!< Counts from 1-RADIOCLOCK_CHANNEL_SAMPLE_RATE
    bool m_gotMinuteMarker; //!< Minute marker has been seen
    int m_second;           //!< Which second we are in
    int m_timeCode[61];     //!< Timecode from data in each second
    QDateTime m_dateTime;   //!< Decoded date and time

    int m_secondMarkers;    //!< Counts number of second markers that have been seen

    Real m_threshold;       //!< Current threshold for display on scope
    Real m_linearThreshold; //!< settings.m_threshold as a linear value rather than dB
    RadioClockSettings::DST m_dst; //!< Daylight savings time status

    // MSF demod state
    int m_timeCodeB[61];

    // TDF demod state
    PhaseDiscriminators m_phaseDiscri;  // FM demodulator
    int m_zeroCount;
    MovingAverageUtil<Real, double, 10> m_fmDemodMovingAverage;
    int m_bits[4];
    ComplexVector m_sampleBuffer[RadioClockSettings::m_scopeStreams];
    static const int m_sampleBufferSize = 60;
    int m_sampleBufferIndex;

    // WWVB state
    bool m_gotMarker;       //!< Marker in previous second

    void processOneSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    void sampleToScope(Complex sample);
    int bcd(int firstBit, int lastBit);
    int bcdMSB(int firstBit, int lastBit, int skipBit1=0, int skipBit2=0);
    int xorBits(int firstBit, int lastBit);
    bool evenParity(int firstBit, int lastBit, int parityBit);
    bool oddParity(int firstBit, int lastBit, int parityBit);

    void dcf77();
    void tdf(Complex &ci);
    void msf60();
    void wwvb();
};

#endif // INCLUDE_RADIOCLOCKSINK_H
