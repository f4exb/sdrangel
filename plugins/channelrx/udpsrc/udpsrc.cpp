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

#include "dsp/dspengine.h"
#include "util/db.h"
#include "dsp/downchannelizer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "device/devicesourceapi.h"

#include "udpsrcgui.h"
#include "udpsrc.h"

const Real UDPSrc::m_agcTarget = 16384.0f;

MESSAGE_CLASS_DEFINITION(UDPSrc::MsgConfigureUDPSrc, Message)
MESSAGE_CLASS_DEFINITION(UDPSrc::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(UDPSrc::MsgUDPSrcSpectrum, Message)

const QString UDPSrc::m_channelID = "sdrangel.channel.udpsrc";

UDPSrc::UDPSrc(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_absoluteFrequencyOffset(0),
    m_outMovingAverage(480, 1e-10),
    m_inMovingAverage(480, 1e-10),
    m_amMovingAverage(1200, 1e-10),
    m_audioFifo(24000),
    m_spectrum(0),
    m_squelch(1e-6),
    m_squelchOpen(false),
    m_squelchOpenCount(0),
    m_squelchCloseCount(0),
    m_squelchGate(4800),
    m_squelchRelease(4800),
    m_agc(9600, m_agcTarget, 1e-6),
    m_settingsMutex(QMutex::Recursive)
{
	setObjectName("UDPSrc");

	m_udpBuffer = new UDPSink<Sample>(this, udpBlockSize, m_settings.m_udpPort);
	m_udpBufferMono = new UDPSink<FixReal>(this, udpBlockSize, m_settings.m_udpPort);
	m_audioSocket = new QUdpSocket(this);
	m_udpAudioBuf = new char[m_udpAudioPayloadSize];

	m_audioBuffer.resize(1<<9);
	m_audioBufferFill = 0;

	m_nco.setFreq(0, m_settings.m_inputSampleRate);
	m_interpolator.create(16, m_settings.m_inputSampleRate, m_settings.m_rfBandwidth / 2.0);
	m_sampleDistanceRemain = m_settings.m_inputSampleRate / m_settings.m_outputSampleRate;
	m_spectrumEnabled = false;
	m_nextSSBId = 0;
	m_nextS16leId = 0;

	m_last = 0;
	m_this = 0;
	m_scale = 0;
	m_magsq = 0;
	m_inMagsq = 0;

	UDPFilter = new fftfilt(0.0, (m_settings.m_rfBandwidth / 2.0) / m_settings.m_outputSampleRate, udpBlockSize);

	m_phaseDiscri.setFMScaling((float) m_settings. m_outputSampleRate / (2.0f * m_settings.m_fmDeviation));

	if (m_audioSocket->bind(QHostAddress::LocalHost, m_settings.m_audioPort))
	{
		qDebug("UDPSrc::UDPSrc: bind audio socket to port %d", m_settings.m_audioPort);
		connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()), Qt::QueuedConnection);
	}
	else
	{
		qWarning("UDPSrc::UDPSrc: cannot bind audio port");
	}

    m_agc.setClampMax(32768.0*32768.0);
    m_agc.setClamping(true);

	//DSPEngine::instance()->addAudioSink(&m_audioFifo);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    applySettings(m_settings, true);
}

