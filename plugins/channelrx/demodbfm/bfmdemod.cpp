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
#include "dsp/pidcontroller.h"
#include <dsp/downchannelizer.h>
#include "dsp/threadedbasebandsamplesink.h"
#include "device/devicesourceapi.h"

#include "rdsparser.h"
#include "bfmdemod.h"

MESSAGE_CLASS_DEFINITION(BFMDemod::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(BFMDemod::MsgReportChannelSampleRateChanged, Message)
MESSAGE_CLASS_DEFINITION(BFMDemod::MsgConfigureBFMDemod, Message)

const QString BFMDemod::m_channelIdURI = "sdrangel.channel.bfm";
const QString BFMDemod::m_channelId = "BFMDemod";
const Real BFMDemod::default_deemphasis = 50.0; // 50 us
const int BFMDemod::m_udpBlockSize = 512;

BFMDemod::BFMDemod(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_inputSampleRate(384000),
        m_inputFrequencyOffset(0),
        m_audioFifo(250000),
        m_settingsMutex(QMutex::Recursive),
        m_pilotPLL(19000/384000, 50/384000, 0.01),
        m_deemphasisFilterX(default_deemphasis * 48000 * 1.0e-6),
        m_deemphasisFilterY(default_deemphasis * 48000 * 1.0e-6),
	m_fmExcursion(default_excursion)
{
	setObjectName(m_channelId);

    m_magsq = 0.0f;
    m_magsqSum = 0.0f;
    m_magsqPeak = 0.0f;
    m_magsqCount = 0;

    m_squelchLevel = 0;
    m_squelchState = 0;

    m_interpolatorDistance = 0.0f;
    m_interpolatorDistanceRemain = 0.0f;

    m_interpolatorRDSDistance = 0.0f;
    m_interpolatorRDSDistanceRemain = 0.0f;

    m_interpolatorStereoDistance = 0.0f;
    m_interpolatorStereoDistanceRemain = 0.0f;

    m_sampleSink = 0;
    m_m1Arg = 0;

    m_rfFilter = new fftfilt(-50000.0 / 384000.0, 50000.0 / 384000.0, filtFftLen);


	m_deemphasisFilterX.configure(default_deemphasis * m_settings.m_audioSampleRate * 1.0e-6);
	m_deemphasisFilterY.configure(default_deemphasis * m_settings.m_audioSampleRate * 1.0e-6);
 	m_phaseDiscri.setFMScaling(384000/m_fmExcursion);

	m_audioBuffer.resize(16384);
	m_audioBufferFill = 0;

	DSPEngine::instance()->addAudioSink(&m_audioFifo);
    m_udpBufferAudio = new UDPSink<AudioSample>(this, m_udpBlockSize, m_settings.m_udpPort);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
    applySettings(m_settings, true);
}

BFMDemod::~BFMDemod()
{
	if (m_rfFilter)
	{
		delete m_rfFilter;
	}

	DSPEngine::instance()->removeAudioSink(&m_audioFifo);
	delete m_udpBufferAudio;

	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void BFMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
{
	Complex ci, cs, cr;
	fftfilt::cmplx *rf;
	int rf_out;
	double msq;
	Real demod;

	m_sampleBuffer.clear();

	m_settingsMutex.lock();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real() / SDR_RX_SCALEF, it->imag() / SDR_RX_SCALEF);
		c *= m_nco.nextIQ();

		rf_out = m_rfFilter->runFilt(c, &rf); // filter RF before demod

		for (int i =0 ; i  <rf_out; i++)
		{
			msq = rf[i].real()*rf[i].real() + rf[i].imag()*rf[i].imag();

            m_magsqSum += msq;

            if (msq > m_magsqPeak)
            {
                m_magsqPeak = msq;
            }

            m_magsqCount++;

//			m_movingAverage.feed(msq);

			if(m_magsq >= m_squelchLevel) {
				m_squelchState = m_settings.m_rfBandwidth / 20; // decay rate
			}

			if(m_squelchState > 0)
			{
				m_squelchState--;

				//demod = phaseDiscriminator2(rf[i], msq);
				demod = m_phaseDiscri.phaseDiscriminator(rf[i]);
			}
			else
			{
				demod = 0;
			}

			if (!m_settings.m_showPilot)
			{
				m_sampleBuffer.push_back(Sample(demod * SDR_RX_SCALEF, 0.0));
			}

			if (m_settings.m_rdsActive)
			{
				//Complex r(demod * 2.0 * std::cos(3.0 * m_pilotPLLSamples[3]), 0.0);
				Complex r(demod * 2.0 * std::cos(3.0 * m_pilotPLLSamples[3]), 0.0);

				if (m_interpolatorRDS.decimate(&m_interpolatorRDSDistanceRemain, r, &cr))
				{
					bool bit;

					if (m_rdsDemod.process(cr.real(), bit))
					{
						if (m_rdsDecoder.frameSync(bit))
						{
						    m_rdsParser.parseGroup(m_rdsDecoder.getGroup());
						}
					}

					m_interpolatorRDSDistanceRemain += m_interpolatorRDSDistance;
				}
			}

			Real sampleStereo = 0.0f;

			// Process stereo if stereo mode is selected

			if (m_settings.m_audioStereo)
			{
				m_pilotPLL.process(demod, m_pilotPLLSamples);

				if (m_settings.m_showPilot)
				{
					m_sampleBuffer.push_back(Sample(m_pilotPLLSamples[1] * SDR_RX_SCALEF, 0.0)); // debug 38 kHz pilot
				}

				if (m_settings.m_lsbStereo)
				{
					// 1.17 * 0.7 = 0.819
					Complex s(demod * m_pilotPLLSamples[1], demod * m_pilotPLLSamples[2]);

					if (m_interpolatorStereo.decimate(&m_interpolatorStereoDistanceRemain, s, &cs))
					{
						sampleStereo = cs.real() + cs.imag();
						m_interpolatorStereoDistanceRemain += m_interpolatorStereoDistance;
					}
				}
				else
				{
					Complex s(demod * 1.17 * m_pilotPLLSamples[1], 0);

					if (m_interpolatorStereo.decimate(&m_interpolatorStereoDistanceRemain, s, &cs))
					{
						sampleStereo = cs.real();
						m_interpolatorStereoDistanceRemain += m_interpolatorStereoDistance;
					}
				}
			}

			Complex e(demod, 0);

			if (m_interpolator.decimate(&m_interpolatorDistanceRemain, e, &ci))
			{
				if (m_settings.m_audioStereo)
				{
					Real deemph_l, deemph_r; // Pre-emphasis is applied on each channel before multiplexing
					m_deemphasisFilterX.process(ci.real() + sampleStereo, deemph_l);
					m_deemphasisFilterY.process(ci.real() - sampleStereo, deemph_r);
					if (m_settings.m_lsbStereo)
					{
						m_audioBuffer[m_audioBufferFill].l = (qint16)(deemph_l * (1<<12) * m_settings.m_volume);
						m_audioBuffer[m_audioBufferFill].r = (qint16)(deemph_r * (1<<12) * m_settings.m_volume);
						if (m_settings.m_copyAudioToUDP) m_udpBufferAudio->write(m_audioBuffer[m_audioBufferFill]);
					}
					else
					{
						m_audioBuffer[m_audioBufferFill].l = (qint16)(deemph_l * (1<<12) * m_settings.m_volume);
						m_audioBuffer[m_audioBufferFill].r = (qint16)(deemph_r * (1<<12) * m_settings.m_volume);
                        if (m_settings.m_copyAudioToUDP) m_udpBufferAudio->write(m_audioBuffer[m_audioBufferFill]);
					}
				}
				else
				{
					Real deemph;
					m_deemphasisFilterX.process(ci.real(), deemph);
					quint16 sample = (qint16)(deemph * (1<<12) * m_settings.m_volume);
					m_audioBuffer[m_audioBufferFill].l = sample;
					m_audioBuffer[m_audioBufferFill].r = sample;
                    if (m_settings.m_copyAudioToUDP) m_udpBufferAudio->write(m_audioBuffer[m_audioBufferFill]);
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
	m_phaseDiscri.reset();
    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
}

void BFMDemod::stop()
{
}

bool BFMDemod::handleMessage(const Message& cmd)
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

		qDebug() << "BFMDemod::handleMessage: MsgChannelizerNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " inputFrequencyOffset: " << notif.getFrequencyOffset();

        applyChannelSettings(notif.getSampleRate(), notif.getFrequencyOffset());

        if (getMessageQueueToGUI())
        {
            MsgReportChannelSampleRateChanged *msg = MsgReportChannelSampleRateChanged::create(getSampleRate());
            getMessageQueueToGUI()->push(msg);
        }

		return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

        qDebug() << "BFMDemod::handleMessage: MsgConfigureChannelizer: sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureBFMDemod::match(cmd))
    {
        MsgConfigureBFMDemod& cfg = (MsgConfigureBFMDemod&) cmd;
        qDebug() << "BFMDemod::handleMessage: MsgConfigureBFMDemod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
	else
	{
		qDebug() << "BFMDemod::handleMessage: passed: " << cmd.getIdentifier();

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

void BFMDemod::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "BFMDemod::applyChannelSettings:"
            << " inputSampleRate: " << inputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if((inputFrequencyOffset != m_inputFrequencyOffset) ||
        (inputSampleRate != m_inputSampleRate) || force)
    {
        m_nco.setFreq(-inputFrequencyOffset, inputSampleRate);
    }

    if ((inputSampleRate != m_inputSampleRate) || force)
    {
        m_pilotPLL.configure(19000.0/inputSampleRate, 50.0/inputSampleRate, 0.01);

        m_settingsMutex.lock();

        m_interpolator.create(16, inputSampleRate, m_settings.m_afBandwidth);
        m_interpolatorDistanceRemain = (Real) inputSampleRate / m_settings.m_audioSampleRate;
        m_interpolatorDistance =  (Real) inputSampleRate / (Real) m_settings.m_audioSampleRate;

        m_interpolatorStereo.create(16, inputSampleRate, m_settings.m_afBandwidth);
        m_interpolatorStereoDistanceRemain = (Real) inputSampleRate / m_settings.m_audioSampleRate;
        m_interpolatorStereoDistance =  (Real) inputSampleRate / (Real) m_settings.m_audioSampleRate;

        m_interpolatorRDS.create(4, inputSampleRate, 600.0);
        m_interpolatorRDSDistanceRemain = (Real) inputSampleRate / 250000.0;
        m_interpolatorRDSDistance =  (Real) inputSampleRate / 250000.0;

        Real lowCut = -(m_settings.m_rfBandwidth / 2.0) / inputSampleRate;
        Real hiCut  = (m_settings.m_rfBandwidth / 2.0) / inputSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_phaseDiscri.setFMScaling(inputSampleRate / m_fmExcursion);

        m_settingsMutex.unlock();
    }

    m_inputSampleRate = inputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void BFMDemod::applySettings(const BFMDemodSettings& settings, bool force)
{
    qDebug() << "BFMDemod::applySettings: MsgConfigureBFMDemod:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioStereo: " << settings.m_audioStereo
            << " m_lsbStereo: " << settings.m_lsbStereo
            << " m_showPilot: " << settings.m_showPilot
            << " m_rdsActive: " << settings.m_rdsActive
            << " m_copyAudioToUDP: " << settings.m_copyAudioToUDP
            << " m_udpAddress: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " force: " << force;

    if ((settings.m_audioStereo && (settings.m_audioStereo != m_settings.m_audioStereo)) || force)
    {
        m_pilotPLL.configure(19000.0/m_inputSampleRate, 50.0/m_inputSampleRate, 0.01);
    }

    if((settings.m_afBandwidth != m_settings.m_afBandwidth) || force)
    {
        m_settingsMutex.lock();

        m_interpolator.create(16, m_inputSampleRate, settings.m_afBandwidth);
        m_interpolatorDistanceRemain = (Real) m_inputSampleRate / settings.m_audioSampleRate;
        m_interpolatorDistance =  (Real) m_inputSampleRate / (Real) settings.m_audioSampleRate;

        m_interpolatorStereo.create(16, m_inputSampleRate, settings.m_afBandwidth);
        m_interpolatorStereoDistanceRemain = (Real) m_inputSampleRate / settings.m_audioSampleRate;
        m_interpolatorStereoDistance =  (Real) m_inputSampleRate / (Real) settings.m_audioSampleRate;

        m_interpolatorRDS.create(4, m_inputSampleRate, 600.0);
        m_interpolatorRDSDistanceRemain = (Real) m_inputSampleRate / 250000.0;
        m_interpolatorRDSDistance =  (Real) m_inputSampleRate / 250000.0;

        m_settingsMutex.unlock();
    }

    if((settings.m_rfBandwidth != m_settings.m_rfBandwidth) ||
       (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        m_settingsMutex.lock();
        Real lowCut = -(settings.m_rfBandwidth / 2.0) / m_inputSampleRate;
        Real hiCut  = (settings.m_rfBandwidth / 2.0) / m_inputSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_phaseDiscri.setFMScaling(m_inputSampleRate / m_fmExcursion);
        m_settingsMutex.unlock();
    }

    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) ||
        (settings.m_audioSampleRate != m_settings.m_audioSampleRate) || force)
    {
        m_settingsMutex.lock();
        qDebug() << "BFMDemod::handleMessage: m_lowpass.create";
        m_lowpass.create(21, settings.m_audioSampleRate, settings.m_afBandwidth);
        m_settingsMutex.unlock();
    }

    if ((settings.m_squelch != m_settings.m_squelch) || force)
    {
        qDebug() << "BFMDemod::handleMessage: set m_squelchLevel";
        m_squelchLevel = std::pow(10.0, settings.m_squelch / 20.0);
        m_squelchLevel *= m_squelchLevel;
    }

    if ((settings.m_audioSampleRate != m_settings.m_audioSampleRate) || force)
    {
        m_deemphasisFilterX.configure(default_deemphasis * settings.m_audioSampleRate * 1.0e-6);
        m_deemphasisFilterY.configure(default_deemphasis * settings.m_audioSampleRate * 1.0e-6);
    }

    if ((settings.m_udpAddress != m_settings.m_udpAddress)
        || (settings.m_udpPort != m_settings.m_udpPort) || force)
    {
        m_udpBufferAudio->setAddress(const_cast<QString&>(settings.m_udpAddress));
        m_udpBufferAudio->setPort(settings.m_udpPort);
    }

    m_settings = settings;
}

QByteArray BFMDemod::serialize() const
{
    return m_settings.serialize();
}

bool BFMDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureBFMDemod *msg = MsgConfigureBFMDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureBFMDemod *msg = MsgConfigureBFMDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

