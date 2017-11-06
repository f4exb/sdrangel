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

#include "hackrfinput.h"

#include <string.h>
#include <errno.h>
#include <QDebug>

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "hackrf/devicehackrfvalues.h"
#include "hackrf/devicehackrfshared.h"

#include "hackrfinputgui.h"
#include "hackrfinputthread.h"

MESSAGE_CLASS_DEFINITION(HackRFInput::MsgConfigureHackRF, Message)
MESSAGE_CLASS_DEFINITION(HackRFInput::MsgReportHackRF, Message)
MESSAGE_CLASS_DEFINITION(HackRFInput::MsgFileRecord, Message)

HackRFInput::HackRFInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(0),
	m_hackRFThread(0),
	m_deviceDescription("HackRF"),
	m_running(false)
{
    openDevice();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    m_deviceAPI->setBuddySharedPtr(&m_sharedParams);
}

HackRFInput::~HackRFInput()
{
    if (m_running) stop();
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
	m_deviceAPI->setBuddySharedPtr(0);
}

void HackRFInput::destroy()
{
    delete this;
}

bool HackRFInput::openDevice()
{
    if (m_dev != 0)
    {
        closeDevice();
    }

    if (!m_sampleFifo.setSize(1<<19))
    {
        qCritical("HackRFInput::start: could not allocate SampleFifo");
        return false;
    }

    if (m_deviceAPI->getSinkBuddies().size() > 0)
    {
        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceHackRFParams *buddySharedParams = (DeviceHackRFParams *) buddy->getBuddySharedPtr();

        if (buddySharedParams == 0)
        {
            qCritical("HackRFInput::openDevice: could not get shared parameters from buddy");
            return false;
        }

        if (buddySharedParams->m_dev == 0) // device is not opened by buddy
        {
            qCritical("HackRFInput::openDevice: could not get HackRF handle from buddy");
            return false;
        }

        m_sharedParams = *(buddySharedParams); // copy parameters from buddy
        m_dev = m_sharedParams.m_dev;          // get HackRF handle
    }
    else
    {
        if ((m_dev = DeviceHackRF::open_hackrf(qPrintable(m_deviceAPI->getSampleSourceSerial()))) == 0)
        {
            qCritical("HackRFInput::openDevice: could not open HackRF %s", qPrintable(m_deviceAPI->getSampleSourceSerial()));
            return false;
        }

        m_sharedParams.m_dev = m_dev;
    }

    return true;
}

bool HackRFInput::start()
{
//	QMutexLocker mutexLocker(&m_mutex);
    if (!m_dev) {
        return false;
    }

    if (m_running) stop();

	if ((m_hackRFThread = new HackRFInputThread(m_dev, &m_sampleFifo)) == 0)
	{
		qFatal("HackRFInput::start: out of memory");
		stop();
		return false;
	}

//	mutexLocker.unlock();

	applySettings(m_settings, true);

	m_hackRFThread->setSamplerate(m_settings.m_devSampleRate);
	m_hackRFThread->setLog2Decimation(m_settings.m_log2Decim);
	m_hackRFThread->setFcPos((int) m_settings.m_fcPos);

	m_hackRFThread->startWork();

	qDebug("HackRFInput::startInput: started");
	m_running = true;

	return true;
}

void HackRFInput::closeDevice()
{
    if (m_deviceAPI->getSinkBuddies().size() == 0)
    {
        qDebug("HackRFInput::closeDevice: closing device since Tx side is not open");

        if(m_dev != 0) // close BladeRF
        {
            hackrf_close(m_dev);
            //hackrf_exit(); // TODO: this may not work if several HackRF Devices are running concurrently. It should be handled globally in the application
        }
    }

    m_sharedParams.m_dev = 0;
    m_dev = 0;
}

void HackRFInput::stop()
{
	qDebug("HackRFInput::stop");
//	QMutexLocker mutexLocker(&m_mutex);

	if (m_hackRFThread != 0)
	{
		m_hackRFThread->stopWork();
		delete m_hackRFThread;
		m_hackRFThread = 0;
	}

	m_running = false;
}

