///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/basebandsamplesink.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "util/db.h"

#include "udpsinksink.h"

const Real UDPSinkSink::m_agcTarget = 16384.0f;

UDPSinkSink::UDPSinkSink() :
        m_channelSampleRate(48000),
        m_channelFrequencyOffset(0),
        m_outMovingAverage(480, 1e-10),
        m_inMovingAverage(480, 1e-10),
        m_amMovingAverage(1200, 1e-10),
        m_audioFifo(24000),
        m_spectrum(nullptr),
        m_spectrumEnabled(false),
        m_spectrumPositiveOnly(false),
        m_squelch(1e-6),
        m_squelchOpen(false),
        m_squelchOpenCount(0),
        m_squelchCloseCount(0),
        m_squelchGate(4800),
        m_squelchRelease(4800),
        m_agc(9600, m_agcTarget, 1e-6)
{
	m_udpBuffer16 = new UDPSinkUtil<Sample16>(this, udpBlockSize, m_settings.m_udpPort);
	m_udpBufferMono16 = new UDPSinkUtil<int16_t>(this, udpBlockSize, m_settings.m_udpPort);
    m_udpBuffer24 = new UDPSinkUtil<Sample24>(this, udpBlockSize, m_settings.m_udpPort);
	m_audioSocket = new QUdpSocket(this);
	m_udpAudioBuf = new char[m_udpAudioPayloadSize];

	m_audioBuffer.resize(1<<9);
	m_audioBufferFill = 0;

	m_nco.setFreq(0, m_channelSampleRate);
	m_interpolator.create(16, m_channelSampleRate, m_settings.m_rfBandwidth / 2.0);
	m_sampleDistanceRemain = m_channelSampleRate / m_settings.m_outputSampleRate;
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
		qDebug("UDPSinkSink::UDPSinkSink: bind audio socket to port %d", m_settings.m_audioPort);
		connect(m_audioSocket, SIGNAL(readyRead()), this, SLOT(audioReadyRead()), Qt::QueuedConnection);
	}
	else
	{
		qWarning("UDPSinkSink::UDPSinkSink: cannot bind audio port");
	}

    m_agc.setClampMax(SDR_RX_SCALED*SDR_RX_SCALED);
    m_agc.setClamping(true);

	//DSPEngine::instance()->addAudioSink(&m_audioFifo);

    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
    applySettings(m_settings, true);
}

UDPSinkSink::~UDPSinkSink()
{
	delete m_audioSocket;
	delete m_udpBuffer24;
    delete m_udpBuffer16;
    delete m_udpBufferMono16;
	delete[] m_udpAudioBuf;
    delete UDPFilter;
}

void UDPSinkSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	Complex ci;
	fftfilt::cmplx* sideband;
	double l, r;

	m_sampleBuffer.clear();

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

			m_sampleDistanceRemain += m_channelSampleRate / m_settings.m_outputSampleRate;

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

	if ((m_spectrum != 0) && (m_spectrumEnabled)) {
		m_spectrum->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), m_spectrumPositiveOnly);
	}
}

void UDPSinkSink::audioReadyRead()
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
						std::size_t res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], std::min(m_audioBufferFill, m_audioBuffer.size()));

						if (res != m_audioBufferFill) {
							qDebug("WFMDemodSink::feed: %lu/%lu audio samples written", res, m_audioBufferFill);
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

						if (res != m_audioBufferFill) {
							qDebug("UDPSinkSink::audioReadyRead: (mono) lost %u samples", m_audioBufferFill - res);
						}

						m_audioBufferFill = 0;
					}
				}
			}

			if (m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill) != m_audioBufferFill) {
				qDebug("UDPSinkSink::audioReadyRead: lost samples");
			}

			m_audioBufferFill = 0;
		}
	}
}

void UDPSinkSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "UDPSinkSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.0);
        m_sampleDistanceRemain = channelSampleRate / m_settings.m_outputSampleRate;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void UDPSinkSink::applySettings(const UDPSinkSettings& settings, bool force)
{
    qDebug() << "UDPSinkSink::applySettings:"
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
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    if ((settings.m_audioActive != m_settings.m_audioActive) || force)
    {
        if (settings.m_audioActive) {
            m_audioBufferFill = 0;
        }
    }

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) ||
        (settings.m_rfBandwidth != m_settings.m_rfBandwidth) ||
        (settings.m_outputSampleRate != m_settings.m_outputSampleRate) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.0);
        m_sampleDistanceRemain = m_channelSampleRate / settings.m_outputSampleRate;

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
            qDebug("UDPSinkSink::handleMessage: audio socket bound to port %d", settings.m_audioPort);
        }
        else
        {
            qWarning("UDPSinkSink::handleMessage: cannot bind audio socket");
        }
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        m_phaseDiscri.setFMScaling((float) settings.m_outputSampleRate / (2.0f * settings.m_fmDeviation));
    }

    m_settings = settings;
}
