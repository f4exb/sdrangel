///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkWork, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkChunkCorrection, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgReportSDRdaemonSinkStreamTiming, Message)

SDRdaemonSinkOutput::SDRdaemonSinkOutput(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_sdrDaemonSinkThread(0),
	m_deviceDescription("SDRdaemonSink"),
    m_startingTimeStamp(0),
	m_masterTimer(deviceAPI->getMasterTimer())
{
}

SDRdaemonSinkOutput::~SDRdaemonSinkOutput()
{
	stop();
}

void SDRdaemonSinkOutput::destroy()
{
    delete this;
}

bool SDRdaemonSinkOutput::start()
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "SDRdaemonSinkOutput::start";

	if((m_sdrDaemonSinkThread = new SDRdaemonSinkThread(&m_sampleSourceFifo)) == 0)
	{
		qFatal("out of memory");
		stop();
		return false;
	}
	m_sdrDaemonSinkThread->setRemoteAddress(m_settings.m_address, m_settings.m_dataPort);
	m_sdrDaemonSinkThread->setCenterFrequency(m_settings.m_centerFrequency);
	m_sdrDaemonSinkThread->setSamplerate(m_settings.m_sampleRate);
	m_sdrDaemonSinkThread->setNbBlocksFEC(m_settings.m_nbFECBlocks);
	m_sdrDaemonSinkThread->connectTimer(m_masterTimer);
	m_sdrDaemonSinkThread->startWork();

    double delay = ((127*127*m_settings.m_txDelay) / m_settings.m_sampleRate)/(128 + m_settings.m_nbFECBlocks);
    m_sdrDaemonSinkThread->setTxDelay((int) (delay*1e6));

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("SDRdaemonSinkOutput::start: started");

	return true;
}

void SDRdaemonSinkOutput::stop()
{
	qDebug() << "SDRdaemonSinkOutput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if(m_sdrDaemonSinkThread != 0)
	{
	    m_sdrDaemonSinkThread->stopWork();
		delete m_sdrDaemonSinkThread;
		m_sdrDaemonSinkThread = 0;
	}
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

    if (MsgConfigureSDRdaemonSink::match(message))
    {
        qDebug() << "SDRdaemonSinkOutput::handleMessage:" << message.getIdentifier();
	    MsgConfigureSDRdaemonSink& conf = (MsgConfigureSDRdaemonSink&) message;
        applySettings(conf.getSettings(), conf.getForce());
        return true;
    }
	else if (MsgConfigureSDRdaemonSinkWork::match(message))
	{
		MsgConfigureSDRdaemonSinkWork& conf = (MsgConfigureSDRdaemonSinkWork&) message;
		bool working = conf.isWorking();

		if (m_sdrDaemonSinkThread != 0)
		{
			if (working)
			{
			    m_sdrDaemonSinkThread->startWork();
			}
			else
			{
			    m_sdrDaemonSinkThread->stopWork();
			}
		}

		return true;
	}
	else if (MsgConfigureSDRdaemonSinkStreamTiming::match(message))
	{
        MsgReportSDRdaemonSinkStreamTiming *report;

		if (m_sdrDaemonSinkThread != 0 && getMessageQueueToGUI())
		{
			report = MsgReportSDRdaemonSinkStreamTiming::create(m_sdrDaemonSinkThread->getSamplesCount());
			getMessageQueueToGUI()->push(report);
		}

		return true;
	}
	else if (MsgConfigureSDRdaemonSinkChunkCorrection::match(message))
	{
	    MsgConfigureSDRdaemonSinkChunkCorrection& conf = (MsgConfigureSDRdaemonSinkChunkCorrection&) message;

	    if (m_sdrDaemonSinkThread != 0)
        {
	        m_sdrDaemonSinkThread->setChunkCorrection(conf.getChunkCorrection());
        }

	    return true;
	}
	else
	{
		return false;
	}
}

void SDRdaemonSinkOutput::applySettings(const SDRdaemonSinkSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);
    bool forwardChange = false;
    bool changeTxDelay = false;

    if (force || (m_settings.m_address != settings.m_address) || (m_settings.m_dataPort != settings.m_dataPort))
    {
        m_settings.m_address = settings.m_address;
        m_settings.m_dataPort = settings.m_dataPort;

        if (m_sdrDaemonSinkThread != 0)
        {
            m_sdrDaemonSinkThread->setRemoteAddress(m_settings.m_address, m_settings.m_dataPort);
        }
    }

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency))
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;

        if (m_sdrDaemonSinkThread != 0)
        {
            m_sdrDaemonSinkThread->setCenterFrequency(m_settings.m_centerFrequency);
        }

        forwardChange = true;
    }

    if (force || (m_settings.m_sampleRate != settings.m_sampleRate))
    {
        m_settings.m_sampleRate = settings.m_sampleRate;

        if (m_sdrDaemonSinkThread != 0)
        {
            m_sdrDaemonSinkThread->setSamplerate(m_settings.m_sampleRate);
        }

        forwardChange = true;
        changeTxDelay = true;
    }

    if (force || (m_settings.m_log2Interp != settings.m_log2Interp))
    {
        m_settings.m_log2Interp = settings.m_log2Interp;
        forwardChange = true;
    }

    if (force || (m_settings.m_nbFECBlocks != settings.m_nbFECBlocks))
    {
        m_settings.m_nbFECBlocks = settings.m_nbFECBlocks;

        if (m_sdrDaemonSinkThread != 0)
        {
            m_sdrDaemonSinkThread->setNbBlocksFEC(m_settings.m_nbFECBlocks);
        }

        changeTxDelay = true;
    }

    if (force || (m_settings.m_txDelay != settings.m_txDelay))
    {
        m_settings.m_txDelay = settings.m_txDelay;
        changeTxDelay = true;
    }

    if (changeTxDelay)
    {
        double delay = ((127*127*m_settings.m_txDelay) / m_settings.m_sampleRate)/(128 + m_settings.m_nbFECBlocks);
        qDebug("SDRdaemonSinkOutput::applySettings: Tx delay: %f us", delay*1e6);

        if (m_sdrDaemonSinkThread != 0)
        {
            // delay is calculated as a fraction of the nominal UDP block process time
            // frame size: 127 * 127 samples
            // divided by sample rate gives the frame process time
            // divided by the number of actual blocks including FEC blocks gives the block (i.e. UDP block) process time
            m_sdrDaemonSinkThread->setTxDelay((int) (delay*1e6));
        }
    }

    mutexLocker.unlock();

    qDebug("SDRdaemonSinkOutput::applySettings: %s m_centerFrequency: %llu m_sampleRate: %llu m_log2Interp: %d m_txDelay: %f m_nbFECBlocks: %d",
            forwardChange ? "forward change" : "",
            m_settings.m_centerFrequency,
            m_settings.m_sampleRate,
            m_settings.m_log2Interp,
            m_settings.m_txDelay,
            m_settings.m_nbFECBlocks);

    if (forwardChange)
    {
        DSPSignalNotification *notif = new DSPSignalNotification(m_settings.m_sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

}
