///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <string.h>
#include <errno.h>
#include <QDebug>

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "sdrdaemonfecinput.h"

#include <device/devicesourceapi.h>
#include <dsp/filerecord.h>

#include "sdrdaemonfecgui.h"
#include "sdrdaemonfecudphandler.h"

MESSAGE_CLASS_DEFINITION(SDRdaemonFECInput::MsgConfigureSDRdaemonUDPLink, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonFECInput::MsgConfigureSDRdaemonAutoCorr, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonFECInput::MsgConfigureSDRdaemonWork, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonFECInput::MsgConfigureSDRdaemonStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonFECInput::MsgReportSDRdaemonAcquisition, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonFECInput::MsgReportSDRdaemonFECStreamData, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming, Message)

SDRdaemonFECInput::SDRdaemonFECInput(const QTimer& masterTimer, DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_address("127.0.0.1"),
	m_port(9090),
	m_SDRdaemonUDPHandler(0),
	m_deviceDescription(),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_startingTimeStamp(0),
    m_masterTimer(masterTimer),
    m_autoFollowRate(false),
    m_autoCorrBuffer(false)
{
	m_sampleFifo.setSize(96000 * 4);
	m_SDRdaemonUDPHandler = new SDRdaemonFECUDPHandler(&m_sampleFifo, getOutputMessageQueueToGUI(), m_deviceAPI);
	m_SDRdaemonUDPHandler->connectTimer(&m_masterTimer);
}

SDRdaemonFECInput::~SDRdaemonFECInput()
{
	stop();
	delete m_SDRdaemonUDPHandler;
}

bool SDRdaemonFECInput::start()
{
	qDebug() << "SDRdaemonInput::start";
	MsgConfigureSDRdaemonWork *command = MsgConfigureSDRdaemonWork::create(true);
	getInputMessageQueue()->push(command);
	return true;
}

void SDRdaemonFECInput::stop()
{
	qDebug() << "SDRdaemonInput::stop";
	MsgConfigureSDRdaemonWork *command = MsgConfigureSDRdaemonWork::create(false);
	getInputMessageQueue()->push(command);
}

const QString& SDRdaemonFECInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int SDRdaemonFECInput::getSampleRate() const
{
	return m_sampleRate;
}

quint64 SDRdaemonFECInput::getCenterFrequency() const
{
	return m_centerFrequency;
}

std::time_t SDRdaemonFECInput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

void SDRdaemonFECInput::getRemoteAddress(QString &s)
{
	if (m_SDRdaemonUDPHandler) {
		m_SDRdaemonUDPHandler->getRemoteAddress(s);
	}
}


bool SDRdaemonFECInput::handleMessage(const Message& message)
{
	if (MsgConfigureSDRdaemonUDPLink::match(message))
	{
		MsgConfigureSDRdaemonUDPLink& conf = (MsgConfigureSDRdaemonUDPLink&) message;
		m_SDRdaemonUDPHandler->configureUDPLink(conf.getAddress(), conf.getPort());
		return true;
	}
	else if (MsgConfigureSDRdaemonAutoCorr::match(message))
	{
		MsgConfigureSDRdaemonAutoCorr& conf = (MsgConfigureSDRdaemonAutoCorr&) message;
		bool dcBlock = conf.getDCBlock();
		bool iqImbalance = conf.getIQImbalance();
		m_deviceAPI->configureCorrections(dcBlock, iqImbalance);
		return true;
	}
	else if (MsgConfigureSDRdaemonWork::match(message))
	{
		MsgConfigureSDRdaemonWork& conf = (MsgConfigureSDRdaemonWork&) message;
		bool working = conf.isWorking();

		if (working) {
			m_SDRdaemonUDPHandler->start();
		} else {
			m_SDRdaemonUDPHandler->stop();
		}

		return true;
	}
	else if (MsgConfigureSDRdaemonStreamTiming::match(message))
	{
		return true;
	}
	else
	{
		return false;
	}
}

