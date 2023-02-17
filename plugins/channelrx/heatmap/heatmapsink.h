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

#ifndef INCLUDE_HEATMAPSINK_H
#define INCLUDE_HEATMAPSINK_H

#include <QVector>
#include <QMutex>

#include "dsp/channelsamplesink.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/gaussian.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"
#include "util/messagequeue.h"
#include "util/crc.h"

#include "heatmapsettings.h"

#include <vector>
#include <iostream>
#include <fstream>

class ChannelAPI;
class HeatMap;
class ScopeVis;

class HeatMapSink : public ChannelSampleSink {
public:
    HeatMapSink(HeatMap *heatMap);
    ~HeatMapSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const HeatMapSettings& settings, bool force = false);
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

    double getMagSq() const { return m_magsq; }

    void getMagSqLevels(double& avg, double &peak, int& nbSamples)
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

        m_magsqSum = 0.0;
        m_magsqPeak = 0.0;
        m_magsqCount = 0;
    }

    void getMagLevels(double& avg, double& pulseAvg, double &maxPeak, double &minPeak)
    {
        QMutexLocker mutexLocker(&m_mutex);
        avg = m_magAvg;
        pulseAvg = m_magPulseAvg;
        maxPeak = m_magMaxPeak;
        minPeak = m_magMinPeak;
    }

    void resetMagLevels()
    {
        QMutexLocker mutexLocker(&m_mutex);
        m_magSum = 0.0;
        m_magCount = 0;
        m_magAvg = std::numeric_limits<double>::quiet_NaN();
        m_magPulseSum = 0.0;
        m_magPulseCount = 0;
        m_magPulseAvg = std::numeric_limits<double>::quiet_NaN();
        m_magMinPeak = std::numeric_limits<double>::max();
        m_magMaxPeak = -std::numeric_limits<double>::max();
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

    ScopeVis* m_scopeSink;    // Scope GUI to display filtered power
    HeatMap *m_heatMap;
    HeatMapSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_sinkSampleRate;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    // For power meter in GUI (same as other channels)
    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;
    MovingAverageUtil<Real, double, 16> m_movingAverage;

    // For heat map
    double m_magSum;
    double m_magCount;
    double m_magAvg;
    double m_magPulseSum;
    double m_magPulseCount;
    double m_magPulseAvg;
    double m_magMaxPeak;
    double m_magMinPeak;

    int m_averageCnt;
    double m_pulseThresholdLinear;

    MessageQueue *m_messageQueueToChannel;
    QMutex m_mutex;

    SampleVector m_sampleBuffer;
    int m_sampleBufferSize;
    int m_sampleBufferIndex;

    void processOneSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    void sampleToScope(Complex sample);
};

#endif // INCLUDE_HEATMAPSINK_H

