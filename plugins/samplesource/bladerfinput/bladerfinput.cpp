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

#include "bladerfinput.h"

#include <string.h>
#include <errno.h>
#include <QDebug>

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"

#include "bladerfinputgui.h"
#include "bladerfinputthread.h"

MESSAGE_CLASS_DEFINITION(BladerfInput::MsgConfigureBladerf, Message)
MESSAGE_CLASS_DEFINITION(BladerfInput::MsgReportBladerf, Message)

BladerfInput::BladerfInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(0),
	m_bladerfThread(0),
	m_deviceDescription("BladeRFInput"),
	m_running(false)
{
    openDevice();
    m_deviceAPI->setBuddySharedPtr(&m_sharedParams);
}

BladerfInput::~BladerfInput()
{
    if (m_running) stop();
    closeDevice();
    m_deviceAPI->setBuddySharedPtr(0);
}

bool BladerfInput::openDevice()
{
    if (m_dev != 0)
    {
        closeDevice();
    }

    int res;

    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("BladerfInput::start: could not allocate SampleFifo");
        return false;
    }

    if (m_deviceAPI->getSinkBuddies().size() > 0)
    {
        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceBladeRFParams *buddySharedParams = (DeviceBladeRFParams *) buddy->getBuddySharedPtr();

        if (buddySharedParams == 0)
        {
            qCritical("BladerfInput::start: could not get shared parameters from buddy");
            return false;
        }

        if (buddy->getDeviceSinkEngine()->state() == DSPDeviceSinkEngine::StRunning) // Tx side is running so it must have device ownership
        {
            if ((m_dev = buddySharedParams->m_dev) == 0) // get device handle from Tx but do not take ownership
            {
                qCritical("BladerfInput::start: could not get BladeRF handle from buddy");
                return false;
            }
        }
        else // Tx is not running so Rx opens device and takes ownership
        {
            if (!DeviceBladeRF::open_bladerf(&m_dev, 0)) // TODO: fix; Open first available device as there is no proper handling for multiple devices
            {
                qCritical("BladerfInput::start: could not open BladeRF");
                return false;
            }

            m_sharedParams.m_dev = m_dev;
        }
    }
    else // No Tx part open so Rx opens device and takes ownership
    {
        if (!DeviceBladeRF::open_bladerf(&m_dev, 0)) // TODO: fix; Open first available device as there is no proper handling for multiple devices
        {
            qCritical("BladerfInput::start: could not open BladeRF");
            return false;
        }

        m_sharedParams.m_dev = m_dev;
    }

    // TODO: adjust USB transfer data according to sample rate
    if ((res = bladerf_sync_config(m_dev, BLADERF_MODULE_RX, BLADERF_FORMAT_SC16_Q11, 64, 8192, 32, 10000)) < 0)
    {
        qCritical("BladerfInput::start: bladerf_sync_config with return code %d", res);
        return false;
    }

    if ((res = bladerf_enable_module(m_dev, BLADERF_MODULE_RX, true)) < 0)
    {
        qCritical("BladerfInput::start: bladerf_enable_module with return code %d", res);
        return false;
    }

    return true;
}

bool BladerfInput::start(int device)
{
//	QMutexLocker mutexLocker(&m_mutex);

    if (!m_dev) {
        return false;
    }

    if (m_running) stop();

	if((m_bladerfThread = new BladerfInputThread(m_dev, &m_sampleFifo)) == 0) {
		qFatal("BladerfInput::start: out of memory");
		stop();
		return false;
	}

	m_bladerfThread->setLog2Decimation(m_settings.m_log2Decim);
	m_bladerfThread->setFcPos((int) m_settings.m_fcPos);

	m_bladerfThread->startWork();

//	mutexLocker.unlock();
	applySettings(m_settings, true);

	qDebug("BladerfInput::startInput: started");
	m_running = true;

	return true;
}

