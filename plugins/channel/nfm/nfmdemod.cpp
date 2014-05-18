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
#include <stdio.h>
#include "nfmdemod.h"
#include "audio/audiooutput.h"
#include "dsp/dspcommands.h"

MESSAGE_CLASS_DEFINITION(NFMDemod::MsgConfigureNFMDemod, Message)

NFMDemod::NFMDemod(AudioFifo* audioFifo, SampleSink* sampleSink) :
	m_sampleSink(sampleSink),
	m_audioFifo(audioFifo)
{
	m_rfBandwidth = 12500;
	m_volume = 2.0;
	m_squelchLevel = pow(10.0, -40.0 / 20.0);
	m_sampleRate = 500000;
	m_frequency = 0;
	m_squelchLevel *= m_squelchLevel;

	m_nco.setFreq(m_frequency, m_sampleRate);
	m_interpolator.create(16, m_sampleRate, 12500);
	m_sampleDistanceRemain = (Real)m_sampleRate / 44100.0;

	m_lowpass.create(21, 44100, 3000);

	m_audioBuffer.resize(256);
	m_audioBufferFill = 0;

	m_movingAverage.resize(16, 0);
}

NFMDemod::~NFMDemod()
{
}

void NFMDemod::configure(MessageQueue* messageQueue, Real rfBandwidth, Real afBandwidth, Real volume, Real squelch)
{
	Message* cmd = MsgConfigureNFMDemod::create(rfBandwidth, afBandwidth, volume, squelch);
	cmd->submit(messageQueue, this);
}

void NFMDemod::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst)
{
	Complex ci;
	bool consumed;

	for(SampleVector::const_iterator it = begin; it < end; ++it) {
		Complex c(it->real() / 32768.0, it->imag() / 32768.0);
		c *= m_nco.nextIQ();

		consumed = false;
		if(m_interpolator.interpolate(&m_sampleDistanceRemain, c, &consumed, &ci)) {
			m_sampleBuffer.push_back(Sample(ci.real() * 32768.0, ci.imag() * 32768.0));

			m_movingAverage.feed(ci.real() * ci.real() + ci.imag() * ci.imag());

			if(m_movingAverage.average() >= m_squelchLevel)
				m_squelchState = m_sampleRate / 50;

			if(m_squelchState > 0) {
				m_squelchState--;
				Complex d = ci * conj(m_lastSample);
				m_lastSample = ci;
				Real demod = atan2(d.imag(), d.real()) / M_PI;
				demod = m_lowpass.filter(demod);
				demod *= m_volume;
				qint16 sample = demod * 32767;

				m_audioBuffer[m_audioBufferFill].l = sample;
				m_audioBuffer[m_audioBufferFill].r = sample;
				++m_audioBufferFill;
				if(m_audioBufferFill >= m_audioBuffer.size()) {
					uint res = m_audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);
					/*
					if(res != m_audioBufferFill)
						qDebug("lost %u samples", m_audioBufferFill - res);
					*/
					m_audioBufferFill = 0;
				}
			}

			m_sampleDistanceRemain += (Real)m_sampleRate / 44100.0;
		}
	}
	if(m_audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 0) != m_audioBufferFill)
		;//qDebug("lost samples");
	m_audioBufferFill = 0;

	if(m_sampleSink != NULL)
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), firstOfBurst);
	m_sampleBuffer.clear();
}

void NFMDemod::start()
{
	m_squelchState = 0;
}

void NFMDemod::stop()
{
}

bool NFMDemod::handleMessage(Message* cmd)
{
	if(DSPSignalNotification::match(cmd)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)cmd;
		qDebug("%d samples/sec, %lld Hz offset", signal->getSampleRate(), signal->getFrequencyOffset());
		m_sampleRate = signal->getSampleRate();
		m_nco.setFreq(-signal->getFrequencyOffset(), m_sampleRate);
		m_interpolator.create(16, m_sampleRate, m_rfBandwidth / 2.1);
		m_sampleDistanceRemain = m_sampleRate / 44100.0;
		m_squelchState = 0;
		cmd->completed();
		return true;
	} else if(MsgConfigureNFMDemod::match(cmd)) {
		MsgConfigureNFMDemod* cfg = (MsgConfigureNFMDemod*)cmd;
		m_rfBandwidth = cfg->getRFBandwidth();
		m_interpolator.create(16, m_sampleRate, m_rfBandwidth / 2.1);
		m_lowpass.create(21, 44100, cfg->getAFBandwidth());
		m_squelchLevel = pow(10.0, cfg->getSquelch() / 20.0);
		m_squelchLevel *= m_squelchLevel;
		m_volume = cfg->getVolume();
		cmd->completed();
		return true;
	} else {
		if(m_sampleSink != NULL)
		   return m_sampleSink->handleMessage(cmd);
		else return false;
	}
}
