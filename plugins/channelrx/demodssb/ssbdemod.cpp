///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// (c) 2014 Modified by John Greb
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


#include <QTime>
#include <QDebug>
#include <stdio.h>

#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include <dsp/downchannelizer.h>
#include "dsp/threadedbasebandsamplesink.h"
#include "device/devicesourceapi.h"
#include "util/db.h"

#include "ssbdemod.h"

MESSAGE_CLASS_DEFINITION(SSBDemod::MsgConfigureSSBDemod, Message)
MESSAGE_CLASS_DEFINITION(SSBDemod::MsgConfigureSSBDemodPrivate, Message)
MESSAGE_CLASS_DEFINITION(SSBDemod::MsgConfigureChannelizer, Message)

const QString SSBDemod::m_channelIdURI = "de.maintech.sdrangelove.channel.ssb";
const QString SSBDemod::m_channelId = "SSBDemod";
const int SSBDemod::m_udpBlockSize = 512;

SSBDemod::SSBDemod(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_audioBinaual(false),
        m_audioFlipChannels(false),
        m_dsb(false),
        m_audioMute(false),
        m_agc(12000, agcTarget, 1e-2),
        m_agcActive(false),
        m_agcClamping(false),
        m_agcNbSamples(12000),
        m_agcPowerThreshold(1e-2),
        m_agcThresholdGate(0),
        m_audioActive(false),
        m_sampleSink(0),
        m_audioFifo(24000),
        m_settingsMutex(QMutex::Recursive)
{
	setObjectName(m_channelId);

	m_Bandwidth = 5000;
	m_LowCutoff = 300;
	m_volume = 2.0;
	m_spanLog2 = 3;
	m_inputSampleRate = 48000;
	m_inputFrequencyOffset = 0;
	m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();

	m_audioBuffer.resize(1<<9);
	m_audioBufferFill = 0;
	m_undersampleCount = 0;
	m_sum = 0;

	m_usb = true;
	m_magsq = 0.0f;
	m_magsqSum = 0.0f;
	m_magsqPeak = 0.0f;
	m_magsqCount = 0;

	m_agc.setClampMax(SDR_RX_SCALED*SDR_RX_SCALED);
	m_agc.setClamping(m_agcClamping);

	SSBFilter = new fftfilt(m_LowCutoff / m_audioSampleRate, m_Bandwidth / m_audioSampleRate, ssbFftLen);
	DSBFilter = new fftfilt((2.0f * m_Bandwidth) / m_audioSampleRate, 2 * ssbFftLen);

	DSPEngine::instance()->addAudioSink(&m_audioFifo);
	m_udpBufferAudio = new UDPSink<qint16>(this, m_udpBlockSize, m_settings.m_udpPort);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
	applySettings(m_settings, true);
}

SSBDemod::~SSBDemod()
{
	if (SSBFilter) delete SSBFilter;
	if (DSBFilter) delete DSBFilter;
	DSPEngine::instance()->removeAudioSink(&m_audioFifo);

	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;

    delete m_udpBufferAudio;
}

void SSBDemod::configure(MessageQueue* messageQueue,
		Real Bandwidth,
		Real LowCutoff,
		Real volume,
		int spanLog2,
		bool audioBinaural,
		bool audioFlipChannel,
		bool dsb,
		bool audioMute,
		bool agc,
		bool agcClamping,
        int agcTimeLog2,
        int agcPowerThreshold,
        int agcThresholdGate)
{
	Message* cmd = MsgConfigureSSBDemodPrivate::create(
	        Bandwidth,
	        LowCutoff,
	        volume,
	        spanLog2,
	        audioBinaural,
	        audioFlipChannel,
	        dsb,
	        audioMute,
	        agc,
	        agcClamping,
	        agcTimeLog2,
	        agcPowerThreshold,
	        agcThresholdGate);
	messageQueue->push(cmd);
}

void SSBDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly __attribute__((unused)))
{
	Complex ci;
	fftfilt::cmplx *sideband;
	int n_out;

	m_settingsMutex.lock();

	int decim = 1<<(m_spanLog2 - 1);
	unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if(m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
		{
			if (m_dsb)
			{
				n_out = DSBFilter->runDSB(ci, &sideband);
			}
			else
			{
				n_out = SSBFilter->runSSB(ci, &sideband, m_usb);
			}

			m_interpolatorDistanceRemain += m_interpolatorDistance;
		}
		else
		{
			n_out = 0;
		}

		for (int i = 0; i < n_out; i++)
		{
			// Downsample by 2^(m_scaleLog2 - 1) for SSB band spectrum display
			// smart decimation with bit gain using float arithmetic (23 bits significand)

			m_sum += sideband[i];

			if (!(m_undersampleCount++ & decim_mask))
			{
				Real avgr = m_sum.real() / decim;
				Real avgi = m_sum.imag() / decim;
				m_magsq = (avgr * avgr + avgi * avgi) / (SDR_RX_SCALED*SDR_RX_SCALED);

                m_magsqSum += m_magsq;

                if (m_magsq > m_magsqPeak)
                {
                    m_magsqPeak = m_magsq;
                }

                m_magsqCount++;

				if (!m_dsb & !m_usb)
				{ // invert spectrum for LSB
					m_sampleBuffer.push_back(Sample(avgi, avgr));
				}
				else
				{
					m_sampleBuffer.push_back(Sample(avgr, avgi));
				}

                m_sum.real(0.0);
                m_sum.imag(0.0);
			}

            double agcVal = m_agcActive ? m_agc.feedAndGetValue(sideband[i]) : 10.0; // 10.0 for 3276.8, 1.0 for 327.68
            m_audioActive = agcVal != 0.0;

			if (m_audioMute)
			{
				m_audioBuffer[m_audioBufferFill].r = 0;
				m_audioBuffer[m_audioBufferFill].l = 0;

                if (m_settings.m_copyAudioToUDP) { m_udpBufferAudio->write(0); }
			}
			else
			{
				if (m_audioBinaual)
				{
					if (m_audioFlipChannels)
					{
						m_audioBuffer[m_audioBufferFill].r = (qint16)(sideband[i].imag() * m_volume * agcVal);
						m_audioBuffer[m_audioBufferFill].l = (qint16)(sideband[i].real() * m_volume * agcVal);
					}
					else
					{
						m_audioBuffer[m_audioBufferFill].r = (qint16)(sideband[i].real() * m_volume * agcVal);
						m_audioBuffer[m_audioBufferFill].l = (qint16)(sideband[i].imag() * m_volume * agcVal);
					}

                    if (m_settings.m_copyAudioToUDP) { m_udpBufferAudio->write(m_audioBuffer[m_audioBufferFill].r + m_audioBuffer[m_audioBufferFill].l); }
				}
				else
				{
					Real demod = (sideband[i].real() + sideband[i].imag()) * 0.7;
					qint16 sample = (qint16)(demod * m_volume * agcVal);
					m_audioBuffer[m_audioBufferFill].l = sample;
					m_audioBuffer[m_audioBufferFill].r = sample;

					if (m_settings.m_copyAudioToUDP) { m_udpBufferAudio->write(sample); }
				}
			}

			++m_audioBufferFill;

			if (m_audioBufferFill >= m_audioBuffer.size())
			{
				uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);

				if (res != m_audioBufferFill)
				{
					qDebug("lost %u samples", m_audioBufferFill - res);
				}

				m_audioBufferFill = 0;
			}
		}
	}

	if (m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 0) != m_audioBufferFill)
	{
		qDebug("SSBDemod::feed: lost samples");
	}
	m_audioBufferFill = 0;

	if (m_sampleSink != 0)
	{
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), !m_dsb);
	}

	m_sampleBuffer.clear();

	m_settingsMutex.unlock();
}

