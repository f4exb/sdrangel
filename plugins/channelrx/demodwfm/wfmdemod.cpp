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


#include <QTime>
#include <QDebug>
#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGWFMDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGWFMDemodReport.h"

#include <dsp/downchannelizer.h>
#include "dsp/threadedbasebandsamplesink.h"
#include "device/devicesourceapi.h"
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "util/db.h"

#include "wfmdemod.h"

MESSAGE_CLASS_DEFINITION(WFMDemod::MsgConfigureWFMDemod, Message)
MESSAGE_CLASS_DEFINITION(WFMDemod::MsgConfigureChannelizer, Message)

const QString WFMDemod::m_channelIdURI = "sdrangel.channel.wfmdemod";
const QString WFMDemod::m_channelId = "WFMDemod";
const int WFMDemod::m_udpBlockSize = 512;

WFMDemod::WFMDemod(DeviceSourceAPI* deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_inputSampleRate(384000),
        m_inputFrequencyOffset(0),
        m_squelchOpen(false),
        m_magsq(0.0f),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_audioFifo(250000),
        m_settingsMutex(QMutex::Recursive)
{
	setObjectName(m_channelId);

	m_rfFilter = new fftfilt(-50000.0 / 384000.0, 50000.0 / 384000.0, rfFilterFftLength);
	m_phaseDiscri.setFMScaling(384000/75000);

	m_audioBuffer.resize(16384);
	m_audioBufferFill = 0;

	DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(&m_audioFifo, getInputMessageQueue());
	m_audioSampleRate = DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate();

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
	applySettings(m_settings, true);

	m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);
}

WFMDemod::~WFMDemod()
{
	DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(&m_audioFifo);

	m_deviceAPI->removeChannelAPI(this);
	m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
    delete m_rfFilter;
}

void WFMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
{
	Complex ci;
	fftfilt::cmplx *rf;
	int rf_out;
	Real demod;
	double msq;
	float fmDev;

	m_settingsMutex.lock();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		rf_out = m_rfFilter->runFilt(c, &rf); // filter RF before demod

		for (int i = 0 ; i < rf_out; i++)
		{
		    msq = rf[i].real()*rf[i].real() + rf[i].imag()*rf[i].imag();
		    Real magsq = msq / (SDR_RX_SCALED*SDR_RX_SCALED);
		    m_magsqSum += magsq;
		    m_movingAverage(magsq);

            if (magsq > m_magsqPeak) {
                m_magsqPeak = magsq;
            }

            m_magsqCount++;

            if (magsq >= m_squelchLevel)
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

			m_squelchOpen = (m_squelchState > (m_settings.m_rfBandwidth / 20));

			if (m_squelchOpen && !m_settings.m_audioMute) { // squelch open and not mute
                demod = m_phaseDiscri.phaseDiscriminatorDelta(rf[i], msq, fmDev);
            } else {
                demod = 0;
            }

            Complex e(demod, 0);

			if (m_interpolator.decimate(&m_interpolatorDistanceRemain, e, &ci))
			{
				qint16 sample = (qint16)(ci.real() * 3276.8f * m_settings.m_volume);
				m_sampleBuffer.push_back(Sample(sample, sample));
				m_audioBuffer[m_audioBufferFill].l = sample;
				m_audioBuffer[m_audioBufferFill].r = sample;

				++m_audioBufferFill;

				if(m_audioBufferFill >= m_audioBuffer.size())
				{
					uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 1);

					if (res != m_audioBufferFill) {
						qDebug("WFMDemod::feed: %u/%u audio samples written", res, m_audioBufferFill);
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
			qDebug("WFMDemod::feed: %u/%u tail samples written", res, m_audioBufferFill);
		}

		m_audioBufferFill = 0;
	}

	m_sampleBuffer.clear();

	m_settingsMutex.unlock();
}

void WFMDemod::start()
{
	m_squelchState = 0;
	m_audioFifo.clear();
	m_phaseDiscri.reset();
	applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
}

void WFMDemod::stop()
{
}

bool WFMDemod::handleMessage(const Message& cmd)
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;
        qDebug() << "WFMDemod::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << notif.getSampleRate()
                << " m_inputFrequencyOffset: " << notif.getFrequencyOffset();

        applyChannelSettings(notif.getSampleRate(), notif.getFrequencyOffset());

		return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "WFMDemod::handleMessage: MsgConfigureChannelizer:"
                << " sampleRate: " << cfg.getSampleRate()
                << " inputFrequencyOffset: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureWFMDemod::match(cmd))
    {
        MsgConfigureWFMDemod& cfg = (MsgConfigureWFMDemod&) cmd;
        qDebug("WFMDemod::handleMessage: MsgConfigureWFMDemod");

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (BasebandSampleSink::MsgThreadedSink::match(cmd))
    {
        BasebandSampleSink::MsgThreadedSink& cfg = (BasebandSampleSink::MsgThreadedSink&) cmd;
        const QThread *thread = cfg.getThread();
        qDebug("WFMDemod::handleMessage: BasebandSampleSink::MsgThreadedSink: %p", thread);
        return true;
    }
    else if (DSPConfigureAudio::match(cmd))
    {
        DSPConfigureAudio& cfg = (DSPConfigureAudio&) cmd;
        uint32_t sampleRate = cfg.getSampleRate();

        qDebug() << "WFMDemod::handleMessage: DSPConfigureAudio:"
                << " sampleRate: " << sampleRate;

        if (sampleRate != m_audioSampleRate) {
            applyAudioSampleRate(sampleRate);
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        return true;
    }
	else
	{
		return false;
	}
}

void WFMDemod::applyAudioSampleRate(int sampleRate)
{
    qDebug("WFMDemod::applyAudioSampleRate: %d", sampleRate);

    m_settingsMutex.lock();

    m_interpolator.create(16, m_inputSampleRate, m_settings.m_afBandwidth);
    m_interpolatorDistanceRemain = (Real) m_inputSampleRate / sampleRate;
    m_interpolatorDistance =  (Real) m_inputSampleRate / (Real) sampleRate;

    m_settingsMutex.unlock();

    m_audioSampleRate = sampleRate;
}

void WFMDemod::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "WFMDemod::applyChannelSettings:"
            << " inputSampleRate: " << inputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if((inputFrequencyOffset != m_inputFrequencyOffset) ||
        (inputSampleRate != m_inputSampleRate) || force)
    {
        m_nco.setFreq(-inputFrequencyOffset, inputSampleRate);
    }

    if ((inputSampleRate != m_inputSampleRate) || force)
    {
        qDebug() << "WFMDemod::applyChannelSettings: m_interpolator.create";
        m_interpolator.create(16, inputSampleRate, m_settings.m_afBandwidth);
        m_interpolatorDistanceRemain = (Real) inputSampleRate / (Real) m_audioSampleRate;
        m_interpolatorDistance =  (Real) inputSampleRate / (Real) m_audioSampleRate;
        qDebug() << "WFMDemod::applySettings: m_rfFilter->create_filter";
        Real lowCut = -(m_settings.m_rfBandwidth / 2.0) / inputSampleRate;
        Real hiCut  = (m_settings.m_rfBandwidth / 2.0) / inputSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_fmExcursion = m_settings.m_rfBandwidth / (Real) inputSampleRate;
        m_phaseDiscri.setFMScaling(1.0f/m_fmExcursion);
        qDebug("WFMDemod::applySettings: m_fmExcursion: %f", m_fmExcursion);
    }

    m_inputSampleRate = inputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void WFMDemod::applySettings(const WFMDemodSettings& settings, bool force)
{
    qDebug() << "WFMDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " force: " << force;

    if((settings.m_afBandwidth != m_settings.m_afBandwidth) ||
       (settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_settingsMutex.lock();
        qDebug() << "WFMDemod::applySettings: m_interpolator.create";
        m_interpolator.create(16, m_inputSampleRate, settings.m_afBandwidth);
        m_interpolatorDistanceRemain = (Real) m_inputSampleRate / (Real) m_audioSampleRate;
        m_interpolatorDistance =  (Real) m_inputSampleRate / (Real) m_audioSampleRate;
        qDebug() << "WFMDemod::applySettings: m_rfFilter->create_filter";
        Real lowCut = -(settings.m_rfBandwidth / 2.0) / m_inputSampleRate;
        Real hiCut  = (settings.m_rfBandwidth / 2.0) / m_inputSampleRate;
        m_rfFilter->create_filter(lowCut, hiCut);
        m_fmExcursion = settings.m_rfBandwidth / (Real) m_inputSampleRate;
        m_phaseDiscri.setFMScaling(1.0f/m_fmExcursion);
        qDebug("WFMDemod::applySettings: m_fmExcursion: %f", m_fmExcursion);
        m_settingsMutex.unlock();
    }

    if ((settings.m_squelch != m_settings.m_squelch) || force)
    {
        qDebug() << "WFMDemod::applySettings: set m_squelchLevel";
        m_squelchLevel = pow(10.0, settings.m_squelch / 10.0);
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

QByteArray WFMDemod::serialize() const
{
    return m_settings.serialize();
}

bool WFMDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureWFMDemod *msg = MsgConfigureWFMDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureWFMDemod *msg = MsgConfigureWFMDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int WFMDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setWfmDemodSettings(new SWGSDRangel::SWGWFMDemodSettings());
    response.getWfmDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int WFMDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    WFMDemodSettings settings = m_settings;
    bool frequencyOffsetChanged = false;

    if (channelSettingsKeys.contains("inputFrequencyOffset"))
    {
        settings.m_inputFrequencyOffset = response.getWfmDemodSettings()->getInputFrequencyOffset();
        frequencyOffsetChanged = true;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getWfmDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("afBandwidth")) {
        settings.m_afBandwidth = response.getWfmDemodSettings()->getAfBandwidth();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getWfmDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getWfmDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getWfmDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getWfmDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getWfmDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getWfmDemodSettings()->getAudioDeviceName();
    }

    if (frequencyOffsetChanged)
    {
        MsgConfigureChannelizer* channelConfigMsg = MsgConfigureChannelizer::create(
                requiredBW(settings.m_rfBandwidth), settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(channelConfigMsg);
    }

    MsgConfigureWFMDemod *msg = MsgConfigureWFMDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("WFMDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureWFMDemod *msgToGUI = MsgConfigureWFMDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int WFMDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setWfmDemodReport(new SWGSDRangel::SWGWFMDemodReport());
    response.getWfmDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void WFMDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const WFMDemodSettings& settings)
{
    response.getWfmDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getWfmDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getWfmDemodSettings()->setAfBandwidth(settings.m_afBandwidth);
    response.getWfmDemodSettings()->setVolume(settings.m_volume);
    response.getWfmDemodSettings()->setSquelch(settings.m_squelch);
    response.getWfmDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getWfmDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getWfmDemodSettings()->getTitle()) {
        *response.getWfmDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getWfmDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getWfmDemodSettings()->getAudioDeviceName()) {
        *response.getWfmDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getWfmDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
}

void WFMDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getWfmDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getWfmDemodReport()->setSquelch(m_squelchState > 0 ? 1 : 0);
    response.getWfmDemodReport()->setAudioSampleRate(m_audioSampleRate);
    response.getWfmDemodReport()->setChannelSampleRate(m_inputSampleRate);
}

