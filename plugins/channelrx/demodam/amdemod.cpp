///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
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

#include "amdemod.h"

#include <QTime>
#include <QDebug>

#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGAMDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGAMDemodReport.h"

#include "dsp/downchannelizer.h"
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/dspcommands.h"
#include "dsp/fftfilt.h"
#include "device/devicesourceapi.h"
#include "util/db.h"
#include "util/stepfunctions.h"

MESSAGE_CLASS_DEFINITION(AMDemod::MsgConfigureAMDemod, Message)
MESSAGE_CLASS_DEFINITION(AMDemod::MsgConfigureChannelizer, Message)

const QString AMDemod::m_channelIdURI = "sdrangel.channel.amdemod";
const QString AMDemod::m_channelId = "AMDemod";
const int AMDemod::m_udpBlockSize = 512;

AMDemod::AMDemod(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_inputSampleRate(48000),
        m_inputFrequencyOffset(0),
        m_running(false),
        m_squelchOpen(false),
        m_squelchDelayLine(9600),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_volumeAGC(0.003),
        m_syncAMAGC(12000, 0.1, 1e-2),
        m_audioFifo(48000),
        m_settingsMutex(QMutex::Recursive)
{
    setObjectName(m_channelId);

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

	m_magsq = 0.0;

	DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(&m_audioFifo, getInputMessageQueue());
	m_audioSampleRate = DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate();
    DSBFilter = new fftfilt((2.0f * m_settings.m_rfBandwidth) / m_audioSampleRate, 2 * 1024);
    SSBFilter = new fftfilt(0.0f, m_settings.m_rfBandwidth / m_audioSampleRate, 1024);
    m_syncAMAGC.setThresholdEnable(false);
    m_syncAMAGC.resize(12000, 6000, 0.1);

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
    applySettings(m_settings, true);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    m_pllFilt.create(101, m_audioSampleRate, 200.0);
    m_pll.computeCoefficients(0.05, 0.707, 1000);
    m_syncAMBuffIndex = 0;
}

AMDemod::~AMDemod()
{
	DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(&m_audioFifo);
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
    delete DSBFilter;
    delete SSBFilter;
}

void AMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
{
	Complex ci;

	if (!m_running) {
        return;
    }

	m_settingsMutex.lock();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if (m_interpolatorDistance < 1.0f) // interpolate
		{
            processOneSample(ci);

		    while (m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
		}
		else // decimate
		{
	        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
	        {
	            processOneSample(ci);
	            m_interpolatorDistanceRemain += m_interpolatorDistance;
	        }
		}
	}

	if (m_audioBufferFill > 0)
	{
		uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 10);

		if (res != m_audioBufferFill)
		{
			qDebug("AMDemod::feed: %u/%u tail samples written", res, m_audioBufferFill);
		}

		m_audioBufferFill = 0;
	}

	m_settingsMutex.unlock();
}

void AMDemod::processOneSample(Complex &ci)
{
    Real re = ci.real() / SDR_RX_SCALEF;
    Real im = ci.imag() / SDR_RX_SCALEF;
    Real magsq = re*re + im*im;
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;

    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }

    m_magsqCount++;

    m_squelchDelayLine.write(magsq);

    if (m_magsq < m_squelchLevel)
    {
        if (m_squelchCount > 0) {
            m_squelchCount--;
        }
    }
    else
    {
        if (m_squelchCount < m_audioSampleRate / 10) {
            m_squelchCount++;
        }
    }

    qint16 sample;

    m_squelchOpen = (m_squelchCount >= m_audioSampleRate / 20);

    if (m_squelchOpen && !m_settings.m_audioMute)
    {
        Real demod;

        if (m_settings.m_pll)
        {
            std::complex<float> s(re, im);
            s = m_pllFilt.filter(s);
            m_pll.feed(s.real(), s.imag());
            float yr = re * m_pll.getImag() - im * m_pll.getReal();
            float yi = re * m_pll.getReal() + im * m_pll.getImag();

            fftfilt::cmplx *sideband;
            std::complex<float> cs(yr, yi);
            int n_out;

            if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMDSB) {
                n_out = DSBFilter->runDSB(cs, &sideband, false);
            } else {
                n_out = SSBFilter->runSSB(cs, &sideband, m_settings.m_syncAMOperation == AMDemodSettings::SyncAMUSB, false);
            }

            for (int i = 0; i < n_out; i++)
            {
                float agcVal = m_syncAMAGC.feedAndGetValue(sideband[i]);
                fftfilt::cmplx z = sideband[i] * agcVal; // * m_syncAMAGC.getStepValue();

                if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMDSB) {
                    m_syncAMBuff[i] = (z.real() + z.imag());
                } else if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMUSB) {
                    m_syncAMBuff[i] = (z.real() + z.imag());
                } else {
                    m_syncAMBuff[i] = (z.real() + z.imag());
                }

