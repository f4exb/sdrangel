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
#include <QNetworkAccessManager>
#include <QBuffer>
#include <QThread>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGAaroniaRTSAReport.h"

#include "aaroniartsainput.h"
#include "device/deviceapi.h"
#include "aaroniartsainputworker.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"

MESSAGE_CLASS_DEFINITION(AaroniaRTSAInput::MsgConfigureAaroniaRTSA, Message)
MESSAGE_CLASS_DEFINITION(AaroniaRTSAInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(AaroniaRTSAInput::MsgSetStatus, Message)


AaroniaRTSAInput::AaroniaRTSAInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_sampleRate(10000000),
    m_centerFrequency(1450000),
	m_settings(),
	m_aaroniaRTSAWorker(nullptr),
    m_aaroniaRTSAWorkerThread(nullptr),
	m_deviceDescription("AaroniaRTSA"),
	m_running(false),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_deviceAPI->setNbSourceStreams(1);

    if (!m_sampleFifo.setSize(getSampleRate() * 2)) {
        qCritical("AaroniaRTSAInput::AaroniaRTSAInput: Could not allocate SampleFifo");
    }

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AaroniaRTSAInput::networkManagerFinished
    );
}

AaroniaRTSAInput::~AaroniaRTSAInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AaroniaRTSAInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }
}

void AaroniaRTSAInput::destroy()
{
    delete this;
}

void AaroniaRTSAInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool AaroniaRTSAInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return true;
    }

    m_aaroniaRTSAWorkerThread = new QThread();
	m_aaroniaRTSAWorker = new AaroniaRTSAInputWorker(&m_sampleFifo);
    m_aaroniaRTSAWorker->setInputMessageQueue(getInputMessageQueue());
	m_aaroniaRTSAWorker->moveToThread(m_aaroniaRTSAWorkerThread);

    QObject::connect(m_aaroniaRTSAWorkerThread, &QThread::finished, m_aaroniaRTSAWorker, &QObject::deleteLater);
    QObject::connect(m_aaroniaRTSAWorkerThread, &QThread::finished, m_aaroniaRTSAWorkerThread, &QThread::deleteLater);

	connect(this, &AaroniaRTSAInput::setWorkerCenterFrequency, m_aaroniaRTSAWorker, &AaroniaRTSAInputWorker::onCenterFrequencyChanged);
    connect(this, &AaroniaRTSAInput::setWorkerSampleRate, m_aaroniaRTSAWorker, &AaroniaRTSAInputWorker::onSampleRateChanged);
	connect(this, &AaroniaRTSAInput::setWorkerServerAddress, m_aaroniaRTSAWorker, &AaroniaRTSAInputWorker::onServerAddressChanged);
	connect(m_aaroniaRTSAWorker, &AaroniaRTSAInputWorker::updateStatus, this, &AaroniaRTSAInput::setWorkerStatus);

	m_aaroniaRTSAWorkerThread->start();
	m_running = true;

	mutexLocker.unlock();
	applySettings(m_settings, QList<QString>(), true);

	return true;
}

void AaroniaRTSAInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

	m_running = false;
	setWorkerStatus(0);

	if (m_aaroniaRTSAWorkerThread)
	{
        m_aaroniaRTSAWorkerThread->quit();
        m_aaroniaRTSAWorkerThread->wait();
		m_aaroniaRTSAWorker = nullptr;
        m_aaroniaRTSAWorkerThread = nullptr;
	}
}

QByteArray AaroniaRTSAInput::serialize() const
{
    return m_settings.serialize();
}

bool AaroniaRTSAInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureAaroniaRTSA* message = MsgConfigureAaroniaRTSA::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAaroniaRTSA* messageToGUI = MsgConfigureAaroniaRTSA::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& AaroniaRTSAInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int AaroniaRTSAInput::getSampleRate() const
{
	return m_sampleRate;
}

