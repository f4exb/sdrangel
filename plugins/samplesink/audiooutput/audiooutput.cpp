///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "device/deviceapi.h"
#include "audio/audiodevicemanager.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"

#include "audiooutputworker.h"
#include "audiooutput.h"

MESSAGE_CLASS_DEFINITION(AudioOutput::MsgConfigureAudioOutput, Message)
MESSAGE_CLASS_DEFINITION(AudioOutput::MsgStartStop, Message)

AudioOutput::AudioOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_audioFifo(48000),
	m_settings(),
    m_audioDeviceIndex(-1),
    m_centerFrequency(0),
	m_worker(nullptr),
    m_workerThread(nullptr),
	m_deviceDescription("AudioOutput")
{
    m_deviceAPI->setNbSinkStreams(1);
    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    m_sampleRate = audioDeviceManager->getOutputSampleRate(m_audioDeviceIndex);
    m_settings.m_deviceName = AudioDeviceManager::m_defaultDeviceName;
    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(48000));
}

AudioOutput::~AudioOutput()
{
	stop();
}

void AudioOutput::destroy()
{
    delete this;
}

void AudioOutput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool AudioOutput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return true;
    }

	qDebug("AudioOutput::start");

    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->addAudioSink(&m_audioFifo, getInputMessageQueue(), m_audioDeviceIndex);

    m_workerThread = new QThread();
	m_worker = new AudioOutputWorker(&m_sampleSourceFifo, &m_audioFifo);
    m_worker->moveToThread(m_workerThread);

    QObject::connect(m_workerThread, &QThread::started, m_worker, &AudioOutputWorker::startWork);
    QObject::connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    QObject::connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater);

    m_worker->setSamplerate(m_sampleRate);
    m_worker->setIQMapping(m_settings.m_iqMapping);
    m_worker->connectTimer(m_deviceAPI->getMasterTimer());
    m_workerThread->start();
    m_running = true;

	mutexLocker.unlock();

	qDebug("AudioOutput::start: started");

	return true;
}

void AudioOutput::stop()
{
    if (!m_running) {
        return;
    }

    qDebug("AudioOutput::stop");
    m_running = false;

    if (m_workerThread)
    {
        m_worker->stopWork();
        m_workerThread->quit();
        m_workerThread->wait();
        m_worker = nullptr;
        m_workerThread = nullptr;
    }

    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->removeAudioSink(&m_audioFifo);

    qDebug("AudioOutput::stop: stopped");
}

QByteArray AudioOutput::serialize() const
{
    return m_settings.serialize();
}

bool AudioOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureAudioOutput* message = MsgConfigureAudioOutput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAudioOutput* messageToGUI = MsgConfigureAudioOutput::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& AudioOutput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int AudioOutput::getSampleRate() const
{
    return m_sampleRate;
}

quint64 AudioOutput::getCenterFrequency() const
{
    return m_centerFrequency;
}

bool AudioOutput::handleMessage(const Message& message)
{
    if(MsgConfigureAudioOutput::match(message))
    {
        qDebug() << "AudioOutput::handleMessage: MsgConfigureAudioOutput";
        MsgConfigureAudioOutput& conf = (MsgConfigureAudioOutput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "AudioOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

void AudioOutput::applySettings(const AudioOutputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "AudioOutput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    bool forwardChange = false;

    if (settingsKeys.contains("deviceName") || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        m_audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_deviceName);
        //qDebug("AMDemod::applySettings: audioDeviceName: %s audioDeviceIndex: %d", qPrintable(settings.m_audioDeviceName), audioDeviceIndex);
        audioDeviceManager->removeAudioSink(&m_audioFifo);
        audioDeviceManager->addAudioSink(&m_audioFifo, getInputMessageQueue(), m_audioDeviceIndex);
        m_sampleRate = audioDeviceManager->getOutputSampleRate(m_audioDeviceIndex);
        forwardChange = true;
    }

    if (settingsKeys.contains("volume") || force)
    {
        m_audioOutputDevice.setVolume(settings.m_volume);
        qDebug() << "AudioOutput::applySettings: set volume to " << settings.m_volume;
    }

    if (settingsKeys.contains("iqMapping") || force)
    {
        forwardChange = true;

        if (m_running) {
            m_worker->setIQMapping(settings.m_iqMapping);
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
        if (m_running) {
            m_worker->setSamplerate(m_sampleRate);
        }

        DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, 0);
        m_centerFrequency = 0;
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

int AudioOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int AudioOutput::webapiRun(
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

int AudioOutput::webapiSettingsGet(
        SWGSDRangel::SWGDeviceSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAudioOutputSettings(new SWGSDRangel::SWGAudioOutputSettings());
    response.getAudioOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);

    return 200;
}

int AudioOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    AudioOutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureAudioOutput *msg = MsgConfigureAudioOutput::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAudioOutput *msgToGUI = MsgConfigureAudioOutput::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void AudioOutput::webapiUpdateDeviceSettings(
        AudioOutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("deviceName")) {
        settings.m_deviceName = *response.getAudioOutputSettings()->getDeviceName();
    }
    if (deviceSettingsKeys.contains("volume")) {
        settings.m_volume = response.getAudioOutputSettings()->getVolume();
    }
    if (deviceSettingsKeys.contains("iqMapping")) {
        settings.m_iqMapping = (AudioOutputSettings::IQMapping) response.getAudioOutputSettings()->getIqMapping();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAudioOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAudioOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAudioOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAudioOutputSettings()->getReverseApiDeviceIndex();
    }
}

void AudioOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const AudioOutputSettings& settings)
{
    response.getAudioOutputSettings()->setDeviceName(new QString(settings.m_deviceName));
    response.getAudioOutputSettings()->setVolume(settings.m_volume);
    response.getAudioOutputSettings()->setIqMapping((int) settings.m_iqMapping);

    response.getAudioOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAudioOutputSettings()->getReverseApiAddress()) {
        *response.getAudioOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAudioOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAudioOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAudioOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void AudioOutput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AudioOutputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AudioOutput"));
    swgDeviceSettings->setAudioOutputSettings(new SWGSDRangel::SWGAudioOutputSettings());
    SWGSDRangel::SWGAudioOutputSettings *swgAudioOutputSettings = swgDeviceSettings->getAudioOutputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("deviceName") || force) {
        swgAudioOutputSettings->setDeviceName(new QString(settings.m_deviceName));
    }
    if (deviceSettingsKeys.contains("volume") || force) {
        swgAudioOutputSettings->setVolume(settings.m_volume);
    }
    if (deviceSettingsKeys.contains("iqMapping") || force) {
        swgAudioOutputSettings->setIqMapping(settings.m_iqMapping);
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

void AudioOutput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AudioOutput"));

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

void AudioOutput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AudioOutput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AudioOutput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

