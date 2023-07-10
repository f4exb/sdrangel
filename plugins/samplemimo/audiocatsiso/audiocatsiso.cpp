///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB                              //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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
#include <QThread>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/samplesourcefifo.h"
#include "dsp/samplesinkfifo.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "device/deviceapi.h"
#include "util/serialutil.h"

#include "audiocatsiso.h"
#include "audiocatinputworker.h"
#include "audiocatoutputworker.h"
#include "audiocatsisocatworker.h"

MESSAGE_CLASS_DEFINITION(AudioCATSISO::MsgConfigureAudioCATSISO, Message)
MESSAGE_CLASS_DEFINITION(AudioCATSISO::MsgStartStop, Message)

AudioCATSISO::AudioCATSISO(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_inputFifo(48000),
    m_outputFifo(48000),
	m_settings(),
    m_inputWorker(nullptr),
    m_outputWorker(nullptr),
    m_catWorker(nullptr),
    m_inputWorkerThread(nullptr),
    m_outputWorkerThread(nullptr),
    m_catWorkerThread(nullptr),
	m_deviceDescription("AudioCATSISO"),
	m_rxRunning(false),
    m_rxAudioDeviceIndex(-1),
    m_txRunning(false),
    m_txAudioDeviceIndex(-1),
    m_ptt(false),
    m_catRunning(false),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    m_mimoType = MIMOAsynchronous;
    m_deviceAPI->setNbSourceStreams(1);
    m_deviceAPI->setNbSinkStreams(1);
    m_inputFifo.setLabel("Input");
    m_outputFifo.setLabel("Output");

    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();

    m_rxSampleRate = audioDeviceManager->getInputSampleRate(m_rxAudioDeviceIndex);
    m_settings.m_rxDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_sampleMIFifo.init(1, SampleSinkFifo::getSizePolicy(m_rxSampleRate));

    m_txSampleRate = audioDeviceManager->getOutputSampleRate(m_txAudioDeviceIndex);
    m_settings.m_txDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_sampleMOFifo.init(1, SampleSourceFifo::getSizePolicy(m_txSampleRate));

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AudioCATSISO::networkManagerFinished
    );

    listComPorts();
}

AudioCATSISO::~AudioCATSISO()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AudioCATSISO::networkManagerFinished
    );
    delete m_networkManager;

    if (m_rxRunning) {
        stopRx();
    }

    if (m_txRunning) {
        stopTx();
    }
}

void AudioCATSISO::destroy()
{
    delete this;
}

void AudioCATSISO::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool AudioCATSISO::startRx()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_rxRunning) {
        return true;
    }

    qDebug() << "AudioCATSISO::startRx";

    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->addAudioSource(&m_inputFifo, getInputMessageQueue(), m_rxAudioDeviceIndex);

    m_inputWorkerThread = new QThread();
    m_inputWorker = new AudioCATInputWorker(&m_sampleMIFifo, &m_inputFifo);
    m_inputWorker->moveToThread(m_inputWorkerThread);

    QObject::connect(m_inputWorkerThread, &QThread::started, m_inputWorker, &AudioCATInputWorker::startWork);
    QObject::connect(m_inputWorkerThread, &QThread::finished, m_inputWorker, &QObject::deleteLater);
    QObject::connect(m_inputWorkerThread, &QThread::finished, m_inputWorkerThread, &QThread::deleteLater);

    m_inputWorker->setLog2Decimation(m_settings.m_log2Decim);
    m_inputWorker->setFcPos(m_settings.m_fcPosRx);
    m_inputWorker->setIQMapping(m_settings.m_rxIQMapping);
    m_inputWorker->startWork();
    m_inputWorkerThread->start();

    qDebug("AudioCATSISO::startRx: started");
    m_rxRunning = true;

    qDebug() << "AudioCATSISO::startRx: start CAT";

    m_catWorkerThread = new QThread();
    m_catWorker = new AudioCATSISOCATWorker();
    m_inputWorker->moveToThread(m_catWorkerThread);

    QObject::connect(m_catWorkerThread, &QThread::started, m_catWorker, &AudioCATSISOCATWorker::startWork);
    QObject::connect(m_catWorkerThread, &QThread::finished, m_catWorker, &QObject::deleteLater);
    QObject::connect(m_catWorkerThread, &QThread::finished, m_catWorkerThread, &QThread::deleteLater);

    m_catWorker->setMessageQueueToGUI(getMessageQueueToGUI());
    m_catWorker->setMessageQueueToSISO(getInputMessageQueue());
    m_catWorkerThread->start();

    AudioCATSISOCATWorker::MsgConfigureAudioCATSISOCATWorker *msgToCAT = AudioCATSISOCATWorker::MsgConfigureAudioCATSISOCATWorker::create(
        m_settings, QList<QString>(), true
    );
    m_catWorker->getInputMessageQueue()->push(msgToCAT);

    qDebug() << "AudioCATSISO::startRx: CAT started";
    m_catRunning = true;

    return true;
}

