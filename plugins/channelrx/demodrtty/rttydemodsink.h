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

#ifndef INCLUDE_RTTYDEMODSINK_H
#define INCLUDE_RTTYDEMODSINK_H

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/raisedcosine.h"
#include "dsp/fftfactory.h"
#include "dsp/fftengine.h"
#include "util/movingaverage.h"
#include "util/movingmaximum.h"
#include "util/doublebufferfifo.h"
#include "util/messagequeue.h"

#include "rttydemodsettings.h"

class ChannelAPI;
class RttyDemod;
class ScopeVis;


class RttyDemodSink : public ChannelSampleSink {
public:
    RttyDemodSink(RttyDemod *packetDemod);
    ~RttyDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const RttyDemodSettings& settings, bool force = false);
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
    RttyDemod *m_rttyDemod;
    RttyDemodSettings m_settings;
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
    Lowpass<Real> m_envelope1;
    Lowpass<Real> m_envelope2;
    Lowpass<Real> m_lowpass1;
    Lowpass<Real> m_lowpass2;
    Lowpass<Complex> m_lowpassComplex1;
    Lowpass<Complex> m_lowpassComplex2;
    RaisedCosine<Complex> m_raisedCosine1;
    RaisedCosine<Complex> m_raisedCosine2;

    MovingMaximum<Real> m_movMax1;
    MovingMaximum<Real> m_movMax2;

    int m_expLength;
    int m_samplesPerBit;
    Complex *m_prods1;
    Complex *m_prods2;
    Complex *m_exp;
    Complex m_sum1;
    Complex m_sum2;
    int m_sampleIdx;
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
    BaudotDecoder m_rttyDecoder;

    // For baud rate estimation
    unsigned int m_cycleCount;
    std::vector<int> m_clockHistogram;
    int m_edgeCount;
    MovingAverageUtil<Real, Real, 5> m_baudRateAverage;

    // For frequency shift estimation
    std::vector<Real> m_shiftEstMag;
    int m_fftSequence;
    FFTEngine *m_fft;
    int m_fftCounter;
    static const int m_fftSize = 128; // ~7Hz res
    MovingAverageUtil<Real, Real, 16> m_freq1Average;
    MovingAverageUtil<Real, Real, 16> m_freq2Average;

    SampleVector m_sampleBuffer;
    static const int m_sampleBufferSize = RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE / 20;
    int m_sampleBufferIndex;

    void processOneSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    void sampleToScope(Complex sample);
    void init();
    void receiveBit(bool bit);
    int estimateBaudRate();
    void estimateFrequencyShift();
};

#endif // INCLUDE_RTTYDEMODSINK_H

