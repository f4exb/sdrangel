///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <thread>
#include <chrono>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QThread>
#include <QMutexLocker>
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGSpectranMISOSettings.h"

#include "spectran/devicespectran.h"
#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/samplesourcefifo.h"
#include "dsp/samplesinkfifo.h"
#include "spectranmisostreamworker.h"
#include "spectranmiso.h"

MESSAGE_CLASS_DEFINITION(SpectranMISO::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(SpectranMISO::MsgChangeMode, Message)

const QMap<SpectranMISOMode, QString> SpectranMISO::m_modeNames = {
    { SpectranMISOMode::SPECTRANMISO_MODE_RX_IQ, "iqreceiver" },
    { SpectranMISOMode::SPECTRANMISO_MODE_TX_IQ, "iqtransmitter" },
    { SpectranMISOMode::SPECTRANMISO_MODE_RXTX_IQ, "iqtransceiver" },
    { SpectranMISOMode::SPECTRANMISO_MODE_RX_RAW, "raw" },
    { SpectranMISOMode::SPECTRANMISO_MODE_2RX_RAW_INTL, "raw" },
    { SpectranMISOMode::SPECTRANMISO_MODE_2RX_RAW, "raw" }
};

const QMap<SpectranModel, QString> SpectranMISO::m_spectranModelNames = {
    { SpectranModel::SPECTRAN_V6, "spectranv6" },
    { SpectranModel::SPECTRAN_V6Eco, "spectranv6eco" }
};

const QMap<SpectranRxChannel, QString> SpectranMISO::m_rxChannelNames = {
    { SpectranRxChannel::SPECTRAN_RX_CHANNEL_1, "Rx1"},
    { SpectranRxChannel::SPECTRAN_RX_CHANNEL_2, "Rx2"}
};

const QMap<SpectranMISOClockRate, QString> SpectranMISO::m_clockRateNames = {
    { SpectranMISOClockRate::SPECTRANMISO_CLOCK_46M, "46MHz" },
    { SpectranMISOClockRate::SPECTRANMISO_CLOCK_61M, "61MHz" },
    { SpectranMISOClockRate::SPECTRANMISO_CLOCK_76M, "77MHz" },
    { SpectranMISOClockRate::SPECTRANMISO_CLOCK_92M, "92MHz" },
    { SpectranMISOClockRate::SPECTRANMISO_CLOCK_122M, "122MHz" },
    { SpectranMISOClockRate::SPECTRANMISO_CLOCK_245M, "245MHz" }
};

const QMap<unsigned int, QString> SpectranMISO::m_log2DecimationNames = {
    { 0, "Full" },
    { 1, "1 / 2" },
    { 2, "1 / 4" },
    { 3, "1 / 8" },
    { 4, "1 / 16" },
    { 5, "1 / 32" },
    { 6, "1 / 64" },
    { 7, "1 / 128" },
    { 8, "1 / 256" },
    { 9, "1 / 512" },
    { 10, "1 / 1024" } // Do not use, just for range checking
};

SpectranMISO::SpectranMISO(DeviceAPI* deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_deviceDescription("SpectranMISO"),
    m_running(false),
    m_masterTimer(deviceAPI->getMasterTimer()),
    m_networkManager(nullptr)
{
    m_restartMode = m_settings.m_mode;
    m_mimoType = MIMOHalfSynchronous;
    m_sampleMIFifo.init(2, SampleSourceFifo::getSizePolicy(m_settings.m_sampleRate));
    m_sampleMOFifo.init(1, SampleSourceFifo::getSizePolicy(m_settings.m_sampleRate));
    m_deviceAPI->setNbSourceStreams(2);
    m_deviceAPI->setNbSinkStreams(1);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SpectranMISO::networkManagerFinished
    );
    setSpectranModel(m_deviceAPI->getSamplingDeviceDisplayName());
}

SpectranMISO::~SpectranMISO()
{
    stop();
}

void SpectranMISO::destroy()
{
    delete this;
}

bool SpectranMISO::startRx()
{
    return start(m_settings.m_mode);
}

bool SpectranMISO::startTx()
{
    return start(m_settings.m_mode);
}

void SpectranMISO::stopRx()
{
    stop();
}

void SpectranMISO::stopTx()
{
    stop();
}

void SpectranMISO::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

void SpectranMISO::setSpectranModel(const QString& deviceDisplayName)
{
    QString deviceName = deviceDisplayName.split("[").first().trimmed();

    if (deviceName == "SpectranV6") {
        m_spectranModel = SpectranModel::SPECTRAN_V6;
    } else if (deviceName == "SpectranV6Eco") {
        m_spectranModel = SpectranModel::SPECTRAN_V6Eco;
    } else {
        m_spectranModel = SpectranModel::SPECTRAN_V6; // Default to SPECTRAN_V6
    }
}

bool SpectranMISO::start(const SpectranMISOMode& mode)
{
    if (m_running) {
        return true; // Already running
    }

    qDebug("SpectranMISO::start: mode=%s", SpectranMISOSettings::m_modeDisplayNames.value(mode, "Unknown").toStdString().c_str());

    QMutexLocker mutexLocker(&m_mutex);

    if (!startMode(mode)) {
        return false;
    }

    m_streamWorkerThread = new QThread();
    m_streamWorker = new SpectranMISOStreamWorker(&m_sampleMIFifo, &m_sampleMOFifo);
    m_streamWorker->moveToThread(m_streamWorkerThread);
    m_streamWorker->setDevice(&m_device);
    m_streamWorker->setMode(mode);
    m_streamWorker->setMessageQueueToGUI(getMessageQueueToGUI());
    m_streamWorker->setMessageQueueToSISO(getInputMessageQueue());

    QObject::connect(m_streamWorkerThread, &QThread::started, m_streamWorker, &SpectranMISOStreamWorker::startWork);
    QObject::connect(m_streamWorker, &SpectranMISOStreamWorker::stopped, this, &SpectranMISO::streamStopped);
    QObject::connect(m_streamWorker, &SpectranMISOStreamWorker::restart, this, &SpectranMISO::streamStoppedForRestart);
    QObject::connect(m_streamWorkerThread, &QThread::finished, m_streamWorker, &QObject::deleteLater);
    QObject::connect(m_streamWorkerThread, &QThread::finished, m_streamWorkerThread, &QThread::deleteLater);

    m_streamWorkerThread->start();
    m_running = true;

    applySettings(m_settings, QList<QString>(), true);

    return true; // Successfully started
}

void SpectranMISO::stop()
{
    if (!m_running) {
        return; // Not running
    }

    qDebug("SpectranMISO::stop");
    m_streamWorker->stopWork();
}

void SpectranMISO::streamStopped()
{
    qDebug("SpectranMISO::streamStopped");
    QMutexLocker mutexLocker(&m_mutex);

    if (m_streamWorkerThread)
    {
        m_running = false;
        stopMode();
        m_streamWorkerThread->quit();
        qDebug("SpectranMISO::streamStopped: waiting for worker thread to finish");
        m_streamWorkerThread->wait(); // Blocks until the thread finishes
        qDebug("SpectranMISO::streamStopped: worker thread finished");
        m_streamWorkerThread = nullptr;
        m_streamWorker = nullptr;
    }
}

void SpectranMISO::restart(const SpectranMISOMode& mode)
{
    if (!m_running) {
        return; // Not running
    }

    qDebug("SpectranMISO::restart");
    m_restartMode = mode;
    m_streamWorker->restartWork();
}

void SpectranMISO::streamStoppedForRestart()
{
    qDebug("SpectranMISO::streamStoppedForRestart in mode %s",
        SpectranMISOSettings::m_modeDisplayNames.value(m_restartMode, "Unknown").toStdString().c_str());

    if (m_streamWorkerThread)
    {
        m_running = false;
        stopMode();
        m_streamWorkerThread->quit();
        qDebug("SpectranMISO::streamStoppedForRestart: waiting for worker thread to finish");
        m_streamWorkerThread->wait(); // Blocks until the thread finishes
        qDebug("SpectranMISO::streamStoppedForRestart: worker thread finished");
        m_streamWorkerThread = nullptr;
        m_streamWorker = nullptr;
        start(m_restartMode);
    }
}

QByteArray SpectranMISO::serialize() const
{
    return m_settings.serialize();
}

bool SpectranMISO::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureSpectranMISO* message = MsgConfigureSpectranMISO::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSpectranMISO* messageToGUI = MsgConfigureSpectranMISO::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& SpectranMISO::getDeviceDescription() const
{
    return m_deviceDescription;
}

int SpectranMISO::getSourceSampleRate(int index) const
{
    Q_UNUSED(index);
    return getSampleRate(m_settings);
}

int SpectranMISO::getSinkSampleRate(int index) const
{
    Q_UNUSED(index);
    return getSampleRate(m_settings);
}

int SpectranMISO::getSampleRate(const SpectranMISOSettings& settings)
{
    if (settings.m_mode == SPECTRANMISO_MODE_RX_IQ
        || settings.m_mode == SPECTRANMISO_MODE_RXTX_IQ || settings.m_mode == SPECTRANMISO_MODE_TX_IQ)
    {
        return settings.m_sampleRate; // For IQ modes, the sample rate is directly specified
    }
    else if (settings.m_mode == SPECTRANMISO_MODE_2RX_RAW_INTL)
    {
        // For 2x RAW mode, the sample rate is determined by the clock rate only (no decimation)
        return getActualRawRate(settings.m_clockRate);
    }
    else
    {
        // For raw modes, the sample rate is determined by the clock rate and decimation
        return getActualRawRate(settings.m_clockRate) / (1 << settings.m_logDecimation);
    }
}

int SpectranMISO::getActualRawRate(const SpectranMISOClockRate& clockRate)
{
    switch (clockRate) {
        case SPECTRANMISO_CLOCK_46M:
            return 46080000;
        case SPECTRANMISO_CLOCK_61M:
            return 61440000;
        case SPECTRANMISO_CLOCK_76M:
            return 76800000;
        case SPECTRANMISO_CLOCK_92M:
            return 92160000;
        case SPECTRANMISO_CLOCK_122M:
            return 122880000;
        case SPECTRANMISO_CLOCK_245M:
            return 245760000;
        default:
            return 0; // Invalid clock rate
    }
}

void SpectranMISO::setSourceSampleRate(int sampleRate, int index)
{
    Q_UNUSED(index);
    setSampleRate(sampleRate);
}

void SpectranMISO::setSinkSampleRate(int sampleRate, int index)
{
    Q_UNUSED(index);
    setSampleRate(sampleRate);
}

void SpectranMISO::setSampleRate(int sampleRate)
{
    SpectranMISOSettings settings = m_settings;
    settings.m_sampleRate = sampleRate;

    MsgConfigureSpectranMISO* message = MsgConfigureSpectranMISO::create(settings, QList<QString>{"sampleRate"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureSpectranMISO::create(settings, QList<QString>{"sampleRate"}, false));
    }
}

quint64 SpectranMISO::getSourceCenterFrequency(int index) const
{
    Q_UNUSED(index);
    return m_settings.m_rxCenterFrequency;
}

void SpectranMISO::setSourceCenterFrequency(qint64 centerFrequency, int index)
{
    Q_UNUSED(index);
    SpectranMISOSettings settings = m_settings;
    settings.m_rxCenterFrequency = centerFrequency;

    MsgConfigureSpectranMISO* message = MsgConfigureSpectranMISO::create(settings, QList<QString>{"sampleRate"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureSpectranMISO::create(settings, QList<QString>{"sampleRate"}, false));
    }
}

quint64 SpectranMISO::getSinkCenterFrequency(int index) const
{
    Q_UNUSED(index);
    return m_settings.m_txCenterFrequency;
}

void SpectranMISO::setSinkCenterFrequency(qint64 centerFrequency, int index)
{
    Q_UNUSED(index);
    SpectranMISOSettings settings = m_settings;
    settings.m_txCenterFrequency = centerFrequency;

    MsgConfigureSpectranMISO* message = MsgConfigureSpectranMISO::create(settings, QList<QString>{"sampleRate"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureSpectranMISO::create(settings, QList<QString>{"sampleRate"}, false));
    }
}

bool SpectranMISO::handleMessage(const Message& message)
{
    qDebug("SpectranMISO::handleMessage: %s", message.getIdentifier());
    if (MsgConfigureSpectranMISO::match(message))
    {
        const MsgConfigureSpectranMISO& configMessage = static_cast<const MsgConfigureSpectranMISO&>(message);
        const SpectranMISOSettings& settings = configMessage.getSettings();
        const QList<QString>& settingsKeys = configMessage.getSettingsKeys();
        bool force = configMessage.getForce();

        if (!applySettings(settings, settingsKeys, force)) {
            return false; // Failed to apply settings
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SpectranMISO::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            // Start Rx engine
            if (m_deviceAPI->initDeviceEngine(0)) {
                m_deviceAPI->startDeviceEngine(0);
            }

            // Start Tx engine
            if (m_deviceAPI->initDeviceEngine(1)) {
                m_deviceAPI->startDeviceEngine(1);
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine(0); // Stop Rx engine
            m_deviceAPI->stopDeviceEngine(1); // Stop Tx engine
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else if (MsgChangeMode::match(message))
    {
        const MsgChangeMode& cmd = static_cast<const MsgChangeMode&>(message);
        SpectranMISOSettings settings = cmd.getSettings();
        qDebug() << "SpectranMISO::handleMessage: MsgChangeMode: "
            << SpectranMISOSettings::m_modeDisplayNames.value(settings.m_mode, "Unknown")
            << ", RxChannel: " << SpectranMISO::m_rxChannelNames.value(settings.m_rxChannel, "Unknown");

        if (settings.m_mode == m_settings.m_mode && settings.m_rxChannel == m_settings.m_rxChannel) {
            return true; // No change
        }

        m_settings = settings;

        if (m_running) {
            restart(settings.m_mode);
        }

        return true;
    }
    else
    {
        qDebug("SpectranMISO::handleMessage: unknown message");
    }

    return false;
}

bool SpectranMISO::applySettings(
    const SpectranMISOSettings& settings,
    const QList<QString>& settingsKeys,
    bool force)
{
    QString debugString = settings.getDebugString(settingsKeys, force);
    qDebug("SpectranMISO::applySettings: %s", debugString.toStdString().c_str());
    bool forwardChangeRxDSP = false;
    bool forwardChangeTxDSP = false;

    if (settingsKeys.contains("sampleRate")
    || settingsKeys.contains("mode")
    || settingsKeys.contains("clockRate")
    || settingsKeys.contains("logDecimation") || force)
    {
        forwardChangeRxDSP = true;
        forwardChangeTxDSP = true;
    }
    if (settingsKeys.contains("rxCenterFrequency") || force)
    {
        forwardChangeRxDSP = true;
    }
    if (settingsKeys.contains("txCenterFrequency") || force)
    {
        forwardChangeTxDSP = true;
    }

    if (m_running)
    {
        AARTSAAPI_Config config, root;

        if (AARTSAAPI_ConfigRoot(&m_device, &root) == AARTSAAPI_OK)
        {
            if (settings.m_mode == SPECTRANMISO_MODE_TX_IQ) // Tx only
            {
                if (settingsKeys.contains("txCenterFrequency") || force)
                {
                    // Set the center frequency
                    if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/centerfreq") == AARTSAAPI_OK)
                        AARTSAAPI_ConfigSetFloat(&m_device, &config, settings.m_txCenterFrequency);
                    else
                        qWarning("SpectranMISO::applySettings: cannot find main/centerfreq");
                    // Propagate to the worker
                    m_streamWorker->setCenterFrequency(settings.m_txCenterFrequency);
                }
                if (settingsKeys.contains("sampleRate") || force)
                {
                    m_sampleMIFifo.resize(SampleSourceFifo::getSizePolicy(settings.m_sampleRate));
                    m_sampleMOFifo.resize(SampleSourceFifo::getSizePolicy(settings.m_sampleRate));
                    // Propagate to the worker
                    m_streamWorker->setSampleRate(settings.m_sampleRate);
                }
            }
            else if (settings.m_mode == SPECTRANMISO_MODE_RXTX_IQ) // Rx + Tx
            {
                if (settingsKeys.contains("rxCenterFrequency") || force)
                {
                    // Set the Rx center frequency
                    if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/demodcenterfreq") == AARTSAAPI_OK)
                        AARTSAAPI_ConfigSetFloat(&m_device, &config, settings.m_rxCenterFrequency);
                    else
                        qWarning("SpectranMISO::applySettings: cannot find main/demodcenterfreq");
                }

                if (settingsKeys.contains("txCenterFrequency") || force)
                {
                    // Set the Tx center frequency
                    if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/centerfreq") == AARTSAAPI_OK)
                        AARTSAAPI_ConfigSetFloat(&m_device, &config, settings.m_txCenterFrequency);
                    else
                        qWarning("SpectranMISO::applySettings: cannot find main/centerfreq");
                }

                if (settingsKeys.contains("sampleRate") || force)
                {
                    m_sampleMIFifo.resize(SampleSourceFifo::getSizePolicy(settings.m_sampleRate));
                    m_sampleMOFifo.resize(SampleSourceFifo::getSizePolicy(settings.m_sampleRate));
                    // Set the sample rate of the receiver equal to the transmitter sample rate
                    if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/demodspanfreq") == AARTSAAPI_OK)
                        AARTSAAPI_ConfigSetFloat(&m_device, &config, (double) settings.m_sampleRate / 1.5);
                    else
                        qWarning("SpectranMISO::applySettings: cannot find main/demodspanfreq");
                }
            }
            else // Rx only modes
            {
                if (settingsKeys.contains("rxCenterFrequency") || force)
                {
                    // Set the center frequency
                    if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/centerfreq") == AARTSAAPI_OK)
                        AARTSAAPI_ConfigSetFloat(&m_device, &config, settings.m_rxCenterFrequency);
                    else
                        qWarning("SpectranMISO::applySettings: cannot find main/centerfreq");
                }
                if (!SpectranMISOSettings::isRawMode(settings.m_mode) && (settingsKeys.contains("sampleRate") || force))
                {
                    m_sampleMIFifo.resize(SampleSourceFifo::getSizePolicy(settings.m_sampleRate));
                    m_sampleMOFifo.resize(SampleSourceFifo::getSizePolicy(settings.m_sampleRate));
                    // Set the sample rate
                    if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/spanfreq") == AARTSAAPI_OK)
                        AARTSAAPI_ConfigSetFloat(&m_device, &config, (double) settings.m_sampleRate / 1.5);
                    else
                        qWarning("SpectranMISO::applySettings: cannot find main/spanfreq");
                }
                // V6Eco only supports 61 MHz clock rate (61.44 actually) and therefore does not need to set it
                if ((m_spectranModel == SpectranModel::SPECTRAN_V6) && (settingsKeys.contains("clockRate") || force))
                {
                    // Set the clock rate
                    if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"device/receiverclock") == AARTSAAPI_OK)
                        AARTSAAPI_ConfigSetString(&m_device, &config, m_clockRateNames[settings.m_clockRate].toStdWString().c_str());
                    else
                        qWarning("SpectranMISO::applySettings: cannot find device/receiverclock");
                }
                if (SpectranMISOSettings::isRawMode(settings.m_mode))
                {
                    m_sampleMIFifo.resize(SampleSourceFifo::getSizePolicy(getSampleRate(settings)));
                    m_sampleMOFifo.resize(SampleSourceFifo::getSizePolicy(getSampleRate(settings)));

                    if ((settingsKeys.contains("logDecimation") || force) && SpectranMISOSettings::isDecimationEnabled(settings.m_mode))
                    {
                        // Decimation factor changed
                        // Set the decimation factor
                        if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/decimation") == AARTSAAPI_OK)
                            AARTSAAPI_ConfigSetString(&m_device, &config, m_log2DecimationNames[settings.m_logDecimation].toStdWString().c_str());
                        else
                            qWarning("SpectranMISO::applySettings: cannot find main/decimation");
                    }
                }
            }
        }
    }

    if (forwardChangeRxDSP)
    {
        int sampleRate = getSampleRate(settings);
        DSPMIMOSignalNotification *notif = new DSPMIMOSignalNotification(sampleRate, settings.m_rxCenterFrequency, true, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (forwardChangeTxDSP)
    {
        int sampleRate = getSampleRate(settings);
        DSPMIMOSignalNotification *notif = new DSPMIMOSignalNotification(sampleRate, settings.m_txCenterFrequency, false, 0);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    // Update changed settings
    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    return true;
}

int SpectranMISO::webapiSettingsGet(
    SWGSDRangel::SWGDeviceSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setSpectranMisoSettings(new SWGSDRangel::SWGSpectranMISOSettings());
    response.getSpectranMisoSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200; // HTTP OK
}

int SpectranMISO::webapiSettingsPutPatch(
    bool force,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response, // query + response
    QString& errorMessage)
{
    (void) errorMessage;
    SpectranMISOSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureSpectranMISO* message = MsgConfigureSpectranMISO::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgConfigureSpectranMISO* messageToGUI = MsgConfigureSpectranMISO::create(settings, deviceSettingsKeys, force);
        getMessageQueueToGUI()->push(messageToGUI);
    }

    webapiFormatDeviceSettings(response, m_settings);
    return 200; // HTTP OK
}

int SpectranMISO::webapiRunGet(
    int subsystemIndex,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) subsystemIndex;
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int SpectranMISO::webapiRun(
    bool run,
    int subsystemIndex,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) subsystemIndex;
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        getMessageQueueToGUI()->push(msgToGUI);
    }

    return 200;
}

void SpectranMISO::webapiFormatDeviceSettings(
    SWGSDRangel::SWGDeviceSettings& response,
    const SpectranMISOSettings& settings)
{
    response.getSpectranMisoSettings()->setMode(settings.m_mode);
    response.getSpectranMisoSettings()->setRxCenterFrequency(settings.m_rxCenterFrequency);
    response.getSpectranMisoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    response.getSpectranMisoSettings()->setSampleRate(settings.m_sampleRate);
    response.getSpectranMisoSettings()->setStreamIndex(settings.m_streamIndex);
    response.getSpectranMisoSettings()->setSpectrumStreamIndex(settings.m_spectrumStreamIndex);
    response.getSpectranMisoSettings()->setStreamLock(settings.m_streamLock ? 1 : 0);
    response.getSpectranMisoSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSpectranMisoSettings()->getReverseApiAddress()) {
        *response.getSpectranMisoSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSpectranMisoSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSpectranMisoSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSpectranMisoSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void SpectranMISO::webapiUpdateDeviceSettings(
    SpectranMISOSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("mode")) {
        settings.m_mode = (SpectranMISOMode) response.getSpectranMisoSettings()->getMode();
    }
    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        settings.m_rxCenterFrequency = response.getSpectranMisoSettings()->getRxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        settings.m_txCenterFrequency = response.getSpectranMisoSettings()->getTxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getSpectranMisoSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getSpectranMisoSettings()->getStreamIndex();
    }
    if (deviceSettingsKeys.contains("spectrumStreamIndex")) {
        settings.m_spectrumStreamIndex = response.getSpectranMisoSettings()->getSpectrumStreamIndex();
    }
    if (deviceSettingsKeys.contains("streamLock")) {
        settings.m_streamLock = response.getSpectranMisoSettings()->getStreamLock() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSpectranMisoSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSpectranMisoSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSpectranMisoSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getSpectranMisoSettings()->getReverseApiDeviceIndex();
    }
}

void SpectranMISO::webapiReverseSendSettings(
    const QList<QString>& deviceSettingsKeys,
    const SpectranMISOSettings& settings,
    bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SpectranMISO"));

    if (deviceSettingsKeys.contains("mode") || force) {
        swgDeviceSettings->getSpectranMisoSettings()->setMode((int) settings.m_mode);
    }
    if (deviceSettingsKeys.contains("rxCenterFrequency") || force) {
        swgDeviceSettings->getSpectranMisoSettings()->setRxCenterFrequency(settings.m_rxCenterFrequency);
    }
    if (deviceSettingsKeys.contains("txCenterFrequency") || force) {
        swgDeviceSettings->getSpectranMisoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    }
    if (deviceSettingsKeys.contains("sampleRate") || force) {
        swgDeviceSettings->getSpectranMisoSettings()->setSampleRate(settings.m_sampleRate);
    }
    if (deviceSettingsKeys.contains("streamIndex") || force) {
        swgDeviceSettings->getSpectranMisoSettings()->setStreamIndex(settings.m_streamIndex);
    }
    if (deviceSettingsKeys.contains("spectrumStreamIndex") || force) {
        swgDeviceSettings->getSpectranMisoSettings()->setSpectrumStreamIndex(settings.m_spectrumStreamIndex);
    }
    if (deviceSettingsKeys.contains("streamLock") || force) {
        swgDeviceSettings->getSpectranMisoSettings()->setStreamLock(settings.m_streamLock ? 1 : 0);
    }
}

void SpectranMISO::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(2); // MIMO
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("SpectranMISO"));

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