void BladerfInput::closeDevice()
{
    int res;

    if ((res = bladerf_enable_module(m_dev, BLADERF_MODULE_RX, false)) < 0)
    {
        qCritical("BladerfInput::stop: bladerf_enable_module with return code %d", res);
    }

    if (m_deviceAPI->getSinkBuddies().size() > 0)
    {
        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceBladeRFParams *buddySharedParams = (DeviceBladeRFParams *) buddy->getBuddySharedPtr();

        if (buddy->getDeviceSinkEngine()->state() == DSPDeviceSinkEngine::StRunning) // Tx side running
        {
            if ((m_sharedParams.m_dev != 0) && (buddySharedParams->m_dev == 0)) // Rx has the ownership but not the Tx
            {
                buddySharedParams->m_dev = m_dev; // transfer ownership
            }
        }
        else // Tx is not running so Rx must have the ownership
        {
            if(m_dev != 0) // close BladeRF
            {
                bladerf_close(m_dev);
            }
        }
    }
    else // No Tx part open
    {
        if(m_dev != 0) // close BladeRF
        {
            bladerf_close(m_dev);
        }
    }

    m_sharedParams.m_dev = 0;
    m_dev = 0;
}

void BladerfInput::stop()
{
//	QMutexLocker mutexLocker(&m_mutex);

	if(m_bladerfThread != 0)
	{
		m_bladerfThread->stopWork();
		delete m_bladerfThread;
		m_bladerfThread = 0;
	}

	m_running = false;
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
	if (MsgConfigureBladerf::match(message))
	{
		MsgConfigureBladerf& conf = (MsgConfigureBladerf&) message;
		qDebug() << "BladerfInput::handleMessage: MsgConfigureBladerf";

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

bool BladerfInput::applySettings(const BladeRFInputSettings& settings, bool force)
{
	bool forwardChange = false;
//	QMutexLocker mutexLocker(&m_mutex);

	qDebug() << "BladerfInput::applySettings: m_dev: " << m_dev;

	if (m_settings.m_dcBlock != settings.m_dcBlock)
	{
		m_settings.m_dcBlock = settings.m_dcBlock;
		m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
	}

	if (m_settings.m_iqCorrection != settings.m_iqCorrection)
	{
		m_settings.m_iqCorrection = settings.m_iqCorrection;
		m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
	}

	if ((m_settings.m_lnaGain != settings.m_lnaGain) || force)
	{
		m_settings.m_lnaGain = settings.m_lnaGain;

		if (m_dev != 0)
		{
			if(bladerf_set_lna_gain(m_dev, getLnaGain(m_settings.m_lnaGain)) != 0)
			{
				qDebug("BladerfInput::applySettings: bladerf_set_lna_gain() failed");
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: LNA gain set to " << getLnaGain(m_settings.m_lnaGain);
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
				qDebug("BladerfInput::applySettings: bladerf_set_rxvga1() failed");
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: VGA1 gain set to " << m_settings.m_vga1;
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
				qDebug("BladerfInput::applySettings: bladerf_set_rxvga2() failed");
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: VGA2 gain set to " << m_settings.m_vga2;
			}
		}
	}

	if ((m_settings.m_xb200 != settings.m_xb200) || force)
	{
		m_settings.m_xb200 = settings.m_xb200;

		if (m_dev != 0)
		{
		    bool changeSettings;

		    if (m_deviceAPI->getSinkBuddies().size() > 0)
		    {
		        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[0];
		        DeviceBladeRFParams *buddySharedParams = (DeviceBladeRFParams *) buddy->getBuddySharedPtr();

		        if (buddy->getDeviceSinkEngine()->state() == DSPDeviceSinkEngine::StRunning) // Tx side running
		        {
		            changeSettings = false;
		        }
		        else
		        {
		            changeSettings = true;
		        }
		    }
		    else // No Tx open
		    {
                changeSettings = true;
		    }

		    if (changeSettings)
		    {
	            if (m_settings.m_xb200)
	            {
	                if (bladerf_expansion_attach(m_dev, BLADERF_XB_200) != 0)
	                {
	                    qDebug("BladerfInput::applySettings: bladerf_expansion_attach(xb200) failed");
	                }
	                else
	                {
	                    qDebug() << "BladerfInput::applySettings: Attach XB200";
	                }
	            }
	            else
	            {
	                if (bladerf_expansion_attach(m_dev, BLADERF_XB_NONE) != 0)
	                {
	                    qDebug("BladerfInput::applySettings: bladerf_expansion_attach(none) failed");
	                }
	                else
	                {
	                    qDebug() << "BladerfInput::applySettings: Detach XB200";
	                }
	            }

	            m_sharedParams.m_xb200Attached = m_settings.m_xb200;
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
				qDebug("BladerfInput::applySettings: bladerf_xb200_set_path(BLADERF_MODULE_RX) failed");
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: set xb200 path to " << m_settings.m_xb200Path;
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
				qDebug("BladerfInput::applySettings: bladerf_xb200_set_filterbank(BLADERF_MODULE_RX) failed");
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: set xb200 filter to " << m_settings.m_xb200Filter;
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
				qCritical("BladerfInput::applySettings: could not set sample rate: %d", m_settings.m_devSampleRate);
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: bladerf_set_sample_rate(BLADERF_MODULE_RX) actual sample rate is " << actualSamplerate;
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
				qCritical("BladerfInput::applySettings: could not set bandwidth: %d", m_settings.m_bandwidth);
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: bladerf_set_bandwidth(BLADERF_MODULE_RX) actual bandwidth is " << actualBandwidth;
			}
		}
	}

	if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
	{
		m_settings.m_log2Decim = settings.m_log2Decim;
		forwardChange = true;

		if (m_bladerfThread != 0)
		{
			m_bladerfThread->setLog2Decimation(m_settings.m_log2Decim);
			qDebug() << "BladerfInput::applySettings: set decimation to " << (1<<m_settings.m_log2Decim);
		}
	}

	if ((m_settings.m_fcPos != settings.m_fcPos) || force)
	{
		m_settings.m_fcPos = settings.m_fcPos;

		if (m_bladerfThread != 0)
		{
			m_bladerfThread->setFcPos((int) m_settings.m_fcPos);
			qDebug() << "BladerfInput::applySettings: set fc pos (enum) to " << (int) m_settings.m_fcPos;
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

	if ((m_settings.m_log2Decim == 0) || (m_settings.m_fcPos == BladeRFInputSettings::FC_POS_CENTER))
	{
		deviceCenterFrequency = m_settings.m_centerFrequency;
		f_img = deviceCenterFrequency;
		f_cut = deviceCenterFrequency + m_settings.m_bandwidth/2;
	}
	else
	{
		if (m_settings.m_fcPos == BladeRFInputSettings::FC_POS_INFRA)
		{
			deviceCenterFrequency = m_settings.m_centerFrequency + (m_settings.m_devSampleRate / 4);
			f_img = deviceCenterFrequency + m_settings.m_devSampleRate/2;
			f_cut = deviceCenterFrequency + m_settings.m_bandwidth/2;
		}
		else if (m_settings.m_fcPos == BladeRFInputSettings::FC_POS_SUPRA)
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
			qDebug("BladerfInput::applySettings: bladerf_set_frequency(%lld) failed", m_settings.m_centerFrequency);
		}
	}

	if (forwardChange)
	{
		int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
		m_deviceAPI->getDeviceInputMessageQueue()->push(notif);
	}

	qDebug() << "BladerfInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
			<< " device center freq: " << deviceCenterFrequency << " Hz"
			<< " device sample rate: " << m_settings.m_devSampleRate << "S/s"
			<< " Actual sample rate: " << m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim) << "S/s"
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

//struct bladerf *BladerfInput::open_bladerf_from_serial(const char *serial)
//{
//    int status;
//    struct bladerf *dev;
//    struct bladerf_devinfo info;
//
//    /* Initialize all fields to "don't care" wildcard values.
//     *
//     * Immediately passing this to bladerf_open_with_devinfo() would cause
//     * libbladeRF to open any device on any available backend. */
//    bladerf_init_devinfo(&info);
//
//    /* Specify the desired device's serial number, while leaving all other
//     * fields in the info structure wildcard values */
//    if (serial != NULL)
//    {
//		strncpy(info.serial, serial, BLADERF_SERIAL_LENGTH - 1);
//		info.serial[BLADERF_SERIAL_LENGTH - 1] = '\0';
//    }
//
//    status = bladerf_open_with_devinfo(&dev, &info);
//
//    if (status == BLADERF_ERR_NODEV)
//    {
//        fprintf(stderr, "No devices available with serial=%s\n", serial);
//        return NULL;
//    }
//    else if (status != 0)
//    {
//        fprintf(stderr, "Failed to open device with serial=%s (%s)\n",
//                serial, bladerf_strerror(status));
//        return NULL;
//    }
//    else
//    {
//        return dev;
//    }
//}
