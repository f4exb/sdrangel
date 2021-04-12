///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_NFMMODSOURCE_H
#define INCLUDE_NFMMODSOURCE_H

#include <QObject>
#include <QMutex>
#include <QVector>

#include <iostream>
#include <fstream>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/firfilter.h"
#include "dsp/filterrc.h"
#include "util/movingaverage.h"
#include "dsp/cwkeyer.h"
#include "audio/audiofifo.h"

#include "nfmmodsettings.h"
#include "nfmmoddcs.h"

class ChannelAPI;

class NFMModSource : public QObject, public ChannelSampleSource
{
    Q_OBJECT
public:
    NFMModSource();
    virtual ~NFMModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples);

    void setInputFileStream(std::ifstream *ifstream) { m_ifstream = ifstream; }
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
    void applySettings(const NFMModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    NFMModSettings m_settings;
    ChannelAPI *m_channel;

    NCO m_carrierNco;
    NCOF m_toneNco;
    NCOF m_ctcssNco;
    NFMModDCS m_dcsMod;
    float m_modPhasor; //!< baseband modulator phasor
    Complex m_modSample;

    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    Interpolator m_feedbackInterpolator;
    Real m_feedbackInterpolatorDistance;
    Real m_feedbackInterpolatorDistanceRemain;
    bool m_feedbackInterpolatorConsumed;

    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    Lowpass<Real> m_lowpass;
    Bandpass<Real> m_bandpass;
    HighPassFilterRC m_preemphasisFilter;

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

    std::ifstream *m_ifstream;
    CWKeyer m_cwKeyer;

    QMutex m_mutex;

    static const int m_levelNbSamples;
    static const float m_preemphasis;

    void processOneSample(Complex& ci);
    void pullAF(Real& sample);
    void pullAudio(unsigned int nbSamples);
    void pushFeedback(Real sample);
    void calculateLevel(Real& sample);
    void modulateSample();

private slots:
    void handleAudio();
};



#endif // INCLUDE_NFMMODSOURCE_H