UDPSrc::~UDPSrc()
{
	delete m_audioSocket;
	delete m_udpBuffer;
	delete m_udpBufferMono;
	delete[] m_udpAudioBuf;
	if (UDPFilter) delete UDPFilter;
	if (m_settings.m_audioActive) DSPEngine::instance()->removeAudioSink(&m_audioFifo);
	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
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

            if ((m_settings.m_agc) &&
                (m_settings.m_sampleFormat != UDPSrcSettings::FormatNFM) &&
                (m_settings.m_sampleFormat != UDPSrcSettings::FormatNFMMono) &&
                (m_settings.m_sampleFormat != UDPSrcSettings::FormatS16LE))
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

			m_sampleDistanceRemain += m_settings.m_inputSampleRate / m_settings.m_outputSampleRate;

			calculateSquelch(m_inMagsq);

			if (m_settings.m_sampleFormat == UDPSrcSettings::FormatLSB) // binaural LSB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, false);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? sideband[i].real() * m_settings.m_gain : 0;
						r = m_squelchOpen ? sideband[i].imag() * m_settings.m_gain : 0;
					    m_udpBuffer->write(Sample(l, r));
					    m_outMovingAverage.feed((l*l + r*r) / (1<<30));
					}
				}
			}
			if (m_settings.m_sampleFormat == UDPSrcSettings::FormatUSB) // binaural USB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, true);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? sideband[i].real() * m_settings.m_gain : 0;
						r = m_squelchOpen ? sideband[i].imag() * m_settings.m_gain : 0;
						m_udpBuffer->write(Sample(l, r));
						m_outMovingAverage.feed((l*l + r*r) / (1<<30));
					}
				}
			}
			else if (m_settings.m_sampleFormat == UDPSrcSettings::FormatNFM)
			{
				double demod = m_squelchOpen ? 32768.0 * m_phaseDiscri.phaseDiscriminator(ci) * m_settings.m_gain : 0;
				m_udpBuffer->write(Sample(demod, demod));
				m_outMovingAverage.feed((demod * demod) / (1<<30));
			}
			else if (m_settings.m_sampleFormat == UDPSrcSettings::FormatNFMMono)
			{
				FixReal demod = m_squelchOpen ? (FixReal) (32768.0f * m_phaseDiscri.phaseDiscriminator(ci) * m_settings.m_gain) : 0;
				m_udpBufferMono->write(demod);
				m_outMovingAverage.feed((demod * demod) / 1073741824.0);
			}
			else if (m_settings.m_sampleFormat == UDPSrcSettings::FormatLSBMono) // Monaural LSB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, false);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? (sideband[i].real() + sideband[i].imag()) * 0.7 * m_settings.m_gain : 0;
						m_udpBufferMono->write(l);
						m_outMovingAverage.feed((l * l) / (1<<30));
					}
				}
			}
			else if (m_settings.m_sampleFormat == UDPSrcSettings::FormatUSBMono) // Monaural USB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, true);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? (sideband[i].real() + sideband[i].imag()) * 0.7 * m_settings.m_gain : 0;
						m_udpBufferMono->write(l);
						m_outMovingAverage.feed((l * l) / (1<<30));
					}
				}
			}
			else if (m_settings.m_sampleFormat == UDPSrcSettings::FormatAMMono)
			{
				FixReal demod = m_squelchOpen ? (FixReal) (sqrt(inMagSq) * agcFactor * m_settings.m_gain) : 0;
				m_udpBufferMono->write(demod);
				m_outMovingAverage.feed((demod * demod) / 1073741824.0);
			}
            else if (m_settings.m_sampleFormat == UDPSrcSettings::FormatAMNoDCMono)
            {
                if (m_squelchOpen)
                {
                    double demodf = sqrt(inMagSq);
                    m_amMovingAverage.feed(demodf);
                    FixReal demod = (FixReal) ((demodf - m_amMovingAverage.average()) * agcFactor * m_settings.m_gain);
                    m_udpBufferMono->write(demod);
                    m_outMovingAverage.feed((demod * demod) / 1073741824.0);
                }
                else
                {
                    m_udpBufferMono->write(0);
                    m_outMovingAverage.feed(0);
                }
            }
            else if (m_settings.m_sampleFormat == UDPSrcSettings::FormatAMBPFMono)
            {
                if (m_squelchOpen)
                {
                    double demodf = sqrt(inMagSq);
                    demodf = m_bandpass.filter(demodf);
                    demodf /= 301.0;
                    FixReal demod = (FixReal) (demodf * agcFactor * m_settings.m_gain);
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
	                Sample s(ci.real() * m_settings.m_gain, ci.imag() * m_settings.m_gain);
	                m_udpBuffer->write(s);
	                m_outMovingAverage.feed((inMagSq*m_settings.m_gain*m_settings.m_gain) / (1<<30));
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
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

		UDPSrcSettings settings;

		settings.m_inputSampleRate = notif.getSampleRate();
		settings.m_inputFrequencyOffset = notif.getFrequencyOffset();

		//apply(false);
		applySettings(settings);

		qDebug() << "UDPSrc::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << settings.m_inputSampleRate
                 << " frequencyOffset: " << notif.getFrequencyOffset();

        return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        qDebug() << "UDPSrc::handleMessage: MsgConfigureChannelizer:"
                << " sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        return true;
    }
    else if (MsgConfigureUDPSrc::match(cmd))
    {
        MsgConfigureUDPSrc& cfg = (MsgConfigureUDPSrc&) cmd;

        UDPSrcSettings settings = cfg.getSettings();

        // These settings are set with DownChannelizer::MsgChannelizerNotification
        m_absoluteFrequencyOffset = settings.m_inputFrequencyOffset;
        settings.m_inputSampleRate = m_settings.m_inputSampleRate;
        settings.m_inputFrequencyOffset = m_settings.m_inputFrequencyOffset;

        applySettings(settings, cfg.getForce());

        qDebug() << "UDPSrc::handleMessage: MsgConfigureUDPSrc: "
                << " m_inputSampleRate: " << settings.m_inputSampleRate
                << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
                << " m_audioActive: " << settings.m_audioActive
                << " m_audioStereo: " << settings.m_audioStereo
                << " m_gain: " << settings.m_gain
                << " m_volume: " << settings.m_volume
                << " m_squelchEnabled: " << settings.m_squelchEnabled
                << " m_squelchdB: " << settings.m_squelchdB
                << " m_squelchGate" << settings.m_squelchGate
                << " m_agc" << settings.m_agc
                << " m_sampleFormat: " << settings.m_sampleFormat
                << " m_outputSampleRate: " << settings.m_outputSampleRate
                << " m_rfBandwidth: " << settings.m_rfBandwidth
                << " m_fmDeviation: " << settings.m_fmDeviation
                << " m_udpAddressStr: " << settings.m_udpAddress
                << " m_udpPort: " << settings.m_udpPort
                << " m_audioPort: " << settings.m_audioPort
                << " force: " << cfg.getForce();

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

		if (m_settings.m_audioActive)
		{
			if (m_settings.m_audioStereo)
			{
				for (int i = 0; i < udpReadBytes - 3; i += 4)
				{
					qint16 l_sample = (qint16) *(&m_udpAudioBuf[i]);
					qint16 r_sample = (qint16) *(&m_udpAudioBuf[i+2]);
					m_audioBuffer[m_audioBufferFill].l  = l_sample * m_settings.m_volume;
					m_audioBuffer[m_audioBufferFill].r  = r_sample * m_settings.m_volume;
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
					m_audioBuffer[m_audioBufferFill].l  = sample * m_settings.m_volume;
					m_audioBuffer[m_audioBufferFill].r  = sample * m_settings.m_volume;
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

void UDPSrc::applySettings(const UDPSrcSettings& settings, bool force)
{
    m_settingsMutex.lock();

    if ((settings.m_inputSampleRate != m_settings.m_inputSampleRate) ||
        (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) ||
        (settings.m_rfBandwidth != m_settings.m_rfBandwidth) ||
        (settings.m_outputSampleRate != m_settings.m_outputSampleRate) || force)
    {
        m_nco.setFreq(-settings.m_inputFrequencyOffset, settings.m_inputSampleRate);
        m_interpolator.create(16, settings.m_inputSampleRate, settings.m_rfBandwidth / 2.0);
        m_sampleDistanceRemain = settings.m_inputSampleRate / settings.m_outputSampleRate;

        if ((settings.m_sampleFormat == UDPSrcSettings::FormatLSB) ||
            (settings.m_sampleFormat == UDPSrcSettings::FormatLSBMono) ||
            (settings.m_sampleFormat == UDPSrcSettings::FormatUSB) ||
            (settings.m_sampleFormat == UDPSrcSettings::FormatUSBMono))
        {
            m_squelchGate = settings.m_outputSampleRate * 0.05;
        }
        else
        {
            m_squelchGate = (settings.m_outputSampleRate * settings.m_squelchGate) / 100;
        }

        m_squelchRelease = (settings.m_outputSampleRate * settings.m_squelchGate) / 100;
        initSquelch(m_squelchOpen);
        m_agc.resize(settings.m_outputSampleRate * 0.2, m_agcTarget); // Fixed 200 ms
        int stepDownDelay =  (settings.m_outputSampleRate * (settings.m_squelchGate == 0 ? 1 : settings.m_squelchGate))/100;
        m_agc.setStepDownDelay(stepDownDelay);
        m_agc.setGate(settings.m_outputSampleRate * 0.05);

        m_bandpass.create(301, settings.m_outputSampleRate, 300.0, settings.m_rfBandwidth / 2.0f);

        m_inMovingAverage.resize(settings.m_outputSampleRate * 0.01, 1e-10);  // 10 ms
        m_amMovingAverage.resize(settings.m_outputSampleRate * 0.005, 1e-10); //  5 ms
        m_outMovingAverage.resize(settings.m_outputSampleRate * 0.01, 1e-10); // 10 ms
    }

    if ((settings.m_audioActive != m_settings.m_audioActive) || force)
    {
        if (settings.m_audioActive)
        {
            m_audioBufferFill = 0;
            DSPEngine::instance()->addAudioSink(&m_audioFifo);
        }
        else
        {
            DSPEngine::instance()->removeAudioSink(&m_audioFifo);
        }
    }

    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force)
    {
        if ((settings.m_sampleFormat == UDPSrcSettings::FormatLSB) ||
            (settings.m_sampleFormat == UDPSrcSettings::FormatLSBMono) ||
            (settings.m_sampleFormat == UDPSrcSettings::FormatUSB) ||
            (settings.m_sampleFormat == UDPSrcSettings::FormatUSBMono))
        {
            m_squelchGate = settings.m_outputSampleRate * 0.05;
        }
        else
        {
            m_squelchGate = (settings.m_outputSampleRate * settings.m_squelchGate)/100;
        }

        m_squelchRelease = (settings.m_outputSampleRate * settings.m_squelchGate)/100;
        initSquelch(m_squelchOpen);
        int stepDownDelay =  (settings.m_outputSampleRate * (settings.m_squelchGate == 0 ? 1 : settings.m_squelchGate))/100;
        m_agc.setStepDownDelay(stepDownDelay); // same delay for up and down
    }

    if ((settings.m_squelchdB != m_settings.m_squelchdB) || force)
    {
        m_squelch = CalcDb::powerFromdB(settings.m_squelchdB);
        m_agc.setThreshold(m_squelch*(1<<23));
    }

    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force)
    {
        m_udpBuffer->setAddress(const_cast<QString&>(settings.m_udpAddress));
        m_udpBufferMono->setAddress(const_cast<QString&>(settings.m_udpAddress));
    }

    if ((settings.m_udpPort != m_settings.m_udpPort) || force)
    {
        m_udpBuffer->setPort(settings.m_udpPort);
        m_udpBufferMono->setPort(settings.m_udpPort);
    }

    if ((settings.m_audioPort != m_settings.m_audioPort) || force)
    {
        disconnect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()));
        delete m_audioSocket;
        m_audioSocket = new QUdpSocket(this);

        if (m_audioSocket->bind(QHostAddress::LocalHost, settings.m_audioPort))
        {
            connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()), Qt::QueuedConnection);
            qDebug("UDPSrc::handleMessage: audio socket bound to port %d", settings.m_audioPort);
        }
        else
        {
            qWarning("UDPSrc::handleMessage: cannot bind audio socket");
        }
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling((float) settings.m_outputSampleRate / (2.0f * settings.m_fmDeviation));
    }

    m_settingsMutex.unlock();

    m_settings = settings;
}
