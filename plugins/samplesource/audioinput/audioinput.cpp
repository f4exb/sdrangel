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
#include "device/deviceapi.h"

#include "audioinput.h"
#include "audioinputworker.h"

MESSAGE_CLASS_DEFINITION(AudioInput::MsgConfigureAudioInput, Message)
MESSAGE_CLASS_DEFINITION(AudioInput::MsgStartStop, Message)

AudioInput::AudioInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_audioDeviceIndex(-1),
    m_worker(nullptr),
    m_workerThread(nullptr),
    m_deviceDescription("AudioInput"),
    m_running(false),
    m_centerFrequency(0)
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_fifo.setSize(20*AudioInputWorker::m_convBufSamples);
    m_deviceAPI->setNbSourceStreams(1);
    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    m_sampleRate = audioDeviceManager->getInputSampleRate(m_audioDeviceIndex);
    m_settings.m_deviceName = AudioDeviceManager::m_defaultDeviceName;
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AudioInput::networkManagerFinished
    );
}

AudioInput::~AudioInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AudioInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }
}

void AudioInput::destroy()
{
    delete this;
}

void AudioInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool AudioInput::start()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return true;
    }

    if(!m_sampleFifo.setSize(96000*4))
    {
        qCritical("Could not allocate SampleFifo");
        return false;
    }

    qDebug() << "AudioInput::start";

    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->addAudioSource(&m_fifo, getInputMessageQueue(), m_audioDeviceIndex);

    m_workerThread = new QThread();
    m_worker = new AudioInputWorker(&m_sampleFifo, &m_fifo);
    m_worker->moveToThread(m_workerThread);

    QObject::connect(m_workerThread, &QThread::started, m_worker, &AudioInputWorker::startWork);
    QObject::connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    QObject::connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater);

    m_worker->setLog2Decimation(m_settings.m_log2Decim);
    m_worker->setIQMapping(m_settings.m_iqMapping);
    m_worker->startWork();
    m_workerThread->start();
    m_running = true;
	mutexLocker.unlock();

    qDebug("AudioInput::start: started");

    return true;
}

void AudioInput::stop()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug("AudioInput::stop");
    m_running = false;

    if (m_workerThread)
    {
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread = nullptr;
        m_worker = nullptr;
    }

    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->removeAudioSource(&m_fifo);
}

QByteArray AudioInput::serialize() const
{
    return m_settings.serialize();
}

bool AudioInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureAudioInput* message = MsgConfigureAudioInput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAudioInput* messageToGUI = MsgConfigureAudioInput::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& AudioInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int AudioInput::getSampleRate() const
{
    return m_settings.m_sampleRate/(1<<m_settings.m_log2Decim);
}

quint64 AudioInput::getCenterFrequency() const
{
    return m_centerFrequency;
}

