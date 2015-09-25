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

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "hackrfinput.h"

#include "hackrfgui.h"
#include "hackrfserializer.h"
#include "hackrfthread.h"

MESSAGE_CLASS_DEFINITION(HackRFInput::MsgConfigureHackRT, Message)
MESSAGE_CLASS_DEFINITION(HackRFInput::MsgReportHackRF, Message)

HackRFInput::Settings::Settings() :
	m_centerFrequency(435000*1000),
	m_devSampleRateIndex(0),
	m_LOppmTenths(0),
	m_lnaGain(14),
	m_imjRejFilterIndex(0),
	m_bandwidthIndex(0),
	m_vgaGain(4),
	m_log2Decim(0),
	m_fcPos(FC_POS_CENTER),
	m_biasT(false),
	m_lnaExt(false)
{
}

void HackRFInput::Settings::resetToDefaults()
{
	m_centerFrequency = 435000*1000;
	m_devSampleRateIndex = 0;
	m_LOppmTenths = 0;
	m_lnaGain = 14;
	m_imjRejFilterIndex = 0;
	m_bandwidthIndex = 0;
	m_vgaGain = 4;
	m_log2Decim = 0;
	m_fcPos = FC_POS_CENTER;
	m_biasT = false;
	m_lnaExt = false;
}

QByteArray HackRFInput::Settings::serialize() const
{
	HackRFSerializer::AirspyData data;

	data.m_data.m_frequency = m_centerFrequency;
	data.m_LOppmTenths = m_LOppmTenths;
	data.m_sampleRateIndex = m_devSampleRateIndex;
	data.m_log2Decim = m_log2Decim;
	data.m_fcPos = (qint32) m_fcPos;
	data.m_lnaGain = m_lnaGain;
	data.m_imjRejFilterIndex = m_imjRejFilterIndex;
	data.m_bandwidthIndex = m_bandwidthIndex;
	data.m_vgaGain = m_vgaGain;
	data.m_biasT  = m_biasT;
	data.m_lnaExt  = m_lnaExt;

	QByteArray byteArray;

	HackRFSerializer::writeSerializedData(data, byteArray);

	return byteArray;
}

bool HackRFInput::Settings::deserialize(const QByteArray& serializedData)
{
	HackRFSerializer::AirspyData data;

	bool valid = HackRFSerializer::readSerializedData(serializedData, data);

	m_centerFrequency = data.m_data.m_frequency;
	m_LOppmTenths = data.m_LOppmTenths;
	m_devSampleRateIndex = data.m_sampleRateIndex;
	m_log2Decim = data.m_log2Decim;
	m_fcPos = (fcPos_t) data.m_fcPos;
	m_lnaGain = data.m_lnaGain;
	m_imjRejFilterIndex = data.m_imjRejFilterIndex;
	m_bandwidthIndex = data.m_bandwidthIndex;
	m_vgaGain = data.m_vgaGain;
	m_biasT = data.m_biasT;
	m_lnaExt = data.m_lnaExt;

	return valid;
}

HackRFInput::HackRFInput() :
	m_settings(),
	m_dev(0),
	m_airspyThread(0),
	m_deviceDescription("Airspy")
{
	m_sampleRates.push_back(10000000);
	m_sampleRates.push_back(2500000);
}

HackRFInput::~HackRFInput()
{
	stop();
}

bool HackRFInput::init(const Message& cmd)
{
	return false;
}

