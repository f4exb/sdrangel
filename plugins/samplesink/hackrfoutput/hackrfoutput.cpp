///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include "hackrf/devicehackrfshared.h"
#include "hackrfoutputthread.h"
#include "hackrfoutput.h"

MESSAGE_CLASS_DEFINITION(HackRFOutput::MsgConfigureHackRF, Message)
MESSAGE_CLASS_DEFINITION(HackRFOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(HackRFOutput::MsgReportHackRF, Message)

HackRFOutput::HackRFOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(nullptr),
	m_hackRFThread(nullptr),
	m_deviceDescription("HackRFOutput"),
	m_running(false)
{
    openDevice();
    m_deviceAPI->setNbSinkStreams(1);
    m_deviceAPI->setBuddySharedPtr(&m_sharedParams);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &HackRFOutput::networkManagerFinished
    );
}

HackRFOutput::~HackRFOutput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &HackRFOutput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

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

    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_settings.m_devSampleRate));

    if (m_deviceAPI->getSourceBuddies().size() > 0)
    {
        DeviceAPI *buddy = m_deviceAPI->getSourceBuddies()[0];
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
        if ((m_dev = DeviceHackRF::open_hackrf(qPrintable(m_deviceAPI->getSamplingDeviceSerial()))) == 0)
        {
            qCritical("HackRFOutput::openDevice: could not open HackRF %s", qPrintable(m_deviceAPI->getSamplingDeviceSerial()));
            return false;
        }

        m_sharedParams.m_dev = m_dev;
    }

    return true;
}

