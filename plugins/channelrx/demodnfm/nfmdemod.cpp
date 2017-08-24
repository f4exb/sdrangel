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

#include "nfmdemod.h"

#include <QTime>
#include <QDebug>
#include <stdio.h>
#include <complex.h>
#include <dsp/downchannelizer.h>
#include "util/stepfunctions.h"
#include "audio/audiooutput.h"
#include "dsp/pidcontroller.h"
#include "dsp/dspengine.h"
#include "nfmdemodgui.h"

static const double afSqTones[2] = {1000.0, 6000.0}; // {1200.0, 8000.0};

MESSAGE_CLASS_DEFINITION(NFMDemod::MsgConfigureNFMDemod, Message)

NFMDemod::NFMDemod() :
	m_ctcssIndex(0),
	m_sampleCount(0),
	m_squelchCount(0),
	m_squelchGate(2400),
	m_audioMute(false),
	m_squelchOpen(false),
	m_afSquelchOpen(false),
	m_magsq(0.0f),
    m_magsqSum(0.0f),
    m_magsqPeak(0.0f),
    m_magsqCount(0),
    m_movingAverage(40, 0),
    m_afSquelch(2, afSqTones),
    m_fmExcursion(2400),
    m_audioFifo(48000),
    m_settingsMutex(QMutex::Recursive)
{
	setObjectName("NFMDemod");

	m_config.m_inputSampleRate = 96000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_rfBandwidth = 12500;
	m_config.m_afBandwidth = 3000;
	m_config.m_fmDeviation = 2000;
	m_config.m_squelchGate = 5; // 10s of ms at 48000 Hz sample rate. Corresponds to 2400 for AGC attack
	m_config.m_deltaSquelch = false;
	m_config.m_squelch = -30.0;
	m_config.m_volume = 1.0;
	m_config.m_ctcssOn = false;
	m_config.m_audioMute = false;
	m_config.m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();

	apply();

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

	m_agcLevel = 1.0;
	m_movingAverage.resize(32, 0);

	m_ctcssDetector.setCoefficients(3000, 6000.0); // 0.5s / 2 Hz resolution
	m_afSquelch.setCoefficients(24, 600, 48000.0, 200, 0); // 0.5ms test period, 300ms average span, 48kS/s SR, 100ms attack, no decay

	DSPEngine::instance()->addAudioSink(&m_audioFifo);
}

NFMDemod::~NFMDemod()
{
	DSPEngine::instance()->removeAudioSink(&m_audioFifo);
}

