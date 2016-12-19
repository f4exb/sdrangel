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

#include <QTime>
#include <QDebug>
#include <QMutexLocker>
#include <stdio.h>
#include <complex.h>
#include <algorithm>
#include <dsp/upchannelizer.h>
#include "dsp/dspengine.h"
#include "dsp/pidcontroller.h"
#include "wfmmod.h"

MESSAGE_CLASS_DEFINITION(WFMMod::MsgConfigureWFMMod, Message)
MESSAGE_CLASS_DEFINITION(WFMMod::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(WFMMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(WFMMod::MsgConfigureAFInput, Message)
MESSAGE_CLASS_DEFINITION(WFMMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(WFMMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(WFMMod::MsgReportFileSourceStreamTiming, Message)

const int WFMMod::m_levelNbSamples = 480; // every 10ms
const int WFMMod::m_rfFilterFFTLength = 1024;

WFMMod::WFMMod() :
	m_modPhasor(0.0f),
    m_audioFifo(4, 48000),
	m_settingsMutex(QMutex::Recursive),
	m_fileSize(0),
	m_recordLength(0),
	m_sampleRate(48000),
	m_afInput(WFMModInputNone),
	m_levelCalcCount(0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f)
{
	setObjectName("WFMod");

	m_config.m_outputSampleRate = 384000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_rfBandwidth = 125000;
	m_config.m_afBandwidth = 8000;
	m_config.m_fmDeviation = 50000.0f;
	m_config.m_toneFrequency = 1000.0f;
	m_config.m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();

	m_rfFilter = new fftfilt(-62500.0 / 384000.0, 62500.0 / 384000.0, m_rfFilterFFTLength);

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
    m_cwSmoother.setNbFadeSamples(192); // 2 ms @ 48 kHz
}

WFMMod::~WFMMod()
{
    delete m_rfFilter;
    DSPEngine::instance()->removeAudioSource(&m_audioFifo);
}

void WFMMod::configure(MessageQueue* messageQueue,
		Real rfBandwidth,
		Real afBandwidth,
		float fmDeviation,
		float toneFrequency,
		float volumeFactor,
		bool audioMute,
		bool playLoop)
{
	Message* cmd = MsgConfigureWFMMod::create(rfBandwidth, afBandwidth, fmDeviation, toneFrequency, volumeFactor, audioMute, playLoop);
	messageQueue->push(cmd);
}

void WFMMod::pull(Sample& sample)
{
	Complex ci, ri;
	Real t;

	m_settingsMutex.lock();

    if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_modSample, &ri))
    {
        pullAF(m_modSample);
        calculateLevel(m_modSample.real());
    }

    m_interpolatorDistanceRemain += m_interpolatorDistance;

    m_modPhasor += (m_running.m_fmDeviation / (float) m_running.m_outputSampleRate) * ri.real() * M_PI;
    ci.real(cos(m_modPhasor) * 29204.0f); // -1 dB
    ci.imag(sin(m_modPhasor) * 29204.0f);

    // RF filtering is unnecessary

    ci *= m_carrierNco.nextIQ(); // shift to carrier frequency

    m_settingsMutex.unlock();

    Real magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
	magsq /= (1<<30);
	m_movingAverage.feed(magsq);
	m_magsq = m_movingAverage.average();

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();
}

void WFMMod::pullAF(Complex& sample)
{
    int16_t audioSample[2];

    switch (m_afInput)
    {
    case WFMModInputTone:
        sample.real(m_toneNco.next() * m_running.m_volumeFactor);
        sample.imag(0.0f);
        break;
    case WFMModInputFile:
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
            	sample.real(0.0f);
                sample.imag(0.0f);
            }
            else
            {
                Real s;
            	m_ifstream.read(reinterpret_cast<char*>(&s), sizeof(Real));
            	sample.real(s * m_running.m_volumeFactor);
                sample.imag(0.0f);
            }
        }
        else
        {
            sample.real(0.0f);
            sample.imag(0.0f);
        }
        break;
    case WFMModInputAudio:
        {
            Real s = (audioSample[0] + audioSample[1])  / 65536.0f;
            m_audioFifo.read(reinterpret_cast<quint8*>(audioSample), 1, 10);
            sample.real(s * m_running.m_volumeFactor);
            sample.imag(0.0f);
        }
        break;
    case WFMModInputCWTone:
        Real fadeFactor;

        if (m_cwKeyer.getSample())
        {
            m_cwSmoother.getFadeSample(true, fadeFactor);
            sample.real(m_toneNco.next() * m_running.m_volumeFactor * fadeFactor);
            sample.imag(0.0f);
        }
        else
        {
            if (m_cwSmoother.getFadeSample(false, fadeFactor))
            {
                sample.real(m_toneNco.next() * m_running.m_volumeFactor * fadeFactor);
                sample.imag(0.0f);
            }
            else
            {
                sample.real(0.0f);
                sample.imag(0.0f);
                m_toneNco.setPhase(0);
            }
        }
        break;
    case WFMModInputNone:
    default:
        sample.real(0.0f);
        sample.imag(0.0f);
        break;
    }
}

