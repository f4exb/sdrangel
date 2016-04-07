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
#include "dsddemod.h"
#include "dsddemodgui.h"
#include "audio/audiooutput.h"
#include "dsp/channelizer.h"
#include "dsp/pidcontroller.h"
#include "dsp/dspengine.h"

static const Real afSqTones[2] = {1200.0, 6400.0}; // {1200.0, 8000.0};

MESSAGE_CLASS_DEFINITION(DSDDemod::MsgConfigureDSDDemod, Message)

DSDDemod::DSDDemod(SampleSink* sampleSink) :
	m_sampleCount(0),
	m_squelchCount(0),
	m_agcAttack(2400),
	m_audioMute(false),
	m_squelchOpen(false),
	m_afSquelch(2, afSqTones),
	m_audioFifo(4, 48000),
	m_fmExcursion(24),
	m_settingsMutex(QMutex::Recursive),
    m_scope(sampleSink),
	m_scopeEnabled(true)
{
	setObjectName("DSDDemod");

	m_config.m_inputSampleRate = 96000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_rfBandwidth = 100;
	m_config.m_demodGain = 100;
	m_config.m_fmDeviation = 100;
	m_config.m_squelchGate = 5; // 10s of ms at 48000 Hz sample rate. Corresponds to 2400 for AGC attack
	m_config.m_squelch = -30.0;
	m_config.m_volume = 1.0;
	m_config.m_audioMute = false;
	m_config.m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();

	apply();

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

	m_agcLevel = 1.0;
	m_AGC.resize(m_agcAttack, m_agcLevel);

	m_afSquelch.setCoefficients(24, 600, 48000.0, 200, 0); // 4000 Hz span, 250us, 100ms attack

	DSPEngine::instance()->addAudioSink(&m_audioFifo);
}

DSDDemod::~DSDDemod()
{
	DSPEngine::instance()->removeAudioSink(&m_audioFifo);
}

void DSDDemod::configure(MessageQueue* messageQueue,
		int  rfBandwidth,
		int  demodGain,
		int  fmDeviation,
		int  volume,
		int  squelchGate,
		Real squelch,
		bool audioMute)
{
	Message* cmd = MsgConfigureDSDDemod::create(rfBandwidth,
			demodGain,
			fmDeviation,
			volume,
			squelchGate,
			squelch,
			audioMute);
	messageQueue->push(cmd);
}

void DSDDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
	Complex ci;

	m_settingsMutex.lock();
	m_scopeSampleBuffer.clear();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		{
			if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
			{

				qint16 sample;

				m_AGC.feed(ci);

				Real demod = 32768.0f * m_phaseDiscri.phaseDiscriminator(ci) * ((float) m_running.m_demodGain / 100.0f);
				m_sampleCount++;

				// AF processing

				if (getMag() > m_squelchLevel)
				{
					if (m_squelchCount < m_agcAttack)
					{
						m_squelchCount++;
					}
				}
				else
				{
					m_squelchCount = 0;
				}

				m_squelchOpen = m_squelchCount == m_agcAttack; // wait for AGC to stabilize

				if ((m_squelchOpen) && !m_running.m_audioMute)
				{
                    sample = demod;
				}
				else
				{
					sample = 0;
				}

				Sample s(demod, 0.0);
				m_scopeSampleBuffer.push_back(s);

				m_audioBuffer[m_audioBufferFill].l = sample;
				m_audioBuffer[m_audioBufferFill].r = sample;
				++m_audioBufferFill;

				if (m_audioBufferFill >= m_audioBuffer.size())
				{
					uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 10);

					if (res != m_audioBufferFill)
					{
						qDebug("DSDDemod::feed: %u/%u audio samples written", res, m_audioBufferFill);
					}

					m_audioBufferFill = 0;
				}

				m_interpolatorDistanceRemain += m_interpolatorDistance;
			}
		}
	}

	if (m_audioBufferFill > 0)
	{
		uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 10);

		if (res != m_audioBufferFill)
		{
			qDebug("NFMDemod::feed: %u/%u tail samples written", res, m_audioBufferFill);
		}

		m_audioBufferFill = 0;
	}

    if((m_scope != 0) && (m_scopeEnabled))
    {
        m_scope->feed(m_scopeSampleBuffer.begin(), m_scopeSampleBuffer.end(), true); // true = real samples for what it's worth
    }

	m_settingsMutex.unlock();
}

