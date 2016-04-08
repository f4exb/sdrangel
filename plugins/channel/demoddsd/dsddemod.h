///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_DSDDEMOD_H
#define INCLUDE_DSDDEMOD_H

#include <dsp/phasediscri.h>
#include <QMutex>
#include <vector>
#include "dsp/samplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/bandpass.h"
#include "dsp/afsquelch.h"
#include "dsp/agc.h"
#include "dsp/afsquelch.h"
#include "audio/audiofifo.h"
#include "util/message.h"
#include "dsddecoder.h"

class DSDDemodGUI;

class DSDDemod : public SampleSink {
public:
    DSDDemod(SampleSink* sampleSink);
	~DSDDemod();

	void configure(MessageQueue* messageQueue,
			int  rfBandwidth,
			int  demodGain,
            int  volume,
			int  fmDeviation,
			int  squelchGate,
			Real squelch,
			bool audioMute);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	void registerGUI(DSDDemodGUI *dsdDemodGUI) {
		m_dsdDemodGUI = dsdDemodGUI;
	}

	Real getMag() { return m_AGC.getAverage() / (1<<15); }
	bool getSquelchOpen() const { return m_squelchOpen; }

private:
	class MsgConfigureDSDDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int  getRFBandwidth() const { return m_rfBandwidth; }
		int  getDemodGain() const { return m_demodGain; }
		int  getFMDeviation() const { return m_fmDeviation; }
        int  getVolume() const { return m_volume; }
		int  getSquelchGate() const { return m_squelchGate; }
		Real getSquelch() const { return m_squelch; }
		bool getAudioMute() const { return m_audioMute; }

		static MsgConfigureDSDDemod* create(int rfBandwidth,
				int  demodGain,
				int  fmDeviation,
				int  volume,
				int  squelchGate,
				Real squelch,
				bool audioMute)
		{
			return new MsgConfigureDSDDemod(rfBandwidth, demodGain, fmDeviation, volume, squelchGate, squelch, audioMute);
		}

	private:
		Real m_rfBandwidth;
		Real m_demodGain;
		int  m_fmDeviation;
		int  m_volume;
		int  m_squelchGate;
		Real m_squelch;
		bool m_audioMute;

		MsgConfigureDSDDemod(int rfBandwidth,
				int  demodGain,
				int  fmDeviation,
				int  volume,
				int  squelchGate,
				Real squelch,
				bool audioMute) :
			Message(),
			m_rfBandwidth(rfBandwidth),
			m_demodGain(demodGain),
			m_fmDeviation(fmDeviation),
			m_volume(volume),
			m_squelchGate(squelchGate),
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
		int  m_rfBandwidth;
		int  m_demodGain;
		int  m_volume;
		int  m_fmDeviation;
		int  m_squelchGate;
		Real m_squelch;
		bool m_audioMute;
		quint32 m_audioSampleRate;

		Config() :
			m_inputSampleRate(-1),
			m_inputFrequencyOffset(0),
			m_rfBandwidth(-1),
			m_demodGain(-1),
			m_volume(-1),
			m_fmDeviation(1),
			m_squelchGate(1),
			m_squelch(0),
			m_audioMute(false),
			m_audioSampleRate(0)
		{ }
	};

	Config m_config;
	Config m_running;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	int m_sampleCount;
	int m_squelchCount;
	int m_agcAttack;
	bool m_audioMute;

	double m_squelchLevel;
	bool m_squelchOpen;

	Real m_lastArgument;
	MagAGC m_AGC;
	AFSquelch m_afSquelch;
	Real m_agcLevel; // AGC will aim to  this level
	Real m_agcFloor; // AGC will not go below this level

	Real m_fmExcursion;

	qint16 *m_dsdInBuffer;             //!< Input buffer for DSD decoder process
	int m_dsdInCount;
	SampleVector m_scopeSampleBuffer;
	AudioVector m_audioBuffer;
	uint m_audioBufferFill;

	AudioFifo m_audioFifo;
	SampleSink* m_scope;
	bool m_scopeEnabled;

	DSDDecoder m_dsdDecoder;
	DSDDemodGUI *m_dsdDemodGUI;
	QMutex m_settingsMutex;

    PhaseDiscriminators m_phaseDiscri;

	void apply();
};

#endif // INCLUDE_DSDDEMOD_H