void WFMMod::calculateLevel(const Real& sample)
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
        //qDebug("WFMMod::calculateLevel: %f %f", rmsLevel, m_peakLevel);
        emit levelChanged(rmsLevel, m_peakLevel, m_levelNbSamples);
        m_peakLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCalcCount = 0;
    }
}

void WFMMod::start()
{
	qDebug() << "WFMMod::start: m_outputSampleRate: " << m_config.m_outputSampleRate
			<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

	m_audioFifo.clear();
}

void WFMMod::stop()
{
}

bool WFMMod::handleMessage(const Message& cmd)
{
	if (UpChannelizer::MsgChannelizerNotification::match(cmd))
	{
		UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;

		m_config.m_outputSampleRate = notif.getSampleRate();
		m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

		apply();

		qDebug() << "WFMMod::handleMessage: MsgChannelizerNotification:"
				<< " m_outputSampleRate: " << m_config.m_outputSampleRate
				<< " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

		return true;
	}
	else if (MsgConfigureWFMMod::match(cmd))
	{
	    MsgConfigureWFMMod& cfg = (MsgConfigureWFMMod&) cmd;

		m_config.m_rfBandwidth = cfg.getRFBandwidth();
		m_config.m_afBandwidth = cfg.getAFBandwidth();
		m_config.m_fmDeviation = cfg.getFMDeviation();
		m_config.m_toneFrequency = cfg.getToneFrequency();
		m_config.m_volumeFactor = cfg.getVolumeFactor();
		m_config.m_audioMute = cfg.getAudioMute();
		m_config.m_playLoop = cfg.getPlayLoop();

		apply();

		qDebug() << "WFMMod::handleMessage: MsgConfigureWFMMod:"
				<< " m_rfBandwidth: " << m_config.m_rfBandwidth
				<< " m_afBandwidth: " << m_config.m_afBandwidth
				<< " m_fmDeviation: " << m_config.m_fmDeviation
                << " m_toneFrequency: " << m_config.m_toneFrequency
                << " m_volumeFactor: " << m_config.m_volumeFactor
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

void WFMMod::apply()
{

	if ((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
	    (m_config.m_outputSampleRate != m_running.m_outputSampleRate))
	{
        m_settingsMutex.lock();
		m_carrierNco.setFreq(m_config.m_inputFrequencyOffset, m_config.m_outputSampleRate);
        m_settingsMutex.unlock();
	}

	if((m_config.m_outputSampleRate != m_running.m_outputSampleRate) ||
	    (m_config.m_audioSampleRate != m_running.m_audioSampleRate) ||
		(m_config.m_afBandwidth != m_running.m_afBandwidth))
	{
		m_settingsMutex.lock();
		m_interpolatorDistanceRemain = 0;
		m_interpolatorConsumed = false;
		m_interpolatorDistance = (Real) m_config.m_audioSampleRate / (Real) m_config.m_outputSampleRate;
        m_interpolator.create(48, m_config.m_audioSampleRate, m_config.m_afBandwidth, 3.0);
		m_settingsMutex.unlock();
	}

	if ((m_config.m_rfBandwidth != m_running.m_rfBandwidth) ||
		(m_config.m_outputSampleRate != m_running.m_outputSampleRate))
	{
		m_settingsMutex.lock();
        Real lowCut = -(m_config.m_rfBandwidth / 2.0) / m_config.m_outputSampleRate;
        Real hiCut  = (m_config.m_rfBandwidth / 2.0) / m_config.m_outputSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
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
        m_cwSmoother.setNbFadeSamples(m_config.m_audioSampleRate / 250); // 4 ms
    }

	m_running.m_outputSampleRate = m_config.m_outputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_rfBandwidth = m_config.m_rfBandwidth;
	m_running.m_afBandwidth = m_config.m_afBandwidth;
	m_running.m_fmDeviation = m_config.m_fmDeviation;
    m_running.m_volumeFactor = m_config.m_volumeFactor;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;
	m_running.m_audioMute = m_config.m_audioMute;
	m_running.m_playLoop = m_config.m_playLoop;
}

void WFMMod::openFileStream()
{
    if (m_ifstream.is_open()) {
        m_ifstream.close();
    }

    m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
    m_fileSize = m_ifstream.tellg();
    m_ifstream.seekg(0,std::ios_base::beg);

    m_sampleRate = 48000; // fixed rate
    m_recordLength = m_fileSize / (sizeof(Real) * m_sampleRate);

    qDebug() << "WFMMod::openFileStream: " << m_fileName.toStdString().c_str()
            << " fileSize: " << m_fileSize << "bytes"
            << " length: " << m_recordLength << " seconds";

    MsgReportFileSourceStreamData *report;
    report = MsgReportFileSourceStreamData::create(m_sampleRate, m_recordLength);
    getOutputMessageQueue()->push(report);
}

void WFMMod::seekFileStream(int seekPercentage)
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
