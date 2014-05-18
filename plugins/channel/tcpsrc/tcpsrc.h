#ifndef INCLUDE_TCPSRC_H
#define INCLUDE_TCPSRC_H

#include <QHostAddress>
#include "dsp/samplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/message.h"

class QTcpServer;
class QTcpSocket;
class TCPSrcGUI;

class TCPSrc : public SampleSink {
	Q_OBJECT

public:
	enum SampleFormat {
		FormatS8,
		FormatS16LE
	};

	TCPSrc(MessageQueue* uiMessageQueue, TCPSrcGUI* tcpSrcGUI, SampleSink* spectrum);
	~TCPSrc();

	void configure(MessageQueue* messageQueue, SampleFormat sampleFormat, Real outputSampleRate, Real rfBandwidth, int tcpPort);
	void setSpectrum(MessageQueue* messageQueue, bool enabled);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst);
	void start();
	void stop();
	bool handleMessage(Message* cmd);

	class MsgTCPSrcConnection : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getConnect() const { return m_connect; }
		quint32 getID() const { return m_id; }
		const QHostAddress& getPeerAddress() const { return m_peerAddress; }
		int getPeerPort() const { return m_peerPort; }

		static MsgTCPSrcConnection* create(bool connect, quint32 id, const QHostAddress& peerAddress, int peerPort)
		{
			return new MsgTCPSrcConnection(connect, id, peerAddress, peerPort);
		}

	private:
		bool m_connect;
		quint32 m_id;
		QHostAddress m_peerAddress;
		int m_peerPort;

		MsgTCPSrcConnection(bool connect, quint32 id, const QHostAddress& peerAddress, int peerPort) :
			Message(),
			m_connect(connect),
			m_id(id),
			m_peerAddress(peerAddress),
			m_peerPort(peerPort)
		{ }
	};

protected:
	class MsgTCPSrcConfigure : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		SampleFormat getSampleFormat() const { return m_sampleFormat; }
		Real getOutputSampleRate() const { return m_outputSampleRate; }
		Real getRFBandwidth() const { return m_rfBandwidth; }
		int getTCPPort() const { return m_tcpPort; }

		static MsgTCPSrcConfigure* create(SampleFormat sampleFormat, Real sampleRate, Real rfBandwidth, int tcpPort)
		{
			return new MsgTCPSrcConfigure(sampleFormat, sampleRate, rfBandwidth, tcpPort);
		}

	private:
		SampleFormat m_sampleFormat;
		Real m_outputSampleRate;
		Real m_rfBandwidth;
		int m_tcpPort;

		MsgTCPSrcConfigure(SampleFormat sampleFormat, Real outputSampleRate, Real rfBandwidth, int tcpPort) :
			Message(),
			m_sampleFormat(sampleFormat),
			m_outputSampleRate(outputSampleRate),
			m_rfBandwidth(rfBandwidth),
			m_tcpPort(tcpPort)
		{ }
	};
	class MsgTCPSrcSpectrum : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getEnabled() const { return m_enabled; }

		static MsgTCPSrcSpectrum* create(bool enabled)
		{
			return new MsgTCPSrcSpectrum(enabled);
		}

	private:
		bool m_enabled;

		MsgTCPSrcSpectrum(bool enabled) :
			Message(),
			m_enabled(enabled)
		{ }
	};

	MessageQueue* m_uiMessageQueue;
	TCPSrcGUI* m_tcpSrcGUI;

	int m_inputSampleRate;

	int m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	int m_tcpPort;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;

	SampleVector m_sampleBuffer;
	std::vector<qint8> m_sampleBufferS8;
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
	Sockets m_s8Sockets;
	Sockets m_s16leSockets;
	quint32 m_nextS8Id;
	quint32 m_nextS16leId;

	void closeAllSockets(Sockets* sockets);

protected slots:
	void onNewConnection();
	void onDisconnected();
};

#endif // INCLUDE_TCPSRC_H
