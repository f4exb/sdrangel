///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// (c) 2015 John Greb
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

#include <stdio.h>

#include <QTime>
#include <QDebug>
#include <QThread>

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"

#include "lorademod.h"

MESSAGE_CLASS_DEFINITION(LoRaDemod::MsgConfigureLoRaDemod, Message)

const QString LoRaDemod::m_channelIdURI = "sdrangel.channel.lorademod";
const QString LoRaDemod::m_channelId = "LoRaDemod";

LoRaDemod::LoRaDemod(DeviceAPI* deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new LoRaDemodBaseband();
    m_basebandSink->moveToThread(m_thread);

	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);
}

LoRaDemod::~LoRaDemod()
{
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_basebandSink;
    delete m_thread;
}


void LoRaDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool pO)
{
    (void) pO;
	m_basebandSink->feed(begin, end);
}

void LoRaDemod::start()
{
    qDebug() << "LoRaDemod::start";

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_basebandSink->reset();
    m_thread->start();
}

void LoRaDemod::stop()
{
    qDebug() << "LoRaDemod::stop";
	m_thread->exit();
	m_thread->wait();
}

bool LoRaDemod::handleMessage(const Message& cmd)
{
	if (MsgConfigureLoRaDemod::match(cmd))
	{
		qDebug() << "LoRaDemod::handleMessage: MsgConfigureLoRaDemod";
		MsgConfigureLoRaDemod& cfg = (MsgConfigureLoRaDemod&) cmd;
		LoRaDemodSettings settings = cfg.getSettings();
		applySettings(settings, cfg.getForce());

		return true;
	}
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "LoRaDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);

        return true;
    }
	else
	{
		return false;
	}
}

QByteArray LoRaDemod::serialize() const
{
    return m_settings.serialize();
}

bool LoRaDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureLoRaDemod *msg = MsgConfigureLoRaDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureLoRaDemod *msg = MsgConfigureLoRaDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void LoRaDemod::applySettings(const LoRaDemodSettings& settings, bool force)
{
    qDebug() << "LoRaDemod::applySettings:"
            << " m_centerFrequency: " << settings.m_centerFrequency
            << " m_bandwidthIndex: " << settings.m_bandwidthIndex
            << " m_spread: " << settings.m_spread
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_title: " << settings.m_title
            << " force: " << force;

    LoRaDemodBaseband::MsgConfigureLoRaDemodBaseband *msg = LoRaDemodBaseband::MsgConfigureLoRaDemodBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    m_settings = settings;
}