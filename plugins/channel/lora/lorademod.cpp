///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// (c) 2015 John Greb
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
#include "lorademod.h"
#include "dsp/dspcommands.h"

#include "lorabits.h"

MESSAGE_CLASS_DEFINITION(LoRaDemod::MsgConfigureLoRaDemod, Message)

LoRaDemod::LoRaDemod(SampleSink* sampleSink) :
	m_sampleSink(sampleSink)
{
	m_Bandwidth = 7813;
	m_sampleRate = 96000;
	m_frequency = 0;
	m_nco.setFreq(m_frequency, m_sampleRate);
	m_interpolator.create(16, m_sampleRate, m_Bandwidth/1.9);
	m_sampleDistanceRemain = (Real)m_sampleRate / m_Bandwidth;

	m_chirp = 0;
	m_angle = 0;
	m_bin = 0;
	m_result = 0;
	m_count = 0;
	m_header = 0;
	m_time = 0;

	loraFilter = new sfft(LORA_SFFT_LEN);
	negaFilter = new sfft(LORA_SFFT_LEN);

	mov = new float[4*LORA_SFFT_LEN];
	history = new short[256];
}

LoRaDemod::~LoRaDemod()
{
	if (loraFilter)
		delete loraFilter;
	if (negaFilter)
		delete negaFilter;
	if (mov)
		delete [] mov;
	if (history)
		delete [] history;
}

void LoRaDemod::configure(MessageQueue* messageQueue, Real Bandwidth)
{
	Message* cmd = MsgConfigureLoRaDemod::create(Bandwidth);
	cmd->submit(messageQueue, this);
}

short LoRaDemod::synch(short bin)
{
	if (bin < 0) {
		m_time = 0;
		return 0;
	}
	history[m_time] = bin;
	if (m_time > 12)
		if (history[m_time] == history[m_time - 6])
			if (history[m_time] == history[m_time - 12]) {
				m_time = 0;
				m_tune = bin;
				return 0;
			}

	m_time++;
	m_time &= 255;
	if (m_time < 4)
		return 0;
	return (LORA_SFFT_LEN + bin - m_tune) & (LORA_SFFT_LEN - 1);
}

int LoRaDemod::detect(Complex c, Complex a)
{
	short i, result, negresult, movpoint;
	float peak, negpeak, tfloat;
	float mag[LORA_SFFT_LEN];
	float rev[LORA_SFFT_LEN];

	loraFilter->run(c * a);
	negaFilter->run(c * conj(a));

	// process spectrum twice in FFTLEN
	if (++m_count & ((1 << DATA_BITS) - 1))
		return m_result;
	movpoint = 3 & (m_count >> DATA_BITS);

	loraFilter->fetch(mag);
	negaFilter->fetch(rev);
	peak = negpeak = 0.0f;
	result = negresult = 0;
	for (i = 0; i < LORA_SFFT_LEN; i++) {
		if (rev[i] > negpeak) {
			negpeak = rev[i];
			negresult = i;
		}
		tfloat = mov[i] + mov[LORA_SFFT_LEN + i] +mov[2 * LORA_SFFT_LEN + i]
				+ mov[3 * LORA_SFFT_LEN + i] + mag[i];
		if (tfloat > peak) {
			peak = tfloat;
			result = i;
		}
		mov[movpoint * LORA_SFFT_LEN + i] = mag[i];
	}
	if (peak > negpeak * 4) {
		m_result = synch(result);
	} else {
		synch(-1);
	}
	return m_result;
}

void LoRaDemod::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool pO)
{
	int newangle;
	Complex ci;

	m_sampleBuffer.clear();
	for(SampleVector::const_iterator it = begin; it < end; ++it) {
		Complex c(it->real() / 32768.0, it->imag() / 32768.0);
		c *= m_nco.nextIQ();

		if(m_interpolator.interpolate(&m_sampleDistanceRemain, c, &ci)) {
			m_chirp = (m_chirp + 1) & (SPREADFACTOR - 1);
                        m_angle = (m_angle + m_chirp) & (SPREADFACTOR - 1);
			Complex cangle(cos(M_PI*2*m_angle/SPREADFACTOR),-sin(M_PI*2*m_angle/SPREADFACTOR));
			newangle = detect(ci, cangle);

			m_bin = (m_bin + newangle) & (LORA_SFFT_LEN - 1);
			Complex nangle(cos(M_PI*2*m_bin/LORA_SFFT_LEN),sin(M_PI*2*m_bin/LORA_SFFT_LEN));
			m_sampleBuffer.push_back(Sample(nangle.real() * 100, nangle.imag() * 100));
			m_sampleDistanceRemain += (Real)m_sampleRate / m_Bandwidth;
		}
	}
	if(m_sampleSink != NULL)
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), false);
}

void LoRaDemod::start()
{
}

void LoRaDemod::stop()
{
}

bool LoRaDemod::handleMessage(Message* cmd)
{
	if(DSPSignalNotification::match(cmd)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)cmd;
		m_sampleRate = signal->getSampleRate();
		m_nco.setFreq(-signal->getFrequencyOffset(), m_sampleRate);
		m_interpolator.create(16, m_sampleRate, m_Bandwidth/1.9);
		m_sampleDistanceRemain = m_sampleRate / m_Bandwidth;
		cmd->completed();
		return true;
	} else if(MsgConfigureLoRaDemod::match(cmd)) {
		MsgConfigureLoRaDemod* cfg = (MsgConfigureLoRaDemod*)cmd;
		m_Bandwidth = cfg->getBandwidth();
		m_interpolator.create(16, m_sampleRate, m_Bandwidth/1.9);
		cmd->completed();
		return true;
	} else {
		if(m_sampleSink != NULL)
			 return m_sampleSink->handleMessage(cmd);
		else
			return false;
	}
}
