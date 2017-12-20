///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "chanalyzerng.h"

#include <QTime>
#include <QDebug>
#include <stdio.h>

#include "device/devicesourceapi.h"
#include "audio/audiooutput.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/downchannelizer.h"

MESSAGE_CLASS_DEFINITION(ChannelAnalyzerNG::MsgConfigureChannelAnalyzer, Message)
MESSAGE_CLASS_DEFINITION(ChannelAnalyzerNG::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(ChannelAnalyzerNG::MsgReportChannelSampleRateChanged, Message)

const QString ChannelAnalyzerNG::m_channelIdURI = "sdrangel.channel.chanalyzerng";
const QString ChannelAnalyzerNG::m_channelId = "ChannelAnalyzerNG";

ChannelAnalyzerNG::ChannelAnalyzerNG(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_sampleSink(0),
        m_settingsMutex(QMutex::Recursive)
{
    setObjectName(m_channelId);

	m_undersampleCount = 0;
	m_sum = 0;
	m_usb = true;
	m_magsq = 0;
	m_useInterpolator = false;
	m_interpolatorDistance = 1.0f;
	m_interpolatorDistanceRemain = 0.0f;
	SSBFilter = new fftfilt(m_config.m_LowCutoff / m_config.m_inputSampleRate, m_config.m_Bandwidth / m_config.m_inputSampleRate, ssbFftLen);
	DSBFilter = new fftfilt(m_config.m_Bandwidth / m_config.m_inputSampleRate, 2*ssbFftLen);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

	apply(true);
}

ChannelAnalyzerNG::~ChannelAnalyzerNG()
{
	if (SSBFilter) delete SSBFilter;
	if (DSBFilter) delete DSBFilter;
	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void ChannelAnalyzerNG::configure(MessageQueue* messageQueue,
		int channelSampleRate,
		Real Bandwidth,
		Real LowCutoff,
		int  spanLog2,
		bool ssb)
{
    Message* cmd = MsgConfigureChannelAnalyzer::create(channelSampleRate, Bandwidth, LowCutoff, spanLog2, ssb);
	messageQueue->push(cmd);
}

void ChannelAnalyzerNG::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly __attribute__((unused)))
{
	fftfilt::cmplx *sideband = 0;
	Complex ci;

	m_settingsMutex.lock();

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if (m_useInterpolator)
		{
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci, sideband);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
		}
		else
		{
	        processOneSample(c, sideband);
		}
	}

	if(m_sampleSink != 0)
	{
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), m_running.m_ssb); // m_ssb = positive only
	}

	m_sampleBuffer.clear();

	m_settingsMutex.unlock();
}

void ChannelAnalyzerNG::start()
{
}

void ChannelAnalyzerNG::stop()
{
}

bool ChannelAnalyzerNG::handleMessage(const Message& cmd)
{
	qDebug() << "ChannelAnalyzerNG::handleMessage: " << cmd.getIdentifier();

	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

		m_config.m_inputSampleRate = notif.getSampleRate();
		m_config.m_frequency = notif.getFrequencyOffset();

        qDebug() << "ChannelAnalyzerNG::handleMessage: MsgChannelizerNotification:"
                << " m_sampleRate: " << m_config.m_inputSampleRate
                << " frequencyOffset: " << m_config.m_frequency;

		apply();

		if (getMessageQueueToGUI())
		{
            MsgReportChannelSampleRateChanged *msg = MsgReportChannelSampleRateChanged::create();
            getMessageQueueToGUI()->push(msg);
		}

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
	else if (MsgConfigureChannelAnalyzer::match(cmd))
	{
		MsgConfigureChannelAnalyzer& cfg = (MsgConfigureChannelAnalyzer&) cmd;

        m_config.m_channelSampleRate = cfg.getChannelSampleRate();
		m_config.m_Bandwidth = cfg.getBandwidth();
		m_config.m_LowCutoff = cfg.getLoCutoff();
		m_config.m_spanLog2 = cfg.getSpanLog2();
		m_config.m_ssb = cfg.getSSB();

        qDebug() << "ChannelAnalyzerNG::handleMessage: MsgConfigureChannelAnalyzer:"
                << " m_channelSampleRate: " << m_config.m_channelSampleRate
                << " m_Bandwidth: " << m_config.m_Bandwidth
                << " m_LowCutoff: " << m_config.m_LowCutoff
                << " m_spanLog2: " << m_config.m_spanLog2
                << " m_ssb: " << m_config.m_ssb;

        apply();
		return true;
	}
	else
	{
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



void ChannelAnalyzerNG::apply(bool force)
{
    if ((m_running.m_frequency != m_config.m_frequency) ||
        (m_running.m_inputSampleRate != m_config.m_inputSampleRate) ||
        force)
    {
        m_nco.setFreq(-m_config.m_frequency, m_config.m_inputSampleRate);
    }

    if ((m_running.m_inputSampleRate != m_config.m_inputSampleRate) ||
        (m_running.m_channelSampleRate != m_config.m_channelSampleRate) ||
        force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, m_config.m_inputSampleRate, m_config.m_inputSampleRate / 2.2);
        m_interpolatorDistanceRemain = 0.0f;
        m_interpolatorDistance =  (Real) m_config.m_inputSampleRate / (Real) m_config.m_channelSampleRate;
        m_useInterpolator = (m_config.m_inputSampleRate != m_config.m_channelSampleRate); // optim
        m_settingsMutex.unlock();
    }

    if ((m_running.m_channelSampleRate != m_config.m_channelSampleRate) ||
        (m_running.m_Bandwidth != m_config.m_Bandwidth) ||
        (m_running.m_LowCutoff != m_config.m_LowCutoff) ||
         force)
    {
        float bandwidth = m_config.m_Bandwidth;
        float lowCutoff = m_config.m_LowCutoff;

        if (bandwidth < 0)
        {
            bandwidth = -bandwidth;
            lowCutoff = -lowCutoff;
            m_usb = false;
        }
        else
        {
            m_usb = true;
        }

        if (bandwidth < 100.0f)
        {
            bandwidth = 100.0f;
            lowCutoff = 0;
        }

        m_settingsMutex.lock();

        SSBFilter->create_filter(lowCutoff / m_config.m_channelSampleRate, bandwidth / m_config.m_channelSampleRate);
        DSBFilter->create_dsb_filter(bandwidth / m_config.m_channelSampleRate);

        m_settingsMutex.unlock();
    }

    m_running.m_frequency = m_config.m_frequency;
    m_running.m_channelSampleRate = m_config.m_channelSampleRate;
    m_running.m_inputSampleRate = m_config.m_inputSampleRate;
    m_running.m_Bandwidth = m_config.m_Bandwidth;
    m_running.m_LowCutoff = m_config.m_LowCutoff;

    //m_settingsMutex.lock();
    m_running.m_spanLog2 = m_config.m_spanLog2;
    m_running.m_ssb = m_config.m_ssb;
    //m_settingsMutex.unlock();
}
