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

#ifndef INCLUDE_VORDEMODSCSINK_H
#define INCLUDE_VORDEMODSCSINK_H

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/agc.h"
#include "dsp/firfilter.h"
#include "dsp/goertzel.h"
#include "dsp/morsedemod.h"
#include "audio/audiofifo.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"
#include "util/messagequeue.h"
#include <QFile>
#include <QTextStream>

#include "vordemodsettings.h"

class VORDemodSCSink : public ChannelSampleSink {
public:
    VORDemodSCSink();
    ~VORDemodSCSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const VORDemodSettings& settings, bool force = false);
    void setMessageQueueToChannel(MessageQueue *messageQueue);
    void applyAudioSampleRate(int sampleRate);

    int getAudioSampleRate() const { return m_audioSampleRate; }
    double getMagSq() const { return m_magsq; }
    bool getSquelchOpen() const { return m_squelchOpen; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }

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

    int m_channelFrequencyOffset;

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

    VORDemodSettings m_settings;
    int m_channelSampleRate;
    int m_audioSampleRate;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    Real m_squelchLevel;
    uint32_t m_squelchCount;
    bool m_squelchOpen;
    DoubleBufferFIFO<Real> m_squelchDelayLine;
    double m_magsq;
    double m_magsqSum;
    double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

    MessageQueue *m_messageQueueToChannel;
    MessageQueue *m_messageQueueToGUI;

    MovingAverageUtil<Real, double, 16> m_movingAverage;
    SimpleAGC<4800> m_volumeAGC;
    Bandpass<Real> m_bandpass;

    Interpolator m_audioInterpolator;
    Real m_audioInterpolatorDistance;
    Real m_audioInterpolatorDistanceRemain;
    AudioVector m_audioBuffer;
    AudioFifo m_audioFifo;
    std::size_t m_audioBufferFill;

    NCO m_ncoRef;
    Lowpass<Complex> m_lowpassRef;
    Complex m_refPrev;
    Goertzel m_varGoertzel;
    Goertzel m_refGoertzel;
    MorseDemod m_morseDemod;

    void processOneSample(Complex &ci);
    void processOneAudioSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
};

#endif // INCLUDE_VORDEMODSCSINK_H
