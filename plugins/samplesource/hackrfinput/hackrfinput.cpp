///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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
#include <QNetworkReply>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"
#include "hackrf/devicehackrfvalues.h"
#include "hackrf/devicehackrfshared.h"

#include "hackrfinput.h"
#include "hackrfinputthread.h"

MESSAGE_CLASS_DEFINITION(HackRFInput::MsgConfigureHackRF, Message)
MESSAGE_CLASS_DEFINITION(HackRFInput::MsgReportHackRF, Message)
MESSAGE_CLASS_DEFINITION(HackRFInput::MsgStartStop, Message)

HackRFInput::HackRFInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(nullptr),
	m_hackRFThread(nullptr),
	m_deviceDescription("HackRF"),
	m_running(false)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    openDevice();

    m_deviceAPI->setNbSourceStreams(1);
    m_deviceAPI->setBuddySharedPtr(&m_sharedParams);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &HackRFInput::networkManagerFinished
    );
}

HackRFInput::~HackRFInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &HackRFInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
	m_deviceAPI->setBuddySharedPtr(0);
}

void HackRFInput::destroy()
{
    delete this;
}

bool HackRFInput::openDevice()
{
    if (m_dev)
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
        DeviceAPI *buddy = m_deviceAPI->getSinkBuddies()[0];
        DeviceHackRFParams *buddySharedParams = (DeviceHackRFParams *) buddy->getBuddySharedPtr();

        if (buddySharedParams == 0)
        {
            qCritical("HackRFInput::openDevice: could not get shared parameters from buddy");
            return false;
        }

        if (buddySharedParams->m_dev == nullptr) // device is not opened by buddy
        {
            qCritical("HackRFInput::openDevice: could not get HackRF handle from buddy");
            return false;
        }

        m_sharedParams = *(buddySharedParams); // copy parameters from buddy
        m_dev = m_sharedParams.m_dev;          // get HackRF handle
    }
    else
    {
        if ((m_dev = DeviceHackRF::open_hackrf(qPrintable(m_deviceAPI->getSamplingDeviceSerial()))) == 0)
        {
            qCritical("HackRFInput::openDevice: could not open HackRF %s", qPrintable(m_deviceAPI->getSamplingDeviceSerial()));
            m_dev = nullptr;
            return false;
        }

        m_sharedParams.m_dev = m_dev;
    }

    qDebug("HackRFInput::openDevice: success");
    return true;
}

void HackRFInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool HackRFInput::start()
{
//	QMutexLocker mutexLocker(&m_mutex);
    if (!m_dev) {
        return false;
    }

    if (m_running) {
        stop();
    }

    m_hackRFThread = new HackRFInputThread(m_dev, &m_sampleFifo);

//	mutexLocker.unlock();

	applySettings(m_settings, QList<QString>(), true);

	m_hackRFThread->setSamplerate(m_settings.m_devSampleRate);
	m_hackRFThread->setLog2Decimation(m_settings.m_log2Decim);
	m_hackRFThread->setFcPos((int) m_settings.m_fcPos);
    m_hackRFThread->setIQOrder(m_settings.m_iqOrder);
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

        if (m_dev) // close HackRF
        {
            hackrf_close(m_dev);
            //hackrf_exit(); // TODO: this may not work if several HackRF Devices are running concurrently. It should be handled globally in the application
        }
    }

    m_sharedParams.m_dev = 0;
    m_dev = nullptr;
}

