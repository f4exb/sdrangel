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

#include "device/devicesinkapi.h"

#include "sdrdaemonsinkgui.h"
#include "sdrdaemonsinkoutput.h"
#include "sdrdaemonsinkthread.h"

MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkName, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkWork, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgReportSDRdaemonSinkGeneration, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgReportSDRdaemonSinkStreamTiming, Message)

SDRdaemonSinkOutput::SDRdaemonSinkOutput(DeviceSinkAPI *deviceAPI, const QTimer& masterTimer) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_fileSinkThread(0),
	m_deviceDescription("FileSink"),
	m_fileName("./test.sdriq"),
	m_startingTimeStamp(0),
	m_masterTimer(masterTimer)
{
}

SDRdaemonSinkOutput::~SDRdaemonSinkOutput()
{
	stop();
}

void SDRdaemonSinkOutput::openFileStream()
{
	if (m_ofstream.is_open()) {
		m_ofstream.close();
	}

	m_ofstream.open(m_fileName.toStdString().c_str(), std::ios::binary);

	int actualSampleRate = m_settings.m_sampleRate * (1<<m_settings.m_log2Interp);
	m_ofstream.write((const char *) &actualSampleRate, sizeof(int));
    //m_ofstream.write((const char *) &m_settings.m_sampleRate, sizeof(int));
	m_ofstream.write((const char *) &m_settings.m_centerFrequency, sizeof(quint64));
    m_startingTimeStamp = time(0);
    m_ofstream.write((const char *) &m_startingTimeStamp, sizeof(std::time_t));

	qDebug() << "FileSinkOutput::openFileStream: " << m_fileName.toStdString().c_str();
}

bool SDRdaemonSinkOutput::start()
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "FileSinkOutput::start";

	openFileStream();

	if((m_fileSinkThread = new FileSinkThread(&m_ofstream, &m_sampleSourceFifo)) == 0)
	{
		qFatal("out of memory");
		stop();
		return false;
	}

	m_fileSinkThread->setSamplerate(m_settings.m_sampleRate);
	m_fileSinkThread->setLog2Interpolation(m_settings.m_log2Interp);
	m_fileSinkThread->connectTimer(m_masterTimer);
	m_fileSinkThread->startWork();

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("FileSinkOutput::start: started");

	MsgReportSDRdaemonSinkGeneration *report = MsgReportSDRdaemonSinkGeneration::create(true); // acquisition on
	getOutputMessageQueueToGUI()->push(report);

	return true;
}

void SDRdaemonSinkOutput::stop()
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

    MsgReportSDRdaemonSinkGeneration *report = MsgReportSDRdaemonSinkGeneration::create(false); // acquisition off
	getOutputMessageQueueToGUI()->push(report);
}

const QString& SDRdaemonSinkOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int SDRdaemonSinkOutput::getSampleRate() const
{
	return m_settings.m_sampleRate;
}

quint64 SDRdaemonSinkOutput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

std::time_t SDRdaemonSinkOutput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool SDRdaemonSinkOutput::handleMessage(const Message& message)
{
	if (MsgConfigureSDRdaemonSinkName::match(message))
	{
		MsgConfigureSDRdaemonSinkName& conf = (MsgConfigureSDRdaemonSinkName&) message;
		m_fileName = conf.getFileName();
		openFileStream();
		return true;
	}
	else if (MsgConfigureSDRdaemonSink::match(message))
    {
	    qDebug() << "FileSinkOutput::handleMessage: MsgConfigureFileSink";
	    MsgConfigureSDRdaemonSink& conf = (MsgConfigureSDRdaemonSink&) message;
        applySettings(conf.getSettings(), false);
        return true;
    }
	else if (MsgConfigureSDRdaemonSinkWork::match(message))
	{
		MsgConfigureSDRdaemonSinkWork& conf = (MsgConfigureSDRdaemonSinkWork&) message;
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
	else if (MsgConfigureSDRdaemonSinkStreamTiming::match(message))
	{
        MsgReportSDRdaemonSinkStreamTiming *report;

		if (m_fileSinkThread != 0)
		{
			report = MsgReportSDRdaemonSinkStreamTiming::create(m_fileSinkThread->getSamplesCount());
			getOutputMessageQueueToGUI()->push(report);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void SDRdaemonSinkOutput::applySettings(const FileSinkSettings& settings, bool force)
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
            m_fileSinkThread->setSamplerate(m_settings.m_sampleRate);
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
        m_deviceAPI->getDeviceInputMessageQueue()->push(notif);
    }

}
