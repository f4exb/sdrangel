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
#include "dsp/movingaverage.h"
#include "audio/audiofifo.h"
#include "util/message.h"

class AudioFifo;

class NFMDemod : public SampleSink {
public:
	NFMDemod(AudioFifo* audioFifo, SampleSink* sampleSink);
	~NFMDemod();

	void configure(MessageQueue* messageQueue, Real rfBandwidth, Real afBandwidth, Real volume, Real squelch);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst);
	void start();
	void stop();
	bool handleMessage(Message* cmd);

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

	Real m_rfBandwidth;
	Real m_volume;
	Real m_squelchLevel;
	int m_sampleRate;
	int m_frequency;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	Lowpass<Real> m_lowpass;

	int m_squelchState;

	Complex m_lastSample;
	MovingAverage m_movingAverage;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo* m_audioFifo;

	SampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;
};

#endif // INCLUDE_NFMDEMOD_H
