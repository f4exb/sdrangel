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
#include <cstdio>
#include <iostream>

#include "util/simpleserializer.h"
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

FileSourceInput::FileSourceInput(MessageQueue* msgQueueToGUI, const QTimer& masterTimer) :
	SampleSource(msgQueueToGUI),
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
	stopInput();
}

void FileSourceInput::openFileStream()
{
	std::cerr << "FileSourceInput::openFileStream: " << m_fileName.toStdString() << std::endl;

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

	MsgReportFileSourceStreamData::create(m_sampleRate, m_centerFrequency, m_startingTimeStamp)->submit(m_guiMessageQueue); // file stream data
}

bool FileSourceInput::startInput(int device)
{
	QMutexLocker mutexLocker(&m_mutex);
	std::cerr << "FileSourceInput::startInput" << std::endl;

	/*
	if (!m_ifstream.is_open()) {
		openFileStream();
	}*/

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
		goto failed;
	}

	m_fileSourceThread->setSamplerate(m_sampleRate);
	m_fileSourceThread->connectTimer(m_masterTimer);
	m_fileSourceThread->startWork();

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);

	MsgReportFileSourceAcquisition::create(true)->submit(m_guiMessageQueue); // acquisition on

	return true;

failed:
	stopInput();
	return false;
}

void FileSourceInput::stopInput()
{
	std::cerr << "FileSourceInput::stopInput" << std::endl;
	QMutexLocker mutexLocker(&m_mutex);

	if(m_fileSourceThread != NULL) {
		m_fileSourceThread->stopWork();
		delete m_fileSourceThread;
		m_fileSourceThread = NULL;
	}

	m_deviceDescription.clear();

	MsgReportFileSourceAcquisition::create(false)->submit(m_guiMessageQueue); // acquisition off
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

bool FileSourceInput::handleMessage(Message* message)
{
	if (MsgConfigureFileSourceName::match(message))
	{
		//std::cerr << "FileSourceInput::handleMessage: MsgConfigureFileName" << std::endl;
		MsgConfigureFileSourceName* conf = (MsgConfigureFileSourceName*) message;
		m_fileName = conf->getFileName();
		openFileStream();
		message->completed();
		return true;
	}
	else if (MsgConfigureFileSourceWork::match(message))
	{
		//std::cerr << "FileSourceInput::handleMessage: MsgConfigureFileSourceWork: ";
		MsgConfigureFileSourceWork* conf = (MsgConfigureFileSourceWork*) message;
		bool working = conf->isWorking();
		//std::cerr << (working ? "working" : "not working") << std::endl;
		if (m_fileSourceThread != 0)
		{
			if (working) {
				m_fileSourceThread->startWork();
			} else {
				m_fileSourceThread->stopWork();
			}

			MsgReportFileSourceStreamTiming::create(m_fileSourceThread->getSamplesCount())->submit(m_guiMessageQueue);
		}
		message->completed();
		return true;
	}
	else if (MsgConfigureFileSourceStreamTiming::match(message))
	{
		if (m_fileSourceThread != 0) {
			MsgReportFileSourceStreamTiming::create(m_fileSourceThread->getSamplesCount())->submit(m_guiMessageQueue);
		}else {
			MsgReportFileSourceStreamTiming::create(0)->submit(m_guiMessageQueue);
		}
	}
	else
	{
		return false;
	}
}

bool FileSourceInput::applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);
	bool wasRunning = false;

	if((m_settings.m_fileName != settings.m_fileName) || force) {
		m_settings.m_fileName = settings.m_fileName;

		if (m_fileSourceThread != 0) {
			wasRunning = m_fileSourceThread->isRunning();
			if (wasRunning) {
				m_fileSourceThread->stopWork();
			}
		}

		if (m_ifstream.is_open()) {
			m_ifstream.close();
		}

		openFileStream();

		if (m_fileSourceThread != 0) {
			m_fileSourceThread->setSamplerate(m_sampleRate);
			if (wasRunning) {
				m_fileSourceThread->startWork();
			}
		}

		std::cerr << "FileSourceInput::applySettings:"
				<< " file name: " << settings.m_fileName.toStdString()
				<< " center freq: " << m_centerFrequency << " Hz"
				<< " sample rate: " << m_sampleRate
				<< " Unix timestamp: " << m_startingTimeStamp << std::endl;
	}

	return true;
}
