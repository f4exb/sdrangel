///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_WFMDEMOD_H
#define INCLUDE_WFMDEMOD_H

#include <dsp/basebandsamplesink.h>
#include <QMutex>
#include <vector>
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/movingaverage.h"
#include "dsp/fftfilt.h"
#include "dsp/phasediscri.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#define rfFilterFftLength 1024

class WFMDemod : public BasebandSampleSink {
public:
	WFMDemod(BasebandSampleSink* sampleSink);
	virtual ~WFMDemod();

	void configure(
	        MessageQueue* messageQueue,
	        Real rfBandwidth,
	        Real afBandwidth,
	        Real volume,
	        Real squelch,
	        bool auduiMute);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	Real getMagSq() const { return m_movingAverage.average(); }
    bool getSquelchOpen() const { return m_squelchOpen; }

    void getMagSqLevels(Real& avg, Real& peak, int& nbSamples)
    {
        avg = m_magsqSum / m_magsqCount;
        m_magsq = avg;
        peak = m_magsqPeak;
        nbSamples = m_magsqCount;
        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

private:
	class MsgConfigureWFMDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getRFBandwidth() const { return m_rfBandwidth; }
		Real getAFBandwidth() const { return m_afBandwidth; }
		Real getVolume() const { return m_volume; }
		Real getSquelch() const { return m_squelch; }
        bool getAudioMute() const { return m_audioMute; }

		static MsgConfigureWFMDemod* create(Real rfBandwidth, Real afBandwidth, Real volume, Real squelch, bool audioMute)
		{
			return new MsgConfigureWFMDemod(rfBandwidth, afBandwidth, volume, squelch, audioMute);
		}

	private:
		Real m_rfBandwidth;
		Real m_afBandwidth;
		Real m_volume;
		Real m_squelch;
		bool m_audioMute;

		MsgConfigureWFMDemod(Real rfBandwidth, Real afBandwidth, Real volume, Real squelch, bool audioMute) :
			Message(),
			m_rfBandwidth(rfBandwidth),
			m_afBandwidth(afBandwidth),
			m_volume(volume),
			m_squelch(squelch),
			m_audioMute(audioMute)
		{ }
	};

	struct AudioSample {
		qint16 l;
		qint16 r;
	};
	typedef std::vector<AudioSample> AudioVector;

	enum RateState {
		RSInitialFill,
		RSRunning
	};

	struct Config {
		int m_inputSampleRate;
		qint64 m_inputFrequencyOffset;
		Real m_rfBandwidth;
		Real m_afBandwidth;
		Real m_squelch;
		Real m_volume;
		quint32 m_audioSampleRate;
		bool m_audioMute;

		Config() :
			m_inputSampleRate(-1),
			m_inputFrequencyOffset(0),
			m_rfBandwidth(-1),
			m_afBandwidth(-1),
			m_squelch(0),
			m_volume(0),
			m_audioSampleRate(0),
			m_audioMute(false)
		{ }
	};

	Config m_config;
	Config m_running;

	NCO m_nco;
	Interpolator m_interpolator; //!< Interpolator between sample rate sent from DSP engine and requested RF bandwidth (rational)
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	fftfilt* m_rfFilter;

	Real m_squelchLevel;
	int m_squelchState;
    bool m_squelchOpen;
    Real m_magsq; //!< displayed averaged value
    Real m_magsqSum;
    Real m_magsqPeak;
    int  m_magsqCount;

	Real m_lastArgument;
	MovingAverage<double> m_movingAverage;
	Real m_fmExcursion;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;

	BasebandSampleSink* m_sampleSink;
	AudioFifo m_audioFifo;
	SampleVector m_sampleBuffer;
	QMutex m_settingsMutex;

	PhaseDiscriminators m_phaseDiscri;

	void apply();
};

#endif // INCLUDE_WFMDEMOD_H
