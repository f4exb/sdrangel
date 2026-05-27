///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2020, 2022-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2018 beta-tester <alpha-beta-release@gmx.net>                   //
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
#include <cstdarg>
#include <cstdio>

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QBuffer>
#include <QThread>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "FOBOSinput.h"
#include "device/deviceapi.h"
#include "FOBOSworker.h"
#include "dsp/dspcommands.h"

MESSAGE_CLASS_DEFINITION(FOBOSInput::MsgConfigureFOBOS, Message)
MESSAGE_CLASS_DEFINITION(FOBOSInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(FOBOSInput::MsgReportFOBOSBackend, Message)

namespace
{
    static const char* kFobosInputLogPath = "sdrangel_fobos_source.log";

    static void fobosInputLog(const char* fmt, ...)
    {
        FILE* f = std::fopen(kFobosInputLogPath, "a");
        if (!f) {
            return;
        }
        va_list ap;
        va_start(ap, fmt);
        std::vfprintf(f, fmt, ap);
        va_end(ap);
        std::fprintf(f, "\n");
        std::fclose(f);
    }

    static const char* boolText(bool v)
    {
        return v ? "true" : "false";
    }
}


FOBOSInput::FOBOSInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
	m_worker(nullptr),
    m_workerThread(nullptr),
	m_deviceDescription("FOBOSInput"),
	m_running(false),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FOBOSInput::networkManagerFinished
    );
}

FOBOSInput::~FOBOSInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FOBOSInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }
}

void FOBOSInput::destroy()
{
    delete this;
}

void FOBOSInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool FOBOSInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return true;
    }

    if (!m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(m_settings.m_sampleRate)))
    {
        qCritical("FOBOSInput::FOBOSInput: Could not allocate SampleFifo");
        return false;
    }

    m_workerThread = new QThread();
    m_worker = new FOBOSWorker(&m_sampleFifo);
    m_worker->moveToThread(m_workerThread);

    QObject::connect(m_workerThread, &QThread::started, m_worker, &FOBOSWorker::startWork);
    QObject::connect(m_worker, &FOBOSWorker::backendStatusChanged, this, &FOBOSInput::handleWorkerBackendStatus, Qt::QueuedConnection);
    QObject::connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater, Qt::QueuedConnection);
    QObject::connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater);

	m_worker->setSamplerate(m_settings.m_sampleRate);
    // FOBOS_START_SETTINGS_HANDOFF
    m_worker->setCenterFrequency(m_settings.m_centerFrequency);
    m_worker->setLog2Decimation(m_settings.m_log2Decim);
    m_worker->setFcPos(static_cast<int>(m_settings.m_fcPos));
    m_worker->setFrequencyShift(m_settings.m_frequencyShift);
    m_worker->setAmplitudeBits(m_settings.m_amplitudeBits);
    m_worker->setLnaGain(static_cast<unsigned int>(m_settings.m_autoCorrOptions));
    m_worker->setModulation(static_cast<int>(m_settings.m_modulation));
    m_worker->setInputMode(static_cast<int>(m_settings.m_inputMode));
    m_worker->setBandwidthPercent(m_settings.m_bandwidthPercent);
    m_worker->setGpoMask(m_settings.m_gpoMask);
    m_worker->setExternalClock(m_settings.m_externalClock);
    m_workerThread->start();
	m_running = true;

	mutexLocker.unlock();

	applySettings(m_settings, QList<QString>(), true);

	return true;
}

void FOBOSInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

    m_running = false;

    if (m_workerThread)
    {
        m_worker->stopWork();
        m_workerThread->quit();
        m_workerThread->wait();
        m_worker = nullptr;
        m_workerThread = nullptr;
    }
}

QByteArray FOBOSInput::serialize() const
{
    return m_settings.serialize();
}

bool FOBOSInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureFOBOS* message = MsgConfigureFOBOS::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFOBOS* messageToGUI = MsgConfigureFOBOS::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& FOBOSInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FOBOSInput::getSampleRate() const
{
	return m_settings.m_sampleRate/(1<<m_settings.m_log2Decim);
}

