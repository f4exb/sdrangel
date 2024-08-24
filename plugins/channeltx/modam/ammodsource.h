///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#ifndef INCLUDE_AMMODSOURCE_H
#define INCLUDE_AMMODSOURCE_H

#include <QObject>
#include <QRecursiveMutex>
#include <QVector>

#include <iostream>
#include <fstream>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"
#include "audio/audiofifo.h"

#include "ammodsettings.h"

class ChannelAPI;
class CWKeyer;

class AMModSource : public QObject,  public ChannelSampleSource
{
    Q_OBJECT
public:
    AMModSource();
    ~AMModSource() final;

    void pull(SampleVector::iterator begin, unsigned int nbSamples) final;
    void pullOne(Sample& sample) final;
    void prefetch(unsigned int nbSamples) final;

    void setInputFileStream(std::ifstream *ifstream) { m_ifstream = ifstream; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    AudioFifo *getFeedbackAudioFifo() { return &m_feedbackAudioFifo; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    void applyAudioSampleRate(int sampleRate);
    void applyFeedbackAudioSampleRate(int sampleRate);
    int getAudioSampleRate() const { return m_audioSampleRate; }
    int getFeedbackAudioSampleRate() const { return m_feedbackAudioSampleRate; }
    void setCWKeyer(CWKeyer *cwKeyer) { m_cwKeyer = cwKeyer; }
    CWKeyer* getCWKeyer() { return m_cwKeyer; }
    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    void applySettings(const AMModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);

private:
    int m_channelSampleRate = 48000;
    int m_channelFrequencyOffset = 0;
    AMModSettings m_settings;
    ChannelAPI *m_channel;

    NCO m_carrierNco;
    NCOF m_toneNco;
    Complex m_modSample;

    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    Interpolator m_feedbackInterpolator;
    Real m_feedbackInterpolatorDistance;
    Real m_feedbackInterpolatorDistanceRemain;

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    int m_audioSampleRate = 48000;
    AudioVector m_audioBuffer;
    unsigned int m_audioBufferFill;
    AudioVector m_audioReadBuffer;
    unsigned int m_audioReadBufferFill;
    AudioFifo m_audioFifo;

    int m_feedbackAudioSampleRate;
    AudioVector m_feedbackAudioBuffer;
    uint m_feedbackAudioBufferFill;
    AudioFifo m_feedbackAudioFifo;
    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;
    bool m_demodBufferEnabled;

    quint32 m_levelCalcCount = 0;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel = 0.0f;
    Real m_levelSum = 0.0f;

    std::ifstream *m_ifstream = nullptr;
    CWKeyer *m_cwKeyer = nullptr;

    QRecursiveMutex m_mutex;

    static const int m_levelNbSamples;

    void processOneSample(const Complex& ci);
    void pullAF(Real& sample);
    void pullAudio(unsigned int nbSamples);
    void pushFeedback(Real sample);
    void calculateLevel(const Real& sample);
    void modulateSample();

private slots:
    void handleAudio();
};



#endif // INCLUDE_AMMODSOURCE_H
