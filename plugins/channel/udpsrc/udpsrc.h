#ifndef INCLUDE_UDPSRC_H
#define INCLUDE_UDPSRC_H

#include <QMutex>
#include <QHostAddress>
#include "dsp/samplesink.h"
#include "dsp/nco.h"
#include "dsp/fftfilt.h"
#include "dsp/interpolator.h"
#include "util/message.h"
#include "audio/audiofifo.h"


#define udpFftLen 2048

class QUdpSocket;
class UDPSrcGUI;

class UDPSrc : public SampleSink {
	Q_OBJECT

public:
	enum SampleFormat {
		FormatSSB,
		FormatNFM,
		FormatS16LE,
		FormatNone
	};

	struct AudioSample {
		qint16 l;
		qint16 r;
	};

	typedef std::vector<AudioSample> AudioVector;

	UDPSrc(MessageQueue* uiMessageQueue, UDPSrcGUI* udpSrcGUI, SampleSink* spectrum);
	virtual ~UDPSrc();

	void configure(MessageQueue* messageQueue,
			SampleFormat sampleFormat,
			Real outputSampleRate,
			Real rfBandwidth,
			QString& udpAddress,
			int udpPort,
			int audioPort);
	void configureImmediate(MessageQueue* messageQueue,
			bool audioActive,
			int boost,
			int volume);
	void setSpectrum(MessageQueue* messageQueue, bool enabled);
	Real getMagSq() const { return m_magsq; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

public slots:
    void audioReadyRead();

protected:
	class MsgUDPSrcConfigure : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		SampleFormat getSampleFormat() const { return m_sampleFormat; }
		Real getOutputSampleRate() const { return m_outputSampleRate; }
		Real getRFBandwidth() const { return m_rfBandwidth; }
		const QString& getUDPAddress() const { return m_udpAddress; }
		int getUDPPort() const { return m_udpPort; }
		int getAudioPort() const { return m_audioPort; }

		static MsgUDPSrcConfigure* create(SampleFormat
				sampleFormat,
				Real sampleRate,
				Real rfBandwidth,
				QString& udpAddress,
				int udpPort,
				int audioPort)
		{
			return new MsgUDPSrcConfigure(sampleFormat,
					sampleRate,
					rfBandwidth,
					udpAddress,
					udpPort,
					audioPort);
		}

	private:
		SampleFormat m_sampleFormat;
		Real m_outputSampleRate;
		Real m_rfBandwidth;
		QString m_udpAddress;
		int m_udpPort;
		int m_audioPort;

		MsgUDPSrcConfigure(SampleFormat sampleFormat,
				Real outputSampleRate,
				Real rfBandwidth,
				QString& udpAddress,
				int udpPort,
				int audioPort) :
			Message(),
			m_sampleFormat(sampleFormat),
			m_outputSampleRate(outputSampleRate),
			m_rfBandwidth(rfBandwidth),
			m_udpAddress(udpAddress),
			m_udpPort(udpPort),
			m_audioPort(audioPort)
		{ }
	};

	class MsgUDPSrcConfigureImmediate : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		int getBoost() const { return m_boost; }
		int getVolume() const { return m_volume; }
		bool getAudioActive() const { return m_audioActive; }

		static MsgUDPSrcConfigureImmediate* create(
				bool audioActive,
				int boost,
				int volume)
		{
			return new MsgUDPSrcConfigureImmediate(
					audioActive,
					boost,
					volume);
		}

	private:
		int m_boost;
		int m_volume;
		bool m_audioActive;

		MsgUDPSrcConfigureImmediate(
				bool audioActive,
				int boost,
				int volume) :
			Message(),
			m_audioActive(audioActive),
			m_boost(boost),
			m_volume(volume)
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
	QUdpSocket *m_socket;
	QUdpSocket *m_audioSocket;

	int m_inputSampleRate;

	int m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	QHostAddress m_udpAddress;
	quint16 m_udpPort;
	quint16 m_audioPort;
	int m_boost;
	bool m_audioActive;
	bool m_audioStereo;
	int m_volume;
	Real m_magsq;

	Real m_scale;
	Complex m_last, m_this;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	fftfilt* UDPFilter;

	SampleVector m_sampleBuffer;
	SampleVector m_sampleBufferSSB;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo m_audioFifo;

	SampleSink* m_spectrum;
	bool m_spectrumEnabled;

	quint32 m_nextSSBId;
	quint32 m_nextS16leId;

	QMutex m_settingsMutex;
};

#endif // INCLUDE_UDPSRC_H
