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
#include "boost/format.hpp"
#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGBFMDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGBFMDemodReport.h"
#include "SWGRDSReport.h"

#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/downchannelizer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"
#include "util/db.h"

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

    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(&m_audioFifo, getInputMessageQueue());
    m_audioSampleRate = DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate();

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

	m_deemphasisFilterX.configure(default_deemphasis * m_audioSampleRate * 1.0e-6);
	m_deemphasisFilterY.configure(default_deemphasis * m_audioSampleRate * 1.0e-6);
 	m_phaseDiscri.setFMScaling(384000/m_fmExcursion);

	m_audioBuffer.resize(16384);
	m_audioBufferFill = 0;

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
    applySettings(m_settings, true);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);
}

BFMDemod::~BFMDemod()
{
	DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(&m_audioFifo);

	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
    delete m_rfFilter;
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

            if (msq > m_magsqPeak) {
                m_magsqPeak = msq;
            }

            m_magsqCount++;

			if (msq >= m_squelchLevel)
			{
			    if (m_squelchState < m_settings.m_rfBandwidth / 10) { // twice attack and decay rate
			        m_squelchState++;
			    }
			}
			else
			{
			    if (m_squelchState > 0) {
			        m_squelchState--;
			    }
			}

			if (m_squelchState > m_settings.m_rfBandwidth / 20) { // squelch open
				demod = m_phaseDiscri.phaseDiscriminator(rf[i]);
			} else {
				demod = 0;
			}

			if (!m_settings.m_showPilot) {
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
						if (m_rdsDecoder.frameSync(bit)) {
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

				if (m_settings.m_showPilot) {
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
                    m_audioBuffer[m_audioBufferFill].l = (qint16)(deemph_l * (1<<12) * m_settings.m_volume);
                    m_audioBuffer[m_audioBufferFill].r = (qint16)(deemph_r * (1<<12) * m_settings.m_volume);
				}
				else
				{
					Real deemph;
					m_deemphasisFilterX.process(ci.real(), deemph);
					quint16 sample = (qint16)(deemph * (1<<12) * m_settings.m_volume);
					m_audioBuffer[m_audioBufferFill].l = sample;
					m_audioBuffer[m_audioBufferFill].r = sample;
				}

				++m_audioBufferFill;

				if (m_audioBufferFill >= m_audioBuffer.size())
				{
					uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);

					if(res != m_audioBufferFill) {
						qDebug("BFMDemod::feed: %u/%u audio samples written", res, m_audioBufferFill);
					}

					m_audioBufferFill = 0;
				}

				m_interpolatorDistanceRemain += m_interpolatorDistance;
			}
		}
	}

	if (m_audioBufferFill > 0)
	{
		uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);

		if (res != m_audioBufferFill) {
			qDebug("BFMDemod::feed: %u/%u tail samples written", res, m_audioBufferFill);
		}

		m_audioBufferFill = 0;
	}

	if (m_sampleSink != 0) {
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
    else if (DSPConfigureAudio::match(cmd))
    {
        DSPConfigureAudio& cfg = (DSPConfigureAudio&) cmd;
        uint32_t sampleRate = cfg.getSampleRate();

        qDebug() << "BFMDemod::handleMessage: DSPConfigureAudio:"
                << " sampleRate: " << sampleRate;

        if (sampleRate != m_audioSampleRate) {
            applyAudioSampleRate(sampleRate);
        }

        return true;
    }
    else if (BasebandSampleSink::MsgThreadedSink::match(cmd))
    {
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
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

void BFMDemod::applyAudioSampleRate(int sampleRate)
{
    qDebug("BFMDemod::applyAudioSampleRate: %d", sampleRate);

    m_settingsMutex.lock();

    m_interpolator.create(16, m_inputSampleRate, m_settings.m_afBandwidth);
    m_interpolatorDistanceRemain = (Real) m_inputSampleRate / sampleRate;
    m_interpolatorDistance =  (Real) m_inputSampleRate / (Real) sampleRate;

    m_interpolatorStereo.create(16, m_inputSampleRate, m_settings.m_afBandwidth);
    m_interpolatorStereoDistanceRemain = (Real) m_inputSampleRate / sampleRate;
    m_interpolatorStereoDistance =  (Real) m_inputSampleRate / (Real) sampleRate;

    m_deemphasisFilterX.configure(default_deemphasis * sampleRate * 1.0e-6);
    m_deemphasisFilterY.configure(default_deemphasis * sampleRate * 1.0e-6);

    m_settingsMutex.unlock();

    m_audioSampleRate = sampleRate;
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
        m_interpolatorDistanceRemain = (Real) inputSampleRate / m_audioSampleRate;
        m_interpolatorDistance =  (Real) inputSampleRate / (Real) m_audioSampleRate;

        m_interpolatorStereo.create(16, inputSampleRate, m_settings.m_afBandwidth);
        m_interpolatorStereoDistanceRemain = (Real) inputSampleRate / m_audioSampleRate;
        m_interpolatorStereoDistance =  (Real) inputSampleRate / (Real) m_audioSampleRate;

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
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioStereo: " << settings.m_audioStereo
            << " m_lsbStereo: " << settings.m_lsbStereo
            << " m_showPilot: " << settings.m_showPilot
            << " m_rdsActive: " << settings.m_rdsActive
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " force: " << force;

    if ((settings.m_audioStereo && (settings.m_audioStereo != m_settings.m_audioStereo)) || force)
    {
        m_pilotPLL.configure(19000.0/m_inputSampleRate, 50.0/m_inputSampleRate, 0.01);
    }

    if((settings.m_afBandwidth != m_settings.m_afBandwidth) || force)
    {
        m_settingsMutex.lock();

        m_interpolator.create(16, m_inputSampleRate, settings.m_afBandwidth);
        m_interpolatorDistanceRemain = (Real) m_inputSampleRate / m_audioSampleRate;
        m_interpolatorDistance =  (Real) m_inputSampleRate / (Real) m_audioSampleRate;

        m_interpolatorStereo.create(16, m_inputSampleRate, settings.m_afBandwidth);
        m_interpolatorStereoDistanceRemain = (Real) m_inputSampleRate / m_audioSampleRate;
        m_interpolatorStereoDistance =  (Real) m_inputSampleRate / (Real) m_audioSampleRate;

        m_interpolatorRDS.create(4, m_inputSampleRate, 600.0);
        m_interpolatorRDSDistanceRemain = (Real) m_inputSampleRate / 250000.0;
        m_interpolatorRDSDistance =  (Real) m_inputSampleRate / 250000.0;

        m_settingsMutex.unlock();
    }

    if((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_settingsMutex.lock();
        Real lowCut = -(settings.m_rfBandwidth / 2.0) / m_inputSampleRate;
        Real hiCut  = (settings.m_rfBandwidth / 2.0) / m_inputSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_phaseDiscri.setFMScaling(m_inputSampleRate / m_fmExcursion);
        m_settingsMutex.unlock();
    }

    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) || force)
    {
        m_settingsMutex.lock();
        qDebug() << "BFMDemod::handleMessage: m_lowpass.create";
        m_lowpass.create(21, m_audioSampleRate, settings.m_afBandwidth);
        m_settingsMutex.unlock();
    }

    if ((settings.m_squelch != m_settings.m_squelch) || force)
    {
        qDebug() << "BFMDemod::handleMessage: set m_squelchLevel";
        m_squelchLevel = std::pow(10.0, settings.m_squelch / 10.0);
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_audioDeviceName);
        //qDebug("AMDemod::applySettings: audioDeviceName: %s audioDeviceIndex: %d", qPrintable(settings.m_audioDeviceName), audioDeviceIndex);
        audioDeviceManager->addAudioSink(&m_audioFifo, getInputMessageQueue(), audioDeviceIndex);
        uint32_t audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);

        if (m_audioSampleRate != audioSampleRate) {
            applyAudioSampleRate(audioSampleRate);
        }
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

int BFMDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setBfmDemodSettings(new SWGSDRangel::SWGBFMDemodSettings());
    response.getBfmDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int BFMDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    BFMDemodSettings settings = m_settings;
    bool frequencyOffsetChanged = false;

    if (channelSettingsKeys.contains("inputFrequencyOffset"))
    {
        settings.m_inputFrequencyOffset = response.getBfmDemodSettings()->getInputFrequencyOffset();
        frequencyOffsetChanged = true;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getBfmDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("afBandwidth")) {
        settings.m_afBandwidth = response.getBfmDemodSettings()->getAfBandwidth();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getBfmDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getBfmDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("audioStereo")) {
        settings.m_audioStereo = response.getBfmDemodSettings()->getAudioStereo() != 0;
    }
    if (channelSettingsKeys.contains("lsbStereo")) {
        settings.m_lsbStereo = response.getBfmDemodSettings()->getLsbStereo() != 0;
    }
    if (channelSettingsKeys.contains("showPilot")) {
        settings.m_showPilot = response.getBfmDemodSettings()->getShowPilot() != 0;
    }
    if (channelSettingsKeys.contains("rdsActive")) {
        settings.m_rdsActive = response.getBfmDemodSettings()->getRdsActive() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getBfmDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getBfmDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getBfmDemodSettings()->getAudioDeviceName();
    }

    if (frequencyOffsetChanged)
    {
        MsgConfigureChannelizer* channelConfigMsg = MsgConfigureChannelizer::create(
                requiredBW(settings.m_rfBandwidth), settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(channelConfigMsg);
    }

    MsgConfigureBFMDemod *msg = MsgConfigureBFMDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("BFMDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBFMDemod *msgToGUI = MsgConfigureBFMDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int BFMDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setBfmDemodReport(new SWGSDRangel::SWGBFMDemodReport());
    response.getBfmDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void BFMDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const BFMDemodSettings& settings)
{
    response.getBfmDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getBfmDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getBfmDemodSettings()->setAfBandwidth(settings.m_afBandwidth);
    response.getBfmDemodSettings()->setVolume(settings.m_volume);
    response.getBfmDemodSettings()->setSquelch(settings.m_squelch);
    response.getBfmDemodSettings()->setAudioStereo(settings.m_audioStereo ? 1 : 0);
    response.getBfmDemodSettings()->setLsbStereo(settings.m_lsbStereo ? 1 : 0);
    response.getBfmDemodSettings()->setShowPilot(settings.m_showPilot ? 1 : 0);
    response.getBfmDemodSettings()->setRdsActive(settings.m_rdsActive ? 1 : 0);
    response.getBfmDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getBfmDemodSettings()->getTitle()) {
        *response.getBfmDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getBfmDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getBfmDemodSettings()->getAudioDeviceName()) {
        *response.getBfmDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getBfmDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
}

void BFMDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getBfmDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getBfmDemodReport()->setSquelch(m_squelchState > 0 ? 1 : 0);
    response.getBfmDemodReport()->setAudioSampleRate(m_audioSampleRate);
    response.getBfmDemodReport()->setChannelSampleRate(m_inputSampleRate);
    response.getBfmDemodReport()->setPilotLocked(getPilotLock() ? 1 : 0);
    response.getBfmDemodReport()->setPilotPowerDb(CalcDb::dbPower(getPilotLevel()));

    if (m_settings.m_rdsActive)
    {
        response.getBfmDemodReport()->setRdsReport(new SWGSDRangel::SWGRDSReport());
        webapiFormatRDSReport(response.getBfmDemodReport()->getRdsReport());
    }
    else
    {
        response.getBfmDemodReport()->setRdsReport(0);
    }
}

void BFMDemod::webapiFormatRDSReport(SWGSDRangel::SWGRDSReport *report)
{
    report->setDemodStatus(round(getDemodQua()));
    report->setDecodStatus(round(getDecoderQua()));
    report->setRdsDemodAccumDb(CalcDb::dbPower(std::fabs(getDemodAcc())));
    report->setRdsDemodFrequency(getDemodFclk());
    report->setPid(new QString(str(boost::format("%04X") % getRDSParser().m_pi_program_identification).c_str()));
    report->setPiType(new QString(getRDSParser().pty_table[getRDSParser().m_pi_program_type].c_str()));
    report->setPiCoverage(new QString(getRDSParser().coverage_area_codes[getRDSParser().m_pi_area_coverage_index].c_str()));
    report->setProgServiceName(new QString(getRDSParser().m_g0_program_service_name));
    report->setMusicSpeech(new QString((getRDSParser().m_g0_music_speech ? "Music" : "Speech")));
    report->setMonoStereo(new QString((getRDSParser().m_g0_mono_stereo ? "Mono" : "Stereo")));
    report->setRadioText(new QString(getRDSParser().m_g2_radiotext));
    std::string time = str(boost::format("%4i-%02i-%02i %02i:%02i (%+.1fh)")\
        % (1900 + getRDSParser().m_g4_year) % getRDSParser().m_g4_month % getRDSParser().m_g4_day % getRDSParser().m_g4_hours % getRDSParser().m_g4_minutes % getRDSParser().m_g4_local_time_offset);
    report->setTime(new QString(time.c_str()));
    report->setAltFrequencies(new QList<SWGSDRangel::SWGRDSReport_altFrequencies*>);

    for (std::set<double>::iterator it = getRDSParser().m_g0_alt_freq.begin(); it != getRDSParser().m_g0_alt_freq.end(); ++it)
    {
        if (*it > 76.0)
        {
            report->getAltFrequencies()->append(new SWGSDRangel::SWGRDSReport_altFrequencies);
            report->getAltFrequencies()->back()->setFrequency(*it);
        }
    }
}
