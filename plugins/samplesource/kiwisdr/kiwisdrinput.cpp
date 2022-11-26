///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Vort                                                       //
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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
#include "SWGKiwiSDRReport.h"

#include "kiwisdrinput.h"
#include "device/deviceapi.h"
#include "kiwisdrworker.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"

MESSAGE_CLASS_DEFINITION(KiwiSDRInput::MsgConfigureKiwiSDR, Message)
MESSAGE_CLASS_DEFINITION(KiwiSDRInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(KiwiSDRInput::MsgSetStatus, Message)


KiwiSDRInput::KiwiSDRInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_sampleRate(12000),
	m_settings(),
	m_kiwiSDRWorker(nullptr),
    m_kiwiSDRWorkerThread(nullptr),
	m_deviceDescription("KiwiSDR"),
	m_running(false),
	m_masterTimer(deviceAPI->getMasterTimer())
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_deviceAPI->setNbSourceStreams(1);

    if (!m_sampleFifo.setSize(getSampleRate() * 2)) {
        qCritical("KiwiSDRInput::KiwiSDRInput: Could not allocate SampleFifo");
    }

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &KiwiSDRInput::networkManagerFinished
    );
}

KiwiSDRInput::~KiwiSDRInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &KiwiSDRInput::networkManagerFinished
    );
    delete m_networkManager;

    if (m_running) {
        stop();
    }
}

void KiwiSDRInput::destroy()
{
    delete this;
}

void KiwiSDRInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool KiwiSDRInput::start()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return true;
    }

    m_kiwiSDRWorkerThread = new QThread();
	m_kiwiSDRWorker = new KiwiSDRWorker(&m_sampleFifo);
    m_kiwiSDRWorker->setInputMessageQueue(getInputMessageQueue());
	m_kiwiSDRWorker->moveToThread(m_kiwiSDRWorkerThread);

    QObject::connect(m_kiwiSDRWorkerThread, &QThread::finished, m_kiwiSDRWorker, &QObject::deleteLater);
    QObject::connect(m_kiwiSDRWorkerThread, &QThread::finished, m_kiwiSDRWorkerThread, &QThread::deleteLater);

	connect(this, &KiwiSDRInput::setWorkerCenterFrequency, m_kiwiSDRWorker, &KiwiSDRWorker::onCenterFrequencyChanged);
	connect(this, &KiwiSDRInput::setWorkerServerAddress, m_kiwiSDRWorker, &KiwiSDRWorker::onServerAddressChanged);
	connect(this, &KiwiSDRInput::setWorkerGain, m_kiwiSDRWorker, &KiwiSDRWorker::onGainChanged);
	connect(m_kiwiSDRWorker, &KiwiSDRWorker::updateStatus, this, &KiwiSDRInput::setWorkerStatus);

	m_kiwiSDRWorkerThread->start();
	m_running = true;

	mutexLocker.unlock();
	applySettings(m_settings, QList<QString>(), true);

	return true;
}

void KiwiSDRInput::stop()
{
	QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

	m_running = false;
	setWorkerStatus(0);

	if (m_kiwiSDRWorkerThread)
	{
        m_kiwiSDRWorkerThread->quit();
        m_kiwiSDRWorkerThread->wait();
		m_kiwiSDRWorker = nullptr;
        m_kiwiSDRWorkerThread = nullptr;
	}
}

QByteArray KiwiSDRInput::serialize() const
{
    return m_settings.serialize();
}

bool KiwiSDRInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureKiwiSDR* message = MsgConfigureKiwiSDR::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureKiwiSDR* messageToGUI = MsgConfigureKiwiSDR::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& KiwiSDRInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int KiwiSDRInput::getSampleRate() const
{
	return m_sampleRate;
}

