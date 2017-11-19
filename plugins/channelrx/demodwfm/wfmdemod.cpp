///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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
#include <complex.h>

#include <dsp/downchannelizer.h>
#include "dsp/threadedbasebandsamplesink.h"
#include <device/devicesourceapi.h>
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/pidcontroller.h"

#include "wfmdemod.h"

MESSAGE_CLASS_DEFINITION(WFMDemod::MsgConfigureWFMDemod, Message)
MESSAGE_CLASS_DEFINITION(WFMDemod::MsgConfigureChannelizer, Message)

const QString WFMDemod::m_channelID = "de.maintech.sdrangelove.channel.wfm";

WFMDemod::WFMDemod(DeviceSourceAPI* deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_absoluteFrequencyOffset(0),
    m_squelchOpen(false),
    m_magsq(0.0f),
    m_magsqSum(0.0f),
    m_magsqPeak(0.0f),
    m_magsqCount(0),
    m_movingAverage(40, 0),
    m_sampleSink(0),
    m_audioFifo(250000),
    m_settingsMutex(QMutex::Recursive)

{
	setObjectName("WFMDemod");

	m_rfFilter = new fftfilt(-50000.0 / 384000.0, 50000.0 / 384000.0, rfFilterFftLength);
	m_phaseDiscri.setFMScaling(384000/75000);

	m_audioBuffer.resize(16384);
	m_audioBufferFill = 0;

	m_movingAverage.resize(16, 0);

	DSPEngine::instance()->addAudioSink(&m_audioFifo);

	m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

	applySettings(m_settings, true);
}

WFMDemod::~WFMDemod()
{
	if (m_rfFilter)
	{
		delete m_rfFilter;
	}

	DSPEngine::instance()->removeAudioSink(&m_audioFifo);

	m_deviceAPI->removeChannelAPI(this);
	m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void WFMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
{
	Complex ci;
	fftfilt::cmplx *rf;
	int rf_out;
	Real demod;
	double msq;
	float fmDev;

	m_settingsMutex.lock();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		//Complex c(it->real() / 32768.0f, it->imag() / 32768.0f);
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		rf_out = m_rfFilter->runFilt(c, &rf); // filter RF before demod

		for (int i = 0 ; i < rf_out; i++)
		{
		    demod = m_phaseDiscri.phaseDiscriminatorDelta(rf[i], msq, fmDev);
		    Real magsq = msq / (1<<30);

			m_movingAverage.feed(magsq);
            m_magsqSum += magsq;

            if (magsq > m_magsqPeak)
            {
                m_magsqPeak = magsq;
            }

            m_magsqCount++;

			if(m_movingAverage.average() >= m_squelchLevel)
				m_squelchState = m_settings.m_rfBandwidth / 20; // decay rate

			if (m_squelchState > 0)
			{
				m_squelchState--;
				m_squelchOpen = true;
			}
			else
			{
				demod = 0;
                m_squelchOpen = false;
			}

            if (m_settings.m_audioMute)
            {
                demod = 0;
            }

            Complex e(demod, 0);

			if(m_interpolator.decimate(&m_interpolatorDistanceRemain, e, &ci))
			{
				quint16 sample = (qint16)(ci.real() * 3276.8f * m_settings.m_volume);
				m_sampleBuffer.push_back(Sample(sample, sample));
				m_audioBuffer[m_audioBufferFill].l = sample;
				m_audioBuffer[m_audioBufferFill].r = sample;
				++m_audioBufferFill;

				if(m_audioBufferFill >= m_audioBuffer.size())
				{
					uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);

					if(res != m_audioBufferFill)
					{
						qDebug("WFMDemod::feed: %u/%u audio samples written", res, m_audioBufferFill);
					}

					m_audioBufferFill = 0;
				}

				m_interpolatorDistanceRemain += m_interpolatorDistance;
			}
		}
	}

	if(m_audioBufferFill > 0)
	{
		uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);

		if(res != m_audioBufferFill)
		{
			qDebug("WFMDemod::feed: %u/%u tail samples written", res, m_audioBufferFill);
		}

		m_audioBufferFill = 0;
	}

	if(m_sampleSink != NULL)
	{
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), false);
	}

	m_sampleBuffer.clear();

	m_settingsMutex.unlock();
}

