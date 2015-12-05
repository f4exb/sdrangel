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

#include "udpsrc.h"

#include <QUdpSocket>
#include <QThread>
#include <QHostAddress>
#include "dsp/channelizer.h"
#include "dsp/dspengine.h"
#include "udpsrcgui.h"

MESSAGE_CLASS_DEFINITION(UDPSrc::MsgUDPSrcConfigure, Message)
MESSAGE_CLASS_DEFINITION(UDPSrc::MsgUDPSrcConfigureImmediate, Message)
MESSAGE_CLASS_DEFINITION(UDPSrc::MsgUDPSrcSpectrum, Message)

UDPSrc::UDPSrc(MessageQueue* uiMessageQueue, UDPSrcGUI* udpSrcGUI, SampleSink* spectrum) :
	m_settingsMutex(QMutex::Recursive),
	m_audioFifo(4, 24000),
	m_audioActive(false),
	m_audioStereo(false),
	m_volume(20)
{
	setObjectName("UDPSrc");

	m_socket = new QUdpSocket(this);
	m_audioSocket = new QUdpSocket(this);

	m_audioBuffer.resize(1<<9);
	m_audioBufferFill = 0;

	m_inputSampleRate = 96000;
	m_sampleFormat = FormatSSB;
	m_outputSampleRate = 48000;
	m_rfBandwidth = 32000;
	m_udpPort = 9999;
	m_audioPort = m_udpPort - 1;
	m_nco.setFreq(0, m_inputSampleRate);
	m_interpolator.create(16, m_inputSampleRate, m_rfBandwidth / 2.0);
	m_sampleDistanceRemain = m_inputSampleRate / m_outputSampleRate;
	m_uiMessageQueue = uiMessageQueue;
	m_udpSrcGUI = udpSrcGUI;
	m_spectrum = spectrum;
	m_spectrumEnabled = false;
	m_nextSSBId = 0;
	m_nextS16leId = 0;

	m_last = 0;
	m_this = 0;
	m_scale = 0;
	m_boost = 0;
	m_magsq = 0;
	m_sampleBufferSSB.resize(udpFftLen);
	UDPFilter = new fftfilt(0.3 / 48.0, 16.0 / 48.0, udpFftLen);

	if (m_audioSocket->bind(QHostAddress::LocalHost, m_audioPort))
	{
		qDebug("UDPSrc::UDPSrc: bind audio socket to port %d", m_audioPort);
		connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()));
	}
	else
	{
		qWarning("UDPSrc::UDPSrc: cannot bind audio port");
	}

	//DSPEngine::instance()->addAudioSink(&m_audioFifo);
}

UDPSrc::~UDPSrc()
{
	delete m_audioSocket;
	delete m_socket;
	if (UDPFilter) delete UDPFilter;
	if (m_audioActive) DSPEngine::instance()->removeAudioSink(&m_audioFifo);
}

void UDPSrc::configure(MessageQueue* messageQueue,
		SampleFormat sampleFormat,
		Real outputSampleRate,
		Real rfBandwidth,
		QString& udpAddress,
		int udpPort,
		int audioPort)
{
	Message* cmd = MsgUDPSrcConfigure::create(sampleFormat,
			outputSampleRate,
			rfBandwidth,
			udpAddress,
			udpPort,
			audioPort);
	messageQueue->push(cmd);
}

void UDPSrc::configureImmediate(MessageQueue* messageQueue,
		bool audioActive,
		bool audioStereo,
		int boost,
		int volume)
{
	Message* cmd = MsgUDPSrcConfigureImmediate::create(
			audioActive,
			audioStereo,
			boost,
			volume);
	messageQueue->push(cmd);
}

void UDPSrc::setSpectrum(MessageQueue* messageQueue, bool enabled)
{
	Message* cmd = MsgUDPSrcSpectrum::create(enabled);
	messageQueue->push(cmd);
}