void SSBDemod::start()
{
    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
}

void SSBDemod::stop()
{
}

bool SSBDemod::handleMessage(const Message& cmd)
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;
		qDebug("SSBDemod::handleMessage: MsgChannelizerNotification: m_sampleRate");

		applyChannelSettings(notif.getSampleRate(), notif.getFrequencyOffset());

		return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "SSBDemod::handleMessage: MsgConfigureChannelizer: sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureSSBDemod::match(cmd))
    {
        MsgConfigureSSBDemod& cfg = (MsgConfigureSSBDemod&) cmd;
        qDebug("SSBDemod::handleMessage: MsgConfigureSSBDemod");

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
	else
	{
		if(m_sampleSink != 0)
		{
		   return m_sampleSink->handleMessage(cmd);
		}
		else
		{
			return false;
		}
	}
}

void SSBDemod::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "SSBDemod::applyChannelSettings:"
            << " inputSampleRate: " << inputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if ((m_inputFrequencyOffset != inputFrequencyOffset) ||
        (m_inputSampleRate != inputSampleRate) || force)
    {
        m_nco.setFreq(-inputFrequencyOffset, inputSampleRate);
    }

    if ((m_inputSampleRate != inputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, inputSampleRate, m_Bandwidth * 1.5f, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) inputSampleRate / (Real) m_settings.m_audioSampleRate;
        m_settingsMutex.unlock();
    }

    m_inputSampleRate = inputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void SSBDemod::applySettings(const SSBDemodSettings& settings, bool force)
{
    qDebug() << "SSBDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_lowCutoff: " << settings.m_lowCutoff
            << " m_volume: " << settings.m_volume
            << " m_spanLog2: " << settings.m_spanLog2
            << " m_audioBinaual: " << settings.m_audioBinaural
            << " m_audioFlipChannels: " << settings.m_audioFlipChannels
            << " m_dsb: " << settings.m_dsb
            << " m_audioMute: " << settings.m_audioMute
            << " m_copyAudioToUDP: " << settings.m_copyAudioToUDP
            << " m_agcActive: " << settings.m_agc
            << " m_agcClamping: " << settings.m_agcClamping
            << " m_agcTimeLog2: " << settings.m_agcTimeLog2
            << " agcPowerThreshold: " << settings.m_agcPowerThreshold
            << " agcThresholdGate: " << settings.m_agcThresholdGate;

    if((m_settings.m_rfBandwidth != settings.m_rfBandwidth) ||
        (m_settings.m_lowCutoff != settings.m_lowCutoff) ||
        (m_settings.m_audioSampleRate != settings.m_audioSampleRate) || force)
    {
        float band, lowCutoff;

        band = settings.m_rfBandwidth;
        lowCutoff = settings.m_lowCutoff;
        m_audioSampleRate = settings.m_audioSampleRate;

        if (band < 0) {
            band = -band;
            lowCutoff = -lowCutoff;
            m_usb = false;
        } else {
            m_usb = true;
        }

        if (band < 100.0f)
        {
            band = 100.0f;
            lowCutoff = 0;
        }

        m_Bandwidth = band;
        m_LowCutoff = lowCutoff;

        m_settingsMutex.lock();
        m_interpolator.create(16, m_inputSampleRate, m_Bandwidth * 1.5f, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_inputSampleRate / (Real) m_settings.m_audioSampleRate;
        SSBFilter->create_filter(m_LowCutoff / (float) m_audioSampleRate, m_Bandwidth / (float) m_audioSampleRate);
        DSBFilter->create_dsb_filter((2.0f * m_Bandwidth) / (float) m_audioSampleRate);
        m_settingsMutex.unlock();
    }

    if ((m_settings.m_volume != settings.m_volume) || force)
    {
        m_volume = settings.m_volume;
        m_volume /= 4.0; // for 3276.8
    }

    if ((m_settings.m_agcTimeLog2 != settings.m_agcTimeLog2) ||
        (m_settings.m_agcPowerThreshold != settings.m_agcPowerThreshold) ||
        (m_settings.m_agcThresholdGate != settings.m_agcThresholdGate) ||
        (m_settings.m_agcClamping != settings.m_agcClamping) || force)
    {
        int agcNbSamples = 48 * (1<<settings.m_agcTimeLog2);
        m_agc.setThresholdEnable(settings.m_agcPowerThreshold != -99);
        double agcPowerThreshold = CalcDb::powerFromdB(settings.m_agcPowerThreshold) * (SDR_RX_SCALED*SDR_RX_SCALED);
        int agcThresholdGate = 48 * settings.m_agcThresholdGate; // ms
        bool agcClamping = settings.m_agcClamping;

        if (m_agcNbSamples != agcNbSamples)
        {
            m_settingsMutex.lock();
            m_agc.resize(agcNbSamples, agcTarget);
            m_agc.setStepDownDelay(agcNbSamples);
            m_agcNbSamples = agcNbSamples;
            m_settingsMutex.unlock();
        }

        if (m_agcPowerThreshold != agcPowerThreshold)
        {
            m_agc.setThreshold(agcPowerThreshold);
            m_agcPowerThreshold = agcPowerThreshold;
        }

        if (m_agcThresholdGate != agcThresholdGate)
        {
            m_agc.setGate(agcThresholdGate);
            m_agcThresholdGate = agcThresholdGate;
        }

        if (m_agcClamping != agcClamping)
        {
            m_agc.setClamping(agcClamping);
            m_agcClamping = agcClamping;
        }

        qDebug() << "SBDemod::applySettings: AGC:"
            << " agcNbSamples: " << agcNbSamples
            << " agcPowerThreshold: " << agcPowerThreshold
            << " agcThresholdGate: " << agcThresholdGate
            << " agcClamping: " << agcClamping;
    }

    if ((m_settings.m_udpAddress != settings.m_udpAddress)
        || (m_settings.m_udpPort != settings.m_udpPort) || force)
    {
        m_udpBufferAudio->setAddress(const_cast<QString&>(settings.m_udpAddress));
        m_udpBufferAudio->setPort(settings.m_udpPort);
    }

    m_spanLog2 = settings.m_spanLog2;
    m_audioBinaual = settings.m_audioBinaural;
    m_audioFlipChannels = settings.m_audioFlipChannels;
    m_dsb = settings.m_dsb;
    m_audioMute = settings.m_audioMute;
    m_agcActive = settings.m_agc;

    m_settings = settings;
}

QByteArray SSBDemod::serialize() const
{
    return m_settings.serialize();
}

bool SSBDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSSBDemod *msg = MsgConfigureSSBDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSSBDemod *msg = MsgConfigureSSBDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}
