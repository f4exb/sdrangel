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
#include "nfmdemod.h"
#include "nfmdemodgui.h"
#include "audio/audiooutput.h"
#include "dsp/channelizer.h"
#include "dsp/pidcontroller.h"
#include "dsp/dspengine.h"

static const Real afSqTones[2] = {1200.0, 6000.0}; // {1200.0, 8000.0};

MESSAGE_CLASS_DEFINITION(NFMDemod::MsgConfigureNFMDemod, Message)

NFMDemod::NFMDemod() :
	m_ctcssIndex(0),
	m_sampleCount(0),
	m_afSquelch(2, afSqTones),
	m_squelchOpen(false),
	m_audioFifo(4, 48000),
	m_settingsMutex(QMutex::Recursive)
{
	setObjectName("NFMDemod");

	m_config.m_inputSampleRate = 96000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_rfBandwidth = 12500;
	m_config.m_afBandwidth = 3000;
	m_config.m_squelch = -30.0;
	m_config.m_volume = 2.0;
	m_config.m_ctcssOn = false;
	m_config.m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();

	apply();

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

	m_movingAverage.resize(16, 0);
	m_agcLevel = 0.003; // 0.003
	//m_AGC.resize(480, m_agcLevel, 0, 0.1*m_agcLevel);
	m_AGC.resize(240, m_agcLevel*m_agcLevel, 0.3);

	m_ctcssDetector.setCoefficients(3000, 6000.0); // 0.5s / 2 Hz resolution
	m_afSquelch.setCoefficients(24, 48000.0, 5, 1); // 4000 Hz span, 250us
	m_afSquelch.setThreshold(0.001);

	DSPEngine::instance()->addAudioSink(&m_audioFifo);
}

NFMDemod::~NFMDemod()
{
	DSPEngine::instance()->removeAudioSink(&m_audioFifo);
}

void NFMDemod::configure(MessageQueue* messageQueue,
		Real rfBandwidth,
		Real afBandwidth,
		Real volume,
		Real squelch,
		bool ctcssOn)
{
	Message* cmd = MsgConfigureNFMDemod::create(rfBandwidth,
			afBandwidth,
			volume,
			squelch,
			ctcssOn);
	messageQueue->push(cmd);
}

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

void NFMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
	Complex ci;

	m_settingsMutex.lock();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real() / 32768.0, it->imag() / 32768.0);
		c *= m_nco.nextIQ();

		{
			if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
			{

				qint16 sample;

				//m_AGC.feed(abs(ci));
				//ci *= (m_agcLevel / m_AGC.getValue());

				m_AGC.feed(ci);

				// demod
				/*
				Real argument = arg(ci);
				Real demod = argument - m_lastArgument;
				m_lastArgument = argument;
				*/

				/*
				// Original NFM
				Complex d = conj(m_m1Sample) * ci;
				Real demod = atan2(d.imag(), d.real());
				demod /= M_PI;
				*/

				/*
				Real argument1 = arg(ci);//atan2(ci.imag(), ci.real());
				Real argument2 = m_lastSample.real();
				Real demod = angleDist(argument2, argument1);
				m_lastSample = Complex(argument1, 0);
				*/

				// Alternative without atan - needs AGC
				// http://www.embedded.com/design/configurable-systems/4212086/DSP-Tricks--Frequency-demodulation-algorithms-
				Real ip = ci.real() - m_m2Sample.real();
				Real qp = ci.imag() - m_m2Sample.imag();
				Real h1 = m_m1Sample.real() * qp;
				Real h2 = m_m1Sample.imag() * ip;
				Real demod = (h1 - h2) * 10000;

				m_m2Sample = m_m1Sample;
				m_m1Sample = ci;
				m_sampleCount++;

				// AF processing

				if(m_afSquelch.analyze(&demod))
				{
					m_squelchOpen = m_afSquelch.open();
				}

				if (m_squelchOpen)
				{
					if (m_running.m_ctcssOn)
					{
						Real ctcss_sample = m_lowpass.filter(demod);

						if ((m_sampleCount & 7) == 7) // decimate 48k -> 6k
						{
							if (m_ctcssDetector.analyze(&ctcss_sample))
							{
								int maxToneIndex;

								if (m_ctcssDetector.getDetectedTone(maxToneIndex))
								{
									if (maxToneIndex+1 != m_ctcssIndex)
									{
										m_nfmDemodGUI->setCtcssFreq(m_ctcssDetector.getToneSet()[maxToneIndex]);
										m_ctcssIndex = maxToneIndex+1;
									}
								}
								else
								{
									if (m_ctcssIndex != 0)
									{
										m_nfmDemodGUI->setCtcssFreq(0);
										m_ctcssIndex = 0;
									}
								}
							}
						}
					}

					if (m_running.m_ctcssOn && m_ctcssIndexSelected && (m_ctcssIndexSelected != m_ctcssIndex))
					{
						sample = 0;
					}
					else
					{
						demod = m_bandpass.filter(demod);
						demod *= m_running.m_volume;
						sample = demod * ((1<<15)/301); // denominator = bandpass filter number of taps
					}

					m_AGC.openedSquelch();
				}
				else
				{
					if (m_ctcssIndex != 0)
					{
						m_nfmDemodGUI->setCtcssFreq(0);
						m_ctcssIndex = 0;
					}

					m_AGC.closedSquelch();
					sample = 0;
				}

				m_audioBuffer[m_audioBufferFill].l = sample;
				m_audioBuffer[m_audioBufferFill].r = sample;
				++m_audioBufferFill;

				if (m_audioBufferFill >= m_audioBuffer.size())
				{
					uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 10);

					if (res != m_audioBufferFill)
					{
						qDebug("NFMDemod::feed: %u/%u audio samples written", res, m_audioBufferFill);
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

	m_settingsMutex.unlock();
}

void NFMDemod::start()
{
	m_audioFifo.clear();
	m_m1Sample = 0;
}

void NFMDemod::stop()
{
}

bool NFMDemod::handleMessage(const Message& cmd)
{
	qDebug() << "NFMDemod::handleMessage";

	if (Channelizer::MsgChannelizerNotification::match(cmd))
	{
		Channelizer::MsgChannelizerNotification& notif = (Channelizer::MsgChannelizerNotification&) cmd;

		m_config.m_inputSampleRate = notif.getSampleRate();
		m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

		apply();

		qDebug() << "NFMDemod::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << m_config.m_inputSampleRate
				<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

		return true;
	}
	else if (MsgConfigureNFMDemod::match(cmd))
	{
		MsgConfigureNFMDemod& cfg = (MsgConfigureNFMDemod&) cmd;

		m_config.m_rfBandwidth = cfg.getRFBandwidth();
		m_config.m_afBandwidth = cfg.getAFBandwidth();
		m_config.m_volume = cfg.getVolume();
		m_config.m_squelch = cfg.getSquelch();
		m_config.m_ctcssOn = cfg.getCtcssOn();

		apply();

		qDebug() << "  - MsgConfigureNFMDemod: m_rfBandwidth: " << m_config.m_rfBandwidth
				<< " m_afBandwidth: " << m_config.m_afBandwidth
				<< " m_volume: " << m_config.m_volume
				<< " m_squelch: " << m_config.m_squelch
				<< " m_ctcssOn: " << m_config.m_ctcssOn;

		return true;
	}
	else
	{
		return false;
	}
}

void NFMDemod::apply()
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
		m_interpolator.create(16, m_config.m_inputSampleRate, m_config.m_rfBandwidth / 2.2);
		m_interpolatorDistanceRemain = 0;
		m_interpolatorDistance =  (Real) m_config.m_inputSampleRate / (Real) m_config.m_audioSampleRate;
		m_settingsMutex.unlock();
	}

	if ((m_config.m_afBandwidth != m_running.m_afBandwidth) ||
		(m_config.m_audioSampleRate != m_running.m_audioSampleRate))
	{
		m_settingsMutex.lock();
		m_lowpass.create(301, m_config.m_audioSampleRate, 250.0);
		m_bandpass.create(301, m_config.m_audioSampleRate, 300.0, m_config.m_afBandwidth);
		m_settingsMutex.unlock();
	}

	if (m_config.m_squelch != m_running.m_squelch)
	{
		m_squelchLevel = pow(10.0, m_config.m_squelch / 10.0);
		m_squelchLevel *= m_squelchLevel;
		m_afSquelch.setThreshold(m_squelchLevel);
		m_afSquelch.reset();
	}

	m_running.m_inputSampleRate = m_config.m_inputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_rfBandwidth = m_config.m_rfBandwidth;
	m_running.m_afBandwidth = m_config.m_afBandwidth;
	m_running.m_squelch = m_config.m_squelch;
	m_running.m_volume = m_config.m_volume;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;
	m_running.m_ctcssOn = m_config.m_ctcssOn;
}
