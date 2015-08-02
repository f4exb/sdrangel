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
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportFileSource, Message)

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
	m_ifstream.open(m_settings.m_fileName.toStdString().c_str(), std::ios::binary);
	FileSink::Header header;
	FileSink::readHeader(m_ifstream, header);

	m_sampleRate = header.sampleRate;
	m_centerFrequency = header.centerFrequency;
	m_startingTimeStamp = header.startTimeStamp;
}

bool FileSourceInput::startInput(int device)
{
	QMutexLocker mutexLocker(&m_mutex);

	openFileStream();

	if((m_fileSourceThread = new FileSourceThread(&m_ifstream, &m_sampleFifo)) == NULL) {
		qFatal("out of memory");
		goto failed;
	}

	m_fileSourceThread->setSamplerate(m_sampleRate);
	m_fileSourceThread->connectTimer(m_masterTimer);
	m_fileSourceThread->startWork();

	mutexLocker.unlock();
	applySettings(m_generalSettings, m_settings, true);

	qDebug("bladerfInput: start");
	//MsgReportBladerf::create(m_gains)->submit(m_guiMessageQueue); Pass anything here

	return true;

failed:
	stopInput();
	return false;
}

void FileSourceInput::stopInput()
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_fileSourceThread != NULL) {
		m_fileSourceThread->stopWork();
		delete m_fileSourceThread;
		m_fileSourceThread = NULL;
	}

	m_deviceDescription.clear();
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
	if(MsgConfigureFileSource::match(message)) {
		MsgConfigureFileSource* conf = (MsgConfigureFileSource*)message;
		if(!applySettings(conf->getGeneralSettings(), conf->getSettings(), false))
			qDebug("File Source config error");
		message->completed();
		return true;
	} else {
		return false;
	}
}

bool FileSourceInput::applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);

	if((m_settings.m_fileName != settings.m_fileName) || force) {
		m_settings.m_fileName = settings.m_fileName;

		if (m_fileSourceThread != 0) {
			m_fileSourceThread->stopWork();
		}

		if (m_ifstream.is_open()) {
			m_ifstream.close();
		}

		openFileStream();

		if (m_fileSourceThread != 0) {
			m_fileSourceThread->setSamplerate(m_sampleRate);
		}

		std::cerr << "FileSourceInput: center freq: " << m_generalSettings.m_centerFrequency << " Hz"
				<< " file name: " << settings.m_fileName.toStdString() << std::endl;
	}

	return true;
}