quint64 KiwiSDRInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void KiwiSDRInput::setCenterFrequency(qint64 centerFrequency)
{
	KiwiSDRSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureKiwiSDR* message = MsgConfigureKiwiSDR::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureKiwiSDR* messageToGUI = MsgConfigureKiwiSDR::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

void KiwiSDRInput::setWorkerStatus(int status)
{
	if (m_guiMessageQueue) {
		m_guiMessageQueue->push(MsgSetStatus::create(status));
    }
}

bool KiwiSDRInput::handleMessage(const Message& message)
{
    if (MsgConfigureKiwiSDR::match(message))
    {
        MsgConfigureKiwiSDR& conf = (MsgConfigureKiwiSDR&) message;
        qDebug() << "KiwiSDRInput::handleMessage: MsgConfigureKiwiSDR";

        bool success = applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());

        if (!success) {
            qDebug("KiwiSDRInput::handleMessage: config error");
        }

        return true;
    }
    else if (KiwiSDRWorker::MsgReportSampleRate::match(message))
    {
        KiwiSDRWorker::MsgReportSampleRate& report = (KiwiSDRWorker::MsgReportSampleRate&) message;
        m_sampleRate = report.getSampleRate();
        qDebug() << "KiwiSDRInput::handleMessage: KiwiSDRWorker::MsgReportSampleRate: m_sampleRate: " << m_sampleRate;

        if (!m_sampleFifo.setSize(m_sampleRate * 2)) {
            qCritical("KiwiSDRInput::KiwiSDRInput: Could not allocate SampleFifo");
        }

		DSPSignalNotification *notif = new DSPSignalNotification(
			m_sampleRate, m_settings.m_centerFrequency);
		m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "KiwiSDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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

int KiwiSDRInput::getStatus() const
{
    if (m_kiwiSDRWorker) {
        return m_kiwiSDRWorker->getStatus();
    } else {
        return 0;
    }
}

bool KiwiSDRInput::applySettings(const KiwiSDRSettings& settings, const QList<QString>& settingsKeys, bool force)
{
	qDebug() << "KiwiSDRInput::applySettings: force: "<< force << settings.getDebugString(settingsKeys, force);

	if (settingsKeys.contains("serverAddress") || force)
    {
		emit setWorkerServerAddress(settings.m_serverAddress);
    }

	if (settingsKeys.contains("gain") ||
		settingsKeys.contains("useAGC") || force)
	{
		emit setWorkerGain(settings.m_gain, settings.m_useAGC);
	}

    if (settingsKeys.contains("dcBlock")) {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, false);
    }

    if (settingsKeys.contains("centerFrequency") || force)
    {
        emit setWorkerCenterFrequency(settings.m_centerFrequency);

		DSPSignalNotification *notif = new DSPSignalNotification(
			getSampleRate(), settings.m_centerFrequency);
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

int KiwiSDRInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int KiwiSDRInput::webapiRun(
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

int KiwiSDRInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setKiwiSdrSettings(new SWGSDRangel::SWGKiwiSDRSettings());
    response.getKiwiSdrSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int KiwiSDRInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    KiwiSDRSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureKiwiSDR *msg = MsgConfigureKiwiSDR::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureKiwiSDR *msgToGUI = MsgConfigureKiwiSDR::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void KiwiSDRInput::webapiUpdateDeviceSettings(
        KiwiSDRSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("gain")) {
        settings.m_gain = response.getKiwiSdrSettings()->getGain();
    }
    if (deviceSettingsKeys.contains("useAGC")) {
        settings.m_useAGC = response.getKiwiSdrSettings()->getUseAgc();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getKiwiSdrSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getKiwiSdrSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("serverAddress")) {
        settings.m_serverAddress = *response.getKiwiSdrSettings()->getServerAddress();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getKiwiSdrSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getKiwiSdrSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getKiwiSdrSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getKiwiSdrSettings()->getReverseApiDeviceIndex();
    }
}

int KiwiSDRInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setKiwiSdrReport(new SWGSDRangel::SWGKiwiSDRReport());
    response.getKiwiSdrReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void KiwiSDRInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const KiwiSDRSettings& settings)
{
    response.getKiwiSdrSettings()->setGain(settings.m_gain);
    response.getKiwiSdrSettings()->setUseAgc(settings.m_useAGC ? 1 : 0);
    response.getKiwiSdrSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getKiwiSdrSettings()->setCenterFrequency(settings.m_centerFrequency);

    if (response.getKiwiSdrSettings()->getServerAddress()) {
        *response.getKiwiSdrSettings()->getServerAddress() = settings.m_serverAddress;
    } else {
        response.getKiwiSdrSettings()->setServerAddress(new QString(settings.m_serverAddress));
    }

    response.getKiwiSdrSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getKiwiSdrSettings()->getReverseApiAddress()) {
        *response.getKiwiSdrSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getKiwiSdrSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getKiwiSdrSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getKiwiSdrSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

void KiwiSDRInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getKiwiSdrReport()->setStatus(getStatus());
}

void KiwiSDRInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const KiwiSDRSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("KiwiSDR"));
    swgDeviceSettings->setKiwiSdrSettings(new SWGSDRangel::SWGKiwiSDRSettings());
    SWGSDRangel::SWGKiwiSDRSettings *swgKiwiSDRSettings = swgDeviceSettings->getKiwiSdrSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("gain")) {
        swgKiwiSDRSettings->setGain(settings.m_gain);
    }
    if (deviceSettingsKeys.contains("useAGC")) {
        swgKiwiSDRSettings->setUseAgc(settings.m_useAGC ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgKiwiSDRSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("centerFrequency") || force) {
        swgKiwiSDRSettings->setCenterFrequency(settings.m_centerFrequency);
    }
    if (deviceSettingsKeys.contains("serverAddress") || force) {
        swgKiwiSDRSettings->setServerAddress(new QString(settings.m_serverAddress));
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

void KiwiSDRInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("KiwiSDR"));

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

void KiwiSDRInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "KiwiSDRInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("KiwiSDRInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
