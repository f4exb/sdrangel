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
#include "bladerfgui.h"
#include "bladerfinput.h"
#include "bladerfthread.h"

MESSAGE_CLASS_DEFINITION(BladerfInput::MsgConfigureBladerf, Message)
MESSAGE_CLASS_DEFINITION(BladerfInput::MsgReportBladerf, Message)

BladerfInput::Settings::Settings() :
	m_centerFrequency(435000*1000),
	m_devSampleRate(3072000),
	m_lnaGain(0),
	m_vga1(20),
	m_vga2(9),
	m_bandwidth(1500000),
	m_log2Decim(0),
	m_fcPos(FC_POS_INFRA),
	m_xb200(false),
	m_xb200Path(BLADERF_XB200_MIX),
	m_xb200Filter(BLADERF_XB200_AUTO_1DB)
{
}

void BladerfInput::Settings::resetToDefaults()
{
	m_centerFrequency = 435000*1000;
	m_devSampleRate = 3072000;
	m_lnaGain = 0;
	m_vga1 = 20;
	m_vga2 = 9;
	m_bandwidth = 1500000;
	m_log2Decim = 0;
	m_fcPos = FC_POS_INFRA;
	m_xb200 = false;
	m_xb200Path = BLADERF_XB200_MIX;
	m_xb200Filter = BLADERF_XB200_AUTO_1DB;
}

QByteArray BladerfInput::Settings::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_lnaGain);
	s.writeS32(2, m_vga1);
	s.writeS32(3, m_vga2);
	s.writeU32(4, m_log2Decim);
	s.writeBool(5, m_xb200);
	s.writeS32(6, (int) m_xb200Path);
	s.writeS32(7, (int) m_xb200Filter);
	s.writeS32(8, m_bandwidth);
	s.writeS32(9, (int) m_fcPos);
	s.writeU64(10, m_centerFrequency);
	s.writeS32(11, m_devSampleRate);
	return s.final();
}

bool BladerfInput::Settings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		int intval;
		d.readS32(1, &m_lnaGain, 0);
		d.readS32(2, &m_vga1, 20);
		d.readS32(3, &m_vga2, 9);
		d.readU32(4, &m_log2Decim, 0);
		d.readBool(5, &m_xb200);
		d.readS32(6, &intval);
		m_xb200Path = (bladerf_xb200_path) intval;
		d.readS32(7, &intval);
		m_xb200Filter = (bladerf_xb200_filter) intval;
		d.readS32(8, &m_bandwidth, 0);
		d.readS32(9, &intval, 0);
		m_fcPos = (fcPos_t) intval;
		d.readU64(10, &m_centerFrequency, 435000*1000);
		d.readS32(11, &m_devSampleRate, 3072000);
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

BladerfInput::BladerfInput() :
	m_settings(),
	m_dev(0),
	m_bladerfThread(0),
	m_deviceDescription("BladeRF")
{
}

BladerfInput::~BladerfInput()
{
	stop();
}

bool BladerfInput::init(const Message& cmd)
{
	return false;
}

bool BladerfInput::start(int device)
{
	QMutexLocker mutexLocker(&m_mutex);

	if (m_dev != 0)
	{
		stop();
	}

	int res;
	int fpga_loaded;

	if (!m_sampleFifo.setSize(96000 * 4))
	{
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	if ((m_dev = open_bladerf_from_serial(0)) == 0) // TODO: fix; Open first available device as there is no proper handling for multiple devices
	{
		qCritical("could not open BladeRF");
		return false;
	}

    fpga_loaded = bladerf_is_fpga_configured(m_dev);

    if (fpga_loaded < 0)
    {
    	qCritical("Failed to check FPGA state: %s",
                  bladerf_strerror(fpga_loaded));
    	return false;
    }
    else if (fpga_loaded == 0)
    {
    	qCritical("The device's FPGA is not loaded.");
    	return false;
    }

    // TODO: adjust USB transfer data according to sample rate
    if ((res = bladerf_sync_config(m_dev, BLADERF_MODULE_RX, BLADERF_FORMAT_SC16_Q11, 64, 8192, 32, 10000)) < 0)
    {
    	qCritical("bladerf_sync_config with return code %d", res);
    	goto failed;
    }

    if ((res = bladerf_enable_module(m_dev, BLADERF_MODULE_RX, true)) < 0)
    {
    	qCritical("bladerf_enable_module with return code %d", res);
    	goto failed;
    }

	if((m_bladerfThread = new BladerfThread(m_dev, &m_sampleFifo)) == NULL) {
		qFatal("out of memory");
		goto failed;
	}

	m_bladerfThread->startWork();

	mutexLocker.unlock();
	applySettings(m_settings, true);

	qDebug("BladerfInput::startInput: started");

	return true;

failed:
	stop();
	return false;
}

void BladerfInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_bladerfThread != 0)
	{
		m_bladerfThread->stopWork();
		delete m_bladerfThread;
		m_bladerfThread = 0;
	}

	if(m_dev != 0)
	{
		bladerf_close(m_dev);
		m_dev = 0;
	}

	m_deviceDescription.clear();
}

