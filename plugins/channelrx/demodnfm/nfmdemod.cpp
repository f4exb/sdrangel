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
#include "SWGNFMDemodSettings.h"

#include "dsp/downchannelizer.h"
#include "util/stepfunctions.h"
#include "audio/audiooutput.h"
#include "dsp/pidcontroller.h"
#include "dsp/dspengine.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "device/devicesourceapi.h"

#include "nfmdemodgui.h"
#include "nfmdemod.h"

MESSAGE_CLASS_DEFINITION(NFMDemod::MsgConfigureNFMDemod, Message)
MESSAGE_CLASS_DEFINITION(NFMDemod::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(NFMDemod::MsgReportCTCSSFreq, Message)

const QString NFMDemod::m_channelIdURI = "de.maintech.sdrangelove.channel.nfm";
const QString NFMDemod::m_channelId = "NFMDemod";

static const double afSqTones[2] = {1000.0, 6000.0}; // {1200.0, 8000.0};
const int NFMDemod::m_udpBlockSize = 512;

NFMDemod::NFMDemod(DeviceSourceAPI *devieAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(devieAPI),
        m_inputSampleRate(48000),
        m_inputFrequencyOffset(0),
        m_ctcssIndex(0),
        m_sampleCount(0),
        m_squelchCount(0),
        m_squelchGate(2),
        m_audioMute(false),
        m_squelchLevel(-990),
        m_squelchOpen(false),
        m_afSquelchOpen(false),
        m_magsq(0.0f),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_movingAverage(40, 0),
        m_afSquelch(2, afSqTones),
        m_fmExcursion(2400),
        m_audioFifo(48000),
        m_settingsMutex(QMutex::Recursive)
{
	setObjectName(m_channelId);

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

	m_agcLevel = 1.0;
	m_movingAverage.resize(32, 0);

	m_ctcssDetector.setCoefficients(3000, 6000.0); // 0.5s / 2 Hz resolution
	m_afSquelch.setCoefficients(24, 600, 48000.0, 200, 0); // 0.5ms test period, 300ms average span, 48kS/s SR, 100ms attack, no decay

	DSPEngine::instance()->addAudioSink(&m_audioFifo);
	m_udpBufferAudio = new UDPSink<qint16>(this, m_udpBlockSize, m_settings.m_udpPort);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
	applySettings(m_settings, true);
}

NFMDemod::~NFMDemod()
{
	DSPEngine::instance()->removeAudioSink(&m_audioFifo);
	delete m_udpBufferAudio;
	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

float arctan2(Real y, Real x)
{
	Real coeff_1 = M_PI / 4;
	Real coeff_2 = 3 * coeff_1;
	Real abs_y = fabs(y) + 1e-10;      // kludge to prevent 0/0 condition
	Real angle;
	if( x>= 0) {
		Real r = (x - abs_y) / (x + abs_y);
		angle = coeff_1 - coeff_1 * r;
	} else {
		Real r = (x + abs_y) / (abs_y - x);
		angle = coeff_2 - coeff_1 * r;
	}
	if(y < 0) {
		return(-angle);
	} else {
	    return(angle);
	}
}

Real angleDist(Real a, Real b)
{
	Real dist = b - a;

	while(dist <= M_PI)
		dist += 2 * M_PI;
	while(dist >= M_PI)
		dist -= 2 * M_PI;

	return dist;
}

void NFMDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst __attribute__((unused)))
{
	Complex ci;

	m_settingsMutex.lock();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
        {

            qint16 sample;

            double magsqRaw; // = ci.real()*ci.real() + c.imag()*c.imag();
            Real deviation;

            Real demod = m_phaseDiscri.phaseDiscriminatorDelta(ci, magsqRaw, deviation);

            Real magsq = magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED);
            m_movingAverage.feed(magsq);
            m_magsqSum += magsq;

            if (magsq > m_magsqPeak)
            {
                m_magsqPeak = magsq;
            }

            m_magsqCount++;
            m_sampleCount++;

            // AF processing

            if (m_settings.m_deltaSquelch)
            {
                if (m_afSquelch.analyze(demod)) {
                    m_afSquelchOpen = m_afSquelch.evaluate() ? m_squelchGate + 480 : 0;
                }

                if (m_afSquelchOpen)
                {
                    if (m_squelchCount < m_squelchGate + 480)
                    {
                        m_squelchCount++;
                    }
                }
                else
                {
                    if (m_squelchCount > 0)
                    {
                        m_squelchCount--;
                    }
                }
            }
            else
            {
                if (m_movingAverage.average() < m_squelchLevel)
                {
                    if (m_squelchCount > 0)
                    {
                        m_squelchCount--;
                    }
                }
                else
                {
                    if (m_squelchCount < m_squelchGate + 480)
                    {
                        m_squelchCount++;
                    }
                }
            }

            m_squelchOpen = (m_squelchCount > m_squelchGate);

            if ((m_squelchOpen) && !m_settings.m_audioMute)
            {
                if (m_settings.m_ctcssOn)
                {
                    Real ctcss_sample = m_lowpass.filter(demod);

                    if ((m_sampleCount & 7) == 7) // decimate 48k -> 6k
                    {
                        if (m_ctcssDetector.analyze(&ctcss_sample))
                        {
                            int maxToneIndex;

                            if (m_ctcssDetector.getDetectedTone(maxToneIndex))
                            {
                                if (maxToneIndex+1 != m_ctcssIndex)
                                {
                                    if (getMessageQueueToGUI()) {
                                        MsgReportCTCSSFreq *msg = MsgReportCTCSSFreq::create(m_ctcssDetector.getToneSet()[maxToneIndex]);
                                        getMessageQueueToGUI()->push(msg);
                                    }
                                    m_ctcssIndex = maxToneIndex+1;
                                }
                            }
                            else
                            {
                                if (m_ctcssIndex != 0)
                                {
                                    if (getMessageQueueToGUI()) {
                                        MsgReportCTCSSFreq *msg = MsgReportCTCSSFreq::create(0);
                                        getMessageQueueToGUI()->push(msg);
                                    }
                                    m_ctcssIndex = 0;
                                }
                            }
                        }
                    }
                }

                if (m_settings.m_ctcssOn && m_ctcssIndexSelected && (m_ctcssIndexSelected != m_ctcssIndex))
                {
                    sample = 0;
                    if (m_settings.m_copyAudioToUDP) m_udpBufferAudio->write(0);
                }
                else
                {
                    demod = m_bandpass.filter(demod);
                    Real squelchFactor = StepFunctions::smootherstep((Real) (m_squelchCount - m_squelchGate) / 480.0f);
                    sample = demod * m_settings.m_volume * squelchFactor;
                    if (m_settings.m_copyAudioToUDP) m_udpBufferAudio->write(demod * 5.0f * squelchFactor);
                }
            }
            else
            {
                if (m_ctcssIndex != 0)
                {
                    if (getMessageQueueToGUI()) {
                        MsgReportCTCSSFreq *msg = MsgReportCTCSSFreq::create(0);
                        getMessageQueueToGUI()->push(msg);
                    }

                    m_ctcssIndex = 0;
                }

                sample = 0;
                if (m_settings.m_copyAudioToUDP) m_udpBufferAudio->write(0);
            }

            m_audioBuffer[m_audioBufferFill].l = sample;
            m_audioBuffer[m_audioBufferFill].r = sample;
            ++m_audioBufferFill;

            if (m_audioBufferFill >= m_audioBuffer.size())
            {
                uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 10);

                if (res != m_audioBufferFill)
                {
                    qDebug("NFMDemod::feed: %u/%u audio samples written", res, m_audioBufferFill);
                }

                m_audioBufferFill = 0;
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
        }
	}

	if (m_audioBufferFill > 0)
	{
		uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill, 10);

		if (res != m_audioBufferFill)
		{
			qDebug("NFMDemod::feed: %u/%u tail samples written", res, m_audioBufferFill);
		}

		m_audioBufferFill = 0;
	}

	m_settingsMutex.unlock();
}

