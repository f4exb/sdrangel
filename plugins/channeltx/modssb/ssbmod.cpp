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
#include "util/db.h"

MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureSSBMod, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureAFInput, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(SSBMod::MsgReportFileSourceStreamTiming, Message)

const int SSBMod::m_levelNbSamples = 480; // every 10ms
const int SSBMod::m_ssbFftLen = 1024;

SSBMod::SSBMod(BasebandSampleSink* sampleSink) :
    m_SSBFilter(0),
    m_DSBFilter(0),
	m_SSBFilterBuffer(0),
	m_DSBFilterBuffer(0),
	m_SSBFilterBufferIndex(0),
	m_DSBFilterBufferIndex(0),
    m_sampleSink(sampleSink),
    m_movingAverage(40, 0),
    m_audioFifo(4800),
	m_settingsMutex(QMutex::Recursive),
	m_fileSize(0),
	m_recordLength(0),
	m_sampleRate(48000),
	m_afInput(SSBModInputNone),
	m_levelCalcCount(0),
	m_peakLevel(0.0f),
	m_levelSum(0.0f),
	m_inAGC(9600, 0.2, 1e-4)
{
	setObjectName("SSBMod");

    m_SSBFilter = new fftfilt(m_config.m_lowCutoff / m_config.m_audioSampleRate, m_config.m_bandwidth / m_config.m_audioSampleRate, m_ssbFftLen);
    m_DSBFilter = new fftfilt((2.0f * m_config.m_bandwidth) / m_config.m_audioSampleRate, 2 * m_ssbFftLen);
    m_SSBFilterBuffer = new Complex[m_ssbFftLen>>1]; // filter returns data exactly half of its size
    m_DSBFilterBuffer = new Complex[m_ssbFftLen];
    memset(m_SSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen>>1));
    memset(m_DSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen));

    m_config.m_outputSampleRate = 48000;
	m_config.m_inputFrequencyOffset = 0;
	m_config.m_bandwidth = 12500;
	m_config.m_toneFrequency = 1000.0f;
	m_config.m_audioSampleRate = DSPEngine::instance()->getAudioSampleRate();

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

//    m_magsqSpectrum = 0.0f;
//    m_magsqSum = 0.0f;
//    m_magsqPeak = 0.0f;
//    m_magsqCount = 0;
    m_sum.real(0.0f);
    m_sum.imag(0.0f);
    m_undersampleCount = 0;
    m_sumCount = 0;

	m_movingAverage.resize(16, 0);
	m_magsq = 0.0;

	m_toneNco.setFreq(1000.0, m_config.m_audioSampleRate);
	DSPEngine::instance()->addAudioSource(&m_audioFifo);

	// CW keyer
	m_cwKeyer.setSampleRate(m_config.m_audioSampleRate);
	m_cwKeyer.setWPM(13);
	m_cwKeyer.setMode(CWKeyer::CWNone);

	m_cwSmoother.setNbFadeSamples(192); // 4 ms at 48 kHz
	m_inAGC.setGate(m_config.m_agcThresholdGate);
	m_inAGC.setStepDownDelay(m_config.m_agcThresholdDelay);
	m_inAGC.setClamping(true);
    apply();
}

SSBMod::~SSBMod()
{
    if (m_SSBFilter) {
        delete m_SSBFilter;
    }

    if (m_DSBFilter) {
        delete m_DSBFilter;
    }

    if (m_SSBFilterBuffer) {
        delete m_SSBFilterBuffer;
    }

    if (m_DSBFilterBuffer) {
        delete m_DSBFilterBuffer;
    }

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
		bool playLoop,
		bool agc,
		float agcOrder,
		int agcTime,
		int agcThreshold,
		int agcThresholdGate,
		int agcThresholdDelay)
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
			playLoop,
			agc,
			agcOrder,
			agcTime,
			agcThreshold,
			agcThresholdGate,
			agcThresholdDelay);
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
    ci *= 29204.0f; //scaling at -1 dB to account for possible filter overshoot

    m_settingsMutex.unlock();

    Real magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
	magsq /= (1<<30);
	m_movingAverage.feed(magsq);
	m_magsq = m_movingAverage.average();

	sample.m_real = (FixReal) ci.real();
	sample.m_imag = (FixReal) ci.imag();
}

