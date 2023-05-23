///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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
#include "SWGDeviceReport.h"
#include "SWGAaroniaRTSAOutputReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"

#include "aaroniartsaoutputworker.h"
#include "aaroniartsaoutput.h"

MESSAGE_CLASS_DEFINITION(AaroniaRTSAOutput::MsgConfigureAaroniaRTSAOutput, Message)
MESSAGE_CLASS_DEFINITION(AaroniaRTSAOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(AaroniaRTSAOutput::MsgReportSampleRateAndFrequency, Message)
MESSAGE_CLASS_DEFINITION(AaroniaRTSAOutput::MsgSetStatus, Message)

AaroniaRTSAOutput::AaroniaRTSAOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
	m_deviceDescription("AaroniaRTSAOutput"),
    m_worker(nullptr),
    m_workerThread(nullptr),
    m_running(false)
{
	m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_settings.m_sampleRate));
    m_deviceAPI->setNbSinkStreams(1);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AaroniaRTSAOutput::networkManagerFinished
    );
}

AaroniaRTSAOutput::~AaroniaRTSAOutput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AaroniaRTSAOutput::networkManagerFinished
    );
    delete m_networkManager;
	stop();
}

void AaroniaRTSAOutput::destroy()
{
    delete this;
}

void AaroniaRTSAOutput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool AaroniaRTSAOutput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return true;
    }

	qDebug() << "AaroniaRTSAOutput::start";

    m_workerThread = new QThread();
	m_worker = new AaroniaRTSAOutputWorker(&m_sampleSourceFifo);
    m_worker->moveToThread(m_workerThread);

    QObject::connect(m_workerThread, &QThread::started, m_worker, &AaroniaRTSAOutputWorker::startWork);
    QObject::connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    QObject::connect(m_workerThread, &QThread::finished, m_workerThread, &QThread::deleteLater);
    QObject::connect(m_worker, &AaroniaRTSAOutputWorker::updateStatus, this, &AaroniaRTSAOutput::setWorkerStatus);

    m_workerThread->start();
    m_running = true;

	mutexLocker.unlock();
    applySettings(m_settings, QList<QString>(), true);

	qDebug("AaroniaRTSAOutput::start: started");
	return true;
}


void AaroniaRTSAOutput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

	qDebug() << "AaroniaRTSAOutput::stop";
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

QByteArray AaroniaRTSAOutput::serialize() const
{
    return m_settings.serialize();
}

bool AaroniaRTSAOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureAaroniaRTSAOutput* message = MsgConfigureAaroniaRTSAOutput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAaroniaRTSAOutput* messageToGUI = MsgConfigureAaroniaRTSAOutput::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

void AaroniaRTSAOutput::setMessageQueueToGUI(MessageQueue *queue)
{
    m_guiMessageQueue = queue;
}

const QString& AaroniaRTSAOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int AaroniaRTSAOutput::getSampleRate() const
{
    return m_settings.m_sampleRate;
}

