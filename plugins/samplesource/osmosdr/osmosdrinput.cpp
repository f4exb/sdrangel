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
#include "osmosdrinput.h"
#include "osmosdrthread.h"
#include "osmosdrgui.h"
#include "util/simpleserializer.h"

MESSAGE_CLASS_DEFINITION(OsmoSDRInput::MsgConfigureOsmoSDR, Message)

OsmoSDRInput::Settings::Settings() :
	m_swapIQ(false),
	m_decimation(3),
	m_lnaGain(-50),
	m_mixerGain(40),
	m_mixerEnhancement(0),
	m_if1gain(-30),
	m_if2gain(0),
	m_if3gain(0),
	m_if4gain(0),
	m_if5gain(30),
	m_if6gain(30),
	m_opAmpI1(0),
	m_opAmpI2(0),
	m_opAmpQ1(0),
	m_opAmpQ2(0),
	m_dcOfsI(0),
	m_dcOfsQ(0)
{
}

void OsmoSDRInput::Settings::resetToDefaults()
{
	m_swapIQ = false;
	m_decimation = 3;
	m_lnaGain = -50;
	m_mixerGain = 40;
	m_mixerEnhancement = 0;
	m_if1gain = -30;
	m_if2gain = 0;
	m_if3gain = 0;
	m_if4gain = 0;
	m_if5gain = 30;
	m_if6gain = 30;
	m_opAmpI1 = 0;
	m_opAmpI2 = 0;
	m_opAmpQ1 = 0;
	m_opAmpQ2 = 0;
	m_dcOfsI = 0;
	m_dcOfsQ = 0;
}

QByteArray OsmoSDRInput::Settings::serialize() const
{
	SimpleSerializer s(1);
	s.writeBool(1, m_swapIQ);
	s.writeS32(2, m_decimation);
	s.writeS32(3, m_lnaGain);
	s.writeS32(4, m_mixerGain);
	s.writeS32(5, m_mixerEnhancement);
	s.writeS32(6, m_if1gain);
	s.writeS32(7, m_if2gain);
	s.writeS32(8, m_if3gain);
	s.writeS32(9, m_if4gain);
	s.writeS32(10, m_if5gain);
	s.writeS32(11, m_if6gain);
	s.writeS32(12, m_opAmpI1);
	s.writeS32(13, m_opAmpI2);
	s.writeS32(14, m_opAmpQ1);
	s.writeS32(15, m_opAmpQ2);
	s.writeS32(16, m_dcOfsI);
	s.writeS32(17, m_dcOfsQ);
	return s.final();
}

bool OsmoSDRInput::Settings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		d.readBool(1, &m_swapIQ, false);
		d.readS32(2, &m_decimation, 3);
		d.readS32(3, &m_lnaGain, -50);
		d.readS32(4, &m_mixerGain, 40);
		d.readS32(5, &m_mixerEnhancement, 0);
		d.readS32(6, &m_if1gain, -30);
		d.readS32(7, &m_if2gain, 0);
		d.readS32(8, &m_if3gain, 0);
		d.readS32(9, &m_if4gain, 0);
		d.readS32(10, &m_if5gain, 30);
		d.readS32(11, &m_if6gain, 30);
		d.readS32(12, &m_opAmpI1, 0);
		d.readS32(13, &m_opAmpI2, 0);
		d.readS32(14, &m_opAmpQ1, 0);
		d.readS32(15, &m_opAmpQ2, 0);
		d.readS32(16, &m_dcOfsI, 0);
		d.readS32(17, &m_dcOfsQ, 0);
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}
#if 0
OsmoSDRInput::Settings::Settings() :
{
}

