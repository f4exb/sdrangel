///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include "ssbmod.h"

#include <QTime>
#include <QDebug>
#include <QMutexLocker>
#include <stdio.h>
#include <complex.h>
#include <dsp/upchannelizer.h>
#include "dsp/dspengine.h"
#include "dsp/pidcontroller.h"

MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureSSBMod, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureAFInput, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgReportFileSourceStreamTiming, Message)

const int SSBMod::m_levelNbSamples = 480; // every 10ms

SSBMod::SSBMod() :
    m_audioFifo(4, 48000),
	m_settingsMutex(QMutex::Recursive),
	m_fileSize(0),
	m_recordLength(0),
	m_sampleRate(48000),
	m_afInput(SSBModInputNone),
	m_levelCalcCount(0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f)
{
	setObjectName("SSBMod");

	m_config.m_outputSampleRate = 48000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_bandwidth = 12500;
	m_config.m_toneFrequency = 1000.0f;
	m_config.m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();

	apply();

	//m_audioBuffer.resize(1<<14);
	//m_audioBufferFill = 0;

	m_movingAverage.resize(16, 0);
	m_volumeAGC.resize(4096, 0.003, 0);
	m_magsq = 0.0;

	m_toneNco.setFreq(1000.0, m_config.m_audioSampleRate);
	DSPEngine::instance()->addAudioSource(&m_audioFifo);

	// CW keyer
	m_cwKeyer.setSampleRate(m_config.m_audioSampleRate);
	m_cwKeyer.setWPM(13);
	m_cwKeyer.setMode(CWKeyer::CWNone);
}

SSBMod::~SSBMod()
{
    DSPEngine::instance()->removeAudioSource(&m_audioFifo);
}

void SSBMod::configure(MessageQueue* messageQueue,
		Real bandwidth,
		Real lowCutoff,
		float toneFrequency,
		float volumeFactor,
		int spanLog2,
		bool audioBinaural,
		bool audioFlipChannels,
		bool dsb,
		bool audioMute,
		bool playLoop)
{
	Message* cmd = MsgConfigureSSBMod::create(bandwidth,
			lowCutoff,
			toneFrequency,
			volumeFactor,
			spanLog2,
			audioBinaural,
			audioFlipChannels,
			dsb,
			audioMute,
			playLoop);
	messageQueue->push(cmd);
}

void SSBMod::pull(Sample& sample)
{
	Complex ci;

	m_settingsMutex.lock();

    if (m_interpolatorDistance > 1.0f) // decimate
    {
    	modulateSample();

        while (!m_interpolator.decimate(&m_interpolatorDistanceRemain, m_modSample, &ci))
        {
        	modulateSample();
        }
    }
    else
    {
        if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_modSample, &ci))
        {
        	modulateSample();
        }
    }

    m_interpolatorDistanceRemain += m_interpolatorDistance;

    ci *= m_carrierNco.nextIQ(); // shift to carrier frequency

    m_settingsMutex.unlock();

    Real magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
	magsq /= (1<<30);
	m_movingAverage.feed(magsq);
	m_magsq = m_movingAverage.average();

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();
}

void SSBMod::modulateSample()
{
	Real t;

    pullAF(t);
    calculateLevel(t);

    m_modSample.real(0.0f); // TOOO
    m_modSample.imag(0.0f);
}

void SSBMod::pullAF(Real& sample)
{
    int16_t audioSample[2];

    switch (m_afInput)
    {
    case SSBModInputTone:
        sample = m_toneNco.next();
        break;
    case SSBModInputFile:
        // sox f4exb_call.wav --encoding float --endian little f4exb_call.raw
        // ffplay -f f32le -ar 48k -ac 1 f4exb_call.raw
        if (m_ifstream.is_open())
        {
            if (m_ifstream.eof())
            {
            	if (m_running.m_playLoop)
            	{
                    m_ifstream.clear();
                    m_ifstream.seekg(0, std::ios::beg);
            	}
            }

            if (m_ifstream.eof())
            {
            	sample = 0.0f;
            }
            else
            {
            	m_ifstream.read(reinterpret_cast<char*>(&sample), sizeof(Real));
            	sample *= m_running.m_volumeFactor;
            }
        }
        else
        {
            sample = 0.0f;
        }
        break;
    case SSBModInputAudio:
        m_audioFifo.read(reinterpret_cast<quint8*>(audioSample), 1, 10);
        sample = ((audioSample[0] + audioSample[1])  / 65536.0f) * m_running.m_volumeFactor;
        break;
    case SSBModInputCWTone:
        if (m_cwKeyer.getSample())
        {
            sample = m_toneNco.next();
        }
        else
        {
            sample = 0.0f;
            m_toneNco.setPhase(0);
        }
        break;
    case SSBModInputNone:
    default:
        sample = 0.0f;
        break;
    }
}