bool AudioCATSISO::startTx()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_txRunning) {
        return true;
    }

	qDebug("AudioCATSISO::startTx");

    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->addAudioSink(&m_outputFifo, getInputMessageQueue(), m_txAudioDeviceIndex);

    m_outputWorkerThread = new QThread();
	m_outputWorker = new AudioCATOutputWorker(&m_sampleMOFifo, &m_outputFifo);
    m_outputWorker->moveToThread(m_outputWorkerThread);

    QObject::connect(m_outputWorkerThread, &QThread::started, m_outputWorker, &AudioCATOutputWorker::startWork);
    QObject::connect(m_outputWorkerThread, &QThread::finished, m_outputWorker, &QObject::deleteLater);
    QObject::connect(m_outputWorkerThread, &QThread::finished, m_outputWorkerThread, &QThread::deleteLater);

    m_outputWorker->setSamplerate(m_txSampleRate);
    m_outputWorker->setVolume(m_settings.m_txVolume);
    m_outputWorker->setIQMapping(m_settings.m_txIQMapping);
    m_outputWorker->connectTimer(m_deviceAPI->getMasterTimer());
    m_outputWorkerThread->start();
    m_txRunning = true;

	mutexLocker.unlock();

	qDebug("AudioCATSISO::startTx: started");

	return true;
}

void AudioCATSISO::stopRx()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_rxRunning) {
        return;
    }

    qDebug("AudioCATSISO::stopRx");
    m_rxRunning = false;

    if (m_inputWorkerThread)
    {
        m_inputWorkerThread->quit();
        m_inputWorkerThread->wait();
        m_inputWorkerThread = nullptr;
        m_inputWorker = nullptr;
    }

    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->removeAudioSource(&m_inputFifo);

    qDebug("AudioCATSISO::stopRx: stopped");
    qDebug("AudioCATSISO::stopRx: stop CAT");
    m_catRunning = false;

    if (m_catWorkerThread)
    {
        m_catWorkerThread->quit();
        m_catWorkerThread->wait();
        m_catWorkerThread = nullptr;
        m_catWorker = nullptr;
    }

    qDebug("AudioCATSISO::stopRx: CAT stopped");
}

void AudioCATSISO::stopTx()
{
    if (!m_txRunning) {
        return;
    }

    qDebug("AudioCATSISO::stopTx");
    m_txRunning = false;

    if (m_outputWorkerThread)
    {
        m_outputWorker->stopWork();
        m_outputWorkerThread->quit();
        m_outputWorkerThread->wait();
        m_outputWorker = nullptr;
        m_outputWorkerThread = nullptr;
    }

    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->removeAudioSink(&m_outputFifo);

    qDebug("AudioCATSISO::stopTx: stopped");
}

QByteArray AudioCATSISO::serialize() const
{
    return m_settings.serialize();
}

bool AudioCATSISO::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureAudioCATSISO* message = MsgConfigureAudioCATSISO::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAudioCATSISO* messageToGUI = MsgConfigureAudioCATSISO::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& AudioCATSISO::getDeviceDescription() const
{
    return m_deviceDescription;
}

int AudioCATSISO::getSourceSampleRate(int) const
{
    return m_rxSampleRate / (1<<m_settings.m_log2Decim);
}

