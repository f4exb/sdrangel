///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "hackrfoutput.h"

#include <string.h>
#include <errno.h>
#include <QDebug>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "hackrf/devicehackrfshared.h"

#include "hackrfoutputthread.h"

MESSAGE_CLASS_DEFINITION(HackRFOutput::MsgConfigureHackRF, Message)
MESSAGE_CLASS_DEFINITION(HackRFOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(HackRFOutput::MsgReportHackRF, Message)

HackRFOutput::HackRFOutput(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(0),
	m_hackRFThread(0),
	m_deviceDescription("HackRFOutput"),
	m_running(false)
{
    openDevice();
    m_deviceAPI->setBuddySharedPtr(&m_sharedParams);
}

HackRFOutput::~HackRFOutput()
{
    if (m_running) stop();
    closeDevice();
	m_deviceAPI->setBuddySharedPtr(0);
}

void HackRFOutput::destroy()
{
    delete this;
}

bool HackRFOutput::openDevice()
{
    if (m_dev != 0)
    {
        closeDevice();
    }

    m_sampleSourceFifo.resize(m_settings.m_devSampleRate/(1<<(m_settings.m_log2Interp <= 4 ? m_settings.m_log2Interp : 4)));

    if (m_deviceAPI->getSourceBuddies().size() > 0)
    {
        DeviceSourceAPI *buddy = m_deviceAPI->getSourceBuddies()[0];
        DeviceHackRFParams *buddySharedParams = (DeviceHackRFParams *) buddy->getBuddySharedPtr();

        if (buddySharedParams == 0)
        {
            qCritical("HackRFOutput::openDevice: could not get shared parameters from buddy");
            return false;
        }

        if ((m_dev = buddySharedParams->m_dev) == 0) // device is not opened by buddy
        {
            qCritical("HackRFOutput::openDevice: could not get HackRF handle from buddy");
            return false;
        }

        m_sharedParams = *(buddySharedParams); // copy parameters from buddy
        m_sharedParams.m_dev = m_dev;
    }
    else
    {
        if ((m_dev = DeviceHackRF::open_hackrf(qPrintable(m_deviceAPI->getSampleSinkSerial()))) == 0)
        {
            qCritical("HackRFOutput::openDevice: could not open HackRF %s", qPrintable(m_deviceAPI->getSampleSinkSerial()));
            return false;
        }

        m_sharedParams.m_dev = m_dev;
    }

    return true;
}

void HackRFOutput::init()
{
    applySettings(m_settings, true);
}

bool HackRFOutput::start()
{
    if (!m_dev) {
        return false;
    }

    if (m_running) stop();

	if((m_hackRFThread = new HackRFOutputThread(m_dev, &m_sampleSourceFifo)) == 0)
	{
		qFatal("HackRFOutput::start: out of memory");
		stop();
		return false;
	}

//	mutexLocker.unlock();

	applySettings(m_settings, true);
	m_hackRFThread->setLog2Interpolation(m_settings.m_log2Interp);

	m_hackRFThread->startWork();

	qDebug("HackRFOutput::start: started");
    m_running = true;

	return true;
}

void HackRFOutput::closeDevice()
{
    if (m_deviceAPI->getSourceBuddies().size() == 0)
    {
        qDebug("HackRFOutput::closeDevice: closing device since Rx side is not open");

        if(m_dev != 0) // close HackRF
        {
            hackrf_close(m_dev);
            //hackrf_exit(); // TODO: this may not work if several HackRF Devices are running concurrently. It should be handled globally in the application
        }
    }

    m_sharedParams.m_dev = 0;
    m_dev = 0;
}

void HackRFOutput::stop()
{
	qDebug("HackRFOutput::stop");
//	QMutexLocker mutexLocker(&m_mutex);

	if(m_hackRFThread != 0)
	{
		m_hackRFThread->stopWork();
		delete m_hackRFThread;
		m_hackRFThread = 0;
	}

	m_running = false;
}

QByteArray HackRFOutput::serialize() const
{
    return m_settings.serialize();
}

bool HackRFOutput::deserialize(const QByteArray& data)
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

const QString& HackRFOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int HackRFOutput::getSampleRate() const
{
	int rate = m_settings.m_devSampleRate;
	return (rate / (1<<m_settings.m_log2Interp));
}

quint64 HackRFOutput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void HackRFOutput::setCenterFrequency(qint64 centerFrequency)
{
    HackRFOutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureHackRF* message = MsgConfigureHackRF::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureHackRF* messageToGUI = MsgConfigureHackRF::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool HackRFOutput::handleMessage(const Message& message)
{
	if (MsgConfigureHackRF::match(message))
	{
		MsgConfigureHackRF& conf = (MsgConfigureHackRF&) message;
		qDebug() << "HackRFOutput::handleMessage: MsgConfigureHackRF";

		bool success = applySettings(conf.getSettings(), conf.getForce());

		if (!success)
		{
			qDebug("HackRFOutput::handleMessage: MsgConfigureHackRF: config error");
		}

		return true;
	}
    else if (DeviceHackRFShared::MsgConfigureFrequencyDelta::match(message))
    {
        DeviceHackRFShared::MsgConfigureFrequencyDelta& conf = (DeviceHackRFShared::MsgConfigureFrequencyDelta&) message;
        HackRFOutputSettings newSettings = m_settings;
        newSettings.m_centerFrequency = m_settings.m_centerFrequency + conf.getFrequencyDelta();
        qDebug() << "HackRFOutput::handleMessage: DeviceHackRFShared::MsgConfigureFrequencyDelta: newFreq: " << newSettings.m_centerFrequency;
        applySettings(newSettings, false);

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "HackRFOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initGeneration())
            {
                m_deviceAPI->startGeneration();
                DSPEngine::instance()->startAudioInput();
            }
        }
        else
        {
            m_deviceAPI->stopGeneration();
            DSPEngine::instance()->stopAudioInput();
        }

        return true;
    }
	else
	{
		return false;
	}
}

void HackRFOutput::setDeviceCenterFrequency(quint64 freq_hz, qint32 LOppmTenths)
{
	qint64 df = ((qint64)freq_hz * LOppmTenths) / 10000000LL;
	freq_hz += df;

	hackrf_error rc = (hackrf_error) hackrf_set_freq(m_dev, static_cast<uint64_t>(freq_hz));

	if (rc != HACKRF_SUCCESS)
	{
		qWarning("HackRFOutput::setDeviceCenterFrequency: could not frequency to %llu Hz", freq_hz);
	}
	else
	{
		qWarning("HackRFOutput::setDeviceCenterFrequency: frequency set to %llu Hz", freq_hz);
	}
}

bool HackRFOutput::applySettings(const HackRFOutputSettings& settings, bool force)
{
//	QMutexLocker mutexLocker(&m_mutex);

	bool forwardChange    = false;
	bool suspendThread    = false;
	bool threadWasRunning = false;
	hackrf_error rc;

	qDebug() << "HackRFOutput::applySettings";

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) ||
        (m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        suspendThread = true;
    }

    if (suspendThread)
    {
        if (m_hackRFThread)
        {
            if (m_hackRFThread->isRunning())
            {
                m_hackRFThread->stopWork();
                threadWasRunning = true;
            }
        }
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || (m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        forwardChange = true;
        int fifoSize = std::max(
                (int) ((settings.m_devSampleRate/(1<<settings.m_log2Interp)) * DeviceHackRFShared::m_sampleFifoLengthInSeconds),
                DeviceHackRFShared::m_sampleFifoMinSize);
        m_sampleSourceFifo.resize(fifoSize);
    }

	if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_sample_rate_manual(m_dev, settings.m_devSampleRate, 1);

			if (rc != HACKRF_SUCCESS)
			{
				qCritical("HackRFOutput::applySettings: could not set sample rate to %llu S/s: %s",
				        settings.m_devSampleRate,
				        hackrf_error_name(rc));
			}
			else
			{
			    qDebug("HackRFOutput::applySettings: sample rate set to %llu S/s",
			                                settings.m_devSampleRate);
			}
		}
	}

	if ((m_settings.m_log2Interp != settings.m_log2Interp) || force)
	{
		if (m_hackRFThread != 0)
		{
			m_hackRFThread->setLog2Interpolation(settings.m_log2Interp);
			qDebug() << "HackRFOutput: set interpolation to " << (1<<settings.m_log2Interp);
		}
	}

	if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency) ||
			(m_settings.m_LOppmTenths != settings.m_LOppmTenths))
	{
		if (m_dev != 0)
		{
			setDeviceCenterFrequency(settings.m_centerFrequency, settings.m_LOppmTenths);
			qDebug() << "HackRFOutput::applySettings: center freq: " << settings.m_centerFrequency << " Hz LOppm: " << settings.m_LOppmTenths;
		}

		forwardChange = true;
	}

	if ((m_settings.m_vgaGain != settings.m_vgaGain) || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_txvga_gain(m_dev, settings.m_vgaGain);

			if(rc != HACKRF_SUCCESS)
			{
				qDebug("HackRFOutput::applySettings: hackrf_set_txvga_gain failed: %s", hackrf_error_name(rc));
			}
			else
			{
				qDebug() << "HackRFOutput:applySettings: TxVGA gain set to " << settings.m_vgaGain;
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

	if (threadWasRunning)
	{
	    m_hackRFThread->startWork();
	}

    m_settings.m_devSampleRate = settings.m_devSampleRate;
    m_settings.m_log2Interp = settings.m_log2Interp;
    m_settings.m_centerFrequency = settings.m_centerFrequency;
    m_settings.m_LOppmTenths = settings.m_LOppmTenths;
    m_settings.m_vgaGain = settings.m_vgaGain;
    m_settings.m_bandwidth = settings.m_bandwidth;
    m_settings.m_biasT = settings.m_biasT;
    m_settings.m_lnaExt = settings.m_lnaExt;

	if (forwardChange)
	{
		int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
		m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

	return true;
}

int HackRFOutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setHackRfOutputSettings(new SWGSDRangel::SWGHackRFOutputSettings());
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int HackRFOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    HackRFOutputSettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getHackRfOutputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getHackRfOutputSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getHackRfOutputSettings()->getBandwidth();
    }
    if (deviceSettingsKeys.contains("vgaGain")) {
        settings.m_vgaGain = response.getHackRfOutputSettings()->getVgaGain();
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getHackRfOutputSettings()->getLog2Interp();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getHackRfOutputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("biasT")) {
        settings.m_biasT = response.getHackRfOutputSettings()->getBiasT() != 0;
    }
    if (deviceSettingsKeys.contains("lnaExt")) {
        settings.m_lnaExt = response.getHackRfOutputSettings()->getLnaExt() != 0;
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

void HackRFOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const HackRFOutputSettings& settings)
{
    response.getHackRfOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getHackRfOutputSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getHackRfOutputSettings()->setBandwidth(settings.m_bandwidth);
    response.getHackRfOutputSettings()->setVgaGain(settings.m_vgaGain);
    response.getHackRfOutputSettings()->setLog2Interp(settings.m_log2Interp);
    response.getHackRfOutputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getHackRfOutputSettings()->setBiasT(settings.m_biasT ? 1 : 0);
    response.getHackRfOutputSettings()->setLnaExt(settings.m_lnaExt ? 1 : 0);
}

int HackRFOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int HackRFOutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgStartStop *messagetoGui = MsgStartStop::create(run);
        m_guiMessageQueue->push(messagetoGui);
    }

    return 200;
}