void SpectranMISO::networkManagerFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        // Process the response data as needed
    } else {
        qWarning() << "Network request failed:" << reply->errorString();
    }
    reply->deleteLater();
}

bool SpectranMISO::startMode(const SpectranMISOMode& mode)
{
    qDebug("SpectranMISO::startMode: mode=%s", SpectranMISOSettings::m_modeDisplayNames[mode].toStdString().c_str());
    // Start the streaming process
    QString type = QString("%1/%2").arg(m_spectranModelNames[m_spectranModel]).arg(m_modeNames[mode]);

    AARTSAAPI_Result res = AARTSAAPI_OpenDevice(
        DeviceSpectran::instance().getLibraryHandle(),
        &m_device,
        type.toStdWString().c_str(),
        m_deviceAPI->getSamplingDeviceSerial().toStdWString().c_str()
    );

    if (res != AARTSAAPI_OK)
    {
        qCritical("SpectranMISO::startMode: Cannot open Spectran device. Return code: %x", res);
        return false;
    }

    applyCommonSettings(mode);

    if ((res = AARTSAAPI_ConnectDevice(&m_device)) != AARTSAAPI_OK)
    {
        qCritical("SpectranMISO::startMode: Cannot connect to Spectran device. Return code: %x", res);
        AARTSAAPI_CloseDevice(DeviceSpectran::instance().getLibraryHandle(), &m_device);
        return false;
    }

    if ((res = AARTSAAPI_StartDevice(&m_device)) != AARTSAAPI_OK)
    {
        qCritical("SpectranMISO::startMode: Cannot start Spectran device. Return code: %x", res);
        AARTSAAPI_CloseDevice(DeviceSpectran::instance().getLibraryHandle(), &m_device);
        return false;
    }

    return true; // Successfully started
}

