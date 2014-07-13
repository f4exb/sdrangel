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
#include <stdio.h>
#include "usbdemod.h"
#include "audio/audiooutput.h"
#include "dsp/dspcommands.h"

MESSAGE_CLASS_DEFINITION(USBDemod::MsgConfigureUSBDemod, Message)

USBDemod::USBDemod(AudioFifo* audioFifo, SampleSink* sampleSink) :
	m_sampleSink(sampleSink),
	m_audioFifo(audioFifo)
{
	m_Bandwidth = 5000;
	m_volume = 2.0;
	m_sampleRate = 96000;
	m_frequency = 0;

	m_audioBuffer.resize(512);
	m_audioBufferFill = 0;
	m_undersampleCount = 0;

	USBFilter = new fftfilt(0.01, m_Bandwidth / 48000.0, usbFftLen);
	// if (!USBFilter) segfault;
}

USBDemod::~USBDemod()
{
	if (USBFilter) delete USBFilter;
}

void USBDemod::configure(MessageQueue* messageQueue, Real Bandwidth, Real volume)
{
	Message* cmd = MsgConfigureUSBDemod::create(Bandwidth, volume);
	cmd->submit(messageQueue, this);
}

void USBDemod::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
	Real a, b;
	Complex c;
	int n_out;
	cmplx *sideband;
	bool consumed;

	for(SampleVector::const_iterator it = begin; it < end; ++it) {
		a = it->real();
		b = it->imag();
		it++;	// TODO: Assumes 96kHz; Expect breakage.
		a += it->real();
		b += it->imag();
		c = Complex(a / 65536.0, b / 65536.0);

		n_out = USBFilter->run(c, &sideband, true);
		for (int i = 0; i < n_out; i++) {
			Real demod = (sideband[i].real() + sideband[i].imag()) * 0.7 * 32768.0;

			// Downsample by 4x for audio display
			if (!(m_undersampleCount++ & 3))
				m_sampleBuffer.push_back(Sample(demod, 0.0));

			qint16 sample = (qint16)(demod * m_volume);
			m_audioBuffer[m_audioBufferFill].l = sample;
			m_audioBuffer[m_audioBufferFill].r = sample;
			++m_audioBufferFill;
			if(m_audioBufferFill >= m_audioBuffer.size()) {
				uint res = m_audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);
				m_audioBufferFill = 0;
			}
		}
	}
	if(m_audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 0) != m_audioBufferFill)
		;//qDebug("lost samples");
	m_audioBufferFill = 0;

	if(m_sampleSink != NULL)
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
	m_sampleBuffer.clear();
}

void USBDemod::start()
{
}

void USBDemod::stop()
{
}

bool USBDemod::handleMessage(Message* cmd)
{
	if(DSPSignalNotification::match(cmd)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)cmd;
		m_sampleRate = signal->getSampleRate();
		cmd->completed();
		return true;
	} else if(MsgConfigureUSBDemod::match(cmd)) {
		MsgConfigureUSBDemod* cfg = (MsgConfigureUSBDemod*)cmd;
		m_Bandwidth = cfg->getBandwidth();
		USBFilter->create_filter(0.3 / 48.0, m_Bandwidth / 48000.0);
		m_volume = cfg->getVolume();
		m_volume *= m_volume * 0.1;
		cmd->completed();
		return true;
	} else {
		if(m_sampleSink != NULL)
		   return m_sampleSink->handleMessage(cmd);
		else return false;
	}
}
