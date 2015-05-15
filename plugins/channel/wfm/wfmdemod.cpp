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
#include <complex.h>
#include "audio/audiooutput.h"
#include "dsp/dspcommands.h"
#include "dsp/pidcontroller.h"
#include "wfmdemod.h"

//#include <iostream>

MESSAGE_CLASS_DEFINITION(WFMDemod::MsgConfigureWFMDemod, Message)

WFMDemod::WFMDemod(AudioFifo* audioFifo, SampleSink* sampleSink) :
	m_sampleSink(sampleSink),
	m_audioFifo(audioFifo)
{
	m_config.m_inputSampleRate = 384000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_rfBandwidth = 180000;
	m_config.m_afBandwidth = 15000;
	m_config.m_squelch = -60.0;
	m_config.m_volume = 2.0;
	m_config.m_audioSampleRate = 48000;
	m_rfFilter = new fftfilt(-50000.0 / 384000.0, 50000.0 / 384000.0, rfFilterFftLength);

	apply();

	m_audioBuffer.resize(16384);
	m_audioBufferFill = 0;

	m_movingAverage.resize(16, 0);
}

WFMDemod::~WFMDemod()
{
	if (m_rfFilter)
		delete m_rfFilter;
}

void WFMDemod::configure(MessageQueue* messageQueue, Real rfBandwidth, Real afBandwidth, Real volume, Real squelch)
{
	Message* cmd = MsgConfigureWFMDemod::create(rfBandwidth, afBandwidth, volume, squelch);
	cmd->submit(messageQueue, this);
}

/*
float arctan2(Real y, Real x)
{
	Real coeff_1 = M_PI / 4;
	Real coeff_2 = 3 * coeff_1;
	Real abs_y = fabs(y) + 1e-10;      // kludge to prevent 0/0 condition
	Real angle;
	if( x>= 0) {
		Real r = (x - abs_y) / (x + abs_y);
		angle = coeff_1 - coeff_1 * r;
	} else {
		Real r = (x + abs_y) / (abs_y - x);
		angle = coeff_2 - coeff_1 * r;
	}
	if(y < 0)
		return(-angle);
	else return(angle);
}

Real angleDist(Real a, Real b)
{
	Real dist = b - a;

	while(dist <= M_PI)
		dist += 2 * M_PI;
	while(dist >= M_PI)
		dist -= 2 * M_PI;

	return dist;
}
*/

void WFMDemod::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst)
{
	Complex ci;
	fftfilt::cmplx *rf;
	int rf_out;

	if (m_audioFifo->size() <= 0)
		return;

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real() / 32768.0, it->imag() / 32768.0);
		c *= m_nco.nextIQ();

		rf_out = m_rfFilter->runFilt(c, &rf); // filter RF before demod

		for (int i =0 ; i  <rf_out; i++)
		{
			Real x = rf[i].real() * m_lastSample.real() + rf[i].imag() * m_lastSample.imag();
			Real y = rf[i].real() * m_lastSample.imag() - rf[i].imag() * m_lastSample.real();
			Real demod = atan2(x,y) / M_PI;
			m_lastSample = rf[i];

			Complex e(demod, 0);

			if(m_interpolator.interpolate(&m_interpolatorDistanceRemain, e, &ci))
			{
				quint16 sample = (qint16)(ci.real() * 3000 * m_running.m_volume);
				m_sampleBuffer.push_back(Sample(sample, sample));
				m_audioBuffer[m_audioBufferFill].l = sample;
				m_audioBuffer[m_audioBufferFill].r = sample;
				++m_audioBufferFill;

				if(m_audioBufferFill >= m_audioBuffer.size())
				{
					uint res = m_audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);
					if(res != m_audioBufferFill)
						qDebug("lost %u samples", m_audioBufferFill - res);
					m_audioBufferFill = 0;
				}

				m_interpolatorDistanceRemain += m_interpolatorDistance;
			}
		}

#if 0
		{
			if(m_interpolator.interpolate(&m_interpolatorDistanceRemain, e, &ci))
			{
				m_sampleBuffer.push_back(Sample(ci.real() * 32767.0, ci.imag() * 32767.0));

				m_movingAverage.feed(ci.real() * ci.real() + ci.imag() * ci.imag());
				if(m_movingAverage.average() >= m_squelchLevel)
					m_squelchState = m_running.m_audioSampleRate/ 20;

				qint16 sample;
				if(m_squelchState > 0) {
					m_squelchState--;

					/*
					Real argument = arg(ci);
					argument /= M_PI;
					Real demod = argument - m_lastArgument;
					m_lastArgument = argument;
					*/


					//ci *= 32768.0;

					/*
					Complex d = conj(m_lastSample) * ci;
					m_lastSample = ci;
					Real demod = atan2(d.imag(), d.real());
					*/


					//m_lastSample = ci;
					//std::cerr << "demod=" << demod << std::endl;

					/*
					Real argument = atan2(ci.real()*m_lastSample.imag() - m_lastSample.real()*ci.imag(),
							ci.real()*m_lastSample.real() + ci.imag()*m_lastSample.imag());
					argument /= M_PI;
					Real demod = argument - m_lastArgument;
					m_lastArgument = argument;
					m_lastSample = ci;
					*/

					//Real demod = arctan2(d.imag(), d.real());
/*
					Real argument1 = arg(ci);//atan2(ci.imag(), ci.real());
					Real argument2 = m_lastSample.real();
					Real demod = angleDist(argument2, argument1);
					m_lastSample = Complex(argument1, 0);
*/
					//demod /= M_PI;

					demod = m_lowpass.filter(demod);

					/*
					if(demod < -1)
						demod = -1;
					else if(demod > 1)
						demod = 1;
					*/

					demod *= m_running.m_volume;
					sample = demod * 64;

				} else {
					sample = 0;
				}

				m_audioBuffer[m_audioBufferFill].l = sample;
				m_audioBuffer[m_audioBufferFill].r = sample;
				++m_audioBufferFill;
				if(m_audioBufferFill >= m_audioBuffer.size()) {
					uint res = m_audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);
					if(res != m_audioBufferFill)
						qDebug("lost %u audio samples", m_audioBufferFill - res);
					m_audioBufferFill = 0;
				}

				m_interpolatorDistanceRemain += m_interpolatorDistance;
			}
		}
