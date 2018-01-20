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

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"

#include "bladerfinputgui.h"
#include "bladerfinputthread.h"

MESSAGE_CLASS_DEFINITION(BladerfInput::MsgConfigureBladerf, Message)
MESSAGE_CLASS_DEFINITION(BladerfInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(BladerfInput::MsgFileRecord, Message)

BladerfInput::BladerfInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(0),
	m_bladerfThread(0),
	m_deviceDescription("BladeRFInput"),
	m_running(false)
{
    openDevice();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    m_deviceAPI->setBuddySharedPtr(&m_sharedParams);
}

BladerfInput::~BladerfInput()
{
    if (m_running) stop();
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
    m_deviceAPI->setBuddySharedPtr(0);
}

void BladerfInput::destroy()
{
    delete this;
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
        qCritical("BladerfInput::openDevice: could not allocate SampleFifo");
        return false;
    }

    if (m_deviceAPI->getSinkBuddies().size() > 0)
    {
        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceBladeRFParams *buddySharedParams = (DeviceBladeRFParams *) sinkBuddy->getBuddySharedPtr();

        if (buddySharedParams == 0)
        {
            qCritical("BladerfInput::openDevice: could not get shared parameters from buddy");
            return false;
        }

        if (buddySharedParams->m_dev == 0) // device is not opened by buddy
        {
            qCritical("BladerfInput::openDevice: could not get BladeRF handle from buddy");
            return false;
        }

        m_sharedParams = *(buddySharedParams); // copy parameters from buddy
        m_dev = m_sharedParams.m_dev;          // get BladeRF handle
    }
    else
    {
        if (!DeviceBladeRF::open_bladerf(&m_dev, qPrintable(m_deviceAPI->getSampleSourceSerial())))
        {
            qCritical("BladerfInput::start: could not open BladeRF %s", qPrintable(m_deviceAPI->getSampleSourceSerial()));
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

void BladerfInput::init()
{
    applySettings(m_settings, true);
}

bool BladerfInput::start()
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

    if (m_dev == 0) { // was never open
        return;
    }

    if ((res = bladerf_enable_module(m_dev, BLADERF_MODULE_RX, false)) < 0)
    {
        qCritical("BladerfInput::stop: bladerf_enable_module with return code %d", res);
    }

    if (m_deviceAPI->getSinkBuddies().size() == 0)
    {
        qDebug("BladerfInput::closeDevice: closing device since Tx side is not open");

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

QByteArray BladerfInput::serialize() const
{
    return m_settings.serialize();
}

bool BladerfInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureBladerf* message = MsgConfigureBladerf::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladerf* messageToGUI = MsgConfigureBladerf::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
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

void BladerfInput::setCenterFrequency(qint64 centerFrequency)
{
    BladeRFInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureBladerf* message = MsgConfigureBladerf::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureBladerf* messageToGUI = MsgConfigureBladerf::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool BladerfInput::handleMessage(const Message& message)
{
	if (MsgConfigureBladerf::match(message))
	{
		MsgConfigureBladerf& conf = (MsgConfigureBladerf&) message;
		qDebug() << "BladerfInput::handleMessage: MsgConfigureBladerf";

		if (!applySettings(conf.getSettings(), conf.getForce()))
		{
			qDebug("BladeRF config error");
		}

		return true;
	}
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "BladerfInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop()) {
            m_fileSink->startRecording();
        } else {
            m_fileSink->stopRecording();
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "BladerfInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initAcquisition())
            {
                m_deviceAPI->startAcquisition();
                DSPEngine::instance()->startAudioOutput();
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
            DSPEngine::instance()->stopAudioOutput();
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

	if ((m_settings.m_dcBlock != settings.m_dcBlock) ||
	    (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
	{
		m_settings.m_dcBlock = settings.m_dcBlock;
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

	if ((m_settings.m_fcPos != settings.m_fcPos) || force)
	{
		if (m_bladerfThread != 0)
		{
			m_bladerfThread->setFcPos((int) settings.m_fcPos);
			qDebug() << "BladerfInput::applySettings: set fc pos (enum) to " << (int) settings.m_fcPos;
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

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_fcPos != settings.m_fcPos)
        || (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;
        m_settings.m_log2Decim = settings.m_log2Decim;
        m_settings.m_fcPos = settings.m_fcPos;

        qint64 deviceCenterFrequency = m_settings.m_centerFrequency;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
        qint64 f_img = deviceCenterFrequency;
        qint64 f_cut = deviceCenterFrequency + m_settings.m_bandwidth/2;
        quint32 devSampleRate = m_settings.m_devSampleRate;

        forwardChange = true;

        if ((m_settings.m_log2Decim == 0) || (settings.m_fcPos == BladeRFInputSettings::FC_POS_CENTER))
        {
            f_img = deviceCenterFrequency;
        }
        else
        {
            if (settings.m_fcPos == BladeRFInputSettings::FC_POS_INFRA)
            {
                deviceCenterFrequency += (devSampleRate / 4);
                f_img = deviceCenterFrequency + devSampleRate/2;
            }
            else if (settings.m_fcPos == BladeRFInputSettings::FC_POS_SUPRA)
            {
                deviceCenterFrequency -= (devSampleRate / 4);
                f_img = deviceCenterFrequency - devSampleRate/2;
            }
        }

        if (m_dev != 0)
        {
            if (bladerf_set_frequency( m_dev, BLADERF_MODULE_RX, deviceCenterFrequency ) != 0)
            {
                qDebug("BladerfInput::applySettings: bladerf_set_frequency(%lld) failed", m_settings.m_centerFrequency);
            }
            else
            {
                qDebug() << "BladerfInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
                        << " device center freq: " << deviceCenterFrequency << " Hz"
                        << " device sample rate: " << m_settings.m_devSampleRate << "S/s"
                        << " Actual sample rate: " << m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim) << "S/s"
                        << " BW: " << m_settings.m_bandwidth << "Hz"
                        << " img: " << f_img << "Hz"
                        << " cut: " << f_cut << "Hz"
                        << " img - cut: " << f_img - f_cut;
            }
        }
    }

	if (forwardChange)
	{
		int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

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

int BladerfInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int BladerfInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
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
