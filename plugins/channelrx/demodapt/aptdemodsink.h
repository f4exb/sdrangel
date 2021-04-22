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

#ifndef INCLUDE_APTDEMODSINK_H
#define INCLUDE_APTDEMODSINK_H

#include "dsp/channelsamplesink.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"
#include "util/messagequeue.h"

#include "aptdemodsettings.h"
#include <apt.h>

#include <vector>
#include <iostream>
#include <fstream>

// FIXME: Use lower sample rate for better SNR?
// Do we want an audio filter? Subcarrier at 2800Hz. Does libaptdec have one?
#define APTDEMOD_AUDIO_SAMPLE_RATE 48000
// Lines are 2 per second -> 4160 words per second

class APTDemod;

class APTDemodSink : public ChannelSampleSink {
public:
    APTDemodSink(APTDemod *packetDemod);
    ~APTDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const APTDemodSettings& settings, bool force = false);
    void setImageWorkerMessageQueue(MessageQueue *messageQueue) { m_imageWorkerMessageQueue = messageQueue; }

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

    int getSamples(float *samples, int count);
    void resetDecoder();

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

    APTDemod *m_aptDemod;
    APTDemodSettings m_settings;
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

    MessageQueue *m_imageWorkerMessageQueue;

    MovingAverageUtil<Real, double, 16> m_movingAverage;

    PhaseDiscriminators m_phaseDiscri;

    // Audio buffer - should probably use a FIFO
    float *m_samples;
    int m_sampleCount;
    int m_samplesLength;
    int m_readIdx;
    int m_writeIdx;

    int m_row;          // Row of image currently being received
    int m_zenith;       // Row number of Zenith

    void processOneSample(Complex &ci);
    MessageQueue *getImageWorkerMessageQueue() { return m_imageWorkerMessageQueue; }
};

#endif // INCLUDE_APTDEMODSINK_H