void SSBMod::pullAudio(int nbSamples)
{
    unsigned int nbSamplesAudio = nbSamples * ((Real) m_config.m_audioSampleRate / (Real) m_config.m_basebandSampleRate);

    if (nbSamplesAudio > m_audioBuffer.size())
    {
        m_audioBuffer.resize(nbSamplesAudio);
    }

    m_audioFifo.read(reinterpret_cast<quint8*>(&m_audioBuffer[0]), nbSamplesAudio, 10);
    m_audioBufferFill = 0;
}

void SSBMod::modulateSample()
{
    pullAF(m_modSample);
    calculateLevel(m_modSample);
    m_audioBufferFill++;
}

void SSBMod::pullAF(Complex& sample)
{
	if (m_running.m_audioMute)
	{
        sample.real(0.0f);
        sample.imag(0.0f);
        return;
	}

    Complex ci;
    fftfilt::cmplx *filtered;
    int n_out = 0;

    int decim = 1<<(m_running.m_spanLog2 - 1);
    unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

    switch (m_afInput)
    {
    case SSBModInputTone:
    	if (m_running.m_dsb)
    	{
    		Real t = m_toneNco.next()/1.25;
    		sample.real(t);
    		sample.imag(t);
    	}
    	else
    	{
    		if (m_running.m_usb) {
    			sample = m_toneNco.nextIQ();
    		} else {
    			sample = m_toneNco.nextQI();
    		}
    	}
        break;
    case SSBModInputFile:
    	// Monaural (mono):
        // sox f4exb_call.wav --encoding float --endian little f4exb_call.raw
        // ffplay -f f32le -ar 48k -ac 1 f4exb_call.raw
    	// Binaural (stereo):
        // sox f4exb_call.wav --encoding float --endian little f4exb_call.raw
        // ffplay -f f32le -ar 48k -ac 2 f4exb_call.raw
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
                ci.real(0.0f);
                ci.imag(0.0f);
            }
            else
            {
            	if (m_running.m_audioBinaural)
            	{
            		Complex c;
                	m_ifstream.read(reinterpret_cast<char*>(&c), sizeof(Complex));

                	if (m_running.m_audioFlipChannels)
                	{
                        ci.real(c.imag() * m_running.m_volumeFactor);
                        ci.imag(c.real() * m_running.m_volumeFactor);
                	}
                	else
                	{
                    	ci = c * m_running.m_volumeFactor;
                	}
            	}
            	else
            	{
                    Real real;
                	m_ifstream.read(reinterpret_cast<char*>(&real), sizeof(Real));

                	if (m_running.m_agc)
                	{
                        ci.real(real);
                        ci.imag(0.0f);
                        m_inAGC.feed(ci);
                        ci *= m_running.m_volumeFactor;
                	}
                	else
                	{
                        ci.real(real * m_running.m_volumeFactor);
                        ci.imag(0.0f);
                	}
            	}
            }
        }
        else
        {
            ci.real(0.0f);
            ci.imag(0.0f);
        }
        break;
    case SSBModInputAudio:
        if (m_running.m_audioBinaural)
    	{
        	if (m_running.m_audioFlipChannels)
        	{
                ci.real((m_audioBuffer[m_audioBufferFill].r / 32768.0f) * m_running.m_volumeFactor);
                ci.imag((m_audioBuffer[m_audioBufferFill].l / 32768.0f) * m_running.m_volumeFactor);
        	}
        	else
        	{
                ci.real((m_audioBuffer[m_audioBufferFill].l / 32768.0f) * m_running.m_volumeFactor);
                ci.imag((m_audioBuffer[m_audioBufferFill].r / 32768.0f) * m_running.m_volumeFactor);
        	}
    	}
        else
        {
            if (m_running.m_agc)
            {
                ci.real(((m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r)  / 65536.0f));
                ci.imag(0.0f);
                m_inAGC.feed(ci);
                ci *= m_running.m_volumeFactor;
            }
            else
            {
                ci.real(((m_audioBuffer[m_audioBufferFill].l + m_audioBuffer[m_audioBufferFill].r)  / 65536.0f) * m_running.m_volumeFactor);
                ci.imag(0.0f);
            }
        }

        break;
    case SSBModInputCWTone:
    	Real fadeFactor;

        if (m_cwKeyer.getSample())
        {
            m_cwSmoother.getFadeSample(true, fadeFactor);

        	if (m_running.m_dsb)
        	{
        		Real t = m_toneNco.next() * fadeFactor;
        		sample.real(t);
        		sample.imag(t);
        	}
        	else
        	{
        		if (m_running.m_usb) {
        			sample = m_toneNco.nextIQ() * fadeFactor;
        		} else {
        			sample = m_toneNco.nextQI() * fadeFactor;
        		}
        	}
        }
        else
        {
        	if (m_cwSmoother.getFadeSample(false, fadeFactor))
        	{
            	if (m_running.m_dsb)
            	{
            		Real t = (m_toneNco.next() * fadeFactor)/1.25;
            		sample.real(t);
            		sample.imag(t);
            	}
            	else
            	{
            		if (m_running.m_usb) {
            			sample = m_toneNco.nextIQ() * fadeFactor;
            		} else {
            			sample = m_toneNco.nextQI() * fadeFactor;
            		}
            	}
        	}
        	else
        	{
                sample.real(0.0f);
                sample.imag(0.0f);
                m_toneNco.setPhase(0);
        	}
        }

        break;
    case SSBModInputNone:
    default:
        break;
    }

    if ((m_afInput == SSBModInputFile) || (m_afInput == SSBModInputAudio)) // real audio
    {
    	if (m_running.m_dsb)
    	{
    		n_out = m_DSBFilter->runDSB(ci, &filtered);

    		if (n_out > 0)
    		{
    			memcpy((void *) m_DSBFilterBuffer, (const void *) filtered, n_out*sizeof(Complex));
    			m_DSBFilterBufferIndex = 0;
    		}

    		sample = m_DSBFilterBuffer[m_DSBFilterBufferIndex];
    		m_DSBFilterBufferIndex++;
    	}
    	else
    	{
    		n_out = m_SSBFilter->runSSB(ci, &filtered, m_running.m_usb);

    		if (n_out > 0)
    		{
    			memcpy((void *) m_SSBFilterBuffer, (const void *) filtered, n_out*sizeof(Complex));
    			m_SSBFilterBufferIndex = 0;
    		}

    		sample = m_SSBFilterBuffer[m_SSBFilterBufferIndex];
    		m_SSBFilterBufferIndex++;
    	}

    	if (n_out > 0)
    	{
            for (int i = 0; i < n_out; i++)
            {
                // Downsample by 2^(m_scaleLog2 - 1) for SSB band spectrum display
                // smart decimation with bit gain using float arithmetic (23 bits significand)

                m_sum += filtered[i];

                if (!(m_undersampleCount++ & decim_mask))
                {
                    Real avgr = (m_sum.real() / decim) * 29204.0f; //scaling at -1 dB to account for possible filter overshoot
                    Real avgi = (m_sum.imag() / decim) * 29204.0f;
//                    m_magsqSpectrum = (avgr * avgr + avgi * avgi) / (1<<30);
//
//                    m_magsqSum += m_magsqSpectrum;
//
//                    if (m_magsqSpectrum > m_magsqPeak)
//                    {
//                        m_magsqPeak = m_magsqSpectrum;
//                    }
//
//                    m_magsqCount++;

                    if (!m_running.m_dsb & !m_running.m_usb)
                    { // invert spectrum for LSB
                        m_sampleBuffer.push_back(Sample(avgi, avgr));
                    }
                    else
                    {
                        m_sampleBuffer.push_back(Sample(avgr, avgi));
                    }

                    m_sum.real(0.0);
                    m_sum.imag(0.0);
                }
            }
    	}
    } // Real audio
    else if ((m_afInput == SSBModInputTone) || (m_afInput == SSBModInputCWTone)) // tone
    {
        m_sum += sample;

        if (!(m_undersampleCount++ & decim_mask))
        {
            Real avgr = (m_sum.real() / decim) * 29204.0f; //scaling at -1 dB to account for possible filter overshoot
            Real avgi = (m_sum.imag() / decim) * 29204.0f;
//            m_magsqSpectrum = (avgr * avgr + avgi * avgi) / (1<<30);
//
//            m_magsqSum += m_magsqSpectrum;
//
//            if (m_magsqSpectrum > m_magsqPeak)
//            {
//                m_magsqPeak = m_magsqSpectrum;
//            }
//
//            m_magsqCount++;

            if (!m_running.m_dsb & !m_running.m_usb)
            { // invert spectrum for LSB
                m_sampleBuffer.push_back(Sample(avgi, avgr));
            }
            else
            {
                m_sampleBuffer.push_back(Sample(avgr, avgi));
            }

            m_sum.real(0.0);
            m_sum.imag(0.0);
        }

        if (m_sumCount < (m_running.m_dsb ? m_ssbFftLen : m_ssbFftLen>>1))
        {
            n_out = 0;
            m_sumCount++;
        }
        else
        {
            n_out = m_sumCount;
            m_sumCount = 0;
        }
    }

    if (n_out > 0)
    {
        if (m_sampleSink != 0)
        {
            m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), !m_running.m_dsb);
        }

        m_sampleBuffer.clear();
    }
}

