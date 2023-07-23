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
#include <QList>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGAirspyReport.h"

#include "airspyinput.h"
#include "airspyplugin.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "airspysettings.h"
#include "airspyworker.h"
#ifdef ANDROID
#include "util/android.h"
#endif

MESSAGE_CLASS_DEFINITION(AirspyInput::MsgConfigureAirspy, Message)
MESSAGE_CLASS_DEFINITION(AirspyInput::MsgStartStop, Message)

const qint64 AirspyInput::loLowLimitFreq = 24000000L;
const qint64 AirspyInput::loHighLimitFreq = 1900000000L;

AirspyInput::AirspyInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_dev(nullptr),
	m_airspyWorker(nullptr),
    m_airspyWorkerThread(nullptr),
	m_deviceDescription("Airspy"),
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
        &AirspyInput::networkManagerFinished
    );
}

AirspyInput::~AirspyInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AirspyInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }

    closeDevice();
}

void AirspyInput::destroy()
{
    delete this;
}

bool AirspyInput::openDevice()
{
    if (m_dev != 0)
    {
        closeDevice();
    }

    airspy_error rc;

    rc = (airspy_error) airspy_init();

    if (rc != AIRSPY_SUCCESS)
    {
        qCritical("AirspyInput::start: failed to initiate Airspy library %s", airspy_error_name(rc));
    }

    if (!m_sampleFifo.setSize(1<<19))
    {
        qCritical("AirspyInput::start: could not allocate SampleFifo");
        return false;
    }

#ifdef ANDROID
    int fd;
    QString serial = m_deviceAPI->getSamplingDeviceSerial();
    if ((fd = Android::openUSBDevice(serial)) < 0)
    {
        qCritical("AirspyInput::openDevice: could not open USB device %s", qPrintable(serial));
        return false;
    }
    if (airspy_open_fd(&m_dev, fd) != AIRSPY_SUCCESS)
    {
        qCritical("AirspyInput::openDevice: could not open Airspy: %s", qPrintable(serial));
        return false;
    }
#else
    int device = m_deviceAPI->getSamplingDeviceSequence();

    if ((m_dev = open_airspy_from_sequence(device)) == 0)
    {
        qCritical("AirspyInput::start: could not open Airspy #%d", device);
        return false;
    }
#endif

#ifdef LIBAIRSPY_DEFAULT_RATES
    qDebug("AirspyInput::start: default rates");
    m_sampleRates.clear();
    m_sampleRates.push_back(10000000);
    m_sampleRates.push_back(2500000);
#else
    uint32_t nbSampleRates;
    uint32_t *sampleRates;

    airspy_get_samplerates(m_dev, &nbSampleRates, 0);

    sampleRates = new uint32_t[nbSampleRates];

    airspy_get_samplerates(m_dev, sampleRates, nbSampleRates);

    if (nbSampleRates == 0)
    {
        qCritical("AirspyInput::start: could not obtain Airspy sample rates");
        return false;
    }
    else
    {
        qDebug("AirspyInput::start: %d sample rates", nbSampleRates);
    }

    m_sampleRates.clear();

    for (unsigned int i=0; i<nbSampleRates; i++)
    {
        m_sampleRates.push_back(sampleRates[i]);
        qDebug("AirspyInput::start: sampleRates[%d] = %u Hz", i, sampleRates[i]);
    }

    delete[] sampleRates;
#endif

//    MsgReportAirspy *message = MsgReportAirspy::create(m_sampleRates);
//    getOutputMessageQueueToGUI()->push(message);

    rc = (airspy_error) airspy_set_sample_type(m_dev, AIRSPY_SAMPLE_INT16_IQ);

    if (rc != AIRSPY_SUCCESS)
    {
        qCritical("AirspyInput::start: could not set sample type to INT16_IQ");
        return false;
    }

    return true;
}

void AirspyInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool AirspyInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_dev) {
        return false;
    }

    if (m_running) {
        return true;
    }

    m_airspyWorkerThread = new QThread();
	m_airspyWorker = new AirspyWorker(m_dev, &m_sampleFifo);
    m_airspyWorker->moveToThread(m_airspyWorkerThread);

    QObject::connect(m_airspyWorkerThread, &QThread::started, m_airspyWorker, &AirspyWorker::startWork);
    QObject::connect(m_airspyWorkerThread, &QThread::finished, m_airspyWorker, &QObject::deleteLater);
    QObject::connect(m_airspyWorkerThread, &QThread::finished, m_airspyWorkerThread, &QThread::deleteLater);

	m_airspyWorker->setSamplerate(m_sampleRates[m_settings.m_devSampleRateIndex]);
	m_airspyWorker->setLog2Decimation(m_settings.m_log2Decim);
    m_airspyWorker->setIQOrder(m_settings.m_iqOrder);
	m_airspyWorker->setFcPos((int) m_settings.m_fcPos);
    mutexLocker.unlock();

    m_airspyWorkerThread->start();

    qDebug("AirspyInput::startInput: started");
    applySettings(m_settings, QList<QString>(), true);
    m_running = true;

	return true;
}

void AirspyInput::closeDevice()
{
    if (m_dev)
    {
        airspy_stop_rx(m_dev);
        airspy_close(m_dev);
        m_dev = nullptr;
    }

    m_deviceDescription.clear();
    airspy_exit();
}

void AirspyInput::stop()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

	qDebug("AirspyInput::stop");

	if (m_airspyWorkerThread)
	{
        m_airspyWorkerThread->quit();
        m_airspyWorkerThread->wait();
        m_airspyWorkerThread = nullptr;
        m_airspyWorker = nullptr;
	}

	m_running = false;
}

QByteArray AirspyInput::serialize() const
{
    return m_settings.serialize();
}

bool AirspyInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureAirspy* message = MsgConfigureAirspy::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAirspy* messageToGUI = MsgConfigureAirspy::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& AirspyInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int AirspyInput::getSampleRate() const
{
	int rate = m_sampleRates[m_settings.m_devSampleRateIndex];
	return (rate / (1<<m_settings.m_log2Decim));
}

