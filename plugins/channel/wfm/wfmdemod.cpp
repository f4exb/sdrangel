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
#include "wfmdemod.h"
#include "audio/audiooutput.h"
#include "dsp/dspcommands.h"

MESSAGE_CLASS_DEFINITION(WFMDemod::MsgConfigureWFMDemod, Message)

WFMDemod::WFMDemod(AudioFifo* audioFifo, SampleSink* sampleSink) :
	m_sampleSink(sampleSink),
	m_audioFifo(audioFifo)
{
	m_volume = 2.0;
	m_sampleRate = 250000;
	m_frequency = 0;

	m_nco.setFreq(m_frequency, m_sampleRate);
	m_interpolator.create(16, m_sampleRate, 16000);
	m_sampleDistanceRemain = (Real)m_sampleRate / 48000.0;

	m_audioBuffer.resize(256);
	m_audioBufferFill = 0;
}

WFMDemod::~WFMDemod()
{
}

void WFMDemod::configure(MessageQueue* messageQueue, Real afBandwidth, Real volume)
{
	Message* cmd = MsgConfigureWFMDemod::create(afBandwidth, volume);
	cmd->submit(messageQueue, this);
}

void WFMDemod::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
	Complex ci;
	bool consumed;
	qint16 sample;
	Real demod;

	for(SampleVector::const_iterator it = begin; it < end; ++it) {
		Complex c(it->real() , it->imag());
		Complex d = c * conj(m_lastSample);
		m_lastSample = c;
		demod = atan2(d.imag(), d.real()) * (1 /M_PI);
		Complex e(demod, 0);

		consumed = false;
		if(m_interpolator.interpolate(&m_sampleDistanceRemain, e, &consumed, &ci)) {
			sample = (qint16)(3000.0 * ci.real() * m_volume);
			m_sampleBuffer.push_back(Sample(sample, sample));
			m_audioBuffer[m_audioBufferFill].l = sample;
			m_audioBuffer[m_audioBufferFill].r = sample;
			++m_audioBufferFill;
			if(m_audioBufferFill >= m_audioBuffer.size()) {
				uint res = m_audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);
				if(res != m_audioBufferFill)
					qDebug("lost %u samples", m_audioBufferFill - res);
				m_audioBufferFill = 0;
			}

			m_sampleDistanceRemain += (Real)m_sampleRate / 48000.0;
		}
	}
	if(m_audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 0) != m_audioBufferFill)
		qDebug("lost samples");
	m_audioBufferFill = 0;

	if(m_sampleSink != NULL)
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
	m_sampleBuffer.clear();
}

void WFMDemod::start() {}
void WFMDemod::stop() {}

bool WFMDemod::handleMessage(Message* cmd)
{
	if(DSPSignalNotification::match(cmd)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)cmd;
		qDebug("%d samples/sec, %lld Hz offset", signal->getSampleRate(), signal->getFrequencyOffset());
		m_sampleRate = signal->getSampleRate();
		m_interpolator.create(16, m_sampleRate, m_afBandwidth);
		m_sampleDistanceRemain = m_sampleRate / 48000.0;
		cmd->completed();
		return true;
	} else if(MsgConfigureWFMDemod::match(cmd)) {
		MsgConfigureWFMDemod* cfg = (MsgConfigureWFMDemod*)cmd;
		m_afBandwidth = cfg->getAFBandwidth();
		m_interpolator.create(16, m_sampleRate, m_afBandwidth);
		m_volume = cfg->getVolume();
		cmd->completed();
		return true;
	} else {
		if(m_sampleSink != NULL)
		   return m_sampleSink->handleMessage(cmd);
		else return false;
	}
}
