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

MESSAGE_CLASS_DEFINITION(HackRFInput::MsgConfigureHackRF, Message)
MESSAGE_CLASS_DEFINITION(HackRFInput::MsgReportHackRF, Message)

HackRFInput::Settings::Settings() :
	m_centerFrequency(435000*1000),
	m_devSampleRateIndex(0),
	m_LOppmTenths(0),
	m_lnaGain(14),
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
	m_bandwidthIndex = data.m_bandwidthIndex;
	m_vgaGain = data.m_vgaGain;
	m_biasT = data.m_biasT;
	m_lnaExt = data.m_lnaExt;

	return valid;
}

HackRFInput::HackRFInput() :
	m_settings(),
	m_dev(0),
	m_hackRFThread(0),
	m_deviceDescription("HackRF")
{
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
	hackrf_error rc;

	rc = (hackrf_error) hackrf_init();

	if (rc != HACKRF_SUCCESS)
	{
		qCritical("HackRFInput::start: failed to initiate HackRF library %s", hackrf_error_name(rc));
	}

	if (m_dev != 0)
	{
		stop();
	}

	if (!m_sampleFifo.setSize(1<<19))
	{
		qCritical("HackRFInput::start: could not allocate SampleFifo");
		return false;
	}

	if ((m_dev = open_hackrf_from_sequence(device)) == 0)
	{
		qCritical("HackRFInput::start: could not open HackRF #%d", device);
		return false;
	}

	if((m_hackRFThread = new HackRFThread(m_dev, &m_sampleFifo)) == 0)
	{
		qFatal("HackRFInput::start: out of memory");
		stop();
		return false;
	}

	m_hackRFThread->startWork();

	mutexLocker.unlock();

	applySettings(m_settings, true);

	qDebug("HackRFInput::startInput: started");

	return true;
}

void HackRFInput::stop()
{
	qDebug("HackRFInput::stop");
	QMutexLocker mutexLocker(&m_mutex);

	if(m_hackRFThread != 0)
	{
		m_hackRFThread->stopWork();
		delete m_hackRFThread;
		m_hackRFThread = 0;
	}

	if(m_dev != 0)
	{
		hackrf_stop_rx(m_dev);
		hackrf_close(m_dev);
		m_dev = 0;
	}

	hackrf_exit();
}

const QString& HackRFInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int HackRFInput::getSampleRate() const
{
	int rate = HackRFSampleRates::m_rates_k[m_settings.m_devSampleRateIndex] * 1000;
	return (rate / (1<<m_settings.m_log2Decim));
}

quint64 HackRFInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

