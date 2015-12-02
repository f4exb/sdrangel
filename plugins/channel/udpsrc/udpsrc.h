#ifndef INCLUDE_UDPSRC_H
#define INCLUDE_UDPSRC_H

#include <QMutex>
#include <QHostAddress>
#include "dsp/samplesink.h"
#include "dsp/nco.h"
#include "dsp/fftfilt.h"
#include "dsp/interpolator.h"
#include "util/message.h"

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

	UDPSrc(MessageQueue* uiMessageQueue, UDPSrcGUI* udpSrcGUI, SampleSink* spectrum);
	virtual ~UDPSrc();

	void configure(MessageQueue* messageQueue, SampleFormat sampleFormat, Real outputSampleRate, Real rfBandwidth, QString& udpAddress, int udpPort, int boost);
	void setSpectrum(MessageQueue* messageQueue, bool enabled);
	Real getMagSq() const { return m_magsq; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

protected:
	class MsgUDPSrcConfigure : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		SampleFormat getSampleFormat() const { return m_sampleFormat; }
		Real getOutputSampleRate() const { return m_outputSampleRate; }
		Real getRFBandwidth() const { return m_rfBandwidth; }
		const QString& getUDPAddress() const { return m_udpAddress; }
		int getUDPPort() const { return m_udpPort; }
		int getBoost() const { return m_boost; }

		static MsgUDPSrcConfigure* create(SampleFormat sampleFormat, Real sampleRate, Real rfBandwidth, QString& udpAddress, int udpPort, int boost)
		{
			return new MsgUDPSrcConfigure(sampleFormat, sampleRate, rfBandwidth, udpAddress, udpPort, boost);
		}

	private:
		SampleFormat m_sampleFormat;
		Real m_outputSampleRate;
		Real m_rfBandwidth;
		QString m_udpAddress;
		int m_udpPort;
		int m_boost;

		MsgUDPSrcConfigure(SampleFormat sampleFormat, Real outputSampleRate, Real rfBandwidth, QString& udpAddress, int udpPort, int boost) :
			Message(),
			m_sampleFormat(sampleFormat),
			m_outputSampleRate(outputSampleRate),
			m_rfBandwidth(rfBandwidth),
			m_udpAddress(udpAddress),
			m_udpPort(udpPort),
			m_boost(boost)
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

	int m_inputSampleRate;

	int m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	QHostAddress m_udpAddress;
	quint16 m_udpPort;
	int m_boost;
	Real m_magsq;

	Real m_scale;
	Complex m_last, m_this;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	fftfilt* UDPFilter;

	SampleVector m_sampleBuffer;
	SampleVector m_sampleBufferSSB;
	SampleSink* m_spectrum;
	bool m_spectrumEnabled;

	quint32 m_nextSSBId;
	quint32 m_nextS16leId;

	QMutex m_settingsMutex;
};

#endif // INCLUDE_UDPSRC_H