void WFMDemod::start()
{
	m_squelchState = 0;
	m_audioFifo.clear();
	m_phaseDiscri.reset();
}

void WFMDemod::stop()
{
}

bool WFMDemod::handleMessage(const Message& cmd)
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

		WFMDemodSettings settings = m_settings;

        settings.m_inputSampleRate = notif.getSampleRate();
        settings.m_inputFrequencyOffset = notif.getFrequencyOffset();

        applySettings(settings);

        qDebug() << "WFMDemod::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << settings.m_inputSampleRate
                << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset;

		return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        qDebug() << "WFMDemod::handleMessage: MsgConfigureChannelizer:"
                << " sampleRate: " << cfg.getSampleRate()
                << " inputFrequencyOffset: " << cfg.getCenterFrequency();

        return true;
    }
    else if (MsgConfigureWFMDemod::match(cmd))
    {
        MsgConfigureWFMDemod& cfg = (MsgConfigureWFMDemod&) cmd;

        WFMDemodSettings settings = cfg.getSettings();

        // These settings are set with DownChannelizer::MsgChannelizerNotification
        m_absoluteFrequencyOffset = settings.m_inputFrequencyOffset;
        settings.m_inputSampleRate = m_settings.m_inputSampleRate;
        settings.m_inputFrequencyOffset = m_settings.m_inputFrequencyOffset;

        applySettings(settings, cfg.getForce());

        qDebug() << "WFMDemod::handleMessage: MsgConfigureWFMDemod:"
                << " m_rfBandwidth: " << settings.m_rfBandwidth
                << " m_afBandwidth: " << settings.m_afBandwidth
                << " m_volume: " << settings.m_volume
                << " m_squelch: " << settings.m_squelch
                << " m_copyAudioToUDP: " << settings.m_copyAudioToUDP
                << " m_udpAddress: " << settings.m_udpAddress
                << " m_udpPort: " << settings.m_udpPort
                << " force: " << cfg.getForce();

        return true;
    }
	else
	{
		if (m_sampleSink != 0)
		{
		    return m_sampleSink->handleMessage(cmd);
		}
		else
		{
			return false;
		}
	}
}

void WFMDemod::applySettings(const WFMDemodSettings& settings, bool force)
{
    if((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) ||
        (settings.m_inputSampleRate != m_settings.m_inputSampleRate) || force)
    {
        qDebug() << "WFMDemod::applySettings: m_nco.setFreq";
        m_nco.setFreq(-settings.m_inputFrequencyOffset, settings.m_inputSampleRate);
    }

    if((settings.m_inputSampleRate != m_settings.m_inputSampleRate) ||
        (settings.m_audioSampleRate != m_settings.m_audioSampleRate) ||
        (settings.m_afBandwidth != m_settings.m_afBandwidth) ||
        (settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_settingsMutex.lock();
        qDebug() << "WFMDemod::applySettings: m_interpolator.create";
        m_interpolator.create(16, settings.m_inputSampleRate, settings.m_afBandwidth);
        m_interpolatorDistanceRemain = (Real) settings.m_inputSampleRate / (Real) settings.m_audioSampleRate;
        m_interpolatorDistance =  (Real) settings.m_inputSampleRate / (Real) settings.m_audioSampleRate;
        qDebug() << "WFMDemod::applySettings: m_rfFilter->create_filter";
        Real lowCut = -(settings.m_rfBandwidth / 2.0) / settings.m_inputSampleRate;
        Real hiCut  = (settings.m_rfBandwidth / 2.0) / settings.m_inputSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_fmExcursion = settings.m_rfBandwidth / (Real) settings.m_inputSampleRate;
        m_phaseDiscri.setFMScaling(1.0f/m_fmExcursion);
        qDebug("WFMDemod::applySettings: m_fmExcursion: %f", m_fmExcursion);
        m_settingsMutex.unlock();
    }

    if ((settings.m_squelch != m_settings.m_squelch) || force)
    {
        qDebug() << "WFMDemod::applySettings: set m_squelchLevel";
        m_squelchLevel = pow(10.0, settings.m_squelch / 20.0);
        m_squelchLevel *= m_squelchLevel;
    }

    m_settings = settings;
}