quint64 FOBOSInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void FOBOSInput::setCenterFrequency(qint64 centerFrequency)
{
    FOBOSSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureFOBOS* message = MsgConfigureFOBOS::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFOBOS* messageToGUI = MsgConfigureFOBOS::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool FOBOSInput::handleMessage(const Message& message)
{
    if (MsgConfigureFOBOS::match(message))
    {
        MsgConfigureFOBOS& conf = (MsgConfigureFOBOS&) message;
        qDebug() << "FOBOSInput::handleMessage: MsgConfigureFOBOS";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success)
        {
            qDebug("FOBOSInput::handleMessage: config error");
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "FOBOSInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine())
            {
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
    else if (MsgReportFOBOSBackend::match(message))
    {
        const MsgReportFOBOSBackend& report = (const MsgReportFOBOSBackend&) message;
        if (m_guiMessageQueue) {
            MsgReportFOBOSBackend* msgToGUI = MsgReportFOBOSBackend::create(report.getBackend(), report.getDetails());
            m_guiMessageQueue->push(msgToGUI);
        }
        return true;
    }
    else
    {
        return false;
    }
}


void FOBOSInput::handleWorkerBackendStatus(const QString& backend, const QString& details)
{
    if (m_guiMessageQueue) {
        MsgReportFOBOSBackend* msgToGUI = MsgReportFOBOSBackend::create(backend, details);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool FOBOSInput::applySettings(const FOBOSSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "FOBOSInput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);

    const bool criticalRestartRequired = m_running && !force &&
        (settingsKeys.contains("sampleRate") ||
         settingsKeys.contains("inputMode") ||
         settingsKeys.contains("externalClock"));

    if (criticalRestartRequired)
    {
        QStringList keyStrings;
        for (const QString& key : settingsKeys) {
            keyStrings.append(key);
        }
        const QByteArray keyBytes = keyStrings.join(QStringLiteral(",")).toUtf8();

        fobosInputLog("controlled_restart=START reason=critical_settings_changed keys='%s' old_sr=%u new_sr=%u old_input=%d new_input=%d old_external_clock=%s new_external_clock=%s",
                          keyBytes.constData(),
                          static_cast<unsigned int>(m_settings.m_sampleRate),
                          static_cast<unsigned int>(settings.m_sampleRate),
                          static_cast<int>(m_settings.m_inputMode),
                          static_cast<int>(settings.m_inputMode),
                          boolText(m_settings.m_externalClock),
                          boolText(settings.m_externalClock));

        FOBOSSettings restartSettings = m_settings;
        restartSettings.applySettings(settingsKeys, settings);
        m_settings = restartSettings;

        const int sampleRate = m_settings.m_sampleRate/(1<<m_settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        stop();
        fobosInputLog("controlled_restart=AFTER_STOP");

        const bool restartOk = start();
        fobosInputLog("controlled_restart=RETURN result=%s new_sr=%u new_input=%d new_external_clock=%s",
                          restartOk ? "OK" : "FAILED",
                          static_cast<unsigned int>(m_settings.m_sampleRate),
                          static_cast<int>(m_settings.m_inputMode),
                          boolText(m_settings.m_externalClock));
        return restartOk;
    }

    if (settingsKeys.contains("autoCorrOptions") || force)
    {
        switch(settings.m_autoCorrOptions)
        {
        case FOBOSSettings::AutoCorrDC:
            m_deviceAPI->configureCorrections(true, false);
            break;
        case FOBOSSettings::AutoCorrDCAndIQ:
            m_deviceAPI->configureCorrections(true, true);
            break;
        case FOBOSSettings::AutoCorrNone:
        default:
            m_deviceAPI->configureCorrections(false, false);
            break;
        }

        if (m_worker != 0) {
            m_worker->setLnaGain(static_cast<unsigned int>(settings.m_autoCorrOptions));
        }
    }

    if (settingsKeys.contains("sampleRate") || force)
    {
        if (m_worker != 0)
        {
            m_worker->setSamplerate(settings.m_sampleRate);
            qDebug("FOBOSInput::applySettings: sample rate set to %d", settings.m_sampleRate);
        }
    }

    if (settingsKeys.contains("log2Decim") || force)
    {
        if (m_worker != 0)
        {
            m_worker->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "FOBOSInput::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if (settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("fcPos")
        || settingsKeys.contains("frequencyShift")
        || settingsKeys.contains("sampleRate")
        || settingsKeys.contains("log2Decim") || force)
    {
        qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                settings.m_centerFrequency,
                0, // no transverter mode
                settings.m_log2Decim,
                (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                settings.m_sampleRate,
                DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                false);

        int frequencyShift = settings.m_frequencyShift;
        quint32 devSampleRate = settings.m_sampleRate;

        if (settings.m_log2Decim != 0)
        {
            frequencyShift += DeviceSampleSource::calculateFrequencyShift(
                    settings.m_log2Decim,
                    (DeviceSampleSource::fcPos_t) settings.m_fcPos,
                    settings.m_sampleRate,
                    DeviceSampleSource::FSHIFT_STD);
        }

        if (m_worker != 0)
        {
            m_worker->setFcPos((int) settings.m_fcPos);
            m_worker->setFrequencyShift(frequencyShift);
            // FOBOS_APPLY_CENTER_HANDOFF
            m_worker->setCenterFrequency(deviceCenterFrequency);
            qDebug() << "FOBOSInput::applySettings:"
                    << " center freq: " << settings.m_centerFrequency << " Hz"
                    << " device center freq: " << deviceCenterFrequency << " Hz"
                    << " device sample rate: " << devSampleRate << "Hz"
                    << " Actual sample rate: " << devSampleRate/(1<<m_settings.m_log2Decim) << "Hz"
                    << " f shift: " << settings.m_frequencyShift;
        }
    }

    if (settingsKeys.contains("amplitudeBits") || force)
    {
        if (m_worker != 0) {
            m_worker->setAmplitudeBits(settings.m_amplitudeBits);
        }
    }

    if (settingsKeys.contains("dcFactor") || force)
    {
        if (m_worker != 0) {
            m_worker->setDCFactor(settings.m_dcFactor);
        }
    }

    if (settingsKeys.contains("iFactor") || force)
    {
        if (m_worker != 0) {
            m_worker->setIFactor(settings.m_iFactor);
        }
    }

    if (settingsKeys.contains("qFactor") || force)
    {
        if (m_worker != 0) {
            m_worker->setQFactor(settings.m_qFactor);
        }
    }

    if (settingsKeys.contains("phaseImbalance") || force)
    {
        if (m_worker != 0) {
            m_worker->setPhaseImbalance(settings.m_phaseImbalance);
        }
    }

    if (settingsKeys.contains("sampleSizeIndex") || force)
    {
        if (m_worker != 0) {
            m_worker->setBitSize(settings.m_sampleSizeIndex);
        }
    }

    // if ((m_settings.m_sampleRate != settings.m_sampleRate)
    //     || (m_settings.m_centerFrequency != settings.m_centerFrequency)
    //     || (m_settings.m_log2Decim != settings.m_log2Decim)
    //     || (m_settings.m_fcPos != settings.m_fcPos) || force)
    if (settingsKeys.contains("sampleRate")
        || settingsKeys.contains("centerFrequency")
        || settingsKeys.contains("log2Decim")
        || settingsKeys.contains("fcPos") || force)
    {
        int sampleRate = settings.m_sampleRate/(1<<settings.m_log2Decim);
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (settingsKeys.contains("inputMode") || force)
    {
        if (m_worker != 0) {
            m_worker->setInputMode(static_cast<int>(settings.m_inputMode));
        }
    }

    if (settingsKeys.contains("bandwidthPercent") || force)
    {
        if (m_worker != 0) {
            m_worker->setBandwidthPercent(settings.m_bandwidthPercent);
        }
    }

    if (settingsKeys.contains("gpoMask") || force)
    {
        if (m_worker != 0) {
            m_worker->setGpoMask(settings.m_gpoMask);
        }
    }

    if (settingsKeys.contains("externalClock") || force)
    {
        if (m_worker != 0) {
            m_worker->setExternalClock(settings.m_externalClock);
        }
    }

    if (settingsKeys.contains("modulationTone") || force)
    {
        if (m_worker != 0) {
            m_worker->setToneFrequency(settings.m_modulationTone * 10);
        }
    }

    if (settingsKeys.contains("modulation") || force)
    {
        if (m_worker != 0)
        {
            m_worker->setModulation(settings.m_modulation);

            if (settings.m_modulation == FOBOSSettings::ModulationPattern0) {
                m_worker->setPattern0();
            } else if (settings.m_modulation == FOBOSSettings::ModulationPattern1) {
                m_worker->setPattern1();
            } else if (settings.m_modulation == FOBOSSettings::ModulationPattern2) {
                m_worker->setPattern2();
            }
        }
    }

    if (settingsKeys.contains("amModulation") || force)
    {
        if (m_worker != 0) {
            m_worker->setAMModulation(settings.m_amModulation / 100.0f);
        }
    }

    if (settingsKeys.contains("fmDeviation") || force)
    {
        if (m_worker != 0) {
            m_worker->setFMDeviation(settings.m_fmDeviation * 100.0f);
        }
    }

    if (settings.m_useReverseAPI)
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

int FOBOSInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int FOBOSInput::webapiRun(
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

int FOBOSInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setTestSourceSettings(new SWGSDRangel::SWGTestSourceSettings());
    response.getTestSourceSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int FOBOSInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    FOBOSSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureFOBOS *msg = MsgConfigureFOBOS::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFOBOS *msgToGUI = MsgConfigureFOBOS::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void FOBOSInput::webapiUpdateDeviceSettings(
    FOBOSSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("title")) {
        settings.m_title = *response.getTestSourceSettings()->getTitle();
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getTestSourceSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("frequencyShift")) {
        settings.m_frequencyShift = response.getTestSourceSettings()->getFrequencyShift();
    }
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getTestSourceSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getTestSourceSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        int fcPos = response.getTestSourceSettings()->getFcPos();
        fcPos = fcPos < 0 ? 0 : fcPos > 2 ? 2 : fcPos;
        settings.m_fcPos = (FOBOSSettings::fcPos_t) fcPos;
    }
    if (deviceSettingsKeys.contains("sampleSizeIndex")) {
        int sampleSizeIndex = response.getTestSourceSettings()->getSampleSizeIndex();
        sampleSizeIndex = sampleSizeIndex < 0 ? 0 : sampleSizeIndex > 1 ? 2 : sampleSizeIndex;
        settings.m_sampleSizeIndex = sampleSizeIndex;
    }
    if (deviceSettingsKeys.contains("amplitudeBits")) {
        settings.m_amplitudeBits = response.getTestSourceSettings()->getAmplitudeBits();
    }
    if (deviceSettingsKeys.contains("autoCorrOptions")) {
        int autoCorrOptions = response.getTestSourceSettings()->getAutoCorrOptions();
        autoCorrOptions = autoCorrOptions < 0 ? 0 : autoCorrOptions >= FOBOSSettings::AutoCorrLast ? FOBOSSettings::AutoCorrLast-1 : autoCorrOptions;
        settings.m_sampleSizeIndex = (FOBOSSettings::AutoCorrOptions) autoCorrOptions;
    }
    if (deviceSettingsKeys.contains("modulation")) {
        int modulation = response.getTestSourceSettings()->getModulation();
        modulation = modulation < 0 ? 0 : modulation >= FOBOSSettings::ModulationLast ? FOBOSSettings::ModulationLast-1 : modulation;
        settings.m_modulation = (FOBOSSettings::Modulation) modulation;
    }
    if (deviceSettingsKeys.contains("modulationTone")) {
        settings.m_modulationTone = response.getTestSourceSettings()->getModulationTone();
    }
    if (deviceSettingsKeys.contains("amModulation")) {
        settings.m_amModulation = response.getTestSourceSettings()->getAmModulation();
    };
    if (deviceSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getTestSourceSettings()->getFmDeviation();
    };
    if (deviceSettingsKeys.contains("dcFactor")) {
        settings.m_dcFactor = response.getTestSourceSettings()->getDcFactor();
    };
    if (deviceSettingsKeys.contains("iFactor")) {
        settings.m_iFactor = response.getTestSourceSettings()->getIFactor();
    };
    if (deviceSettingsKeys.contains("qFactor")) {
        settings.m_qFactor = response.getTestSourceSettings()->getQFactor();
    };
    if (deviceSettingsKeys.contains("phaseImbalance")) {
        settings.m_phaseImbalance = response.getTestSourceSettings()->getPhaseImbalance();
    };
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getTestSourceSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getTestSourceSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getTestSourceSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getTestSourceSettings()->getReverseApiDeviceIndex();
    }
}

void FOBOSInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const FOBOSSettings& settings)
{
    if (response.getTestSourceSettings()->getTitle()) {
        *response.getTestSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getTestSourceSettings()->setTitle(new QString(settings.m_title));
    }

    response.getTestSourceSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getTestSourceSettings()->setFrequencyShift(settings.m_frequencyShift);
    response.getTestSourceSettings()->setSampleRate(settings.m_sampleRate);
    response.getTestSourceSettings()->setLog2Decim(settings.m_log2Decim);
    response.getTestSourceSettings()->setFcPos((int) settings.m_fcPos);
    response.getTestSourceSettings()->setSampleSizeIndex((int) settings.m_sampleSizeIndex);
    response.getTestSourceSettings()->setAmplitudeBits(settings.m_amplitudeBits);
    response.getTestSourceSettings()->setAutoCorrOptions((int) settings.m_autoCorrOptions);
    response.getTestSourceSettings()->setModulation((int) settings.m_modulation);
    response.getTestSourceSettings()->setModulationTone(settings.m_modulationTone);
    response.getTestSourceSettings()->setAmModulation(settings.m_amModulation);
    response.getTestSourceSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getTestSourceSettings()->setDcFactor(settings.m_dcFactor);
    response.getTestSourceSettings()->setIFactor(settings.m_iFactor);
    response.getTestSourceSettings()->setQFactor(settings.m_qFactor);
    response.getTestSourceSettings()->setPhaseImbalance(settings.m_phaseImbalance);

    response.getTestSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getTestSourceSettings()->getReverseApiAddress()) {
        *response.getTestSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getTestSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getTestSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getTestSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void FOBOSInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const FOBOSSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("FOBOS"));
    swgDeviceSettings->setTestSourceSettings(new SWGSDRangel::SWGTestSourceSettings());
    SWGSDRangel::SWGTestSourceSettings *SWGTestSourceSettings = swgDeviceSettings->getTestSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("title") || force) {
        SWGTestSourceSettings->setTitle(new QString(settings.m_title));
    }
    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        SWGTestSourceSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("frequencyShift") || force) {
        SWGTestSourceSettings->setFrequencyShift(settings.m_frequencyShift);
    }
    if (deviceSettingsKeys.contains("sampleRate") || force) {
        SWGTestSourceSettings->setSampleRate(settings.m_sampleRate);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        SWGTestSourceSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        SWGTestSourceSettings->setFcPos((int) settings.m_fcPos);
    }
    if (deviceSettingsKeys.contains("sampleSizeIndex") || force) {
        SWGTestSourceSettings->setSampleSizeIndex(settings.m_sampleSizeIndex);
    }
    if (deviceSettingsKeys.contains("amplitudeBits") || force) {
        SWGTestSourceSettings->setAmplitudeBits(settings.m_amplitudeBits);
    }
    if (deviceSettingsKeys.contains("autoCorrOptions") || force) {
        SWGTestSourceSettings->setAutoCorrOptions((int) settings.m_sampleSizeIndex);
    }
    if (deviceSettingsKeys.contains("modulation") || force) {
        SWGTestSourceSettings->setModulation((int) settings.m_modulation);
    }
    if (deviceSettingsKeys.contains("modulationTone")) {
        SWGTestSourceSettings->setModulationTone(settings.m_modulationTone);
    }
    if (deviceSettingsKeys.contains("amModulation") || force) {
        SWGTestSourceSettings->setAmModulation(settings.m_amModulation);
    };
    if (deviceSettingsKeys.contains("fmDeviation") || force) {
        SWGTestSourceSettings->setFmDeviation(settings.m_fmDeviation);
    };
    if (deviceSettingsKeys.contains("dcFactor") || force) {
        SWGTestSourceSettings->setDcFactor(settings.m_dcFactor);
    };
    if (deviceSettingsKeys.contains("iFactor") || force) {
        SWGTestSourceSettings->setIFactor(settings.m_iFactor);
    };
    if (deviceSettingsKeys.contains("qFactor") || force) {
        SWGTestSourceSettings->setQFactor(settings.m_qFactor);
    };
    if (deviceSettingsKeys.contains("phaseImbalance") || force) {
        SWGTestSourceSettings->setPhaseImbalance(settings.m_phaseImbalance);
    };

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgDeviceSettings->asJson().toUtf8());
    buffer->seek(0);
//    qDebug("FOBOSInput::webapiReverseSendSettings: %s", channelSettingsURL.toStdString().c_str());
//    qDebug("FOBOSInput::webapiReverseSendSettings: query:\n%s", swgDeviceSettings->asJson().toStdString().c_str());

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgDeviceSettings;
}

void FOBOSInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("FOBOS"));

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/device/run")
            .arg(m_settings.m_reverseAPIAddress)
            .arg(m_settings.m_reverseAPIPort)
            .arg(m_settings.m_reverseAPIDeviceIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
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

void FOBOSInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FOBOSInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("FOBOSInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
