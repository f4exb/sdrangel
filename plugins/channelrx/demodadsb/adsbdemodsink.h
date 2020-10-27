///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ADSBDEMODSINK_H
#define INCLUDE_ADSBDEMODSINK_H

#include <vector>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"

#include "adsbdemodsettings.h"

class ADSBDemodSink : public ChannelSampleSink {
public:
    ADSBDemodSink();
    ~ADSBDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

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

    void init(int samplesPerBit);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const ADSBDemodSettings& settings, bool force = false);
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }
    void setMessageQueueToWorker(MessageQueue *messageQueue) { m_messageQueueToWorker = messageQueue; }

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

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    ADSBDemodSettings m_settings;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    int m_sampleIdx;
    int m_sampleCount;
    int m_skipCount;            // Samples to skip, because we've already received a frame
    Real *m_sampleBuffer;
    Real *m_preamble;

    int m_totalSamples;         // These two values are derived from samplesPerBit
    int m_samplesPerChip;

    double m_magsq; //!< displayed averaged value
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    MovingAverageUtil<Real, double, 32> m_movingAverage;

    MessageQueue *m_messageQueueToGUI;
    MessageQueue *m_messageQueueToWorker;

    void processOneSample(Complex &ci);
    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }
    MessageQueue *getMessageQueueToWorker() { return m_messageQueueToWorker; }
};

#endif // INCLUDE_ADSBDEMODSINK_H