const QString& BladerfInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int BladerfInput::getSampleRate() const
{
	int rate = m_settings.m_devSampleRate;
	return (rate / (1<<m_settings.m_log2Decim));
}

quint64 BladerfInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

bool BladerfInput::handleMessage(const Message& message)
{
	qDebug() << "BladerfInput::handleMessage";

	if (MsgConfigureBladerf::match(message))
	{
		qDebug() << "  - MsgConfigureBladerf";

		MsgConfigureBladerf& conf = (MsgConfigureBladerf&) message;

		if (!applySettings(conf.getSettings(), false))
		{
			qDebug("BladeRF config error");
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool BladerfInput::applySettings(const Settings& settings, bool force)
{
	bool forwardChange = false;
	QMutexLocker mutexLocker(&m_mutex);

	qDebug() << "BladerfInput::applySettings: m_dev: " << m_dev;

	if ((m_settings.m_lnaGain != settings.m_lnaGain) || force)
	{
		m_settings.m_lnaGain = settings.m_lnaGain;

		if (m_dev != 0)
		{
			if(bladerf_set_lna_gain(m_dev, getLnaGain(m_settings.m_lnaGain)) != 0)
			{
				qDebug("bladerf_set_lna_gain() failed");
			}
			else
			{
				qDebug() << "BladerfInput: LNA gain set to " << getLnaGain(m_settings.m_lnaGain);
			}
		}
	}

	if ((m_settings.m_vga1 != settings.m_vga1) || force)
	{
		m_settings.m_vga1 = settings.m_vga1;

		if (m_dev != 0)
		{
			if(bladerf_set_rxvga1(m_dev, m_settings.m_vga1) != 0)
			{
				qDebug("bladerf_set_rxvga1() failed");
			}
			else
			{
				qDebug() << "BladerfInput: VGA1 gain set to " << m_settings.m_vga1;
			}
		}
	}

	if ((m_settings.m_vga2 != settings.m_vga2) || force)
	{
		m_settings.m_vga2 = settings.m_vga2;

		if(m_dev != 0)
		{
			if(bladerf_set_rxvga2(m_dev, m_settings.m_vga2) != 0)
			{
				qDebug("bladerf_set_rxvga2() failed");
			}
			else
			{
				qDebug() << "BladerfInput: VGA2 gain set to " << m_settings.m_vga2;
			}
		}
	}

	if ((m_settings.m_xb200 != settings.m_xb200) || force)
	{
		m_settings.m_xb200 = settings.m_xb200;

		if (m_dev != 0)
		{
			if (m_settings.m_xb200)
			{
				if (bladerf_expansion_attach(m_dev, BLADERF_XB_200) != 0)
				{
					qDebug("bladerf_expansion_attach(xb200) failed");
				}
				else
				{
					qDebug() << "BladerfInput: Attach XB200";
				}
			}
			else
			{
				if (bladerf_expansion_attach(m_dev, BLADERF_XB_NONE) != 0)
				{
					qDebug("bladerf_expansion_attach(none) failed");
				}
				else
				{
					qDebug() << "BladerfInput: Detach XB200";
				}
			}
		}
	}

	if ((m_settings.m_xb200Path != settings.m_xb200Path) || force)
	{
		m_settings.m_xb200Path = settings.m_xb200Path;

		if (m_dev != 0)
		{
			if(bladerf_xb200_set_path(m_dev, BLADERF_MODULE_RX, m_settings.m_xb200Path) != 0)
			{
				qDebug("bladerf_xb200_set_path(BLADERF_MODULE_RX) failed");
			}
			else
			{
				qDebug() << "BladerfInput: set xb200 path to " << m_settings.m_xb200Path;
			}
		}
	}

	if ((m_settings.m_xb200Filter != settings.m_xb200Filter) || force)
	{
		m_settings.m_xb200Filter = settings.m_xb200Filter;

		if (m_dev != 0)
		{
			if(bladerf_xb200_set_filterbank(m_dev, BLADERF_MODULE_RX, m_settings.m_xb200Filter) != 0)
			{
				qDebug("bladerf_xb200_set_filterbank(BLADERF_MODULE_RX) failed");
			}
			else
			{
				qDebug() << "BladerfInput: set xb200 filter to " << m_settings.m_xb200Filter;
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
				m_bladerfThread->setSamplerate(m_settings.m_devSampleRate);
			}
		}
	}

	if ((m_settings.m_bandwidth != settings.m_bandwidth) || force)
	{
		m_settings.m_bandwidth = settings.m_bandwidth;

		if(m_dev != 0)
		{
			unsigned int actualBandwidth;

			if( bladerf_set_bandwidth(m_dev, BLADERF_MODULE_RX, m_settings.m_bandwidth, &actualBandwidth) < 0)
			{
				qCritical("could not set bandwidth: %d", m_settings.m_bandwidth);
			}
			else
			{
				qDebug() << "bladerf_set_bandwidth(BLADERF_MODULE_RX) actual bandwidth is " << actualBandwidth;
			}
		}
	}

	if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
	{
		m_settings.m_log2Decim = settings.m_log2Decim;
		forwardChange = true;

		if(m_dev != 0)
		{
			m_bladerfThread->setLog2Decimation(m_settings.m_log2Decim);
			qDebug() << "BladerfInput: set decimation to " << (1<<m_settings.m_log2Decim);
		}
	}

	if ((m_settings.m_fcPos != settings.m_fcPos) || force)
	{
		m_settings.m_fcPos = settings.m_fcPos;

		if(m_dev != 0)
		{
			m_bladerfThread->setFcPos((int) m_settings.m_fcPos);
			qDebug() << "BladerfInput: set fc pos (enum) to " << (int) m_settings.m_fcPos;
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

	qDebug() << "BladerfInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
			<< " device center freq: " << deviceCenterFrequency << " Hz"
			<< " device sample rate: " << m_settings.m_devSampleRate << "Hz"
			<< " Actual sample rate: " << m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim) << "Hz"
			<< " BW: " << m_settings.m_bandwidth << "Hz"
			<< " img: " << f_img << "Hz"
			<< " cut: " << f_cut << "Hz"
			<< " img - cut: " << f_img - f_cut;

	return true;
}

bladerf_lna_gain BladerfInput::getLnaGain(int lnaGain)
{
	if (lnaGain == 2)
	{
		return BLADERF_LNA_GAIN_MAX;
	}
	else if (lnaGain == 1)
	{
		return BLADERF_LNA_GAIN_MID;
	}
	else
	{
		return BLADERF_LNA_GAIN_BYPASS;
	}
}

struct bladerf *BladerfInput::open_bladerf_from_serial(const char *serial)
{
    int status;
    struct bladerf *dev;
    struct bladerf_devinfo info;

    /* Initialize all fields to "don't care" wildcard values.
     *
     * Immediately passing this to bladerf_open_with_devinfo() would cause
     * libbladeRF to open any device on any available backend. */
    bladerf_init_devinfo(&info);

    /* Specify the desired device's serial number, while leaving all other
     * fields in the info structure wildcard values */
    if (serial != NULL)
    {
		strncpy(info.serial, serial, BLADERF_SERIAL_LENGTH - 1);
		info.serial[BLADERF_SERIAL_LENGTH - 1] = '\0';
    }

    status = bladerf_open_with_devinfo(&dev, &info);

    if (status == BLADERF_ERR_NODEV)
    {
        fprintf(stderr, "No devices available with serial=%s\n", serial);
        return NULL;
    }
    else if (status != 0)
    {
        fprintf(stderr, "Failed to open device with serial=%s (%s)\n",
                serial, bladerf_strerror(status));
        return NULL;
    }
    else
    {
        return dev;
    }
}
