#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include "tcpsrc.h"
#include "tcpsrcgui.h"
#include "dsp/dspcommands.h"

MESSAGE_CLASS_DEFINITION(TCPSrc::MsgTCPSrcConfigure, Message)
MESSAGE_CLASS_DEFINITION(TCPSrc::MsgTCPSrcConnection, Message)
MESSAGE_CLASS_DEFINITION(TCPSrc::MsgTCPSrcSpectrum, Message)

TCPSrc::TCPSrc(MessageQueue* uiMessageQueue, TCPSrcGUI* tcpSrcGUI, SampleSink* spectrum)
{
	m_inputSampleRate = 100000;
	m_sampleFormat = FormatS8;
	m_outputSampleRate = 50000;
	m_rfBandwidth = 50000;
	m_tcpPort = 9999;
	m_nco.setFreq(0, m_inputSampleRate);
	m_interpolator.create(16, m_inputSampleRate, m_rfBandwidth / 2.1);
	m_sampleDistanceRemain = m_inputSampleRate / m_outputSampleRate;
	m_uiMessageQueue = uiMessageQueue;
	m_tcpSrcGUI = tcpSrcGUI;
	m_spectrum = spectrum;
	m_spectrumEnabled = false;
	m_nextS8Id = 0;
	m_nextS16leId = 0;
}

TCPSrc::~TCPSrc()
{
}

void TCPSrc::configure(MessageQueue* messageQueue, SampleFormat sampleFormat, Real outputSampleRate, Real rfBandwidth, int tcpPort)
{
	Message* cmd = MsgTCPSrcConfigure::create(sampleFormat, outputSampleRate, rfBandwidth, tcpPort);
	cmd->submit(messageQueue, this);
}

void TCPSrc::setSpectrum(MessageQueue* messageQueue, bool enabled)
{
	Message* cmd = MsgTCPSrcSpectrum::create(enabled);
	cmd->submit(messageQueue, this);
}

void TCPSrc::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst)
{
	Complex ci;
	bool consumed;

	for(SampleVector::const_iterator it = begin; it < end; ++it) {
		Complex c(it->real() / 32768.0, it->imag() / 32768.0);
		c *= m_nco.nextIQ();

		consumed = false;
		if(m_interpolator.interpolate(&m_sampleDistanceRemain, c, &consumed, &ci)) {
			m_sampleBuffer.push_back(Sample(ci.real() * 32768.0, ci.imag() * 32768.0));
			m_sampleDistanceRemain += m_inputSampleRate / m_outputSampleRate;
		}
	}

	if((m_spectrum != NULL) && (m_spectrumEnabled))
		m_spectrum->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), firstOfBurst);

	for(int i = 0; i < m_s16leSockets.count(); i++)
		m_s16leSockets[i].socket->write((const char*)&m_sampleBuffer[0], m_sampleBuffer.size() * 4);

	if(m_s8Sockets.count() > 0) {
		for(SampleVector::const_iterator it = m_sampleBuffer.begin(); it != m_sampleBuffer.end(); ++it) {
			m_sampleBufferS8.push_back(it->real() >> 8);
			m_sampleBufferS8.push_back(it->imag() >> 8);
		}
		for(int i = 0; i < m_s8Sockets.count(); i++)
			m_s8Sockets[i].socket->write((const char*)&m_sampleBufferS8[0], m_sampleBufferS8.size());
	}

	m_sampleBuffer.clear();
	m_sampleBufferS8.clear();
}

