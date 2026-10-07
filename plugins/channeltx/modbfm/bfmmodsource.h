///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_BFMMODSOURCE_H
#define INCLUDE_BFMMODSOURCE_H

#include <QObject>
#include <QRecursiveMutex>
#include <QVector>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "util/movingaverage.h"
#include "dsp/cwkeyer.h"
#include "dsp/fmpreemphasis.h"
#include "audio/audiofifo.h"

#include "bfmmodsettings.h"
#include "rdsencoder.h"
#include "lookaheadlimiter.h"

class ChannelAPI;
class AudioFileReader;

class BFMModSource : public QObject, public ChannelSampleSource
{
    Q_OBJECT
public:
    BFMModSource();
    virtual ~BFMModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples);

    void setInputFileReader(AudioFileReader *audioFileReader) { m_audioFileReader = audioFileReader; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    AudioFifo *getFeedbackAudioFifo() { return &m_feedbackAudioFifo; }
    void applyAudioSampleRate(int sampleRate);
    void applyFeedbackAudioSampleRate(int sampleRate);
    int getAudioSampleRate() const { return m_audioSampleRate; }
    int getFeedbackAudioSampleRate() const { return m_feedbackAudioSampleRate; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    CWKeyer& getCWKeyer() { return m_cwKeyer; }
    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    void applySettings(const QStringList& settingsKeys, const BFMModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    BFMModSettings m_settings;
    ChannelAPI *m_channel;

    NCO m_carrierNco;
    NCO m_toneNco;
    NCO m_cwToneNco;
    float m_modPhasor; //!< baseband modulator phasor
    double m_mpxPhase; //!< phase of the common 19 kHz multiplex reference
    Complex m_modSample;
    FMPreemphasis m_preemphasisLeft;
    FMPreemphasis m_preemphasisRight;
    RDSEncoder m_rdsEncoder;
    LookaheadLimiter m_lookaheadLimiter; //!< Limits overshoot from audio interpolation

    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    Interpolator m_feedbackInterpolator;
    Real m_feedbackInterpolatorDistance;
    Real m_feedbackInterpolatorDistanceRemain;

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    fftfilt* m_rfFilter;
    static const int m_rfFilterFFTLength;
    fftfilt::cmplx *m_rfFilterBuffer;
    int m_rfFilterBufferIndex;

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    int m_audioSampleRate;
    AudioVector m_audioBuffer;
    unsigned int m_audioBufferFill;
    AudioVector m_audioReadBuffer;
    unsigned int m_audioReadBufferFill;
    AudioFifo m_audioFifo;

    int m_feedbackAudioSampleRate;
    AudioVector m_feedbackAudioBuffer;
    uint m_feedbackAudioBufferFill;
    AudioFifo m_feedbackAudioFifo;

    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;

    AudioFileReader *m_audioFileReader;
    CWKeyer m_cwKeyer;

    Real m_limiterGain;    //!< Stereo-linked peak limiter gain
    Real m_limiterRelease; //!< Per audio sample release coefficient of the limiter

    QRecursiveMutex m_mutex;

    static const int m_levelNbSamples;

    void processOneSample(Complex& ci);
    void pullAF(Real& left, Real& right);
    void pullAudio(unsigned int nbSamples);
    void discardAudio(unsigned int nbSamples);
    void pushFeedback(Complex sample);
    void calculateLevel(const Real& sample);
    void modulateAudio();
    void applyLimiter(Real& left, Real& right);
    void createAudioInterpolator(Real afBandwidth);
    void createRFFilter(Real rfBandwidth, int channelSampleRate);
    static Real audioCutoff(Real afBandwidth, int audioSampleRate);

private slots:
    void handleAudio();
};

#endif // INCLUDE_BFMMODSOURCE_H
