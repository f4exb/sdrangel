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

#include "../../channelrx/demodwfm/wfmdemod.h"

#include <QTime>
#include <QDebug>
#include <stdio.h>
#include <complex.h>
#include <dsp/downchannelizer.h>
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/pidcontroller.h"

MESSAGE_CLASS_DEFINITION(WFMDemod::MsgConfigureWFMDemod, Message)

WFMDemod::WFMDemod(BasebandSampleSink* sampleSink) :
	m_sampleSink(sampleSink),
	m_audioFifo(4, 250000),
	m_settingsMutex(QMutex::Recursive)
{
	setObjectName("WFMDemod");

	m_config.m_inputSampleRate = 384000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_rfBandwidth = 180000;
	m_config.m_afBandwidth = 15000;
	m_config.m_squelch = -60.0;
	m_config.m_volume = 2.0;
	m_config.m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();
	m_rfFilter = new fftfilt(-50000.0 / 384000.0, 50000.0 / 384000.0, rfFilterFftLength);
	m_phaseDiscri.setFMScaling(384000/75000);

	apply();

	m_audioBuffer.resize(16384);
	m_audioBufferFill = 0;

	m_movingAverage.resize(16, 0);

	DSPEngine::instance()->addAudioSink(&m_audioFifo);
}

WFMDemod::~WFMDemod()
{
	if (m_rfFilter)
	{
		delete m_rfFilter;
	}

	DSPEngine::instance()->removeAudioSink(&m_audioFifo);
}

void WFMDemod::configure(MessageQueue* messageQueue, Real rfBandwidth, Real afBandwidth, Real volume, Real squelch)
{
	Message* cmd = MsgConfigureWFMDemod::create(rfBandwidth, afBandwidth, volume, squelch);
	messageQueue->push(cmd);
}

void WFMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
	Complex ci;
	fftfilt::cmplx *rf;
	int rf_out;
	Real msq, demod;

	m_settingsMutex.lock();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real() / 32768.0f, it->imag() / 32768.0f);
		c *= m_nco.nextIQ();

		rf_out = m_rfFilter->runFilt(c, &rf); // filter RF before demod

		for (int i =0 ; i  <rf_out; i++)
		{
			msq = rf[i].real()*rf[i].real() + rf[i].imag()*rf[i].imag();

			m_movingAverage.feed(msq);

			if(m_movingAverage.average() >= m_squelchLevel)
				m_squelchState = m_running.m_rfBandwidth / 20; // decay rate

			if(m_squelchState > 0)
			{
				m_squelchState--;

				// Alternative without atan
				// http://www.embedded.com/design/configurable-systems/4212086/DSP-Tricks--Frequency-demodulation-algorithms-
				// in addition it needs scaling by instantaneous magnitude squared and volume (0..10) adjustment factor
				/*
				Real ip = rf[i].real() - m_m2Sample.real();
				Real qp = rf[i].imag() - m_m2Sample.imag();
				Real h1 = m_m1Sample.real() * qp;
				Real h2 = m_m1Sample.imag() * ip;
				demod = (h1 - h2) / (msq * 10.0);*/
				demod = m_phaseDiscri.phaseDiscriminator2(rf[i]);
			}
			else
			{
				demod = 0;
			}

			Complex e(demod, 0);

			if(m_interpolator.decimate(&m_interpolatorDistanceRemain, e, &ci))
			{
				quint16 sample = (qint16)(ci.real() * 3000 * m_running.m_volume);
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
	qDebug() << "WFMDemod::handleMessage";

	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

		m_config.m_inputSampleRate = notif.getSampleRate();
		m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

		apply();

		qDebug() << "WFMDemod::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << m_config.m_inputSampleRate
				<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

		return true;
	}
	else if (MsgConfigureWFMDemod::match(cmd))
	{
		MsgConfigureWFMDemod& cfg = (MsgConfigureWFMDemod&) cmd;

		m_config.m_rfBandwidth = cfg.getRFBandwidth();
		m_config.m_afBandwidth = cfg.getAFBandwidth();
		m_config.m_volume = cfg.getVolume();
		m_config.m_squelch = cfg.getSquelch();

		apply();

		qDebug() << "WFMDemod::handleMessage: MsgConfigureWFMDemod: m_rfBandwidth: " << m_config.m_rfBandwidth
				<< " m_afBandwidth: " << m_config.m_afBandwidth
				<< " m_volume: " << m_config.m_volume
				<< " m_squelch: " << m_config.m_squelch;

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

void WFMDemod::apply()
{

	if((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
		(m_config.m_inputSampleRate != m_running.m_inputSampleRate))
	{
		qDebug() << "WFMDemod::handleMessage: m_nco.setFreq";
		m_nco.setFreq(-m_config.m_inputFrequencyOffset, m_config.m_inputSampleRate);
	}

	if((m_config.m_inputSampleRate != m_running.m_inputSampleRate) ||
		(m_config.m_afBandwidth != m_running.m_afBandwidth))
	{
		m_settingsMutex.lock();
		qDebug() << "WFMDemod::handleMessage: m_interpolator.create";
		m_interpolator.create(16, m_config.m_inputSampleRate, m_config.m_afBandwidth);
		m_interpolatorDistanceRemain = (Real) m_config.m_inputSampleRate / m_config.m_audioSampleRate;
		m_interpolatorDistance =  (Real) m_config.m_inputSampleRate / (Real) m_config.m_audioSampleRate;
		m_settingsMutex.unlock();
	}

	if((m_config.m_inputSampleRate != m_running.m_inputSampleRate) ||
		(m_config.m_rfBandwidth != m_running.m_rfBandwidth) ||
		(m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset))
	{
		m_settingsMutex.lock();
		qDebug() << "WFMDemod::handleMessage: m_rfFilter->create_filter";
		Real lowCut = -(m_config.m_rfBandwidth / 2.0) / m_config.m_inputSampleRate;
		Real hiCut  = (m_config.m_rfBandwidth / 2.0) / m_config.m_inputSampleRate;
		m_rfFilter->create_filter(lowCut, hiCut);
		m_fmExcursion = m_config.m_rfBandwidth / 2.0;
		m_phaseDiscri.setFMScaling(m_config.m_inputSampleRate / m_fmExcursion);
		m_settingsMutex.unlock();
	}

	if((m_config.m_afBandwidth != m_running.m_afBandwidth) ||
		(m_config.m_audioSampleRate != m_running.m_audioSampleRate))
	{
		m_settingsMutex.lock();
		qDebug() << "WFMDemod::handleMessage: m_lowpass.create";
		m_lowpass.create(21, m_config.m_audioSampleRate, m_config.m_afBandwidth);
		m_settingsMutex.unlock();
	}

	if(m_config.m_squelch != m_running.m_squelch) {
		qDebug() << "WFMDemod::handleMessage: set m_squelchLevel";
		m_squelchLevel = pow(10.0, m_config.m_squelch / 20.0);
		m_squelchLevel *= m_squelchLevel;
	}

	m_running.m_inputSampleRate = m_config.m_inputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_rfBandwidth = m_config.m_rfBandwidth;
	m_running.m_afBandwidth = m_config.m_afBandwidth;
	m_running.m_squelch = m_config.m_squelch;
	m_running.m_volume = m_config.m_volume;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;

}