void NFMDemod::start()
{
    qDebug() << "NFMDemod::start";
    m_squelchCount = 0;
	m_audioFifo.clear();
	m_phaseDiscri.reset();
	applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
}

void NFMDemod::stop()
{
}

bool NFMDemod::handleMessage(const Message& cmd)
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;
		qDebug() << "NFMDemod::handleMessage: DownChannelizer::MsgChannelizerNotification";

		applyChannelSettings(notif.getSampleRate(), notif.getFrequencyOffset());

		return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;

        qDebug() << "NFMDemod::handleMessage: MsgConfigureChannelizer:"
                 << " sampleRate: " << cfg.getSampleRate()
                 << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
	else if (MsgConfigureNFMDemod::match(cmd))
	{
	    MsgConfigureNFMDemod& cfg = (MsgConfigureNFMDemod&) cmd;
		qDebug() << "NFMDemod::handleMessage: MsgConfigureNFMDemod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
	}
	else
	{
		return false;
	}
}

void NFMDemod::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "NFMDemod::applyChannelSettings:"
            << " inputSampleRate: " << inputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if ((inputFrequencyOffset != m_inputFrequencyOffset) ||
        (inputSampleRate != m_inputSampleRate) || force)
    {
        m_nco.setFreq(-inputFrequencyOffset, inputSampleRate);
    }

    if ((inputSampleRate != m_inputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, inputSampleRate, m_settings.m_rfBandwidth / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) inputSampleRate / (Real) m_settings.m_audioSampleRate;
        m_settingsMutex.unlock();
    }

    m_inputSampleRate = inputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void NFMDemod::applySettings(const NFMDemodSettings& settings, bool force)
{
    qDebug() << "NFMDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_volume: " << settings.m_volume
            << " m_squelchGate: " << settings.m_squelchGate
            << " m_deltaSquelch: " << settings.m_deltaSquelch
            << " m_squelch: " << settings.m_squelch
            << " m_ctcssIndex: " << settings.m_ctcssIndex
            << " m_ctcssOn: " << settings.m_ctcssOn
            << " m_audioMute: " << settings.m_audioMute
            << " m_copyAudioToUDP: " << settings.m_copyAudioToUDP
            << " m_udpAddress: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) ||
        (settings.m_audioSampleRate != m_settings.m_audioSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, m_inputSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) m_inputSampleRate / (Real) settings.m_audioSampleRate;
        m_settingsMutex.unlock();
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) ||
        (settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_phaseDiscri.setFMScaling((8.0f*settings.m_rfBandwidth) / static_cast<float>(settings.m_fmDeviation)); // integrate 4x factor
    }

    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) ||
        (settings.m_audioSampleRate != m_settings.m_audioSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_lowpass.create(301, settings.m_audioSampleRate, 250.0);
        m_bandpass.create(301, settings.m_audioSampleRate, 300.0, settings.m_afBandwidth);
        m_settingsMutex.unlock();
    }

    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force)
    {
        m_squelchGate = 480 * settings.m_squelchGate; // gate is given in 10s of ms at 48000 Hz audio sample rate
        m_squelchCount = 0; // reset squelch open counter
    }

    if ((settings.m_squelch != m_settings.m_squelch) ||
        (settings.m_deltaSquelch != m_settings.m_deltaSquelch) || force)
    {
        if (settings.m_deltaSquelch)
        { // input is a value in negative millis
            m_squelchLevel = (- settings.m_squelch) / 1000.0;
            m_afSquelch.setThreshold(m_squelchLevel);
            m_afSquelch.reset();
        }
        else
        { // input is a value in centi-Bels
            m_squelchLevel = std::pow(10.0, settings.m_squelch / 100.0);
            m_movingAverage.fill(0.0);
        }

        m_squelchCount = 0; // reset squelch open counter
    }

    if ((settings.m_udpAddress != m_settings.m_udpAddress)
        || (settings.m_udpPort != m_settings.m_udpPort) || force)
    {
        m_udpBufferAudio->setAddress(const_cast<QString&>(settings.m_udpAddress));
        m_udpBufferAudio->setPort(settings.m_udpPort);
    }

    if ((settings.m_ctcssIndex != m_settings.m_ctcssIndex) || force)
    {
        setSelectedCtcssIndex(settings.m_ctcssIndex);
    }

    m_settings = settings;
}