QString OsmoSDRInput::Settings::serialize() const
{
	return QString("osmosdr:a:%1:%2:%3:%4:%5:%6:%7:%8:%9:%10:%11:%12:%13:%14:%15:%16:%17:%18")
		.arg(centerFrequency)
		.arg(swapIQ ? 1 : 0)
		.arg(decimation)
		.arg(lnaGain)
		.arg(mixerGain)
		.arg(mixerEnhancement)
		.arg(if1gain)
		.arg(if2gain)
		.arg(if3gain)
		.arg(if4gain)
		.arg(if5gain)
		.arg(if6gain)
		.arg(opAmpI1)
		.arg(opAmpI2)
		.arg(opAmpQ1)
		.arg(opAmpQ2)
		.arg(dcOfsI)
		.arg(dcOfsQ);
}

bool OsmoSDRInput::Settings::deserialize(const QString& settings)
{
	QStringList list = settings.split(":");
	if(list.size() < 2)
		return false;
	if(list[0] != "osmosdr")
		return false;

	if(list[1] == "a") {
		bool ok;
		if(list.size() != 20)
			return false;
		centerFrequency = list[2].toLongLong(&ok);
		if(!ok)
			return false;
		swapIQ = (list[3].toInt(&ok) != 0) ? true : false;
		if(!ok)
			return false;
		decimation = list[4].toInt(&ok);
		if(!ok)
			return false;
		lnaGain = list[5].toInt(&ok);
		if(!ok)
			return false;
		mixerGain = list[6].toInt(&ok);
		if(!ok)
			return false;
		mixerEnhancement = list[7].toInt(&ok);
		if(!ok)
			return false;
		if1gain = list[8].toInt(&ok);
		if(!ok)
			return false;
		if2gain = list[9].toInt(&ok);
		if(!ok)
			return false;
		if3gain = list[10].toInt(&ok);
		if(!ok)
			return false;
		if4gain = list[11].toInt(&ok);
		if(!ok)
			return false;
		if5gain = list[12].toInt(&ok);
		if(!ok)
			return false;
		if6gain = list[13].toInt(&ok);
		if(!ok)
			return false;
		opAmpI1 = list[14].toInt(&ok);
		if(!ok)
			return false;
		opAmpI2 = list[15].toInt(&ok);
		if(!ok)
			return false;
		opAmpQ1 = list[16].toInt(&ok);
		if(!ok)
			return false;
		opAmpQ2 = list[17].toInt(&ok);
		if(!ok)
			return false;
		dcOfsI = list[18].toInt(&ok);
		if(!ok)
			return false;
		dcOfsQ = list[19].toInt(&ok);
		if(!ok)
			return false;
		return true;
	} else {
		return false;
	}
}

MessageRegistrator OsmoSDRInput::MsgConfigureSourceOsmoSDR::ID("MsgConfigureSourceOsmoSDR");
#endif
OsmoSDRInput::OsmoSDRInput(MessageQueue* msgQueueToGUI) :
	SampleSource(msgQueueToGUI),
	m_settings(),
	m_dev(NULL),
	m_osmoSDRThread(NULL),
	m_deviceDescription()
{
}

OsmoSDRInput::~OsmoSDRInput()
{
	stopInput();
}

bool OsmoSDRInput::startInput(int device)
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_dev != NULL)
		stopInput();

	char vendor[256];
	char product[256];
	char serial[256];
	int res;

	if(!m_sampleFifo.setSize(524288)) {
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	if((res = osmosdr_open(&m_dev, device)) < 0) {
		qCritical("could not open OsmoSDR #%d: %s", device, strerror(errno));
		return false;
	}

	vendor[0] = '\0';
	product[0] = '\0';
	serial[0] = '\0';
	if((res = osmosdr_get_usb_strings(m_dev, vendor, product, serial)) < 0) {
		qCritical("error accessing USB device");
		goto failed;
	}
	qDebug("OsmoSDRInput open: %s %s, SN: %s", vendor, product, serial);
	m_deviceDescription = QString("%1 (SN %2)").arg(product).arg(serial);

	if((res = osmosdr_set_tuner_gain_mode(m_dev, 1)) < 0) {
		qCritical("error setting tuner gain mode");
		goto failed;
	}

	if((res = osmosdr_reset_buffer(m_dev)) < 0) {
		qCritical("could not reset USB EP buffers: %s", strerror(errno));
		goto failed;
	}

	if((m_osmoSDRThread = new OsmoSDRThread(m_dev, &m_sampleFifo)) == NULL) {
		qFatal("out of memory");
		goto failed;
	}
	m_osmoSDRThread->startWork();

	mutexLocker.unlock();
	applySettings(m_generalSettings, m_settings, true);

	qDebug("OsmoSDRInput: start");

	return true;

failed:
	stopInput();
	return false;
}