quint64 AaroniaRTSAInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void AaroniaRTSAInput::setCenterFrequency(qint64 centerFrequency)
{
	AaroniaRTSAInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureAaroniaRTSA* message = MsgConfigureAaroniaRTSA::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAaroniaRTSA* messageToGUI = MsgConfigureAaroniaRTSA::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

void AaroniaRTSAInput::setWorkerStatus(int status)
{
	if (m_guiMessageQueue) {
		m_guiMessageQueue->push(MsgSetStatus::create(status));
    }
}

bool AaroniaRTSAInput::handleMessage(const Message& message)
{
    if (MsgConfigureAaroniaRTSA::match(message))
    {
        MsgConfigureAaroniaRTSA& conf = (MsgConfigureAaroniaRTSA&) message;
        qDebug() << "AaroniaRTSAInput::handleMessage: MsgConfigureAaroniaRTSA";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success) {
            qDebug("AaroniaRTSAInput::handleMessage: config error");
        }

        return true;
    }
    else if (AaroniaRTSAInputWorker::MsgReportSampleRateAndFrequency::match(message))
    {
        AaroniaRTSAInputWorker::MsgReportSampleRateAndFrequency& report = (AaroniaRTSAInputWorker::MsgReportSampleRateAndFrequency&) message;
        m_sampleRate = report.getSampleRate();
        m_centerFrequency = report.getCenterFrequency();
        qDebug() << "AaroniaRTSAInput::handleMessage: AaroniaRTSAInputWorker::MsgReportSampleRateAndFrequency:"
            << " m_sampleRate: " << m_sampleRate
            << " m-centerFrequency" << m_centerFrequency;

        if (!m_sampleFifo.setSize(m_sampleRate * 2)) {
            qCritical("AaroniaRTSAInput::AaroniaRTSAInput: Could not allocate SampleFifo");
        }

		DSPSignalNotification *notif = new DSPSignalNotification(
			m_sampleRate, m_centerFrequency);
		m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "AaroniaRTSAInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

int AaroniaRTSAInput::getStatus() const
{
    if (m_aaroniaRTSAWorker) {
        return m_aaroniaRTSAWorker->getStatus();
    } else {
        return 0;
    }
}

bool AaroniaRTSAInput::applySettings(const AaroniaRTSAInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
	qDebug() << "AaroniaRTSAInput::applySettings: force: "<< force << settings.getDebugString(settingsKeys, force);

	if (settingsKeys.contains("serverAddress") || force)
{
		emit setWorkerServerAddress(settings.m_serverAddress);
    }

    if (settingsKeys.contains("centerFrequency") || force)
    {
        emit setWorkerCenterFrequency(settings.m_centerFrequency);

		// DSPSignalNotification *notif = new DSPSignalNotification(
		// 	getSampleRate(), settings.m_centerFrequency);
		// m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
	}

    if (settingsKeys.contains("sampleRate")) {
        emit setWorkerSampleRate(settings.m_sampleRate);
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

int AaroniaRTSAInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int AaroniaRTSAInput::webapiRun(
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

int AaroniaRTSAInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
	response.setAaroniaRtsaSettings(new SWGSDRangel::SWGAaroniaRTSASettings());
	response.getAaroniaRtsaSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int AaroniaRTSAInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    AaroniaRTSAInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureAaroniaRTSA *msg = MsgConfigureAaroniaRTSA::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAaroniaRTSA *msgToGUI = MsgConfigureAaroniaRTSA::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void AaroniaRTSAInput::webapiUpdateDeviceSettings(
        AaroniaRTSAInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
		settings.m_centerFrequency = response.getAaroniaRtsaSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
		settings.m_sampleRate = response.getAaroniaRtsaSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("serverAddress")) {
		settings.m_serverAddress = *response.getAaroniaRtsaSettings()->getServerAddress();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
		settings.m_useReverseAPI = response.getAaroniaRtsaSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
		settings.m_reverseAPIAddress = *response.getAaroniaRtsaSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
		settings.m_reverseAPIPort = response.getAaroniaRtsaSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
		settings.m_reverseAPIDeviceIndex = response.getAaroniaRtsaSettings()->getReverseApiDeviceIndex();
    }
}

int AaroniaRTSAInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
	response.setAaroniaRtsaReport(new SWGSDRangel::SWGAaroniaRTSAReport());
	response.getAirspyHfReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void AaroniaRTSAInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const AaroniaRTSAInputSettings& settings)
{
	response.getAaroniaRtsaSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getAaroniaRtsaSettings()->setSampleRate(settings.m_sampleRate);

	if (response.getAaroniaRtsaSettings()->getServerAddress()) {
		*response.getAaroniaRtsaSettings()->getServerAddress() = settings.m_serverAddress;
    } else {
		response.getAaroniaRtsaSettings()->setServerAddress(new QString(settings.m_serverAddress));
    }

	response.getAaroniaRtsaSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

	if (response.getAaroniaRtsaSettings()->getReverseApiAddress()) {
		*response.getAaroniaRtsaSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
		response.getAaroniaRtsaSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

	response.getAaroniaRtsaSettings()->setReverseApiPort(settings.m_reverseAPIPort);
	response.getAaroniaRtsaSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void AaroniaRTSAInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
	response.getAaroniaRtsaReport()->setStatus(getStatus());
}

void AaroniaRTSAInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AaroniaRTSAInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AaroniaRTSA"));
	swgDeviceSettings->setAaroniaRtsaSettings(new SWGSDRangel::SWGAaroniaRTSASettings());
	SWGSDRangel::SWGAaroniaRTSASettings *swgAaroniaRTSASettings = swgDeviceSettings->getAaroniaRtsaSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgAaroniaRTSASettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("serverAddress") || force) {
        swgAaroniaRTSASettings->setServerAddress(new QString(settings.m_serverAddress));
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

void AaroniaRTSAInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AaroniaRTSA"));

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

void AaroniaRTSAInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AaroniaRTSAInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AaroniaRTSAInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
