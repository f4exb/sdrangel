// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// (C) 2015 John Greb                                                            //
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

#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include "tcpsrc.h"
#include "tcpsrcgui.h"
#include "dsp/channelizer.h"

MESSAGE_CLASS_DEFINITION(TCPSrc::MsgTCPSrcConfigure, Message)
MESSAGE_CLASS_DEFINITION(TCPSrc::MsgTCPSrcConnection, Message)
MESSAGE_CLASS_DEFINITION(TCPSrc::MsgTCPSrcSpectrum, Message)

TCPSrc::TCPSrc(MessageQueue* uiMessageQueue, TCPSrcGUI* tcpSrcGUI, SampleSink* spectrum) :
	m_settingsMutex(QMutex::Recursive)
{
	setObjectName("TCPSrc");

	m_inputSampleRate = 96000;
	m_sampleFormat = FormatSSB;
	m_outputSampleRate = 48000;
	m_rfBandwidth = 32000;
	m_tcpPort = 9999;
	m_nco.setFreq(0, m_inputSampleRate);
	m_interpolator.create(16, m_inputSampleRate, m_rfBandwidth / 2.0);
	m_sampleDistanceRemain = m_inputSampleRate / m_outputSampleRate;
	m_uiMessageQueue = uiMessageQueue;
	m_tcpSrcGUI = tcpSrcGUI;
	m_spectrum = spectrum;
	m_spectrumEnabled = false;
	m_nextSSBId = 0;
	m_nextS16leId = 0;

	m_last = 0;
	m_this = 0;
	m_scale = 0;
	m_boost = 0;
	m_magsq = 0;
	m_sampleBufferSSB.resize(tcpFftLen);
	TCPFilter = new fftfilt(0.3 / 48.0, 16.0 / 48.0, tcpFftLen);
	// if (!TCPFilter) segfault;
}

TCPSrc::~TCPSrc()
{
	if (TCPFilter) delete TCPFilter;
}

void TCPSrc::configure(MessageQueue* messageQueue, SampleFormat sampleFormat, Real outputSampleRate, Real rfBandwidth, int tcpPort, int boost)
{
	Message* cmd = MsgTCPSrcConfigure::create(sampleFormat, outputSampleRate, rfBandwidth, tcpPort, boost);
	messageQueue->push(cmd);
}

void TCPSrc::setSpectrum(MessageQueue* messageQueue, bool enabled)
{
	Message* cmd = MsgTCPSrcSpectrum::create(enabled);
	messageQueue->push(cmd);
}

void TCPSrc::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
	Complex ci;
	fftfilt::cmplx* sideband;
	Real l, r;

	m_sampleBuffer.clear();

	m_settingsMutex.lock();

	// Rtl-Sdr uses full 16-bit scale; FCDPP does not
	//int rescale = 32768 * (1 << m_boost);
	int rescale = (1 << m_boost);

	for(SampleVector::const_iterator it = begin; it < end; ++it) {
		//Complex c(it->real() / 32768.0f, it->imag() / 32768.0f);
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if(m_interpolator.interpolate(&m_sampleDistanceRemain, c, &ci))
		{
			m_magsq = ((ci.real()*ci.real() +  ci.imag()*ci.imag())*rescale*rescale) / (1<<30);
			m_sampleBuffer.push_back(Sample(ci.real() * rescale, ci.imag() * rescale));
			m_sampleDistanceRemain += m_inputSampleRate / m_outputSampleRate;
		}
	}

	if((m_spectrum != 0) && (m_spectrumEnabled))
	{
		m_spectrum->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), positiveOnly);
	}

	for(int i = 0; i < m_s16leSockets.count(); i++)
	{
		m_s16leSockets[i].socket->write((const char*)&m_sampleBuffer[0], m_sampleBuffer.size() * 4);
	}

	if((m_sampleFormat == FormatSSB) && (m_ssbSockets.count() > 0)) {
		for(SampleVector::const_iterator it = m_sampleBuffer.begin(); it != m_sampleBuffer.end(); ++it) {
			//Complex cj(it->real() / 30000.0, it->imag() / 30000.0);
			Complex cj(it->real(), it->imag());
			int n_out = TCPFilter->runSSB(cj, &sideband, true);
			if (n_out) {
				for (int i = 0; i < n_out; i+=2) {
					//l = (sideband[i].real() + sideband[i].imag()) * 0.7 * 32000.0;
					//r = (sideband[i+1].real() + sideband[i+1].imag()) * 0.7 * 32000.0;
					l = (sideband[i].real() + sideband[i].imag()) * 0.7;
					r = (sideband[i+1].real() + sideband[i+1].imag()) * 0.7;
					m_sampleBufferSSB.push_back(Sample(l, r));
				}
				for(int i = 0; i < m_ssbSockets.count(); i++)
					m_ssbSockets[i].socket->write((const char*)&m_sampleBufferSSB[0], n_out * 2);
				m_sampleBufferSSB.clear();
			}
		}
	}

	if((m_sampleFormat == FormatNFM) && (m_ssbSockets.count() > 0)) {
		for(SampleVector::const_iterator it = m_sampleBuffer.begin(); it != m_sampleBuffer.end(); ++it) {
			Complex cj(it->real() / 32768.0f, it->imag() / 32768.0f);
			// An FFT filter here is overkill, but was already set up for SSB
			int n_out = TCPFilter->runFilt(cj, &sideband);
			if (n_out) {
				Real sum = 1.0;
				for (int i = 0; i < n_out; i+=2) {
					l = m_this.real() * (m_last.imag() - sideband[i].imag())
					  - m_this.imag() * (m_last.real() - sideband[i].real());
					m_last = sideband[i];
					r = m_last.real() * (m_this.imag() - sideband[i+1].imag())
					  - m_last.imag() * (m_this.real() - sideband[i+1].real());
					m_this = sideband[i+1];
					m_sampleBufferSSB.push_back(Sample(l * m_scale, r * m_scale));
					sum += m_this.real() * m_this.real() + m_this.imag() * m_this.imag(); 
				}
				// TODO: correct levels
				m_scale = 24000 * tcpFftLen / sum;
				for(int i = 0; i < m_ssbSockets.count(); i++)
					m_ssbSockets[i].socket->write((const char*)&m_sampleBufferSSB[0], n_out * 2);
				m_sampleBufferSSB.clear();
			}
		}
	}

	m_settingsMutex.unlock();
}

