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

#ifndef INCLUDE_USBDEMOD_H
#define INCLUDE_USBDEMOD_H

#include <vector>
#include "dsp/samplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/fftfilt.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#define usbFftLen 1024

class AudioFifo;

class USBDemod : public SampleSink {
public:
	USBDemod(AudioFifo* audioFifo, SampleSink* sampleSink);
	~USBDemod();

	void configure(MessageQueue* messageQueue, Real Bandwidth, Real volume);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	void start();
	void stop();
	bool handleMessage(Message* cmd);

private:
	class MsgConfigureUSBDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getBandwidth() const { return m_Bandwidth; }
		Real getVolume() const { return m_volume; }

		static MsgConfigureUSBDemod* create(Real Bandwidth, Real volume)
		{
			return new MsgConfigureUSBDemod(Bandwidth, volume);
		}

	private:
		Real m_Bandwidth;
		Real m_volume;

		MsgConfigureUSBDemod(Real Bandwidth, Real volume) :
			Message(),
			m_Bandwidth(Bandwidth),
			m_volume(volume)
		{ }
	};

	struct AudioSample {
		qint16 l;
		qint16 r;
	};
	typedef std::vector<AudioSample> AudioVector;

	Real m_Bandwidth;
	Real m_volume;
	int m_undersampleCount;
	int m_sampleRate;
	int m_frequency;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	Lowpass<Real> m_lowpass;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo* m_audioFifo;
	fftfilt* USBFilter;

	SampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;
};

#endif // INCLUDE_USBDEMOD_H