bool HackRFInput::handleMessage(const Message& message)
{
	if (MsgConfigureHackRF::match(message))
	{
		MsgConfigureHackRF& conf = (MsgConfigureHackRF&) message;
		qDebug() << "HackRFInput::handleMessage: MsgConfigureHackRF";

		bool success = applySettings(conf.getSettings(), false);

		if (!success)
		{
			qDebug("HackRFInput::handleMessage: config error");
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

	hackrf_error rc = (hackrf_error) hackrf_set_freq(m_dev, static_cast<uint64_t>(freq_hz));

	if (rc != HACKRF_SUCCESS)
	{
		qWarning("HackRFInput::setCenterFrequency: could not frequency to %llu Hz", freq_hz);
	}
	else
	{
		qWarning("HackRFInput::setCenterFrequency: frequency set to %llu Hz", freq_hz);
	}
}

bool HackRFInput::applySettings(const Settings& settings, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);

	bool forwardChange = false;
	hackrf_error rc;

	qDebug() << "HackRFInput::applySettings";

	if ((m_settings.m_devSampleRateIndex != settings.m_devSampleRateIndex) || force)
	{
		forwardChange = true;

		if (settings.m_devSampleRateIndex < HackRFSampleRates::m_nb_rates)
		{
			m_settings.m_devSampleRateIndex = settings.m_devSampleRateIndex;
		}
		else
		{
			m_settings.m_devSampleRateIndex = HackRFSampleRates::m_nb_rates - 1;
		}

		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_sample_rate_manual(m_dev, HackRFSampleRates::m_rates_k[m_settings.m_devSampleRateIndex]*1000, 1);

			if (rc != HACKRF_SUCCESS)
			{
				qCritical("HackRFInput::applySettings: could not set sample rate index %u (%d kS/s): %s", m_settings.m_devSampleRateIndex, HackRFSampleRates::m_rates_k[m_settings.m_devSampleRateIndex], hackrf_error_name(rc));
			}
			else
			{
				qDebug("HackRFInput::applySettings: sample rate set to index: %u (%d kS/s)", m_settings.m_devSampleRateIndex, HackRFSampleRates::m_rates_k[m_settings.m_devSampleRateIndex]);
				m_hackRFThread->setSamplerate(HackRFSampleRates::m_rates_k[m_settings.m_devSampleRateIndex]);
			}
		}
	}

	if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
	{
		m_settings.m_log2Decim = settings.m_log2Decim;
		forwardChange = true;

		if(m_dev != 0)
		{
			m_hackRFThread->setLog2Decimation(m_settings.m_log2Decim);
			qDebug() << "HackRFInput: set decimation to " << (1<<m_settings.m_log2Decim);
		}
	}

	if ((m_settings.m_fcPos != settings.m_fcPos) || force)
	{
		m_settings.m_fcPos = settings.m_fcPos;

		if(m_dev != 0)
		{
			m_hackRFThread->setFcPos((int) m_settings.m_fcPos);
			qDebug() << "HackRFInput: set fc pos (enum) to " << (int) m_settings.m_fcPos;
		}
	}

	qint64 deviceCenterFrequency = m_settings.m_centerFrequency;
	qint64 f_img = deviceCenterFrequency;
	quint32 devSampleRate = HackRFSampleRates::m_rates_k[m_settings.m_devSampleRateIndex] * 1000;

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

			qDebug() << "HackRFInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
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
			rc = (hackrf_error) hackrf_set_lna_gain(m_dev, m_settings.m_lnaGain);

			if(rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: airspy_set_lna_gain failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: LNA gain set to " << m_settings.m_lnaGain;
			}
		}
	}

	if ((m_settings.m_vgaGain != settings.m_vgaGain) || force)
	{
		m_settings.m_vgaGain = settings.m_vgaGain;

		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_vga_gain(m_dev, m_settings.m_vgaGain);

			if (rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: hackrf_set_vga_gain failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: VGA gain set to " << m_settings.m_vgaGain;
			}
		}
	}

	if ((m_settings.m_bandwidthIndex != settings.m_bandwidthIndex) || force)
	{

		if (settings.m_bandwidthIndex < HackRFBandwidths::m_nb_bw)
		{
			m_settings.m_bandwidthIndex = settings.m_bandwidthIndex;
		}
		else
		{
			m_settings.m_bandwidthIndex = HackRFBandwidths::m_nb_bw - 1;
		}

		if (m_dev != 0)
		{
			uint32_t bw_index = hackrf_compute_baseband_filter_bw_round_down_lt(HackRFBandwidths::m_bw_k[m_settings.m_bandwidthIndex]);
			rc = (hackrf_error) hackrf_set_baseband_filter_bandwidth(m_dev, bw_index);

			if (rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: hackrf_set_baseband_filter_bandwidth failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: Baseband BW gain set to " << HackRFBandwidths::m_bw_k[m_settings.m_bandwidthIndex];
			}
		}
	}

	if ((m_settings.m_biasT != settings.m_biasT) || force)
	{
		m_settings.m_biasT = settings.m_biasT;

		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_antenna_enable(m_dev, (m_settings.m_biasT ? 1 : 0));

			if(rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: hackrf_set_antenna_enable failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: bias tee set to " << m_settings.m_biasT;
			}
		}
	}

	if ((m_settings.m_lnaExt != settings.m_lnaExt) || force)
	{
		m_settings.m_lnaExt = settings.m_lnaExt;

		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_amp_enable(m_dev, (m_settings.m_lnaExt ? 1 : 0));

			if(rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: hackrf_set_amp_enable failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: extra LNA set to " << m_settings.m_lnaExt;
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

hackrf_device *HackRFInput::open_hackrf_from_sequence(int sequence)
{
	hackrf_device_list_t *hackrf_devices = hackrf_device_list();
	hackrf_device *hackrf_ptr;
	hackrf_error rc;

	rc = (hackrf_error) hackrf_device_list_open(hackrf_devices, sequence, &hackrf_ptr);

	if (rc == HACKRF_SUCCESS)
	{
		return hackrf_ptr;
	}
	else
	{
		return 0;
	}
}