void SpectranMISO::stopMode()
{
    qDebug("SpectranMISO::stopMode");
    // Stop the streaming process
    AARTSAAPI_StopDevice(&m_device);
    // Release the hardware
    AARTSAAPI_DisconnectDevice(&m_device); // g_main_loop is stopped in the stream worker
    // Close the device handle
    AARTSAAPI_CloseDevice(DeviceSpectran::instance().getLibraryHandle(), &m_device);
    qDebug("SpectranMISO::stopMode: ended");
}

void SpectranMISO::applyCommonSettings(const SpectranMISOMode& mode)
{
    AARTSAAPI_Config config, root;

    if (AARTSAAPI_ConfigRoot(&m_device, &root) == AARTSAAPI_OK)
    {
        // In I/Q mode and for SpectranV6 clock is set to slow
        if (!SpectranMISOSettings::isRawMode(mode) && (m_spectranModel == SpectranModel::SPECTRAN_V6))
        {
            if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"device/receiverclock") == AARTSAAPI_OK) {
                AARTSAAPI_ConfigSetString(&m_device, &config, L"92MHz");
            } else {
                qWarning("SpectranMISO::applyCommonSettings: cannot find device/receiverclock");
            }
        }

        if (SpectranMISOSettings::isRawMode(mode))
        {
            if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"device/outputformat") == AARTSAAPI_OK)
                AARTSAAPI_ConfigSetString(&m_device, &config, L"iq");
            else
                qWarning("SpectranMISO::applyCommonSettings: cannot find device/outputformat");
        }

        if (mode != SpectranMISOMode::SPECTRANMISO_MODE_TX_IQ)
        {
            if (mode == SpectranMISOMode::SPECTRANMISO_MODE_2RX_RAW_INTL)
            {
                if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"device/receiverchannel") == AARTSAAPI_OK) {
                    AARTSAAPI_ConfigSetString(&m_device, &config, L"Rx12"); // Always set in I/Q interleave mode
                } else {
                    qWarning("SpectranMISO::applyCommonSettings: cannot find device/receiverchannel");
                }
            }
            else if (mode == SpectranMISOMode::SPECTRANMISO_MODE_2RX_RAW)
            {
                if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"device/receiverchannel") == AARTSAAPI_OK) {
                    AARTSAAPI_ConfigSetString(&m_device, &config, L"Rx1+Rx2"); // Always set in dual channel mode
                } else {
                    qWarning("SpectranMISO::applyCommonSettings: cannot find device/receiverchannel");
                }
            }
            else
            {
                if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"device/receiverchannel") == AARTSAAPI_OK) {
                    AARTSAAPI_ConfigSetString(&m_device, &config, m_rxChannelNames[m_settings.m_rxChannel].toStdWString().c_str());
                } else {
                    qWarning("SpectranMISO::applyCommonSettings: cannot find device/receiverchannel");
                }
            }

            // Set Rx reference level to -20 dB
            if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/reflevel") == AARTSAAPI_OK) {
                AARTSAAPI_ConfigSetFloat(&m_device, &config, -20.0);
            }
        }

        if (mode == SpectranMISOMode::SPECTRANMISO_MODE_TX_IQ)
        {
            qDebug("SpectranMISO::applyCommonSettings: applying TX IQ settings");
            if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/centerfreq") == AARTSAAPI_OK)
                AARTSAAPI_ConfigSetFloat(&m_device, &config, 2350.0e6);

            // Set the frequency range of the transmitter - this is the full range not the IQ mod range
            if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/spanfreq") == AARTSAAPI_OK) {
                AARTSAAPI_ConfigSetFloat(&m_device, &config, 50.0e6); // Choose 50 MHz as per example
            }
            // Set the transmitter gain
            if (AARTSAAPI_ConfigFind(&m_device, &root, &config, L"main/transgain") == AARTSAAPI_OK) {
                AARTSAAPI_ConfigSetFloat(&m_device, &config, 0.0);
            }
        }
    }
}
