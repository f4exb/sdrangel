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

#include "SWGDeviceSettings.h"
#include "SWGFileSourceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGFileSourceSettings.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"
#include "device/devicesourceapi.h"

#include "filesourceinput.h"
#include "filesourcethread.h"

MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSource, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceWork, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportFileSourceAcquisition, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(FileSourceInput::MsgReportFileSourceStreamTiming, Message)

FileSourceInput::FileSourceInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_fileSourceThread(NULL),
	m_deviceDescription(),
	m_fileName("..."),
	m_sampleRate(0),
	m_sampleSize(0),
	m_centerFrequency(0),
	m_recordLength(0),
    m_startingTimeStamp(0),
    m_masterTimer(deviceAPI->getMasterTimer())
{
    qDebug("FileSourceInput::FileSourceInput: device source engine: %p", m_deviceAPI->getDeviceSourceEngine());
    qDebug("FileSourceInput::FileSourceInput: device source engine message queue: %p", m_deviceAPI->getDeviceEngineInputMessageQueue());
    qDebug("FileSourceInput::FileSourceInput: device source: %p", m_deviceAPI->getDeviceSourceEngine()->getSource());
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
	m_sampleSize = header.sampleSize;

	if (fileSize > sizeof(FileRecord::Header)) {
		m_recordLength = (fileSize - sizeof(FileRecord::Header)) / (4 * m_sampleRate);
	} else {
		m_recordLength = 0;
	}

	qDebug() << "FileSourceInput::openFileStream: " << m_fileName.toStdString().c_str()
			<< " fileSize: " << fileSize << "bytes"
			<< " length: " << m_recordLength << " seconds";

	if (getMessageQueueToGUI()) {
	    MsgReportFileSourceStreamData *report = MsgReportFileSourceStreamData::create(m_sampleRate,
	            m_sampleSize,
	            m_centerFrequency,
	            m_startingTimeStamp,
	            m_recordLength); // file stream data
	    getMessageQueueToGUI()->push(report);
	}
}

void FileSourceInput::seekFileStream(int seekPercentage)
{
	QMutexLocker mutexLocker(&m_mutex);

	if ((m_ifstream.is_open()) && m_fileSourceThread && !m_fileSourceThread->isRunning())
	{
		int seekPoint = ((m_recordLength * seekPercentage) / 100) * m_sampleRate;
		m_fileSourceThread->setSamplesCount(seekPoint);
		seekPoint *= 4; // + sizeof(FileSink::Header)
		m_ifstream.clear();
		m_ifstream.seekg(seekPoint + sizeof(FileRecord::Header), std::ios::beg);
	}
}

void FileSourceInput::init()
{
    DSPSignalNotification *notif = new DSPSignalNotification(m_settings.m_sampleRate, m_settings.m_centerFrequency);
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
}

bool FileSourceInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "FileSourceInput::start";

	if (m_ifstream.tellg() != 0) {
		m_ifstream.clear();
		m_ifstream.seekg(sizeof(FileRecord::Header), std::ios::beg);
	}

	if(!m_sampleFifo.setSize(m_sampleRate * sizeof(Sample))) {
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	//openFileStream();

	m_fileSourceThread = new FileSourceThread(&m_ifstream, &m_sampleFifo);
	m_fileSourceThread->setSampleRateAndSize(m_sampleRate, m_sampleSize);
	m_fileSourceThread->connectTimer(m_masterTimer);
	m_fileSourceThread->startWork();
	m_deviceDescription = "FileSource";

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("FileSourceInput::startInput: started");

	if (getMessageQueueToGUI()) {
        MsgReportFileSourceAcquisition *report = MsgReportFileSourceAcquisition::create(true); // acquisition on
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

	if (getMessageQueueToGUI()) {
        MsgReportFileSourceAcquisition *report = MsgReportFileSourceAcquisition::create(false); // acquisition off
        getMessageQueueToGUI()->push(report);
	}
}

QByteArray FileSourceInput::serialize() const
{
    return m_settings.serialize();
}

bool FileSourceInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureFileSource* message = MsgConfigureFileSource::create(m_settings);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI())
    {
        MsgConfigureFileSource* messageToGUI = MsgConfigureFileSource::create(m_settings);
        getMessageQueueToGUI()->push(messageToGUI);
    }

    return success;
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

void FileSourceInput::setCenterFrequency(qint64 centerFrequency)
{
    FileSourceSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureFileSource* message = MsgConfigureFileSource::create(m_settings);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI())
    {
        MsgConfigureFileSource* messageToGUI = MsgConfigureFileSource::create(m_settings);
        getMessageQueueToGUI()->push(messageToGUI);
    }
}

std::time_t FileSourceInput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool FileSourceInput::handleMessage(const Message& message)
{
    if (MsgConfigureFileSource::match(message))
    {
        MsgConfigureFileSource& conf = (MsgConfigureFileSource&) message;
        FileSourceSettings settings = conf.getSettings();
        applySettings(settings);
        return true;
    }
    else if (MsgConfigureFileSourceName::match(message))
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
			if (getMessageQueueToGUI()) {
                report = MsgReportFileSourceStreamTiming::create(m_fileSourceThread->getSamplesCount());
                getMessageQueueToGUI()->push(report);
			}
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "FileSourceInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initAcquisition())
            {
                m_deviceAPI->startAcquisition();
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
        }

        return true;
    }
	else
	{
		return false;
	}
}

bool FileSourceInput::applySettings(const FileSourceSettings& settings, bool force)
{
    if ((m_settings.m_centerFrequency != settings.m_centerFrequency) || force) {
        m_centerFrequency = settings.m_centerFrequency;
    }

    m_settings = settings;
    return true;
}

int FileSourceInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setFileSourceSettings(new SWGSDRangel::SWGFileSourceSettings());
    response.getFileSourceSettings()->setFileName(new QString(m_settings.m_fileName));
    return 200;
}

int FileSourceInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int FileSourceInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        getMessageQueueToGUI()->push(msgToGUI);
    }

    return 200;
}

int FileSourceInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setFileSourceReport(new SWGSDRangel::SWGFileSourceReport());
    response.getFileSourceReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void FileSourceInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    int t_sec = 0;
    int t_msec = 0;
    std::size_t samplesCount = 0;

    if (m_fileSourceThread) {
        samplesCount = m_fileSourceThread->getSamplesCount();
    }

    if (m_sampleRate > 0)
    {
        t_sec = samplesCount / m_sampleRate;
        t_msec = (samplesCount - (t_sec * m_sampleRate)) * 1000 / m_sampleRate;
    }

    QTime t(0, 0, 0, 0);
    t = t.addSecs(t_sec);
    t = t.addMSecs(t_msec);
    response.getFileSourceReport()->setElapsedTime(new QString(t.toString("HH:mm:ss.zzz")));

    quint64 startingTimeStampMsec = (quint64) m_startingTimeStamp * 1000LL;
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
    dt = dt.addSecs((quint64) t_sec);
    dt = dt.addMSecs((quint64) t_msec);
    response.getFileSourceReport()->setAbsoluteTime(new QString(dt.toString("yyyy-MM-dd HH:mm:ss.zzz")));

    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    response.getFileSourceReport()->setDurationTime(new QString(recordLength.toString("HH:mm:ss")));

    response.getFileSourceReport()->setFileName(new QString(m_fileName));
    response.getFileSourceReport()->setSampleRate(m_sampleRate);
    response.getFileSourceReport()->setSampleSize(m_sampleSize);
}


