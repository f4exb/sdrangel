///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGChannelSettings.h"
#include "SWGUDPSinkSettings.h"
#include "SWGChannelReport.h"
#include "SWGUDPSinkReport.h"

#include "dsp/dspengine.h"
#include "util/db.h"
#include "dsp/downchannelizer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"

#include "udpsink.h"

const Real UDPSink::m_agcTarget = 16384.0f;

MESSAGE_CLASS_DEFINITION(UDPSink::MsgConfigureUDPSource, Message)
MESSAGE_CLASS_DEFINITION(UDPSink::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(UDPSink::MsgUDPSinkSpectrum, Message)

const QString UDPSink::m_channelIdURI = "sdrangel.channel.udpsink";
const QString UDPSink::m_channelId = "UDPSink";

UDPSink::UDPSink(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_inputSampleRate(48000),
        m_inputFrequencyOffset(0),
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
	setObjectName(m_channelId);

	m_udpBuffer16 = new UDPSinkUtil<Sample16>(this, udpBlockSize, m_settings.m_udpPort);
	m_udpBufferMono16 = new UDPSinkUtil<int16_t>(this, udpBlockSize, m_settings.m_udpPort);
    m_udpBuffer24 = new UDPSinkUtil<Sample24>(this, udpBlockSize, m_settings.m_udpPort);
	m_audioSocket = new QUdpSocket(this);
	m_udpAudioBuf = new char[m_udpAudioPayloadSize];

	m_audioBuffer.resize(1<<9);
	m_audioBufferFill = 0;

	m_nco.setFreq(0, m_inputSampleRate);
	m_interpolator.create(16, m_inputSampleRate, m_settings.m_rfBandwidth / 2.0);
	m_sampleDistanceRemain = m_inputSampleRate / m_settings.m_outputSampleRate;
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
		qDebug("UDPSink::UDPSink: bind audio socket to port %d", m_settings.m_audioPort);
		connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()), Qt::QueuedConnection);
	}
	else
	{
		qWarning("UDPSink::UDPSink: cannot bind audio port");
	}

    m_agc.setClampMax(SDR_RX_SCALED*SDR_RX_SCALED);
    m_agc.setClamping(true);

	//DSPEngine::instance()->addAudioSink(&m_audioFifo);

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
    applySettings(m_settings, true);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

UDPSink::~UDPSink()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
	delete m_audioSocket;
	delete m_udpBuffer24;
    delete m_udpBuffer16;
    delete m_udpBufferMono16;
	delete[] m_udpAudioBuf;
	DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(&m_audioFifo);
	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
    delete UDPFilter;
}

void UDPSink::setSpectrum(MessageQueue* messageQueue, bool enabled)
{
	Message* cmd = MsgUDPSinkSpectrum::create(enabled);
	messageQueue->push(cmd);
}

void UDPSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
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
                (m_settings.m_sampleFormat != UDPSinkSettings::FormatNFM) &&
                (m_settings.m_sampleFormat != UDPSinkSettings::FormatNFMMono) &&
                (m_settings.m_sampleFormat != UDPSinkSettings::FormatIQ16) &&
                (m_settings.m_sampleFormat != UDPSinkSettings::FormatIQ24))
            {
                agcFactor = m_agc.feedAndGetValue(ci);
                inMagSq = m_agc.getMagSq();
            }
            else
            {
                inMagSq = ci.real()*ci.real() + ci.imag()*ci.imag();
            }

		    m_inMovingAverage.feed(inMagSq / (SDR_RX_SCALED*SDR_RX_SCALED));
		    m_inMagsq = m_inMovingAverage.average();

			Sample ss(ci.real(), ci.imag());
			m_sampleBuffer.push_back(ss);

			m_sampleDistanceRemain += m_inputSampleRate / m_settings.m_outputSampleRate;

			calculateSquelch(m_inMagsq);

			if (m_settings.m_sampleFormat == UDPSinkSettings::FormatLSB) // binaural LSB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, false);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? sideband[i].real() * m_settings.m_gain : 0;
						r = m_squelchOpen ? sideband[i].imag() * m_settings.m_gain : 0;
						udpWrite(l, r);
					    m_outMovingAverage.feed((l*l + r*r) / (SDR_RX_SCALED*SDR_RX_SCALED));
					}
				}
			}
			if (m_settings.m_sampleFormat == UDPSinkSettings::FormatUSB) // binaural USB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, true);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? sideband[i].real() * m_settings.m_gain : 0;
						r = m_squelchOpen ? sideband[i].imag() * m_settings.m_gain : 0;
                        udpWrite(l, r);
						m_outMovingAverage.feed((l*l + r*r) / (SDR_RX_SCALED*SDR_RX_SCALED));
					}
				}
			}
			else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatNFM)
			{
                Real discri = m_squelchOpen ? m_phaseDiscri.phaseDiscriminator(ci) * m_settings.m_gain : 0;
				udpWriteNorm(discri, discri);
				m_outMovingAverage.feed(discri*discri);
			}
			else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatNFMMono)
			{
			    Real discri = m_squelchOpen ? m_phaseDiscri.phaseDiscriminator(ci) * m_settings.m_gain : 0;
				udpWriteNormMono(discri);
				m_outMovingAverage.feed(discri*discri);
			}
			else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatLSBMono) // Monaural LSB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, false);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? (sideband[i].real() + sideband[i].imag()) * 0.7 * m_settings.m_gain : 0;
		                udpWriteMono(l);
						m_outMovingAverage.feed((l * l) / (SDR_RX_SCALED*SDR_RX_SCALED));
					}
				}
			}
			else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatUSBMono) // Monaural USB
			{
			    ci *= agcFactor;
				int n_out = UDPFilter->runSSB(ci, &sideband, true);

				if (n_out)
				{
					for (int i = 0; i < n_out; i++)
					{
						l = m_squelchOpen ? (sideband[i].real() + sideband[i].imag()) * 0.7 * m_settings.m_gain : 0;
                        udpWriteMono(l);
						m_outMovingAverage.feed((l * l) / (SDR_RX_SCALED*SDR_RX_SCALED));
					}
				}
			}
			else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatAMMono)
			{
			    Real amplitude = m_squelchOpen ? sqrt(inMagSq) * agcFactor * m_settings.m_gain : 0;
				FixReal demod = (FixReal) amplitude;
                udpWriteMono(demod);
				m_outMovingAverage.feed((amplitude/SDR_RX_SCALEF)*(amplitude/SDR_RX_SCALEF));
			}
            else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatAMNoDCMono)
            {
                if (m_squelchOpen)
                {
                    double demodf = sqrt(inMagSq);
                    m_amMovingAverage.feed(demodf);
                    Real amplitude = (demodf - m_amMovingAverage.average()) * agcFactor * m_settings.m_gain;
                    FixReal demod = (FixReal) amplitude;
                    udpWriteMono(demod);
                    m_outMovingAverage.feed((amplitude/SDR_RX_SCALEF)*(amplitude/SDR_RX_SCALEF));
                }
                else
                {
                    udpWriteMono(0);
                    m_outMovingAverage.feed(0);
                }
            }
            else if (m_settings.m_sampleFormat == UDPSinkSettings::FormatAMBPFMono)
            {
                if (m_squelchOpen)
                {
                    double demodf = sqrt(inMagSq);
                    demodf = m_bandpass.filter(demodf);
                    demodf /= 301.0;
                    Real amplitude = demodf * agcFactor * m_settings.m_gain;
                    FixReal demod = (FixReal) amplitude;
                    udpWriteMono(demod);
                    m_outMovingAverage.feed((amplitude/SDR_RX_SCALEF)*(amplitude/SDR_RX_SCALEF));
                }
                else
                {
                    udpWriteMono(0);
                    m_outMovingAverage.feed(0);
                }
            }
			else // Raw I/Q samples
			{
			    if (m_squelchOpen)
			    {
	                udpWrite(ci.real() * m_settings.m_gain, ci.imag() * m_settings.m_gain);
	                m_outMovingAverage.feed((inMagSq*m_settings.m_gain*m_settings.m_gain) / (SDR_RX_SCALED*SDR_RX_SCALED));
			    }
			    else
			    {
	                udpWrite(0, 0);
	                m_outMovingAverage.feed(0);
			    }
			}

            m_magsq = m_outMovingAverage.average();
		}
	}

	//qDebug() << "UDPSink::feed: " << m_sampleBuffer.size() * 4;

	if((m_spectrum != 0) && (m_spectrumEnabled))
	{
		m_spectrum->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), positiveOnly);
	}

	m_settingsMutex.unlock();
}