void DSDDemod::start()
{
	m_audioFifo.clear();
	m_phaseDiscri.reset();
}

void DSDDemod::stop()
{
}

bool DSDDemod::handleMessage(const Message& cmd)
{
	qDebug() << "DSDDemod::handleMessage";

	if (Channelizer::MsgChannelizerNotification::match(cmd))
	{
		Channelizer::MsgChannelizerNotification& notif = (Channelizer::MsgChannelizerNotification&) cmd;

		m_config.m_inputSampleRate = notif.getSampleRate();
		m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

		apply();

		qDebug() << "DSDDemod::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << m_config.m_inputSampleRate
				<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

		return true;
	}
	else if (MsgConfigureDSDDemod::match(cmd))
	{
		MsgConfigureDSDDemod& cfg = (MsgConfigureDSDDemod&) cmd;

		m_config.m_rfBandwidth = cfg.getRFBandwidth();
		m_config.m_demodGain = cfg.getDemodGain();
		m_config.m_fmDeviation = cfg.getFMDeviation();
		m_config.m_volume = cfg.getVolume();
		m_config.m_squelchGate = cfg.getSquelchGate();
		m_config.m_squelch = cfg.getSquelch();
		m_config.m_audioMute = cfg.getAudioMute();

		apply();

		qDebug() << "DSDDemod::handleMessage: MsgConfigureDSDDemod: m_rfBandwidth: " << m_config.m_rfBandwidth * 100
				<< " m_demodGain: " << m_config.m_demodGain / 100.0
				<< " m_fmDeviation: " << m_config.m_fmDeviation * 100
				<< " m_volume: " << m_config.m_volume / 10.0
				<< " m_squelchGate" << m_config.m_squelchGate
				<< " m_squelch: " << m_config.m_squelch
				<< " m_audioMute: " << m_config.m_audioMute;

		return true;
	}
	else
	{
		return false;
	}
}

void DSDDemod::apply()
{
	if ((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
		(m_config.m_inputSampleRate != m_running.m_inputSampleRate))
	{
		m_nco.setFreq(-m_config.m_inputFrequencyOffset, m_config.m_inputSampleRate);
	}

	if ((m_config.m_inputSampleRate != m_running.m_inputSampleRate) ||
		(m_config.m_rfBandwidth != m_running.m_rfBandwidth))
	{
		m_settingsMutex.lock();
		m_interpolator.create(16, m_config.m_inputSampleRate, (m_config.m_rfBandwidth * 100) / 2.2);
		m_interpolatorDistanceRemain = 0;
		m_interpolatorDistance =  (Real) m_config.m_inputSampleRate / (Real) m_config.m_audioSampleRate;
		m_phaseDiscri.setFMScaling((float) m_config.m_rfBandwidth / (float) m_config.m_fmDeviation);
		m_settingsMutex.unlock();
	}

	if (m_config.m_fmDeviation != m_running.m_fmDeviation)
	{
		m_phaseDiscri.setFMScaling((float) m_config.m_rfBandwidth / (float) m_config.m_fmDeviation);
	}

	if (m_config.m_squelchGate != m_running.m_squelchGate)
	{
		m_agcAttack = 480 * m_config.m_squelchGate; // gate is given in 10s of ms at 48000 Hz audio sample rate
		m_AGC.resize(m_agcAttack, m_agcLevel);
	}

	if (m_config.m_squelch != m_running.m_squelch)
	{
		// input is a value in tenths of dB
		m_squelchLevel = std::pow(10.0, m_config.m_squelch / 200.0);
		//m_squelchLevel *= m_squelchLevel;
		m_afSquelch.setThreshold(m_squelchLevel);
	}

	m_running.m_inputSampleRate = m_config.m_inputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_rfBandwidth = m_config.m_rfBandwidth;
	m_running.m_demodGain = m_config.m_demodGain;
	m_running.m_fmDeviation = m_config.m_fmDeviation;
	m_running.m_squelchGate = m_config.m_squelchGate;
	m_running.m_squelch = m_config.m_squelch;
	m_running.m_volume = m_config.m_volume;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;
	m_running.m_audioMute = m_config.m_audioMute;
}