void OsmoSDRInput::stopInput()
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_osmoSDRThread != NULL) {
		m_osmoSDRThread->stopWork();
		delete m_osmoSDRThread;
		m_osmoSDRThread = NULL;
	}
	if(m_dev != NULL) {
		osmosdr_close(m_dev);
		m_dev = NULL;
	}
	m_deviceDescription.clear();
}

const QString& OsmoSDRInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int OsmoSDRInput::getSampleRate() const
{
	return 4000000 / (1 << m_settings.m_decimation);
}

quint64 OsmoSDRInput::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

bool OsmoSDRInput::handleMessage(Message* message)
{
	if(MsgConfigureOsmoSDR::match(message)) {
		MsgConfigureOsmoSDR* conf = (MsgConfigureOsmoSDR*)message;
		if(!applySettings(conf->getGeneralSettings(), conf->getSettings(), false))
			qDebug("OsmoSDR config error");
		message->completed();
		return true;
	} else {
		return false;
	}

	/*
	if(cmd->sourceType() != DSPCmdConfigureSourceOsmoSDR::SourceType)
		return false;
	if(!applySettings(((DSPCmdConfigureSourceOsmoSDR*)cmd)->getSettings(), false))
		qDebug("OsmoSDR config error");
	cmd->completed();
	return true;
	*/
	return false;
}

