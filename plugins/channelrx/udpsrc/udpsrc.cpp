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

#include <QUdpSocket>
#include <QHostAddress>

#include "dsp/downchannelizer.h"
#include "dsp/dspengine.h"
#include "util/db.h"

#include "udpsrcgui.h"
#include "udpsrc.h"

const Real UDPSrc::m_agcTarget = 16384.0f;

MESSAGE_CLASS_DEFINITION(UDPSrc::MsgUDPSrcConfigure, Message)
MESSAGE_CLASS_DEFINITION(UDPSrc::MsgUDPSrcConfigureImmediate, Message)
MESSAGE_CLASS_DEFINITION(UDPSrc::MsgUDPSrcSpectrum, Message)

UDPSrc::UDPSrc(MessageQueue* uiMessageQueue, UDPSrcGUI* udpSrcGUI, BasebandSampleSink* spectrum) :
    m_outMovingAverage(480, 1e-10),
    m_inMovingAverage(480, 1e-10),
    m_amMovingAverage(1200, 1e-10),
    m_audioFifo(24000),
    m_squelchOpen(false),
    m_squelchOpenCount(0),
    m_squelchCloseCount(0),
    m_squelchGate(4800),
    m_squelchRelease(4800),
    m_agc(9600, m_agcTarget, 1e-6),
    m_settingsMutex(QMutex::Recursive)
{
	setObjectName("UDPSrc");

	m_udpBuffer = new UDPSink<Sample>(this, udpBlockSize, m_config.m_udpPort);
	m_udpBufferMono = new UDPSink<FixReal>(this, udpBlockSize, m_config.m_udpPort);
	m_audioSocket = new QUdpSocket(this);
	m_udpAudioBuf = new char[m_udpAudioPayloadSize];

	m_audioBuffer.resize(1<<9);
	m_audioBufferFill = 0;

	m_nco.setFreq(0, m_config.m_inputSampleRate);
	m_interpolator.create(16, m_config.m_inputSampleRate, m_config.m_rfBandwidth / 2.0);
	m_sampleDistanceRemain = m_config.m_inputSampleRate / m_config.m_outputSampleRate;
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
	UDPFilter = new fftfilt(0.0, (m_config.m_rfBandwidth / 2.0) / m_config.m_outputSampleRate, udpBlockSize);

	m_phaseDiscri.setFMScaling((float) m_config. m_outputSampleRate / (2.0f * m_config.m_fmDeviation));

	if (m_audioSocket->bind(QHostAddress::LocalHost, m_config.m_audioPort))
	{
		qDebug("UDPSrc::UDPSrc: bind audio socket to port %d", m_config.m_audioPort);
		connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()), Qt::QueuedConnection);
	}
	else
	{
		qWarning("UDPSrc::UDPSrc: cannot bind audio port");
	}

    m_agc.setClampMax(32768.0*32768.0);
    m_agc.setClamping(true);

	//DSPEngine::instance()->addAudioSink(&m_audioFifo);
}

UDPSrc::~UDPSrc()
{
	delete m_audioSocket;
	delete m_udpBuffer;
	delete m_udpBufferMono;
	delete[] m_udpAudioBuf;
	if (UDPFilter) delete UDPFilter;
	if (m_running.m_audioActive) DSPEngine::instance()->removeAudioSink(&m_audioFifo);
}

/** what needs the "apply" button validation */
void UDPSrc::configure(MessageQueue* messageQueue,
		SampleFormat sampleFormat,
		Real outputSampleRate,
		Real rfBandwidth,
		int fmDeviation,
		QString& udpAddress,
		int udpPort,
		int audioPort,
		bool force)
{
	Message* cmd = MsgUDPSrcConfigure::create(sampleFormat,
			outputSampleRate,
			rfBandwidth,
			fmDeviation,
			udpAddress,
			udpPort,
			audioPort,
			force);
	messageQueue->push(cmd);
}