void NFMDemod::configure(MessageQueue* messageQueue,
		Real rfBandwidth,
		Real afBandwidth,
		int  fmDeviation,
		Real volume,
		int  squelchGate,
		bool deltaSquelch,
		Real squelch,
		bool ctcssOn,
		bool audioMute,
		bool force)
{
	Message* cmd = MsgConfigureNFMDemod::create(rfBandwidth,
			afBandwidth,
			fmDeviation,
			volume,
			squelchGate,
			deltaSquelch,
			squelch,
			ctcssOn,
			audioMute,
			force);
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

void NFMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
{
	Complex ci;

	m_settingsMutex.lock();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		//Complex c(it->real() / 32768.0f, it->imag() / 32768.0f);
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		{
			if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
			{

				qint16 sample;

				//m_AGC.feed(ci);

                //double magsqRaw = m_AGC.getMagSq();
				double magsqRaw; // = ci.real()*ci.real() + c.imag()*c.imag();
				Real deviation;

				Real demod = m_phaseDiscri.phaseDiscriminatorDelta(ci, magsqRaw, deviation);

                Real magsq = magsqRaw / (1<<30);
                m_movingAverage.feed(magsq);
                m_magsqSum += magsq;

                if (magsq > m_magsqPeak)
                {
                    m_magsqPeak = magsq;
                }

                m_magsqCount++;

				//m_m2Sample = m_m1Sample;
				//m_m1Sample = ci;
				m_sampleCount++;

				// AF processing

				if (m_running.m_deltaSquelch)
				{
	                if (m_afSquelch.analyze(demod)) {
	                    m_afSquelchOpen = m_afSquelch.evaluate() ? m_squelchGate + 480 : 0;
	                }

                    if (m_afSquelchOpen)
                    {
                        if (m_squelchCount < m_squelchGate + 480)
                        {
                            m_squelchCount++;
                        }
                    }
                    else
                    {
                        if (m_squelchCount > 0)
                        {
                            m_squelchCount--;
                        }
                    }
				}
				else
				{
				    if (m_movingAverage.average() < m_squelchLevel)
				    {
                        if (m_squelchCount > 0)
                        {
                            m_squelchCount--;
                        }
				    }
				    else
				    {
                        if (m_squelchCount < m_squelchGate + 480)
                        {
                            m_squelchCount++;
                        }
				    }
				}

//				if ( (m_running.m_deltaSquelch && ((deviation > m_squelchLevel) || (deviation < -m_squelchLevel))) ||
//				     (!m_running.m_deltaSquelch && (m_movingAverage.average() < m_squelchLevel)) )
//				{
//                    if (m_squelchCount > 0)
//                    {
//                        m_squelchCount--;
//                    }
//				}
//				else
//				{
//                    if (m_squelchCount < m_squelchGate + 480)
//                    {
//                        m_squelchCount++;
//                    }
//				}

				//squelchOpen = (getMag() > m_squelchLevel);
				m_squelchOpen = (m_squelchCount > m_squelchGate);

				/*
				if (m_afSquelch.analyze(demod))
				{
					squelchOpen = m_afSquelch.evaluate();
				}*/

				if ((m_squelchOpen) && !m_running.m_audioMute)
				//if (m_AGC.getAverage() > m_squelchLevel)
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
                        Real squelchFactor = StepFunctions::smootherstep((Real) (m_squelchCount - m_squelchGate) / 480.0f);
                        sample = demod * m_running.m_volume * squelchFactor;
					}
				}
				else
				{
					if (m_ctcssIndex != 0)
					{
						m_nfmDemodGUI->setCtcssFreq(0);
						m_ctcssIndex = 0;
					}

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
    qDebug() << "NFMDemod::start";
	m_audioFifo.clear();
	m_phaseDiscri.reset();
	apply(true);
}

void NFMDemod::stop()
{
}

bool NFMDemod::handleMessage(const Message& cmd)
{
	qDebug() << "NFMDemod::handleMessage";

	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

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
		m_config.m_fmDeviation = cfg.getFMDeviation();
		m_config.m_volume = cfg.getVolume();
		m_config.m_squelchGate = cfg.getSquelchGate();
		m_config.m_deltaSquelch = cfg.getDeltaSquelch();
		m_config.m_squelch = cfg.getSquelch();
		m_config.m_ctcssOn = cfg.getCtcssOn();
		m_config.m_audioMute = cfg.getAudioMute();

		apply(cfg.getForce());

		qDebug() << "NFMDemod::handleMessage: MsgConfigureNFMDemod: m_rfBandwidth: " << m_config.m_rfBandwidth
				<< " m_afBandwidth: " << m_config.m_afBandwidth
				<< " m_fmDeviation: " << m_config.m_fmDeviation
				<< " m_volume: " << m_config.m_volume
				<< " m_squelchGate: " << m_config.m_squelchGate
				<< " m_deltaSquelch: " << m_config.m_deltaSquelch
				<< " m_squelch: " << m_squelchLevel
				<< " m_ctcssOn: " << m_config.m_ctcssOn
				<< " m_audioMute: " << m_config.m_audioMute
				<< " force: " << cfg.getForce();

		return true;
	}
	else
	{
		return false;
	}
}

void NFMDemod::apply(bool force)
{
	if ((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
		(m_config.m_inputSampleRate != m_running.m_inputSampleRate) || force)
	{
		m_nco.setFreq(-m_config.m_inputFrequencyOffset, m_config.m_inputSampleRate);
	}

	if ((m_config.m_inputSampleRate != m_running.m_inputSampleRate) ||
		(m_config.m_rfBandwidth != m_running.m_rfBandwidth) || force)
	{
		m_settingsMutex.lock();
		m_interpolator.create(16, m_config.m_inputSampleRate, m_config.m_rfBandwidth / 2.2);
		m_interpolatorDistanceRemain = 0;
		m_interpolatorDistance =  (Real) m_config.m_inputSampleRate / (Real) m_config.m_audioSampleRate;
		m_phaseDiscri.setFMScaling((8.0f*m_config.m_rfBandwidth) / (float) m_config.m_fmDeviation); // integrate 4x factor
		m_settingsMutex.unlock();
	}

	if ((m_config.m_fmDeviation != m_running.m_fmDeviation) || force)
	{
		m_phaseDiscri.setFMScaling((8.0f*m_config.m_rfBandwidth) / (float) m_config.m_fmDeviation); // integrate 4x factor
	}

	if ((m_config.m_afBandwidth != m_running.m_afBandwidth) ||
		(m_config.m_audioSampleRate != m_running.m_audioSampleRate) || force)
	{
		m_settingsMutex.lock();
		m_lowpass.create(301, m_config.m_audioSampleRate, 250.0);
		m_bandpass.create(301, m_config.m_audioSampleRate, 300.0, m_config.m_afBandwidth);
		m_settingsMutex.unlock();
	}

	if ((m_config.m_squelchGate != m_running.m_squelchGate) || force)
	{
		m_squelchGate = 480 * m_config.m_squelchGate; // gate is given in 10s of ms at 48000 Hz audio sample rate
		m_squelchCount = 0; // reset squelch open counter
	}

	if ((m_config.m_squelch != m_running.m_squelch) ||
	    (m_config.m_deltaSquelch != m_running.m_deltaSquelch) || force)
	{
	    if (m_config.m_deltaSquelch)
	    { // input is a value in negative millis
	        m_squelchLevel = (- m_config.m_squelch) / 1000.0;
	        m_afSquelch.setThreshold(m_squelchLevel);
	        m_afSquelch.reset();
	    }
	    else
	    { // input is a value in centi-Bels
	        m_squelchLevel = std::pow(10.0, m_config.m_squelch / 100.0);
	    }
		//m_squelchLevel *= m_squelchLevel;
        //m_afSquelch.setThreshold(m_squelchLevel);
	}

	m_running.m_inputSampleRate = m_config.m_inputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_rfBandwidth = m_config.m_rfBandwidth;
	m_running.m_afBandwidth = m_config.m_afBandwidth;
	m_running.m_fmDeviation = m_config.m_fmDeviation;
	m_running.m_squelchGate = m_config.m_squelchGate;
	m_running.m_deltaSquelch = m_config.m_deltaSquelch;
    m_running.m_squelch = m_config.m_squelch;
	m_running.m_volume = m_config.m_volume;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;
	m_running.m_ctcssOn = m_config.m_ctcssOn;
	m_running.m_audioMute = m_config.m_audioMute;
}