void UDPSink::start()
{
	m_phaseDiscri.reset();
	applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
}

void UDPSink::stop()
{
}

bool UDPSink::handleMessage(const Message& cmd)
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;
		qDebug() << "UDPSink::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << notif.getSampleRate()
                 << " frequencyOffset: " << notif.getFrequencyOffset();

		applyChannelSettings(notif.getSampleRate(), notif.getFrequencyOffset());


        return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "UDPSink::handleMessage: MsgConfigureChannelizer:"
                << " sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureUDPSource::match(cmd))
    {
        MsgConfigureUDPSource& cfg = (MsgConfigureUDPSource&) cmd;
        qDebug("UDPSink::handleMessage: MsgConfigureUDPSource");

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
	else if (MsgUDPSinkSpectrum::match(cmd))
	{
		MsgUDPSinkSpectrum& spc = (MsgUDPSinkSpectrum&) cmd;

		m_spectrumEnabled = spc.getEnabled();

		qDebug() << "UDPSink::handleMessage: MsgUDPSinkSpectrum: m_spectrumEnabled: " << m_spectrumEnabled;

		return true;
	}
    else if (DSPSignalNotification::match(cmd))
    {
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

void UDPSink::audioReadyRead()
{
	while (m_audioSocket->hasPendingDatagrams())
	{
	    qint64 pendingDataSize = m_audioSocket->pendingDatagramSize();
	    qint64 udpReadBytes = m_audioSocket->readDatagram(m_udpAudioBuf, pendingDataSize, 0, 0);
		//qDebug("UDPSink::audioReadyRead: %lld", udpReadBytes);

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
						uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

						if (res != m_audioBufferFill)
						{
							qDebug("UDPSink::audioReadyRead: (stereo) lost %u samples", m_audioBufferFill - res);
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
						uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

						if (res != m_audioBufferFill)
						{
							qDebug("UDPSink::audioReadyRead: (mono) lost %u samples", m_audioBufferFill - res);
						}

						m_audioBufferFill = 0;
					}
				}
			}

			if (m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill) != m_audioBufferFill)
			{
				qDebug("UDPSink::audioReadyRead: lost samples");
			}

			m_audioBufferFill = 0;
		}
	}

	//qDebug("UDPSink::audioReadyRead: done");
}

