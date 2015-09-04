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
#include "dsp/dspcommands.h"
#include "fcdproinput.h"

#include "fcdprogui.h"
#include "fcdproserializer.h"
#include "fcdprothread.h"

MESSAGE_CLASS_DEFINITION(FCDProInput::MsgConfigureFCD, Message)

/*
const uint16_t FCDInput::m_vendorId  = 0x04D8;
const uint16_t FCDInput::m_productId = 0xFB31;
const int FCDInput::m_sampleRate = 192000;
const std::string FCDInput::m_deviceName("hw:CARD=V20");

const uint16_t FCDInput::m_productId = 0xFB56;
const int FCDInput::m_sampleRate = 96000;
const std::string FCDInput::m_deviceName("hw:CARD=V10");
*/

FCDProInput::Settings::Settings() :
	centerFrequency(435000000),
	range(0),
	gain(0),
	bias(0)
{
}

void FCDProInput::Settings::resetToDefaults()
{
	centerFrequency = 435000000;
	range = 0;
	gain = 0;
	bias = 0;
}

QByteArray FCDProInput::Settings::serialize() const
{
	FCDProSerializer::FCDData data;

	data.m_data.m_lnaGain = gain;
	data.m_data.m_frequency = centerFrequency;
	data.m_range = range;
	data.m_bias = bias;

	QByteArray byteArray;

	FCDProSerializer::writeSerializedData(data, byteArray);

	return byteArray;

	/*
	SimpleSerializer s(1);
	s.writeU64(1, centerFrequency);
	s.writeS32(2, range);
	s.writeS32(3, gain);
	s.writeS32(4, bias);
	return s.final();*/
}

bool FCDProInput::Settings::deserialize(const QByteArray& serializedData)
{
	FCDProSerializer::FCDData data;

	bool valid = FCDProSerializer::readSerializedData(serializedData, data);

	gain = data.m_data.m_lnaGain;
	centerFrequency = data.m_data.m_frequency;
	range = data.m_range;
	bias = data.m_bias;

	return valid;

	/*
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
	return true;*/
}

FCDProInput::FCDProInput() :
	m_dev(0),
	m_settings(),
	m_FCDThread(0),
	m_deviceDescription("Funcube Dongle Pro")
{
}

FCDProInput::~FCDProInput()
{
	stop();
}

bool FCDProInput::init(const Message& cmd)
{
	return false;
}

bool FCDProInput::start(int device)
{
	qDebug() << "FCDProInput::start with device #" << device;

	QMutexLocker mutexLocker(&m_mutex);

	if (m_FCDThread)
	{
		return false;
	}

	m_dev = fcdOpen(0x04D8, 0xFB56, device);

	if (m_dev == 0)
	{
		qCritical("FCDProInput::start: could not open FCD");
		return false;
	}

	/* Apply settings before streaming to avoid bus contention;
	 * there is very little spare bandwidth on a full speed USB device.
	 * Failure is harmless if no device is found
	 * ... This is rubbish...*/

	//applySettings(m_settings, true);

	if(!m_sampleFifo.setSize(96000*4))
	{
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	if ((m_FCDThread = new FCDProThread(&m_sampleFifo)) == NULL)
	{
		qFatal("out of memory");
		return false;
	}

	m_FCDThread->startWork();

	mutexLocker.unlock();
	applySettings(m_settings, true);

	qDebug("FCDProInput::started");
	return true;
}

void FCDProInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_FCDThread)
	{
		m_FCDThread->stopWork();
		// wait for thread to quit ?
		delete m_FCDThread;
		m_FCDThread = 0;
	}

	fcdClose(m_dev);
	m_dev = 0;
}

const QString& FCDProInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FCDProInput::getSampleRate() const
{
	return 96000;
}

quint64 FCDProInput::getCenterFrequency() const
{
	return m_settings.centerFrequency;
}

bool FCDProInput::handleMessage(const Message& message)
{
	if(MsgConfigureFCD::match(message))
	{
		qDebug() << "FCDProInput::handleMessage: MsgConfigureFCD";
		MsgConfigureFCD& conf = (MsgConfigureFCD&) message;
		applySettings(conf.getSettings(), false);
		return true;
	}
	else
	{
		return false;
	}
}

void FCDProInput::applySettings(const Settings& settings, bool force)
{
	bool signalChange = false;

	if ((m_settings.centerFrequency != settings.centerFrequency) || force)
	{
		qDebug() << "FCDProInput::applySettings: fc: " << settings.centerFrequency;
		m_settings.centerFrequency = settings.centerFrequency;

		if (m_dev != 0)
		{
			set_center_freq((double) m_settings.centerFrequency);
		}

		signalChange = true;
	}

	if ((m_settings.gain != settings.gain) || force)
	{
		m_settings.gain = settings.gain;

		if (m_dev != 0)
		{
			set_lna_gain(settings.gain > 0);
		}
	}

	if ((m_settings.bias != settings.bias) || force)
	{
		m_settings.bias = settings.bias;

		if (m_dev != 0)
		{
			set_bias_t(settings.bias > 0);
		}
	}
    
    if (signalChange)
    {
		DSPSignalNotification *notif = new DSPSignalNotification(96000, m_settings.centerFrequency);
		getOutputMessageQueue()->push(notif);        
    }
}

void FCDProInput::set_center_freq(double freq)
{
	if (fcdAppSetFreq(m_dev, freq) == FCD_MODE_NONE)
	{
		qDebug("No FCD HID found for frquency change");
	}
}

void FCDProInput::set_bias_t(bool on)
{
	quint8 cmd = on ? 1 : 0;

	fcdAppSetParam(m_dev, FCD_CMD_APP_SET_BIAS_TEE, &cmd, 1);
}

void FCDProInput::set_lna_gain(bool on)
{
	quint8 cmd = on ? 1 : 0;

	fcdAppSetParam(m_dev, FCD_CMD_APP_SET_LNA_GAIN, &cmd, 1);
}


