///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include "airspygui.h"
#include "airspyinput.h"
#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "airspyserializer.h"
#include "airspythread.h"

MESSAGE_CLASS_DEFINITION(AirspyInput::MsgConfigureAirspy, Message)
MESSAGE_CLASS_DEFINITION(AirspyInput::MsgReportAirspy, Message)

AirspyInput::Settings::Settings() :
	m_centerFrequency(435000*1000),
	m_devSampleRate(2500000),
	m_LOppmTenths(0),
	m_lnaGain(1),
	m_mixerGain(5),
	m_vgaGain(5),
	m_log2Decim(0),
	m_fcPos(FC_POS_INFRA),
	m_biasT(false)
{
}

void AirspyInput::Settings::resetToDefaults()
{
	m_centerFrequency = 435000*1000;
	m_devSampleRate = 2500000;
	m_LOppmTenths = 0;
	m_lnaGain = 1;
	m_mixerGain = 5;
	m_vgaGain = 5;
	m_log2Decim = 0;
	m_fcPos = FC_POS_INFRA;
	m_biasT = false;
}

QByteArray AirspyInput::Settings::serialize() const
{
	AirspySerializer::AirspyData data;

	data.m_data.m_frequency = m_centerFrequency;
	data.m_LOppmTenths = m_LOppmTenths;
	data.m_sampleRate = m_devSampleRate;
	data.m_log2Decim = m_log2Decim;
	data.m_fcPos = m_fcPos;
	data.m_lnaGain = m_lnaGain;
	data.m_mixerGain = m_mixerGain;
	data.m_vgaGain = m_vgaGain;
	data.m_biasT  = m_biasT;

	QByteArray byteArray;

	AirspySerializer::writeSerializedData(data, byteArray);

	return byteArray;
}

bool AirspyInput::Settings::deserialize(const QByteArray& serializedData)
{
	AirspySerializer::AirspyData data;

	bool valid = AirspySerializer::readSerializedData(serializedData, data);

	m_centerFrequency = data.m_data.m_frequency;
	m_LOppmTenths = data.m_LOppmTenths;
	m_devSampleRate = data.m_sampleRate;
	m_log2Decim = data.m_log2Decim;
	m_fcPos = data.m_fcPos;
	m_lnaGain = data.m_lnaGain;
	m_mixerGain = data.m_mixerGain;
	m_vgaGain = data.m_vgaGain;
	m_biasT = data.m_biasT;

	return valid;
}

AirspyInput::AirspyInput() :
	m_settings(),
	m_dev(0),
	m_airspyThread(0),
	m_deviceDescription("Airspy")
{
}

AirspyInput::~AirspyInput()
{
	stop();
}

bool AirspyInput::init(const Message& cmd)
{
	return false;
}

bool AirspyInput::start(int device)
{
	QMutexLocker mutexLocker(&m_mutex);
	airspy_error rc;

	rc = airspy_init();

	if (rc != AIRSPY_SUCCESS)
	{
		qCritical("AirspyInput::start: failed to initiate Airspy library %s", airspy_error_name(rc));
	}

	if (m_dev != 0)
	{
		stop();
	}

	if (!m_sampleFifo.setSize(96000 * 4))
	{
		qCritical("AirspyInput::start: could not allocate SampleFifo");
		return false;
	}

	if ((m_dev = open_airspy_from_sequence(device)) == 0) // TODO: fix; Open first available device as there is no proper handling for multiple devices
	{
		qCritical("AirspyInput::start: could not open Airspy");
		return false;
	}

#ifdef LIBAIRSPY_OLD
	m_sampleRates.push_back(2500000);
	m_sampleRates.push_back(10000000)
#else
	uint32_t nbSampleRates;
	uint32_t sampleRates[];

	airspy_get_samplerates(m_dev, &nbSampleRates, 0);

	sampleRates = new uint32_t[nbSampleRates];

	airspy_get_samplerates(m_dev, sampleRates, nbSampleRates);

	for (int i=0; i<nbSampleRates; i++)
	{
		m_sampleRates.push_back(sampleRates[i]);
	}

	delete[] sampleRates;
#endif

	MsgReportAirspy *message = MsgReportAirspy::create(m_sampleRates);
	getOutputMessageQueueToGUI()->push(message);

	if((m_airspyThread = new AirspyThread(m_dev, &m_sampleFifo)) == NULL) {
		qFatal("out of memory");
		goto failed;
	}

	m_airspyThread->startWork();

	mutexLocker.unlock();
	applySettings(m_settings, true);

	qDebug("AirspyInput::startInput: started");

	return true;
}

void AirspyInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_airspyThread != 0)
	{
		m_airspyThread->stopWork();
		delete m_airspyThread;
		m_airspyThread = 0;
	}

	if(m_dev != 0)
	{
		airspy_stop_rx(m_dev);
		airspy_close(m_dev);
		m_dev = 0;
	}

	airspy_exit();
}

const QString& AirspyInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int AirspyInput::getSampleRate() const
{
	int rate = m_settings.m_devSampleRate;
	return (rate / (1<<m_settings.m_log2Decim));
}

quint64 AirspyInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

bool AirspyInput::handleMessage(const Message& message)
{
	if (MsgConfigureAirspy::match(message))
	{
		MsgConfigureAirspy& conf = (MsgConfigureAirspy&) message;
		qDebug() << "AirspyInput::handleMessage: MsgConfigureAirspy";

		if (!applySettings(conf.getSettings(), false))
		{
			qDebug("Airspy config error");
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool AirspyInput::applySettings(const Settings& settings, bool force)
{
	bool forwardChange = false;
	airspy_error rc;
	QMutexLocker mutexLocker(&m_mutex);

	qDebug() << "AirspyInput::applySettings: m_dev: " << m_dev;

	if ((m_settings.m_lnaGain != settings.m_lnaGain) || force)
	{
		m_settings.m_lnaGain = settings.m_lnaGain;

		if (m_dev != 0)
		{
			rc = airspy_set_lna_gain(m_dev, m_settings.m_lnaGain);

			if(rc != AIRSPY_SUCCESS)
			{
				qDebug("AirspyInput::applySettings: airspy_set_lna_gain failed: %s", airspy_error_name(rc));
			}
			else
			{
				qDebug() << "AirspyInput:applySettings: LNA gain set to " << m_settings.m_lnaGain;
			}
		}
	}

	if ((m_settings.m_mixerGain != settings.m_mixerGain) || force)
	{
		m_settings.m_mixerGain = settings.m_mixerGain;

		if (m_dev != 0)
		{
			rc = airspy_set_mixer_gain(m_dev, m_settings.m_mixerGain);

			if(rc != AIRSPY_SUCCESS)
			{
				qDebug("AirspyInput::applySettings: airspy_set_mixer_gain failed: %s", airspy_error_name(rc));
			}
			else
			{
				qDebug() << "AirspyInput:applySettings: mixer gain set to " << m_settings.m_mixerGain;
			}
		}
	}

	if ((m_settings.m_vgaGain != settings.m_vgaGain) || force)
	{
		m_settings.m_vgaGain = settings.m_vgaGain;

		if (m_dev != 0)
		{
			rc = airspy_set_vga_gain(m_dev, m_settings.m_vgaGain);

			if(rc != AIRSPY_SUCCESS)
			{
				qDebug("AirspyInput::applySettings: airspy_set_vga_gain failed: %s", airspy_error_name(rc));
			}
			else
			{
				qDebug() << "AirspyInput:applySettings: VGA gain set to " << m_settings.m_vgaGain;
			}
		}
	}

	if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
	{
		m_settings.m_devSampleRate = settings.m_devSampleRate;
		forwardChange = true;

		if (m_dev != 0)
		{
			unsigned int actualSamplerate;

			if (bladerf_set_sample_rate(m_dev, BLADERF_MODULE_RX, m_settings.m_devSampleRate, &actualSamplerate) < 0)
			{
				qCritical("could not set sample rate: %d", m_settings.m_devSampleRate);
			}
			else
			{
				qDebug() << "bladerf_set_sample_rate(BLADERF_MODULE_RX) actual sample rate is " << actualSamplerate;
				m_airspyThread->setSamplerate(m_settings.m_devSampleRate);
			}
		}
	}

	if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
	{
		m_settings.m_log2Decim = settings.m_log2Decim;
		forwardChange = true;

		if(m_dev != 0)
		{
			m_airspyThread->setLog2Decimation(m_settings.m_log2Decim);
			qDebug() << "AirspyInput: set decimation to " << (1<<m_settings.m_log2Decim);
		}
	}

	if ((m_settings.m_fcPos != settings.m_fcPos) || force)
	{
		m_settings.m_fcPos = settings.m_fcPos;

		if(m_dev != 0)
		{
			m_airspyThread->setFcPos((int) m_settings.m_fcPos);
			qDebug() << "AirspyInput: set fc pos (enum) to " << (int) m_settings.m_fcPos;
		}
	}

	if (m_settings.m_centerFrequency != settings.m_centerFrequency)
	{
		forwardChange = true;
	}

	m_settings.m_centerFrequency = settings.m_centerFrequency;

	qint64 deviceCenterFrequency = m_settings.m_centerFrequency;
	qint64 f_img = deviceCenterFrequency;
	qint64 f_cut = deviceCenterFrequency + m_settings.m_bandwidth/2;

	if ((m_settings.m_log2Decim == 0) || (m_settings.m_fcPos == FC_POS_CENTER))
	{
		deviceCenterFrequency = m_settings.m_centerFrequency;
		f_img = deviceCenterFrequency;
		f_cut = deviceCenterFrequency + m_settings.m_bandwidth/2;
	}
	else
	{
		if (m_settings.m_fcPos == FC_POS_INFRA)
		{
			deviceCenterFrequency = m_settings.m_centerFrequency + (m_settings.m_devSampleRate / 4);
			f_img = deviceCenterFrequency + m_settings.m_devSampleRate/2;
			f_cut = deviceCenterFrequency + m_settings.m_bandwidth/2;
		}
		else if (m_settings.m_fcPos == FC_POS_SUPRA)
		{
			deviceCenterFrequency = m_settings.m_centerFrequency - (m_settings.m_devSampleRate / 4);
			f_img = deviceCenterFrequency - m_settings.m_devSampleRate/2;
			f_cut = deviceCenterFrequency - m_settings.m_bandwidth/2;
		}
	}

	if (m_dev != NULL)
	{
		if (bladerf_set_frequency( m_dev, BLADERF_MODULE_RX, deviceCenterFrequency ) != 0)
		{
			qDebug("bladerf_set_frequency(%lld) failed", m_settings.m_centerFrequency);
		}
	}

	if (forwardChange)
	{
		int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
		getOutputMessageQueue()->push(notif);
	}

	qDebug() << "AirspyInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
			<< " device center freq: " << deviceCenterFrequency << " Hz"
			<< " device sample rate: " << m_settings.m_devSampleRate << "Hz"
			<< " Actual sample rate: " << m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim) << "Hz"
			<< " BW: " << m_settings.m_bandwidth << "Hz"
			<< " img: " << f_img << "Hz"
			<< " cut: " << f_cut << "Hz"
			<< " img - cut: " << f_img - f_cut;

	return true;
}

struct airspy_device *AirspyInput::open_airspy_from_sequence(int sequence)
{
	struct airspy_device *devinfo;
	int rc;

	for (int i=0; i < AIRSPY_MAX_DEVICE; i++)
	{
		rc = airspy_open(&devinfo);

		if ((rc == AIRSPY_SUCCESS) && (i == sequence))
		{
			return devinfo;
		}
		else
		{
			break; // finished
		}
	}

	return 0;
}