void SSBMod::calculateLevel(Complex& sample)
{
    Real t = sample.real(); // TODO: possibly adjust depending on sample type

    if (m_levelCalcCount < m_levelNbSamples)
    {
        m_peakLevel = std::max(std::fabs(m_peakLevel), t);
        m_levelSum += t * t;
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

		m_config.m_basebandSampleRate = notif.getBasebandSampleRate();
		m_config.m_outputSampleRate = notif.getSampleRate();
		m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

		apply();

		qDebug() << "SSBMod::handleMessage: MsgChannelizerNotification:"
				<< " m_basebandSampleRate: " << m_config.m_basebandSampleRate
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
		lowCutoff = cfg.getLowCutoff();

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

		m_config.m_toneFrequency = cfg.getToneFrequency();
		m_config.m_volumeFactor = cfg.getVolumeFactor();
		m_config.m_spanLog2 = cfg.getSpanLog2();
		m_config.m_audioBinaural = cfg.getAudioBinaural();
		m_config.m_audioFlipChannels = cfg.getAudioFlipChannels();
		m_config.m_dsb = cfg.getDSB();
		m_config.m_audioMute = cfg.getAudioMute();
		m_config.m_playLoop = cfg.getPlayLoop();
		m_config.m_agc = cfg.getAGC();

		m_config.m_agcTime = cfg.getAGCTime(); // ms
		m_config.m_agcOrder = cfg.getAGCOrder();
		m_config.m_agcThresholdEnable = cfg.getAGCThreshold() != -99;
		m_config.m_agcThreshold = CalcDb::powerFromdB(cfg.getAGCThreshold()); // power dB
		m_config.m_agcThresholdGate = cfg.getAGCThresholdGate(); // ms
		m_config.m_agcThresholdDelay = cfg.getAGCThresholdDelay(); // ms

//        m_config.m_agcTime = 48 * cfg.getAGCTime(); // ms
//        m_config.m_agcOrder = cfg.getAGCOrder();
//        m_config.m_agcThresholdEnable = cfg.getAGCThreshold() != -99;
//        m_config.m_agcThreshold = CalcDb::powerFromdB(cfg.getAGCThreshold()); // power dB
//        m_config.m_agcThresholdGate = 48 * cfg.getAGCThresholdGate(); // ms
//        m_config.m_agcThresholdDelay = 48 * cfg.getAGCThresholdDelay(); // ms

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
				<< " m_playLoop: " << m_config.m_playLoop
				<< " m_agc: " << m_config.m_agc
				<< " m_agcTime: " << m_config.m_agcTime
                << " m_agcOrder: " << m_config.m_agcOrder
                << " m_agcThresholdEnable: " << m_config.m_agcThresholdEnable
                << " m_agcThreshold: " << m_config.m_agcThreshold
                << " m_agcThresholdGate: " << m_config.m_agcThresholdGate
                << " m_agcThresholdDelay: " << m_config.m_agcThresholdDelay;

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
        getMessageQueueToGUI()->push(report);

        return true;
    }
	else
	{
		return false;
	}
}