void HackRFOutput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool HackRFOutput::start()
{
    if (!m_dev) {
        return false;
    }

    if (m_running) {
        stop();
    }

    m_hackRFThread = new HackRFOutputThread(m_dev, &m_sampleSourceFifo);

//	mutexLocker.unlock();

	applySettings(m_settings, QList<QString>(), true);

    m_hackRFThread->setLog2Interpolation(m_settings.m_log2Interp);
    m_hackRFThread->setFcPos((int) m_settings.m_fcPos);

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

    MsgConfigureHackRF* message = MsgConfigureHackRF::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureHackRF* messageToGUI = MsgConfigureHackRF::create(m_settings, QList<QString>(), true);
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

    MsgConfigureHackRF* message = MsgConfigureHackRF::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureHackRF* messageToGUI = MsgConfigureHackRF::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool HackRFOutput::handleMessage(const Message& message)
{
	if (MsgConfigureHackRF::match(message))
	{
		MsgConfigureHackRF& conf = (MsgConfigureHackRF&) message;
		qDebug() << "HackRFOutput::handleMessage: MsgConfigureHackRF";

		bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

		if (!success) {
			qDebug("HackRFOutput::handleMessage: MsgConfigureHackRF: config error");
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "HackRFOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
        qint64 centerFrequency = DeviceSampleSink::calculateCenterFrequency(
            freqMsg.getFrequency(),
            0,
            m_settings.m_log2Interp,
            (DeviceSampleSink::fcPos_t) m_settings.m_fcPos,
            m_settings.m_devSampleRate);
        qDebug("HackRFOutput::handleMessage: MsgSynchronizeFrequency: centerFrequency: %lld Hz", centerFrequency);
        HackRFOutputSettings settings = m_settings;
        settings.m_centerFrequency = centerFrequency;

        if (m_guiMessageQueue)
        {
            MsgConfigureHackRF* messageToGUI = MsgConfigureHackRF::create(settings, QList<QString>{"centerFrequency"}, false);
            m_guiMessageQueue->push(messageToGUI);
        }

        m_settings.m_centerFrequency = settings.m_centerFrequency;
		int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2Interp);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
		m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        return true;
    }
	else
	{
		return false;
	}
}

void HackRFOutput::setDeviceCenterFrequency(quint64 freq_hz, int loPpmTenths)
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

bool HackRFOutput::applySettings(const HackRFOutputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
//	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "HackRFOutput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);

	bool forwardChange    = false;
	bool suspendThread    = false;
	bool threadWasRunning = false;
	hackrf_error rc;

    // if ((m_settings.m_devSampleRate != settings.m_devSampleRate) ||
    //     (m_settings.m_log2Interp != settings.m_log2Interp) || force)
    if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("log2Interp") || force)
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

    if (settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("log2Interp") || force)
    {
        forwardChange = true;
        unsigned int fifoRate = std::max(
            (unsigned int) settings.m_devSampleRate / (1<<settings.m_log2Interp),
            DeviceHackRFShared::m_sampleFifoMinRate);
        m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(fifoRate));
    }

	if (settingsKeys.contains("devSampleRate") || force)
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

	if (settingsKeys.contains("log2Interp") || force)
	{
		if (m_hackRFThread != 0)
		{
			m_hackRFThread->setLog2Interpolation(settings.m_log2Interp);
			qDebug() << "HackRFOutput: set interpolation to " << (1<<settings.m_log2Interp);
		}
	}

	if (settingsKeys.contains("centerFrequency") ||
	    settingsKeys.contains("devSampleRate") ||
        settingsKeys.contains("log2Interp") ||
        settingsKeys.contains("fcPos") ||
        settingsKeys.contains("transverterMode") ||
        settingsKeys.contains("transverterDeltaFrequency") ||
        settingsKeys.contains("LOppmTenths") || force)
	{
        qint64 deviceCenterFrequency = DeviceSampleSink::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                settings.m_transverterDeltaFrequency,
                settings.m_log2Interp,
                (DeviceSampleSink::fcPos_t) settings.m_fcPos,
                settings.m_devSampleRate,
                settings.m_transverterMode);
        setDeviceCenterFrequency(deviceCenterFrequency, settings.m_LOppmTenths);

        if (m_deviceAPI->getSourceBuddies().size() > 0)
        {
            DeviceAPI *buddy = m_deviceAPI->getSourceBuddies()[0];
            DeviceHackRFShared::MsgSynchronizeFrequency *freqMsg = DeviceHackRFShared::MsgSynchronizeFrequency::create(deviceCenterFrequency);
            buddy->getSamplingDeviceInputMessageQueue()->push(freqMsg);
        }

		forwardChange = true;
	}

	if (settingsKeys.contains("fcPos") || force)
	{
		if (m_hackRFThread != 0) {
			m_hackRFThread->setFcPos((int) settings.m_fcPos);
		}
	}

	if (settingsKeys.contains("vgaGain") || force)
	{
		if (m_dev != 0)
		{
			rc = (hackrf_error) hackrf_set_txvga_gain(m_dev, settings.m_vgaGain);

			if (rc != HACKRF_SUCCESS) {
				qDebug("HackRFOutput::applySettings: hackrf_set_txvga_gain failed: %s", hackrf_error_name(rc));
			} else {
				qDebug() << "HackRFOutput:applySettings: TxVGA gain set to " << settings.m_vgaGain;
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

	if (threadWasRunning) {
	    m_hackRFThread->startWork();
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
                QString& errorMessage)
{
    (void) errorMessage;
    response.setHackRfOutputSettings(new SWGSDRangel::SWGHackRFOutputSettings());
    response.getHackRfOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int HackRFOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    HackRFOutputSettings settings = m_settings;
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

void HackRFOutput::webapiUpdateDeviceSettings(
    HackRFOutputSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
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
    if (deviceSettingsKeys.contains("fcPos"))
    {
        int fcPos = response.getHackRfOutputSettings()->getFcPos();
        fcPos = fcPos < 0 ? 0 : fcPos > 2 ? 2 : fcPos;
        settings.m_fcPos = (HackRFOutputSettings::fcPos_t) fcPos;
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
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getHackRfOutputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getHackRfOutputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getHackRfOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getHackRfOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getHackRfOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getHackRfOutputSettings()->getReverseApiDeviceIndex();
    }
}

void HackRFOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const HackRFOutputSettings& settings)
{
    response.getHackRfOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getHackRfOutputSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getHackRfOutputSettings()->setBandwidth(settings.m_bandwidth);
    response.getHackRfOutputSettings()->setVgaGain(settings.m_vgaGain);
    response.getHackRfOutputSettings()->setLog2Interp(settings.m_log2Interp);
    response.getHackRfOutputSettings()->setFcPos(settings.m_fcPos);
    response.getHackRfOutputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getHackRfOutputSettings()->setBiasT(settings.m_biasT ? 1 : 0);
    response.getHackRfOutputSettings()->setLnaExt(settings.m_lnaExt ? 1 : 0);
    response.getHackRfOutputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getHackRfOutputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    response.getHackRfOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getHackRfOutputSettings()->getReverseApiAddress()) {
        *response.getHackRfOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getHackRfOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getHackRfOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getHackRfOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int HackRFOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int HackRFOutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
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

void HackRFOutput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const HackRFOutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("HackRF"));
    swgDeviceSettings->setHackRfOutputSettings(new SWGSDRangel::SWGHackRFOutputSettings());
    SWGSDRangel::SWGHackRFOutputSettings *swgHackRFOutputSettings = swgDeviceSettings->getHackRfOutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgHackRFOutputSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgHackRFOutputSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("bandwidth") || force) {
        swgHackRFOutputSettings->setBandwidth(settings.m_bandwidth);
    }
    if (deviceSettingsKeys.contains("vgaGain") || force) {
        swgHackRFOutputSettings->setVgaGain(settings.m_vgaGain);
    }
    if (deviceSettingsKeys.contains("log2Interp") || force) {
        swgHackRFOutputSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgHackRFOutputSettings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgHackRFOutputSettings->setDevSampleRate(settings.m_devSampleRate);
    }
    if (deviceSettingsKeys.contains("biasT") || force) {
        swgHackRFOutputSettings->setBiasT(settings.m_biasT ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("lnaExt") || force) {
        swgHackRFOutputSettings->setLnaExt(settings.m_lnaExt ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgHackRFOutputSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgHackRFOutputSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
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

void HackRFOutput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
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

void HackRFOutput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "HackRFOutput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("HackRFOutput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