//                    if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMDSB) {
//                        m_syncAMBuff[i] = (sideband[i].real() + sideband[i].imag())/2.0f;
//                    } else if (m_settings.m_syncAMOperation == AMDemodSettings::SyncAMUSB) {
//                        m_syncAMBuff[i] = (sideband[i].real() + sideband[i].imag());
//                    } else {
//                        m_syncAMBuff[i] = (sideband[i].real() + sideband[i].imag());
//                    }

                m_syncAMBuffIndex = 0;
            }

            m_syncAMBuffIndex = m_syncAMBuffIndex < 2*1024 ? m_syncAMBuffIndex : 0;
            demod = m_syncAMBuff[m_syncAMBuffIndex++]*4.0f; // mos pifometrico
//                demod = m_syncAMBuff[m_syncAMBuffIndex++]*(SDR_RX_SCALEF/602.0f);
//                m_volumeAGC.feed(demod);
//                demod /= (10.0*m_volumeAGC.getValue());
        }
        else
        {
            demod = sqrt(m_squelchDelayLine.readBack(m_audioSampleRate/20));
            m_volumeAGC.feed(demod);
            demod = (demod - m_volumeAGC.getValue()) / m_volumeAGC.getValue();
        }

        if (m_settings.m_bandpassEnable)
        {
            demod = m_bandpass.filter(demod);
            demod /= 301.0f;
        }

        Real attack = (m_squelchCount - 0.05f * m_audioSampleRate) / (0.05f * m_audioSampleRate);
        sample = demod * StepFunctions::smootherstep(attack) * (m_audioSampleRate/24) * m_settings.m_volume;
    }
    else
    {
        sample = 0;
    }

    m_audioBuffer[m_audioBufferFill].l = sample;
    m_audioBuffer[m_audioBufferFill].r = sample;
    ++m_audioBufferFill;

    if (m_audioBufferFill >= m_audioBuffer.size())
    {
        uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 10);

        if (res != m_audioBufferFill)
        {
            qDebug("AMDemod::processOneSample: %u/%u audio samples written", res, m_audioBufferFill);
            m_audioFifo.clear();
        }

        m_audioBufferFill = 0;
    }
}

void AMDemod::start()
{
	qDebug("AMDemod::start");
	m_squelchCount = 0;
	m_audioFifo.clear();
    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
    m_running = true;
}

void AMDemod::stop()
{
    qDebug("AMDemod::stop");
    m_running = false;
}

bool AMDemod::handleMessage(const Message& cmd)
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

        qDebug() << "AMDemod::handleMessage: MsgChannelizerNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " inputFrequencyOffset: " << notif.getFrequencyOffset();

        applyChannelSettings(notif.getSampleRate(), notif.getFrequencyOffset());

		return true;
	}
	else if (MsgConfigureChannelizer::match(cmd))
	{
	    MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

	    qDebug() << "AMDemod::handleMessage: MsgConfigureChannelizer:"
                << " sampleRate: " << cfg.getSampleRate()
                << " inputFrequencyOffset: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
	}
	else if (MsgConfigureAMDemod::match(cmd))
	{
        MsgConfigureAMDemod& cfg = (MsgConfigureAMDemod&) cmd;
        qDebug() << "AMDemod::handleMessage: MsgConfigureAMDemod";
        applySettings(cfg.getSettings(), cfg.getForce());

		return true;
	}
    else if (BasebandSampleSink::MsgThreadedSink::match(cmd))
    {
        BasebandSampleSink::MsgThreadedSink& cfg = (BasebandSampleSink::MsgThreadedSink&) cmd;
        const QThread *thread = cfg.getThread();
        qDebug("AMDemod::handleMessage: BasebandSampleSink::MsgThreadedSink: %p", thread);
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        return true;
    }
    else if (DSPConfigureAudio::match(cmd))
    {
        DSPConfigureAudio& cfg = (DSPConfigureAudio&) cmd;
        uint32_t sampleRate = cfg.getSampleRate();

        qDebug() << "AMDemod::handleMessage: DSPConfigureAudio:"
                << " sampleRate: " << sampleRate;

        if (sampleRate != m_audioSampleRate) {
            applyAudioSampleRate(sampleRate);
        }

        return true;
    }
	else
	{
		return false;
	}
}

void AMDemod::applyAudioSampleRate(int sampleRate)
{
    qDebug("AMDemod::applyAudioSampleRate: %d", sampleRate);

    MsgConfigureChannelizer* channelConfigMsg = MsgConfigureChannelizer::create(
            sampleRate, m_settings.m_inputFrequencyOffset);
    m_inputMessageQueue.push(channelConfigMsg);

    m_settingsMutex.lock();

    m_interpolator.create(16, m_inputSampleRate, m_settings.m_rfBandwidth / 2.2f);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorDistance = (Real) m_inputSampleRate / (Real) sampleRate;
    m_bandpass.create(301, sampleRate, 300.0, m_settings.m_rfBandwidth / 2.0f);
    m_audioFifo.setSize(sampleRate);
    m_squelchDelayLine.resize(sampleRate/5);
    DSBFilter->create_dsb_filter((2.0f * m_settings.m_rfBandwidth) / (float) sampleRate);
    m_pllFilt.create(101, sampleRate, 200.0);

    if (m_settings.m_pll) {
        m_volumeAGC.resizeNew(sampleRate, 0.003);
    } else {
        m_volumeAGC.resizeNew(sampleRate/10, 0.003);
    }

    m_syncAMAGC.resize(sampleRate/4, sampleRate/8, 0.1);
    m_pll.setSampleRate(sampleRate);

    m_settingsMutex.unlock();
    m_audioSampleRate = sampleRate;
}