void UDPSink::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "UDPSink::applyChannelSettings:"
            << " inputSampleRate: " << inputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if((inputFrequencyOffset != m_inputFrequencyOffset) ||
        (inputSampleRate != m_inputSampleRate) || force)
    {
        m_nco.setFreq(-inputFrequencyOffset, inputSampleRate);
    }

    if ((inputSampleRate != m_inputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, inputSampleRate, m_settings.m_rfBandwidth / 2.0);
        m_sampleDistanceRemain = inputSampleRate / m_settings.m_outputSampleRate;
        m_settingsMutex.unlock();
    }

    m_inputSampleRate = inputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void UDPSink::applySettings(const UDPSinkSettings& settings, bool force)
{
    qDebug() << "UDPSink::applySettings:"
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
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_audioActive != m_settings.m_audioActive) || force) {
        reverseAPIKeys.append("audioActive");
    }
    if ((settings.m_audioStereo != m_settings.m_audioStereo) || force) {
        reverseAPIKeys.append("audioStereo");
    }
    if ((settings.m_gain != m_settings.m_gain) || force) {
        reverseAPIKeys.append("gain");
    }
    if ((settings.m_volume != m_settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
    }
    if ((settings.m_squelchEnabled != m_settings.m_squelchEnabled) || force) {
        reverseAPIKeys.append("squelchEnabled");
    }
    if ((settings.m_squelchdB != m_settings.m_squelchdB) || force) {
        reverseAPIKeys.append("squelchDB");
    }
    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force) {
        reverseAPIKeys.append("squelchGate");
    }
    if ((settings.m_agc != m_settings.m_agc) || force) {
        reverseAPIKeys.append("agc");
    }
    if ((settings.m_sampleFormat != m_settings.m_sampleFormat) || force) {
        reverseAPIKeys.append("sampleFormat");
    }
    if ((settings.m_outputSampleRate != m_settings.m_outputSampleRate) || force) {
        reverseAPIKeys.append("outputSampleRate");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force) {
        reverseAPIKeys.append("udpAddress");
    }
    if ((settings.m_udpPort != m_settings.m_udpPort) || force) {
        reverseAPIKeys.append("udpPort");
    }
    if ((settings.m_audioPort != m_settings.m_audioPort) || force) {
        reverseAPIKeys.append("audioPort");
    }

    m_settingsMutex.lock();

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) ||
        (settings.m_rfBandwidth != m_settings.m_rfBandwidth) ||
        (settings.m_outputSampleRate != m_settings.m_outputSampleRate) || force)
    {
        m_interpolator.create(16, m_inputSampleRate, settings.m_rfBandwidth / 2.0);
        m_sampleDistanceRemain = m_inputSampleRate / settings.m_outputSampleRate;

        if ((settings.m_sampleFormat == UDPSinkSettings::FormatLSB) ||
            (settings.m_sampleFormat == UDPSinkSettings::FormatLSBMono) ||
            (settings.m_sampleFormat == UDPSinkSettings::FormatUSB) ||
            (settings.m_sampleFormat == UDPSinkSettings::FormatUSBMono))
        {
            m_squelchGate = settings.m_outputSampleRate * 0.05;
        }
        else
        {
            m_squelchGate = (settings.m_outputSampleRate * settings.m_squelchGate) / 100;
        }

        m_squelchRelease = (settings.m_outputSampleRate * settings.m_squelchGate) / 100;
        initSquelch(m_squelchOpen);
        m_agc.resize(settings.m_outputSampleRate/5, settings.m_outputSampleRate/20, m_agcTarget); // Fixed 200 ms
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
            DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(&m_audioFifo, getInputMessageQueue());
        }
        else
        {
            DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(&m_audioFifo);
        }
    }

    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force)
    {
        if ((settings.m_sampleFormat == UDPSinkSettings::FormatLSB) ||
            (settings.m_sampleFormat == UDPSinkSettings::FormatLSBMono) ||
            (settings.m_sampleFormat == UDPSinkSettings::FormatUSB) ||
            (settings.m_sampleFormat == UDPSinkSettings::FormatUSBMono))
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
        m_udpBuffer16->setAddress(const_cast<QString&>(settings.m_udpAddress));
        m_udpBufferMono16->setAddress(const_cast<QString&>(settings.m_udpAddress));
        m_udpBuffer24->setAddress(const_cast<QString&>(settings.m_udpAddress));
    }

    if ((settings.m_udpPort != m_settings.m_udpPort) || force)
    {
        m_udpBuffer16->setPort(settings.m_udpPort);
        m_udpBufferMono16->setPort(settings.m_udpPort);
        m_udpBuffer24->setPort(settings.m_udpPort);
    }

    if ((settings.m_audioPort != m_settings.m_audioPort) || force)
    {
        disconnect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()));
        delete m_audioSocket;
        m_audioSocket = new QUdpSocket(this);

        if (m_audioSocket->bind(QHostAddress::LocalHost, settings.m_audioPort))
        {
            connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()), Qt::QueuedConnection);
            qDebug("UDPSink::handleMessage: audio socket bound to port %d", settings.m_audioPort);
        }
        else
        {
            qWarning("UDPSink::handleMessage: cannot bind audio socket");
        }
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling((float) settings.m_outputSampleRate / (2.0f * settings.m_fmDeviation));
    }

    m_settingsMutex.unlock();

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

