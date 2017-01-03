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
#include "dsp/dspengine.h"
#include "device/devicesinkapi.h"
#include "device/devicesourceapi.h"

#include "bladerfoutput.h"
#include "bladerfoutputgui.h"
#include "bladerfoutputthread.h"

MESSAGE_CLASS_DEFINITION(BladerfOutput::MsgConfigureBladerf, Message)
MESSAGE_CLASS_DEFINITION(BladerfOutput::MsgReportBladerf, Message)

BladerfOutput::BladerfOutput(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(0),
	m_bladerfThread(0),
	m_deviceDescription("BladeRFOutput")
{
    m_deviceAPI->setBuddySharedPtr(&m_sharedParams);
}

BladerfOutput::~BladerfOutput()
{
    if (m_dev != 0)
    {
        stop();
    }

    m_deviceAPI->setBuddySharedPtr(0);
}

bool BladerfOutput::init(const Message& cmd)
{
	return false;
}

bool BladerfOutput::start(int device)
{
//	QMutexLocker mutexLocker(&m_mutex);

	if (m_dev != 0)
	{
		stop();
	}

	int res;

	m_sampleSourceFifo.resize(m_settings.m_devSampleRate); // 1s long

    if (m_deviceAPI->getSourceBuddies().size() > 0)
    {
        DeviceSourceAPI *buddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceBladeRFParams *buddySharedParams = (DeviceBladeRFParams *) buddy->getBuddySharedPtr();

        if (buddySharedParams == 0)
        {
            qCritical("BladerfOutput::start: could not get shared parameters from buddy");
            return false;
        }

        if (buddy->getDeviceSourceEngine()->state() == DSPDeviceSourceEngine::StRunning) // Rx side is running so it must have device ownership
        {
            qDebug("BladerfOutput::start: Rx side is running");

            if ((m_dev = buddySharedParams->m_dev) == 0) // get device handle from Rx but do not take ownership
            {
                qCritical("BladerfOutput::start: could not get BladeRF handle from buddy");
                return false;
            }
        }
        else // Rx is not running so Tx opens device and takes ownership
        {
            qDebug("BladerfOutput::start: Rx side is not running");

            if (!DeviceBladeRF::open_bladerf(&m_dev, 0)) // TODO: fix; Open first available device as there is no proper handling for multiple devices
            {
                qCritical("BladerfOutput::start: could not open BladeRF");
                return false;
            }

            m_sharedParams.m_dev = m_dev;
        }
    }
    else // No Rx part open so Tx opens device and takes ownership
    {
        qDebug("BladerfOutput::start: Rx side is not open");

        if (!DeviceBladeRF::open_bladerf(&m_dev, 0)) // TODO: fix; Open first available device as there is no proper handling for multiple devices
        {
            qCritical("BladerfOutput::start: could not open BladeRF");
            return false;
        }

        m_sharedParams.m_dev = m_dev;
    }

    // TODO: adjust USB transfer data according to sample rate
    if ((res = bladerf_sync_config(m_dev, BLADERF_MODULE_TX, BLADERF_FORMAT_SC16_Q11, 64, 8192, 32, 10000)) < 0)
    {
    	qCritical("BladerfOutput::start: bladerf_sync_config with return code %d", res);
    	goto failed;
    }

    if ((res = bladerf_enable_module(m_dev, BLADERF_MODULE_TX, true)) < 0)
    {
    	qCritical("BladerfOutput::start: bladerf_enable_module with return code %d", res);
    	goto failed;
    }

	if((m_bladerfThread = new BladerfOutputThread(m_dev, &m_sampleSourceFifo)) == 0)
	{
		qFatal("BladerfOutput::start: out of memory");
		goto failed;
	}

	m_bladerfThread->startWork();

//	mutexLocker.unlock();
	applySettings(m_settings, true);

	qDebug("BladerfOutput::start: started");

	return true;

failed:
	stop();
	return false;
}

void BladerfOutput::stop()
{
//	QMutexLocker mutexLocker(&m_mutex);
    int res;

    if(m_bladerfThread != 0)
	{
		m_bladerfThread->stopWork();
		delete m_bladerfThread;
		m_bladerfThread = 0;
	}

    if ((res = bladerf_enable_module(m_dev, BLADERF_MODULE_TX, false)) < 0)
    {
        qCritical("BladerfOutput::stop: bladerf_enable_module with return code %d", res);
    }

    if (m_deviceAPI->getSourceBuddies().size() > 0)
    {
        DeviceSourceAPI *buddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceBladeRFParams *buddySharedParams = (DeviceBladeRFParams *) buddy->getBuddySharedPtr();

        if (buddy->getDeviceSourceEngine()->state() == DSPDeviceSourceEngine::StRunning) // Rx side running
        {
            qDebug("BladerfOutput::stop: Rx side is running");

            if ((m_sharedParams.m_dev != 0) && (buddySharedParams->m_dev == 0)) // Tx has the ownership but not the Rx
            {
                buddySharedParams->m_dev = m_dev; // transfer ownership
            }
        }
        else // Rx is not running so Tx must have the ownership
        {
            qDebug("BladerfOutput::stop: Rx side is not running");

            if(m_dev != 0) // close BladeRF
            {
                bladerf_close(m_dev);
            }
        }
    }
    else // No Rx part open
    {
        qDebug("BladerfOutput::stop: Rx side is not open");

        if(m_dev != 0) // close BladeRF
        {
            bladerf_close(m_dev);
        }
    }

    m_sharedParams.m_dev = 0;
    m_dev = 0;
}

const QString& BladerfOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int BladerfOutput::getSampleRate() const
{
	int rate = m_settings.m_devSampleRate;
	return (rate / (1<<m_settings.m_log2Interp));
}

quint64 BladerfOutput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

bool BladerfOutput::handleMessage(const Message& message)
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

bool BladerfOutput::applySettings(const BladeRFOutputSettings& settings, bool force)
{
	bool forwardChange = false;
//	QMutexLocker mutexLocker(&m_mutex);

	qDebug() << "BladerfOutput::applySettings: m_dev: " << m_dev;

	if ((m_settings.m_vga1 != settings.m_vga1) || force)
	{
		m_settings.m_vga1 = settings.m_vga1;

		if (m_dev != 0)
		{
			if(bladerf_set_txvga1(m_dev, m_settings.m_vga1) != 0)
			{
				qDebug("BladerfOutput::applySettings: bladerf_set_txvga1() failed");
			}
			else
			{
				qDebug() << "BladerfOutput::applySettings: VGA1 gain set to " << m_settings.m_vga1;
			}
		}
	}

	if ((m_settings.m_vga2 != settings.m_vga2) || force)
	{
		m_settings.m_vga2 = settings.m_vga2;

		if(m_dev != 0)
		{
			if(bladerf_set_txvga2(m_dev, m_settings.m_vga2) != 0)
			{
				qDebug("BladerfOutput::applySettings:bladerf_set_rxvga2() failed");
			}
			else
			{
				qDebug() << "BladerfOutput::applySettings: VGA2 gain set to " << m_settings.m_vga2;
			}
		}
	}

	if ((m_settings.m_xb200 != settings.m_xb200) || force)
	{
		m_settings.m_xb200 = settings.m_xb200;

		if (m_dev != 0)
		{
            bool changeSettings;

            if (m_deviceAPI->getSourceBuddies().size() > 0)
            {
                DeviceSourceAPI *buddy = m_deviceAPI->getSourceBuddies()[0];
                DeviceBladeRFParams *buddySharedParams = (DeviceBladeRFParams *) buddy->getBuddySharedPtr();

                if (buddy->getDeviceSourceEngine()->state() == DSPDeviceSourceEngine::StRunning) // Tx side running
                {
                    changeSettings = false;
                }
                else
                {
                    changeSettings = true;
                }
            }
            else // No Rx open
            {
                changeSettings = true;
            }

            if (changeSettings)
            {
                if (m_settings.m_xb200)
                {
                    if (bladerf_expansion_attach(m_dev, BLADERF_XB_200) != 0)
                    {
                        qDebug("BladerfOutput::applySettings: bladerf_expansion_attach(xb200) failed");
                    }
                    else
                    {
                        qDebug() << "BladerfOutput::applySettings: Attach XB200";
                    }
                }
                else
                {
                    if (bladerf_expansion_attach(m_dev, BLADERF_XB_NONE) != 0)
                    {
                        qDebug("BladerfOutput::applySettings: bladerf_expansion_attach(none) failed");
                    }
                    else
                    {
                        qDebug() << "BladerfOutput::applySettings: Detach XB200";
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
			if(bladerf_xb200_set_path(m_dev, BLADERF_MODULE_TX, m_settings.m_xb200Path) != 0)
			{
				qDebug("BladerfOutput::applySettings: bladerf_xb200_set_path(BLADERF_MODULE_TX) failed");
			}
			else
			{
				qDebug() << "BladerfOutput::applySettings: set xb200 path to " << m_settings.m_xb200Path;
			}
		}
	}

	if ((m_settings.m_xb200Filter != settings.m_xb200Filter) || force)
	{
		m_settings.m_xb200Filter = settings.m_xb200Filter;

		if (m_dev != 0)
		{
			if(bladerf_xb200_set_filterbank(m_dev, BLADERF_MODULE_TX, m_settings.m_xb200Filter) != 0)
			{
				qDebug("BladerfOutput::applySettings: bladerf_xb200_set_filterbank(BLADERF_MODULE_TX) failed");
			}
			else
			{
				qDebug() << "BladerfOutput::applySettings: set xb200 filter to " << m_settings.m_xb200Filter;
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

			if (bladerf_set_sample_rate(m_dev, BLADERF_MODULE_TX, m_settings.m_devSampleRate, &actualSamplerate) < 0)
			{
				qCritical("BladerfOutput::applySettings: could not set sample rate: %d", m_settings.m_devSampleRate);
			}
			else
			{
				qDebug() << "BladerfOutput::applySettings: bladerf_set_sample_rate(BLADERF_MODULE_TX) actual sample rate is " << actualSamplerate;
			}
		}
	}

	if ((m_settings.m_bandwidth != settings.m_bandwidth) || force)
	{
		m_settings.m_bandwidth = settings.m_bandwidth;

		if(m_dev != 0)
		{
			unsigned int actualBandwidth;

			if( bladerf_set_bandwidth(m_dev, BLADERF_MODULE_TX, m_settings.m_bandwidth, &actualBandwidth) < 0)
			{
				qCritical("BladerfOutput::applySettings: could not set bandwidth: %d", m_settings.m_bandwidth);
			}
			else
			{
				qDebug() << "BladerfOutput::applySettings: bladerf_set_bandwidth(BLADERF_MODULE_TX) actual bandwidth is " << actualBandwidth;
			}
		}
	}

	if ((m_settings.m_log2Interp != settings.m_log2Interp) || force)
	{
		m_settings.m_log2Interp = settings.m_log2Interp;
		forwardChange = true;

		if(m_dev != 0)
		{
			m_bladerfThread->setLog2Interpolation(m_settings.m_log2Interp);
			qDebug() << "BladerfOutput::applySettings: set interpolation to " << (1<<m_settings.m_log2Interp);
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

    deviceCenterFrequency = m_settings.m_centerFrequency;
    f_img = deviceCenterFrequency;
    f_cut = deviceCenterFrequency + m_settings.m_bandwidth/2;

	if (m_dev != NULL)
	{
		if (bladerf_set_frequency( m_dev, BLADERF_MODULE_TX, deviceCenterFrequency ) != 0)
		{
			qDebug("BladerfOutput::applySettings: bladerf_set_frequency(%lld) failed", m_settings.m_centerFrequency);
		}
	}

	if (forwardChange)
	{
		int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
		m_deviceAPI->getDeviceInputMessageQueue()->push(notif);
	}

	qDebug() << "BladerfOutput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
			<< " device center freq: " << deviceCenterFrequency << " Hz"
			<< " device sample rate: " << m_settings.m_devSampleRate << "Hz"
			<< " baseband sample rate: " << m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp) << "Hz"
			<< " BW: " << m_settings.m_bandwidth << "Hz"
			<< " img: " << f_img << "Hz"
			<< " cut: " << f_cut << "Hz"
			<< " img - cut: " << f_img - f_cut;

	return true;
}