QByteArray NFMDemod::serialize() const
{
    return m_settings.serialize();
}

bool NFMDemod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    NFMDemod::MsgConfigureChannelizer* channelConfigMsg = NFMDemod::MsgConfigureChannelizer::create(
            48000, m_settings.m_inputFrequencyOffset);
    m_inputMessageQueue.push(channelConfigMsg);

    MsgConfigureNFMDemod *msg = MsgConfigureNFMDemod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int NFMDemod::webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage __attribute__((unused)))
{
    response.setNfmDemodSettings(new SWGSDRangel::SWGNFMDemodSettings());
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int NFMDemod::webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage  __attribute__((unused)))
{
    NFMDemodSettings settings = m_settings;
    bool frequencyOffsetChanged = false;

    if (channelSettingsKeys.contains("afBandwidth")) {
        settings.m_afBandwidth = response.getNfmDemodSettings()->getAfBandwidth();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getNfmDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("audioSampleRate")) {
        settings.m_audioSampleRate = response.getNfmDemodSettings()->getAudioSampleRate();
    }
    if (channelSettingsKeys.contains("copyAudioToUDP")) {
        settings.m_copyAudioToUDP = response.getNfmDemodSettings()->getCopyAudioToUdp() != 0;
    }
    if (channelSettingsKeys.contains("ctcssIndex")) {
        settings.m_ctcssIndex = response.getNfmDemodSettings()->getCtcssIndex();
    }
    if (channelSettingsKeys.contains("ctcssOn")) {
        settings.m_ctcssOn = response.getNfmDemodSettings()->getCtcssOn() != 0;
    }
    if (channelSettingsKeys.contains("deltaSquelch")) {
        settings.m_deltaSquelch = response.getNfmDemodSettings()->getDeltaSquelch() != 0;
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getNfmDemodSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset"))
    {
        settings.m_inputFrequencyOffset = response.getNfmDemodSettings()->getInputFrequencyOffset();
        frequencyOffsetChanged = true;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getNfmDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getNfmDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getNfmDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("squelchGate")) {
        settings.m_squelchGate = response.getNfmDemodSettings()->getSquelchGate();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getNfmDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getNfmDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getNfmDemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getNfmDemodSettings()->getVolume();
    }

    if (frequencyOffsetChanged)
    {
        MsgConfigureChannelizer* channelConfigMsg = MsgConfigureChannelizer::create(
                48000, settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(channelConfigMsg);
    }

    MsgConfigureNFMDemod *msg = MsgConfigureNFMDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureNFMDemod *msgToGUI = MsgConfigureNFMDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void NFMDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const NFMDemodSettings& settings)
{
    response.getNfmDemodSettings()->setAfBandwidth(settings.m_afBandwidth);
    response.getNfmDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getNfmDemodSettings()->setAudioSampleRate(settings.m_audioSampleRate);
    response.getNfmDemodSettings()->setCopyAudioToUdp(settings.m_copyAudioToUDP ? 1 : 0);
    response.getNfmDemodSettings()->setCtcssIndex(settings.m_ctcssIndex);
    response.getNfmDemodSettings()->setCtcssOn(settings.m_ctcssOn ? 1 : 0);
    response.getNfmDemodSettings()->setDeltaSquelch(settings.m_deltaSquelch ? 1 : 0);
    response.getNfmDemodSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getNfmDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getNfmDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getNfmDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getNfmDemodSettings()->setSquelch(settings.m_squelch);
    response.getNfmDemodSettings()->setSquelchGate(settings.m_squelchGate);
    response.getNfmDemodSettings()->setUdpPort(settings.m_udpPort);
    response.getNfmDemodSettings()->setVolume(settings.m_volume);

    if (response.getNfmDemodSettings()->getTitle()) {
        *response.getNfmDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getNfmDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getNfmDemodSettings()->getUdpAddress()) {
        *response.getNfmDemodSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getNfmDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }
}

