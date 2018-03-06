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

#include "dsp/downchannelizer.h"
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"

MESSAGE_CLASS_DEFINITION(AMDemod::MsgConfigureAMDemod, Message)
MESSAGE_CLASS_DEFINITION(AMDemod::MsgConfigureChannelizer, Message)

const QString AMDemod::m_channelIdURI = "de.maintech.sdrangelove.channel.am";
const QString AMDemod::m_channelId = "AMDemod";
const int AMDemod::m_udpBlockSize = 512;

AMDemod::AMDemod(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_inputSampleRate(48000),
        m_inputFrequencyOffset(0),
        m_running(false),
        m_squelchOpen(false),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_volumeAGC(0.003),
        m_audioFifo(48000),
        m_settingsMutex(QMutex::Recursive)
{
    setObjectName(m_channelId);

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

	m_magsq = 0.0;

	DSPEngine::instance()->addAudioSink(&m_audioFifo);
    m_audioNetSink = new AudioNetSink(0); // parent thread allocated dynamically
    m_audioNetSink->setDestination(m_settings.m_udpAddress, m_settings.m_udpPort);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
    applySettings(m_settings, true);
}

AMDemod::~AMDemod()
{
	DSPEngine::instance()->removeAudioSink(&m_audioFifo);
    delete m_audioNetSink;
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

bool AMDemod::isAudioNetSinkRTPCapable() const
{
    return m_audioNetSink && m_audioNetSink->isRTPCapable();
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
    else if (DSPSignalNotification::match(cmd))
    {
        return true;
    }
	else
	{
		return false;
	}
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
        m_interpolatorDistance = (Real) inputSampleRate / (Real) m_settings.m_audioSampleRate;
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
            << " m_copyAudioToUDP: " << settings.m_copyAudioToUDP
            << " m_copyAudioUseRTP: " << settings.m_copyAudioUseRTP
            << " m_udpAddress: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " force: " << force;

    if((m_settings.m_rfBandwidth != settings.m_rfBandwidth) ||
        (m_settings.m_audioSampleRate != settings.m_audioSampleRate) ||
        (m_settings.m_bandpassEnable != settings.m_bandpassEnable) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, m_inputSampleRate, settings.m_rfBandwidth / 2.2f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_inputSampleRate / (Real) settings.m_audioSampleRate;
        m_bandpass.create(301, settings.m_audioSampleRate, 300.0, settings.m_rfBandwidth / 2.0f);
        m_settingsMutex.unlock();
    }

    if ((m_settings.m_squelch != settings.m_squelch) || force)
    {
        m_squelchLevel = pow(10.0, settings.m_squelch / 10.0);
    }

    if ((m_settings.m_udpAddress != settings.m_udpAddress)
        || (m_settings.m_udpPort != settings.m_udpPort) || force)
    {
        m_audioNetSink->setDestination(settings.m_udpAddress, settings.m_udpPort);
    }

    if ((settings.m_copyAudioUseRTP != m_settings.m_copyAudioUseRTP) || force)
    {
        if (settings.m_copyAudioUseRTP)
        {
            if (m_audioNetSink->selectType(AudioNetSink::SinkRTP)) {
                qDebug("NFMDemod::applySettings: set audio sink to RTP mode");
            } else {
                qWarning("NFMDemod::applySettings: RTP support for audio sink not available. Fall back too UDP");
            }
        }
        else
        {
            if (m_audioNetSink->selectType(AudioNetSink::SinkUDP)) {
                qDebug("NFMDemod::applySettings: set audio sink to UDP mode");
            } else {
                qWarning("NFMDemod::applySettings: failed to set audio sink to UDP mode");
            }
        }
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

