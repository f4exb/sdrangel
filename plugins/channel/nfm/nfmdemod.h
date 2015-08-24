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

#ifndef INCLUDE_NFMDEMOD_H
#define INCLUDE_NFMDEMOD_H

#include <vector>
#include "dsp/samplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/bandpass.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/ctcssdetector.h"
#include "dsp/afsquelch.h"
#include "audio/audiofifo.h"
#include "util/message.h"

class AudioFifo;
class NFMDemodGUI;

class NFMDemod : public SampleSink {
public:
	NFMDemod();
	~NFMDemod();

	void configure(MessageQueue* messageQueue, Real rfBandwidth, Real afBandwidth, Real volume, Real squelch);

	virtual void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	void registerGUI(NFMDemodGUI *nfmDemodGUI) {
		m_nfmDemodGUI = nfmDemodGUI;
	}

	const Real *getCtcssToneSet(int& nbTones) const {
		nbTones = m_ctcssDetector.getNTones();
		return m_ctcssDetector.getToneSet();
	}

	void setSelectedCtcssIndex(int selectedCtcssIndex) {
		m_ctcssIndexSelected = selectedCtcssIndex;
	}

private:
	class MsgConfigureNFMDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getRFBandwidth() const { return m_rfBandwidth; }
		Real getAFBandwidth() const { return m_afBandwidth; }
		Real getVolume() const { return m_volume; }
		Real getSquelch() const { return m_squelch; }

		static MsgConfigureNFMDemod* create(Real rfBandwidth, Real afBandwidth, Real volume, Real squelch)
		{
			return new MsgConfigureNFMDemod(rfBandwidth, afBandwidth, volume, squelch);
		}

	private:
		Real m_rfBandwidth;
		Real m_afBandwidth;
		Real m_volume;
		Real m_squelch;

		MsgConfigureNFMDemod(Real rfBandwidth, Real afBandwidth, Real volume, Real squelch) :
			Message(),
			m_rfBandwidth(rfBandwidth),
			m_afBandwidth(afBandwidth),
			m_volume(volume),
			m_squelch(squelch)
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
		int  m_ctcssIndex;
		quint32 m_audioSampleRate;

		Config() :
			m_inputSampleRate(-1),
			m_inputFrequencyOffset(0),
			m_rfBandwidth(-1),
			m_afBandwidth(-1),
			m_squelch(0),
			m_volume(0),
			m_ctcssIndex(0),
			m_audioSampleRate(0)
		{ }
	};

	Config m_config;
	Config m_running;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	Lowpass<Real> m_lowpass;
	Bandpass<Real> m_bandpass;
	CTCSSDetector m_ctcssDetector;
	int m_ctcssIndex; // 0 for nothing detected
	int m_ctcssIndexSelected;
	int m_sampleCount;

	double m_squelchLevel;
	//int m_squelchState;
	AFSquelch m_afSquelch;
	bool m_squelchOpen;

	Real m_lastArgument;
	Complex m_m1Sample;
	Complex m_m2Sample;
	MovingAverage<Real> m_movingAverage;
	AlphaAGC m_AGC;
	Real m_agcLevel; // AGC will aim to  this level
	Real m_agcFloor; // AGC will not go below this level

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;

	AudioFifo m_audioFifo;
	SampleVector m_sampleBuffer;

	NFMDemodGUI *m_nfmDemodGUI;

	void apply();
};

#endif // INCLUDE_NFMDEMOD_H
