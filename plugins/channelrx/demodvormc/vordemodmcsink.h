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

#ifndef INCLUDE_VORDEMODSINK_H
#define INCLUDE_VORDEMODSINK_H

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/agc.h"
#include "dsp/firfilter.h"
#include "dsp/goertzel.h"
#include "audio/audiofifo.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"
#include "util/messagequeue.h"

#include "vordemodmcsettings.h"

#include <vector>

// Highest frequency is the FM subcarrier at up to ~11kHz
// However, old VORs can have 0.005% frequency offset, which is 6kHz
#define VORDEMOD_CHANNEL_BANDWIDTH 18000
// Sample rate needs to be at least twice the above
// Also need to consider impact frequency resolution of Goertzel filters
// May as well make it a common audio rate, to possibly avoid decimation
#define VORDEMOD_CHANNEL_SAMPLE_RATE 48000

class VORDemodMCSink : public ChannelSampleSink {
public:
    VORDemodMCSink(const VORDemodMCSettings& settings, int subChannel,
                MessageQueue *messageQueueToGUI);
    ~VORDemodMCSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const VORDemodMCSettings& settings, bool force = false);
    void setMessageQueueToGUI(MessageQueue *messageQueue) { m_messageQueueToGUI = messageQueue; }
    void applyAudioSampleRate(int sampleRate);

    int getAudioSampleRate() const { return m_audioSampleRate; }
    double getMagSq() const { return m_magsq; }
    bool getSquelchOpen() const { return m_squelchOpen; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }

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

    int m_subChannelId; // The id for the VOR this demod sink was created for
    int m_vorFrequencyHz; // The VORs frequency
    int m_frequencyOffset;  // Different between sample source center frequeny and VOR center frequency
    int m_channelFrequencyOffset;
    bool m_outOfBand;

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

    VORDemodMCSettings m_settings;
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

    MessageQueue *m_messageQueueToGUI;

    MovingAverageUtil<Real, double, 16> m_movingAverage;
    SimpleAGC<4800> m_volumeAGC;
    Bandpass<Real> m_bandpass;

    Interpolator m_audioInterpolator;
    Real m_audioInterpolatorDistance;
    Real m_audioInterpolatorDistanceRemain;
    AudioVector m_audioBuffer;
    AudioFifo m_audioFifo;
    uint32_t m_audioBufferFill;

    NCO m_ncoIdent;
    NCO m_ncoRef;
    Lowpass<Complex> m_lowpassRef;
    Lowpass<Complex> m_lowpassIdent;
    Complex m_refPrev;
    MovingAverageUtilVar<Real, double> m_movingAverageIdent;
    static const int m_identBins = 10;
    Real m_identMins[m_identBins];
    Real m_identMin;
    Real m_identMaxs[m_identBins];
    Real m_identNoise;
    int m_binSampleCnt;
    int m_binCnt;
    int m_samplesPerDot7wpm;
    int m_samplesPerDot10wpm;
    int m_prevBit;
    int m_bitTime;
    QString m_ident;
    Goertzel m_varGoertzel;
    Goertzel m_refGoertzel;

    void processOneSample(Complex &ci);
    void processOneAudioSample(Complex &ci);
    MessageQueue *getMessageQueueToGUI() { return m_messageQueueToGUI; }
};

#endif // INCLUDE_VORDEMODSINK_H
