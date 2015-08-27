///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

// FIXME: FCD is handled very badly!

#include <QDebug>
#include <string.h>
#include <errno.h>
#include "fcdinput.h"
#include "fcdthread.h"
#include "fcdgui.h"
#include "qthid.h"
#include "dsp/dspcommands.h"
#include "util/simpleserializer.h"

MESSAGE_CLASS_DEFINITION(FCDInput::MsgConfigureFCD, Message)
//MESSAGE_CLASS_DEFINITION(FCDInput::MsgReportFCD, Message)

FCDInput::Settings::Settings() :
	centerFrequency(435000000),
	range(0),
	gain(0),
	bias(0)
{
}

void FCDInput::Settings::resetToDefaults()
{
	centerFrequency = 435000000;
	range = 0;
	gain = 0;
	bias = 0;
}

QByteArray FCDInput::Settings::serialize() const
{
	SimpleSerializer s(1);
	s.writeU64(1, centerFrequency);
	s.writeS32(2, range);
	s.writeS32(3, gain);
	s.writeS32(4, bias);
	return s.final();
}

bool FCDInput::Settings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (d.isValid() && d.getVersion() == 1)
	{
		d.readU64(1, &centerFrequency, 435000000);
        d.readS32(2, &range, 0);
		d.readS32(3, &gain, 0);
		d.readS32(4, &bias, 0);
		return true;
	}

	resetToDefaults();
	return true;
}

FCDInput::FCDInput() :
	m_settings(),
	m_FCDThread(0),
	m_deviceDescription()
{
}

FCDInput::~FCDInput()
{
	stop();
}

bool FCDInput::init(const Message& cmd)
{
	return false;
}

bool FCDInput::start(int device)
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_FCDThread)
	{
		return false;
	}

	/* Apply settings before streaming to avoid bus contention;
	 * there is very little spare bandwidth on a full speed USB device.
	 * Failure is harmless if no device is found */

	applySettings(m_settings, true);

	if(!m_sampleFifo.setSize(4096*16))
	{
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	if ((m_FCDThread = new FCDThread(&m_sampleFifo)) == NULL)
	{
		qFatal("out of memory");
		return false;
	}

	m_deviceDescription = QString("Funcube Dongle");

	qDebug("FCDInput::start");
	return true;
}

void FCDInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_FCDThread)
	{
		m_FCDThread->stopWork();
		// wait for thread to quit ?
		delete m_FCDThread;
		m_FCDThread = 0;
	}

	m_deviceDescription.clear();
}

const QString& FCDInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FCDInput::getSampleRate() const
{
	return 96000;
}

quint64 FCDInput::getCenterFrequency() const
{
	return m_settings.centerFrequency;
}

bool FCDInput::handleMessage(const Message& message)
{
	if(MsgConfigureFCD::match(message))
	{
		qDebug() << "FCDInput::handleMessage: MsgConfigureFCD";
		MsgConfigureFCD& conf = (MsgConfigureFCD&) message;
		applySettings(conf.getSettings(), false);
		return true;
	}
	else
	{
		return false;
	}
}

void FCDInput::applySettings(const Settings& settings, bool force)
{
	bool signalChange = false;

	if ((m_settings.centerFrequency != settings.centerFrequency))
	{
		qDebug() << "FCDInput::applySettings: fc: " << settings.centerFrequency;
		m_settings.centerFrequency = settings.centerFrequency;
		set_center_freq((double) m_settings.centerFrequency);
		signalChange = true;
	}

	if ((m_settings.gain != settings.gain) || force)
	{
		set_lna_gain(settings.gain > 0);
		m_settings.gain = settings.gain;
	}

	if ((m_settings.bias != settings.bias) || force)
	{
		set_bias_t(settings.bias > 0);
		m_settings.bias = settings.bias;
	}
    
    if (signalChange)
    {
		DSPSignalNotification *notif = new DSPSignalNotification(96000, m_settings.centerFrequency);
		getOutputMessageQueue()->push(notif);        
    }
}

void FCDInput::set_center_freq(double freq)
{
	if (fcdAppSetFreq(freq) == FCD_MODE_NONE)
	{
		qDebug("No FCD HID found for frquency change");
	}
}

void FCDInput::set_bias_t(bool on)
{
	quint8 cmd = on ? 1 : 0;

	fcdAppSetParam(FCD_CMD_APP_SET_BIAS_TEE, &cmd, 1);
}

void FCDInput::set_lna_gain(bool on)
{
	quint8 cmd = on ? 1 : 0;

	fcdAppSetParam(FCD_CMD_APP_SET_LNA_GAIN, &cmd, 1);
}


