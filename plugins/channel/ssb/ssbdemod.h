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

#ifndef INCLUDE_SSBDEMOD_H
#define INCLUDE_SSBDEMOD_H

#include <QMutex>
#include <vector>
#include "dsp/samplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#define ssbFftLen 1024

class SSBDemod : public SampleSink {
public:
	SSBDemod(SampleSink* sampleSink);
	virtual ~SSBDemod();

	void configure(MessageQueue* messageQueue,
			Real Bandwidth,
			Real LowCutoff,
			Real volume,
			int spanLog2,
			bool audioBinaural,
			bool audioFlipChannels);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	Real getMagSq() const { return m_magsq; }

private:
	class MsgConfigureSSBDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getBandwidth() const { return m_Bandwidth; }
		Real getLoCutoff() const { return m_LowCutoff; }
		Real getVolume() const { return m_volume; }
		int  getSpanLog2() const { return m_spanLog2; }
		bool getAudioBinaural() const { return m_audioBinaural; }
		bool getAudioFlipChannels() const { return m_audioFlipChannels; }

		static MsgConfigureSSBDemod* create(Real Bandwidth,
				Real LowCutoff,
				Real volume,
				int spanLog2,
				bool audioBinaural,
				bool audioFlipChannels)
		{
			return new MsgConfigureSSBDemod(Bandwidth, LowCutoff, volume, spanLog2, audioBinaural, audioFlipChannels);
		}

	private:
		Real m_Bandwidth;
		Real m_LowCutoff;
		Real m_volume;
		int  m_spanLog2;
		bool m_audioBinaural;
		bool m_audioFlipChannels;

		MsgConfigureSSBDemod(Real Bandwidth,
				Real LowCutoff,
				Real volume,
				int spanLog2,
				bool audioBinaural,
				bool audioFlipChannels) :
			Message(),
			m_Bandwidth(Bandwidth),
			m_LowCutoff(LowCutoff),
			m_volume(volume),
			m_spanLog2(spanLog2),
			m_audioBinaural(audioBinaural),
			m_audioFlipChannels(audioFlipChannels)
		{ }
	};

	struct AudioSample {
		qint16 l;
		qint16 r;
	};

	typedef std::vector<AudioSample> AudioVector;

	Real m_Bandwidth;
	Real m_LowCutoff;
	Real m_volume;
	int m_spanLog2;
	int m_undersampleCount;
	int m_sampleRate;
	int m_frequency;
	bool m_audioBinaual;
	bool m_audioFlipChannels;
	bool m_usb;
	Real m_magsq;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	fftfilt* SSBFilter;

	SampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo m_audioFifo;
	quint32 m_audioSampleRate;

	QMutex m_settingsMutex;
};

#endif // INCLUDE_SSBDEMOD_H