void TCPSrc::start()
{
	m_tcpServer = new QTcpServer();
	connect(m_tcpServer, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
	connect(m_tcpServer, SIGNAL(acceptError(QAbstractSocket::SocketError)), this, SLOT(onTcpServerError(QAbstractSocket::SocketError)));
	m_tcpServer->listen(QHostAddress::Any, m_tcpPort);
}

void TCPSrc::stop()
{
	closeAllSockets(&m_ssbSockets);
	closeAllSockets(&m_s16leSockets);

	if(m_tcpServer->isListening())
		m_tcpServer->close();
	delete m_tcpServer;
}

bool TCPSrc::handleMessage(const Message& cmd)
{
	qDebug() << "TCPSrc::handleMessage";

	if (Channelizer::MsgChannelizerNotification::match(cmd))
	{
		Channelizer::MsgChannelizerNotification& notif = (Channelizer::MsgChannelizerNotification&) cmd;

		m_settingsMutex.lock();

		m_inputSampleRate = notif.getSampleRate();
		m_nco.setFreq(-notif.getFrequencyOffset(), m_inputSampleRate);
		m_interpolator.create(16, m_inputSampleRate, m_rfBandwidth / 2.0);
		m_sampleDistanceRemain = m_inputSampleRate / m_outputSampleRate;

		m_settingsMutex.unlock();

		qDebug() << "TCPSrc::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << m_inputSampleRate
				<< " frequencyOffset: " << notif.getFrequencyOffset();

		return true;
	}
	else if (MsgTCPSrcConfigure::match(cmd))
	{
		MsgTCPSrcConfigure& cfg = (MsgTCPSrcConfigure&) cmd;

		m_settingsMutex.lock();

		m_sampleFormat = cfg.getSampleFormat();
		m_outputSampleRate = cfg.getOutputSampleRate();
		m_rfBandwidth = cfg.getRFBandwidth();

		if (cfg.getTCPPort() != m_tcpPort)
		{
			m_tcpPort = cfg.getTCPPort();

			if(m_tcpServer->isListening())
			{
				m_tcpServer->close();
			}

			m_tcpServer->listen(QHostAddress::Any, m_tcpPort);
		}

		m_boost = cfg.getBoost();
		m_interpolator.create(16, m_inputSampleRate, m_rfBandwidth / 2.0);
		m_sampleDistanceRemain = m_inputSampleRate / m_outputSampleRate;

		if (m_sampleFormat == FormatSSB)
		{
			TCPFilter->create_filter(0.3 / 48.0, m_rfBandwidth / 2.0 / m_outputSampleRate);
		}
		else
		{
			TCPFilter->create_filter(0.0, m_rfBandwidth / 2.0 / m_outputSampleRate);
		}

		m_settingsMutex.unlock();

		qDebug() << "  - MsgTCPSrcConfigure: m_sampleFormat: " << m_sampleFormat
				<< " m_outputSampleRate: " << m_outputSampleRate
				<< " m_rfBandwidth: " << m_rfBandwidth
				<< " m_boost: " << m_boost;

		return true;
	}
	else if (MsgTCPSrcSpectrum::match(cmd))
	{
		MsgTCPSrcSpectrum& spc = (MsgTCPSrcSpectrum&) cmd;

		m_spectrumEnabled = spc.getEnabled();

		qDebug() << "  - MsgTCPSrcSpectrum: m_spectrumEnabled: " << m_spectrumEnabled;

		return true;
	}
	else if (MsgTCPConnection::match(cmd))
	{
		MsgTCPConnection& con = (MsgTCPConnection&) cmd;

		if (con.getConnect())
		{
			processNewConnection();
		}
		else
		{
			processDeconnection();
		}
	}
	else
	{
		if(m_spectrum != 0)
		{
		   return m_spectrum->handleMessage(cmd);
		}
		else
		{
			return false;
		}
	}
}

void TCPSrc::closeAllSockets(Sockets* sockets)
{
	for(int i = 0; i < sockets->count(); ++i)
	{
		MsgTCPSrcConnection* msg = MsgTCPSrcConnection::create(false, sockets->at(i).id, QHostAddress(), 0);
		m_uiMessageQueue->push(msg);
		sockets->at(i).socket->close();
	}
}


void TCPSrc::onNewConnection()
{
	qDebug("TCPSrc::onNewConnection");
	MsgTCPConnection *cmd = MsgTCPConnection::create(true);
	getInputMessageQueue()->push(cmd);
}

void TCPSrc::processNewConnection()
{
	qDebug("TCPSrc::processNewConnection");

	while(m_tcpServer->hasPendingConnections())
	{
		qDebug("TCPSrc::processNewConnection: has a pending connection");
		QTcpSocket* connection = m_tcpServer->nextPendingConnection();
		connection->setSocketOption(QAbstractSocket:: KeepAliveOption, 1);
		connect(connection, SIGNAL(disconnected()), this, SLOT(onDisconnected()));

		switch(m_sampleFormat) {

			case FormatNFM:
			case FormatSSB:
			{
				quint32 id = (FormatSSB << 24) | m_nextSSBId;
				MsgTCPSrcConnection* msg = MsgTCPSrcConnection::create(true, id, connection->peerAddress(), connection->peerPort());
				m_nextSSBId = (m_nextSSBId + 1) & 0xffffff;
				m_ssbSockets.push_back(Socket(id, connection));
				m_uiMessageQueue->push(msg);
				break;
			}

			case FormatS16LE:
			{
				qDebug("TCPSrc::processNewConnection: establish new S16LE connection");
				quint32 id = (FormatS16LE << 24) | m_nextS16leId;
				MsgTCPSrcConnection* msg = MsgTCPSrcConnection::create(true, id, connection->peerAddress(), connection->peerPort());
				m_nextS16leId = (m_nextS16leId + 1) & 0xffffff;
				m_s16leSockets.push_back(Socket(id, connection));
				m_uiMessageQueue->push(msg);
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
	qDebug("TCPSrc::onDisconnected");
	MsgTCPConnection *cmd = MsgTCPConnection::create(false);
	getInputMessageQueue()->push(cmd);
}

void TCPSrc::processDeconnection()
{
	quint32 id;
	QTcpSocket* socket = 0;

	qDebug("TCPSrc::processDeconnection");

	for(int i = 0; i < m_ssbSockets.count(); i++)
	{
		if(m_ssbSockets[i].socket == sender())
		{
			id = m_ssbSockets[i].id;
			socket = m_ssbSockets[i].socket;
			socket->close();
			m_ssbSockets.removeAt(i);
			break;
		}
	}

	if(socket == 0)
	{
		for(int i = 0; i < m_s16leSockets.count(); i++)
		{
			if(m_s16leSockets[i].socket == sender())
			{
				qDebug("TCPSrc::processDeconnection: remove S16LE socket #%d", i);

				id = m_s16leSockets[i].id;
				socket = m_s16leSockets[i].socket;
				socket->close();
				m_s16leSockets.removeAt(i);
				break;
			}
		}
	}

	if(socket != 0)
	{
		MsgTCPSrcConnection* msg = MsgTCPSrcConnection::create(false, id, QHostAddress(), 0);
		m_uiMessageQueue->push(msg);
		socket->deleteLater();
	}
}

void TCPSrc::onTcpServerError(QAbstractSocket::SocketError socketError)
{
	qDebug("TCPSrc::onTcpServerError: %s", qPrintable(m_tcpServer->errorString()));
}
