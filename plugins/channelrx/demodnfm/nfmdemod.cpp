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

#include <dsp/downchannelizer.h>
#include "util/stepfunctions.h"
#include "audio/audiooutput.h"
#include "dsp/pidcontroller.h"
#include "dsp/dspengine.h"
#include "dsp/threadedbasebandsamplesink.h"
#include <device/devicesourceapi.h>

#include "nfmdemodgui.h"
#include "nfmdemod.h"

MESSAGE_CLASS_DEFINITION(NFMDemod::MsgConfigureNFMDemod, Message)
MESSAGE_CLASS_DEFINITION(NFMDemod::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(NFMDemod::MsgReportCTCSSFreq, Message)

const QString NFMDemod::m_channelID = "de.maintech.sdrangelove.channel.nfm";

static const double afSqTones[2] = {1000.0, 6000.0}; // {1200.0, 8000.0};
const int NFMDemod::m_udpBlockSize = 512;

NFMDemod::NFMDemod(DeviceSourceAPI *devieAPI) :
    m_deviceAPI(devieAPI),
    m_absoluteFrequencyOffset(0),
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
	setObjectName("NFMDemod");

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
	if(y < 0)
		return(-angle);
	else return(angle);
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

            Real magsq = magsqRaw / (1<<30);
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
	m_audioFifo.clear();
	m_phaseDiscri.reset();
}

void NFMDemod::stop()
{
}

bool NFMDemod::handleMessage(const Message& cmd)
{
	qDebug() << "NFMDemod::handleMessage";

	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

		NFMDemodSettings settings = m_settings;

		settings.m_inputSampleRate = notif.getSampleRate();
		settings.m_inputFrequencyOffset = notif.getFrequencyOffset();

		applySettings(settings);

		qDebug() << "NFMDemod::handleMessage: MsgChannelizerNotification: m_inputSampleRate: " << settings.m_inputSampleRate
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
	else if (MsgConfigureNFMDemod::match(cmd))
	{
	    MsgConfigureNFMDemod& cfg = (MsgConfigureNFMDemod&) cmd;

	    NFMDemodSettings settings = cfg.getSettings();

	    m_absoluteFrequencyOffset = settings.m_inputFrequencyOffset;
        settings.m_inputSampleRate = m_settings.m_inputSampleRate;
        settings.m_inputFrequencyOffset = m_settings.m_inputFrequencyOffset;

		qDebug() << "NFMDemod::handleMessage: MsgConfigureNFMDemod:"
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
				<< " force: " << cfg.getForce();

        applySettings(settings, cfg.getForce());

        return true;
	}
	else
	{
		return false;
	}
}

void NFMDemod::applySettings(const NFMDemodSettings& settings, bool force)
{
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) ||
        (settings.m_inputSampleRate != m_settings.m_inputSampleRate) || force)
    {
        m_nco.setFreq(-settings.m_inputFrequencyOffset, settings.m_inputSampleRate);
    }

    if ((settings.m_inputSampleRate != m_settings.m_inputSampleRate) ||
        (settings.m_rfBandwidth != m_settings.m_rfBandwidth) ||
        (settings.m_audioSampleRate != m_settings.m_audioSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, settings.m_inputSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) settings.m_inputSampleRate / (Real) settings.m_audioSampleRate;
        m_settingsMutex.unlock();
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) ||
        (settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_phaseDiscri.setFMScaling((8.0f*settings.m_rfBandwidth) / (float) settings.m_fmDeviation); // integrate 4x factor
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