void SSBMod::apply()
{
    if ((m_config.m_bandwidth != m_running.m_bandwidth) ||
        (m_config.m_lowCutoff != m_running.m_lowCutoff) ||
        (m_config.m_audioSampleRate != m_running.m_audioSampleRate))
    {
        m_settingsMutex.lock();
        m_SSBFilter->create_filter(m_config.m_lowCutoff / m_config.m_audioSampleRate, m_config.m_bandwidth / m_config.m_audioSampleRate);
        m_DSBFilter->create_dsb_filter((2.0f * m_config.m_bandwidth) / m_config.m_audioSampleRate);
        m_settingsMutex.unlock();
    }

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
        m_interpolator.create(48, m_config.m_audioSampleRate, m_config.m_bandwidth, 3.0);
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
        m_settingsMutex.lock();
	    m_cwKeyer.setSampleRate(m_config.m_audioSampleRate);
	    m_cwSmoother.setNbFadeSamples(m_config.m_audioSampleRate / 250); // 4 ms
        m_settingsMutex.unlock();
	}

	if (m_config.m_dsb != m_running.m_dsb)
	{
		if (m_config.m_dsb)
		{
			memset(m_DSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen));
			m_DSBFilterBufferIndex = 0;
		}
		else
		{
			memset(m_SSBFilterBuffer, 0, sizeof(Complex)*(m_ssbFftLen>>1));
			m_SSBFilterBufferIndex = 0;
		}
	}

    if ((m_config.m_agcTime != m_running.m_agcTime) || (m_config.m_agcOrder != m_running.m_agcOrder))
    {
        m_inAGC.resize(m_config.m_agcTime, m_config.m_agcOrder);
    }

    if (m_config.m_agcThresholdEnable != m_running.m_agcThresholdEnable)
    {
        m_inAGC.setThresholdEnable(m_config.m_agcThresholdEnable);
    }

    if (m_config.m_agcThreshold != m_running.m_agcThreshold)
    {
        m_inAGC.setThreshold(m_config.m_agcThreshold);
    }

    if (m_config.m_agcThresholdGate != m_running.m_agcThresholdGate)
    {
        m_inAGC.setGate(m_config.m_agcThresholdGate);
    }

    if (m_config.m_agcThresholdDelay != m_running.m_agcThresholdDelay)
    {
        m_inAGC.setStepDownDelay(m_config.m_agcThresholdDelay);
    }

	m_running.m_outputSampleRate = m_config.m_outputSampleRate;
	m_running.m_inputFrequencyOffset = m_config.m_inputFrequencyOffset;
	m_running.m_bandwidth = m_config.m_bandwidth;
	m_running.m_lowCutoff = m_config.m_lowCutoff;
	m_running.m_usb = m_config.m_usb;
	m_running.m_toneFrequency = m_config.m_toneFrequency;
    m_running.m_volumeFactor = m_config.m_volumeFactor;
	m_running.m_audioSampleRate = m_config.m_audioSampleRate;
	m_running.m_spanLog2 = m_config.m_spanLog2;
	m_running.m_audioBinaural = m_config.m_audioBinaural;
	m_running.m_audioFlipChannels = m_config.m_audioFlipChannels;
	m_running.m_dsb = m_config.m_dsb;
	m_running.m_audioMute = m_config.m_audioMute;
	m_running.m_playLoop = m_config.m_playLoop;
	m_running.m_agc = m_config.m_agc;
    m_running.m_agcOrder = m_config.m_agcOrder;
    m_running.m_agcTime = m_config.m_agcTime;
    m_running.m_agcThresholdEnable = m_config.m_agcThresholdEnable;
    m_running.m_agcThreshold = m_config.m_agcThreshold;
    m_running.m_agcThresholdGate = m_config.m_agcThresholdGate;
    m_running.m_agcThresholdDelay = m_config.m_agcThresholdDelay;
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

    qDebug() << "SSBMod::openFileStream: " << m_fileName.toStdString().c_str()
            << " fileSize: " << m_fileSize << "bytes"
            << " length: " << m_recordLength << " seconds";

    MsgReportFileSourceStreamData *report;
    report = MsgReportFileSourceStreamData::create(m_sampleRate, m_recordLength);
    getMessageQueueToGUI()->push(report);
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
