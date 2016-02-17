///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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
#include "dsp/filesink.h"

#include "sdrdaemongui.h"
#include "sdrdaemoninput.h"
#include "sdrdaemonudphandler.h"

MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgConfigureSDRdaemonUDPLink, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgConfigureSDRdaemonAutoCorr, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgConfigureSDRdaemonWork, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgConfigureSDRdaemonStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgReportSDRdaemonAcquisition, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgReportSDRdaemonStreamData, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgReportSDRdaemonStreamTiming, Message)

SDRdaemonInput::SDRdaemonInput(const QTimer& masterTimer) :
	m_address("127.0.0.1"),
	m_port(9090),
	m_SDRdaemonUDPHandler(0),
	m_deviceDescription(),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_startingTimeStamp(0),
	m_masterTimer(masterTimer)
{
	m_sampleFifo.setSize(96000 * 4);
	m_SDRdaemonUDPHandler = new SDRdaemonUDPHandler(&m_sampleFifo, getOutputMessageQueueToGUI());
	m_SDRdaemonUDPHandler->connectTimer(m_masterTimer);
}

SDRdaemonInput::~SDRdaemonInput()
{
	stop();
	delete m_SDRdaemonUDPHandler;
}

bool SDRdaemonInput::init(const Message& message)
{
	qDebug() << "SDRdaemonInput::init";
	return false;
}

bool SDRdaemonInput::start(int device)
{
	qDebug() << "SDRdaemonInput::start";
	MsgConfigureSDRdaemonWork *command = MsgConfigureSDRdaemonWork::create(true);
	getInputMessageQueue()->push(command);
	return true;
}

void SDRdaemonInput::stop()
{
	qDebug() << "SDRdaemonInput::stop";
	MsgConfigureSDRdaemonWork *command = MsgConfigureSDRdaemonWork::create(false);
	getInputMessageQueue()->push(command);
}

const QString& SDRdaemonInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int SDRdaemonInput::getSampleRate() const
{
	return m_sampleRate;
}

quint64 SDRdaemonInput::getCenterFrequency() const
{
	return m_centerFrequency;
}

std::time_t SDRdaemonInput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool SDRdaemonInput::handleMessage(const Message& message)
{
	if (MsgConfigureSDRdaemonUDPLink::match(message))
	{
		// TODO: change UDP settings
		return true;
	}
	else if (MsgConfigureSDRdaemonAutoCorr::match(message))
	{
		MsgConfigureSDRdaemonAutoCorr& conf = (MsgConfigureSDRdaemonAutoCorr&) message;
		bool dcBlock = conf.getDCBlock();
		bool iqImbalance = conf.getIQImbalance();
		DSPEngine::instance()->configureCorrections(dcBlock, iqImbalance);
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