void UDPSrc::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
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

	if (m_sampleFormat == FormatSSB)
	{
		for(SampleVector::const_iterator it = m_sampleBuffer.begin(); it != m_sampleBuffer.end(); ++it)
		{
			//Complex cj(it->real() / 30000.0, it->imag() / 30000.0);
			Complex cj(it->real(), it->imag());
			int n_out = UDPFilter->runSSB(cj, &sideband, true);

			if (n_out)
			{
				for (int i = 0; i < n_out; i+=2)
				{
					//l = (sideband[i].real() + sideband[i].imag()) * 0.7 * 32000.0;
					//r = (sideband[i+1].real() + sideband[i+1].imag()) * 0.7 * 32000.0;
					l = (sideband[i].real() + sideband[i].imag()) * 0.7;
					r = (sideband[i+1].real() + sideband[i+1].imag()) * 0.7;
					m_sampleBufferSSB.push_back(Sample(l, r));
				}

				m_socket->writeDatagram((const char*)&m_sampleBufferSSB[0], (qint64 ) (n_out * 2), m_udpAddress, m_udpPort);
				m_sampleBufferSSB.clear();
			}
		}
	}
	else if (m_sampleFormat == FormatNFM)
	{
		for(SampleVector::const_iterator it = m_sampleBuffer.begin(); it != m_sampleBuffer.end(); ++it)
		{
			Complex cj(it->real() / 32768.0f, it->imag() / 32768.0f);
			// An FFT filter here is overkill, but was already set up for SSB
			int n_out = UDPFilter->runFilt(cj, &sideband);

			if (n_out)
			{
				Real sum = 1.0;
				for (int i = 0; i < n_out; i+=2)
				{
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
				m_scale = 24000 * udpFftLen / sum;
				m_socket->writeDatagram((const char*)&m_sampleBufferSSB[0], (qint64 ) (n_out * 2), m_udpAddress, m_udpPort);
				m_sampleBufferSSB.clear();
			}
		}
	}
	else
	{
		m_socket->writeDatagram((const char*)&m_sampleBuffer[0], (qint64 ) (m_sampleBuffer.size() * 4), m_udpAddress, m_udpPort);
	}

	m_settingsMutex.unlock();
}

void UDPSrc::start()
{
}

void UDPSrc::stop()
{
}

