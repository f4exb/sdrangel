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
#include "ssbdemod.h"
#include "audio/audiooutput.h"
#include "dsp/dspcommands.h"

MESSAGE_CLASS_DEFINITION(SSBDemod::MsgConfigureSSBDemod, Message)

SSBDemod::SSBDemod(AudioFifo* audioFifo, SampleSink* sampleSink) :
	m_sampleSink(sampleSink),
	m_audioFifo(audioFifo)
{
	m_Bandwidth = 5000;
	m_volume = 2.0;
	m_sampleRate = 96000;
	m_frequency = 0;
	m_nco.setFreq(m_frequency, m_sampleRate);
	m_interpolator.create(16, m_sampleRate, 5000);
	m_sampleDistanceRemain = (Real)m_sampleRate / 48000.0;

	m_audioBuffer.resize(512);
	m_audioBufferFill = 0;
	m_undersampleCount = 0;

	m_usb = true;
	SSBFilter = new fftfilt(0.01, m_Bandwidth / 48000.0, ssbFftLen);
	// if (!USBFilter) segfault;
}

SSBDemod::~SSBDemod()
{
	if (SSBFilter) delete SSBFilter;
}

void SSBDemod::configure(MessageQueue* messageQueue, Real Bandwidth, Real volume)
{
	Message* cmd = MsgConfigureSSBDemod::create(Bandwidth, volume);
	cmd->submit(messageQueue, this);
}

void SSBDemod::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
	Complex ci;
	cmplx *sideband;
	int n_out;

	for(SampleVector::const_iterator it = begin; it < end; ++it) {
		Complex c(it->real() / 32768.0, it->imag() / 32768.0);
		c *= m_nco.nextIQ();

		if(m_interpolator.interpolate(&m_sampleDistanceRemain, c, &ci)) {
			n_out = SSBFilter->runSSB(ci, &sideband, m_usb);
			m_sampleDistanceRemain += (Real)m_sampleRate / 48000.0;
		} else
			n_out = 0;

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
				if (res != m_audioBufferFill)
					qDebug("lost %u samples", m_audioBufferFill - res);
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

void SSBDemod::start()
{
}

void SSBDemod::stop()
{
}

bool SSBDemod::handleMessage(Message* cmd)
{
	float band;

	if(DSPSignalNotification::match(cmd)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)cmd;
		qDebug("%d samples/sec, %lld Hz offset", signal->getSampleRate(), signal->getFrequencyOffset());
		m_sampleRate = signal->getSampleRate();
		m_nco.setFreq(-signal->getFrequencyOffset(), m_sampleRate);
		m_interpolator.create(16, m_sampleRate, m_Bandwidth);
		m_sampleDistanceRemain = m_sampleRate / 48000.0;
		cmd->completed();
		return true;
	} else if(MsgConfigureSSBDemod::match(cmd)) {
		MsgConfigureSSBDemod* cfg = (MsgConfigureSSBDemod*)cmd;

		band = cfg->getBandwidth();
		if (band < 0) {
			band = -band;
			m_usb = false;
		} else
			m_usb = true;
		if (band < 500.0f)
			band = 500.0f;
		m_Bandwidth = band;

		m_interpolator.create(16, m_sampleRate, band * 2.0f);
		SSBFilter->create_filter(0.3f / 48.0f, m_Bandwidth / 48000.0f);

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
