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
#include <cstdio>
#include <iostream>

#include "util/simpleserializer.h"
#include "bladerfgui.h"
#include "bladerfinput.h"
#include "bladerfthread.h"

MESSAGE_CLASS_DEFINITION(BladerfInput::MsgConfigureBladerf, Message)
MESSAGE_CLASS_DEFINITION(BladerfInput::MsgReportBladerf, Message)

BladerfInput::Settings::Settings() :
	m_lnaGain(0),
	m_vga1(20),
	m_vga2(9),
	m_samplerate(3072000),
	m_bandwidth(1500000),
	m_log2Decim(0),
	m_xb200(false),
	m_xb200Path(BLADERF_XB200_MIX),
	m_xb200Filter(BLADERF_XB200_AUTO_1DB)
{
}

void BladerfInput::Settings::resetToDefaults()
{
	m_lnaGain = 0;
	m_vga1 = 20;
	m_vga2 = 9;
	m_samplerate = 3072000;
	m_bandwidth = 1500000;
	m_log2Decim = 0;
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
	s.writeS32(4, m_samplerate);
	s.writeU32(5, m_log2Decim);
	s.writeBool(6, m_xb200);
	s.writeS32(7, (int) m_xb200Path);
	s.writeS32(8, (int) m_xb200Filter);
	s.writeS32(9, m_bandwidth);
	return s.final();
}

bool BladerfInput::Settings::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		int intval;
		d.readS32(1, &m_lnaGain, 0);
		d.readS32(2, &m_vga1, 20);
		d.readS32(3, &m_vga2, 9);
		d.readS32(4, &m_samplerate, 0);
		d.readU32(5, &m_log2Decim, 0);
		d.readBool(6, &m_xb200);
		d.readS32(7, &intval);
		m_xb200Path = (bladerf_xb200_path) intval;
		d.readS32(8, &intval);
		m_xb200Filter = (bladerf_xb200_filter) intval;
		d.readS32(9, &m_bandwidth, 0);
		return true;
	} else {
		resetToDefaults();
		return false;
	}
}

BladerfInput::BladerfInput(MessageQueue* msgQueueToGUI) :
	SampleSource(msgQueueToGUI),
	m_settings(),
	m_dev(NULL),
	m_bladerfThread(NULL),
	m_deviceDescription()
{
}

BladerfInput::~BladerfInput()
{
	stopInput();
}