void SSBMod::calculateLevel(Real& sample)
{
    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), sample);
        m_levelSum += sample * sample;
        m_levelCalcCount++;
    }
    else
    {
        qreal rmsLevel = sqrt(m_levelSum / m_levelNbSamples);
        //qDebug("NFMMod::calculateLevel: %f %f", rmsLevel, m_peakLevel);
        emit levelChanged(rmsLevel, m_peakLevel, m_levelNbSamples);
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void SSBMod::start()
{
	qDebug() << "SSBMod::start: m_outputSampleRate: " << m_config.m_outputSampleRate
			<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

	m_audioFifo.clear();
}

void SSBMod::stop()
{
}

bool SSBMod::handleMessage(const Message& cmd)
{
	if (UpChannelizer::MsgChannelizerNotification::match(cmd))
	{
		UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;

		m_config.m_outputSampleRate = notif.getSampleRate();
		m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

		apply();

		qDebug() << "SSBMod::handleMessage: MsgChannelizerNotification:"
				<< " m_outputSampleRate: " << m_config.m_outputSampleRate
				<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

		return true;
	}
	else if (MsgConfigureSSBMod::match(cmd))
	{
		float band, lowCutoff;

	    MsgConfigureSSBMod& cfg = (MsgConfigureSSBMod&) cmd;
	    m_settingsMutex.lock();

		band = cfg.getBandwidth();
		lowCutoff = cfg.getLoCutoff();

		if (band < 0) // negative means LSB
		{
			band = -band;            // turn to positive
			lowCutoff = -lowCutoff;
			m_config.m_usb = false;  // and take note of side band
		}
		else
		{
			m_config.m_usb = true;
		}

		if (band < 100.0f) // at least 100 Hz
		{
			band = 100.0f;
			lowCutoff = 0;
		}

		m_config.m_bandwidth = band;
		m_config.m_lowCutoff = lowCutoff;

		// TODO: move to apply
		SSBFilter->create_filter(m_config.m_lowCutoff / (float) m_config.m_audioSampleRate, m_config.m_bandwidth / (float) m_config.m_audioSampleRate);
		DSBFilter->create_dsb_filter((2.0f * m_config.m_bandwidth) / (float) m_config.m_audioSampleRate);

		m_config.m_toneFrequency = cfg.getToneFrequency();
		m_config.m_volumeFactor = cfg.getVolumeFactor();
		m_config.m_spanLog2 = cfg.getSpanLog2();
		m_config.m_audioBinaural = cfg.getAudioBinaural();
		m_config.m_audioFlipChannels = cfg.getAudioFlipChannels();
		m_config.m_dsb = cfg.getDSB();
		m_config.m_audioMute = cfg.getAudioMute();
		m_config.m_playLoop = cfg.getPlayLoop();

		apply();

		m_settingsMutex.unlock();

		qDebug() << "SSBMod::handleMessage: MsgConfigureSSBMod:"
				<< " m_bandwidth: " << m_config.m_bandwidth
				<< " m_lowCutoff: " << m_config.m_lowCutoff
                << " m_toneFrequency: " << m_config.m_toneFrequency
                << " m_volumeFactor: " << m_config.m_volumeFactor
				<< " m_spanLog2: " << m_config.m_spanLog2
				<< " m_audioBinaural: " << m_config.m_audioBinaural
				<< " m_audioFlipChannels: " << m_config.m_audioFlipChannels
				<< " m_dsb: " << m_config.m_dsb
				<< " m_audioMute: " << m_config.m_audioMute
				<< " m_playLoop: " << m_config.m_playLoop;

		return true;
	}
	else if (MsgConfigureFileSourceName::match(cmd))
    {
        MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) cmd;
        m_fileName = conf.getFileName();
        openFileStream();
        return true;
    }
    else if (MsgConfigureFileSourceSeek::match(cmd))
    {
        MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        seekFileStream(seekPercentage);

        return true;
    }
    else if (MsgConfigureAFInput::match(cmd))
    {
        MsgConfigureAFInput& conf = (MsgConfigureAFInput&) cmd;
        m_afInput = conf.getAFInput();

        return true;
    }
    else if (MsgConfigureFileSourceStreamTiming::match(cmd))
    {
    	std::size_t samplesCount;

    	if (m_ifstream.eof()) {
    		samplesCount = m_fileSize / sizeof(Real);
    	} else {
    		samplesCount = m_ifstream.tellg() / sizeof(Real);
    	}

    	MsgReportFileSourceStreamTiming *report;
        report = MsgReportFileSourceStreamTiming::create(samplesCount);
        getOutputMessageQueue()->push(report);

        return true;
    }
	else
	{
		return false;
	}
}