void TCPSrc::start()
{
	m_tcpServer = new QTcpServer();
	connect(m_tcpServer, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
	m_tcpServer->listen(QHostAddress::Any, m_tcpPort);
}

void TCPSrc::stop()
{
	closeAllSockets(&m_s8Sockets);
	closeAllSockets(&m_s16leSockets);

	if(m_tcpServer->isListening())
		m_tcpServer->close();
	delete m_tcpServer;
}

bool TCPSrc::handleMessage(Message* cmd)
{
	if(DSPSignalNotification::match(cmd)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)cmd;
		qDebug("%d samples/sec, %lld Hz offset", signal->getSampleRate(), signal->getFrequencyOffset());
		m_inputSampleRate = signal->getSampleRate();
		m_nco.setFreq(-signal->getFrequencyOffset(), m_inputSampleRate);
		m_interpolator.create(16, m_inputSampleRate, m_rfBandwidth / 2.1);
		m_sampleDistanceRemain = m_inputSampleRate / m_outputSampleRate;
		cmd->completed();
		return true;
	} else if(DSPSignalNotification::match(cmd)) {
		MsgTCPSrcConfigure* cfg = (MsgTCPSrcConfigure*)cmd;
		m_sampleFormat = cfg->getSampleFormat();
		m_outputSampleRate = cfg->getOutputSampleRate();
		m_rfBandwidth = cfg->getRFBandwidth();
		if(cfg->getTCPPort() != m_tcpPort) {
			m_tcpPort = cfg->getTCPPort();
			if(m_tcpServer->isListening())
				m_tcpServer->close();
			m_tcpServer->listen(QHostAddress::Any, m_tcpPort);
		}
		m_interpolator.create(16, m_inputSampleRate, m_rfBandwidth / 2.1);
		m_sampleDistanceRemain = m_inputSampleRate / m_outputSampleRate;
		cmd->completed();
		return true;
	} else if(MsgTCPSrcSpectrum::match(cmd)) {
		MsgTCPSrcSpectrum* spc = (MsgTCPSrcSpectrum*)cmd;
		m_spectrumEnabled = spc->getEnabled();
		cmd->completed();
		return true;
	} else {
		if(m_spectrum != NULL)
		   return m_spectrum->handleMessage(cmd);
		else return false;
	}
}

void TCPSrc::closeAllSockets(Sockets* sockets)
{
	for(int i = 0; i < sockets->count(); ++i) {
		MsgTCPSrcConnection* msg = MsgTCPSrcConnection::create(false, sockets->at(i).id, QHostAddress(), 0);
		msg->submit(m_uiMessageQueue, (PluginGUI*)m_tcpSrcGUI);
		sockets->at(i).socket->close();
	}
}

void TCPSrc::onNewConnection()
{
	while(m_tcpServer->hasPendingConnections()) {
		QTcpSocket* connection = m_tcpServer->nextPendingConnection();
		connect(connection, SIGNAL(disconnected()), this, SLOT(onDisconnected()));

		switch(m_sampleFormat) {
			case FormatS8: {
				quint32 id = (FormatS8 << 24) | m_nextS8Id;
				MsgTCPSrcConnection* msg = MsgTCPSrcConnection::create(true, id, connection->peerAddress(), connection->peerPort());
				m_nextS8Id = (m_nextS8Id + 1) & 0xffffff;
				m_s8Sockets.push_back(Socket(id, connection));
				msg->submit(m_uiMessageQueue, (PluginGUI*)m_tcpSrcGUI);
				break;
			}

			case FormatS16LE: {
				quint32 id = (FormatS16LE << 24) | m_nextS16leId;
				MsgTCPSrcConnection* msg = MsgTCPSrcConnection::create(true, id, connection->peerAddress(), connection->peerPort());
				m_nextS16leId = (m_nextS16leId + 1) & 0xffffff;
				m_s16leSockets.push_back(Socket(id, connection));
				msg->submit(m_uiMessageQueue, (PluginGUI*)m_tcpSrcGUI);
				break;
			}

			default:
				delete connection;
				break;
		}
	}
}

void TCPSrc::onDisconnected()
{
	quint32 id;
	QTcpSocket* socket = NULL;

	for(int i = 0; i < m_s8Sockets.count(); i++) {
		if(m_s8Sockets[i].socket == sender()) {
			id = m_s8Sockets[i].id;
			socket = m_s8Sockets[i].socket;
			m_s8Sockets.removeAt(i);
			break;
		}
	}
	if(socket == NULL) {
		for(int i = 0; i < m_s16leSockets.count(); i++) {
			if(m_s16leSockets[i].socket == sender()) {
				id = m_s16leSockets[i].id;
				socket = m_s16leSockets[i].socket;
				m_s16leSockets.removeAt(i);
				break;
			}
		}
	}
	if(socket != NULL) {
		MsgTCPSrcConnection* msg = MsgTCPSrcConnection::create(false, id, QHostAddress(), 0);
		msg->submit(m_uiMessageQueue, (PluginGUI*)m_tcpSrcGUI);
		socket->deleteLater();
	}
}
