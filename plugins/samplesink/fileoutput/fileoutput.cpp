///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <errno.h>
#include <QDebug>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"

#include "device/deviceapi.h"

#include "fileoutput.h"
#include "fileoutputworker.h"

MESSAGE_CLASS_DEFINITION(FileOutput::MsgConfigureFileOutput, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgConfigureFileOutputName, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgConfigureFileOutputWork, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgConfigureFileOutputStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgReportFileOutputGeneration, Message)
MESSAGE_CLASS_DEFINITION(FileOutput::MsgReportFileOutputStreamTiming, Message)

FileOutput::FileOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_fileOutputWorker(nullptr),
	m_deviceDescription("FileOutput"),
	m_fileName("./test.sdriq"),
	m_startingTimeStamp(0),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    m_deviceAPI->setNbSinkStreams(1);
}

FileOutput::~FileOutput()
{
	stop();
}

void FileOutput::destroy()
{
    delete this;
}

void FileOutput::openFileStream()
{
	if (m_ofstream.is_open()) {
		m_ofstream.close();
	}

	m_ofstream.open(m_fileName.toStdString().c_str(), std::ios::binary);

    FileRecord::Header header;
	int actualSampleRate = m_settings.m_sampleRate * (1<<m_settings.m_log2Interp);
    header.sampleRate = actualSampleRate;
    header.centerFrequency = m_settings.m_centerFrequency;
    m_startingTimeStamp = time(0);
    header.startTimeStamp = m_startingTimeStamp;
    header.sampleSize = SDR_RX_SAMP_SZ;

    FileRecord::writeHeader(m_ofstream, header);

	qDebug() << "FileOutput::openFileStream: " << m_fileName.toStdString().c_str();
}

void FileOutput::init()
{
    applySettings(m_settings, true);
}

bool FileOutput::start()
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "FileOutput::start";

	openFileStream();

	m_fileOutputWorker = new FileOutputWorker(&m_ofstream, &m_sampleSourceFifo);
    m_fileOutputWorker->moveToThread(&m_fileOutputWorkerThread);
	m_fileOutputWorker->setSamplerate(m_settings.m_sampleRate);
	m_fileOutputWorker->setLog2Interpolation(m_settings.m_log2Interp);
	m_fileOutputWorker->connectTimer(m_masterTimer);
	startWorker();

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("FileOutput::start: started");

    if (getMessageQueueToGUI())
    {
        MsgReportFileOutputGeneration *report = MsgReportFileOutputGeneration::create(true); // acquisition on
        getMessageQueueToGUI()->push(report);
    }

	return true;
}

void FileOutput::stop()
{
	qDebug() << "FileSourceInput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if (m_fileOutputWorker)
	{
		stopWorker();
		delete m_fileOutputWorker;
		m_fileOutputWorker = nullptr;
	}

    if (m_ofstream.is_open()) {
        m_ofstream.close();
    }

    if (getMessageQueueToGUI())
    {
        MsgReportFileOutputGeneration *report = MsgReportFileOutputGeneration::create(false); // acquisition off
        getMessageQueueToGUI()->push(report);
    }
}

void FileOutput::startWorker()
{
    m_fileOutputWorker->startWork();
    m_fileOutputWorkerThread.start();
}

void FileOutput::stopWorker()
{
    m_fileOutputWorker->stopWork();
    m_fileOutputWorkerThread.quit();
    m_fileOutputWorkerThread.wait();
}

QByteArray FileOutput::serialize() const
{
    return m_settings.serialize();
}

bool FileOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureFileOutput* message = MsgConfigureFileOutput::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFileOutput* messageToGUI = MsgConfigureFileOutput::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& FileOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FileOutput::getSampleRate() const
{
	return m_settings.m_sampleRate;
}

quint64 FileOutput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void FileOutput::setCenterFrequency(qint64 centerFrequency)
{
    FileOutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureFileOutput* message = MsgConfigureFileOutput::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFileOutput* messageToGUI = MsgConfigureFileOutput::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

std::time_t FileOutput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool FileOutput::handleMessage(const Message& message)
{
	if (MsgConfigureFileOutputName::match(message))
	{
		MsgConfigureFileOutputName& conf = (MsgConfigureFileOutputName&) message;
		m_fileName = conf.getFileName();
		openFileStream();
		return true;
	}
	else if (MsgStartStop::match(message))
	{
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "FileOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine())
            {
                m_deviceAPI->startDeviceEngine();
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine();
        }

        return true;
	}
	else if (MsgConfigureFileOutput::match(message))
    {
	    qDebug() << "FileOutput::handleMessage: MsgConfigureFileOutput";
	    MsgConfigureFileOutput& conf = (MsgConfigureFileOutput&) message;
        applySettings(conf.getSettings(), conf.getForce());
        return true;
    }
	else if (MsgConfigureFileOutputWork::match(message))
	{
		MsgConfigureFileOutputWork& conf = (MsgConfigureFileOutputWork&) message;
		bool working = conf.isWorking();

		if (m_fileOutputWorker != 0)
		{
			if (working) {
				startWorker();
			} else {
				stopWorker();
			}
		}

		return true;
	}
	else if (MsgConfigureFileOutputStreamTiming::match(message))
	{
        MsgReportFileOutputStreamTiming *report;

		if (m_fileOutputWorker != 0 && getMessageQueueToGUI())
		{
			report = MsgReportFileOutputStreamTiming::create(m_fileOutputWorker->getSamplesCount());
			getMessageQueueToGUI()->push(report);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void FileOutput::applySettings(const FileOutputSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);
    bool forwardChange = false;

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency))
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
        forwardChange = true;
    }

    if (force || (m_settings.m_sampleRate != settings.m_sampleRate))
    {
        m_settings.m_sampleRate = settings.m_sampleRate;

        if (m_fileOutputWorker != 0)
        {
            m_fileOutputWorker->setSamplerate(m_settings.m_sampleRate);
        }

        forwardChange = true;
    }

    if (force || (m_settings.m_log2Interp != settings.m_log2Interp))
    {
        m_settings.m_log2Interp = settings.m_log2Interp;

        if (m_fileOutputWorker != 0)
        {
            m_fileOutputWorker->setLog2Interpolation(m_settings.m_log2Interp);
        }

        forwardChange = true;
    }

    if (forwardChange)
    {
        qDebug("FileOutput::applySettings: forward: m_centerFrequency: %llu m_sampleRate: %llu m_log2Interp: %d",
                m_settings.m_centerFrequency,
                m_settings.m_sampleRate,
                m_settings.m_log2Interp);
        DSPSignalNotification *notif = new DSPSignalNotification(m_settings.m_sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

}

int FileOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int FileOutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgStartStop *messagetoGui = MsgStartStop::create(run);
        m_guiMessageQueue->push(messagetoGui);
    }

    return 200;
}