bool UDPSrc::handleMessage(const Message& cmd)
{
	qDebug() << "UDPSrc::handleMessage";

	if (Channelizer::MsgChannelizerNotification::match(cmd))
	{
		Channelizer::MsgChannelizerNotification& notif = (Channelizer::MsgChannelizerNotification&) cmd;

		m_settingsMutex.lock();

		m_inputSampleRate = notif.getSampleRate();
		m_nco.setFreq(-notif.getFrequencyOffset(), m_inputSampleRate);
		m_interpolator.create(16, m_inputSampleRate, m_rfBandwidth / 2.0);
		m_sampleDistanceRemain = m_inputSampleRate / m_outputSampleRate;

		m_settingsMutex.unlock();

		qDebug() << "UDPSrc::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << m_inputSampleRate
				<< " frequencyOffset: " << notif.getFrequencyOffset();

		return true;
	}
	else if (MsgUDPSrcConfigureImmediate::match(cmd))
	{
		MsgUDPSrcConfigureImmediate& cfg = (MsgUDPSrcConfigureImmediate&) cmd;

		m_settingsMutex.lock();

		if (cfg.getAudioActive() != m_audioActive)
		{
			m_audioActive = cfg.getAudioActive();

			if (m_audioActive)
			{
				m_audioBufferFill = 0;
				DSPEngine::instance()->addAudioSink(&m_audioFifo);
			}
			else
			{
				DSPEngine::instance()->removeAudioSink(&m_audioFifo);
			}
		}

		if (cfg.getAudioStereo() != m_audioStereo)
		{
			m_audioStereo = cfg.getAudioStereo();
		}

		if (cfg.getBoost() != m_boost)
		{
			m_boost = cfg.getBoost();
		}

		if (cfg.getVolume() != m_volume)
		{
			m_volume = cfg.getVolume();
		}

		m_settingsMutex.unlock();

		qDebug() << "UDPSrc::handleMessage: MsgUDPSrcConfigureImmediate: "
				<< " m_audioActive: " << m_audioActive
				<< " m_audioStereo: " << m_audioStereo
				<< " m_boost: " << m_boost
				<< " m_volume: " << m_volume;

		return true;

	}
	else if (MsgUDPSrcConfigure::match(cmd))
	{
		MsgUDPSrcConfigure& cfg = (MsgUDPSrcConfigure&) cmd;

		m_settingsMutex.lock();

		m_sampleFormat = cfg.getSampleFormat();
		m_outputSampleRate = cfg.getOutputSampleRate();
		m_rfBandwidth = cfg.getRFBandwidth();

		if (cfg.getUDPAddress() != m_udpAddress.toString())
		{
			m_udpAddress.setAddress(cfg.getUDPAddress());
		}

		if (cfg.getUDPPort() != m_udpPort)
		{
			m_udpPort = cfg.getUDPPort();
		}

		if (cfg.getAudioPort() != m_audioPort)
		{
			m_audioPort = cfg.getAudioPort();

			disconnect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()));
			delete m_audioSocket;
			m_audioSocket = new QUdpSocket(this);

			if (m_audioSocket->bind(QHostAddress::Any, m_audioPort))
			{
				connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()));
			}
			else
			{
				qWarning("UDPSrc::handleMessage: cannot bind audio socket");
			}
		}

		m_interpolator.create(16, m_inputSampleRate, m_rfBandwidth / 2.0);
		m_sampleDistanceRemain = m_inputSampleRate / m_outputSampleRate;

		if (m_sampleFormat == FormatSSB)
		{
			UDPFilter->create_filter(0.3 / 48.0, m_rfBandwidth / 2.0 / m_outputSampleRate);
		}
		else
		{
			UDPFilter->create_filter(0.0, m_rfBandwidth / 2.0 / m_outputSampleRate);
		}

		m_settingsMutex.unlock();

		qDebug() << "UDPSrc::handleMessage: MsgUDPSrcConfigure: m_sampleFormat: " << m_sampleFormat
				<< " m_outputSampleRate: " << m_outputSampleRate
				<< " m_rfBandwidth: " << m_rfBandwidth
				<< " m_boost: " << m_boost
				<< " m_udpAddress: " << cfg.getUDPAddress()
				<< " m_udpPort: " << m_udpPort
				<< " m_audioPort: " << m_audioPort;

		return true;
	}
	else if (MsgUDPSrcSpectrum::match(cmd))
	{
		MsgUDPSrcSpectrum& spc = (MsgUDPSrcSpectrum&) cmd;

		m_spectrumEnabled = spc.getEnabled();

		qDebug() << "UDPSrc::handleMessage: MsgUDPSrcSpectrum: m_spectrumEnabled: " << m_spectrumEnabled;

		return true;
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

void UDPSrc::audioReadyRead()
{
	QByteArray buffer;

	while (m_audioSocket->hasPendingDatagrams())
	{
		buffer.resize(m_audioSocket->pendingDatagramSize());
		m_audioSocket->readDatagram(buffer.data(), buffer.size(), 0, 0);
		//qDebug("UDPSrc::audioReadyRead: %d", buffer.size());

		if (m_audioActive)
		{
			if (m_audioStereo)
			{
				for (int i = 0; i < buffer.size() - 3; i += 4)
				{
					qint16 l_sample = (qint16) *(&buffer.data()[i]);
					qint16 r_sample = (qint16) *(&buffer.data()[i+2]);
					m_audioBuffer[m_audioBufferFill].l  = l_sample * 10 * m_volume;
					m_audioBuffer[m_audioBufferFill].r  = r_sample * 10 * m_volume;
					++m_audioBufferFill;

					if (m_audioBufferFill >= m_audioBuffer.size())
					{
						uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);

						if (res != m_audioBufferFill)
						{
							qDebug("UDPSrc::audioReadyRead: (stereo) lost %u samples", m_audioBufferFill - res);
						}

						m_audioBufferFill = 0;
					}
				}
			}
			else
			{
				for (int i = 0; i < buffer.size() - 1; i += 2)
				{
					qint16 sample = (qint16) *(&buffer.data()[i]);
					m_audioBuffer[m_audioBufferFill].l  = sample * 10 * m_volume;
					m_audioBuffer[m_audioBufferFill].r  = sample * 10 * m_volume;
					++m_audioBufferFill;

					if (m_audioBufferFill >= m_audioBuffer.size())
					{
						uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);

						if (res != m_audioBufferFill)
						{
							qDebug("UDPSrc::audioReadyRead: (mono) lost %u samples", m_audioBufferFill - res);
						}

						m_audioBufferFill = 0;
					}
				}
			}

			if (m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 0) != m_audioBufferFill)
			{
				qDebug("UDPSrc::audioReadyRead: lost samples");
			}

			m_audioBufferFill = 0;
		}
	}

	//qDebug("UDPSrc::audioReadyRead: done");
}
