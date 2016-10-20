///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include "ammod.h"

#include <QTime>
#include <QDebug>
#include <stdio.h>
#include <complex.h>
#include <dsp/upchannelizer.h>
#include "dsp/dspengine.h"
#include "dsp/pidcontroller.h"

MESSAGE_CLASS_DEFINITION(AMMod::MsgConfigureAMMod, Message)

AMMod::AMMod() :
	m_settingsMutex(QMutex::Recursive)
{
	setObjectName("AMMod");

	m_config.m_outputSampleRate = 48000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_rfBandwidth = 12500;
	m_config.m_afBandwidth = 3000;
	m_config.m_modPercent = 20;
	m_config.m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();

	apply();

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

	m_movingAverage.resize(16, 0);
	m_volumeAGC.resize(4096, 0.003, 0);
	m_magsq = 0.0;
}

AMMod::~AMMod()
{
}

void AMMod::configure(MessageQueue* messageQueue, Real rfBandwidth, Real afBandwidth, int modPercent, bool audioMute)
{
	Message* cmd = MsgConfigureAMMod::create(rfBandwidth, afBandwidth, modPercent, audioMute);
	messageQueue->push(cmd);
}

void AMMod::pull(Sample& sample)
{
	// TODO
}

void AMMod::start()
{
	qDebug() << "AMMod::start: m_outputSampleRate: " << m_config.m_outputSampleRate
			<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

	m_audioFifo.clear();
}

void AMMod::stop()
{
}

bool AMMod::handleMessage(const Message& cmd)
{
	qDebug() << "AMMod::handleMessage";

	if (UpChannelizer::MsgChannelizerNotification::match(cmd))
	{
		UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;

		m_config.m_outputSampleRate = notif.getSampleRate();
		m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

		apply();

		qDebug() << "AMMod::handleMessage: MsgChannelizerNotification:"
				<< " m_outputSampleRate: " << m_config.m_outputSampleRate
				<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

		return true;
	}
	else if (MsgConfigureAMMod::match(cmd))
	{
		MsgConfigureAMMod& cfg = (MsgConfigureAMMod&) cmd;

		m_config.m_rfBandwidth = cfg.getRFBandwidth();
		m_config.m_afBandwidth = cfg.getAFBandwidth();
		m_config.m_modPercent = cfg.getModPercent();
		m_config.m_audioMute = cfg.getAudioMute();

		apply();

		qDebug() << "AMMod::handleMessage: MsgConfigureAMMod:"
				<< " m_rfBandwidth: " << m_config.m_rfBandwidth
				<< " m_afBandwidth: " << m_config.m_afBandwidth
				<< " m_modPercent: " << m_config.m_modPercent
				<< " m_audioMute: " << m_config.m_audioMute;

		return true;
	}
	else
	{
		return false;
	}
}

void AMMod::apply()
{

	if((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
		(m_config.m_outputSampleRate != m_running.m_outputSampleRate))
	{
		m_carrierNco.setFreq(-m_config.m_inputFrequencyOffset, m_config.m_outputSampleRate);
	}

	if((m_config.m_outputSampleRate != m_running.m_outputSampleRate) ||
		(m_config.m_rfBandwidth != m_running.m_rfBandwidth))
	{
		m_settingsMutex.lock();
		m_interpolator.create(16, m_config.m_outputSampleRate, m_config.m_rfBandwidth / 2.2);
		m_interpolatorDistanceRemain = 0;
		m_interpolatorDistance = (Real) m_config.m_outputSampleRate / (Real) m_config.m_audioSampleRate;
		m_settingsMutex.unlock();
	}

	if((m_config.m_afBandwidth != m_running.m_afBandwidth) ||
		(m_config.m_audioSampleRate != m_running.m_audioSampleRate))
	{
		m_settingsMutex.lock();
		m_lowpass.create(21, m_config.m_audioSampleRate, m_config.m_afBandwidth);
		m_settingsMutex.unlock();
	}

	m_running.m_outputSampleRate = m_config.m_outputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_rfBandwidth = m_config.m_rfBandwidth;
	m_running.m_afBandwidth = m_config.m_afBandwidth;
	m_running.m_modPercent = m_config.m_modPercent;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;
	m_running.m_audioMute = m_config.m_audioMute;
}

