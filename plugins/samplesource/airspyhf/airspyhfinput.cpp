///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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
#include "SWGDeviceReport.h"
#include "SWGAirspyHFReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"

#include "airspyhfinput.h"

#include "airspyhfplugin.h"
#include "airspyhfsettings.h"
#include "airspyhfworker.h"

MESSAGE_CLASS_DEFINITION(AirspyHFInput::MsgConfigureAirspyHF, Message)
MESSAGE_CLASS_DEFINITION(AirspyHFInput::MsgStartStop, Message)

const qint64 AirspyHFInput::loLowLimitFreqHF   =      9000L;
const qint64 AirspyHFInput::loHighLimitFreqHF  =  31000000L;
const qint64 AirspyHFInput::loLowLimitFreqVHF  =  60000000L;
const qint64 AirspyHFInput::loHighLimitFreqVHF = 260000000L;

AirspyHFInput::AirspyHFInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(nullptr),
	m_airspyHFWorker(nullptr),
    m_airspyHFWorkerThread(nullptr),
	m_deviceDescription("AirspyHF"),
	m_running(false)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    openDevice();
    m_deviceAPI->setNbSourceStreams(1);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AirspyHFInput::networkManagerFinished
    );
}

AirspyHFInput::~AirspyHFInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AirspyHFInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void AirspyHFInput::destroy()
{
    delete this;
}

bool AirspyHFInput::openDevice()
{
    if (m_dev)
    {
        closeDevice();
    }

    airspyhf_error rc;

    if (!m_sampleFifo.setSize(1<<19))
    {
        qCritical("AirspyHFInput::openDevice: could not allocate SampleFifo");
        return false;
    }

    if ((m_dev = open_airspyhf_from_serial(m_deviceAPI->getSamplingDeviceSerial())) == 0)
    {
        qCritical("AirspyHFInput::openDevice: could not open Airspy HF with serial %s", qPrintable(m_deviceAPI->getSamplingDeviceSerial()));
        m_dev = nullptr;
        return false;
    }
    else
    {
        qDebug("AirspyHFInput::openDevice: opened Airspy HF with serial %s", qPrintable(m_deviceAPI->getSamplingDeviceSerial()));
    }

    uint32_t nbSampleRates;
    uint32_t *sampleRates;

    rc = (airspyhf_error) airspyhf_get_samplerates(m_dev, &nbSampleRates, 0);

    if (rc == AIRSPYHF_SUCCESS)
    {
        qDebug("AirspyHFInput::openDevice: %d sample rates for Airspy HF", nbSampleRates);
    }
    else
    {
        qCritical("AirspyHFInput::openDevice: could not obtain the number of Airspy HF sample rates");
        closeDevice();
        return false;
    }

    sampleRates = new uint32_t[nbSampleRates];

    rc = (airspyhf_error) airspyhf_get_samplerates(m_dev, sampleRates, nbSampleRates);

    if (rc == AIRSPYHF_SUCCESS)
    {
        qDebug("AirspyHFInput::openDevice: obtained Airspy HF sample rates");
    }
    else
    {
        qCritical("AirspyHFInput::openDevice: could not obtain Airspy HF sample rates");
        closeDevice();
        return false;
    }

    m_sampleRates.clear();

    for (unsigned int i = 0; i < nbSampleRates; i++)
    {
        m_sampleRates.push_back(sampleRates[i]);
        qDebug("AirspyHFInput::openDevice: sampleRates[%d] = %u Hz", i, sampleRates[i]);
    }

    delete[] sampleRates;

    return true;
}

void AirspyHFInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool AirspyHFInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_dev) {
        return false;
    }

    if (m_running) {
        stop();
    }

    m_airspyHFWorkerThread = new QThread();
    m_airspyHFWorker = new AirspyHFWorker(m_dev, &m_sampleFifo);
    m_airspyHFWorker->moveToThread(m_airspyHFWorkerThread);
	int sampleRateIndex = m_settings.m_devSampleRateIndex;

    QObject::connect(m_airspyHFWorkerThread, &QThread::started, m_airspyHFWorker, &AirspyHFWorker::startWork);
    QObject::connect(m_airspyHFWorkerThread, &QThread::finished, m_airspyHFWorker, &QObject::deleteLater);
    QObject::connect(m_airspyHFWorkerThread, &QThread::finished, m_airspyHFWorkerThread, &QThread::deleteLater);

    if (m_settings.m_devSampleRateIndex >= m_sampleRates.size()) {
        sampleRateIndex = m_sampleRates.size() - 1;
    }

    if (sampleRateIndex >= 0) {
        m_airspyHFWorker->setSamplerate(m_sampleRates[sampleRateIndex]);
    }

	m_airspyHFWorker->setLog2Decimation(m_settings.m_log2Decim);
    m_airspyHFWorker->setIQOrder(m_settings.m_iqOrder);
    mutexLocker.unlock();

    m_airspyHFWorkerThread->start();

    qDebug("AirspyHFInput::startInput: started");
    applySettings(m_settings, QList<QString>(), true);
    m_running = true;

	return m_running;
}

void AirspyHFInput::closeDevice()
{
    if (m_dev)
    {
        airspyhf_stop(m_dev);
        airspyhf_close(m_dev);
        m_dev = nullptr;
    }

    m_deviceDescription.clear();
}

void AirspyHFInput::stop()
{
	qDebug("AirspyHFInput::stop");
	QMutexLocker mutexLocker(&m_mutex);

    if (m_airspyHFWorkerThread)
    {
        m_airspyHFWorkerThread->quit();
        m_airspyHFWorkerThread->wait();
        m_airspyHFWorkerThread = nullptr;
        m_airspyHFWorker = nullptr;
    }

	m_running = false;
}

QByteArray AirspyHFInput::serialize() const
{
    return m_settings.serialize();
}

bool AirspyHFInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureAirspyHF* message = MsgConfigureAirspyHF::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAirspyHF* messageToGUI = MsgConfigureAirspyHF::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& AirspyHFInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int AirspyHFInput::getSampleRate() const
{
    int sampleRateIndex = m_settings.m_devSampleRateIndex;

    if (m_settings.m_devSampleRateIndex >= m_sampleRates.size()) {
        sampleRateIndex = m_sampleRates.size() - 1;
    }

    if (sampleRateIndex >= 0)
    {
        int rate = m_sampleRates[sampleRateIndex];
        return (rate / (1<<m_settings.m_log2Decim));
    }
    else
    {
        return 0;
    }
}

