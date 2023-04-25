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

#ifndef INCLUDE_FREEDVDEMODSINK_H
#define INCLUDE_FREEDVDEMODSINK_H

#include <vector>

#include <QTimer>
#include <QRecursiveMutex>

#include "dsp/channelsamplesink.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "dsp/agc.h"
#include "audio/audiofifo.h"
#include "audio/audioresampler.h"
#include "util/doublebufferfifo.h"

#include "freedvdemodsettings.h"

struct freedv;
class BasebandSampleSink;

class FreeDVDemodSink : public ChannelSampleSink {
public:
    FreeDVDemodSink();
	~FreeDVDemodSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

	void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
	void applySettings(const FreeDVDemodSettings& settings, bool force = false);
    void applyAudioSampleRate(int sampleRate);
	void applyFreeDVMode(FreeDVDemodSettings::FreeDVMode mode);
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }
    void resyncFreeDV();

	void setSpectrumSink(BasebandSampleSink* spectrumSink) { m_spectrumSink = spectrumSink; }
    int getAudioSampleRate() const { return m_audioSampleRate; }
    uint32_t getModemSampleRate() const { return m_modemSampleRate; }
    double getMagSq() const { return m_magsq; }
	bool getAudioActive() const { return m_audioActive; }

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

	void getSNRLevels(double& avg, double& peak, int& nbSamples);
	int getBER() const { return m_freeDVStats.m_ber; }
	float getFrequencyOffset() const { return m_freeDVStats.m_freqOffset; }
	bool isSync() const { return m_freeDVStats.m_sync; }

	/**
	 * Level changed
	 * \param rmsLevel RMS level in range 0.0 - 1.0
	 * \param peakLevel Peak level in range 0.0 - 1.0
	 * \param numSamples Number of samples analyzed
	 */
	void getLevels(qreal& rmsLevel, qreal& peakLevel, int& numSamples)
    {
        rmsLevel = m_rmsLevel;
        peakLevel = m_peakLevel;
        numSamples = m_levelInNbSamples;
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

	struct FreeDVStats
	{
		FreeDVStats();
		void init();
		void collect(struct freedv *freedv);

		bool m_sync;
		float m_snrEst;
		float m_clockOffset;
		float m_freqOffset;
		float m_syncMetric;
		int m_totalBitErrors;
		int m_lastTotalBitErrors;
		int m_ber; //!< estimated BER (b/s)
		uint32_t m_frameCount;
		uint32_t m_berFrameCount; //!< count of frames for BER estimation
		uint32_t m_fps; //!< frames per second
	};

	struct FreeDVSNR
	{
		FreeDVSNR();
		void accumulate(float snrdB);

		double m_sum;
		float m_peak;
		int m_n;
		bool m_reset;
	};

	struct LevelRMS
	{
		LevelRMS();
		void accumulate(float fsample);

		double m_sum;
		float m_peak;
		int m_n;
		bool m_reset;
	};

    FreeDVDemodSettings m_settings;

	Real m_hiCutoff;
	Real m_lowCutoff;
	Real m_volume;
	int m_spanLog2;
	fftfilt::cmplx m_sum;
	int m_undersampleCount;
	int m_channelSampleRate;
	uint32_t m_modemSampleRate;
	uint32_t m_speechSampleRate;
	int m_audioSampleRate;
	int m_channelFrequencyOffset;
	bool m_audioMute;
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;
	SimpleAGC<4800> m_simpleAGC;
    bool m_agcActive;
    DoubleBufferFIFO<fftfilt::cmplx> m_squelchDelayLine;
    bool m_audioActive;         //!< True if an audio signal is produced (no AGC or AGC and above threshold)

	NCOF m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
	fftfilt* SSBFilter;
    fftfilt::cmplx* m_SSBFilterBuffer;
    unsigned int m_SSBFilterBufferIndex;

	BasebandSampleSink* m_spectrumSink;
	SampleVector m_sampleBuffer;

	AudioVector m_audioBuffer;
	std::size_t m_audioBufferFill;
	AudioFifo m_audioFifo;

    struct freedv *m_freeDV;
    int m_nSpeechSamples;
    int m_nMaxModemSamples;
    int m_nin;
    int m_iSpeech;
    int m_iModem;
    int16_t *m_speechOut;
    int16_t *m_modIn;
    AudioResampler m_audioResampler;
	FreeDVStats m_freeDVStats;
	FreeDVSNR m_freeDVSNR;
	LevelRMS m_levelIn;
	int m_levelInNbSamples;
    Real m_rmsLevel;
    Real m_peakLevel;
	QRecursiveMutex m_mutex;

    static const unsigned int m_ssbFftLen;
    static const float m_agcTarget;

	void pushSampleToDV(int16_t sample);
	void pushSampleToAudio(int16_t sample);
    void processOneSample(Complex &ci);
    void calculateLevel(int16_t& sample);
};

#endif // INCLUDE_FREEDVDEMODSINK_H
