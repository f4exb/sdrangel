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

#include <string.h>
#include <errno.h>
#include <QDebug>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"

#include "device/deviceapi.h"

#include "filesinkoutput.h"
#include "filesinkthread.h"

MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgConfigureFileSink, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgConfigureFileSinkName, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgConfigureFileSinkWork, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgConfigureFileSinkStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgReportFileSinkGeneration, Message)
MESSAGE_CLASS_DEFINITION(FileSinkOutput::MsgReportFileSinkStreamTiming, Message)

FileSinkOutput::FileSinkOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_fileSinkThread(0),
	m_deviceDescription("FileSink"),
	m_fileName("./test.sdriq"),
	m_startingTimeStamp(0),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    m_deviceAPI->setNbSinkStreams(1);
}

FileSinkOutput::~FileSinkOutput()
{
	stop();
}

void FileSinkOutput::destroy()
{
    delete this;
}

void FileSinkOutput::openFileStream()
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

	qDebug() << "FileSinkOutput::openFileStream: " << m_fileName.toStdString().c_str();
}

void FileSinkOutput::init()
{
    applySettings(m_settings, true);
}

bool FileSinkOutput::start()
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "FileSinkOutput::start";

	openFileStream();

	m_fileSinkThread = new FileSinkThread(&m_ofstream, &m_sampleSourceFifo);
	m_fileSinkThread->setSamplerate(m_settings.m_sampleRate);
	m_fileSinkThread->setLog2Interpolation(m_settings.m_log2Interp);
	m_fileSinkThread->connectTimer(m_masterTimer);
	m_fileSinkThread->startWork();

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("FileSinkOutput::start: started");

    if (getMessageQueueToGUI())
    {
        MsgReportFileSinkGeneration *report = MsgReportFileSinkGeneration::create(true); // acquisition on
        getMessageQueueToGUI()->push(report);
    }

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

    if (m_ofstream.is_open()) {
        m_ofstream.close();
    }

    if (getMessageQueueToGUI())
    {
        MsgReportFileSinkGeneration *report = MsgReportFileSinkGeneration::create(false); // acquisition off
        getMessageQueueToGUI()->push(report);
    }
}

QByteArray FileSinkOutput::serialize() const
{
    return m_settings.serialize();
}

bool FileSinkOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureFileSink* message = MsgConfigureFileSink::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFileSink* messageToGUI = MsgConfigureFileSink::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& FileSinkOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FileSinkOutput::getSampleRate() const
{
	return m_settings.m_sampleRate;
}

quint64 FileSinkOutput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void FileSinkOutput::setCenterFrequency(qint64 centerFrequency)
{
    FileSinkSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureFileSink* message = MsgConfigureFileSink::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFileSink* messageToGUI = MsgConfigureFileSink::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
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
	else if (MsgStartStop::match(message))
	{
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "FileSinkOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
	else if (MsgConfigureFileSink::match(message))
    {
	    qDebug() << "FileSinkOutput::handleMessage: MsgConfigureFileSink";
	    MsgConfigureFileSink& conf = (MsgConfigureFileSink&) message;
        applySettings(conf.getSettings(), conf.getForce());
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

		if (m_fileSinkThread != 0 && getMessageQueueToGUI())
		{
			report = MsgReportFileSinkStreamTiming::create(m_fileSinkThread->getSamplesCount());
			getMessageQueueToGUI()->push(report);
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
    bool forwardChange = false;

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency))
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
        forwardChange = true;
    }

    if (force || (m_settings.m_sampleRate != settings.m_sampleRate))
    {
        m_settings.m_sampleRate = settings.m_sampleRate;

        if (m_fileSinkThread != 0)
        {
            m_fileSinkThread->setSamplerate(m_settings.m_sampleRate);
        }

        forwardChange = true;
    }

    if (force || (m_settings.m_log2Interp != settings.m_log2Interp))
    {
        m_settings.m_log2Interp = settings.m_log2Interp;

        if (m_fileSinkThread != 0)
        {
            m_fileSinkThread->setLog2Interpolation(m_settings.m_log2Interp);
        }

        forwardChange = true;
    }

    if (forwardChange)
    {
        qDebug("FileSinkOutput::applySettings: forward: m_centerFrequency: %llu m_sampleRate: %llu m_log2Interp: %d",
                m_settings.m_centerFrequency,
                m_settings.m_sampleRate,
                m_settings.m_log2Interp);
        DSPSignalNotification *notif = new DSPSignalNotification(m_settings.m_sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

}

int FileSinkOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int FileSinkOutput::webapiRun(
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