bool HackRFInput::start(int device)
{
	QMutexLocker mutexLocker(&m_mutex);
	airspy_error rc;

	rc = (airspy_error) airspy_init();

	if (rc != AIRSPY_SUCCESS)
	{
		qCritical("AirspyInput::start: failed to initiate Airspy library %s", airspy_error_name(rc));
	}

	if (m_dev != 0)
	{
		stop();
	}

	if (!m_sampleFifo.setSize(1<<19))
	{
		qCritical("AirspyInput::start: could not allocate SampleFifo");
		return false;
	}

	if ((m_dev = open_airspy_from_sequence(device)) == 0)
	{
		qCritical("AirspyInput::start: could not open Airspy #%d", device);
		return false;
	}

#ifdef LIBAIRSPY_DYN_RATES
	uint32_t nbSampleRates;
	uint32_t *sampleRates;

	airspy_get_samplerates(m_dev, &nbSampleRates, 0);

	sampleRates = new uint32_t[nbSampleRates];

	airspy_get_samplerates(m_dev, sampleRates, nbSampleRates);

	if (nbSampleRates == 0)
	{
		qCritical("AirspyInput::start: could not obtain Airspy sample rates");
		return false;
	}

	m_sampleRates.clear();

	for (int i=0; i<nbSampleRates; i++)
	{
		m_sampleRates.push_back(sampleRates[i]);
		qDebug("AirspyInput::start: sampleRates[%d] = %u Hz", i, sampleRates[i]);
	}

	delete[] sampleRates;
#endif

	MsgReportHackRF *message = MsgReportHackRF::create(m_sampleRates);
	getOutputMessageQueueToGUI()->push(message);

	rc = (airspy_error) airspy_set_sample_type(m_dev, AIRSPY_SAMPLE_INT16_IQ);

	if (rc != AIRSPY_SUCCESS)
	{
		qCritical("AirspyInput::start: could not set sample type to INT16_IQ");
		return false;
	}

	if((m_airspyThread = new HackRFThread(m_dev, &m_sampleFifo)) == 0)
	{
		qFatal("AirspyInput::start: out of memory");
		stop();
		return false;
	}

	m_airspyThread->startWork();

	mutexLocker.unlock();

	applySettings(m_settings, true);

	qDebug("AirspyInput::startInput: started");

	return true;
}

