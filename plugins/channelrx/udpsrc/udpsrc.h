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

#ifndef INCLUDE_UDPSRC_H
#define INCLUDE_UDPSRC_H

#include <dsp/basebandsamplesink.h>
#include <QMutex>
#include <QHostAddress>
#include "dsp/nco.h"
#include "dsp/fftfilt.h"
#include "dsp/interpolator.h"
#include "dsp/phasediscri.h"
#include "util/udpsink.h"
#include "util/message.h"
#include "audio/audiofifo.h"


class QUdpSocket;
class UDPSrcGUI;

class UDPSrc : public BasebandSampleSink {
	Q_OBJECT

public:
	enum SampleFormat {
		FormatS16LE,
		FormatNFM,
		FormatNFMMono,
		FormatLSB,
		FormatUSB,
		FormatLSBMono,
		FormatUSBMono,
		FormatAMMono,
		FormatNone
	};

	struct AudioSample {
		qint16 l;
		qint16 r;
	};

	typedef std::vector<AudioSample> AudioVector;

	UDPSrc(MessageQueue* uiMessageQueue, UDPSrcGUI* udpSrcGUI, BasebandSampleSink* spectrum);
	virtual ~UDPSrc();

	void configure(MessageQueue* messageQueue,
			SampleFormat sampleFormat,
			Real outputSampleRate,
			Real rfBandwidth,
			int fmDeviation,
			QString& udpAddress,
			int udpPort,
			int audioPort);
	void configureImmediate(MessageQueue* messageQueue,
			bool audioActive,
			bool audioStereo,
			Real gain,
			int volume);
	void setSpectrum(MessageQueue* messageQueue, bool enabled);
	double getMagSq() const { return m_magsq; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	static const int udpBlockSize = 512; // UDP block size in number of bytes

public slots:
    void audioReadyRead();

protected:
	class MsgUDPSrcConfigure : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		SampleFormat getSampleFormat() const { return m_sampleFormat; }
		Real getOutputSampleRate() const { return m_outputSampleRate; }
		Real getRFBandwidth() const { return m_rfBandwidth; }
		int getFMDeviation() const { return m_fmDeviation; }
		const QString& getUDPAddress() const { return m_udpAddress; }
		int getUDPPort() const { return m_udpPort; }
		int getAudioPort() const { return m_audioPort; }

		static MsgUDPSrcConfigure* create(SampleFormat
				sampleFormat,
				Real sampleRate,
				Real rfBandwidth,
				int fmDeviation,
				QString& udpAddress,
				int udpPort,
				int audioPort)
		{
			return new MsgUDPSrcConfigure(sampleFormat,
					sampleRate,
					rfBandwidth,
					fmDeviation,
					udpAddress,
					udpPort,
					audioPort);
		}

	private:
		SampleFormat m_sampleFormat;
		Real m_outputSampleRate;
		Real m_rfBandwidth;
		int m_fmDeviation;
		QString m_udpAddress;
		int m_udpPort;
		int m_audioPort;

		MsgUDPSrcConfigure(SampleFormat sampleFormat,
				Real outputSampleRate,
				Real rfBandwidth,
				int fmDeviation,
				QString& udpAddress,
				int udpPort,
				int audioPort) :
			Message(),
			m_sampleFormat(sampleFormat),
			m_outputSampleRate(outputSampleRate),
			m_rfBandwidth(rfBandwidth),
			m_fmDeviation(fmDeviation),
			m_udpAddress(udpAddress),
			m_udpPort(udpPort),
			m_audioPort(audioPort)
		{ }
	};

	class MsgUDPSrcConfigureImmediate : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getGain() const { return m_gain; }
		int getVolume() const { return m_volume; }
		bool getAudioActive() const { return m_audioActive; }
		bool getAudioStereo() const { return m_audioStereo; }

		static MsgUDPSrcConfigureImmediate* create(
				bool audioActive,
				bool audioStereo,
				int boost,
				int volume)
		{
			return new MsgUDPSrcConfigureImmediate(
					audioActive,
					audioStereo,
					boost,
					volume);
		}

	private:
		Real m_gain;
		int m_volume;
		bool m_audioActive;
		bool m_audioStereo;

		MsgUDPSrcConfigureImmediate(
				bool audioActive,
				bool audioStereo,
				Real gain,
				int volume) :
			Message(),
			m_gain(gain),
            m_volume(volume),
			m_audioActive(audioActive),
			m_audioStereo(audioStereo)
		{ }
	};

	class MsgUDPSrcSpectrum : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getEnabled() const { return m_enabled; }

		static MsgUDPSrcSpectrum* create(bool enabled)
		{
			return new MsgUDPSrcSpectrum(enabled);
		}

	private:
		bool m_enabled;

		MsgUDPSrcSpectrum(bool enabled) :
			Message(),
			m_enabled(enabled)
		{ }
	};

	MessageQueue* m_uiMessageQueue;
	UDPSrcGUI* m_udpSrcGUI;
	QUdpSocket *m_audioSocket;

	int m_inputSampleRate;

	int m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	QString m_udpAddressStr;
	quint16 m_udpPort;
	quint16 m_audioPort;
	Real m_gain;
	bool m_audioActive;
	bool m_audioStereo;
	int m_volume;
	int m_fmDeviation;
	double m_magsq;

	Real m_scale;
	Complex m_last, m_this;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	fftfilt* UDPFilter;

	SampleVector m_sampleBuffer;
	UDPSink<Sample> *m_udpBuffer;
	UDPSink<FixReal> *m_udpBufferMono;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo m_audioFifo;

	BasebandSampleSink* m_spectrum;
	bool m_spectrumEnabled;

	quint32 m_nextSSBId;
	quint32 m_nextS16leId;

	char *m_udpAudioBuf;
	static const int m_udpAudioPayloadSize = 8192; //!< UDP audio samples buffer. No UDP block on Earth is larger than this

    PhaseDiscriminators m_phaseDiscri;

	QMutex m_settingsMutex;
};

#endif // INCLUDE_UDPSRC_H
