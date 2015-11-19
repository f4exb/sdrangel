#ifndef INCLUDE_TCPSRC_H
#define INCLUDE_TCPSRC_H

#include <QMutex>
#include <QHostAddress>
#include "dsp/samplesink.h"
#include "dsp/nco.h"
#include "dsp/fftfilt.h"
#include "dsp/interpolator.h"
#include "util/message.h"

#define tcpFftLen 2048

class QTcpServer;
class QTcpSocket;
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

	void configure(MessageQueue* messageQueue, SampleFormat sampleFormat, Real outputSampleRate, Real rfBandwidth, int tcpPort, int boost);
	void setSpectrum(MessageQueue* messageQueue, bool enabled);
	Real getMagSq() const { return m_magsq; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

	class MsgUDPSrcConnection : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getConnect() const { return m_connect; }
		quint32 getID() const { return m_id; }
		const QHostAddress& getPeerAddress() const { return m_peerAddress; }
		int getPeerPort() const { return m_peerPort; }

		static MsgUDPSrcConnection* create(bool connect, quint32 id, const QHostAddress& peerAddress, int peerPort)
		{
			return new MsgUDPSrcConnection(connect, id, peerAddress, peerPort);
		}

	private:
		bool m_connect;
		quint32 m_id;
		QHostAddress m_peerAddress;
		int m_peerPort;

		MsgUDPSrcConnection(bool connect, quint32 id, const QHostAddress& peerAddress, int peerPort) :
			Message(),
			m_connect(connect),
			m_id(id),
			m_peerAddress(peerAddress),
			m_peerPort(peerPort)
		{ }
	};

protected:
	class MsgUDPSrcConfigure : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		SampleFormat getSampleFormat() const { return m_sampleFormat; }
		Real getOutputSampleRate() const { return m_outputSampleRate; }
		Real getRFBandwidth() const { return m_rfBandwidth; }
		int getTCPPort() const { return m_tcpPort; }
		int getBoost() const { return m_boost; }

		static MsgUDPSrcConfigure* create(SampleFormat sampleFormat, Real sampleRate, Real rfBandwidth, int tcpPort, int boost)
		{
			return new MsgUDPSrcConfigure(sampleFormat, sampleRate, rfBandwidth, tcpPort, boost);
		}

	private:
		SampleFormat m_sampleFormat;
		Real m_outputSampleRate;
		Real m_rfBandwidth;
		int m_tcpPort;
		int m_boost;

		MsgUDPSrcConfigure(SampleFormat sampleFormat, Real outputSampleRate, Real rfBandwidth, int tcpPort, int boost) :
			Message(),
			m_sampleFormat(sampleFormat),
			m_outputSampleRate(outputSampleRate),
			m_rfBandwidth(rfBandwidth),
			m_tcpPort(tcpPort),
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
	UDPSrcGUI* m_tcpSrcGUI;

	int m_inputSampleRate;

	int m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	int m_tcpPort;
	int m_boost;
	Real m_magsq;

	Real m_scale;
	Complex m_last, m_this;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	fftfilt* TCPFilter;

	SampleVector m_sampleBuffer;
	SampleVector m_sampleBufferSSB;
	SampleSink* m_spectrum;
	bool m_spectrumEnabled;

	QTcpServer* m_tcpServer;
	struct Socket {
		quint32 id;
		QTcpSocket* socket;
		Socket(quint32 _id, QTcpSocket* _socket) :
			id(_id),
			socket(_socket)
		{ }
	};
	typedef QList<Socket> Sockets;
	Sockets m_ssbSockets;
	Sockets m_s16leSockets;
	quint32 m_nextSSBId;
	quint32 m_nextS16leId;

	QMutex m_settingsMutex;

	void closeAllSockets(Sockets* sockets);

protected slots:
	void onNewConnection();
	void onDisconnected();
	void onTcpServerError(QAbstractSocket::SocketError socketError);
};

#endif // INCLUDE_TCPSRC_H
