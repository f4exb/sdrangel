///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/channelizer.h"
#include "dsp/pidcontroller.h"

#include "bfmdemod.h"

MESSAGE_CLASS_DEFINITION(BFMDemod::MsgConfigureBFMDemod, Message)

BFMDemod::BFMDemod(SampleSink* sampleSink) :
	m_sampleSink(sampleSink),
	m_audioFifo(4, 250000),
	m_settingsMutex(QMutex::Recursive),
	m_pilotPLL(19000/384000, 50/384000, 0.01),
	m_deemphasisFilterX(default_deemphasis * 48000 * 1.0e-6),
	m_deemphasisFilterY(default_deemphasis * 48000 * 1.0e-6),
	m_fmExcursion(default_excursion),
	m_fmScaling(384000/m_fmExcursion)
{
	setObjectName("BFMDemod");

	m_config.m_inputSampleRate = 384000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_rfBandwidth = 180000;
	m_config.m_afBandwidth = 15000;
	m_config.m_squelch = -60.0;
	m_config.m_volume = 2.0;
	m_config.m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate(); // normally 48 kHz
	m_deemphasisFilterX.configure(default_deemphasis * m_config.m_audioSampleRate * 1.0e-6);
	m_deemphasisFilterY.configure(default_deemphasis * m_config.m_audioSampleRate * 1.0e-6);
	m_rfFilter = new fftfilt(-50000.0 / 384000.0, 50000.0 / 384000.0, rfFilterFftLength);

	apply();

	m_audioBuffer.resize(16384);
	m_audioBufferFill = 0;

	m_movingAverage.resize(16, 0);

	DSPEngine::instance()->addAudioSink(&m_audioFifo);
}

BFMDemod::~BFMDemod()
{
	if (m_rfFilter)
	{
		delete m_rfFilter;
	}

	DSPEngine::instance()->removeAudioSink(&m_audioFifo);
}

void BFMDemod::configure(MessageQueue* messageQueue,
		Real rfBandwidth,
		Real afBandwidth,
		Real volume,
		Real squelch,
		bool audioStereo,
		bool showPilot)
{
	Message* cmd = MsgConfigureBFMDemod::create(rfBandwidth, afBandwidth, volume, squelch, audioStereo, showPilot);
	messageQueue->push(cmd);
}

void BFMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
	Complex ci, cs;
	fftfilt::cmplx *rf;
	int rf_out;
	Real msq, demod;

	m_sampleBuffer.clear();

	m_settingsMutex.lock();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real() / 32768.0f, it->imag() / 32768.0f);
		c *= m_nco.nextIQ();

		rf_out = m_rfFilter->runFilt(c, &rf); // filter RF before demod

		for (int i =0 ; i  <rf_out; i++)
		{
			msq = rf[i].real()*rf[i].real() + rf[i].imag()*rf[i].imag();

			m_movingAverage.feed(msq);

			if(m_movingAverage.average() >= m_squelchLevel)
				m_squelchState = m_running.m_rfBandwidth / 20; // decay rate

			if(m_squelchState > 0)
			{
				m_squelchState--;

				//demod = phaseDiscriminator2(rf[i], msq);
				demod = phaseDiscriminator(rf[i]);
			}
			else
			{
				demod = 0;
			}

			if (!m_running.m_showPilot)
			{
				m_sampleBuffer.push_back(Sample(demod * (1<<15), 0.0));
			}

			Real sampleStereo;

			// Process stereo if stereo mode is selected

			if (m_running.m_audioStereo)
			{
				m_pilotPLL.process(demod, m_pilotPLLSamples);

				if (m_running.m_showPilot)
				{
					m_sampleBuffer.push_back(Sample(m_pilotPLLSamples[1] * (1<<15), 0.0)); // debug 38 kHz pilot
				}

				Complex s(demod * 1.17 * m_pilotPLLSamples[1], 0);

				if (m_interpolatorStereo.interpolate(&m_interpolatorStereoDistanceRemain, s, &cs))
				{
					sampleStereo = cs.real();
					m_interpolatorStereoDistanceRemain += m_interpolatorStereoDistance;
				}
			}

			Complex e(demod, 0);

			if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, e, &ci))
			{
				if (m_running.m_audioStereo)
				{
					Real deemph_l, deemph_r; // Pre-emphasis is applied on each channel before multiplexing
					m_deemphasisFilterX.process(ci.real() + sampleStereo, deemph_l);
					m_deemphasisFilterY.process(ci.real() - sampleStereo, deemph_r);
					m_audioBuffer[m_audioBufferFill].l = (qint16)(deemph_l * (1<<12) * m_running.m_volume);
					m_audioBuffer[m_audioBufferFill].r = (qint16)(deemph_r * (1<<12) * m_running.m_volume);
				}
				else
				{
					Real deemph;
					m_deemphasisFilterX.process(ci.real(), deemph);
					quint16 sample = (qint16)(deemph * (1<<12) * m_running.m_volume);
					m_audioBuffer[m_audioBufferFill].l = sample;
					m_audioBuffer[m_audioBufferFill].r = sample;
				}

				++m_audioBufferFill;

				if(m_audioBufferFill >= m_audioBuffer.size())
				{
					uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);

					if(res != m_audioBufferFill)
					{
						qDebug("BFMDemod::feed: %u/%u audio samples written", res, m_audioBufferFill);
					}

					m_audioBufferFill = 0;
				}

				m_interpolatorDistanceRemain += m_interpolatorDistance;
			}
		}
	}

	if(m_audioBufferFill > 0)
	{
		uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);

		if(res != m_audioBufferFill)
		{
			qDebug("BFMDemod::feed: %u/%u tail samples written", res, m_audioBufferFill);
		}

		m_audioBufferFill = 0;
	}

	if(m_sampleSink != 0)
	{
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), true);
	}

	m_sampleBuffer.clear();

	m_settingsMutex.unlock();
}

void BFMDemod::start()
{
	m_squelchState = 0;
	m_audioFifo.clear();
	m_m1Sample = 0;
}

void BFMDemod::stop()
{
}

bool BFMDemod::handleMessage(const Message& cmd)
{
	if (Channelizer::MsgChannelizerNotification::match(cmd))
	{
		Channelizer::MsgChannelizerNotification& notif = (Channelizer::MsgChannelizerNotification&) cmd;

		m_config.m_inputSampleRate = notif.getSampleRate();
		m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

		apply();

		qDebug() << "BFMDemod::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << m_config.m_inputSampleRate
				<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

		return true;
	}
	else if (MsgConfigureBFMDemod::match(cmd))
	{
		MsgConfigureBFMDemod& cfg = (MsgConfigureBFMDemod&) cmd;

		m_config.m_rfBandwidth = cfg.getRFBandwidth();
		m_config.m_afBandwidth = cfg.getAFBandwidth();
		m_config.m_volume = cfg.getVolume();
		m_config.m_squelch = cfg.getSquelch();
		m_config.m_audioStereo = cfg.getAudioStereo();
		m_config.m_showPilot = cfg.getShowPilot();

		apply();

		qDebug() << "BFMDemod::handleMessage: MsgConfigureBFMDemod: m_rfBandwidth: " << m_config.m_rfBandwidth
				<< " m_afBandwidth: " << m_config.m_afBandwidth
				<< " m_volume: " << m_config.m_volume
				<< " m_squelch: " << m_config.m_squelch
				<< " m_audioStereo: " << m_config.m_audioStereo
				<< " m_showPilot: " << m_config.m_showPilot;

		return true;
	}
	else
	{
		qDebug() << "BFMDemod::handleMessage: none";

		if (m_sampleSink != 0)
		{
		    return m_sampleSink->handleMessage(cmd);
		}
		else
		{
			return false;
		}
	}
}

