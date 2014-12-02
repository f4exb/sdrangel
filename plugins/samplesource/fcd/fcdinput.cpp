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

#include <string.h>
#include <errno.h>
#include "fcdinput.h"
#include "fcdthread.h"
#include "fcdgui.h"
#include "util/simpleserializer.h"

MESSAGE_CLASS_DEFINITION(FCDInput::MsgConfigureFCD, Message)
//MESSAGE_CLASS_DEFINITION(FCDInput::MsgReportFCD, Message)

FCDInput::Settings::Settings() :
	range(0),
	gain(0),
	bias(0)
{
}

void FCDInput::Settings::resetToDefaults()
{
	range = 0;
	gain = 0;
	bias = 0;
}

QByteArray FCDInput::Settings::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, range);
	s.writeS32(2, gain);
	s.writeS32(3, bias);
	return s.final();
}

bool FCDInput::Settings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);
	if(d.isValid() && d.getVersion() == 1) {
                d.readS32(1, &range, 0);
		d.readS32(2, &gain, 0);
		d.readS32(3, &bias, 0);
		return true;
	}
	resetToDefaults();
	return true;
}

FCDInput::FCDInput(MessageQueue* msgQueueToGUI) :
	SampleSource(msgQueueToGUI),
	m_settings(),
	m_FCDThread(NULL),
	m_deviceDescription()
{
}

FCDInput::~FCDInput()
{
	stopInput();
}

bool FCDInput::startInput(int device)
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_FCDThread)
		return false;

	if(!m_sampleFifo.setSize(4096*16)) {
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	if((m_FCDThread = new FCDThread(&m_sampleFifo)) == NULL) {
		qFatal("out of memory");
		return false;
	}

	m_deviceDescription = QString("Funcube Dongle");

	applySettings(m_generalSettings, m_settings, true);

	qDebug("FCDInput: start");
	return true;
}

void FCDInput::stopInput()
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_FCDThread) {
		m_FCDThread->stopWork();
		// wait for thread to quit ?
		delete m_FCDThread;
		m_FCDThread = NULL;
	}
	m_deviceDescription.clear();
}

const QString& FCDInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FCDInput::getSampleRate() const
{
	return 192000;
}

quint64 FCDInput::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

bool FCDInput::handleMessage(Message* message)
{
	if(MsgConfigureFCD::match(message)) {
		MsgConfigureFCD* conf = (MsgConfigureFCD*)message;
		applySettings(conf->getGeneralSettings(), conf->getSettings(), false);
		message->completed();
		return true;
	} else {
		return false;
	}
}

void FCDInput::applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force)
{
	bool freqChange;

	if((m_generalSettings.m_centerFrequency != generalSettings.m_centerFrequency))
		freqChange = true;
	else
		freqChange = false;

	if(freqChange || force) {
		m_generalSettings.m_centerFrequency = generalSettings.m_centerFrequency;
		set_center_freq( (double)(generalSettings.m_centerFrequency) );
	}

	if(!freqChange || force) {
		set_lna_gain(settings.gain);
		set_bias_t(settings.bias);
	}	

}


