///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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
#include <QThread>

#include <stdio.h>

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesource.h"
#include "chanalyzer.h"

MESSAGE_CLASS_DEFINITION(ChannelAnalyzer::MsgConfigureChannelAnalyzer, Message)

const char* const ChannelAnalyzer::m_channelIdURI = "sdrangel.channel.chanalyzer";
const char* const ChannelAnalyzer::m_channelId = "ChannelAnalyzer";

ChannelAnalyzer::ChannelAnalyzer(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_spectrumVis(SDR_RX_SCALEF),
        m_basebandSampleRate(0)
{
    qDebug("ChannelAnalyzer::ChannelAnalyzer");
    setObjectName(m_channelId);
    getChannelSampleRate();
    m_basebandSink = new ChannelAnalyzerBaseband();
    m_basebandSink->moveToThread(&m_thread);

	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);
}

ChannelAnalyzer::~ChannelAnalyzer()
{
    qDebug("ChannelAnalyzer::~ChannelAnalyzer");
	m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
    qDebug("ChannelAnalyzer::~ChannelAnalyzer: done");
}

int ChannelAnalyzer::getChannelSampleRate()
{
    DeviceSampleSource *source = m_deviceAPI->getSampleSource();

    if (source) {
        m_basebandSampleRate = source->getSampleRate();
    }

    qDebug("ChannelAnalyzer::getChannelSampleRate: %d", m_basebandSampleRate);
    return m_basebandSampleRate;
}

void ChannelAnalyzer::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;
    m_basebandSink->feed(begin, end);
}

void ChannelAnalyzer::start()
{
    qDebug() << "ChannelAnalyzer::start";

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    ChannelAnalyzerBaseband::MsgConfigureChannelAnalyzerBaseband *msg =
        ChannelAnalyzerBaseband::MsgConfigureChannelAnalyzerBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (getMessageQueueToGUI())
    {
        DSPSignalNotification *notifToGUI = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
        getMessageQueueToGUI()->push(notifToGUI);
    }
}

void ChannelAnalyzer::stop()
{
    qDebug() << "ChannelAnalyzer::stop";
	m_basebandSink->stopWork();
	m_thread.quit();
	m_thread.wait();
}

bool ChannelAnalyzer::handleMessage(const Message& cmd)
{
    if (MsgConfigureChannelAnalyzer::match(cmd))
    {
        qDebug("ChannelAnalyzer::handleMessage: MsgConfigureChannelAnalyzer");
        MsgConfigureChannelAnalyzer& cfg = (MsgConfigureChannelAnalyzer&) cmd;

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& cfg = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = cfg.getSampleRate();
        qDebug("ChannelAnalyzer::handleMessage: DSPSignalNotification: %d", m_basebandSampleRate);
        m_centerFrequency = cfg.getCenterFrequency();
        DSPSignalNotification *notif = new DSPSignalNotification(cfg);
        m_basebandSink->getInputMessageQueue()->push(notif);

        if (getMessageQueueToGUI())
        {
            DSPSignalNotification *notifToGUI = new DSPSignalNotification(cfg);
            getMessageQueueToGUI()->push(notifToGUI);
        }

        return true;
    }
	else
	{
	    return false;
	}
}

void ChannelAnalyzer::applySettings(const ChannelAnalyzerSettings& settings, bool force)
{
    qDebug() << "ChannelAnalyzer::applySettings:"
            << " m_rationalDownSample: " << settings.m_rationalDownSample
            << " m_rationalDownSamplerRate: " << settings.m_rationalDownSamplerRate
            << " m_rcc: " << settings.m_rrc
            << " m_rrcRolloff: " << settings.m_rrcRolloff / 100.0
            << " m_bandwidth: " << settings.m_bandwidth
            << " m_lowCutoff: " << settings.m_lowCutoff
            << " m_log2Decim: " << settings.m_log2Decim
            << " m_ssb: " << settings.m_ssb
            << " m_pll: " << settings.m_pll
            << " m_fll: " << settings.m_fll
            << " m_costasLoop: " << settings.m_costasLoop
            << " m_pllPskOrder: " << settings.m_pllPskOrder
            << " m_pllBandwidth: " << settings.m_pllBandwidth
            << " m_pllDampingFactor: " << settings.m_pllDampingFactor
            << " m_pllLoopGain: " << settings.m_pllLoopGain
            << " m_inputType: " << (int) settings.m_inputType;

    ChannelAnalyzerBaseband::MsgConfigureChannelAnalyzerBaseband *msg
        = ChannelAnalyzerBaseband::MsgConfigureChannelAnalyzerBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    m_settings = settings;
}
