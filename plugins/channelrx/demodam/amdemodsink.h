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

#ifndef INCLUDE_AMDEMODSINK_H
#define INCLUDE_AMDEMODSINK_H

#include <QVector>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/agc.h"
#include "dsp/firfilter.h"
#include "dsp/phaselockcomplex.h"
#include "audio/audiofifo.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"

#include "amdemodsettings.h"

class fftfilt;
class ChannelAPI;

class AMDemodSink : public ChannelSampleSink {
public:
    AMDemodSink();
	~AMDemodSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

	void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force = false);
    void applySettings(const AMDemodSettings& settings, bool force = false);
    void applyAudioSampleRate(int sampleRate);

    int getAudioSampleRate() const { return m_audioSampleRate; }
	double getMagSq() const { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }
	bool getPllLocked() const { return m_settings.m_pll && m_pll.locked(); }
	Real getPllFrequency() const { return m_pll.getFreq(); }
    AudioFifo *getAudioFifo() { return &m_audioFifo; }
    void setAudioFifoLabel(const QString& label) { m_audioFifo.setLabel(label); }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }

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

	enum RateState {
		RSInitialFill,
		RSRunning
	};

    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    AMDemodSettings m_settings;
    ChannelAPI *m_channel;
    int m_audioSampleRate;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;

	Real m_squelchLevel;
	int m_squelchCount;
	bool m_squelchOpen;
	DoubleBufferFIFO<Real> m_squelchDelayLine;
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
	int  m_magsqCount;
	MagSqLevelsStore m_magSqLevelStore;

	MovingAverageUtil<Real, double, 16> m_movingAverage;
	SimpleAGC<4800> m_volumeAGC;
    Bandpass<Real> m_bandpass;
    Lowpass<Real> m_lowpass;
    Lowpass<std::complex<float> > m_pllFilt;
    PhaseLockComplex m_pll;
    fftfilt* DSBFilter;
    fftfilt* SSBFilter;
    Real m_syncAMBuff[2*1024];
    uint32_t m_syncAMBuffIndex;
    MagAGC m_syncAMAGC;

	AudioVector m_audioBuffer;
	AudioFifo m_audioFifo;
	std::size_t m_audioBufferFill;
    QVector<qint16> m_demodBuffer;
    int m_demodBufferFill;

    void processOneSample(Complex &ci);
};

#endif // INCLUDE_AMDEMODSINK_H