quint64 AaroniaRTSAOutput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void AaroniaRTSAOutput::setCenterFrequency(qint64 centerFrequency)
{
    AaroniaRTSAOutputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureAaroniaRTSAOutput* message = MsgConfigureAaroniaRTSAOutput::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAaroniaRTSAOutput* messageToGUI = MsgConfigureAaroniaRTSAOutput::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

void AaroniaRTSAOutput::setWorkerStatus(int status)
{
	if (m_guiMessageQueue) {
		m_guiMessageQueue->push(MsgSetStatus::create(status));
    }
}

bool AaroniaRTSAOutput::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        return false;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "AaroniaRTSAOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (MsgConfigureAaroniaRTSAOutput::match(message))
    {
        qDebug() << "AaroniaRTSAOutput::handleMessage:" << message.getIdentifier();
        MsgConfigureAaroniaRTSAOutput& conf = (MsgConfigureAaroniaRTSAOutput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
	else
	{
		return false;
	}
}

int AaroniaRTSAOutput::getStatus() const
{
    // TODO
    // if (m_aaroniaRTSAWorker) {
    //     return m_aaroniaRTSAWorker->getStatus();
    // } else {
        return 0;
    // }
}

void AaroniaRTSAOutput::applySettings(const AaroniaRTSAOutputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "AaroniaRTSAOutput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    QMutexLocker mutexLocker(&m_mutex);
    std::ostringstream os;
    QList<QString> reverseAPIKeys;
    bool forwardChangeToDSP = false;

    if (settingsKeys.contains("centerFrequency") || force)
    {
        if (m_worker) {
            m_worker->setCenterFrequency(settings.m_centerFrequency);
        }

        forwardChangeToDSP = true;
    }

    if (settingsKeys.contains("sampleRate") || force)
    {
        if (m_worker) {
            m_worker->setSampleRate(settings.m_sampleRate);
        }

        forwardChangeToDSP = true;
    }

    if (settingsKeys.contains("serverAddress") || force)
    {
        if (m_worker) {
            m_worker->setServerAddress(settings.m_serverAddress);
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

    if (forwardChangeToDSP)
    {
        DSPSignalNotification *notif = new DSPSignalNotification(settings.m_sampleRate, settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

int AaroniaRTSAOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int AaroniaRTSAOutput::webapiRun(
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

int AaroniaRTSAOutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setAaroniaRtsaOutputSettings(new SWGSDRangel::SWGAaroniaRTSAOutputSettings());
    response.getAaroniaRtsaOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int AaroniaRTSAOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    AaroniaRTSAOutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureAaroniaRTSAOutput *msg = MsgConfigureAaroniaRTSAOutput::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAaroniaRTSAOutput *msgToGUI = MsgConfigureAaroniaRTSAOutput::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void AaroniaRTSAOutput::webapiUpdateDeviceSettings(
        AaroniaRTSAOutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getAaroniaRtsaOutputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getAaroniaRtsaOutputSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("serverAddress")) {
        settings.m_serverAddress = *response.getAaroniaRtsaOutputSettings()->getServerAddress();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAaroniaRtsaOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAaroniaRtsaOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAaroniaRtsaOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAaroniaRtsaOutputSettings()->getReverseApiDeviceIndex();
    }
}

void AaroniaRTSAOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const AaroniaRTSAOutputSettings& settings)
{
    response.getAaroniaRtsaOutputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getAaroniaRtsaOutputSettings()->setSampleRate(settings.m_sampleRate);

    if (response.getAaroniaRtsaOutputSettings()->getServerAddress()) {
        *response.getAaroniaRtsaOutputSettings()->getServerAddress() = settings.m_serverAddress;
    } else {
        response.getAaroniaRtsaOutputSettings()->setServerAddress(new QString(settings.m_serverAddress));
    }

    response.getAaroniaRtsaOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAaroniaRtsaOutputSettings()->getReverseApiAddress()) {
        *response.getAaroniaRtsaOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAaroniaRtsaOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAaroniaRtsaOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAaroniaRtsaOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int AaroniaRTSAOutput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAaroniaRtsaOutputReport(new SWGSDRangel::SWGAaroniaRTSAOutputReport());
    response.getAaroniaRtsaOutputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void AaroniaRTSAOutput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getAaroniaRtsaOutputReport()->setStatus(getStatus());
}

void AaroniaRTSAOutput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AaroniaRTSAOutputSettings& settings, bool force)
{
    (void) deviceSettingsKeys;
    (void) force;
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AaroniaRTSAOutput"));
    swgDeviceSettings->setAaroniaRtsaOutputSettings(new SWGSDRangel::SWGAaroniaRTSAOutputSettings());

    // transfer data that has been modified. When force is on transfer all data except reverse API data

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

void AaroniaRTSAOutput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("LocalInput"));

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

void AaroniaRTSAOutput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AaroniaRTSAOutput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AaroniaRTSAOutput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
