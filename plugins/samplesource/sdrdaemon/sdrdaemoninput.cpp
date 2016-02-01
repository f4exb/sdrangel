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
#include "sdrdaemonthread.h"

MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgConfigureSDRdaemonUDPLink, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgConfigureSDRdaemonWork, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgConfigureSDRdaemonStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgReportSDRdaemonAcquisition, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgReportSDRdaemonStreamData, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonInput::MsgReportSDRdaemonStreamTiming, Message)

SDRdaemonInput::SDRdaemonInput(const QTimer& masterTimer) :
	m_address("127.0.0.1"),
	m_port(9090),
	m_SDRdaemonThread(NULL),
	m_deviceDescription(),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_startingTimeStamp(0),
	m_masterTimer(masterTimer)
{
}

SDRdaemonInput::~SDRdaemonInput()
{
	stop();
}

bool SDRdaemonInput::init(const Message& message)
{
	return false;
}

bool SDRdaemonInput::start(int device)
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "SDRdaemonInput::startInput";

	if(!m_sampleFifo.setSize(96000 * 4)) {
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	if((m_SDRdaemonThread = new SDRdaemonThread(&m_sampleFifo)) == NULL) {
		qFatal("out of memory");
		stop();
		return false;
	}

	m_SDRdaemonThread->setSamplerate(m_sampleRate);
	m_SDRdaemonThread->connectTimer(m_masterTimer);
	m_SDRdaemonThread->startWork();
	m_deviceDescription = "SDRdaemon";

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("SDRdaemonInput::startInput: started");

	MsgReportSDRdaemonAcquisition *report = MsgReportSDRdaemonAcquisition::create(true); // acquisition on
	getOutputMessageQueueToGUI()->push(report);

	return true;
}

void SDRdaemonInput::stop()
{
	qDebug() << "SDRdaemonInput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if(m_SDRdaemonThread != 0)
	{
		m_SDRdaemonThread->stopWork();
		delete m_SDRdaemonThread;
		m_SDRdaemonThread = 0;
	}

	m_deviceDescription.clear();

	MsgReportSDRdaemonAcquisition *report = MsgReportSDRdaemonAcquisition::create(false); // acquisition off
	getOutputMessageQueueToGUI()->push(report);
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
		MsgConfigureSDRdaemonUDPLink& conf = (MsgConfigureSDRdaemonUDPLink&) message;
		updateLink(conf.getAddress(), conf.getPort());
		return true;
	}
	else if (MsgConfigureSDRdaemonWork::match(message))
	{
		MsgConfigureSDRdaemonWork& conf = (MsgConfigureSDRdaemonWork&) message;
		bool working = conf.isWorking();

		if (m_SDRdaemonThread != 0)
		{
			if (working)
			{
				m_SDRdaemonThread->startWork();
				MsgReportSDRdaemonStreamTiming *report =
						MsgReportSDRdaemonStreamTiming::create(m_SDRdaemonThread->getSamplesCount());
				getOutputMessageQueueToGUI()->push(report);
			}
			else
			{
				m_SDRdaemonThread->stopWork();
			}
		}

		return true;
	}
	else if (MsgConfigureSDRdaemonStreamTiming::match(message))
	{
		MsgReportSDRdaemonStreamTiming *report;

		if (m_SDRdaemonThread != 0)
		{
			report = MsgReportSDRdaemonStreamTiming::create(m_SDRdaemonThread->getSamplesCount());
			getOutputMessageQueueToGUI()->push(report);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void SDRdaemonInput::updateLink(const QString& address, quint16 port)
{
	QMutexLocker mutexLocker(&m_mutex);
	bool wasRunning = false;

	if ((m_address != address) || (m_port != port))
	{
		if (m_SDRdaemonThread != 0)
		{
			wasRunning = m_SDRdaemonThread->isRunning();

			if (wasRunning)
			{
				m_SDRdaemonThread->stopWork();
			}
		}

		if (m_SDRdaemonThread != 0)
		{
			m_SDRdaemonThread->updateLink(address, port);

			if (wasRunning)
			{
				m_SDRdaemonThread->startWork();
			}
		}

		m_address = address;
		m_port = port;

		qDebug() << "SDRdaemonInput::updateLink:"
				<< " address: " << m_address.toStdString().c_str()
				<< "port: " << m_port;
	}
}

void SDRdaemonInput:: updateSampleRate(int sampleRate)
{
	QMutexLocker mutexLocker(&m_mutex);
	bool wasRunning = false;

	if (m_sampleRate != sampleRate)
	{
		m_sampleRate = sampleRate;

		if (m_SDRdaemonThread != 0)
		{
			wasRunning = m_SDRdaemonThread->isRunning();

			if (wasRunning)
			{
				m_SDRdaemonThread->stopWork();
			}
		}

		if (m_SDRdaemonThread != 0)
		{
			m_SDRdaemonThread->setSamplerate(m_sampleRate);

			if (wasRunning)
			{
				m_SDRdaemonThread->startWork();
			}
		}
        
		DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
		DSPEngine::instance()->getInputMessageQueue()->push(notif);

		qDebug() << "SDRdaemonInput::updateSampleRate:"
				<< " sample rate: " << m_sampleRate;
	}
}
