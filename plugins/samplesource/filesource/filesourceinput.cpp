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
#include "dsp/filerecord.h"
#include "device/devicesourceapi.h"

#include "filesourcegui.h"
#include "filesourceinput.h"
#include "filesourcethread.h"

MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSource, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceWork, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceSeek, Message)
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
		d.readString(1, &m_fileName, "./test.sdriq");
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

FileSourceInput::FileSourceInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_fileSourceThread(NULL),
	m_deviceDescription(),
	m_fileName("..."),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_recordLength(0),
    m_startingTimeStamp(0),
    m_masterTimer(deviceAPI->getMasterTimer())
{
}

FileSourceInput::~FileSourceInput()
{
	stop();
}

void FileSourceInput::destroy()
{
    delete this;
}

void FileSourceInput::openFileStream()
{
	//stopInput();

	if (m_ifstream.is_open()) {
		m_ifstream.close();
	}

	m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
	quint64 fileSize = m_ifstream.tellg();
	m_ifstream.seekg(0,std::ios_base::beg);
	FileRecord::Header header;
	FileRecord::readHeader(m_ifstream, header);

	m_sampleRate = header.sampleRate;
	m_centerFrequency = header.centerFrequency;
	m_startingTimeStamp = header.startTimeStamp;

	if (fileSize > sizeof(FileRecord::Header)) {
		m_recordLength = (fileSize - sizeof(FileRecord::Header)) / (4 * m_sampleRate);
	} else {
		m_recordLength = 0;
	}

	qDebug() << "FileSourceInput::openFileStream: " << m_fileName.toStdString().c_str()
			<< " fileSize: " << fileSize << "bytes"
			<< " length: " << m_recordLength << " seconds";

	MsgReportFileSourceStreamData *report = MsgReportFileSourceStreamData::create(m_sampleRate,
			m_centerFrequency,
			m_startingTimeStamp,
			m_recordLength); // file stream data

	if (getMessageQueueToGUI()) {
	    getMessageQueueToGUI()->push(report);
	}
}

void FileSourceInput::seekFileStream(int seekPercentage)
{
	QMutexLocker mutexLocker(&m_mutex);

	if ((m_ifstream.is_open()) && !m_fileSourceThread->isRunning())
	{
		int seekPoint = ((m_recordLength * seekPercentage) / 100) * m_sampleRate;
		m_fileSourceThread->setSamplesCount(seekPoint);
		seekPoint *= 4; // + sizeof(FileSink::Header)
		m_ifstream.clear();
		m_ifstream.seekg(seekPoint + sizeof(FileRecord::Header), std::ios::beg);
	}
}

bool FileSourceInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "FileSourceInput::start";

	if (m_ifstream.tellg() != 0) {
		m_ifstream.clear();
		m_ifstream.seekg(sizeof(FileRecord::Header), std::ios::beg);
	}

	if(!m_sampleFifo.setSize(m_sampleRate * 4)) {
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

	if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(report);
	}

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

	if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(report);
	}
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
	else if (MsgConfigureFileSourceSeek::match(message))
	{
		MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) message;
		int seekPercentage = conf.getPercentage();
		seekFileStream(seekPercentage);

		return true;
	}
	else if (MsgConfigureFileSourceStreamTiming::match(message))
	{
		MsgReportFileSourceStreamTiming *report;

		if (m_fileSourceThread != 0)
		{
			report = MsgReportFileSourceStreamTiming::create(m_fileSourceThread->getSamplesCount());

			if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(report);
			}
		}

		return true;
	}
	else
	{
		return false;
	}
}
