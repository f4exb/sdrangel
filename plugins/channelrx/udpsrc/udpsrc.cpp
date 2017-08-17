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

#include "udpsrc.h"

#include <dsp/downchannelizer.h>
#include <QUdpSocket>
#include <QHostAddress>
#include "dsp/dspengine.h"

#include "udpsrcgui.h"

MESSAGE_CLASS_DEFINITION(UDPSrc::MsgUDPSrcConfigure, Message)
MESSAGE_CLASS_DEFINITION(UDPSrc::MsgUDPSrcConfigureImmediate, Message)
MESSAGE_CLASS_DEFINITION(UDPSrc::MsgUDPSrcSpectrum, Message)

UDPSrc::UDPSrc(MessageQueue* uiMessageQueue, UDPSrcGUI* udpSrcGUI, BasebandSampleSink* spectrum) :
	m_udpPort(9999),
	m_gain(1.0),
	m_audioActive(false),
	m_audioStereo(false),
	m_volume(20),
    m_fmDeviation(2500),
    m_outMovingAverage(480, 1e-10),
    m_inMovingAverage(480, 1e-10),
    m_audioFifo(4, 24000),
    m_settingsMutex(QMutex::Recursive)
{
	setObjectName("UDPSrc");

	m_udpBuffer = new UDPSink<Sample>(this, udpBlockSize, m_udpPort);
	m_udpBufferMono = new UDPSink<FixReal>(this, udpBlockSize, m_udpPort);
	m_audioSocket = new QUdpSocket(this);
	m_udpAudioBuf = new char[m_udpAudioPayloadSize];

	m_audioBuffer.resize(1<<9);
	m_audioBufferFill = 0;

	m_inputSampleRate = 96000;
	m_sampleFormat = FormatS16LE;
	m_outputSampleRate = 48000;
	m_rfBandwidth = 32000;
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
	m_magsq = 0;
	m_inMagsq = 0;
	UDPFilter = new fftfilt(0.0, (m_rfBandwidth / 2.0) / m_outputSampleRate, udpBlockSize);

	m_phaseDiscri.setFMScaling((float) m_outputSampleRate / (2.0f * m_fmDeviation));

	if (m_audioSocket->bind(QHostAddress::LocalHost, m_audioPort))
	{
		qDebug("UDPSrc::UDPSrc: bind audio socket to port %d", m_audioPort);
		connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()), Qt::QueuedConnection);
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
	delete m_udpBuffer;
	delete m_udpBufferMono;
	delete[] m_udpAudioBuf;
	if (UDPFilter) delete UDPFilter;
	if (m_audioActive) DSPEngine::instance()->removeAudioSink(&m_audioFifo);
}

void UDPSrc::configure(MessageQueue* messageQueue,
		SampleFormat sampleFormat,
		Real outputSampleRate,
		Real rfBandwidth,
		int fmDeviation,
		QString& udpAddress,
		int udpPort,
		int audioPort)
{
	Message* cmd = MsgUDPSrcConfigure::create(sampleFormat,
			outputSampleRate,
			rfBandwidth,
			fmDeviation,
			udpAddress,
			udpPort,
			audioPort);
	messageQueue->push(cmd);
}

void UDPSrc::configureImmediate(MessageQueue* messageQueue,
		bool audioActive,
		bool audioStereo,
		Real boost,
		int  volume,
		Real squelchDB,
		bool squelchEnabled)
{
	Message* cmd = MsgUDPSrcConfigureImmediate::create(
			audioActive,
			audioStereo,
			boost,
			volume,
			squelchDB,
			squelchEnabled);
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

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if(m_interpolator.decimate(&m_sampleDistanceRemain, c, &ci))
		{
		    double inMagSq = ci.real()*ci.real() + ci.imag()*ci.imag();
			//m_magsq = (inMagSq*m_gain*m_gain) / (1<<30);
			m_outMovingAverage.feed((inMagSq*m_gain*m_gain) / (1<<30));
		    m_inMovingAverage.feed(inMagSq / (1<<30));
		    m_magsq = m_outMovingAverage.average();
		    m_inMagsq = m_inMovingAverage.average();

			Sample s(ci.real() * m_gain, ci.imag() * m_gain);
			m_sampleBuffer.push_back(s);
			m_sampleDistanceRemain += m_inputSampleRate / m_outputSampleRate;

			if (m_sampleFormat == FormatLSB)
			{
				int n_out = UDPFilter->runSSB(ci, &sideband, false);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = sideband[i].real();
						r = sideband[i].imag();
						m_udpBuffer->write(Sample(l, r));
					}
				}
			}
			if (m_sampleFormat == FormatUSB)
			{
				int n_out = UDPFilter->runSSB(ci, &sideband, true);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = sideband[i].real();
						r = sideband[i].imag();
						m_udpBuffer->write(Sample(l, r));
					}
				}
			}
			else if (m_sampleFormat == FormatNFM)
			{
				Real demod = 32768.0f * m_phaseDiscri.phaseDiscriminator(ci);
				m_udpBuffer->write(Sample(demod, demod));
			}
			else if (m_sampleFormat == FormatNFMMono)
			{
				FixReal demod = (FixReal) (32768.0f * m_phaseDiscri.phaseDiscriminator(ci));
				m_udpBufferMono->write(demod);
			}
			else if (m_sampleFormat == FormatLSBMono)
			{
				int n_out = UDPFilter->runSSB(ci, &sideband, false);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = (sideband[i].real() + sideband[i].imag()) * 0.7;
						m_udpBufferMono->write(l);
					}
				}
			}
			else if (m_sampleFormat == FormatUSBMono)
			{
				int n_out = UDPFilter->runSSB(ci, &sideband, true);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = (sideband[i].real() + sideband[i].imag()) * 0.7;
						m_udpBufferMono->write(l);
					}
				}
			}
			else if (m_sampleFormat == FormatAMMono)
			{
				FixReal demod = (FixReal) (32768.0f * sqrt(inMagSq) * m_gain);
				m_udpBufferMono->write(demod);
			}
			else // Raw I/Q samples
			{
				m_udpBuffer->write(s);
			}
		}
	}

	//qDebug() << "UDPSrc::feed: " << m_sampleBuffer.size() * 4;

	if((m_spectrum != 0) && (m_spectrumEnabled))
	{
		m_spectrum->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), positiveOnly);
	}

	m_settingsMutex.unlock();
}

