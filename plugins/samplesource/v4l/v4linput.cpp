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
#include "v4linput.h"
#include "v4lthread.h"
#include "v4lgui.h"
#include "util/simpleserializer.h"

MESSAGE_CLASS_DEFINITION(V4LInput::MsgConfigureV4L, Message)
MESSAGE_CLASS_DEFINITION(V4LInput::MsgReportV4L, Message)

V4LInput::Settings::Settings() :
	m_gain(0),
	m_samplerate(2500000)
{
}

void V4LInput::Settings::resetToDefaults()
{
	m_gain = 0;
	m_samplerate = 2500000;
}

QByteArray V4LInput::Settings::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_gain);
	s.writeS32(2, m_samplerate);
	return s.final();
}

bool V4LInput::Settings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		d.readS32(1, &m_gain, 0);
		d.readS32(2, &m_samplerate, 0);
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

V4LInput::V4LInput(MessageQueue* msgQueueToGUI) :
	SampleSource(msgQueueToGUI),
	m_settings(),
	m_dev(0),
	m_V4LThread(NULL),
	m_deviceDescription()
{
}

V4LInput::~V4LInput()
{
	stopInput();
}

bool V4LInput::startInput(int device)
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_dev > 0)
		stopInput();

	char vendor[256];
	char product[256];
	char serial[256];
	int res;
	int numberOfGains;

	vendor[0] = '\0';
	product[0] = '\0';
	serial[0] = '\0';


	if(!m_sampleFifo.setSize(524288)) { // fraction of samplerate
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	OpenSource("/dev/swradio0");
	if (fd < 0) {
		qCritical("could not open SDR");
		return false;
	}
	m_dev = device;
/*
	if((res = rtlsdr_get_usb_strings(m_dev, vendor, product, serial)) < 0) {
		qCritical("error accessing USB device");
		goto failed;
	}
	qWarning("RTLSDRInput open: %s %s, SN: %s", vendor, product, serial);
	m_deviceDescription = QString("%1 (SN %2)").arg(product).arg(serial);

	if((res = rtlsdr_set_sample_rate(m_dev, 1536000)) < 0) {
		qCritical("could not set sample rate: %s", strerror(errno));
		goto failed;
	}

	if((res = rtlsdr_set_tuner_gain_mode(m_dev, 1)) < 0) {
		qCritical("error setting tuner gain mode");
		goto failed;
	}
	if((res = rtlsdr_set_agc_mode(m_dev, 0)) < 0) {
		qCritical("error setting agc mode");
		goto failed;
	}

	numberOfGains = rtlsdr_get_tuner_gains(m_dev, NULL);
	if(numberOfGains < 0) {
		qCritical("error getting number of gain values supported");
		goto failed;
	}
	m_gains.resize(numberOfGains);
	if(rtlsdr_get_tuner_gains(m_dev, &m_gains[0]) < 0) {
		qCritical("error getting gain values");
		goto failed;
	}
	if((res = rtlsdr_reset_buffer(m_dev)) < 0) {
		qCritical("could not reset USB EP buffers: %s", strerror(errno));
		goto failed;
	}
*/
	if((m_V4LThread = new V4LThread(&m_sampleFifo)) == NULL) {
		qFatal("out of memory");
		goto failed;
	}
	m_V4LThread->startWork();

	mutexLocker.unlock();
	applySettings(m_generalSettings, m_settings, true);

	qDebug("V4LInput: start");
	MsgReportV4L::create(m_gains)->submit(m_guiMessageQueue);

	return true;

failed:
	stopInput();
	return false;
}

void V4LInput::stopInput()
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_V4LThread != NULL) {
		m_V4LThread->stopWork();
		delete m_V4LThread;
		m_V4LThread = NULL;
	}
	if(m_dev > 0) {
		CloseSource();
		m_dev = 0;
	}
	m_deviceDescription.clear();
}

const QString& V4LInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int V4LInput::getSampleRate() const
{
	int result = m_settings.m_samplerate / 4;
	if (result > 200000) result /= 4;
	return result;
}

quint64 V4LInput::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

bool V4LInput::handleMessage(Message* message)
{
	if(MsgConfigureV4L::match(message)) {
		MsgConfigureV4L* conf = (MsgConfigureV4L*)message;
		if(!applySettings(conf->getGeneralSettings(), conf->getSettings(), false))
			qDebug("V4L config error");
		message->completed();
		return true;
	} else {
		return false;
	}
}

bool V4LInput::applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);

	if((m_generalSettings.m_centerFrequency != generalSettings.m_centerFrequency) || force) {
		m_generalSettings.m_centerFrequency = generalSettings.m_centerFrequency;
		if(m_dev > 0)
			V4LInput::set_center_freq( (double)(generalSettings.m_centerFrequency
								+ (settings.m_samplerate / 4) ));
	}
	if((m_settings.m_gain != settings.m_gain) || force) {
		m_settings.m_gain = settings.m_gain;
		if(m_dev > 0)
			V4LInput::set_tuner_gain((double)m_settings.m_gain);
	}
	if((m_settings.m_samplerate != settings.m_samplerate) || force) {
		m_settings.m_samplerate = settings.m_samplerate;
		if(m_dev > 0)
			m_V4LThread->setSamplerate((double)settings.m_samplerate);
	}
	return true;
}
