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

#include <dsp/downchannelizer.h>
#include "audio/audiooutput.h"
#include "dsp/pidcontroller.h"
#include "dsp/dspengine.h"
#include "dsp/threadedbasebandsamplesink.h"
#include <dsp/downchannelizer.h>
#include <device/devicesourceapi.h>

#include "dsddemod.h"

MESSAGE_CLASS_DEFINITION(DSDDemod::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(DSDDemod::MsgConfigureDSDDemod, Message)
MESSAGE_CLASS_DEFINITION(DSDDemod::MsgConfigureMyPosition, Message)

const QString DSDDemod::m_channelID = "sdrangel.channel.dsddemod";
const int DSDDemod::m_udpBlockSize = 512;

DSDDemod::DSDDemod(DeviceSourceAPI *deviceAPI) :
	m_deviceAPI(deviceAPI),
	m_absoluteFrequencyOffset(0),
    m_interpolatorDistance(0.0f),
    m_interpolatorDistanceRemain(0.0f),
    m_sampleCount(0),
    m_squelchCount(0),
    m_squelchGate(0),
    m_squelchLevel(1e-4),
    m_squelchOpen(false),
    m_movingAverage(40, 0),
    m_fmExcursion(24),
    m_audioFifo1(48000),
    m_audioFifo2(48000),
    m_scope(0),
    m_scopeEnabled(true),
    m_dsdDecoder(),
    m_settingsMutex(QMutex::Recursive)
{
	setObjectName("DSDDemod");

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

    m_udpBufferAudio = new UDPSink<AudioSample>(this, m_udpBlockSize, m_settings.m_udpPort);
    m_audioFifo1.setUDPSink(m_udpBufferAudio);
    m_audioFifo2.setUDPSink(m_udpBufferAudio);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    applySettings(m_settings, true);
}

DSDDemod::~DSDDemod()
{
    delete[] m_sampleBuffer;
	DSPEngine::instance()->removeAudioSink(&m_audioFifo1);
    DSPEngine::instance()->removeAudioSink(&m_audioFifo2);
    delete m_udpBufferAudio;

    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
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

            Real demod = 32768.0f * m_phaseDiscri.phaseDiscriminator(ci) * m_settings.m_demodGain;
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

            if (m_settings.m_enableCosineFiltering) { // show actual input to FSK demod
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

            if (m_settings.m_syncOrConstellation)
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
                if ((m_settings.m_slot1On) && m_dsdDecoder.mbeDVReady1())
                {
                    if (!m_settings.m_audioMute)
                    {
                        DSPEngine::instance()->pushMbeFrame(
                                m_dsdDecoder.getMbeDVFrame1(),
                                m_dsdDecoder.getMbeRateIndex(),
                                m_settings.m_volume * 10.0,
                                m_settings.m_tdmaStereo ? 1 : 3, // left or both channels
                                &m_audioFifo1);
                    }

                    m_dsdDecoder.resetMbeDV1();
                }

                if ((m_settings.m_slot2On) && m_dsdDecoder.mbeDVReady2())
                {
                    if (!m_settings.m_audioMute)
                    {
                        DSPEngine::instance()->pushMbeFrame(
                                m_dsdDecoder.getMbeDVFrame2(),
                                m_dsdDecoder.getMbeRateIndex(),
                                m_settings.m_volume * 10.0,
                                m_settings.m_tdmaStereo ? 2 : 3, // right or both channels
                                &m_audioFifo2);
                    }

                    m_dsdDecoder.resetMbeDV2();
                }
            }

//            if (DSPEngine::instance()->hasDVSerialSupport() && m_dsdDecoder.mbeDVReady1())
//            {
//                if (!m_settings.m_audioMute)
//                {
//                    DSPEngine::instance()->pushMbeFrame(m_dsdDecoder.getMbeDVFrame1(), m_dsdDecoder.getMbeRateIndex(), m_settings.m_volume, &m_audioFifo1);
//                }
//
//                m_dsdDecoder.resetMbeDV1();
//            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
	}

	if (!DSPEngine::instance()->hasDVSerialSupport())
	{
	    if (m_settings.m_slot1On)
	    {
	        int nbAudioSamples;
	        short *dsdAudio = m_dsdDecoder.getAudio1(nbAudioSamples);

	        if (nbAudioSamples > 0)
	        {
	            if (!m_settings.m_audioMute) {
	                m_audioFifo1.write((const quint8*) dsdAudio, nbAudioSamples, 10);
	            }

	            m_dsdDecoder.resetAudio1();
	        }
	    }

        if (m_settings.m_slot2On)
        {
            int nbAudioSamples;
            short *dsdAudio = m_dsdDecoder.getAudio2(nbAudioSamples);

            if (nbAudioSamples > 0)
            {
                if (!m_settings.m_audioMute) {
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
//	        if (!m_settings.m_audioMute) {
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

		DSDDemodSettings settings = m_settings;
		settings.m_inputSampleRate = notif.getSampleRate();
		settings.m_inputFrequencyOffset = notif.getFrequencyOffset();

		applySettings(settings);

		qDebug() << "DSDDemod::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << settings.m_inputSampleRate
				<< " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset;

		return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureDSDDemod::match(cmd))
    {
        MsgConfigureDSDDemod& cfg = (MsgConfigureDSDDemod&) cmd;

        DSDDemodSettings settings = cfg.getSettings();

        // These settings are set with DownChannelizer::MsgChannelizerNotification
        m_absoluteFrequencyOffset = settings.m_inputFrequencyOffset;
        settings.m_inputSampleRate = m_settings.m_inputSampleRate;
        settings.m_inputFrequencyOffset = m_settings.m_inputFrequencyOffset;

        applySettings(settings, cfg.getForce());

        qDebug() << "DSDDemod::handleMessage: MsgConfigureDSDDemod: m_rfBandwidth: " << m_settings.m_rfBandwidth
                << " m_fmDeviation: " << m_settings.m_fmDeviation
                << " m_demodGain: " << m_settings.m_demodGain
                << " m_volume: " << m_settings.m_volume
                << " m_baudRate: " << m_settings.m_baudRate
                << " m_squelchGate" << m_settings.m_squelchGate
                << " m_squelch: " << m_settings.m_squelch
                << " m_audioMute: " << m_settings.m_audioMute
                << " m_enableCosineFiltering: " << m_settings.m_enableCosineFiltering
                << " m_syncOrConstellation: " << m_settings.m_syncOrConstellation
                << " m_slot1On: " << m_settings.m_slot1On
                << " m_slot2On: " << m_settings.m_slot2On
                << " m_tdmaStereo: " << m_settings.m_tdmaStereo
                << " m_pllLock: " << m_settings.m_pllLock
                << " m_udpCopyAudio: " << m_settings.m_udpCopyAudio
                << " m_udpAddress: " << m_settings.m_udpAddress
                << " m_udpPort: " << m_settings.m_udpPort
                << " force: " << cfg.getForce();

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

void DSDDemod::applySettings(DSDDemodSettings& settings, bool force)
{
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) ||
        (settings.m_inputSampleRate != m_settings.m_inputSampleRate) || force)
    {
        m_nco.setFreq(-settings.m_inputFrequencyOffset, settings.m_inputSampleRate);
    }

    if ((settings.m_inputSampleRate != m_settings.m_inputSampleRate) ||
        (settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, settings.m_inputSampleRate, (settings.m_rfBandwidth) / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) settings.m_inputSampleRate / (Real) settings.m_audioSampleRate;
        m_phaseDiscri.setFMScaling((float) settings.m_rfBandwidth / (float) settings.m_fmDeviation);
        m_settingsMutex.unlock();
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling((float) settings.m_rfBandwidth / (float) settings.m_fmDeviation);
    }

    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force)
    {
        m_squelchGate = 480 * settings.m_squelchGate; // gate is given in 10s of ms at 48000 Hz audio sample rate
        m_squelchCount = 0; // reset squelch open counter
    }

    if ((settings.m_squelch != m_settings.m_squelch) || force)
    {
        // input is a value in dB
        m_squelchLevel = std::pow(10.0, settings.m_squelch / 10.0);
    }

    if ((settings.m_volume != m_settings.m_volume) || force)
    {
        m_dsdDecoder.setAudioGain(settings.m_volume);
    }

    if ((settings.m_baudRate != m_settings.m_baudRate) || force)
    {
        m_dsdDecoder.setBaudRate(settings.m_baudRate);
    }

    if ((settings.m_enableCosineFiltering != m_settings.m_enableCosineFiltering) || force)
    {
        m_dsdDecoder.enableCosineFiltering(settings.m_enableCosineFiltering);
    }

    if ((settings.m_tdmaStereo != m_settings.m_tdmaStereo) || force)
    {
        m_dsdDecoder.setTDMAStereo(settings.m_tdmaStereo);
    }

    if ((settings.m_pllLock != m_settings.m_pllLock) || force)
    {
        m_dsdDecoder.setSymbolPLLLock(settings.m_pllLock);
    }

    if ((settings.m_udpAddress != m_settings.m_udpAddress)
        || (settings.m_udpPort != m_settings.m_udpPort) || force)
    {
        m_udpBufferAudio->setAddress(settings.m_udpAddress);
        m_udpBufferAudio->setPort(settings.m_udpPort);
    }

    if ((settings.m_udpCopyAudio != m_settings.m_udpCopyAudio)
        || (settings.m_slot1On != m_settings.m_slot1On)
        || (settings.m_slot2On != m_settings.m_slot2On) || force)
    {
        m_audioFifo1.setCopyToUDP(settings.m_slot1On && settings.m_udpCopyAudio);
        m_audioFifo2.setCopyToUDP(settings.m_slot2On && !settings.m_slot1On && settings.m_udpCopyAudio);
    }

    m_settings = settings;
}