quint64 AirspyHFInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void AirspyHFInput::setCenterFrequency(qint64 centerFrequency)
{
    AirspyHFSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureAirspyHF* message = MsgConfigureAirspyHF::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAirspyHF* messageToGUI = MsgConfigureAirspyHF::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool AirspyHFInput::handleMessage(const Message& message)
{
	if (MsgConfigureAirspyHF::match(message))
	{
	    MsgConfigureAirspyHF& conf = (MsgConfigureAirspyHF&) message;
		qDebug() << "MsgConfigureAirspyHF::handleMessage: MsgConfigureAirspyHF";

		bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

		if (!success) {
			qDebug("MsgConfigureAirspyHF::handleMessage: AirspyHF config error");
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "AirspyHFInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
	else
	{
		return false;
	}
}

void AirspyHFInput::setDeviceCenterFrequency(quint64 freq_hz, const AirspyHFSettings& settings)
{
    switch(settings.m_bandIndex)
    {
    case 1:
        freq_hz = freq_hz < loLowLimitFreqVHF ? loLowLimitFreqVHF : freq_hz > loHighLimitFreqVHF ? loHighLimitFreqVHF : freq_hz;
        break;
    case 0:
    default:
        freq_hz = freq_hz < loLowLimitFreqHF ? loLowLimitFreqHF : freq_hz > loHighLimitFreqHF ? loHighLimitFreqHF : freq_hz;
        break;
    }

	airspyhf_error rc = (airspyhf_error) airspyhf_set_freq(m_dev, static_cast<uint32_t>(freq_hz));

	if (rc == AIRSPYHF_SUCCESS) {
		qDebug("AirspyHFInput::setDeviceCenterFrequency: frequency set to %llu Hz", freq_hz);
	} else {
		qWarning("AirspyHFInput::setDeviceCenterFrequency: could not frequency to %llu Hz", freq_hz);
	}
}

bool AirspyHFInput::applySettings(const AirspyHFSettings& settings, const QList<QString>& settingsKeys, bool force)
{
	qDebug() << "AirspyHFInput::applySettings: "
        << " force: " << force
        << settings.getDebugString(settingsKeys, force);

    QMutexLocker mutexLocker(&m_mutex);

	bool forwardChange = false;
	airspyhf_error rc;
    QList<QString> reverseAPIKeys;

    int sampleRateIndex = settings.m_devSampleRateIndex;

    if (settingsKeys.contains("dcBlock") ||
        settingsKeys.contains("iqCorrection") || force)
    {
		m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
	}

	if (settingsKeys.contains("devSampleRateIndex") || force)
	{
		forwardChange = true;

		if (settings.m_devSampleRateIndex >= m_sampleRates.size()) {
		    sampleRateIndex = m_sampleRates.size() - 1;
		}

		if (m_dev && (sampleRateIndex >= 0))
		{
			rc = (airspyhf_error) airspyhf_set_samplerate(m_dev, sampleRateIndex);

			if (rc != AIRSPYHF_SUCCESS)
			{
				qCritical("AirspyHFInput::applySettings: could not set sample rate index %u (%d S/s)", sampleRateIndex, m_sampleRates[sampleRateIndex]);
			}
			else if (m_airspyHFWorker)
			{
				qDebug("AirspyHFInput::applySettings: sample rate set to index: %u (%d S/s)", sampleRateIndex, m_sampleRates[sampleRateIndex]);
				m_airspyHFWorker->setSamplerate(m_sampleRates[sampleRateIndex]);
			}
		}
	}

	if (settingsKeys.contains("log2Decim") || force)
	{
		forwardChange = true;

		if (m_airspyHFWorker)
		{
		    m_airspyHFWorker->setLog2Decimation(settings.m_log2Decim);
			qDebug() << "AirspyInput: set decimation to " << (1<<settings.m_log2Decim);
		}
	}

	if (settingsKeys.contains("iqOrder") || force)
	{
		if (m_airspyHFWorker) {
		    m_airspyHFWorker->setIQOrder(settings.m_iqOrder);
		}
	}

	if (settingsKeys.contains("LOppmTenths") || force)
	{
	    if (m_dev)
	    {
	        rc = (airspyhf_error) airspyhf_set_calibration(m_dev, settings.m_LOppmTenths * 100);

	        if (rc != AIRSPYHF_SUCCESS) {
                qCritical("AirspyHFInput::applySettings: could not set LO ppm correction to %f", settings.m_LOppmTenths / 10.0f);
            } else if (m_airspyHFWorker) {
                qDebug("AirspyHFInput::applySettings: LO ppm correction set to %f", settings.m_LOppmTenths / 10.0f);
            }
	    }
	}

	if (force || settingsKeys.contains("centerFrequency")
	    || settingsKeys.contains("transverterDeltaFrequency")
	    || settingsKeys.contains("transverterMode"))
	{
        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
        qint64 f_img = deviceCenterFrequency;

		if (m_dev && (sampleRateIndex >= 0))
		{
            quint32 devSampleRate = m_sampleRates[sampleRateIndex];
			setDeviceCenterFrequency(deviceCenterFrequency, settings);

			qDebug() << "AirspyHFInput::applySettings: center freq: " << settings.m_centerFrequency << " Hz"
					<< " device center freq: " << deviceCenterFrequency << " Hz"
					<< " device sample rate: " << devSampleRate << "Hz"
					<< " Actual sample rate: " << devSampleRate/(1<<m_settings.m_log2Decim) << "Hz"
					<< " img: " << f_img << "Hz";
		}

		forwardChange = true;
	}

    if (settingsKeys.contains("useAGC") || force)
    {
        if (m_dev)
        {
            rc = (airspyhf_error) airspyhf_set_hf_agc(m_dev, settings.m_useAGC ? 1 : 0);

	        if (rc != AIRSPYHF_SUCCESS) {
                qCritical("AirspyHFInput::applySettings: could not set AGC to %d", settings.m_useAGC ? 1 : 0);
            } else {
                qDebug("AirspyHFInput::applySettings: set AGC to %d", settings.m_useAGC ? 1 : 0);
            }
        }
    }

    if (settingsKeys.contains("agcHigh") || force)
    {
        if (m_dev)
        {
            rc = (airspyhf_error) airspyhf_set_hf_agc_threshold(m_dev, settings.m_agcHigh ? 1 : 0);

	        if (rc != AIRSPYHF_SUCCESS) {
                qCritical("AirspyHFInput::applySettings: could not set AGC to %s", settings.m_agcHigh ? "high" : "low");
            } else {
                qDebug("AirspyHFInput::applySettings: set AGC to %s", settings.m_agcHigh ? "high" : "low");
            }
        }
    }

    if (settingsKeys.contains("useDSP") || force)
    {
        if (m_dev)
        {
            rc = (airspyhf_error) airspyhf_set_lib_dsp(m_dev, settings.m_useDSP ? 1 : 0);

	        if (rc != AIRSPYHF_SUCCESS) {
                qCritical("AirspyHFInput::applySettings: could not set DSP to %d", settings.m_useDSP ? 1 : 0);
            } else {
                qDebug("AirspyHFInput::applySettings: set DSP to %d", settings.m_useDSP ? 1 : 0);
            }
        }
    }

    if (settingsKeys.contains("useLNA") || force)
    {
        reverseAPIKeys.append("useLNA");

        if (m_dev)
        {
            rc = (airspyhf_error) airspyhf_set_hf_lna(m_dev, settings.m_useLNA ? 1 : 0);

	        if (rc != AIRSPYHF_SUCCESS) {
                qCritical("AirspyHFInput::applySettings: could not set LNA to %d", settings.m_useLNA ? 1 : 0);
            } else {
                qDebug("AirspyHFInput::applySettings: set LNA to %d", settings.m_useLNA ? 1 : 0);
            }
        }
    }

    if (settingsKeys.contains("attenuatorSteps") || force)
    {
        if (m_dev)
        {
            rc = (airspyhf_error) airspyhf_set_hf_att(m_dev, settings.m_attenuatorSteps);

	        if (rc != AIRSPYHF_SUCCESS) {
                qCritical("AirspyHFInput::applySettings: could not set attenuator to %d dB", settings.m_attenuatorSteps * 6);
            } else {
                qDebug("AirspyHFInput::applySettings: set attenuator to %d dB", settings.m_attenuatorSteps * 6);
            }
        }
    }

	if (forwardChange && (m_sampleRates.size() != 0) && (sampleRateIndex >= 0))
	{
		int sampleRate = m_sampleRates[sampleRateIndex]/(1<<settings.m_log2Decim);
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

airspyhf_device_t *AirspyHFInput::open_airspyhf_from_serial(const QString& serialStr)
{
    airspyhf_device_t *devinfo;
    bool ok;
    airspyhf_error rc;

    uint64_t serial = serialStr.toULongLong(&ok, 16);

    if (!ok)
    {
        qCritical("AirspyHFInput::open_airspyhf_from_serial: invalid serial %s", qPrintable(serialStr));
        return 0;
    }
    else
    {
        rc = (airspyhf_error) airspyhf_open_sn(&devinfo, serial);

        if (rc == AIRSPYHF_SUCCESS) {
            return devinfo;
        } else {
            return 0;
        }
    }
}

int AirspyHFInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setAirspyHfSettings(new SWGSDRangel::SWGAirspyHFSettings());
    response.getAirspyHfSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int AirspyHFInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    AirspyHFSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureAirspyHF *msg = MsgConfigureAirspyHF::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAirspyHF *msgToGUI = MsgConfigureAirspyHF::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void AirspyHFInput::webapiUpdateDeviceSettings(
        AirspyHFSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getAirspyHfSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("devSampleRateIndex")) {
        settings.m_devSampleRateIndex = response.getAirspyHfSettings()->getDevSampleRateIndex();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getAirspyHfSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getAirspyHfSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getAirspyHfSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getAirspyHfSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getAirspyHfSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("bandIndex")) {
        settings.m_bandIndex = response.getAirspyHfSettings()->getBandIndex();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAirspyHfSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAirspyHfSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAirspyHfSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAirspyHfSettings()->getReverseApiDeviceIndex();
    }
    if (deviceSettingsKeys.contains("useAGC")) {
        settings.m_useAGC = response.getAirspyHfSettings()->getUseAgc();
    }
    if (deviceSettingsKeys.contains("agcHigh")) {
        settings.m_agcHigh = response.getAirspyHfSettings()->getAgcHigh();
    }
    if (deviceSettingsKeys.contains("useDSP")) {
        settings.m_useDSP = response.getAirspyHfSettings()->getUseDsp();
    }
    if (deviceSettingsKeys.contains("useLNA")) {
        settings.m_useLNA = response.getAirspyHfSettings()->getUseLna();
    }
    if (deviceSettingsKeys.contains("attenuatorSteps")) {
        settings.m_attenuatorSteps = response.getAirspyHfSettings()->getAttenuatorSteps();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getAirspyHfSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getAirspyHfSettings()->getIqCorrection() != 0;
    }
}

void AirspyHFInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const AirspyHFSettings& settings)
{
    response.getAirspyHfSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getAirspyHfSettings()->setDevSampleRateIndex(settings.m_devSampleRateIndex);
    response.getAirspyHfSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getAirspyHfSettings()->setLog2Decim(settings.m_log2Decim);
    response.getAirspyHfSettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getAirspyHfSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getAirspyHfSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getAirspyHfSettings()->setBandIndex(settings.m_bandIndex ? 1 : 0);

    response.getAirspyHfSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAirspyHfSettings()->getReverseApiAddress()) {
        *response.getAirspyHfSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAirspyHfSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAirspyHfSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAirspyHfSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getAirspyHfSettings()->setUseAgc(settings.m_useAGC ? 1 : 0);
    response.getAirspyHfSettings()->setUseDsp(settings.m_useDSP ? 1 : 0);
    response.getAirspyHfSettings()->setUseLna(settings.m_useLNA ? 1 : 0);
    response.getAirspyHfSettings()->setAgcHigh(settings.m_agcHigh ? 1 : 0);
    response.getAirspyHfSettings()->setAttenuatorSteps(settings.m_attenuatorSteps);
    response.getAirspyHfSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getAirspyHfSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
}

void AirspyHFInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getAirspyHfReport()->setSampleRates(new QList<SWGSDRangel::SWGSampleRate*>);

    for (std::vector<uint32_t>::const_iterator it = getSampleRates().begin(); it != getSampleRates().end(); ++it)
    {
        response.getAirspyHfReport()->getSampleRates()->append(new SWGSDRangel::SWGSampleRate);
        response.getAirspyHfReport()->getSampleRates()->back()->setRate(*it);
    }
}

int AirspyHFInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAirspyHfReport(new SWGSDRangel::SWGAirspyHFReport());
    response.getAirspyHfReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int AirspyHFInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int AirspyHFInput::webapiRun(
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

void AirspyHFInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AirspyHFSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AirspyHF"));
    swgDeviceSettings->setAirspyHfSettings(new SWGSDRangel::SWGAirspyHFSettings());
    SWGSDRangel::SWGAirspyHFSettings *swgAirspyHFSettings = swgDeviceSettings->getAirspyHfSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgAirspyHFSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("devSampleRateIndex") || force) {
        swgAirspyHFSettings->setDevSampleRateIndex(settings.m_devSampleRateIndex);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgAirspyHFSettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgAirspyHFSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgAirspyHFSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgAirspyHFSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgAirspyHFSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("bandIndex") || force) {
        swgAirspyHFSettings->setBandIndex(settings.m_bandIndex);
    }
    if (deviceSettingsKeys.contains("useAGC")) {
        swgAirspyHFSettings->setUseAgc(settings.m_useAGC ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("agcHigh")) {
        swgAirspyHFSettings->setAgcHigh(settings.m_agcHigh ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("useDSP")) {
        swgAirspyHFSettings->setUseDsp(settings.m_useDSP ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("useLNA")) {
        swgAirspyHFSettings->setUseLna(settings.m_useLNA ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("attenuatorSteps")) {
        swgAirspyHFSettings->setAttenuatorSteps(settings.m_attenuatorSteps);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgAirspyHFSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgAirspyHFSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
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

void AirspyHFInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AirspyHF"));

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

void AirspyHFInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AirspyHFInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AirspyHFInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