void UDPSrc::start()
{
	m_phaseDiscri.reset();
}

void UDPSrc::stop()
{
}

bool UDPSrc::handleMessage(const Message& cmd)
{
	qDebug() << "UDPSrc::handleMessage";

	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

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

		if (cfg.getGain() != m_gain)
		{
		    m_gain = cfg.getGain();
		}

		if (cfg.getVolume() != m_volume)
		{
			m_volume = cfg.getVolume();
		}

		m_settingsMutex.unlock();

		qDebug() << "UDPSrc::handleMessage: MsgUDPSrcConfigureImmediate: "
				<< " m_audioActive: " << m_audioActive
				<< " m_audioStereo: " << m_audioStereo
				<< " m_gain: " << m_gain
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

		if (cfg.getUDPAddress() != m_udpAddressStr)
		{
			m_udpAddressStr = cfg.getUDPAddress();
			m_udpBuffer->setAddress(m_udpAddressStr);
			m_udpBufferMono->setAddress(m_udpAddressStr);
		}

		if (cfg.getUDPPort() != m_udpPort)
		{
			m_udpPort = cfg.getUDPPort();
			m_udpBuffer->setPort(m_udpPort);
			m_udpBufferMono->setPort(m_udpPort);
		}

		if (cfg.getAudioPort() != m_audioPort)
		{
			m_audioPort = cfg.getAudioPort();

			disconnect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()));
			delete m_audioSocket;
			m_audioSocket = new QUdpSocket(this);

			if (m_audioSocket->bind(QHostAddress::LocalHost, m_audioPort))
			{
				connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()), Qt::QueuedConnection);
				qDebug("UDPSrc::handleMessage: audio socket bound to port %d", m_audioPort);
			}
			else
			{
				qWarning("UDPSrc::handleMessage: cannot bind audio socket");
			}
		}

		if (cfg.getFMDeviation() != m_fmDeviation)
		{
			m_fmDeviation = cfg.getFMDeviation();
			m_phaseDiscri.setFMScaling((float) m_outputSampleRate / (2.0f * m_fmDeviation));
		}

		m_interpolator.create(16, m_inputSampleRate, m_rfBandwidth / 2.0);
		m_sampleDistanceRemain = m_inputSampleRate / m_outputSampleRate;
		UDPFilter->create_filter(0.0, (m_rfBandwidth / 2.0) / m_outputSampleRate);

        m_inMovingAverage.resize(m_inputSampleRate * 0.01, 1e-10);   // 10 ms
        m_outMovingAverage.resize(m_outputSampleRate * 0.01, 1e-10); // 10 ms

		m_settingsMutex.unlock();

		qDebug() << "UDPSrc::handleMessage: MsgUDPSrcConfigure: m_sampleFormat: " << m_sampleFormat
				<< " m_outputSampleRate: " << m_outputSampleRate
				<< " m_rfBandwidth: " << m_rfBandwidth
				<< " m_udpAddressStr: " << m_udpAddressStr
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
	while (m_audioSocket->hasPendingDatagrams())
	{
	    qint64 pendingDataSize = m_audioSocket->pendingDatagramSize();
	    qint64 udpReadBytes = m_audioSocket->readDatagram(m_udpAudioBuf, pendingDataSize, 0, 0);
		//qDebug("UDPSrc::audioReadyRead: %lld", udpReadBytes);

		if (m_audioActive)
		{
			if (m_audioStereo)
			{
				for (int i = 0; i < udpReadBytes - 3; i += 4)
				{
					qint16 l_sample = (qint16) *(&m_udpAudioBuf[i]);
					qint16 r_sample = (qint16) *(&m_udpAudioBuf[i+2]);
					m_audioBuffer[m_audioBufferFill].l  = l_sample * m_volume;
					m_audioBuffer[m_audioBufferFill].r  = r_sample * m_volume;
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
				for (int i = 0; i < udpReadBytes - 1; i += 2)
				{
					qint16 sample = (qint16) *(&m_udpAudioBuf[i]);
					m_audioBuffer[m_audioBufferFill].l  = sample * m_volume;
					m_audioBuffer[m_audioBufferFill].r  = sample * m_volume;
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