const QString& HackRFInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int HackRFInput::getSampleRate() const
{
	return (m_settings.m_devSampleRate / (1<<m_settings.m_log2Decim));
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

		bool success = applySettings(conf.getSettings(), conf.getForce());

		if (!success)
		{
			qDebug("HackRFInput::handleMessage: config error");
		}

		return true;
	}
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "HackRFInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop()) {
            m_fileSink->startRecording();
        } else {
            m_fileSink->stopRecording();
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
	qint64 df = ((qint64)freq_hz * m_settings.m_LOppmTenths) / 10000000LL;
	freq_hz += df;

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

bool HackRFInput::applySettings(const HackRFInputSettings& settings, bool force)
{
//	QMutexLocker mutexLocker(&m_mutex);

	bool forwardChange = false;
	hackrf_error rc;

	qDebug() << "HackRFInput::applySettings";

	if ((m_settings.m_dcBlock != settings.m_dcBlock) ||
	    (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
	{
		m_settings.m_dcBlock = settings.m_dcBlock;
		m_settings.m_iqCorrection = settings.m_iqCorrection;
		m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
	}

	if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
	{
        m_settings.m_devSampleRate = settings.m_devSampleRate;
        forwardChange = true;

        if (m_dev != 0)
		{
	        rc = (hackrf_error) hackrf_set_sample_rate_manual(m_dev, m_settings.m_devSampleRate, 1);

			if (rc != HACKRF_SUCCESS)
			{
				qCritical("HackRFInput::applySettings: could not set sample rate TO %llu S/s: %s", m_settings.m_devSampleRate, hackrf_error_name(rc));
			}
			else
			{
			    if (m_hackRFThread != 0)
			    {
	                qDebug("HackRFInput::applySettings: sample rate set to %llu S/s", m_settings.m_devSampleRate);
	                m_hackRFThread->setSamplerate(m_settings.m_devSampleRate);
			    }
			}
		}
	}

	if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
	{
		m_settings.m_log2Decim = settings.m_log2Decim;
		forwardChange = true;

		if (m_hackRFThread != 0)
		{
			m_hackRFThread->setLog2Decimation(m_settings.m_log2Decim);
			qDebug() << "HackRFInput: set decimation to " << (1<<m_settings.m_log2Decim);
		}
	}

	qint64 deviceCenterFrequency = m_settings.m_centerFrequency;
	qint64 f_img = deviceCenterFrequency;
	quint32 devSampleRate =m_settings.m_devSampleRate;

	if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency)) // forward delta to buddy if necessary
	{
	    if (m_settings.m_linkTxFrequency && (m_deviceAPI->getSinkBuddies().size() > 0))
	    {
	        DeviceSinkAPI *buddy = m_deviceAPI->getSinkBuddies()[0];
            DeviceHackRFShared::MsgConfigureFrequencyDelta *deltaMsg = DeviceHackRFShared::MsgConfigureFrequencyDelta::create(
                    settings.m_centerFrequency - m_settings.m_centerFrequency);

	        if (buddy->getSampleSinkGUIMessageQueue())
	        {
	            DeviceHackRFShared::MsgConfigureFrequencyDelta *deltaMsgToGUI = new DeviceHackRFShared::MsgConfigureFrequencyDelta(*deltaMsg);
                buddy->getSampleSinkGUIMessageQueue()->push(deltaMsgToGUI);
	        }

	        buddy->getSampleSinkInputMessageQueue()->push(deltaMsg);
	    }
	}

	if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency) ||
			(m_settings.m_LOppmTenths != settings.m_LOppmTenths) ||
			(m_settings.m_fcPos != settings.m_fcPos))
	{
		m_settings.m_centerFrequency = settings.m_centerFrequency;
		m_settings.m_LOppmTenths = settings.m_LOppmTenths;

		if ((m_settings.m_log2Decim == 0) || (settings.m_fcPos == HackRFInputSettings::FC_POS_CENTER))
		{
			deviceCenterFrequency = m_settings.m_centerFrequency;
			f_img = deviceCenterFrequency;
		}
		else
		{
			if (settings.m_fcPos == HackRFInputSettings::FC_POS_INFRA)
			{
				deviceCenterFrequency = m_settings.m_centerFrequency + (devSampleRate / 4);
				f_img = deviceCenterFrequency + devSampleRate/2;
			}
			else if (settings.m_fcPos == HackRFInputSettings::FC_POS_SUPRA)
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

	if ((m_settings.m_fcPos != settings.m_fcPos) || force)
	{
		m_settings.m_fcPos = settings.m_fcPos;

		if (m_hackRFThread != 0)
		{
			m_hackRFThread->setFcPos((int) m_settings.m_fcPos);
			qDebug() << "HackRFInput: set fc pos (enum) to " << (int) m_settings.m_fcPos;
		}
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

	if ((m_settings.m_bandwidth != settings.m_bandwidth) || force)
	{
        m_settings.m_bandwidth = settings.m_bandwidth;

        if (m_dev != 0)
		{
	        uint32_t bw_index = hackrf_compute_baseband_filter_bw_round_down_lt(m_settings.m_bandwidth + 1); // +1 so the round down to lower than yields desired bandwidth
			rc = (hackrf_error) hackrf_set_baseband_filter_bandwidth(m_dev, bw_index);

			if (rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: hackrf_set_baseband_filter_bandwidth failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: Baseband BW filter set to " << m_settings.m_bandwidth << " Hz";
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
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

	m_settings.m_linkTxFrequency = settings.m_linkTxFrequency;

    qDebug() << "HackRFInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
            << " device center freq: " << deviceCenterFrequency << " Hz"
            << " device sample rate: " << m_settings.m_devSampleRate << "S/s"
            << " Actual sample rate: " << m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim) << "S/s";

	return true;
}

//hackrf_device *HackRFInput::open_hackrf_from_sequence(int sequence)
//{
//	hackrf_device_list_t *hackrf_devices = hackrf_device_list();
//	hackrf_device *hackrf_ptr;
//	hackrf_error rc;
//
//	rc = (hackrf_error) hackrf_device_list_open(hackrf_devices, sequence, &hackrf_ptr);
//
//	if (rc == HACKRF_SUCCESS)
//	{
//		return hackrf_ptr;
//	}
//	else
//	{
//		return 0;
//	}
//}