bool OsmoSDRInput::applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);

	if((m_generalSettings.m_centerFrequency != generalSettings.m_centerFrequency) || force) {
		m_generalSettings.m_centerFrequency = generalSettings.m_centerFrequency;
		if(m_dev != NULL) {
			if(osmosdr_set_center_freq(m_dev, m_generalSettings.m_centerFrequency) != 0)
				qDebug("osmosdr_set_center_freq(%lld) failed", m_generalSettings.m_centerFrequency);
		}
	}

	if((m_settings.m_swapIQ != settings.m_swapIQ) || force) {
		m_settings.m_swapIQ = settings.m_swapIQ;
		if(m_dev != NULL) {
			if(osmosdr_set_fpga_iq_swap(m_dev, m_settings.m_swapIQ ? 1 : 0) == 0)
				qDebug("osmosdr_set_fpga_iq_swap() failed");
		}
	}

	if((m_settings.m_decimation != settings.m_decimation) || force) {
		m_settings.m_decimation = settings.m_decimation;
		if(m_dev != NULL) {
			if(!osmosdr_set_fpga_decimation(m_dev, m_settings.m_decimation))
				qDebug("osmosdr_set_fpga_decimation() failed");
		}
	}

	if((m_settings.m_lnaGain != settings.m_lnaGain) || force) {
		m_settings.m_lnaGain = settings.m_lnaGain;
		if(m_dev != NULL) {
			if(!osmosdr_set_tuner_lna_gain(m_dev, m_settings.m_lnaGain))
				qDebug("osmosdr_set_tuner_lna_gain() failed");
		}
	}

	if((m_settings.m_mixerGain != settings.m_mixerGain) || force) {
		m_settings.m_mixerGain = settings.m_mixerGain;
		if(m_dev != NULL) {
			if(!osmosdr_set_tuner_mixer_gain(m_dev, m_settings.m_mixerGain))
				qDebug("osmosdr_set_tuner_mixer_gain() failed");
		}
	}

	if((m_settings.m_mixerEnhancement != settings.m_mixerEnhancement) || force) {
		m_settings.m_mixerEnhancement = settings.m_mixerEnhancement;
		if(m_dev != NULL) {
			if(!osmosdr_set_tuner_mixer_enh(m_dev, m_settings.m_mixerEnhancement))
				qDebug("osmosdr_set_tuner_mixer_enh() failed");
		}
	}

	if((m_settings.m_if1gain != settings.m_if1gain) || force) {
		m_settings.m_if1gain = settings.m_if1gain;
		if(m_dev != NULL) {
			if(!osmosdr_set_tuner_if_gain(m_dev, 1, m_settings.m_if1gain))
				qDebug("osmosdr_set_tuner_if_gain(1) failed");
		}
	}

	if((m_settings.m_if2gain != settings.m_if2gain) || force) {
		m_settings.m_if2gain = settings.m_if2gain;
		if(m_dev != NULL) {
			if(!osmosdr_set_tuner_if_gain(m_dev, 2, m_settings.m_if2gain))
				qDebug("osmosdr_set_tuner_if_gain(2) failed");
		}
	}

	if((m_settings.m_if3gain != settings.m_if3gain) || force) {
		m_settings.m_if3gain = settings.m_if3gain;
		if(m_dev != NULL) {
			if(!osmosdr_set_tuner_if_gain(m_dev, 3, m_settings.m_if3gain))
				qDebug("osmosdr_set_tuner_if_gain(3) failed");
		}
	}

	if((m_settings.m_if4gain != settings.m_if4gain) || force) {
		m_settings.m_if4gain = settings.m_if4gain;
		if(m_dev != NULL) {
			if(!osmosdr_set_tuner_if_gain(m_dev, 4, m_settings.m_if4gain))
				qDebug("osmosdr_set_tuner_if_gain(4) failed");
		}
	}

	if((m_settings.m_if5gain != settings.m_if5gain) || force) {
		m_settings.m_if5gain = settings.m_if5gain;
		if(m_dev != NULL) {
			if(!osmosdr_set_tuner_if_gain(m_dev, 5, m_settings.m_if5gain))
				qDebug("osmosdr_set_tuner_if_gain(5) failed");
		}
	}

	if((m_settings.m_if6gain != settings.m_if6gain) || force) {
		m_settings.m_if6gain = settings.m_if6gain;
		if(m_dev != NULL) {
			if(!osmosdr_set_tuner_if_gain(m_dev, 6, m_settings.m_if6gain))
				qDebug("osmosdr_set_tuner_if_gain(6) failed");
		}
	}

	if((m_settings.m_opAmpI1 != settings.m_opAmpI1) || (m_settings.m_opAmpI2 != settings.m_opAmpI2) ||
	   (m_settings.m_opAmpQ1 != settings.m_opAmpQ1) || (m_settings.m_opAmpQ2 != settings.m_opAmpQ2) ||
	   force) {
		m_settings.m_opAmpI1 = settings.m_opAmpI1;
		m_settings.m_opAmpI2 = settings.m_opAmpI2;
		m_settings.m_opAmpQ1 = settings.m_opAmpQ1;
		m_settings.m_opAmpQ2 = settings.m_opAmpQ2;
		if(m_dev != NULL) {
			if(!osmosdr_set_iq_amp(m_dev, m_settings.m_opAmpI1, m_settings.m_opAmpI2, m_settings.m_opAmpQ1, m_settings.m_opAmpQ2))
				qDebug("osmosdr_set_iq_amp(1) failed");
		}
	}

	if((m_settings.m_dcOfsI != settings.m_dcOfsI) || (m_settings.m_dcOfsQ != settings.m_dcOfsQ) ||
	   force) {
		m_settings.m_dcOfsI = settings.m_dcOfsI;
		m_settings.m_dcOfsQ = settings.m_dcOfsQ;
		if(m_dev != NULL) {
			if(!osmosdr_set_tuner_dc_offset(m_dev, m_settings.m_dcOfsI, m_settings.m_dcOfsQ))
				qDebug("osmosdr_set_tuner_dc_offset() failed");
		}
	}

	return true;
}