bool BladerfInput::startInput(int device)
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_dev != NULL)
		stopInput();

	int res;
	int fpga_loaded;

	if(!m_sampleFifo.setSize(96000 * 4)) {
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	if ((m_dev = open_bladerf_from_serial(0)) == NULL) // TODO: fix; Open first available device as there is no proper handling for multiple devices
	{
		qCritical("could not open BladeRF");
		return false;
	}

    fpga_loaded = bladerf_is_fpga_configured(m_dev);

    if (fpga_loaded < 0) {
    	qCritical("Failed to check FPGA state: %s",
                  bladerf_strerror(fpga_loaded));
    	return false;
    } else if (fpga_loaded == 0) {
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
	applySettings(m_generalSettings, m_settings, true);

	qDebug("bladerfInput: start");
	//MsgReportBladerf::create(m_gains)->submit(m_guiMessageQueue); Pass anything here

	return true;

failed:
	stopInput();
	return false;
}

void BladerfInput::stopInput()
{
	QMutexLocker mutexLocker(&m_mutex);

	if(m_bladerfThread != NULL) {
		m_bladerfThread->stopWork();
		delete m_bladerfThread;
		m_bladerfThread = NULL;
	}
	if(m_dev != NULL) {
		bladerf_close(m_dev);
		m_dev = NULL;
	}
	m_deviceDescription.clear();
}

const QString& BladerfInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int BladerfInput::getSampleRate() const
{
	int rate = m_settings.m_samplerate;
	return (rate / (1<<m_settings.m_log2Decim));
}

quint64 BladerfInput::getCenterFrequency() const
{
	return m_generalSettings.m_centerFrequency;
}

bool BladerfInput::handleMessage(Message* message)
{
	if(MsgConfigureBladerf::match(message)) {
		MsgConfigureBladerf* conf = (MsgConfigureBladerf*)message;
		if(!applySettings(conf->getGeneralSettings(), conf->getSettings(), false))
			qDebug("BladeRF config error");
		message->completed();
		return true;
	} else {
		return false;
	}
}

bool BladerfInput::applySettings(const GeneralSettings& generalSettings, const Settings& settings, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);

	if((m_settings.m_lnaGain != settings.m_lnaGain) || force) {
		m_settings.m_lnaGain = settings.m_lnaGain;
		if(m_dev != NULL) {
			if(bladerf_set_lna_gain(m_dev, getLnaGain(m_settings.m_lnaGain)) != 0) {
				qDebug("bladerf_set_lna_gain() failed");
			} else {
				std::cerr << "BladerfInput: LNA gain set to " << getLnaGain(m_settings.m_lnaGain) << std::endl;
			}
		}
	}

	if((m_settings.m_vga1 != settings.m_vga1) || force) {
		m_settings.m_vga1 = settings.m_vga1;
		if(m_dev != NULL) {
			if(bladerf_set_rxvga1(m_dev, m_settings.m_vga1) != 0) {
				qDebug("bladerf_set_rxvga1() failed");
			} else {
				std::cerr << "BladerfInput: VGA1 gain set to " << m_settings.m_vga1 << std::endl;
			}
		}
	}

	if((m_settings.m_vga2 != settings.m_vga2) || force) {
		m_settings.m_vga2 = settings.m_vga2;
		if(m_dev != NULL) {
			if(bladerf_set_rxvga2(m_dev, m_settings.m_vga2) != 0) {
				qDebug("bladerf_set_rxvga2() failed");
			} else {
				std::cerr << "BladerfInput: VGA2 gain set to " << m_settings.m_vga2 << std::endl;
			}
		}
	}

	if((m_settings.m_xb200 != settings.m_xb200) || force) {
		m_settings.m_xb200 = settings.m_xb200;
		if(m_dev != NULL) {
			if (m_settings.m_xb200) {
				if (bladerf_expansion_attach(m_dev, BLADERF_XB_200) != 0) {
					qDebug("bladerf_expansion_attach(xb200) failed");
				} else {
					std::cerr << "BladerfInput: Attach XB200" << std::endl;
				}
			} else {
				if (bladerf_expansion_attach(m_dev, BLADERF_XB_NONE) != 0) {
					qDebug("bladerf_expansion_attach(none) failed");
				} else {
					std::cerr << "BladerfInput: Detach XB200" << std::endl;
				}
			}
		}
	}

	if((m_settings.m_xb200Path != settings.m_xb200Path) || force) {
		m_settings.m_xb200Path = settings.m_xb200Path;
		if(m_dev != NULL) {
			if(bladerf_xb200_set_path(m_dev, BLADERF_MODULE_RX, m_settings.m_xb200Path) != 0) {
				qDebug("bladerf_xb200_set_path(BLADERF_MODULE_RX) failed");
			} else {
				std::cerr << "BladerfInput: set xb200 path to " << m_settings.m_xb200Path << std::endl;
			}
		}
	}

	if((m_settings.m_xb200Filter != settings.m_xb200Filter) || force) {
		m_settings.m_xb200Filter = settings.m_xb200Filter;
		if(m_dev != NULL) {
			if(bladerf_xb200_set_filterbank(m_dev, BLADERF_MODULE_RX, m_settings.m_xb200Filter) != 0) {
				qDebug("bladerf_xb200_set_filterbank(BLADERF_MODULE_RX) failed");
			} else {
				std::cerr << "BladerfInput: set xb200 filter to " << m_settings.m_xb200Filter << std::endl;
			}
		}
	}

	if((m_settings.m_samplerate != settings.m_samplerate) || force) {
		if(m_dev != NULL) {
			unsigned int actualSamplerate;
			if( bladerf_set_sample_rate(m_dev, BLADERF_MODULE_RX, settings.m_samplerate, &actualSamplerate) < 0)
				qCritical("could not set sample rate: %d", settings.m_samplerate);
			else {
				std::cerr << "bladerf_set_sample_rate(BLADERF_MODULE_RX) actual sample rate is " << actualSamplerate << std::endl;
				m_settings.m_samplerate = settings.m_samplerate;
				m_bladerfThread->setSamplerate(settings.m_samplerate);
			}
		}
	}

	if((m_settings.m_bandwidth != settings.m_bandwidth) || force) {
		if(m_dev != NULL) {
			unsigned int actualBandwidth;
			if( bladerf_set_bandwidth(m_dev, BLADERF_MODULE_RX, settings.m_bandwidth, &actualBandwidth) < 0)
				qCritical("could not set sample rate: %d", settings.m_samplerate);
			else {
				std::cerr << "bladerf_set_bandwidth(BLADERF_MODULE_RX) actual bandwidth is " << actualBandwidth << std::endl;
				m_settings.m_bandwidth = settings.m_bandwidth;
			}
		}
	}

	if((m_settings.m_log2Decim != settings.m_log2Decim) || force) {
		if(m_dev != NULL) {
			m_settings.m_log2Decim = settings.m_log2Decim;
			m_bladerfThread->setLog2Decimation(settings.m_log2Decim);
			std::cerr << "BladerfInput: set decimation to " << (1<<settings.m_log2Decim) << std::endl;
		}
	}

	m_generalSettings.m_centerFrequency = generalSettings.m_centerFrequency;
	if(m_dev != NULL) {
		qint64 centerFrequency = m_generalSettings.m_centerFrequency + (m_settings.m_samplerate / 4);

		if (m_settings.m_log2Decim == 0) { // Little wooby-doop if no decimation
			centerFrequency = m_generalSettings.m_centerFrequency;
		} else {
			centerFrequency = m_generalSettings.m_centerFrequency + (m_settings.m_samplerate / 4);
		}

		if(bladerf_set_frequency( m_dev, BLADERF_MODULE_RX, centerFrequency ) != 0) {
			qDebug("bladerf_set_frequency(%lld) failed", m_generalSettings.m_centerFrequency);
		}
	}

	return true;
}

bladerf_lna_gain BladerfInput::getLnaGain(int lnaGain)
{
	if (lnaGain == 2) {
		return BLADERF_LNA_GAIN_MAX;
	} else if (lnaGain == 1) {
		return BLADERF_LNA_GAIN_MID;
	} else {
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
    if (serial != NULL) {
		strncpy(info.serial, serial, BLADERF_SERIAL_LENGTH - 1);
		info.serial[BLADERF_SERIAL_LENGTH - 1] = '\0';
    }

    status = bladerf_open_with_devinfo(&dev, &info);

    if (status == BLADERF_ERR_NODEV) {
        fprintf(stderr, "No devices available with serial=%s\n", serial);
        return NULL;
    } else if (status != 0) {
        fprintf(stderr, "Failed to open device with serial=%s (%s)\n",
                serial, bladerf_strerror(status));
        return NULL;
    } else {
        return dev;
    }
}
