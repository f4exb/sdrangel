///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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
#include "dsddemod.h"
#include "dsddemodgui.h"
#include "audio/audiooutput.h"
#include "dsp/channelizer.h"
#include "dsp/pidcontroller.h"
#include "dsp/dspengine.h"

static const Real afSqTones[2] = {1200.0, 6400.0}; // {1200.0, 8000.0};

MESSAGE_CLASS_DEFINITION(DSDDemod::MsgConfigureDSDDemod, Message)

DSDDemod::DSDDemod(SampleSink* sampleSink) :
	m_sampleCount(0),
	m_squelchCount(0),
	m_squelchOpen(false),
	m_audioFifo(4, 48000),
	m_fmExcursion(24),
	m_settingsMutex(QMutex::Recursive),
    m_scope(sampleSink),
	m_scopeEnabled(true),
	m_dsdDecoder()
{
	setObjectName("DSDDemod");

	m_config.m_inputSampleRate = 96000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_rfBandwidth = 100;
	m_config.m_demodGain = 100;
	m_config.m_fmDeviation = 100;
	m_config.m_squelchGate = 5; // 10s of ms at 48000 Hz sample rate. Corresponds to 2400 for AGC attack
	m_config.m_squelch = -30.0;
	m_config.m_volume = 1.0;
	m_config.m_baudRate = 4800;
	m_config.m_audioMute = false;
	m_config.m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();
	m_config.m_enableCosineFiltering = false;

	apply();

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

	m_sampleBuffer = new qint16[1<<17]; // 128 kS
	m_sampleBufferIndex = 0;

    m_movingAverage.resize(16, 0);

	DSPEngine::instance()->addAudioSink(&m_audioFifo);
}

DSDDemod::~DSDDemod()
{
    delete[] m_sampleBuffer;
	DSPEngine::instance()->removeAudioSink(&m_audioFifo);
}

void DSDDemod::configure(MessageQueue* messageQueue,
		int  rfBandwidth,
		int  demodGain,
		int  fmDeviation,
		int  volume,
		int  baudRate,
		int  squelchGate,
		Real squelch,
		bool audioMute,
		bool enableCosineFiltering,
		bool syncOrConstellation)
{
	Message* cmd = MsgConfigureDSDDemod::create(rfBandwidth,
			demodGain,
			fmDeviation,
			volume,
			baudRate,
			squelchGate,
			squelch,
			audioMute,
			enableCosineFiltering,
			syncOrConstellation);
	messageQueue->push(cmd);
}

void DSDDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
	Complex ci;
	int samplesPerSymbol = m_dsdDecoder.getSamplesPerSymbol();

	m_settingsMutex.lock();
	m_scopeSampleBuffer.clear();

	m_dsdDecoder.enableMbelib(!DSPEngine::instance()->hasDVSerialSupport()); // disable mbelib if DV serial support is present and activated else enable it

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

        if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
        {
            qint16 sample, delayedSample;

            m_magsq = ((ci.real()*ci.real() +  ci.imag()*ci.imag())) / (Real) (1<<30);
            m_movingAverage.feed(m_magsq);

            Real demod = 32768.0f * m_phaseDiscri.phaseDiscriminator(ci) * ((float) m_running.m_demodGain / 100.0f);
            m_sampleCount++;

            // AF processing

            if (getMagSq() > m_squelchLevel)
            {
                if (m_squelchGate > 0)
                {
                    if (m_squelchCount < m_squelchGate) {
                        m_squelchCount++;
                    }

                    m_squelchOpen = m_squelchCount == m_squelchGate;
                }
                else
                {
                    m_squelchOpen = true;
                }
            }
            else
            {
                m_squelchCount = 0;
                m_squelchOpen = false;
            }

            if (m_squelchOpen)
            {
                sample = demod;
            }
            else
            {
                sample = 0;
            }

            m_dsdDecoder.pushSample(sample);

            if (m_running.m_enableCosineFiltering) { // show actual input to FSK demod
            	sample = m_dsdDecoder.getFilteredSample();
            }

            if (m_sampleBufferIndex < (1<<17)) {
                m_sampleBufferIndex++;
            } else {
                m_sampleBufferIndex = 0;
            }

            m_sampleBuffer[m_sampleBufferIndex] = sample;

            if (m_sampleBufferIndex < samplesPerSymbol) {
                delayedSample = m_sampleBuffer[(1<<17) - samplesPerSymbol + m_sampleBufferIndex]; // wrap
            } else {
                delayedSample = m_sampleBuffer[m_sampleBufferIndex - samplesPerSymbol];
            }

            if (m_running.m_syncOrConstellation)
            {
                Sample s(sample, m_dsdDecoder.getSymbolSyncSample());
                m_scopeSampleBuffer.push_back(s);
            }
            else
            {
                Sample s(sample, delayedSample); // I=signal, Q=signal delayed by 20 samples (2400 baud: lowest rate)
                m_scopeSampleBuffer.push_back(s);
            }

            if (DSPEngine::instance()->hasDVSerialSupport() && m_dsdDecoder.mbeDVReady())
            {
                if (!m_running.m_audioMute) {
                    DSPEngine::instance()->pushMbeFrame(m_dsdDecoder.getMbeDVFrame(), m_dsdDecoder.getMbeRateIndex(), m_running.m_volume, &m_audioFifo);
                }
                m_dsdDecoder.resetMbeDV();
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
	}

	if (!DSPEngine::instance()->hasDVSerialSupport())
	{
	    int nbAudioSamples;
	    short *dsdAudio = m_dsdDecoder.getAudio(nbAudioSamples);

	    if (nbAudioSamples > 0)
	    {
	        if (!m_running.m_audioMute) {
	            uint res = m_audioFifo.write((const quint8*) dsdAudio, nbAudioSamples, 10);
	        }

	        m_dsdDecoder.resetAudio();
	    }
	}


    if ((m_scope != 0) && (m_scopeEnabled))
    {
        m_scope->feed(m_scopeSampleBuffer.begin(), m_scopeSampleBuffer.end(), true); // true = real samples for what it's worth
    }

	m_settingsMutex.unlock();
}

