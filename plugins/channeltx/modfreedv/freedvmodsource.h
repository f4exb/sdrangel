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

#ifndef INCLUDE_FREEDVMODSOURCE_H
#define INCLUDE_FREEDVMODSOURCE_H

#include <QObject>
#include <QMutex>

#include <iostream>
#include <fstream>

#include "dsp/channelsamplesource.h"
#include "dsp/nco.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "util/movingaverage.h"
#include "dsp/cwkeyer.h"
#include "audio/audiofifo.h"
#include "audio/audioresampler.h"

#include "freedvmodsettings.h"

class BasebandSampleSink;

class FreeDVModSource : public QObject, public ChannelSampleSource
{
    Q_OBJECT
public:
    FreeDVModSource();
    virtual ~FreeDVModSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples);

    void setInputFileStream(std::ifstream *ifstream) { m_ifstream = ifstream; }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void applyAudioSampleRate(unsigned int sampleRate);
    CWKeyer& getCWKeyer() { return m_cwKeyer; }
    double getMagSq() const { return m_magsq; }
    void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples) const
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevelOut;
        numSamples = m_levelNbSamples;
    }
    int getAudioSampleRate() const { return m_audioSampleRate; }
    unsigned int getModemSampleRate() const { return m_modemSampleRate; }
    Real getLowCutoff() const { return m_lowCutoff; }
    Real getHiCutoff() const { return m_hiCutoff; }
    void setSpectrumSink(BasebandSampleSink *sampleSink) { m_spectrumSink = sampleSink; }

    void applySettings(const FreeDVModSettings& settings, bool force = false);
    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applyFreeDVMode(FreeDVModSettings::FreeDVMode mode);

private:
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_modemSampleRate;
    Real m_lowCutoff;
    Real m_hiCutoff;
    FreeDVModSettings m_settings;

    NCOF m_carrierNco;
    NCOF m_toneNco;
    Complex m_modSample;

    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

	fftfilt* m_SSBFilter;
	Complex* m_SSBFilterBuffer;
	int m_SSBFilterBufferIndex;
	static const int m_ssbFftLen;

	BasebandSampleSink* m_spectrumSink;
	SampleVector m_sampleBuffer;

    fftfilt::cmplx m_sum;
    int m_undersampleCount;
    int m_sumCount;

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    int m_audioSampleRate;
    AudioVector m_audioBuffer;
    unsigned int m_audioBufferFill;
    AudioVector m_audioReadBuffer;
    unsigned int m_audioReadBufferFill;
    AudioFifo m_audioFifo;

    quint32 m_levelCalcCount;
    qreal m_rmsLevel;
    qreal m_peakLevelOut;
    Real m_peakLevel;
    Real m_levelSum;

    std::ifstream *m_ifstream;
    CWKeyer m_cwKeyer;

    struct freedv *m_freeDV;
    int m_nSpeechSamples;
    int m_nNomModemSamples;
    int m_iSpeech;
    int m_iModem;
    int16_t *m_speechIn;
    int16_t *m_modOut;
    float m_scaleFactor; //!< divide by this amount to scale from int16 to float in [-1.0, 1.0] interval
    AudioResampler m_audioResampler;

    QMutex m_mutex;

    static const int m_levelNbSamples;

    void processOneSample(Complex& ci);
    void pullAF(Complex& sample);
    void pullAudio(unsigned int nbSamples);
    qint16 getAudioSample();
    void pushFeedback(Real sample);
    void calculateLevel(Complex& sample);
    void calculateLevel(qint16& sample);
    void modulateSample();

private slots:
    void handleAudio();
};



#endif // INCLUDE_FREEDVMODSOURCE_H