bool AudioInput::handleMessage(const Message& message)
{
    if (MsgConfigureAudioInput::match(message))
    {
        qDebug() << "AudioInput::handleMessage: MsgConfigureAudioInput";
        MsgConfigureAudioInput& conf = (MsgConfigureAudioInput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "AudioInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else
    {
        return false;
    }
}

void AudioInput::applySettings(const AudioInputSettings& settings, QList<QString> settingsKeys, bool force)
{
    bool forwardChange = false;

    qDebug() << "AudioInput::applySettings: "
        << " force:" << force
        << settings.getDebugString(settingsKeys, force);


    if (settingsKeys.contains("deviceName") || settingsKeys.contains("sampleRate") || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        m_audioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_deviceName);
        AudioDeviceManager::InputDeviceInfo deviceInfo;

        if (audioDeviceManager->getInputDeviceInfo(settings.m_deviceName, deviceInfo))
        {
            deviceInfo.sampleRate = settings.m_sampleRate;
            audioDeviceManager->setInputDeviceInfo(m_audioDeviceIndex, deviceInfo);
        }

        audioDeviceManager->removeAudioSource(&m_fifo);
        audioDeviceManager->addAudioSource(&m_fifo, getInputMessageQueue(), m_audioDeviceIndex);
        m_sampleRate = audioDeviceManager->getInputSampleRate(m_audioDeviceIndex);
        qDebug("AudioInput::applySettings: audioDeviceName: %s audioDeviceIndex: %d sampleRate: %d",
            qPrintable(settings.m_deviceName), m_audioDeviceIndex, m_sampleRate);
        forwardChange = true;
    }

    if (settingsKeys.contains("sampleRate") || force) {
        forwardChange = true;
    }

    if (settingsKeys.contains("volume") || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();

        if (audioDeviceManager->setInputDeviceVolume(settings.m_volume, m_audioDeviceIndex)) {
            qDebug("AudioInput::applySettings: set volume of %d to %f", m_audioDeviceIndex, settings.m_volume);
        } else {
            qWarning("AudioInput::applySettings: failed to set volume of %d to %f", m_audioDeviceIndex, settings.m_volume);
        }
    }

    if (settingsKeys.contains("log2Decim") || force)
    {
        forwardChange = true;

        if (m_running)
        {
            m_worker->setLog2Decimation(settings.m_log2Decim);
            qDebug() << "AudioInput::applySettings: set decimation to " << (1<<settings.m_log2Decim);
        }
    }

    if (settingsKeys.contains("fcPos") || force)
    {
        if (m_worker) {
            m_worker->setFcPos((int) settings.m_fcPos);
        }

        qDebug() << "AudioInput::applySettings: set fc pos (enum) to " << (int) settings.m_fcPos;
    }

    if (settingsKeys.contains("iqMapping") || force)
    {
        forwardChange = true;

        if (m_running) {
            m_worker->setIQMapping(settings.m_iqMapping);
        }
    }

    if (settingsKeys.contains("dcBlock") || settingsKeys.contains("iqImbalance") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqImbalance);
        qDebug("AudioInput::applySettings: corrections: DC block: %s IQ imbalance: %s",
                settings.m_dcBlock ? "true" : "false",
                settings.m_iqImbalance ? "true" : "false");
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
        bool realElseComplex = (m_settings.m_iqMapping == AudioInputSettings::L)
            || (m_settings.m_iqMapping == AudioInputSettings::R);
        DSPSignalNotification *notif = new DSPSignalNotification(m_settings.m_sampleRate/(1<<m_settings.m_log2Decim), 0, realElseComplex);
        m_sampleRate = notif->getSampleRate();
        m_centerFrequency = notif->getCenterFrequency();
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

int AudioInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int AudioInput::webapiRun(
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

int AudioInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setAudioInputSettings(new SWGSDRangel::SWGAudioInputSettings());
    response.getAudioInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);

    return 200;
}

int AudioInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    AudioInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureAudioInput *msg = MsgConfigureAudioInput::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAudioInput *msgToGUI = MsgConfigureAudioInput::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void AudioInput::webapiUpdateDeviceSettings(
        AudioInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("device")) {
        settings.m_deviceName = *response.getAudioInputSettings()->getDevice();
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_sampleRate = response.getAudioInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("volume")) {
        settings.m_volume = response.getAudioInputSettings()->getVolume();
    }
    if (deviceSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getAudioInputSettings()->getLog2Decim();
    }
    if (deviceSettingsKeys.contains("iqMapping")) {
        settings.m_iqMapping = (AudioInputSettings::IQMapping)response.getAudioInputSettings()->getIqMapping();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getAudioInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqImbalance")) {
        settings.m_iqImbalance = response.getAudioInputSettings()->getIqImbalance() != 0;
    }
    if (deviceSettingsKeys.contains("fcPos")) {
        settings.m_fcPos = (AudioInputSettings::fcPos_t) response.getAudioInputSettings()->getFcPos();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAudioInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAudioInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAudioInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAudioInputSettings()->getReverseApiDeviceIndex();
    }
}

void AudioInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const AudioInputSettings& settings)
{
    response.getAudioInputSettings()->setDevice(new QString(settings.m_deviceName));
    response.getAudioInputSettings()->setDevSampleRate(settings.m_sampleRate);
    response.getAudioInputSettings()->setVolume(settings.m_volume);
    response.getAudioInputSettings()->setLog2Decim(settings.m_log2Decim);
    response.getAudioInputSettings()->setIqMapping((int)settings.m_iqMapping);
    response.getAudioInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getAudioInputSettings()->setIqImbalance(settings.m_iqImbalance ? 1 : 0);
    response.getAudioInputSettings()->setFcPos((int) settings.m_fcPos);

    response.getAudioInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAudioInputSettings()->getReverseApiAddress()) {
        *response.getAudioInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAudioInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAudioInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAudioInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void AudioInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AudioInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AudioInput"));
    swgDeviceSettings->setAudioInputSettings(new SWGSDRangel::SWGAudioInputSettings());
    SWGSDRangel::SWGAudioInputSettings *swgAudioInputSettings = swgDeviceSettings->getAudioInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("device") || force) {
        swgAudioInputSettings->setDevice(new QString(settings.m_deviceName));
    }
    if (deviceSettingsKeys.contains("devSampleRate") || force) {
        swgAudioInputSettings->setDevSampleRate(settings.m_sampleRate);
    }
    if (deviceSettingsKeys.contains("volume") || force) {
        swgAudioInputSettings->setVolume(settings.m_volume);
    }
    if (deviceSettingsKeys.contains("log2Decim") || force) {
        swgAudioInputSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (deviceSettingsKeys.contains("iqMapping") || force) {
        swgAudioInputSettings->setIqMapping(settings.m_iqMapping);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgAudioInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqImbalance") || force) {
        swgAudioInputSettings->setIqImbalance(settings.m_iqImbalance ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("fcPos") || force) {
        swgAudioInputSettings->setFcPos(settings.m_fcPos);
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

void AudioInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AudioInput"));

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

void AudioInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AudioInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AudioInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