void DSDDemod::start()
{
	m_audioFifo.clear();
	m_phaseDiscri.reset();
}

void DSDDemod::stop()
{
}

bool DSDDemod::handleMessage(const Message& cmd)
{
	qDebug() << "DSDDemod::handleMessage";

	if (Channelizer::MsgChannelizerNotification::match(cmd))
	{
		Channelizer::MsgChannelizerNotification& notif = (Channelizer::MsgChannelizerNotification&) cmd;

		m_config.m_inputSampleRate = notif.getSampleRate();
		m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

		apply();

		qDebug() << "DSDDemod::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << m_config.m_inputSampleRate
				<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

		return true;
	}
	else if (MsgConfigureDSDDemod::match(cmd))
	{
		MsgConfigureDSDDemod& cfg = (MsgConfigureDSDDemod&) cmd;

		m_config.m_rfBandwidth = cfg.getRFBandwidth();
		m_config.m_demodGain = cfg.getDemodGain();
		m_config.m_fmDeviation = cfg.getFMDeviation();
		m_config.m_volume = cfg.getVolume();
		m_config.m_baudRate = cfg.getBaudRate();
		m_config.m_squelchGate = cfg.getSquelchGate();
		m_config.m_squelch = cfg.getSquelch();
		m_config.m_audioMute = cfg.getAudioMute();
		m_config.m_enableCosineFiltering = cfg.getEnableCosineFiltering();
		m_config.m_syncOrConstellation = cfg.getSyncOrConstellation();

		apply();

		qDebug() << "DSDDemod::handleMessage: MsgConfigureDSDDemod: m_rfBandwidth: " << m_config.m_rfBandwidth * 100
				<< " m_demodGain: " << m_config.m_demodGain / 100.0
				<< " m_fmDeviation: " << m_config.m_fmDeviation * 100
				<< " m_volume: " << m_config.m_volume / 10.0
                << " m_baudRate: " << m_config.m_baudRate
				<< " m_squelchGate" << m_config.m_squelchGate
				<< " m_squelch: " << m_config.m_squelch
				<< " m_audioMute: " << m_config.m_audioMute
				<< " m_enableCosineFiltering: " << m_config.m_enableCosineFiltering
				<< " m_syncOrConstellation: " << m_config.m_syncOrConstellation;

		return true;
	}
	else
	{
		return false;
	}
}

void DSDDemod::apply()
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
		m_interpolator.create(16, m_config.m_inputSampleRate, (m_config.m_rfBandwidth * 100) / 2.2);
		m_interpolatorDistanceRemain = 0;
		m_interpolatorDistance =  (Real) m_config.m_inputSampleRate / (Real) m_config.m_audioSampleRate;
		m_phaseDiscri.setFMScaling((float) m_config.m_rfBandwidth / (float) m_config.m_fmDeviation);
		m_settingsMutex.unlock();
	}

	if (m_config.m_fmDeviation != m_running.m_fmDeviation)
	{
		m_phaseDiscri.setFMScaling((float) m_config.m_rfBandwidth / (float) m_config.m_fmDeviation);
	}

	if (m_config.m_squelchGate != m_running.m_squelchGate)
	{
		m_squelchGate = 480 * m_config.m_squelchGate; // gate is given in 10s of ms at 48000 Hz audio sample rate
		m_squelchCount = 0; // reset squelch open counter
	}

	if (m_config.m_squelch != m_running.m_squelch)
	{
		// input is a value in tenths of dB
		m_squelchLevel = std::pow(10.0, m_config.m_squelch / 100.0);
		//m_squelchLevel *= m_squelchLevel;
	}

    if (m_config.m_volume != m_running.m_volume)
    {
        m_dsdDecoder.setAudioGain(m_config.m_volume / 10.0f);
    }

    if (m_config.m_baudRate != m_running.m_baudRate)
    {
        m_dsdDecoder.setBaudRate(m_config.m_baudRate);
    }

    if (m_config.m_enableCosineFiltering != m_running.m_enableCosineFiltering)
    {
    	m_dsdDecoder.enableCosineFiltering(m_config.m_enableCosineFiltering);
    }

    m_running.m_inputSampleRate = m_config.m_inputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_rfBandwidth = m_config.m_rfBandwidth;
	m_running.m_demodGain = m_config.m_demodGain;
	m_running.m_fmDeviation = m_config.m_fmDeviation;
	m_running.m_squelchGate = m_config.m_squelchGate;
	m_running.m_squelch = m_config.m_squelch;
	m_running.m_volume = m_config.m_volume;
	m_running.m_baudRate = m_config.m_baudRate;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;
	m_running.m_audioMute = m_config.m_audioMute;
	m_running.m_enableCosineFiltering = m_config.m_enableCosineFiltering;
	m_running.m_syncOrConstellation = m_config.m_syncOrConstellation;
}
