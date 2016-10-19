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
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgConfigureFileSinkSeek, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgConfigureFileSinkStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgReportFileSinkGeneration, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgReportFileSinkStreamData, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgReportFileSinkStreamTiming, Message)

FileSinkOutput::Settings::Settings() :
	m_fileName("./test.sdriq")
{
}

void FileSinkOutput::Settings::resetToDefaults()
{
	m_fileName = "./test.sdriq";
}

QByteArray FileSinkOutput::Settings::serialize() const
{
	SimpleSerializer s(1);
	s.writeString(1, m_fileName);
	return s.final();
}

bool FileSinkOutput::Settings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		int intval;
		d.readString(1, &m_fileName, "./test.sdriq");
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

FileSinkOutput::FileSinkOutput(const QTimer& masterTimer) :
	m_settings(),
	m_fileSourceThread(0),
	m_deviceDescription(),
	m_fileName("..."),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_recordLength(0),
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
	FileRecord::Header header;

	header.sampleRate = m_sampleRate;
	header.centerFrequency = m_centerFrequency;
	header.startTimeStamp = m_startingTimeStamp; // TODO: set timestamp

	qDebug() << "FileSinkOutput::openFileStream: " << m_fileName.toStdString().c_str();

	MsgReportFileSinkStreamData *report = MsgReportFileSinkStreamData::create(m_sampleRate,
			m_centerFrequency,
			m_startingTimeStamp); // file stream data

	getOutputMessageQueueToGUI()->push(report);
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

	if((m_fileSourceThread = new FileSinkThread(&m_ifstream, &m_sampleFifo)) == 0) {
		qFatal("out of memory");
		stop();
		return false;
	}

	m_fileSourceThread->setSamplerate(m_sampleRate);
	m_fileSourceThread->connectTimer(m_masterTimer);
	m_fileSourceThread->startWork();
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

	if(m_fileSourceThread != 0)
	{
		m_fileSourceThread->stopWork();
		delete m_fileSourceThread;
		m_fileSourceThread = 0;
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
	else if (MsgConfigureFileSinkWork::match(message))
	{
		MsgConfigureFileSinkWork& conf = (MsgConfigureFileSinkWork&) message;
		bool working = conf.isWorking();

		if (m_fileSourceThread != 0)
		{
			if (working)
			{
				m_fileSourceThread->startWork();
				/*
				MsgReportFileSourceStreamTiming *report =
						MsgReportFileSourceStreamTiming::create(m_fileSourceThread->getSamplesCount());
				getOutputMessageQueueToGUI()->push(report);*/
			}
			else
			{
				m_fileSourceThread->stopWork();
			}
		}

		return true;
	}
	else if (MsgConfigureFileSinkSeek::match(message))
	{
		MsgConfigureFileSinkSeek& conf = (MsgConfigureFileSinkSeek&) message;
		int seekPercentage = conf.getPercentage();
		seekFileStream(seekPercentage);

		return true;
	}
	else if (MsgConfigureFileSinkStreamTiming::match(message))
	{
		MsgReportFileSinkStreamTiming *report;

		if (m_fileSourceThread != 0)
		{
			report = MsgReportFileSinkStreamTiming::create(m_fileSourceThread->getSamplesCount());
			getOutputMessageQueueToGUI()->push(report);
		}

		return true;
	}
	else
	{
		return false;
	}
}