void HackRFInput::stop()
{
	qDebug("HackRFInput::stop");
//	QMutexLocker mutexLocker(&m_mutex);

	if (m_hackRFThread)
	{
		m_hackRFThread->stopWork();
		delete m_hackRFThread;
		m_hackRFThread = nullptr;
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

    MsgConfigureHackRF* message = MsgConfigureHackRF::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureHackRF* messageToGUI = MsgConfigureHackRF::create(m_settings, QList<QString>(), true);
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

    MsgConfigureHackRF* message = MsgConfigureHackRF::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureHackRF* messageToGUI = MsgConfigureHackRF::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool HackRFInput::handleMessage(const Message& message)
{
	if (MsgConfigureHackRF::match(message))
	{
		MsgConfigureHackRF& conf = (MsgConfigureHackRF&) message;
		qDebug() << "HackRFInput::handleMessage: MsgConfigureHackRF";

		bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

		if (!success) {
			qDebug("HackRFInput::handleMessage: config error");
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "HackRFInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine()) {
                m_deviceAPI->startDeviceEngine();
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine();
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else if (DeviceHackRFShared::MsgSynchronizeFrequency::match(message))
    {
        DeviceHackRFShared::MsgSynchronizeFrequency& freqMsg = (DeviceHackRFShared::MsgSynchronizeFrequency&) message;
        qint64 centerFrequency = DeviceSampleSource::calculateCenterFrequency(
            freqMsg.getFrequency(),
            0,
            m_settings.m_log2Decim,
            (DeviceSampleSource::fcPos_t) m_settings.m_fcPos,
            m_settings.m_devSampleRate,
            DeviceSampleSource::FSHIFT_TXSYNC);
        qDebug("HackRFInput::handleMessage: MsgSynchronizeFrequency: centerFrequency: %lld Hz", centerFrequency);
        HackRFInputSettings settings = m_settings;
        settings.m_centerFrequency = centerFrequency;

        if (m_guiMessageQueue)
        {
            QList<QString> settingsKeys({"log2Decim", "fcPos", "devSampleRate", "centerFrequency"});
            MsgConfigureHackRF* messageToGUI = MsgConfigureHackRF::create(settings, settingsKeys, false);
            m_guiMessageQueue->push(messageToGUI);
        }

        m_settings.m_centerFrequency = settings.m_centerFrequency;
		int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        return true;
    }
	else
	{
		return false;
	}
}

void HackRFInput::setDeviceCenterFrequency(quint64 freq_hz, int loPpmTenths)
{
    if (!m_dev) {
        return;
    }

    qint64 df = ((qint64)freq_hz * loPpmTenths) / 10000000LL;
    freq_hz += df;

	hackrf_error rc = (hackrf_error) hackrf_set_freq(m_dev, static_cast<uint64_t>(freq_hz));

	if (rc != HACKRF_SUCCESS) {
		qWarning("HackRFInput::setDeviceCenterFrequency: could not frequency to %llu Hz", freq_hz);
	} else {
		qDebug("HackRFInput::setDeviceCenterFrequency: frequency set to %llu Hz", freq_hz);
	}
}

bool HackRFInput::applySettings(const HackRFInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
//	QMutexLocker mutexLocker(&m_mutex);
    qDebug() << "HackRFInput::applySettings: forcE: " << force << settings.getDebugString(settingsKeys, force);
	bool forwardChange = false;
	hackrf_error rc;

	if (settingsKeys.contains("dcBlock") ||
	    settingsKeys.contains("iqCorrection") || force)
	{
		m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
	}

	if (settingsKeys.contains("devSampleRate") || force)
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
			    if (m_hackRFThread)
			    {
	                qDebug("HackRFInput::applySettings: sample rate set to %llu S/s", settings.m_devSampleRate);
	                m_hackRFThread->setSamplerate(settings.m_devSampleRate);
			    }
    			rc = (hackrf_error) hackrf_set_baseband_filter_bandwidth(m_dev, m_settings.m_bandwidth); // restore baseband bandwidth filter. libhackrf automatically sets baseband filter when sample rate is set.
				if (rc != HACKRF_SUCCESS) {
					qDebug("HackRFInput::applySettings: Restore baseband filter failed: %s", hackrf_error_name(rc));
				} else {
					qDebug() << "HackRFInput:applySettings: Baseband BW filter restored to " << m_settings.m_bandwidth << " Hz";
				}
			}
		}
	}

	if (settingsKeys.contains("log2Decim") || force)
	{
		forwardChange = true;

		if (m_hackRFThread)
		{
			m_hackRFThread->setLog2Decimation(settings.m_log2Decim);
			qDebug() << "HackRFInput: set decimation to " << (1<<settings.m_log2Decim);
		}
	}

	if (settingsKeys.contains("iqOrder") || force)
	{
		if (m_hackRFThread) {
			m_hackRFThread->setIQOrder(settings.m_iqOrder);
		}
	}

	if (settingsKeys.contains("centerFrequency") ||
	    settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("log2Decim") ||
        settingsKeys.contains("fcPos") ||
        settingsKeys.contains("transverterMode") ||
        settingsKeys.contains("transverterDeltaFrequency") ||
        settingsKeys.contains("LOppmTenths") || force)
	{
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                settings.m_transverterDeltaFrequency,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_TXSYNC,
                settings.m_transverterMode);
		setDeviceCenterFrequency(deviceCenterFrequency, settings.m_LOppmTenths);

        if (m_deviceAPI->getSinkBuddies().size() > 0) // forward to buddy if necessary
	    {
	        DeviceAPI *buddy = m_deviceAPI->getSinkBuddies()[0];
            DeviceHackRFShared::MsgSynchronizeFrequency *freqMsg = DeviceHackRFShared::MsgSynchronizeFrequency::create(deviceCenterFrequency);
	        buddy->getSamplingDeviceInputMessageQueue()->push(freqMsg);
	    }

		forwardChange = true;
	}

	if (settingsKeys.contains("fcPos") || force)
	{
		if (m_hackRFThread)
		{
			m_hackRFThread->setFcPos((int) settings.m_fcPos);
			qDebug() << "HackRFInput: set fc pos (enum) to " << (int) settings.m_fcPos;
		}
	}

	if (settingsKeys.contains("lnaGain") || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_lna_gain(m_dev, settings.m_lnaGain);

			if (rc != HACKRF_SUCCESS) {
				qDebug("HackRFInput::applySettings: airspy_set_lna_gain failed: %s", hackrf_error_name(rc));
			} else {
				qDebug() << "HackRFInput:applySettings: LNA gain set to " << settings.m_lnaGain;
			}
		}
	}

	if (settingsKeys.contains("vgaGain") || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_vga_gain(m_dev, settings.m_vgaGain);

			if (rc != HACKRF_SUCCESS) {
				qDebug("HackRFInput::applySettings: hackrf_set_vga_gain failed: %s", hackrf_error_name(rc));
			} else {
				qDebug() << "HackRFInput:applySettings: VGA gain set to " << settings.m_vgaGain;
			}
		}
	}

	if (settingsKeys.contains("bandwidth") || force)
	{
        if (m_dev != 0)
		{
	        uint32_t bw_index = hackrf_compute_baseband_filter_bw_round_down_lt(settings.m_bandwidth + 1); // +1 so the round down to lower than yields desired bandwidth
			rc = (hackrf_error) hackrf_set_baseband_filter_bandwidth(m_dev, bw_index);

			if (rc != HACKRF_SUCCESS) {
				qDebug("HackRFInput::applySettings: hackrf_set_baseband_filter_bandwidth failed: %s", hackrf_error_name(rc));
			} else {
				qDebug() << "HackRFInput:applySettings: Baseband BW filter set to " << settings.m_bandwidth << " Hz";
			}
		}
	}

	if (settingsKeys.contains("biasT") || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_antenna_enable(m_dev, (settings.m_biasT ? 1 : 0));

			if (rc != HACKRF_SUCCESS) {
				qDebug("HackRFInput::applySettings: hackrf_set_antenna_enable failed: %s", hackrf_error_name(rc));
			} else {
				qDebug() << "HackRFInput:applySettings: bias tee set to " << settings.m_biasT;
			}
		}
	}

	if (settingsKeys.contains("lnaExt") || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_amp_enable(m_dev, (settings.m_lnaExt ? 1 : 0));

			if (rc != HACKRF_SUCCESS) {
				qDebug("HackRFInput::applySettings: hackrf_set_amp_enable failed: %s", hackrf_error_name(rc));
			} else {
				qDebug() << "HackRFInput:applySettings: extra LNA set to " << settings.m_lnaExt;
			}
		}
	}

	if (forwardChange)
	{
		int sampleRate = settings.m_devSampleRate/(1<<settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

	return true;
}

int HackRFInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setHackRfInputSettings(new SWGSDRangel::SWGHackRFInputSettings());
    response.getHackRfInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int HackRFInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    HackRFInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureHackRF *msg = MsgConfigureHackRF::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureHackRF *msgToGUI = MsgConfigureHackRF::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void HackRFInput::webapiUpdateDeviceSettings(
        HackRFInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
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
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getHackRfInputSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("fcPos"))
    {
        int fcPos = response.getHackRfInputSettings()->getFcPos();
        fcPos = fcPos < 0 ? 0 : fcPos > 2 ? 2 : fcPos;
        settings.m_fcPos = (HackRFInputSettings::fcPos_t) fcPos;
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
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getHackRfInputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getHackRfInputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getHackRfInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getHackRfInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getHackRfInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getHackRfInputSettings()->getReverseApiDeviceIndex();
    }
}

void HackRFInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const HackRFInputSettings& settings)
{
    response.getHackRfInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getHackRfInputSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getHackRfInputSettings()->setBandwidth(settings.m_bandwidth);
    response.getHackRfInputSettings()->setLnaGain(settings.m_lnaGain);
    response.getHackRfInputSettings()->setVgaGain(settings.m_vgaGain);
    response.getHackRfInputSettings()->setLog2Decim(settings.m_log2Decim);
    response.getHackRfInputSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getHackRfInputSettings()->setFcPos(settings.m_fcPos);
    response.getHackRfInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getHackRfInputSettings()->setBiasT(settings.m_biasT ? 1 : 0);
    response.getHackRfInputSettings()->setLnaExt(settings.m_lnaExt ? 1 : 0);
    response.getHackRfInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getHackRfInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getHackRfInputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getHackRfInputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    response.getHackRfInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getHackRfInputSettings()->getReverseApiAddress()) {
        *response.getHackRfInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getHackRfInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getHackRfInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getHackRfInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int HackRFInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int HackRFInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
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

void HackRFInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const HackRFInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("HackRF"));
    swgDeviceSettings->setHackRfInputSettings(new SWGSDRangel::SWGHackRFInputSettings());
    SWGSDRangel::SWGHackRFInputSettings *swgHackRFInputSettings = swgDeviceSettings->getHackRfInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgHackRFInputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgHackRFInputSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("bandwidth") || force) {
        swgHackRFInputSettings->setBandwidth(settings.m_bandwidth);
    }
    if (deviceSettingsKeys.contains("lnaGain") || force) {
        swgHackRFInputSettings->setLnaGain(settings.m_lnaGain);
    }
    if (deviceSettingsKeys.contains("vgaGain") || force) {
        swgHackRFInputSettings->setVgaGain(settings.m_vgaGain);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgHackRFInputSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgHackRFInputSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgHackRFInputSettings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgHackRFInputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("biasT") || force) {
        swgHackRFInputSettings->setBiasT(settings.m_biasT ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lnaExt") || force) {
        swgHackRFInputSettings->setLnaExt(settings.m_lnaExt ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgHackRFInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgHackRFInputSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgHackRFInputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgHackRFInputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void HackRFInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("HackRF"));

    QString deviceSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(deviceSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
    QNetworkReply *reply;

    if (start) {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "POST", buffer);
    } else {
        reply = m_networkManager->sendCustomRequest(m_networkRequest, "DELETE", buffer);
    }

    buffer->setParent(reply);
    delete swgDeviceSettings;
}

void HackRFInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "HackRFInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("HackRFInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
