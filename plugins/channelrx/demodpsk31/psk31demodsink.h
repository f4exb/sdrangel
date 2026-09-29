///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                    //
// Some code by AI                                                               //
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

#ifndef INCLUDE_PSK31DEMODSINK_H
#define INCLUDE_PSK31DEMODSINK_H

#include <functional>

#include "dsp/channelsamplesink.h"
#include "dsp/costasloop.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/nco.h"
#include "dsp/dsptypes.h"
#include "util/messagequeue.h"
#include "util/movingaverage.h"
#include "util/psk31.h"

#include "psk31demodsettings.h"

class ScopeVis;

class PSK31DemodSink : public ChannelSampleSink
{
public:
    PSK31DemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const QStringList& settingsKeys, const PSK31DemodSettings& settings, bool force = false);
    void setScopeSink(ScopeVis *scopeSink) { m_scopeSink = scopeSink; }
    void setMessageQueueToChannel(MessageQueue *queue) { m_messageQueueToChannel = queue; }
    void setCharacterSink(const std::function<void(QChar)>& sink) { m_characterSink = sink; }
#ifdef PSK31_DEMOD_SINK_BENCH
    void feedTestSample(const Complex& normalizedSample) { processOneSample(normalizedSample * SDR_RX_SCALEF); }
#endif
    double getMagSq() const { return m_magsq; }
    float getFrequencyOffset() const;
    float getSNR() const { return m_snr; }
    bool isLocked() const { return m_locked; }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magSqLevelStore.m_nbSamples;
    }

private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() : m_magsq(1e-12), m_magsqPeak(1e-12), m_nbSamples(1) {}
        double m_magsq;
        double m_magsqPeak;
        int m_nbSamples;
    };

    static const int m_internalSampleRate = 1000;
    static const int m_samplesPerSymbol = 32;
    static const int m_scopeBufferSize = 50;
    static const int m_lockConfirmSymbols = 8;
    static const int m_unlockConfirmSymbols = 2;
    static constexpr Real m_snrReferenceBandwidth = 100.0f;
    static constexpr Real m_timingGain = 0.25f;
    static constexpr Real m_maxTimingCorrection = 2.0f;

    ScopeVis *m_scopeSink;
    MessageQueue *m_messageQueueToChannel;
    PSK31DemodSettings m_settings;

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    Lowpass<Complex> m_lowpass;
    CostasLoop m_costasLoop;

    MovingAverageUtil<Real, double, 16> m_movingAverage;
    MovingAverageUtil<Real, Real, 64> m_lockAverage;
    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;
    double m_snrInPhaseSum;
    double m_snrQuadratureSum;
    int m_snrCount;
    float m_snr;

    Real m_clockCount;
    bool m_previousPolarity;
    int m_lockCount;
    int m_unlockCount;
    bool m_locked;
    int m_reportSampleCount;
    PSK31Decoder m_decoder;
    std::function<void(QChar)> m_characterSink;

    SampleVector m_scopeBuffer;
    int m_scopeBufferIndex;

    void processOneSample(Complex sample);
    void receiveSymbol(bool symbol);
    void resetDecoder();
    void resetSNR();
    void sampleToScope(const Complex& sample);
};

#endif // INCLUDE_PSK31DEMODSINK_H
