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

#ifndef INCLUDE_ILSDEMODSINK_H
#define INCLUDE_ILSDEMODSINK_H

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/agc.h"
#include "dsp/firfilter.h"
#include "dsp/fftfactory.h"
#include "dsp/fftengine.h"
#include "dsp/fftwindow.h"
#include "dsp/decimatorc.h"
#include "dsp/morsedemod.h"
#include "audio/audiofifo.h"
#include "util/movingaverage.h"
#include "util/movingmaximum.h"
#include "util/doublebufferfifo.h"
#include "util/messagequeue.h"

#include "ilsdemodsettings.h"

class ChannelAPI;
class ILSDemod;
class ScopeVis;
class SpectrumVis;

class ILSDemodSink : public ChannelSampleSink {
public:
    ILSDemodSink(ILSDemod *packetDemod);
    ~ILSDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

	void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumSink = spectrumSink; }
    void setScopeSink(ScopeVis* scopeSink) { m_scopeSink = scopeSink; }
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const ILSDemodSettings& settings, bool force = false);
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; m_morseDemod.setMessageQueueToChannel(messageQueue); }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    void applyAudioSampleRate(int sampleRate);

    int getAudioSampleRate() const { return m_audioSampleRate; }
    bool getSquelchOpen() const { return m_squelchOpen; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }

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

	SpectrumVis* m_spectrumSink;
    ScopeVis* m_scopeSink;    // Scope GUI to display baseband waveform
    ILSDemod *m_ilsDemod;
    ILSDemodSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_audioSampleRate;

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
    MovingAverageUtil<Real, double, 16> m_audioMovingAverage;

    DecimatorC m_decimator;

    int m_fftSequence;
    FFTEngine *m_fft;
    int m_fftCounter;
    FFTWindow m_fftWindow;
    static const int m_fftSize = 256; // 2.5Hz res (so 90/150Hz are centered in bins - and FT isn't too wide)
    Real m_powerCarrier;
    Real m_power90;
    Real m_power150;
    Real m_modDepth90;
    Real m_modDepth150;
    Real m_sdm;
    Real m_ddm;
    MovingAverageUtil<Real, Real, 16> m_modDepth90Average;  // ~0.5 sec
    MovingAverageUtil<Real, Real, 16> m_modDepth150Average;
    MovingAverageUtil<Real, Real, 16> m_sdmAverage;
    MovingAverageUtil<Real, Real, 16> m_ddmAverage;

    Real m_squelchLevel;
    uint32_t m_squelchCount;
    bool m_squelchOpen;
    DoubleBufferFIFO<Real> m_squelchDelayLine;
    SimpleAGC<4800> m_volumeAGC;
    Bandpass<Real> m_bandpass;
    Interpolator m_audioInterpolator;
    Real m_audioInterpolatorDistance;
    Real m_audioInterpolatorDistanceRemain;
    AudioVector m_audioBuffer;
    AudioFifo m_audioFifo;
    std::size_t m_audioBufferFill;

    SampleVector m_sampleBuffer;
    static const int m_sampleBufferSize = ILSDemodSettings::ILSDEMOD_CHANNEL_SAMPLE_RATE / 20;
    int m_sampleBufferIndex;
    SampleVector m_spectrumSampleBuffer;

    MorseDemod m_morseDemod;

    void processOneSample(Complex &ci);
    void processOneAudioSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    void sampleToScope(Complex sample, Real demod);
    void calcDDM();
    Real magSq(int bin) const;
};

#endif // INCLUDE_ILSDEMODSINK_H

