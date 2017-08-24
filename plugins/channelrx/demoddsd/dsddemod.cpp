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

#include "../../channelrx/demoddsd/dsddemod.h"

#include <QTime>
#include <QDebug>
#include <stdio.h>
#include <complex.h>
#include <dsp/downchannelizer.h>
#include "audio/audiooutput.h"
#include "dsp/pidcontroller.h"
#include "dsp/dspengine.h"
#include "dsddemodgui.h"

MESSAGE_CLASS_DEFINITION(DSDDemod::MsgConfigureDSDDemod, Message)
MESSAGE_CLASS_DEFINITION(DSDDemod::MsgConfigureMyPosition, Message)

const int DSDDemod::m_udpBlockSize = 512;

DSDDemod::DSDDemod(BasebandSampleSink* sampleSink) :
	m_sampleCount(0),
	m_squelchCount(0),
	m_squelchOpen(false),
    m_movingAverage(40, 0),
    m_fmExcursion(24),
	m_audioFifo1(4, 48000),
    m_audioFifo2(4, 48000),
    m_scope(sampleSink),
    m_scopeEnabled(true),
    m_dsdDecoder(),
    m_settingsMutex(QMutex::Recursive)
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
	m_magsq = 0.0f;
    m_magsqSum = 0.0f;
    m_magsqPeak = 0.0f;
    m_magsqCount = 0;

	DSPEngine::instance()->addAudioSink(&m_audioFifo1);
    DSPEngine::instance()->addAudioSink(&m_audioFifo2);

    m_udpBufferMono = new UDPSink<FixReal>(this, m_udpBlockSize, m_config.m_udpPort);
}

DSDDemod::~DSDDemod()
{
    delete m_udpBufferMono;
    delete[] m_sampleBuffer;
	DSPEngine::instance()->removeAudioSink(&m_audioFifo1);
    DSPEngine::instance()->removeAudioSink(&m_audioFifo2);
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
		bool syncOrConstellation,
		bool slot1On,
		bool slot2On,
		bool tdmaStereo,
		bool pllLock,
        bool udpCopyAudio,
        const QString& udpAddress,
        quint16 udpPort,
        bool force)
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
			syncOrConstellation,
			slot1On,
			slot2On,
			tdmaStereo,
			pllLock,
			udpCopyAudio,
			udpAddress,
			udpPort,
			force);
	messageQueue->push(cmd);
}

void DSDDemod::configureMyPosition(MessageQueue* messageQueue, float myLatitude, float myLongitude)
{
	Message* cmd = MsgConfigureMyPosition::create(myLatitude, myLongitude);
	messageQueue->push(cmd);
}

void DSDDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
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

        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
        {
            qint16 sample, delayedSample;

            Real magsq = ((ci.real()*ci.real() +  ci.imag()*ci.imag()))  / (1<<30);
            m_movingAverage.feed(magsq);

            m_magsqSum += magsq;

            if (magsq > m_magsqPeak)
            {
                m_magsqPeak = magsq;
            }

            m_magsqCount++;

            Real demod = 32768.0f * m_phaseDiscri.phaseDiscriminator(ci) * ((float) m_running.m_demodGain / 100.0f);
            m_sampleCount++;

            // AF processing

            if (m_movingAverage.average() > m_squelchLevel)
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

            if (DSPEngine::instance()->hasDVSerialSupport())
            {
                if ((m_running.m_slot1On) && m_dsdDecoder.mbeDVReady1())
                {
                    if (!m_running.m_audioMute)
                    {
                        DSPEngine::instance()->pushMbeFrame(
                                m_dsdDecoder.getMbeDVFrame1(),
                                m_dsdDecoder.getMbeRateIndex(),
                                m_running.m_volume,
                                m_running.m_tdmaStereo ? 1 : 3, // left or both channels
                                &m_audioFifo1);
                    }

                    m_dsdDecoder.resetMbeDV1();
                }

                if ((m_running.m_slot2On) && m_dsdDecoder.mbeDVReady2())
                {
                    if (!m_running.m_audioMute)
                    {
                        DSPEngine::instance()->pushMbeFrame(
                                m_dsdDecoder.getMbeDVFrame2(),
                                m_dsdDecoder.getMbeRateIndex(),
                                m_running.m_volume,
                                m_running.m_tdmaStereo ? 2 : 3, // right or both channels
                                &m_audioFifo2);
                    }

                    m_dsdDecoder.resetMbeDV2();
                }
            }

//            if (DSPEngine::instance()->hasDVSerialSupport() && m_dsdDecoder.mbeDVReady1())
//            {
//                if (!m_running.m_audioMute)
//                {
//                    DSPEngine::instance()->pushMbeFrame(m_dsdDecoder.getMbeDVFrame1(), m_dsdDecoder.getMbeRateIndex(), m_running.m_volume, &m_audioFifo1);
//                }
//
//                m_dsdDecoder.resetMbeDV1();
//            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
	}

	if (!DSPEngine::instance()->hasDVSerialSupport())
	{
	    if (m_running.m_slot1On)
	    {
	        int nbAudioSamples;
	        short *dsdAudio = m_dsdDecoder.getAudio1(nbAudioSamples);

	        if (nbAudioSamples > 0)
	        {
	            if (!m_running.m_audioMute) {
	                m_audioFifo1.write((const quint8*) dsdAudio, nbAudioSamples, 10);
	            }

	            m_dsdDecoder.resetAudio1();
	        }
	    }

        if (m_running.m_slot2On)
        {
            int nbAudioSamples;
            short *dsdAudio = m_dsdDecoder.getAudio2(nbAudioSamples);

            if (nbAudioSamples > 0)
            {
                if (!m_running.m_audioMute) {
                    m_audioFifo2.write((const quint8*) dsdAudio, nbAudioSamples, 10);
                }

                m_dsdDecoder.resetAudio2();
            }
        }

//	    int nbAudioSamples;
//	    short *dsdAudio = m_dsdDecoder.getAudio1(nbAudioSamples);
//
//	    if (nbAudioSamples > 0)
//	    {
//	        if (!m_running.m_audioMute) {
//	            uint res = m_audioFifo1.write((const quint8*) dsdAudio, nbAudioSamples, 10);
//	        }
//
//	        m_dsdDecoder.resetAudio1();
//	    }
	}


    if ((m_scope != 0) && (m_scopeEnabled))
    {
        m_scope->feed(m_scopeSampleBuffer.begin(), m_scopeSampleBuffer.end(), true); // true = real samples for what it's worth
    }

	m_settingsMutex.unlock();
}

