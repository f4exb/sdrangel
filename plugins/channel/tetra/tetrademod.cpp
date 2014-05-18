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

#include <stdio.h>
#include "tetrademod.h"
#include "dsp/dspcommands.h"

MessageRegistrator TetraDemod::MsgConfigureTetraDemod::ID("MsgConfigureTetraDemod");

static FILE* f = NULL;

TetraDemod::TetraDemod(SampleSink* sampleSink) :
	m_sampleSink(sampleSink)
{
	m_sampleRate = 500000;
	m_frequency = 0;

	m_nco.setFreq(m_frequency, m_sampleRate);
	m_interpolator.create(32, 32 * m_sampleRate, 36000);
	m_sampleDistanceRemain = (Real)m_sampleRate / 36000.0;
}

TetraDemod::~TetraDemod()
{
}

void TetraDemod::configure(MessageQueue* messageQueue)
{
	Message* cmd = MsgConfigureTetraDemod::create();
	cmd->submit(messageQueue, this);
}

void TetraDemod::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst)
{
	size_t count = end - begin;

	Complex ci;
	bool consumed;

	for(SampleVector::const_iterator it = begin; it < end; ++it) {
		Complex c(it->real() / 32768.0, it->imag() / 32768.0);
		c *= m_nco.nextIQ();

		consumed = false;
		if(m_interpolator.interpolate(&m_sampleDistanceRemain, c, &consumed, &ci)) {
			m_sampleBuffer.push_back(Sample(ci.real() * 32768.0, ci.imag() * 32768.0));

			m_sampleDistanceRemain += (Real)m_sampleRate / 36000.0;
		}
	}

	if(f != NULL) {
		fwrite(&m_sampleBuffer[0], m_sampleBuffer.size(), sizeof(m_sampleBuffer[0]), f);
	}

	if(m_sampleSink != NULL)
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), firstOfBurst);
	m_sampleBuffer.clear();
}

void TetraDemod::start()
{
}

void TetraDemod::stop()
{
}

bool TetraDemod::handleMessage(Message* cmd)
{
	if(cmd->id() == DSPSignalNotification::ID()) {
		DSPSignalNotification* signal = (DSPSignalNotification*)cmd;
		qDebug("%d samples/sec, %lld Hz offset", signal->getSampleRate(), signal->getFrequencyOffset());
		m_sampleRate = signal->getSampleRate();
		m_nco.setFreq(-signal->getFrequencyOffset(), m_sampleRate);
		m_interpolator.create(32, m_sampleRate, 25000 / 2);
		m_sampleDistanceRemain = m_sampleRate / 36000.0;
		cmd->completed();
		return true;
	} else if(cmd->id() == MsgConfigureTetraDemod::ID()) {
		if(f == NULL) {
			f = fopen("/tmp/tetra.iq", "wb");
			qDebug("started writing samples");
		} else {
			fclose(f);
			f = NULL;
			qDebug("stopped writing samples");
		}
	} else {
		return false;
	}
}
