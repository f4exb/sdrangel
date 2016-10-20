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
#include "dsp/filerecord.h"

#include "filesinkgui.h"
#include "filesinkoutput.h"
#include "filesinkthread.h"

MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgConfigureFileSink, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgConfigureFileSinkName, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgConfigureFileSinkWork, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgConfigureFileSinkStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgReportFileSinkGeneration, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgReportFileSinkStreamTiming, Message)

FileSinkOutput::FileSinkOutput(const QTimer& masterTimer) :
	m_settings(),
	m_fileSinkThread(0),
	m_deviceDescription(),
	m_fileName("..."),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_startingTimeStamp(0),
	m_masterTimer(masterTimer)
{
}

FileSinkOutput::~FileSinkOutput()
{
	stop();
}

void FileSinkOutput::openFileStream()
{
	//stopInput();

	if (m_ofstream.is_open()) {
		m_ofstream.close();
	}

	m_ofstream.open(m_fileName.toStdString().c_str(), std::ios::binary);

	m_ofstream.write((const char *) &m_settings.m_sampleRate, sizeof(int));
	m_ofstream.write((const char *) &m_settings.m_centerFrequency, sizeof(quint64));
    m_startingTimeStamp = time(0);
    m_ofstream.write((const char *) &m_startingTimeStamp, sizeof(std::time_t));

	qDebug() << "FileSinkOutput::openFileStream: " << m_fileName.toStdString().c_str();
}

bool FileSinkOutput::init(const Message& message)
{
	return false;
}

bool FileSinkOutput::start(int device)
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "FileSinkOutput::start";

	//openFileStream();

	if((m_fileSinkThread = new FileSinkThread(&m_ofstream, &m_sampleSourceFifo)) == 0) {
		qFatal("out of memory");
		stop();
		return false;
	}

	m_fileSinkThread->setSamplerate(m_sampleRate);
	m_fileSinkThread->connectTimer(m_masterTimer);
	m_fileSinkThread->startWork();
	m_deviceDescription = "FileSink";

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("FileSinkOutput::start: started");

	MsgReportFileSinkGeneration *report = MsgReportFileSinkGeneration::create(true); // acquisition on
	getOutputMessageQueueToGUI()->push(report);

	return true;
}

void FileSinkOutput::stop()
{
	qDebug() << "FileSourceInput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if(m_fileSinkThread != 0)
	{
		m_fileSinkThread->stopWork();
		delete m_fileSinkThread;
		m_fileSinkThread = 0;
	}

	m_deviceDescription.clear();

	MsgReportFileSinkGeneration *report = MsgReportFileSinkGeneration::create(false); // acquisition off
	getOutputMessageQueueToGUI()->push(report);
}

const QString& FileSinkOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FileSinkOutput::getSampleRate() const
{
	return m_sampleRate;
}

quint64 FileSinkOutput::getCenterFrequency() const
{
	return m_centerFrequency;
}

std::time_t FileSinkOutput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool FileSinkOutput::handleMessage(const Message& message)
{
	if (MsgConfigureFileSinkName::match(message))
	{
		MsgConfigureFileSinkName& conf = (MsgConfigureFileSinkName&) message;
		m_fileName = conf.getFileName();
		openFileStream();
		return true;
	}
	else if (MsgConfigureFileSink::match(message))
    {
	    MsgConfigureFileSink& conf = (MsgConfigureFileSink&) message;
        applySettings(conf.getSettings(), false);
        return true;
    }
	else if (MsgConfigureFileSinkWork::match(message))
	{
		MsgConfigureFileSinkWork& conf = (MsgConfigureFileSinkWork&) message;
		bool working = conf.isWorking();

		if (m_fileSinkThread != 0)
		{
			if (working)
			{
				m_fileSinkThread->startWork();
			}
			else
			{
				m_fileSinkThread->stopWork();
			}
		}

		return true;
	}
	else if (MsgConfigureFileSinkStreamTiming::match(message))
	{
		MsgReportFileSinkStreamTiming *report;

		if (m_fileSinkThread != 0)
		{
			report = MsgReportFileSinkStreamTiming::create(m_fileSinkThread->getSamplesCount());
			getOutputMessageQueueToGUI()->push(report);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void FileSinkOutput::applySettings(const FileSinkSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency))
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
    }

    if (force || (m_settings.m_sampleRate != settings.m_sampleRate))
    {
        m_settings.m_sampleRate = settings.m_sampleRate;
    }
}