void BFMDemod::apply()
{
	if ((m_config.m_inputSampleRate != m_running.m_inputSampleRate)
		|| (m_config.m_audioStereo && (m_config.m_audioStereo != m_running.m_audioStereo)))
	{
		m_pilotPLL.configure(19000.0/m_config.m_inputSampleRate, 50.0/m_config.m_inputSampleRate, 0.01);
	}

	if((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
		(m_config.m_inputSampleRate != m_running.m_inputSampleRate))
	{
		qDebug() << "BFMDemod::handleMessage: m_nco.setFreq";
		m_nco.setFreq(-m_config.m_inputFrequencyOffset, m_config.m_inputSampleRate);
	}

	if((m_config.m_inputSampleRate != m_running.m_inputSampleRate) ||
		(m_config.m_afBandwidth != m_running.m_afBandwidth))
	{
		m_settingsMutex.lock();
		qDebug() << "BFMDemod::handleMessage: m_interpolator.create";
		m_interpolator.create(16, m_config.m_inputSampleRate, m_config.m_afBandwidth);
		m_interpolatorDistanceRemain = (Real) m_config.m_inputSampleRate / m_config.m_audioSampleRate;
		m_interpolatorDistance =  (Real) m_config.m_inputSampleRate / (Real) m_config.m_audioSampleRate;
		m_interpolatorStereo.create(16, m_config.m_inputSampleRate, m_config.m_afBandwidth);
		m_interpolatorStereoDistanceRemain = (Real) m_config.m_inputSampleRate / m_config.m_audioSampleRate;
		m_interpolatorStereoDistance =  (Real) m_config.m_inputSampleRate / (Real) m_config.m_audioSampleRate;
		m_settingsMutex.unlock();
	}

	if((m_config.m_inputSampleRate != m_running.m_inputSampleRate) ||
		(m_config.m_rfBandwidth != m_running.m_rfBandwidth) ||
		(m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset))
	{
		m_settingsMutex.lock();
		Real lowCut = -(m_config.m_rfBandwidth / 2.0) / m_config.m_inputSampleRate;
		Real hiCut  = (m_config.m_rfBandwidth / 2.0) / m_config.m_inputSampleRate;
		m_rfFilter->create_filter(lowCut, hiCut);
		m_fmScaling = m_config.m_inputSampleRate / m_fmExcursion;
		m_settingsMutex.unlock();

		qDebug() << "BFMDemod::handleMessage: m_rfFilter->create_filter: sampleRate: "
				<< m_config.m_inputSampleRate
				<< " lowCut: " << lowCut * m_config.m_inputSampleRate
				<< " hiCut: " << hiCut * m_config.m_inputSampleRate;
	}

	if((m_config.m_afBandwidth != m_running.m_afBandwidth) ||
		(m_config.m_audioSampleRate != m_running.m_audioSampleRate))
	{
		m_settingsMutex.lock();
		qDebug() << "BFMDemod::handleMessage: m_lowpass.create";
		m_lowpass.create(21, m_config.m_audioSampleRate, m_config.m_afBandwidth);
		m_settingsMutex.unlock();
	}

	if(m_config.m_squelch != m_running.m_squelch) {
		qDebug() << "BFMDemod::handleMessage: set m_squelchLevel";
		m_squelchLevel = std::pow(10.0, m_config.m_squelch / 20.0);
		m_squelchLevel *= m_squelchLevel;
	}

	if (m_config.m_audioSampleRate != m_running.m_audioSampleRate)
	{
		m_deemphasisFilterX.configure(default_deemphasis * m_config.m_audioSampleRate * 1.0e-6);
		m_deemphasisFilterY.configure(default_deemphasis * m_config.m_audioSampleRate * 1.0e-6);
	}

	m_running.m_inputSampleRate = m_config.m_inputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_rfBandwidth = m_config.m_rfBandwidth;
	m_running.m_afBandwidth = m_config.m_afBandwidth;
	m_running.m_squelch = m_config.m_squelch;
	m_running.m_volume = m_config.m_volume;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;
	m_running.m_audioStereo = m_config.m_audioStereo;
	m_running.m_showPilot = m_config.m_showPilot;

}