void HackRFInput::stop()
{
	qDebug("AirspyInput::stop");
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

const QString& HackRFInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int HackRFInput::getSampleRate() const
{
	int rate = m_sampleRates[m_settings.m_devSampleRateIndex];
	return (rate / (1<<m_settings.m_log2Decim));
}

quint64 HackRFInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

bool HackRFInput::handleMessage(const Message& message)
{
	if (MsgConfigureHackRT::match(message))
	{
		MsgConfigureHackRT& conf = (MsgConfigureHackRT&) message;
		qDebug() << "AirspyInput::handleMessage: MsgConfigureAirspy";

		bool success = applySettings(conf.getSettings(), false);

		if (!success)
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

void HackRFInput::setCenterFrequency(quint64 freq_hz)
{
	freq_hz += (freq_hz * m_settings.m_LOppmTenths) / 10000000ULL;

	airspy_error rc = (airspy_error) airspy_set_freq(m_dev, static_cast<uint32_t>(freq_hz));

	if (rc != AIRSPY_SUCCESS)
	{
		qWarning("AirspyInput::setCenterFrequency: could not frequency to %llu Hz", freq_hz);
	}
	else
	{
		qWarning("AirspyInput::setCenterFrequency: frequency set to %llu Hz", freq_hz);
	}
}

bool HackRFInput::applySettings(const Settings& settings, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);

	bool forwardChange = false;
	airspy_error rc;

	qDebug() << "AirspyInput::applySettings";

	if ((m_settings.m_devSampleRateIndex != settings.m_devSampleRateIndex) || force)
	{
		forwardChange = true;

		if (settings.m_devSampleRateIndex < m_sampleRates.size())
		{
			m_settings.m_devSampleRateIndex = settings.m_devSampleRateIndex;
		}
		else
		{
			m_settings.m_devSampleRateIndex = m_sampleRates.size() - 1;
		}

		if (m_dev != 0)
		{
			rc = (airspy_error) airspy_set_samplerate(m_dev, static_cast<airspy_samplerate_t>(m_settings.m_devSampleRateIndex));

			if (rc != AIRSPY_SUCCESS)
			{
				qCritical("AirspyInput::applySettings: could not set sample rate index %u (%d S/s): %s", m_settings.m_devSampleRateIndex, m_sampleRates[m_settings.m_devSampleRateIndex], airspy_error_name(rc));
			}
			else
			{
				qDebug("AirspyInput::applySettings: sample rate set to index: %u (%d S/s)", m_settings.m_devSampleRateIndex, m_sampleRates[m_settings.m_devSampleRateIndex]);
				m_airspyThread->setSamplerate(m_sampleRates[m_settings.m_devSampleRateIndex]);
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

	qint64 deviceCenterFrequency = m_settings.m_centerFrequency;
	qint64 f_img = deviceCenterFrequency;
	quint32 devSampleRate = m_sampleRates[m_settings.m_devSampleRateIndex];

	if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency) ||
			(m_settings.m_LOppmTenths != settings.m_LOppmTenths))
	{
		m_settings.m_centerFrequency = settings.m_centerFrequency;
		m_settings.m_LOppmTenths = settings.m_LOppmTenths;

		if ((m_settings.m_log2Decim == 0) || (m_settings.m_fcPos == FC_POS_CENTER))
		{
			deviceCenterFrequency = m_settings.m_centerFrequency;
			f_img = deviceCenterFrequency;
		}
		else
		{
			if (m_settings.m_fcPos == FC_POS_INFRA)
			{
				deviceCenterFrequency = m_settings.m_centerFrequency + (devSampleRate / 4);
				f_img = deviceCenterFrequency + devSampleRate/2;
			}
			else if (m_settings.m_fcPos == FC_POS_SUPRA)
			{
				deviceCenterFrequency = m_settings.m_centerFrequency - (devSampleRate / 4);
				f_img = deviceCenterFrequency - devSampleRate/2;
			}
		}

		if (m_dev != 0)
		{
			setCenterFrequency(deviceCenterFrequency);

			qDebug() << "AirspyInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
					<< " device center freq: " << deviceCenterFrequency << " Hz"
					<< " device sample rate: " << devSampleRate << "Hz"
					<< " Actual sample rate: " << devSampleRate/(1<<m_settings.m_log2Decim) << "Hz"
					<< " img: " << f_img << "Hz";
		}

		forwardChange = true;
	}

	if ((m_settings.m_lnaGain != settings.m_lnaGain) || force)
	{
		m_settings.m_lnaGain = settings.m_lnaGain;

		if (m_dev != 0)
		{
			rc = (airspy_error) airspy_set_lna_gain(m_dev, m_settings.m_lnaGain);

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
			rc = (airspy_error) airspy_set_mixer_gain(m_dev, m_settings.m_mixerGain);

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
			rc = (airspy_error) airspy_set_vga_gain(m_dev, m_settings.m_vgaGain);

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

	if ((m_settings.m_biasT != settings.m_biasT) || force)
	{
		m_settings.m_biasT = settings.m_biasT;

		if (m_dev != 0)
		{
			rc = (airspy_error) airspy_set_rf_bias(m_dev, (m_settings.m_biasT ? 1 : 0));

			if(rc != AIRSPY_SUCCESS)
			{
				qDebug("AirspyInput::applySettings: airspy_set_rf_bias failed: %s", airspy_error_name(rc));
			}
			else
			{
				qDebug() << "AirspyInput:applySettings: bias tee set to " << m_settings.m_biasT;
			}
		}
	}

	if (forwardChange)
	{
		int sampleRate = devSampleRate/(1<<m_settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
		getOutputMessageQueue()->push(notif);
	}

	return true;
}

struct airspy_device *HackRFInput::open_airspy_from_sequence(int sequence)
{
	airspy_read_partid_serialno_t read_partid_serialno;
	struct airspy_device *devinfo, *retdev = 0;
	uint32_t serial_msb = 0;
	uint32_t serial_lsb = 0;
	airspy_error rc;
	int i;

	for (int i = 0; i < AIRSPY_MAX_DEVICE; i++)
	{
		rc = (airspy_error) airspy_open(&devinfo);

		if (rc == AIRSPY_SUCCESS)
		{
			if (i == sequence)
			{
				return devinfo;
			}
		}
		else
		{
			break;
		}
	}

	return 0;
}