void SSBMod::apply()
{

	if ((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
	    (m_config.m_outputSampleRate != m_running.m_outputSampleRate))
	{
        m_settingsMutex.lock();
		m_carrierNco.setFreq(m_config.m_inputFrequencyOffset, m_config.m_outputSampleRate);
        m_settingsMutex.unlock();
	}

	if((m_config.m_outputSampleRate != m_running.m_outputSampleRate) ||
	   (m_config.m_bandwidth != m_running.m_bandwidth) ||
	   (m_config.m_audioSampleRate != m_running.m_audioSampleRate))
	{
		m_settingsMutex.lock();
		m_interpolatorDistanceRemain = 0;
		m_interpolatorConsumed = false;
		m_interpolatorDistance = (Real) m_config.m_audioSampleRate / (Real) m_config.m_outputSampleRate;
        m_interpolator.create(48, m_config.m_audioSampleRate, m_config.m_bandwidth / 2.2, 3.0);
		m_settingsMutex.unlock();
	}

	if ((m_config.m_toneFrequency != m_running.m_toneFrequency) ||
	    (m_config.m_audioSampleRate != m_running.m_audioSampleRate))
	{
        m_settingsMutex.lock();
        m_toneNco.setFreq(m_config.m_toneFrequency, m_config.m_audioSampleRate);
        m_settingsMutex.unlock();
	}

	if (m_config.m_audioSampleRate != m_running.m_audioSampleRate)
	{
	    m_cwKeyer.setSampleRate(m_config.m_audioSampleRate);
	}

	m_running.m_outputSampleRate = m_config.m_outputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_bandwidth = m_config.m_bandwidth;
	m_running.m_lowCutoff = m_config.m_lowCutoff;
	m_running.m_toneFrequency = m_config.m_toneFrequency;
    m_running.m_volumeFactor = m_config.m_volumeFactor;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;
	m_running.m_spanLog2 = m_config.m_spanLog2;
	m_running.m_audioBinaural = m_config.m_audioBinaural;
	m_running.m_audioFlipChannels = m_config.m_audioFlipChannels;
	m_running.m_dsb = m_config.m_dsb;
	m_running.m_audioMute = m_config.m_audioMute;
	m_running.m_playLoop = m_config.m_playLoop;
}

void SSBMod::openFileStream()
{
    if (m_ifstream.is_open()) {
        m_ifstream.close();
    }

    m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
    m_fileSize = m_ifstream.tellg();
    m_ifstream.seekg(0,std::ios_base::beg);

    m_sampleRate = 48000; // fixed rate
    m_recordLength = m_fileSize / (sizeof(Real) * m_sampleRate);

    qDebug() << "AMMod::openFileStream: " << m_fileName.toStdString().c_str()
            << " fileSize: " << m_fileSize << "bytes"
            << " length: " << m_recordLength << " seconds";

    MsgReportFileSourceStreamData *report;
    report = MsgReportFileSourceStreamData::create(m_sampleRate, m_recordLength);
    getOutputMessageQueue()->push(report);
}

void SSBMod::seekFileStream(int seekPercentage)
{
    QMutexLocker mutexLocker(&m_settingsMutex);

    if (m_ifstream.is_open())
    {
        int seekPoint = ((m_recordLength * seekPercentage) / 100) * m_sampleRate;
        seekPoint *= sizeof(Real);
        m_ifstream.clear();
        m_ifstream.seekg(seekPoint, std::ios::beg);
    }
}
