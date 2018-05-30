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
    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
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

	m_bladerfThread = new BladerfInputThread(m_dev, &m_sampleFifo);
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

        if (conf.getStartStop())
        {
            if (m_settings.m_fileRecordName.size() != 0) {
                m_fileSink->setFileName(m_settings.m_fileRecordName);
            } else {
                m_fileSink->genUniqueFileName(m_deviceAPI->getDeviceUID());
            }

            m_fileSink->startRecording();
        }
        else
        {
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
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
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
//		m_settings.m_dcBlock = settings.m_dcBlock;
//		m_settings.m_iqCorrection = settings.m_iqCorrection;
		m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
	}

	if ((m_settings.m_lnaGain != settings.m_lnaGain) || force)
	{
//		m_settings.m_lnaGain = settings.m_lnaGain;

		if (m_dev != 0)
		{
			if(bladerf_set_lna_gain(m_dev, getLnaGain(settings.m_lnaGain)) != 0)
			{
				qDebug("BladerfInput::applySettings: bladerf_set_lna_gain() failed");
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: LNA gain set to " << getLnaGain(settings.m_lnaGain);
			}
		}
	}

	if ((m_settings.m_vga1 != settings.m_vga1) || force)
	{
//		m_settings.m_vga1 = settings.m_vga1;

		if (m_dev != 0)
		{
			if(bladerf_set_rxvga1(m_dev, settings.m_vga1) != 0)
			{
				qDebug("BladerfInput::applySettings: bladerf_set_rxvga1() failed");
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: VGA1 gain set to " << settings.m_vga1;
			}
		}
	}

	if ((m_settings.m_vga2 != settings.m_vga2) || force)
	{
//		m_settings.m_vga2 = settings.m_vga2;

		if(m_dev != 0)
		{
			if(bladerf_set_rxvga2(m_dev, settings.m_vga2) != 0)
			{
				qDebug("BladerfInput::applySettings: bladerf_set_rxvga2() failed");
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: VGA2 gain set to " << settings.m_vga2;
			}
		}
	}

	if ((m_settings.m_xb200 != settings.m_xb200) || force)
	{
//		m_settings.m_xb200 = settings.m_xb200;

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
	            if (settings.m_xb200)
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

	            m_sharedParams.m_xb200Attached = settings.m_xb200;
		    }
		}
	}

	if ((m_settings.m_xb200Path != settings.m_xb200Path) || force)
	{
//		m_settings.m_xb200Path = settings.m_xb200Path;

		if (m_dev != 0)
		{
			if(bladerf_xb200_set_path(m_dev, BLADERF_MODULE_RX, settings.m_xb200Path) != 0)
			{
				qDebug("BladerfInput::applySettings: bladerf_xb200_set_path(BLADERF_MODULE_RX) failed");
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: set xb200 path to " << settings.m_xb200Path;
			}
		}
	}

	if ((m_settings.m_xb200Filter != settings.m_xb200Filter) || force)
	{
//		m_settings.m_xb200Filter = settings.m_xb200Filter;

		if (m_dev != 0)
		{
			if(bladerf_xb200_set_filterbank(m_dev, BLADERF_MODULE_RX, settings.m_xb200Filter) != 0)
			{
				qDebug("BladerfInput::applySettings: bladerf_xb200_set_filterbank(BLADERF_MODULE_RX) failed");
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: set xb200 filter to " << settings.m_xb200Filter;
			}
		}
	}

	if ((m_settings.m_devSampleRate != settings.m_devSampleRate) || force)
	{
//		m_settings.m_devSampleRate = settings.m_devSampleRate;
		forwardChange = true;

		if (m_dev != 0)
		{
			unsigned int actualSamplerate;

			if (bladerf_set_sample_rate(m_dev, BLADERF_MODULE_RX, settings.m_devSampleRate, &actualSamplerate) < 0)
			{
				qCritical("BladerfInput::applySettings: could not set sample rate: %d", settings.m_devSampleRate);
			}
			else
			{
				qDebug() << "BladerfInput::applySettings: bladerf_set_sample_rate(BLADERF_MODULE_RX) actual sample rate is " << actualSamplerate;
			}
		}
	}

	if ((m_settings.m_bandwidth != settings.m_bandwidth) || force)
	{
//		m_settings.m_bandwidth = settings.m_bandwidth;

		if(m_dev != 0)
		{
			unsigned int actualBandwidth;

			if( bladerf_set_bandwidth(m_dev, BLADERF_MODULE_RX, settings.m_bandwidth, &actualBandwidth) < 0)
			{
				qCritical("BladerfInput::applySettings: could not set bandwidth: %d", settings.m_bandwidth);
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
//        m_settings.m_log2Decim = settings.m_log2Decim;
        forwardChange = true;

        if (m_bladerfThread != 0)
        {
            m_bladerfThread->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "BladerfInput::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_devSampleRate != settings.m_devSampleRate)
        || (m_settings.m_fcPos != settings.m_fcPos)
        || (m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                0,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate);

//        m_settings.m_centerFrequency = settings.m_centerFrequency;
//        m_settings.m_log2Decim = settings.m_log2Decim;
//        m_settings.m_fcPos = settings.m_fcPos;

        forwardChange = true;

        if (m_dev != 0)
        {
            if (bladerf_set_frequency( m_dev, BLADERF_MODULE_RX, deviceCenterFrequency ) != 0) {
                qWarning("BladerfInput::applySettings: bladerf_set_frequency(%lld) failed", settings.m_centerFrequency);
            } else {
                qDebug("BladerfInput::applySettings: bladerf_set_frequency(%lld)", settings.m_centerFrequency);
            }
        }
    }

	if (forwardChange)
	{
		int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

	m_settings = settings;

    qDebug() << "BladerfInput::applySettings: "
            << " m_centerFrequency: " << m_settings.m_centerFrequency << " Hz"
            << " m_bandwidth: " << m_settings.m_bandwidth
            << " m_lnaGain: " << m_settings.m_lnaGain
            << " m_vga1: " << m_settings.m_vga1
            << " m_vga2: " << m_settings.m_vga2
            << " m_log2Decim: " << m_settings.m_log2Decim
            << " m_fcPos: " << m_settings.m_fcPos
            << " m_devSampleRate: " << m_settings.m_devSampleRate
            << " m_dcBlock: " << m_settings.m_dcBlock
            << " m_iqCorrection: " << m_settings.m_iqCorrection
            << " m_xb200Filter: " << m_settings.m_xb200Filter
            << " m_xb200Path: " << m_settings.m_xb200Path
            << " m_xb200: " << m_settings.m_xb200;

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

int BladerfInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setBladeRfInputSettings(new SWGSDRangel::SWGBladeRFInputSettings());
    response.getBladeRfInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

void BladerfInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const BladeRFInputSettings& settings)
{
    response.getBladeRfInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getBladeRfInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getBladeRfInputSettings()->setLnaGain(settings.m_lnaGain);
    response.getBladeRfInputSettings()->setVga1(settings.m_vga1);
    response.getBladeRfInputSettings()->setVga2(settings.m_vga2);
    response.getBladeRfInputSettings()->setBandwidth(settings.m_bandwidth);
    response.getBladeRfInputSettings()->setLog2Decim(settings.m_log2Decim);
    response.getBladeRfInputSettings()->setFcPos((int) settings.m_fcPos);
    response.getBladeRfInputSettings()->setXb200(settings.m_xb200 ? 1 : 0);
    response.getBladeRfInputSettings()->setXb200Path((int) settings.m_xb200Path);
    response.getBladeRfInputSettings()->setXb200Filter((int) settings.m_xb200Filter);
    response.getBladeRfInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getBladeRfInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);

    if (response.getBladeRfInputSettings()->getFileRecordName()) {
        *response.getBladeRfInputSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getBladeRfInputSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }
}

int BladerfInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    BladeRFInputSettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getBladeRfInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getBladeRfInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("lnaGain")) {
        settings.m_lnaGain = response.getBladeRfInputSettings()->getLnaGain();
    }
    if (deviceSettingsKeys.contains("vga1")) {
        settings.m_vga1 = response.getBladeRfInputSettings()->getVga1();
    }
    if (deviceSettingsKeys.contains("vga2")) {
        settings.m_vga2 = response.getBladeRfInputSettings()->getVga2();
    }
    if (deviceSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = response.getBladeRfInputSettings()->getBandwidth();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getBladeRfInputSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        settings.m_fcPos = static_cast<BladeRFInputSettings::fcPos_t>(response.getBladeRfInputSettings()->getFcPos());
    }
    if (deviceSettingsKeys.contains("xb200")) {
        settings.m_xb200 = response.getBladeRfInputSettings()->getXb200() == 0 ? 0 : 1;
    }
    if (deviceSettingsKeys.contains("xb200Path")) {
        settings.m_xb200Path = static_cast<bladerf_xb200_path>(response.getBladeRfInputSettings()->getXb200Path());
    }
    if (deviceSettingsKeys.contains("xb200Filter")) {
        settings.m_xb200Filter = static_cast<bladerf_xb200_filter>(response.getBladeRfInputSettings()->getXb200Filter());
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getBladeRfInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getBladeRfInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getBladeRfInputSettings()->getFileRecordName();
    }

    MsgConfigureBladerf *msg = MsgConfigureBladerf::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureBladerf *msgToGUI = MsgConfigureBladerf::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
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