void AMDemod::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "AMDemod::applyChannelSettings:"
            << " inputSampleRate: " << inputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if ((m_inputFrequencyOffset != inputFrequencyOffset) ||
        (m_inputSampleRate != inputSampleRate) || force)
    {
        m_nco.setFreq(-inputFrequencyOffset, inputSampleRate);
    }

    if ((m_inputSampleRate != inputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, inputSampleRate, m_settings.m_rfBandwidth / 2.2f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) inputSampleRate / (Real) m_audioSampleRate;
        m_settingsMutex.unlock();
    }

    m_inputSampleRate = inputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void AMDemod::applySettings(const AMDemodSettings& settings, bool force)
{
    qDebug() << "AMDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioMute: " << settings.m_audioMute
            << " m_bandpassEnable: " << settings.m_bandpassEnable
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_pll: " << settings.m_pll
            << " m_syncAMOperation: " << (int) settings.m_syncAMOperation
            << " force: " << force;

    if((m_settings.m_rfBandwidth != settings.m_rfBandwidth) ||
        (m_settings.m_bandpassEnable != settings.m_bandpassEnable) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, m_inputSampleRate, settings.m_rfBandwidth / 2.2f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_inputSampleRate / (Real) m_audioSampleRate;
        m_bandpass.create(301, m_audioSampleRate, 300.0, settings.m_rfBandwidth / 2.0f);
        DSBFilter->create_dsb_filter((2.0f * settings.m_rfBandwidth) / (float) m_audioSampleRate);
        m_settingsMutex.unlock();
    }

    if ((m_settings.m_squelch != settings.m_squelch) || force)
    {
        m_squelchLevel = CalcDb::powerFromdB(settings.m_squelch);
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

    if ((m_settings.m_pll != settings.m_pll) || force)
    {
        if (settings.m_pll)
        {
            m_volumeAGC.resizeNew(m_audioSampleRate/4, 0.003);
            m_syncAMBuffIndex = 0;
        }
        else
        {
            m_volumeAGC.resizeNew(m_audioSampleRate/10, 0.003);
        }
    }

    if ((m_settings.m_syncAMOperation != settings.m_syncAMOperation) || force) {
        m_syncAMBuffIndex = 0;
    }

    m_settings = settings;
}

QByteArray AMDemod::serialize() const
{
    return m_settings.serialize();
}

bool AMDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAMDemod *msg = MsgConfigureAMDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAMDemod *msg = MsgConfigureAMDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int AMDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setAmDemodSettings(new SWGSDRangel::SWGAMDemodSettings());
    response.getAmDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int AMDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage __attribute__((unused)))
{
    AMDemodSettings settings = m_settings;
    bool frequencyOffsetChanged = false;

    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getAmDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset"))
    {
        settings.m_inputFrequencyOffset = response.getAmDemodSettings()->getInputFrequencyOffset();
        frequencyOffsetChanged = true;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getAmDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAmDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getAmDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getAmDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getAmDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("bandpassEnable")) {
        settings.m_bandpassEnable = response.getAmDemodSettings()->getBandpassEnable() != 0;
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getAmDemodSettings()->getAudioDeviceName();
    }

    if (frequencyOffsetChanged)
    {
        MsgConfigureChannelizer* channelConfigMsg = MsgConfigureChannelizer::create(
                m_audioSampleRate, settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(channelConfigMsg);
    }

    MsgConfigureAMDemod *msg = MsgConfigureAMDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("AMDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAMDemod *msgToGUI = MsgConfigureAMDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int AMDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setAmDemodReport(new SWGSDRangel::SWGAMDemodReport());
    response.getAmDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void AMDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const AMDemodSettings& settings)
{
    response.getAmDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getAmDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getAmDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getAmDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getAmDemodSettings()->setSquelch(settings.m_squelch);
    response.getAmDemodSettings()->setVolume(settings.m_volume);
    response.getAmDemodSettings()->setBandpassEnable(settings.m_bandpassEnable ? 1 : 0);

    if (response.getAmDemodSettings()->getTitle()) {
        *response.getAmDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getAmDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getAmDemodSettings()->getAudioDeviceName()) {
        *response.getAmDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getAmDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
}

void AMDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getAmDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getAmDemodReport()->setSquelch(m_squelchOpen ? 1 : 0);
    response.getAmDemodReport()->setAudioSampleRate(m_audioSampleRate);
    response.getAmDemodReport()->setChannelSampleRate(m_inputSampleRate);
}

