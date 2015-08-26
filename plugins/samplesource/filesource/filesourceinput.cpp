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
#include "dsp/filesink.h"

#include "filesourcegui.h"
#include "filesourceinput.h"
#include "filesourcethread.h"

MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSource, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceWork, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportFileSourceAcquisition, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportFileSourceStreamTiming, Message)

FileSourceInput::Settings::Settings() :
	m_fileName("./test.sdriq")
{
}

void FileSourceInput::Settings::resetToDefaults()
{
	m_fileName = "./test.sdriq";
}

QByteArray FileSourceInput::Settings::serialize() const
{
	SimpleSerializer s(1);
	s.writeString(1, m_fileName);
	return s.final();
}

bool FileSourceInput::Settings::deserialize(const QByteArray& data)
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

FileSourceInput::FileSourceInput(const QTimer& masterTimer) :
	m_settings(),
	m_fileSourceThread(NULL),
	m_deviceDescription(),
	m_fileName("..."),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_startingTimeStamp(0),
	m_masterTimer(masterTimer)
{
}

FileSourceInput::~FileSourceInput()
{
	stop();
}

void FileSourceInput::openFileStream()
{
	qDebug() << "FileSourceInput::openFileStream: " << m_fileName.toStdString().c_str();

	//stopInput();

	if (m_ifstream.is_open()) {
		m_ifstream.close();
	}

	m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary);
	FileSink::Header header;
	FileSink::readHeader(m_ifstream, header);

	m_sampleRate = header.sampleRate;
	m_centerFrequency = header.centerFrequency;
	m_startingTimeStamp = header.startTimeStamp;

	MsgReportFileSourceStreamData *report = MsgReportFileSourceStreamData::create(m_sampleRate, m_centerFrequency, m_startingTimeStamp); // file stream data
	getOutputMessageQueue()->push(report);
}

bool FileSourceInput::init(const Message& message)
{
	return false;
}

bool FileSourceInput::start(int device)
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "FileSourceInput::startInput";

	if (m_ifstream.tellg() != 0) {
		m_ifstream.clear();
		m_ifstream.seekg(0, std::ios::beg);
	}

	if(!m_sampleFifo.setSize(96000 * 4)) {
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	//openFileStream();

	if((m_fileSourceThread = new FileSourceThread(&m_ifstream, &m_sampleFifo)) == NULL) {
		qFatal("out of memory");
		stop();
		return false;
	}

	m_fileSourceThread->setSamplerate(m_sampleRate);
	m_fileSourceThread->connectTimer(m_masterTimer);
	m_fileSourceThread->startWork();
	m_deviceDescription = "FileSource";

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("FileSourceInput::startInput: started");

	MsgReportFileSourceAcquisition *report = MsgReportFileSourceAcquisition::create(true); // acquisition on
	getOutputMessageQueue()->push(report);

	return true;
}

void FileSourceInput::stop()
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

	MsgReportFileSourceAcquisition *report = MsgReportFileSourceAcquisition::create(false); // acquisition off
	getOutputMessageQueue()->push(report);
}

const QString& FileSourceInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FileSourceInput::getSampleRate() const
{
	return m_sampleRate;
}

quint64 FileSourceInput::getCenterFrequency() const
{
	return m_centerFrequency;
}

std::time_t FileSourceInput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool FileSourceInput::handleMessage(const Message& message)
{
	if (MsgConfigureFileSourceName::match(message))
	{
		MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) message;
		m_fileName = conf.getFileName();
		openFileStream();
		return true;
	}
	else if (MsgConfigureFileSourceWork::match(message))
	{
		MsgConfigureFileSourceWork& conf = (MsgConfigureFileSourceWork&) message;
		bool working = conf.isWorking();

		if (m_fileSourceThread != 0)
		{
			if (working)
			{
				m_fileSourceThread->startWork();
			}
			else
			{
				m_fileSourceThread->stopWork();
			}

			MsgReportFileSourceStreamTiming *report =
					MsgReportFileSourceStreamTiming::create(m_fileSourceThread->getSamplesCount());
			getOutputMessageQueue()->push(report);
		}

		return true;
	}
	else if (MsgConfigureFileSourceStreamTiming::match(message))
	{
		MsgReportFileSourceStreamTiming *report;

		if (m_fileSourceThread != 0)
		{
			report = MsgReportFileSourceStreamTiming::create(m_fileSourceThread->getSamplesCount());
		}
		else
		{
			report = MsgReportFileSourceStreamTiming::create(0);
		}

		getOutputMessageQueue()->push(report);
	}
	else
	{
		return false;
	}
}

bool FileSourceInput::applySettings(const Settings& settings, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);
	bool wasRunning = false;

	if((m_settings.m_fileName != settings.m_fileName) || force)
	{
		m_settings.m_fileName = settings.m_fileName;

		if (m_fileSourceThread != 0)
		{
			wasRunning = m_fileSourceThread->isRunning();

			if (wasRunning)
			{
				m_fileSourceThread->stopWork();
			}
		}

		if (m_ifstream.is_open())
		{
			m_ifstream.close();
		}

		openFileStream();

		if (m_fileSourceThread != 0)
		{
			m_fileSourceThread->setSamplerate(m_sampleRate);

			if (wasRunning)
			{
				m_fileSourceThread->startWork();
			}
		}
        
		DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
		getOutputMessageQueue()->push(notif);                

		qDebug() << "FileSourceInput::applySettings:"
				<< " file name: " << settings.m_fileName.toStdString().c_str()
				<< " center freq: " << m_centerFrequency << " Hz"
				<< " sample rate: " << m_sampleRate
				<< " Unix timestamp: " << m_startingTimeStamp;
	}

	return true;
}
