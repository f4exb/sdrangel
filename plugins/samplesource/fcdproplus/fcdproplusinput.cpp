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
#include "fcdproplusinput.h"

#include "fcdproplusgui.h"
#include "fcdproplusserializer.h"
#include "fcdproplusthread.h"
#include "fcdtraits.h"
#include "fcdproplusconst.h"

MESSAGE_CLASS_DEFINITION(FCDProPlusInput::MsgConfigureFCD, Message)
//MESSAGE_CLASS_DEFINITION(FCDInput::MsgReportFCD, Message)

/*
const uint16_t FCDInput::m_vendorId  = 0x04D8;
const uint16_t FCDInput::m_productId = 0xFB31;
const int FCDInput::m_sampleRate = 192000;
const std::string FCDInput::m_deviceName("hw:CARD=V20");

const uint16_t FCDInput::m_productId = 0xFB56;
const int FCDInput::m_sampleRate = 96000;
const std::string FCDInput::m_deviceName("hw:CARD=V10");
*/

FCDProPlusInput::Settings::Settings() :
	centerFrequency(435000000),
	rangeLow(true),
	lnaGain(true),
	mixGain(true),
	biasT(false),
	ifGain(0),
	ifFilterIndex(0),
	rfFilterIndex(0)
{
}

void FCDProPlusInput::Settings::resetToDefaults()
{
	centerFrequency = 435000000;
	rangeLow = true;
	lnaGain = true;
	biasT = false;
	ifGain = 0;
	rfFilterIndex = 0;
	ifFilterIndex = 0;
}

QByteArray FCDProPlusInput::Settings::serialize() const
{
	FCDProPlusSerializer::FCDData data;

	data.m_data.m_lnaGain = lnaGain;
	data.m_data.m_RxGain1 = ifGain;
	data.m_data.m_frequency = centerFrequency;
	data.m_rangeLow = rangeLow;
	data.m_biasT = biasT;

	QByteArray byteArray;

	FCDProPlusSerializer::writeSerializedData(data, byteArray);

	return byteArray;

	/*
	SimpleSerializer s(1);
	s.writeU64(1, centerFrequency);
	s.writeS32(2, range);
	s.writeS32(3, gain);
	s.writeS32(4, bias);
	return s.final();*/
}

bool FCDProPlusInput::Settings::deserialize(const QByteArray& serializedData)
{
	FCDProPlusSerializer::FCDData data;

	bool valid = FCDProPlusSerializer::readSerializedData(serializedData, data);

	lnaGain = data.m_data.m_lnaGain;
	ifGain = data.m_data.m_RxGain1;
	centerFrequency = data.m_data.m_frequency;
	rangeLow = data.m_rangeLow;
	biasT = data.m_biasT;

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

FCDProPlusInput::FCDProPlusInput() :
	m_dev(0),
	m_settings(),
	m_FCDThread(0),
	m_deviceDescription(fcd_traits<ProPlus>::displayedName)
{
}

FCDProPlusInput::~FCDProPlusInput()
{
	stop();
}

bool FCDProPlusInput::init(const Message& cmd)
{
	return false;
}

bool FCDProPlusInput::start(int device)
{
	qDebug() << "FCDProPlusInput::start with device #" << device;

	QMutexLocker mutexLocker(&m_mutex);

	if (m_FCDThread)
	{
		return false;
	}

	m_dev = fcdOpen(fcd_traits<ProPlus>::vendorId, fcd_traits<ProPlus>::productId, device);

	if (m_dev == 0)
	{
		qCritical("FCDProPlusInput::start: could not open FCD");
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

	if ((m_FCDThread = new FCDProPlusThread(&m_sampleFifo)) == NULL)
	{
		qFatal("out of memory");
		return false;
	}

	m_FCDThread->startWork();

	mutexLocker.unlock();
	applySettings(m_settings, true);

	qDebug("FCDProPlusInput::started");
	return true;
}

void FCDProPlusInput::stop()
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

const QString& FCDProPlusInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FCDProPlusInput::getSampleRate() const
{
	return fcd_traits<ProPlus>::sampleRate;
}

quint64 FCDProPlusInput::getCenterFrequency() const
{
	return m_settings.centerFrequency;
}

bool FCDProPlusInput::handleMessage(const Message& message)
{
	if(MsgConfigureFCD::match(message))
	{
		qDebug() << "FCDProPlusInput::handleMessage: MsgConfigureFCD";
		MsgConfigureFCD& conf = (MsgConfigureFCD&) message;
		applySettings(conf.getSettings(), false);
		return true;
	}
	else
	{
		return false;
	}
}

void FCDProPlusInput::applySettings(const Settings& settings, bool force)
{
	bool signalChange = false;

	if ((m_settings.centerFrequency != settings.centerFrequency) || force)
	{
		qDebug() << "FCDProPlusInput::applySettings: fc: " << settings.centerFrequency;
		m_settings.centerFrequency = settings.centerFrequency;

		if (m_dev != 0)
		{
			set_center_freq((double) m_settings.centerFrequency);
		}

		signalChange = true;
	}

	if ((m_settings.lnaGain != settings.lnaGain) || force)
	{
		m_settings.lnaGain = settings.lnaGain;

		if (m_dev != 0)
		{
			set_lna_gain(settings.lnaGain > 0);
		}
	}

	if ((m_settings.biasT != settings.biasT) || force)
	{
		m_settings.biasT = settings.biasT;

		if (m_dev != 0)
		{
			set_bias_t(settings.biasT > 0);
		}
	}
    
    if (signalChange)
    {
		DSPSignalNotification *notif = new DSPSignalNotification(fcd_traits<ProPlus>::sampleRate, m_settings.centerFrequency);
		getOutputMessageQueue()->push(notif);        
    }
}

void FCDProPlusInput::set_center_freq(double freq)
{
	if (fcdAppSetFreq(m_dev, freq) == FCD_MODE_NONE)
	{
		qDebug("No FCD HID found for frquency change");
	}
}

void FCDProPlusInput::set_bias_t(bool on)
{
	quint8 cmd = on ? 1 : 0;

	fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_BIAS_TEE, &cmd, 1);
}

void FCDProPlusInput::set_lna_gain(bool on)
{
	quint8 cmd = on ? 1 : 0;

	fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_LNA_GAIN, &cmd, 1);
}

void FCDProPlusInput::set_mixer_gain(bool on)
{
	quint8 cmd = on ? 1 : 0;

	fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_MIXER_GAIN, &cmd, 1);
}

void FCDProPlusInput::set_if_gain(int gain)
{
	if (gain < 0)
	{
		return;
	}

	quint8 cmd_value = gain;

	fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_IF_GAIN, &cmd_value, 1);
}

void FCDProPlusInput::set_if_filter(int filterIndex)
{
	if ((filterIndex < 0) || (filterIndex >= FCDProPlusConstants::fcdproplus_if_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProPlusConstants::if_filters[filterIndex].value;

	fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_IF_FILTER, &cmd_value, 1);
}


void FCDProPlusInput::set_rf_filter(int filterIndex)
{
	if ((filterIndex < 0) || (filterIndex >= FCDProPlusConstants::fcdproplus_rf_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProPlusConstants::rf_filters[filterIndex].value;

	fcdAppSetParam(m_dev, FCDPROPLUS_HID_CMD_SET_RF_FILTER, &cmd_value, 1);
}