quint64 AudioCATSISO::getSourceCenterFrequency(int) const
{
    return m_settings.m_rxCenterFrequency;
}

void AudioCATSISO::setSourceCenterFrequency(qint64 centerFrequency, int)
{
    AudioCATSISOSettings settings = m_settings;
    settings.m_rxCenterFrequency = centerFrequency;

    MsgConfigureAudioCATSISO* message = MsgConfigureAudioCATSISO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAudioCATSISO* messageToGUI = MsgConfigureAudioCATSISO::create(settings, QList<QString>{"rxCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

int AudioCATSISO::getSinkSampleRate(int) const
{
    return m_txSampleRate;
}

quint64 AudioCATSISO::getSinkCenterFrequency(int) const
{
    return m_settings.m_txCenterFrequency;
}

void AudioCATSISO::setSinkCenterFrequency(qint64 centerFrequency, int)
{
    AudioCATSISOSettings settings = m_settings;
    settings.m_txCenterFrequency = centerFrequency;

    MsgConfigureAudioCATSISO* message = MsgConfigureAudioCATSISO::create(settings, QList<QString>{"txCenterFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAudioCATSISO* messageToGUI = MsgConfigureAudioCATSISO::create(settings, QList<QString>{"txCenterFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool AudioCATSISO::handleMessage(const Message& message)
{
    if (MsgConfigureAudioCATSISO::match(message))
    {
        qDebug() << "AudioCATSISO::handleMessage: MsgConfigureAudioCATSISO";
        MsgConfigureAudioCATSISO& conf = (MsgConfigureAudioCATSISO&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "AudioCATSISO::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initDeviceEngine(0)) {
                m_deviceAPI->startDeviceEngine(0);
            }

            if (m_settings.m_txEnable)
            {
                if (m_deviceAPI->initDeviceEngine(1)) {
                    m_deviceAPI->startDeviceEngine(1);
                }
            }
        }
        else
        {
            m_deviceAPI->stopDeviceEngine(0);
            m_deviceAPI->stopDeviceEngine(1);
        }

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendStartStop(cmd.getStartStop());
        }

        return true;
    }
    else if (AudioCATSISOSettings::MsgPTT::match(message))
    {
        AudioCATSISOSettings::MsgPTT& cmd = (AudioCATSISOSettings::MsgPTT&) message;
        m_ptt = cmd.getPTT();
        qDebug("AudioCATSISO::handleMessage: MsgPTT: %s", m_ptt ? "on" : "off");
        if (m_catRunning)
        {
            m_catWorker->getInputMessageQueue()->push(&cmd);
            return false;
        }
        else
        {
            return true;
        }
    }
    else if (AudioCATSISOSettings::MsgCATConnect::match(message))
    {
        AudioCATSISOSettings::MsgCATConnect& cmd = (AudioCATSISOSettings::MsgCATConnect&) message;
        qDebug("AudioCATSISO::handleMessage: MsgCATConnect: %s", cmd.getConnect() ? "on" : "off");
        if (m_catRunning)
        {
            m_catWorker->getInputMessageQueue()->push(&cmd);
            return false;
        }
        else
        {
            return true;
        }
    }
    else if (AudioCATSISOCATWorker::MsgReportFrequency::match(message))
    {
        AudioCATSISOCATWorker::MsgReportFrequency& report = (AudioCATSISOCATWorker::MsgReportFrequency&) message;

        if (m_ptt) // Tx
        {
            m_settings.m_txCenterFrequency = report.getFrequency();
            DSPMIMOSignalNotification *notif = new DSPMIMOSignalNotification(
                m_txSampleRate, m_settings.m_txCenterFrequency, false, 0);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
        }
        else // Rx
        {
            m_settings.m_rxCenterFrequency = report.getFrequency();
            DSPMIMOSignalNotification *notif = new DSPMIMOSignalNotification(
                m_rxSampleRate, m_settings.m_rxCenterFrequency, true, 0);
            m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
        }

        return true;
    }
    else
    {
        return false;
    }
}

void AudioCATSISO::applySettings(const AudioCATSISOSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    bool forwardRxChange = false;
    bool forwardTxChange = false;
    bool forwardToCAT = false;

    qDebug() << "AudioCATSISO::applySettings: "
        << " force:" << force
        << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("rxDeviceName") || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        m_rxAudioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_rxDeviceName);
        m_rxSampleRate = audioDeviceManager->getInputSampleRate(m_rxAudioDeviceIndex);
        forwardRxChange = true;

        if (m_rxRunning)
        {
            audioDeviceManager->removeAudioSource(&m_inputFifo);
            audioDeviceManager->addAudioSource(&m_inputFifo, getInputMessageQueue(), m_rxAudioDeviceIndex);
        }
    }

    if (settingsKeys.contains("txDeviceName") || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        m_txAudioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_txDeviceName);
        m_txSampleRate = audioDeviceManager->getOutputSampleRate(m_txAudioDeviceIndex);
        forwardTxChange = true;

        if (m_txRunning)
        {
            audioDeviceManager->removeAudioSink(&m_outputFifo);
            audioDeviceManager->addAudioSink(&m_outputFifo, getInputMessageQueue(), m_txAudioDeviceIndex);
        }
    }

    if (settingsKeys.contains("rxVolume") || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        audioDeviceManager->setInputDeviceVolume(settings.m_rxVolume, m_rxAudioDeviceIndex);
        qDebug() << "AudioCATSISO::applySettings: set Rx volume to " << settings.m_rxVolume;
    }

    if (settingsKeys.contains("txVolume") || force)
    {
        if (m_txRunning) {
            m_outputWorker->setVolume(settings.m_txVolume);
        }
        // m_audioOutput.setVolume(settings.m_txVolume); // doesn't work
        qDebug() << "AudioCATSISO::applySettings: set Tx volume to " << settings.m_txVolume;
    }

    if (settingsKeys.contains("log2Decim") || force)
    {
        forwardRxChange = true;

        if (m_rxRunning)
        {
            m_inputWorker->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "AudioCATSISO::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if (settingsKeys.contains("fcPosRx") || force)
    {
        if (m_inputWorker) {
            m_inputWorker->setFcPos((int) settings.m_fcPosRx);
        }

        qDebug() << "AudioCATSISO::applySettings: set Rx fc pos (enum) to " << (int) settings.m_fcPosRx;
    }

    if (settingsKeys.contains("rxIQMapping") || force)
    {
        forwardRxChange = true;

        if (m_rxRunning) {
            m_inputWorker->setIQMapping(settings.m_rxIQMapping);
        }
    }

    if (settingsKeys.contains("txIQMapping") || force)
    {
        forwardTxChange = true;

        if (m_txRunning) {
            m_outputWorker->setIQMapping(settings.m_txIQMapping);
        }
    }

    if (settingsKeys.contains("dcBlock") || settingsKeys.contains("iqCorrection") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
        qDebug("AudioInput::applySettings: corrections: DC block: %s IQ imbalance: %s",
                settings.m_dcBlock ? "true" : "false",
                settings.m_iqCorrection ? "true" : "false");
    }

    if (settingsKeys.contains("rxCenterFrequency") || force)
    {
        forwardToCAT = true;
        forwardRxChange = true;
    }

    if (settingsKeys.contains("txCenterFrequency") || force)
    {
        forwardToCAT = true;
        forwardTxChange = true;
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

    if (forwardToCAT && m_catRunning)
    {
        AudioCATSISOCATWorker::MsgConfigureAudioCATSISOCATWorker *msg =
            AudioCATSISOCATWorker::MsgConfigureAudioCATSISOCATWorker::create(settings, settingsKeys, force);
        m_catWorker->getInputMessageQueue()->push(msg);
    }

    if (forwardRxChange)
    {
        bool realElseComplex = (m_settings.m_rxIQMapping == AudioCATSISOSettings::L)
            || (m_settings.m_rxIQMapping == AudioCATSISOSettings::R);
        DSPMIMOSignalNotification *notif = new DSPMIMOSignalNotification(
            m_rxSampleRate / (1<<m_settings.m_log2Decim),
            settings.m_rxCenterFrequency,
            true,
            0,
            realElseComplex
        );
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (forwardTxChange)
    {
        if (m_txRunning) {
            m_outputWorker->setSamplerate(m_txSampleRate);
        }

        bool realElseComplex = (m_settings.m_txIQMapping == AudioCATSISOSettings::L)
            || (m_settings.m_txIQMapping == AudioCATSISOSettings::R);
        DSPMIMOSignalNotification *notif = new DSPMIMOSignalNotification(
            m_txSampleRate,
            settings.m_txCenterFrequency,
            false,
            0,
            realElseComplex
        );
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

void AudioCATSISO::listComPorts()
{
    m_comPorts.clear();
    std::vector<std::string> comPorts;
    SerialUtil::getComPorts(comPorts, "tty(USB|ACM)[0-9]+"); // regex is for Linux only

    for (std::vector<std::string>::const_iterator it = comPorts.begin(); it != comPorts.end(); ++it) {
        m_comPorts.push_back(QString(it->c_str()));
    }
}

int AudioCATSISO::webapiRunGet(
    int subsystemIndex,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage
)
{
    (void) subsystemIndex;
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int AudioCATSISO::webapiRun(
    bool run,
    int subsystemIndex,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage
)
{
    (void) subsystemIndex;
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

int AudioCATSISO::webapiSettingsGet(
    SWGSDRangel::SWGDeviceSettings& response,
    QString& errorMessage
)
{
    (void) errorMessage;
    response.setAudioInputSettings(new SWGSDRangel::SWGAudioInputSettings());
    response.getAudioInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);

    return 200;
}

int AudioCATSISO::webapiSettingsPutPatch(
    bool force,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response, // query + response
    QString& errorMessage
)
{
    (void) errorMessage;
    AudioCATSISOSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureAudioCATSISO *msg = MsgConfigureAudioCATSISO::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAudioCATSISO *msgToGUI = MsgConfigureAudioCATSISO::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void AudioCATSISO::webapiUpdateDeviceSettings(
        AudioCATSISOSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        settings.m_rxCenterFrequency = response.getAudioCatsisoSettings()->getRxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        settings.m_txCenterFrequency = response.getAudioCatsisoSettings()->getTxCenterFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getAudioCatsisoSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getAudioCatsisoSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        settings.m_iqOrder = response.getAudioCatsisoSettings()->getIqOrder() != 0;
    }
    if (deviceSettingsKeys.contains("rxDeviceName")) {
        settings.m_rxDeviceName = *response.getAudioCatsisoSettings()->getRxDeviceName();
    }
    if (deviceSettingsKeys.contains("rxIQMapping")) {
        settings.m_rxIQMapping = (AudioCATSISOSettings::IQMapping)response.getAudioCatsisoSettings()->getRxIqMapping();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getAudioCatsisoSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("fcPosRx")) {
        settings.m_fcPosRx = (AudioCATSISOSettings::fcPos_t) response.getAudioCatsisoSettings()->getFcPosRx();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getAudioCatsisoSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getAudioCatsisoSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("rxVolume")) {
        settings.m_rxVolume = response.getAudioCatsisoSettings()->getRxVolume();
    }

    if (deviceSettingsKeys.contains("txDeviceName")) {
        settings.m_txDeviceName = *response.getAudioCatsisoSettings()->getTxDeviceName();
    }
    if (deviceSettingsKeys.contains("txIQMapping")) {
        settings.m_txIQMapping = (AudioCATSISOSettings::IQMapping)response.getAudioCatsisoSettings()->getTxIqMapping();
    }
    if (deviceSettingsKeys.contains("txVolume")) {
        settings.m_txVolume = response.getAudioCatsisoSettings()->getTxVolume();
    }

    if (deviceSettingsKeys.contains("catSpeedIndex")) {
        settings.m_catSpeedIndex = response.getAudioCatsisoSettings()->getCatSpeedIndex();
    }
    if (deviceSettingsKeys.contains("catHandshakeIndex")) {
        settings.m_catHandshakeIndex = response.getAudioCatsisoSettings()->getCatHandshakeIndex();
    }
    if (deviceSettingsKeys.contains("catDataBitsIndex")) {
        settings.m_catDataBitsIndex = response.getAudioCatsisoSettings()->getCatDataBitsIndex();
    }
    if (deviceSettingsKeys.contains("catStopBitsIndex")) {
        settings.m_catStopBitsIndex = response.getAudioCatsisoSettings()->getCatStopBitsIndex();
    }
    if (deviceSettingsKeys.contains("catPTTMethodIndex")) {
        settings.m_catPTTMethodIndex = response.getAudioCatsisoSettings()->getCatPttMethodIndex();
    }
    if (deviceSettingsKeys.contains("catPTTMethodIndex")) {
        settings.m_catDTRHigh = response.getAudioCatsisoSettings()->getCatDtrHigh() != 0;
    }
    if (deviceSettingsKeys.contains("catRTSHigh")) {
        settings.m_catRTSHigh = response.getAudioCatsisoSettings()->getCatRtsHigh() != 0;
    }
    if (deviceSettingsKeys.contains("catRTSHigh")) {
        settings.m_catPollingMs = response.getAudioCatsisoSettings()->getCatPollingMs();
    }

    if (deviceSettingsKeys.contains("txEnable")) {
        settings.m_txEnable = response.getAudioCatsisoSettings()->getTxEnable() != 0;
    }
    if (deviceSettingsKeys.contains("pttSpectrumLink")) {
        settings.m_pttSpectrumLink = response.getAudioCatsisoSettings()->getPttSpectrumLink() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAudioCatsisoSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAudioCatsisoSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAudioCatsisoSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAudioCatsisoSettings()->getReverseApiDeviceIndex();
    }
}

void AudioCATSISO::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const AudioCATSISOSettings& settings)
{
    response.getAudioCatsisoSettings()->setRxCenterFrequency(settings.m_rxCenterFrequency);
    response.getAudioCatsisoSettings()->setTxCenterFrequency(settings.m_txCenterFrequency);
    response.getAudioCatsisoSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getAudioCatsisoSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getAudioCatsisoSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    response.getAudioCatsisoSettings()->setRxDeviceName(new QString(settings.m_rxDeviceName));
    response.getAudioCatsisoSettings()->setRxIqMapping((int)settings.m_rxIQMapping);
    response.getAudioCatsisoSettings()->setLog2Decim(settings.m_log2Decim);
    response.getAudioCatsisoSettings()->setFcPosRx((int) settings.m_fcPosRx);
    response.getAudioCatsisoSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getAudioCatsisoSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getAudioCatsisoSettings()->setRxVolume(settings.m_rxVolume);

    response.getAudioCatsisoSettings()->setTxDeviceName(new QString(settings.m_txDeviceName));
    response.getAudioCatsisoSettings()->setTxIqMapping((int)settings.m_txIQMapping);
    response.getAudioCatsisoSettings()->setTxVolume(settings.m_txVolume);

    response.getAudioCatsisoSettings()->setTxEnable(settings.m_txEnable ? 1 : 0);
    response.getAudioCatsisoSettings()->setPttSpectrumLink(settings.m_pttSpectrumLink ? 1 : 0);

    response.getAudioCatsisoSettings()->setCatSpeedIndex(settings.m_catSpeedIndex);
    response.getAudioCatsisoSettings()->setCatHandshakeIndex(settings.m_catHandshakeIndex);
    response.getAudioCatsisoSettings()->setCatDataBitsIndex(settings.m_catDataBitsIndex);
    response.getAudioCatsisoSettings()->setCatStopBitsIndex(settings.m_catStopBitsIndex);
    response.getAudioCatsisoSettings()->setCatStopBitsIndex(settings.m_catPTTMethodIndex);
    response.getAudioCatsisoSettings()->setCatDtrHigh(settings.m_catDTRHigh ? 1 : 0);
    response.getAudioCatsisoSettings()->setCatRtsHigh(settings.m_catRTSHigh ? 1 : 0);
    response.getAudioCatsisoSettings()->setCatPollingMs(settings.m_catPollingMs);

    response.getAudioCatsisoSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAudioCatsisoSettings()->getReverseApiAddress()) {
        *response.getAudioCatsisoSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAudioCatsisoSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAudioCatsisoSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAudioCatsisoSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void AudioCATSISO::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AudioCATSISOSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AudioCATSISO"));
    swgDeviceSettings->setAudioCatsisoSettings(new SWGSDRangel::SWGAudioCATSISOSettings());
    SWGSDRangel::SWGAudioCATSISOSettings *swgAudioCATSISOSettings = swgDeviceSettings->getAudioCatsisoSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("rxCenterFrequency")) {
        swgAudioCATSISOSettings->setRxCenterFrequency(settings.m_rxCenterFrequency);
    }
    if (deviceSettingsKeys.contains("txCenterFrequency")) {
        swgAudioCATSISOSettings->setTxCenterFrequency(settings.m_txCenterFrequency);
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        swgAudioCATSISOSettings->setTransverterMode(settings.m_transverterMode ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        swgAudioCATSISOSettings->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    }
    if (deviceSettingsKeys.contains("iqOrder")) {
        swgAudioCATSISOSettings->setIqOrder(settings.m_iqOrder ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rxDeviceName") || force) {
        swgAudioCATSISOSettings->setRxDeviceName(new QString(settings.m_rxDeviceName));
    }
    if (deviceSettingsKeys.contains("rxIQMapping")) {
        swgAudioCATSISOSettings->setRxIqMapping((int) settings.m_rxIQMapping);
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        swgAudioCATSISOSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("fcPosRx")) {
        swgAudioCATSISOSettings->setFcPosRx((int) settings.m_fcPosRx);
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        swgAudioCATSISOSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        swgAudioCATSISOSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("rxVolume")) {
        swgAudioCATSISOSettings->setRxVolume(settings.m_rxVolume);
    }

    if (deviceSettingsKeys.contains("txDeviceName")) {
        swgAudioCATSISOSettings->setTxDeviceName(new QString(settings.m_txDeviceName));
    }
    if (deviceSettingsKeys.contains("txIQMapping")) {
        swgAudioCATSISOSettings->setTxIqMapping((int) settings.m_txIQMapping);
    }
    if (deviceSettingsKeys.contains("txVolume")) {
        swgAudioCATSISOSettings->setTxVolume(settings.m_txVolume);
    }

    if (deviceSettingsKeys.contains("txEnable")) {
        swgAudioCATSISOSettings->setTxEnable(settings.m_txEnable ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("pttSpectrumLink")) {
        swgAudioCATSISOSettings->setPttSpectrumLink(settings.m_pttSpectrumLink ? 1 : 0);
    }

    if (deviceSettingsKeys.contains("catSpeedIndex")) {
        swgAudioCATSISOSettings->setCatSpeedIndex(settings.m_catSpeedIndex);
    }
    if (deviceSettingsKeys.contains("catHandshakeIndex")) {
        swgAudioCATSISOSettings->setCatHandshakeIndex(settings.m_catHandshakeIndex);
    }
    if (deviceSettingsKeys.contains("catDataBitsIndex")) {
        swgAudioCATSISOSettings->setCatDataBitsIndex(settings.m_catDataBitsIndex);
    }
    if (deviceSettingsKeys.contains("catStopBitsIndex")) {
        swgAudioCATSISOSettings->setCatStopBitsIndex(settings.m_catStopBitsIndex);
    }
    if (deviceSettingsKeys.contains("catPTTMethodIndex")) {
        swgAudioCATSISOSettings->setCatPttMethodIndex(settings.m_catPTTMethodIndex);
    }
    if (deviceSettingsKeys.contains("m_catDTRHigh")) {
        swgAudioCATSISOSettings->setCatDtrHigh(settings.m_catDTRHigh ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("catRTSHigh")) {
        swgAudioCATSISOSettings->setCatRtsHigh(settings.m_catRTSHigh ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("catPollingMs")) {
        swgAudioCATSISOSettings->setCatPollingMs(settings.m_catPollingMs);
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

void AudioCATSISO::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AudioCATSISO"));

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

void AudioCATSISO::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AudioCATSISO::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AudioCATSISO::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
