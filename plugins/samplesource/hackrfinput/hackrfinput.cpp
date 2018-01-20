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

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "hackrf/devicehackrfvalues.h"
#include "hackrf/devicehackrfshared.h"

#include "hackrfinputthread.h"

MESSAGE_CLASS_DEFINITION(HackRFInput::MsgConfigureHackRF, Message)
MESSAGE_CLASS_DEFINITION(HackRFInput::MsgReportHackRF, Message)
MESSAGE_CLASS_DEFINITION(HackRFInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(HackRFInput::MsgStartStop, Message)

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

void HackRFInput::init()
{
    applySettings(m_settings, true);
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

QByteArray HackRFInput::serialize() const
{
    return m_settings.serialize();
}

bool HackRFInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureHackRF* message = MsgConfigureHackRF::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureHackRF* messageToGUI = MsgConfigureHackRF::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
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

void HackRFInput::setCenterFrequency(qint64 centerFrequency)
{
    HackRFInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureHackRF* message = MsgConfigureHackRF::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureHackRF* messageToGUI = MsgConfigureHackRF::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
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
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "HackRFInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

void HackRFInput::setDeviceCenterFrequency(quint64 freq_hz)
{
	qint64 df = ((qint64)freq_hz * m_settings.m_LOppmTenths) / 10000000LL;
	freq_hz += df;

	hackrf_error rc = (hackrf_error) hackrf_set_freq(m_dev, static_cast<uint64_t>(freq_hz));

	if (rc != HACKRF_SUCCESS)
	{
		qWarning("HackRFInput::setDeviceCenterFrequency: could not frequency to %llu Hz", freq_hz);
	}
	else
	{
		qWarning("HackRFInput::setDeviceCenterFrequency: frequency set to %llu Hz", freq_hz);
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
		m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
	}

	if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
	{
        forwardChange = true;

        if (m_dev != 0)
		{
	        rc = (hackrf_error) hackrf_set_sample_rate_manual(m_dev, settings.m_devSampleRate, 1);

			if (rc != HACKRF_SUCCESS)
			{
				qCritical("HackRFInput::applySettings: could not set sample rate TO %llu S/s: %s", settings.m_devSampleRate, hackrf_error_name(rc));
			}
			else
			{
			    if (m_hackRFThread != 0)
			    {
	                qDebug("HackRFInput::applySettings: sample rate set to %llu S/s", settings.m_devSampleRate);
	                m_hackRFThread->setSamplerate(settings.m_devSampleRate);
			    }
			}
		}
	}

	if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
	{
		forwardChange = true;

		if (m_hackRFThread != 0)
		{
			m_hackRFThread->setLog2Decimation(settings.m_log2Decim);
			qDebug() << "HackRFInput: set decimation to " << (1<<settings.m_log2Decim);
		}
	}

	qint64 deviceCenterFrequency = settings.m_centerFrequency;
	qint64 f_img = deviceCenterFrequency;
	quint32 devSampleRate = settings.m_devSampleRate;

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

	if ((m_settings.m_centerFrequency != settings.m_centerFrequency) ||
        (m_settings.m_LOppmTenths != settings.m_LOppmTenths) ||
        (m_settings.m_log2Decim != settings.m_log2Decim) ||
        (m_settings.m_fcPos != settings.m_fcPos) || force)
	{
		if ((settings.m_log2Decim == 0) || (settings.m_fcPos == HackRFInputSettings::FC_POS_CENTER))
		{
			deviceCenterFrequency = settings.m_centerFrequency;
			f_img = deviceCenterFrequency;
		}
		else
		{
			if (settings.m_fcPos == HackRFInputSettings::FC_POS_INFRA)
			{
				deviceCenterFrequency = settings.m_centerFrequency + (devSampleRate / 4);
				f_img = deviceCenterFrequency + devSampleRate/2;
			}
			else if (settings.m_fcPos == HackRFInputSettings::FC_POS_SUPRA)
			{
				deviceCenterFrequency = settings.m_centerFrequency - (devSampleRate / 4);
				f_img = deviceCenterFrequency - devSampleRate/2;
			}
		}

		if (m_dev != 0)
		{
			setDeviceCenterFrequency(deviceCenterFrequency);

			qDebug() << "HackRFInput::applySettings: center freq: " << settings.m_centerFrequency << " Hz"
					<< " device center freq: " << deviceCenterFrequency << " Hz"
					<< " device sample rate: " << devSampleRate << "Hz"
					<< " Actual sample rate: " << devSampleRate/(1<<settings.m_log2Decim) << "Hz"
					<< " img: " << f_img << "Hz";
		}

		forwardChange = true;
	}

	if ((m_settings.m_fcPos != settings.m_fcPos) || force)
	{
		if (m_hackRFThread != 0)
		{
			m_hackRFThread->setFcPos((int) settings.m_fcPos);
			qDebug() << "HackRFInput: set fc pos (enum) to " << (int) settings.m_fcPos;
		}
	}

	if ((m_settings.m_lnaGain != settings.m_lnaGain) || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_lna_gain(m_dev, settings.m_lnaGain);

			if(rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: airspy_set_lna_gain failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: LNA gain set to " << settings.m_lnaGain;
			}
		}
	}

	if ((m_settings.m_vgaGain != settings.m_vgaGain) || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_vga_gain(m_dev, settings.m_vgaGain);

			if (rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: hackrf_set_vga_gain failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: VGA gain set to " << settings.m_vgaGain;
			}
		}
	}

	if ((m_settings.m_bandwidth != settings.m_bandwidth) || force)
	{
        if (m_dev != 0)
		{
	        uint32_t bw_index = hackrf_compute_baseband_filter_bw_round_down_lt(settings.m_bandwidth + 1); // +1 so the round down to lower than yields desired bandwidth
			rc = (hackrf_error) hackrf_set_baseband_filter_bandwidth(m_dev, bw_index);

			if (rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: hackrf_set_baseband_filter_bandwidth failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: Baseband BW filter set to " << settings.m_bandwidth << " Hz";
			}
		}
	}

	if ((m_settings.m_biasT != settings.m_biasT) || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_antenna_enable(m_dev, (settings.m_biasT ? 1 : 0));

			if(rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: hackrf_set_antenna_enable failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: bias tee set to " << settings.m_biasT;
			}
		}
	}

	if ((m_settings.m_lnaExt != settings.m_lnaExt) || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_amp_enable(m_dev, (settings.m_lnaExt ? 1 : 0));

			if(rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFInput::applySettings: hackrf_set_amp_enable failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFInput:applySettings: extra LNA set to " << settings.m_lnaExt;
			}
		}
	}

	if (forwardChange)
	{
		int sampleRate = devSampleRate/(1<<settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

	m_settings = settings;

    qDebug() << "HackRFInput::applySettings: "
            << " m_centerFrequency: " << m_settings.m_centerFrequency << " Hz"
            << " m_LOppmTenths: " << m_settings.m_LOppmTenths
            << " m_bandwidth: " << m_settings.m_bandwidth
            << " m_lnaGain: " << m_settings.m_lnaGain
            << " m_vgaGain: " << m_settings.m_vgaGain
            << " m_log2Decim: " << m_settings.m_log2Decim
            << " m_fcPos: " << m_settings.m_fcPos
            << " m_devSampleRate: " << m_settings.m_devSampleRate
            << " m_biasT: " << m_settings.m_biasT
            << " m_lnaExt: " << m_settings.m_lnaExt
            << " m_dcBlock: " << m_settings.m_dcBlock
            << " m_linkTxFrequency: " << m_settings.m_linkTxFrequency;

	return true;
}

int HackRFInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setHackRfInputSettings(new SWGSDRangel::SWGHackRFInputSettings());
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int HackRFInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    HackRFInputSettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getHackRfInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getHackRfInputSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getHackRfInputSettings()->getBandwidth();
    }
    if (deviceSettingsKeys.contains("lnaGain")) {
        settings.m_lnaGain = response.getHackRfInputSettings()->getLnaGain();
    }
    if (deviceSettingsKeys.contains("vgaGain")) {
        settings.m_vgaGain = response.getHackRfInputSettings()->getVgaGain();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getHackRfInputSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getHackRfInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("biasT")) {
        settings.m_biasT = response.getHackRfInputSettings()->getBiasT() != 0;
    }
    if (deviceSettingsKeys.contains("lnaExt")) {
        settings.m_lnaExt = response.getHackRfInputSettings()->getLnaExt() != 0;
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getHackRfInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getHackRfInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("linkTxFrequency")) {
        settings.m_linkTxFrequency = response.getHackRfInputSettings()->getLinkTxFrequency() != 0;
    }

    MsgConfigureHackRF *msg = MsgConfigureHackRF::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureHackRF *msgToGUI = MsgConfigureHackRF::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void HackRFInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const HackRFInputSettings& settings)
{
    response.getHackRfInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getHackRfInputSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getHackRfInputSettings()->setBandwidth(settings.m_bandwidth);
    response.getHackRfInputSettings()->setLnaGain(settings.m_lnaGain);
    response.getHackRfInputSettings()->setVgaGain(settings.m_vgaGain);
    response.getHackRfInputSettings()->setLog2Decim(settings.m_log2Decim);
    response.getHackRfInputSettings()->setFcPos(settings.m_fcPos);
    response.getHackRfInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getHackRfInputSettings()->setBiasT(settings.m_biasT ? 1 : 0);
    response.getHackRfInputSettings()->setLnaExt(settings.m_lnaExt ? 1 : 0);
    response.getHackRfInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getHackRfInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getHackRfInputSettings()->setLinkTxFrequency(settings.m_linkTxFrequency ? 1 : 0);
}

int HackRFInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int HackRFInput::webapiRun(
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