QByteArray UDPSink::serialize() const
{
    return m_settings.serialize();
}

bool UDPSink::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureUDPSource *msg = MsgConfigureUDPSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureUDPSource *msg = MsgConfigureUDPSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int UDPSink::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setUdpSinkSettings(new SWGSDRangel::SWGUDPSinkSettings());
    response.getUdpSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int UDPSink::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    UDPSinkSettings settings = m_settings;
    bool frequencyOffsetChanged = false;

    if (channelSettingsKeys.contains("outputSampleRate")) {
        settings.m_outputSampleRate = response.getUdpSinkSettings()->getOutputSampleRate();
    }
    if (channelSettingsKeys.contains("sampleFormat")) {
        settings.m_sampleFormat = (UDPSinkSettings::SampleFormat) response.getUdpSinkSettings()->getSampleFormat();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset"))
    {
        settings.m_inputFrequencyOffset = response.getUdpSinkSettings()->getInputFrequencyOffset();
        frequencyOffsetChanged = true;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getUdpSinkSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getUdpSinkSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getUdpSinkSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getUdpSinkSettings()->getGain();
    }
    if (channelSettingsKeys.contains("squelchDB")) {
        settings.m_squelchdB = response.getUdpSinkSettings()->getSquelchDb();
    }
    if (channelSettingsKeys.contains("squelchGate")) {
        settings.m_squelchGate = response.getUdpSinkSettings()->getSquelchGate();
    }
    if (channelSettingsKeys.contains("squelchEnabled")) {
        settings.m_squelchEnabled = response.getUdpSinkSettings()->getSquelchEnabled() != 0;
    }
    if (channelSettingsKeys.contains("agc")) {
        settings.m_agc = response.getUdpSinkSettings()->getAgc() != 0;
    }
    if (channelSettingsKeys.contains("audioActive")) {
        settings.m_audioActive = response.getUdpSinkSettings()->getAudioActive() != 0;
    }
    if (channelSettingsKeys.contains("audioStereo")) {
        settings.m_audioStereo = response.getUdpSinkSettings()->getAudioStereo() != 0;
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getUdpSinkSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getUdpSinkSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getUdpSinkSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("audioPort")) {
        settings.m_audioPort = response.getUdpSinkSettings()->getAudioPort();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getUdpSinkSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getUdpSinkSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getUdpSinkSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getUdpSinkSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getUdpSinkSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getUdpSinkSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getUdpSinkSettings()->getReverseApiChannelIndex();
    }

    if (frequencyOffsetChanged)
    {
        UDPSink::MsgConfigureChannelizer *msgChan = UDPSink::MsgConfigureChannelizer::create(
                (int) settings.m_outputSampleRate,
                (int) settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(msgChan);
    }

    MsgConfigureUDPSource *msg = MsgConfigureUDPSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("getUdpSinkSettings::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureUDPSource *msgToGUI = MsgConfigureUDPSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int UDPSink::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setUdpSinkReport(new SWGSDRangel::SWGUDPSinkReport());
    response.getUdpSinkReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void UDPSink::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const UDPSinkSettings& settings)
{
    response.getUdpSinkSettings()->setOutputSampleRate(settings.m_outputSampleRate);
    response.getUdpSinkSettings()->setSampleFormat((int) settings.m_sampleFormat);
    response.getUdpSinkSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getUdpSinkSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getUdpSinkSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getUdpSinkSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getUdpSinkSettings()->setGain(settings.m_gain);
    response.getUdpSinkSettings()->setSquelchDb(settings.m_squelchdB);
    response.getUdpSinkSettings()->setSquelchGate(settings.m_squelchGate);
    response.getUdpSinkSettings()->setSquelchEnabled(settings.m_squelchEnabled ? 1 : 0);
    response.getUdpSinkSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getUdpSinkSettings()->setAudioActive(settings.m_audioActive ? 1 : 0);
    response.getUdpSinkSettings()->setAudioStereo(settings.m_audioStereo ? 1 : 0);
    response.getUdpSinkSettings()->setVolume(settings.m_volume);

    if (response.getUdpSinkSettings()->getUdpAddress()) {
        *response.getUdpSinkSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getUdpSinkSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }

    response.getUdpSinkSettings()->setUdpPort(settings.m_udpPort);
    response.getUdpSinkSettings()->setAudioPort(settings.m_audioPort);
    response.getUdpSinkSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getUdpSinkSettings()->getTitle()) {
        *response.getUdpSinkSettings()->getTitle() = settings.m_title;
    } else {
        response.getUdpSinkSettings()->setTitle(new QString(settings.m_title));
    }

    response.getUdpSinkSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getUdpSinkSettings()->getReverseApiAddress()) {
        *response.getUdpSinkSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getUdpSinkSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getUdpSinkSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getUdpSinkSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getUdpSinkSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void UDPSink::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getUdpSinkReport()->setChannelPowerDb(CalcDb::dbPower(getInMagSq()));
    response.getUdpSinkReport()->setOutputPowerDb(CalcDb::dbPower(getMagSq()));
    response.getUdpSinkReport()->setSquelch(m_squelchOpen ? 1 : 0);
    response.getUdpSinkReport()->setInputSampleRate(m_inputSampleRate);
}

void UDPSink::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const UDPSinkSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(0); // single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("UDPSink"));
    swgChannelSettings->setUdpSinkSettings(new SWGSDRangel::SWGUDPSinkSettings());
    SWGSDRangel::SWGUDPSinkSettings *swgUDPSinkSettings = swgChannelSettings->getUdpSinkSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("outputSampleRate") || force) {
        swgUDPSinkSettings->setOutputSampleRate(settings.m_outputSampleRate);
    }
    if (channelSettingsKeys.contains("sampleFormat") || force) {
        swgUDPSinkSettings->setSampleFormat((int) settings.m_sampleFormat);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgUDPSinkSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgUDPSinkSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgUDPSinkSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgUDPSinkSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgUDPSinkSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("squelchDB") || force) {
        swgUDPSinkSettings->setSquelchDb(settings.m_squelchdB);
    }
    if (channelSettingsKeys.contains("squelchGate") || force) {
        swgUDPSinkSettings->setSquelchGate(settings.m_squelchGate);
    }
    if (channelSettingsKeys.contains("squelchEnabled") || force) {
        swgUDPSinkSettings->setSquelchEnabled(settings.m_squelchEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agc") || force) {
        swgUDPSinkSettings->setAgc(settings.m_agc ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioActive") || force) {
        swgUDPSinkSettings->setAudioActive(settings.m_audioActive ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioStereo") || force) {
        swgUDPSinkSettings->setAudioStereo(settings.m_audioStereo ? 1 : 0);
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgUDPSinkSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgUDPSinkSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgUDPSinkSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("audioPort") || force) {
        swgUDPSinkSettings->setAudioPort(settings.m_audioPort);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgUDPSinkSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgUDPSinkSettings->setTitle(new QString(settings.m_title));
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

    delete swgChannelSettings;
}

void UDPSink::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "UDPSink::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("UDPSink::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
