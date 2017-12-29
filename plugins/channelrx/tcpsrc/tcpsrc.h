#ifndef INCLUDE_TCPSRC_H
#define INCLUDE_TCPSRC_H

#include <QMutex>
#include <QHostAddress>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "dsp/nco.h"
#include "dsp/fftfilt.h"
#include "dsp/interpolator.h"
#include "util/message.h"

#include "tcpsrcsettings.h"

#define tcpFftLen 2048

class QTcpServer;
class QTcpSocket;
class TCPSrcGUI;
class DeviceSourceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class TCPSrc : public BasebandSampleSink, public ChannelSinkAPI {
	Q_OBJECT

public:
    class MsgConfigureTCPSrc : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const TCPSrcSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureTCPSrc* create(const TCPSrcSettings& settings, bool force)
        {
            return new MsgConfigureTCPSrc(settings, force);
        }

    private:
        TCPSrcSettings m_settings;
        bool m_force;

        MsgConfigureTCPSrc(const TCPSrcSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        {
        }
    };

    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        int getCenterFrequency() const { return m_centerFrequency; }

        static MsgConfigureChannelizer* create(int sampleRate, int centerFrequency)
        {
            return new MsgConfigureChannelizer(sampleRate, centerFrequency);
        }

    private:
        int m_sampleRate;
        int  m_centerFrequency;

        MsgConfigureChannelizer(int sampleRate, int centerFrequency) :
            Message(),
            m_sampleRate(sampleRate),
            m_centerFrequency(centerFrequency)
        { }
    };

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

	TCPSrc(DeviceSourceAPI* m_deviceAPI);
	virtual ~TCPSrc();
	virtual void destroy() { delete this; }
	void setSpectrum(BasebandSampleSink* spectrum) { m_spectrum = spectrum; }

	void setSpectrum(MessageQueue* messageQueue, bool enabled);
	double getMagSq() const { return m_magsq; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    static const QString m_channelIdURI;
    static const QString m_channelId;

protected:
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
	class MsgTCPConnection : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool getConnect() const { return m_connect; }

		static MsgTCPConnection* create(bool connect)
		{
			return new MsgTCPConnection(connect);
		}

	private:
		bool m_connect;

		MsgTCPConnection(bool connect) :
			Message(),
			m_connect(connect)
		{ }
	};

	DeviceSourceAPI* m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    TCPSrcSettings m_settings;
    int m_absoluteFrequencyOffset;

    int m_inputSampleRate;

	int m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	int m_tcpPort;
	int m_volume;
	double m_magsq;

	Real m_scale;
	Complex m_last, m_this;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;
	fftfilt* TCPFilter;

	SampleVector m_sampleBuffer;
	SampleVector m_sampleBufferSSB;
	BasebandSampleSink* m_spectrum;
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
	void processNewConnection();
	void processDeconnection();

protected slots:
	void onNewConnection();
	void onDisconnected();
	void onTcpServerError(QAbstractSocket::SocketError socketError);
};

#endif // INCLUDE_TCPSRC_H