#endif

	}

	if(m_audioBufferFill > 0) {
		uint res = m_audioFifo->write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);
		if(res != m_audioBufferFill)
			qDebug("lost %u samples", m_audioBufferFill - res);
		m_audioBufferFill = 0;
	}

	if(m_sampleSink != NULL)
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), false);
	m_sampleBuffer.clear();
}

void WFMDemod::start()
{
	m_squelchState = 0;
	m_audioFifo->clear();
	m_interpolatorRegulation = 0.9999;
	m_interpolatorDistance = 1.0;
	m_interpolatorDistanceRemain = 0.0;
	m_lastSample = 0;
}

void WFMDemod::stop()
{
}

bool WFMDemod::handleMessage(Message* cmd)
{
	if(DSPSignalNotification::match(cmd)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)cmd;

		m_config.m_inputSampleRate = signal->getSampleRate();
		m_config.m_inputFrequencyOffset = signal->getFrequencyOffset();
		apply();
		cmd->completed();
		return true;
	} else if(MsgConfigureWFMDemod::match(cmd)) {
		MsgConfigureWFMDemod* cfg = (MsgConfigureWFMDemod*)cmd;
		m_config.m_rfBandwidth = cfg->getRFBandwidth();
		m_config.m_afBandwidth = cfg->getAFBandwidth();
		m_config.m_volume = cfg->getVolume();
		m_config.m_squelch = cfg->getSquelch();
		apply();
		return true;
	} else {
		if(m_sampleSink != NULL)
		   return m_sampleSink->handleMessage(cmd);
		else return false;
	}
}

void WFMDemod::apply()
{

	if((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
		(m_config.m_inputSampleRate != m_running.m_inputSampleRate))
	{
		m_nco.setFreq(-m_config.m_inputFrequencyOffset, m_config.m_inputSampleRate);
	}

	if((m_config.m_inputSampleRate != m_running.m_inputSampleRate) ||
		(m_config.m_afBandwidth != m_running.m_afBandwidth))
	{
		m_interpolator.create(16, m_config.m_inputSampleRate, m_config.m_afBandwidth);
		m_interpolatorDistanceRemain = (Real) m_config.m_inputSampleRate / m_config.m_audioSampleRate;
		m_interpolatorDistance =  m_config.m_inputSampleRate / m_config.m_audioSampleRate;
	}

	if((m_config.m_inputSampleRate != m_running.m_inputSampleRate) ||
		(m_config.m_rfBandwidth != m_running.m_rfBandwidth) ||
		(m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset))
	{
		Real lowCut = (m_config.m_inputFrequencyOffset - (m_config.m_rfBandwidth / 2.0)) / m_config.m_inputSampleRate;
		Real hiCut  = (m_config.m_inputFrequencyOffset + (m_config.m_rfBandwidth / 2.0)) / m_config.m_inputSampleRate;
		m_rfFilter->create_filter(lowCut, hiCut);
	}

	if((m_config.m_afBandwidth != m_running.m_afBandwidth) ||
		(m_config.m_audioSampleRate != m_running.m_audioSampleRate))
	{
		m_lowpass.create(21, m_config.m_audioSampleRate, m_config.m_afBandwidth);
	}

	if(m_config.m_squelch != m_running.m_squelch) {
		m_squelchLevel = pow(10.0, m_config.m_squelch / 20.0);
		m_squelchLevel *= m_squelchLevel;
	}

	m_running.m_inputSampleRate = m_config.m_inputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_rfBandwidth = m_config.m_rfBandwidth;
	m_running.m_squelch = m_config.m_squelch;
	m_running.m_volume = m_config.m_volume;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;

	/*
	std::cerr << "WFMDemod::apply: in=" << m_config.m_inputSampleRate
			<< ", df=" << m_config.m_inputFrequencyOffset
			<< ", rfbw=" << m_config.m_rfBandwidth
			<< ", afbw=" << m_config.m_afBandwidth
			<< std::endl;
	*/

}
