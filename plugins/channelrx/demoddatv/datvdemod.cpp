///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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

#include <QDebug>

#include "device/deviceapi.h"

#include "datvdemod.h"

const QString DATVDemod::m_channelIdURI = "sdrangel.channel.demoddatv";
const QString DATVDemod::m_channelId = "DATVDemod";

MESSAGE_CLASS_DEFINITION(DATVDemod::MsgConfigureDATVDemod, Message)

DATVDemod::DATVDemod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_basebandSampleRate(0)
{
    qDebug("DATVDemod::DATVDemod");
    setObjectName("DATVDemod");
    m_thread = new QThread(this);
    m_basebandSink = new DATVDemodBaseband();
    m_basebandSink->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

}

DATVDemod::~DATVDemod()
{
    qDebug("DATVDemod::~DATVDemod");
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_basebandSink;
    delete m_thread;
}

void DATVDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void DATVDemod::start()
{
	qDebug("DATVDemod::start");

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_basebandSink->reset();
    m_thread->start();
}

void DATVDemod::stop()
{
    qDebug("DATVDemod::stop");
	m_thread->exit();
	m_thread->wait();
}

bool DATVDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureDATVDemod::match(cmd))
	{
        MsgConfigureDATVDemod& objCfg = (MsgConfigureDATVDemod&) cmd;
        qDebug() << "DATVDemod::handleMessage: MsgConfigureDATVDemod";
        applySettings(objCfg.getSettings(), objCfg.getForce());

        return true;
	}
    else if(DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate(); // store for init at start
        qDebug() << "DATVDemod::handleMessage: DSPSignalNotification" << m_basebandSampleRate;

        // Forward to the sink
        DSPSignalNotification* notifToSink = new DSPSignalNotification(notif); // make a copy
        m_basebandSink->getInputMessageQueue()->push(notifToSink);

        return true;
    }
	else
	{
		return false;
	}
}

void DATVDemod::applySettings(const DATVDemodSettings& settings, bool force)
{
    QString debugMsg = tr("DATVDemod::applySettings: force: %1").arg(force);
    settings.debug(debugMsg);

    DATVDemodBaseband::MsgConfigureDATVDemodBaseband *msg = DATVDemodBaseband::MsgConfigureDATVDemodBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    m_settings = settings;
}