/** changes applied immediately */
void UDPSrc::configureImmediate(MessageQueue* messageQueue,
		bool audioActive,
		bool audioStereo,
		Real boost,
		int  volume,
		Real squelchDB,
		Real squelchGate,
		bool squelchEnabled,
		bool agc,
		bool force)
{
	Message* cmd = MsgUDPSrcConfigureImmediate::create(
			audioActive,
			audioStereo,
			boost,
			volume,
			squelchDB,
			squelchGate,
			squelchEnabled,
			agc,
			force);
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
	double l, r;

	m_sampleBuffer.clear();
	m_settingsMutex.lock();

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if(m_interpolator.decimate(&m_sampleDistanceRemain, c, &ci))
		{
		    double inMagSq;
		    double agcFactor = 1.0;

            if ((m_running.m_agc) &&
                (m_running.m_sampleFormat != FormatNFM) &&
                (m_running.m_sampleFormat != FormatNFMMono) &&
                (m_running.m_sampleFormat != FormatS16LE))
            {
                agcFactor = m_agc.feedAndGetValue(ci);
                inMagSq = m_agc.getMagSq();
            }
            else
            {
                inMagSq = ci.real()*ci.real() + ci.imag()*ci.imag();
            }

		    m_inMovingAverage.feed(inMagSq / (1<<30));
		    m_inMagsq = m_inMovingAverage.average();

			Sample ss(ci.real(), ci.imag());
			m_sampleBuffer.push_back(ss);

			m_sampleDistanceRemain += m_running.m_inputSampleRate / m_running.m_outputSampleRate;

			calculateSquelch(m_inMagsq);

			if (m_running.m_sampleFormat == FormatLSB) // binaural LSB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, false);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? sideband[i].real() * m_running.m_gain : 0;
						r = m_squelchOpen ? sideband[i].imag() * m_running.m_gain : 0;
					    m_udpBuffer->write(Sample(l, r));
					    m_outMovingAverage.feed((l*l + r*r) / (1<<30));
					}
				}
			}
			if (m_running.m_sampleFormat == FormatUSB) // binaural USB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, true);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? sideband[i].real() * m_running.m_gain : 0;
						r = m_squelchOpen ? sideband[i].imag() * m_running.m_gain : 0;
						m_udpBuffer->write(Sample(l, r));
						m_outMovingAverage.feed((l*l + r*r) / (1<<30));
					}
				}
			}
			else if (m_running.m_sampleFormat == FormatNFM)
			{
				double demod = m_squelchOpen ? 32768.0 * m_phaseDiscri.phaseDiscriminator(ci) * m_running.m_gain : 0;
				m_udpBuffer->write(Sample(demod, demod));
				m_outMovingAverage.feed((demod * demod) / (1<<30));
			}
			else if (m_running.m_sampleFormat == FormatNFMMono)
			{
				FixReal demod = m_squelchOpen ? (FixReal) (32768.0f * m_phaseDiscri.phaseDiscriminator(ci) * m_running.m_gain) : 0;
				m_udpBufferMono->write(demod);
				m_outMovingAverage.feed((demod * demod) / 1073741824.0);
			}
			else if (m_running.m_sampleFormat == FormatLSBMono) // Monaural LSB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, false);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? (sideband[i].real() + sideband[i].imag()) * 0.7 * m_running.m_gain : 0;
						m_udpBufferMono->write(l);
						m_outMovingAverage.feed((l * l) / (1<<30));
					}
				}
			}
			else if (m_running.m_sampleFormat == FormatUSBMono) // Monaural USB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, true);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? (sideband[i].real() + sideband[i].imag()) * 0.7 * m_running.m_gain : 0;
						m_udpBufferMono->write(l);
						m_outMovingAverage.feed((l * l) / (1<<30));
					}
				}
			}
			else if (m_running.m_sampleFormat == FormatAMMono)
			{
				FixReal demod = m_squelchOpen ? (FixReal) (sqrt(inMagSq) * agcFactor * m_running.m_gain) : 0;
				m_udpBufferMono->write(demod);
				m_outMovingAverage.feed((demod * demod) / 1073741824.0);
			}
            else if (m_running.m_sampleFormat == FormatAMNoDCMono)
            {
                if (m_squelchOpen)
                {
                    double demodf = sqrt(inMagSq);
                    m_amMovingAverage.feed(demodf);
                    FixReal demod = (FixReal) ((demodf - m_amMovingAverage.average()) * agcFactor * m_running.m_gain);
                    m_udpBufferMono->write(demod);
                    m_outMovingAverage.feed((demod * demod) / 1073741824.0);
                }
                else
                {
                    m_udpBufferMono->write(0);
                    m_outMovingAverage.feed(0);
                }
            }
            else if (m_running.m_sampleFormat == FormatAMBPFMono)
            {
                if (m_squelchOpen)
                {
                    double demodf = sqrt(inMagSq);
                    demodf = m_bandpass.filter(demodf);
                    demodf /= 301.0;
                    FixReal demod = (FixReal) (demodf * agcFactor * m_running.m_gain);
                    m_udpBufferMono->write(demod);
                    m_outMovingAverage.feed((demod * demod) / 1073741824.0);
                }
                else
                {
                    m_udpBufferMono->write(0);
                    m_outMovingAverage.feed(0);
                }
            }
			else // Raw I/Q samples
			{
			    if (m_squelchOpen)
			    {
	                Sample s(ci.real() * m_running.m_gain, ci.imag() * m_running.m_gain);
	                m_udpBuffer->write(s);
	                m_outMovingAverage.feed((inMagSq*m_running.m_gain*m_running.m_gain) / (1<<30));
			    }
			    else
			    {
	                Sample s(0, 0);
	                m_udpBuffer->write(s);
	                m_outMovingAverage.feed(0);
			    }
			}

            m_magsq = m_outMovingAverage.average();
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

		m_config.m_inputSampleRate = notif.getSampleRate();
		m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

		apply(false);

		qDebug() << "UDPSrc::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << m_config.m_inputSampleRate
                 << " frequencyOffset: " << notif.getFrequencyOffset();

        return true;
	}
	else if (MsgUDPSrcConfigureImmediate::match(cmd))
	{
		MsgUDPSrcConfigureImmediate& cfg = (MsgUDPSrcConfigureImmediate&) cmd;

		m_config.m_audioActive = cfg.getAudioActive();
		m_config.m_audioStereo = cfg.getAudioStereo();
		m_config.m_gain = cfg.getGain();
		m_config.m_volume = cfg.getVolume();
		m_config.m_squelch = CalcDb::powerFromdB(cfg.getSquelchDB());
		m_config.m_squelchGate = cfg.getSquelchGate();
		m_config.m_squelchEnabled = cfg.getSquelchEnabled();
		m_config.m_agc = cfg.getAGC();

		apply(cfg.getForce());

		qDebug() << "UDPSrc::handleMessage: MsgUDPSrcConfigureImmediate: "
				<< " m_audioActive: " << m_config.m_audioActive
				<< " m_audioStereo: " << m_config.m_audioStereo
				<< " m_gain: " << m_config.m_gain
				<< " m_squelchEnabled: " << m_config.m_squelchEnabled
                << " m_squelch: " << m_config.m_squelch
                << " getSquelchDB: " << cfg.getSquelchDB()
                << " m_squelchGate" << m_config.m_squelchGate
                << " m_agc" << m_config.m_agc;

		return true;

	}
	else if (MsgUDPSrcConfigure::match(cmd))
	{
		MsgUDPSrcConfigure& cfg = (MsgUDPSrcConfigure&) cmd;

		m_config.m_sampleFormat = cfg.getSampleFormat();
		m_config.m_outputSampleRate = cfg.getOutputSampleRate();
		m_config.m_rfBandwidth = cfg.getRFBandwidth();
		m_config.m_udpAddressStr = cfg.getUDPAddress();
		m_config.m_udpPort = cfg.getUDPPort();
		m_config.m_audioPort = cfg.getAudioPort();
		m_config.m_fmDeviation = cfg.getFMDeviation();

		apply(cfg.getForce());

		qDebug() << "UDPSrc::handleMessage: MsgUDPSrcConfigure: m_sampleFormat: " << m_config.m_sampleFormat
				<< " m_outputSampleRate: " << m_config.m_outputSampleRate
				<< " m_rfBandwidth: " << m_config.m_rfBandwidth
				<< " m_udpAddressStr: " << m_config.m_udpAddressStr
				<< " m_udpPort: " << m_config.m_udpPort
				<< " m_audioPort: " << m_config.m_audioPort;

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

void UDPSrc::apply(bool force)
{
    m_settingsMutex.lock();

    if ((m_config.m_inputSampleRate != m_running.m_inputSampleRate) ||
        (m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
        (m_config.m_rfBandwidth != m_running.m_rfBandwidth) ||
        (m_config.m_outputSampleRate != m_running.m_outputSampleRate) || force)
    {
        m_nco.setFreq(-m_config.m_inputFrequencyOffset, m_config.m_inputSampleRate);
        m_interpolator.create(16, m_config.m_inputSampleRate, m_config.m_rfBandwidth / 2.0);
        m_sampleDistanceRemain = m_config.m_inputSampleRate / m_config.m_outputSampleRate;

        if ((m_config.m_sampleFormat == FormatLSB) ||
            (m_config.m_sampleFormat == FormatLSBMono) ||
            (m_config.m_sampleFormat == FormatUSB) ||
            (m_config.m_sampleFormat == FormatUSBMono))
        {
            m_squelchGate = m_config.m_outputSampleRate * 0.05;
        }
        else
        {
            m_squelchGate = m_config.m_outputSampleRate * m_config.m_squelchGate;
        }

        m_squelchRelease = m_config.m_outputSampleRate * m_config.m_squelchGate;
        initSquelch(m_squelchOpen);
        m_agc.resize(m_config.m_outputSampleRate * 0.2, m_agcTarget); // Fixed 200 ms
        m_agc.setStepDownDelay( m_config.m_outputSampleRate * (m_config.m_squelchGate == 0 ? 0.01 : m_config.m_squelchGate));
        m_agc.setGate(m_config.m_outputSampleRate * 0.05);

        m_bandpass.create(301, m_config.m_outputSampleRate, 300.0, m_config.m_rfBandwidth / 2.0f);

        m_inMovingAverage.resize(m_config.m_outputSampleRate * 0.01, 1e-10);  // 10 ms
        m_amMovingAverage.resize(m_config.m_outputSampleRate * 0.005, 1e-10); //  5 ms
        m_outMovingAverage.resize(m_config.m_outputSampleRate * 0.01, 1e-10); // 10 ms
    }

    if ((m_config.m_audioActive != m_config.m_audioActive) || force)
    {
        if (m_config.m_audioActive)
        {
            m_audioBufferFill = 0;
            DSPEngine::instance()->addAudioSink(&m_audioFifo);
        }
        else
        {
            DSPEngine::instance()->removeAudioSink(&m_audioFifo);
        }
    }

    if ((m_config.m_squelchGate != m_running.m_squelchGate) || force)
    {
        if ((m_config.m_sampleFormat == FormatLSB) ||
            (m_config.m_sampleFormat == FormatLSBMono) ||
            (m_config.m_sampleFormat == FormatUSB) ||
            (m_config.m_sampleFormat == FormatUSBMono))
        {
            m_squelchGate = m_config.m_outputSampleRate * 0.05;
        }
        else
        {
            m_squelchGate = m_config.m_outputSampleRate * m_config.m_squelchGate;
        }

        m_squelchRelease = m_config.m_outputSampleRate * m_config.m_squelchGate;
        initSquelch(m_squelchOpen);
        m_agc.setStepDownDelay(m_config.m_outputSampleRate * (m_config.m_squelchGate == 0 ? 0.01 : m_config.m_squelchGate)); // same delay for up and down
    }

    if ((m_config.m_squelch != m_running.m_squelch) || force)
    {
        m_agc.setThreshold(m_config.m_squelch*(1<<23));
    }

    if ((m_config.m_udpAddressStr != m_running.m_udpAddressStr) || force)
    {
        m_udpBuffer->setAddress(m_config.m_udpAddressStr);
        m_udpBufferMono->setAddress(m_config.m_udpAddressStr);
    }

    if ((m_config.m_udpPort != m_running.m_udpPort) || force)
    {
        m_udpBuffer->setPort(m_config.m_udpPort);
        m_udpBufferMono->setPort(m_config.m_udpPort);
    }

    if ((m_config.m_audioPort != m_running.m_audioPort) || force)
    {
        disconnect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()));
        delete m_audioSocket;
        m_audioSocket = new QUdpSocket(this);

        if (m_audioSocket->bind(QHostAddress::LocalHost, m_config.m_audioPort))
        {
            connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()), Qt::QueuedConnection);
            qDebug("UDPSrc::handleMessage: audio socket bound to port %d", m_config.m_audioPort);
        }
        else
        {
            qWarning("UDPSrc::handleMessage: cannot bind audio socket");
        }
    }

    if ((m_config.m_fmDeviation != m_running.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling((float) m_config.m_outputSampleRate / (2.0f * m_config.m_fmDeviation));
    }

    m_settingsMutex.unlock();

    m_running = m_config;
}

void UDPSrc::audioReadyRead()
{
	while (m_audioSocket->hasPendingDatagrams())
	{
	    qint64 pendingDataSize = m_audioSocket->pendingDatagramSize();
	    qint64 udpReadBytes = m_audioSocket->readDatagram(m_udpAudioBuf, pendingDataSize, 0, 0);
		//qDebug("UDPSrc::audioReadyRead: %lld", udpReadBytes);

		if (m_running.m_audioActive)
		{
			if (m_running.m_audioStereo)
			{
				for (int i = 0; i < udpReadBytes - 3; i += 4)
				{
					qint16 l_sample = (qint16) *(&m_udpAudioBuf[i]);
					qint16 r_sample = (qint16) *(&m_udpAudioBuf[i+2]);
					m_audioBuffer[m_audioBufferFill].l  = l_sample * m_running.m_volume;
					m_audioBuffer[m_audioBufferFill].r  = r_sample * m_running.m_volume;
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
					m_audioBuffer[m_audioBufferFill].l  = sample * m_running.m_volume;
					m_audioBuffer[m_audioBufferFill].r  = sample * m_running.m_volume;
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