quint64 AirspyInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void AirspyInput::setCenterFrequency(qint64 centerFrequency)
{
    AirspySettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureAirspy* message = MsgConfigureAirspy::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAirspy* messageToGUI = MsgConfigureAirspy::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool AirspyInput::handleMessage(const Message& message)
{
	if (MsgConfigureAirspy::match(message))
	{
		MsgConfigureAirspy& conf = (MsgConfigureAirspy&) message;
		qDebug() << "AirspyInput::handleMessage: MsgConfigureAirspy";

		bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

		if (!success) {
			qDebug("AirspyInput::handleMessage: Airspy config error");
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "AirspyInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

void AirspyInput::setDeviceCenterFrequency(quint64 freq_hz)
{
	qint64 df = ((qint64)freq_hz * m_settings.m_LOppmTenths) / 10000000LL;
	freq_hz += df;

	airspy_error rc = (airspy_error) airspy_set_freq(m_dev, static_cast<uint32_t>(freq_hz));

	if (rc != AIRSPY_SUCCESS) {
		qWarning("AirspyInput::setDeviceCenterFrequency: could not frequency to %llu Hz", freq_hz);
	} else {
		qDebug("AirspyInput::setDeviceCenterFrequency: frequency set to %llu Hz", freq_hz);
	}
}

bool AirspyInput::applySettings(const AirspySettings& settings, const QList<QString>& settingsKeys, bool force)
{
	QMutexLocker mutexLocker(&m_mutex);

	bool forwardChange = false;
	airspy_error rc = AIRSPY_ERROR_OTHER;

	qDebug() << "AirspyInput::applySettings:"
        << "force:" << force
        << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("dcBlock")) ||
        (settingsKeys.contains("iqCorrection")) || force)
    {
		m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
	}

	if ((settingsKeys.contains("devSampleRateIndex")) || force)
	{
		forwardChange = true;

		if (m_dev != 0)
		{
			rc = (airspy_error) airspy_set_samplerate(m_dev, static_cast<airspy_samplerate_t>(settings.m_devSampleRateIndex));

			if (rc != AIRSPY_SUCCESS)
			{
				qCritical("AirspyInput::applySettings: could not set sample rate index %u (%d S/s): %s", settings.m_devSampleRateIndex, m_sampleRates[settings.m_devSampleRateIndex], airspy_error_name(rc));
			}
			else if (m_airspyWorker)
			{
				qDebug("AirspyInput::applySettings: sample rate set to index: %u (%d S/s)", settings.m_devSampleRateIndex, m_sampleRates[settings.m_devSampleRateIndex]);
				m_airspyWorker->setSamplerate(m_sampleRates[settings.m_devSampleRateIndex]);
			}
		}
	}

	if ((settingsKeys.contains("log2Decim")) || force)
	{
		forwardChange = true;

		if (m_airspyWorker)
		{
			m_airspyWorker->setLog2Decimation(settings.m_log2Decim);
			qDebug() << "AirspyInput: set decimation to " << (1<<settings.m_log2Decim);
		}
	}

	if ((settingsKeys.contains("iqOrder")) || force)
	{
		if (m_airspyWorker) {
			m_airspyWorker->setIQOrder(settings.m_iqOrder);
		}
	}

	if ((settingsKeys.contains("centerFrequency"))
        || (settingsKeys.contains("transverterDeltaFrequency"))
        || (settingsKeys.contains("log2Decim"))
        || (settingsKeys.contains("fcPos"))
        || (settingsKeys.contains("devSampleRateIndex"))
        || (settingsKeys.contains("transverterMode")) || force)
	{
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                settings.m_transverterDeltaFrequency,
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                m_sampleRates[settings.m_devSampleRateIndex],
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                settings.m_transverterMode);

		if (m_dev != 0) {
			setDeviceCenterFrequency(deviceCenterFrequency);
		}

		forwardChange = true;
	}

	if ((settingsKeys.contains("fcPos")) || force)
	{
		if (m_airspyWorker)
		{
			m_airspyWorker->setFcPos((int) settings.m_fcPos);
			qDebug() << "AirspyInput: set fc pos (enum) to " << (int) settings.m_fcPos;
		}
	}

	if (settingsKeys.contains("lnaGain") || force)
	{
		if (m_dev != 0)
		{
			rc = (airspy_error) airspy_set_lna_gain(m_dev, settings.m_lnaGain);

			if (rc != AIRSPY_SUCCESS) {
				qDebug("AirspyInput::applySettings: airspy_set_lna_gain failed: %s", airspy_error_name(rc));
			} else {
				qDebug() << "AirspyInput:applySettings: LNA gain set to " << settings.m_lnaGain;
			}
		}
	}

	if (settingsKeys.contains("lnaAGC") || force)
	{
		if (m_dev != 0) {
			rc = (airspy_error) airspy_set_lna_agc(m_dev, (settings.m_lnaAGC ? 1 : 0));
		}

		if (rc != AIRSPY_SUCCESS) {
			qDebug("AirspyInput::applySettings: airspy_set_lna_agc failed: %s", airspy_error_name(rc));
		} else {
			qDebug() << "AirspyInput:applySettings: LNA AGC set to " << settings.m_lnaAGC;
		}
	}

	if (settingsKeys.contains("mixerGain") || force)
	{
		if (m_dev != 0)
		{
			rc = (airspy_error) airspy_set_mixer_gain(m_dev, settings.m_mixerGain);

			if (rc != AIRSPY_SUCCESS) {
				qDebug("AirspyInput::applySettings: airspy_set_mixer_gain failed: %s", airspy_error_name(rc));
			} else {
				qDebug() << "AirspyInput:applySettings: mixer gain set to " << settings.m_mixerGain;
			}
		}
	}

	if (settingsKeys.contains("mixerAGC") || force)
	{
		if (m_dev != 0) {
			rc = (airspy_error) airspy_set_mixer_agc(m_dev, (settings.m_mixerAGC ? 1 : 0));
		}

		if (rc != AIRSPY_SUCCESS) {
			qDebug("AirspyInput::applySettings: airspy_set_mixer_agc failed: %s", airspy_error_name(rc));
		} else {
			qDebug() << "AirspyInput:applySettings: Mixer AGC set to " << settings.m_mixerAGC;
		}
	}

	if (settingsKeys.contains("vgaGain") || force)
	{
		if (m_dev != 0)
		{
			rc = (airspy_error) airspy_set_vga_gain(m_dev, settings.m_vgaGain);

			if (rc != AIRSPY_SUCCESS) {
				qDebug("AirspyInput::applySettings: airspy_set_vga_gain failed: %s", airspy_error_name(rc));
			} else {
				qDebug() << "AirspyInput:applySettings: VGA gain set to " << settings.m_vgaGain;
			}
		}
	}

	if (settingsKeys.contains("biasT") || force)
	{
		if (m_dev != 0)
		{
			rc = (airspy_error) airspy_set_rf_bias(m_dev, (settings.m_biasT ? 1 : 0));

			if (rc != AIRSPY_SUCCESS) {
				qDebug("AirspyInput::applySettings: airspy_set_rf_bias failed: %s", airspy_error_name(rc));
			} else {
				qDebug() << "AirspyInput:applySettings: bias tee set to " << settings.m_biasT;
			}
		}
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
		int sampleRate = m_sampleRates[m_settings.m_devSampleRateIndex]/(1<<m_settings.m_log2Decim);
		DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

	return true;
}

struct airspy_device *AirspyInput::open_airspy_from_sequence(int sequence)
{
	struct airspy_device *devinfo;
	airspy_error rc = AIRSPY_ERROR_OTHER;

	for (int i = 0; i < AirspyPlugin::m_maxDevices; i++)
	{
		rc = (airspy_error) airspy_open(&devinfo);

		if (rc == AIRSPY_SUCCESS)
		{
			if (i == sequence) {
				return devinfo;
			} else {
			    airspy_close(devinfo);
			}
		}
		else
		{
			break;
		}
	}

	return 0;
}

int AirspyInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int AirspyInput::webapiRun(
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

int AirspyInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setAirspySettings(new SWGSDRangel::SWGAirspySettings());
    response.getAirspySettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int AirspyInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    AirspySettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureAirspy *msg = MsgConfigureAirspy::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAirspy *msgToGUI = MsgConfigureAirspy::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void AirspyInput::webapiUpdateDeviceSettings(
        AirspySettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getAirspySettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getAirspySettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("devSampleRateIndex")) {
        settings.m_devSampleRateIndex = response.getAirspySettings()->getDevSampleRateIndex();
    }
    if (deviceSettingsKeys.contains("lnaGain")) {
        settings.m_lnaGain = response.getAirspySettings()->getLnaGain();
    }
    if (deviceSettingsKeys.contains("mixerGain")) {
        settings.m_mixerGain = response.getAirspySettings()->getMixerGain();
    }
    if (deviceSettingsKeys.contains("vgaGain")) {
        settings.m_vgaGain = response.getAirspySettings()->getVgaGain();
    }
    if (deviceSettingsKeys.contains("lnaAGC")) {
        settings.m_lnaAGC = response.getAirspySettings()->getLnaAgc() != 0;
    }
    if (deviceSettingsKeys.contains("mixerAGC")) {
        settings.m_mixerAGC = response.getAirspySettings()->getMixerAgc() != 0;
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getAirspySettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getAirspySettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        int fcPos = response.getAirspySettings()->getFcPos();
        fcPos = fcPos < 0 ? 0 : fcPos > 2 ? 2 : fcPos;
        settings.m_fcPos = (AirspySettings::fcPos_t) fcPos;
    }
    if (deviceSettingsKeys.contains("biasT")) {
        settings.m_biasT = response.getAirspySettings()->getBiasT() != 0;
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getAirspySettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getAirspySettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getAirspySettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getAirspySettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAirspySettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAirspySettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAirspySettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAirspySettings()->getReverseApiDeviceIndex();
    }
}

int AirspyInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAirspyReport(new SWGSDRangel::SWGAirspyReport());
    response.getAirspyReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void AirspyInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const AirspySettings& settings)
{
    response.getAirspySettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getAirspySettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getAirspySettings()->setDevSampleRateIndex(settings.m_devSampleRateIndex);
    response.getAirspySettings()->setLnaGain(settings.m_lnaGain);
    response.getAirspySettings()->setMixerGain(settings.m_mixerGain);
    response.getAirspySettings()->setVgaGain(settings.m_vgaGain);
    response.getAirspySettings()->setLnaAgc(settings.m_lnaAGC ? 1 : 0);
    response.getAirspySettings()->setMixerAgc(settings.m_mixerAGC ? 1 : 0);
    response.getAirspySettings()->setLog2Decim(settings.m_log2Decim);
    response.getAirspySettings()->setIqOrder(settings.m_iqOrder ? 1 : 0);
    response.getAirspySettings()->setFcPos((int) settings.m_fcPos);
    response.getAirspySettings()->setBiasT(settings.m_biasT ? 1 : 0);
    response.getAirspySettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getAirspySettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getAirspySettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getAirspySettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    response.getAirspySettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAirspySettings()->getReverseApiAddress()) {
        *response.getAirspySettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAirspySettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAirspySettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAirspySettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void AirspyInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getAirspyReport()->setSampleRates(new QList<SWGSDRangel::SWGSampleRate*>);

    for (std::vector<uint32_t>::const_iterator it = getSampleRates().begin(); it != getSampleRates().end(); ++it)
    {
        response.getAirspyReport()->getSampleRates()->append(new SWGSDRangel::SWGSampleRate);
        response.getAirspyReport()->getSampleRates()->back()->setRate(*it);
    }
}

void AirspyInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AirspySettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("Airspy"));
    swgDeviceSettings->setAirspySettings(new SWGSDRangel::SWGAirspySettings());
    SWGSDRangel::SWGAirspySettings *swgAirspySettings = swgDeviceSettings->getAirspySettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgAirspySettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("LOppmTenths") || force) {
        swgAirspySettings->setLOppmTenths(settings.m_LOppmTenths);
    }
    if (deviceSettingsKeys.contains("devSampleRateIndex") || force) {
        swgAirspySettings->setDevSampleRateIndex(settings.m_devSampleRateIndex);
    }
    if (deviceSettingsKeys.contains("lnaGain") || force) {
        swgAirspySettings->setLnaGain(settings.m_lnaGain);
    }
    if (deviceSettingsKeys.contains("mixerGain") || force) {
        swgAirspySettings->setMixerGain(settings.m_mixerGain);
    }
    if (deviceSettingsKeys.contains("vgaGain") || force) {
        swgAirspySettings->setVgaGain(settings.m_vgaGain);
    }
    if (deviceSettingsKeys.contains("lnaAGC") || force) {
        swgAirspySettings->setLnaAgc(settings.m_lnaAGC ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("mixerAGC") || force) {
        swgAirspySettings->setMixerAgc(settings.m_mixerAGC ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgAirspySettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqOrder") || force) {
        swgAirspySettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgAirspySettings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("biasT") || force) {
        swgAirspySettings->setBiasT(settings.m_biasT ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgAirspySettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgAirspySettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency") || force) {
        swgAirspySettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode") || force) {
        swgAirspySettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
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

void AirspyInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("Airspy"));

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

void AirspyInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AirspyInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AirspyInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