void DSDDemod::start()
{
	m_audioFifo1.clear();
    m_audioFifo2.clear();
	m_phaseDiscri.reset();
}

void DSDDemod::stop()
{
}

bool DSDDemod::handleMessage(const Message& cmd)
{
	qDebug() << "DSDDemod::handleMessage";

	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

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
		m_config.m_slot1On = cfg.getSlot1On();
		m_config.m_slot2On = cfg.getSlot2On();
		m_config.m_tdmaStereo = cfg.getTDMAStereo();
		m_config.m_pllLock = cfg.getPLLLock();
		m_config.m_udpCopyAudio = cfg.getUDPCopyAudio();
		m_config.m_udpAddress = cfg.getUDPAddress();
		m_config.m_udpPort = cfg.getUDPPort();

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
				<< " m_syncOrConstellation: " << m_config.m_syncOrConstellation
				<< " m_slot1On: " << m_config.m_slot1On
				<< " m_slot2On: " << m_config.m_slot2On
				<< " m_tdmaStereo: " << m_config.m_tdmaStereo
				<< " m_pllLock: " << m_config.m_pllLock
                << " m_udpCopyAudio: " << m_config.m_udpCopyAudio
                << " m_udpAddress: " << m_config.m_udpAddress
                << " m_udpPort: " << m_config.m_udpPort;

		return true;
	}
	else if (MsgConfigureMyPosition::match(cmd))
	{
		MsgConfigureMyPosition& cfg = (MsgConfigureMyPosition&) cmd;
		m_dsdDecoder.setMyPoint(cfg.getMyLatitude(), cfg.getMyLongitude());
		return true;
	}
	else
	{
		return false;
	}
}

void DSDDemod::apply(bool force)
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
		m_interpolator.create(16, m_config.m_inputSampleRate, (m_config.m_rfBandwidth * 100) / 2.2);
		m_interpolatorDistanceRemain = 0;
		m_interpolatorDistance =  (Real) m_config.m_inputSampleRate / (Real) m_config.m_audioSampleRate;
		m_phaseDiscri.setFMScaling((float) m_config.m_rfBandwidth / (float) m_config.m_fmDeviation);
		m_settingsMutex.unlock();
	}

	if ((m_config.m_fmDeviation != m_running.m_fmDeviation) || force)
	{
		m_phaseDiscri.setFMScaling((float) m_config.m_rfBandwidth / (float) m_config.m_fmDeviation);
	}

	if ((m_config.m_squelchGate != m_running.m_squelchGate) || force)
	{
		m_squelchGate = 480 * m_config.m_squelchGate; // gate is given in 10s of ms at 48000 Hz audio sample rate
		m_squelchCount = 0; // reset squelch open counter
	}

	if ((m_config.m_squelch != m_running.m_squelch) || force)
	{
		// input is a value in tenths of dB
		m_squelchLevel = std::pow(10.0, m_config.m_squelch / 100.0);
		//m_squelchLevel *= m_squelchLevel;
	}

    if ((m_config.m_volume != m_running.m_volume) || force)
    {
        m_dsdDecoder.setAudioGain(m_config.m_volume / 10.0f);
    }

    if ((m_config.m_baudRate != m_running.m_baudRate) || force)
    {
        m_dsdDecoder.setBaudRate(m_config.m_baudRate);
    }

    if ((m_config.m_enableCosineFiltering != m_running.m_enableCosineFiltering) || force)
    {
    	m_dsdDecoder.enableCosineFiltering(m_config.m_enableCosineFiltering);
    }

    if ((m_config.m_tdmaStereo != m_running.m_tdmaStereo) || force)
    {
        m_dsdDecoder.setTDMAStereo(m_config.m_tdmaStereo);
    }

    if ((m_config.m_pllLock != m_running.m_pllLock) || force)
    {
        m_dsdDecoder.setSymbolPLLLock(m_config.m_pllLock);
    }

    if ((m_config.m_udpAddress != m_running.m_udpAddress)
        || (m_config.m_udpPort != m_running.m_udpPort) || force)
    {
        m_udpBufferMono->setAddress(m_config.m_udpAddress);
        m_udpBufferMono->setPort(m_config.m_udpPort);
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
	m_running.m_slot1On = m_config.m_slot1On;
	m_running.m_slot2On = m_config.m_slot2On;
	m_running.m_tdmaStereo = m_config.m_tdmaStereo;
	m_running.m_pllLock = m_config.m_pllLock;
}
