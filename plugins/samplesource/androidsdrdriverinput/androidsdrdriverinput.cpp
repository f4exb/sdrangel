///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <QDesktopServices>
#include <QDebug>
#include <QNetworkReply>
#include <QBuffer>
#include <QJsonParseError>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGAndroidSDRDriverInputReport.h"

#include "util/android.h"
#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"

#include "androidsdrdriverinput.h"
#include "androidsdrdriverinputtcphandler.h"
#ifdef ANDROID
#include "util/android.h"
#endif

MESSAGE_CLASS_DEFINITION(AndroidSDRDriverInput::MsgConfigureAndroidSDRDriverInput, Message)
MESSAGE_CLASS_DEFINITION(AndroidSDRDriverInput::MsgStartStop, Message)

AndroidSDRDriverInput::AndroidSDRDriverInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_androidSDRDriverInputTCPPHandler(nullptr),
    m_deviceDescription("AndroidSDRDriverInput")
{
    m_sampleFifo.setLabel(m_deviceDescription);
    m_sampleFifo.setSize(48000 * 8);
    m_androidSDRDriverInputTCPPHandler = new AndroidSDRDriverInputTCPHandler(&m_sampleFifo, m_deviceAPI);
    m_androidSDRDriverInputTCPPHandler->moveToThread(&m_thread);
    m_androidSDRDriverInputTCPPHandler->setMessageQueueToInput(&m_inputMessageQueue);

    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AndroidSDRDriverInput::networkManagerFinished
    );
}

AndroidSDRDriverInput::~AndroidSDRDriverInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AndroidSDRDriverInput::networkManagerFinished
    );
    delete m_networkManager;
    stop();
    m_androidSDRDriverInputTCPPHandler->deleteLater();
}

void AndroidSDRDriverInput::destroy()
{
    delete this;
}

void AndroidSDRDriverInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

void sendIntent()
{
#ifdef ANDROID
    Android::sendIntent();

    QThread::sleep(2); // FIXME:
#endif
}

bool AndroidSDRDriverInput::start()
{
    qDebug() << "AndroidSDRDriverInput::start";
    sendIntent();
    m_androidSDRDriverInputTCPPHandler->reset();
    m_androidSDRDriverInputTCPPHandler->start();
    m_androidSDRDriverInputTCPPHandler->getInputMessageQueue()->push(AndroidSDRDriverInputTCPHandler::MsgConfigureTcpHandler::create(m_settings, QList<QString>(), true));
    m_thread.start();
    return true;
}

void AndroidSDRDriverInput::stop()
{
    qDebug() << "AndroidSDRDriverInput::stop";
    m_androidSDRDriverInputTCPPHandler->stop();
    m_thread.quit();
    m_thread.wait();
}

QByteArray AndroidSDRDriverInput::serialize() const
{
    return m_settings.serialize();
}

bool AndroidSDRDriverInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureAndroidSDRDriverInput* message = MsgConfigureAndroidSDRDriverInput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAndroidSDRDriverInput* messageToGUI = MsgConfigureAndroidSDRDriverInput::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

void AndroidSDRDriverInput::setMessageQueueToGUI(MessageQueue *queue)
{
    m_guiMessageQueue = queue;
    m_androidSDRDriverInputTCPPHandler->setMessageQueueToGUI(queue);
}

const QString& AndroidSDRDriverInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int AndroidSDRDriverInput::getSampleRate() const
{
    return m_settings.m_devSampleRate;
}

quint64 AndroidSDRDriverInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void AndroidSDRDriverInput::setCenterFrequency(qint64 centerFrequency)
{
    AndroidSDRDriverInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureAndroidSDRDriverInput* message = MsgConfigureAndroidSDRDriverInput::create(settings, QList<QString>{"centerFrequency"}, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureAndroidSDRDriverInput* messageToGUI = MsgConfigureAndroidSDRDriverInput::create(settings, QList<QString>{"centerFrequency"}, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool AndroidSDRDriverInput::handleMessage(const Message& message)
{
    if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "AndroidSDRDriverInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (MsgConfigureAndroidSDRDriverInput::match(message))
    {
        qDebug() << "AndroidSDRDriverInput::handleMessage:" << message.getIdentifier();
        MsgConfigureAndroidSDRDriverInput& conf = (MsgConfigureAndroidSDRDriverInput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
    else if (AndroidSDRDriverInputTCPHandler::MsgReportConnection::match(message))
    {
        qDebug() << "AndroidSDRDriverInput::handleMessage:" << message.getIdentifier();
        AndroidSDRDriverInputTCPHandler::MsgReportConnection& report = (AndroidSDRDriverInputTCPHandler::MsgReportConnection&) message;
        if (report.getConnected())
        {
            qDebug() << "Disconnected - stopping DSP";
            m_deviceAPI->stopDeviceEngine();
        }
        return true;
    }
    else
    {
        return false;
    }
}

void AndroidSDRDriverInput::applySettings(const AndroidSDRDriverInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "AndroidSDRDriverInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);
    QMutexLocker mutexLocker(&m_mutex);
    std::ostringstream os;
    bool forwardChange = false;

    if (settingsKeys.contains("dcBlock") || settingsKeys.contains("iqCorrection") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
        qDebug("AndroidSDRDriverInput::applySettings: corrections: DC block: %s IQ imbalance: %s",
                settings.m_dcBlock ? "true" : "false",
                settings.m_iqCorrection ? "true" : "false");
    }

    if (settingsKeys.contains("centerFrequency") || force) {
        forwardChange = true;
    }
    if (settingsKeys.contains("devSampleRate") || force) {
        forwardChange = true;
    }

    mutexLocker.unlock();

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (forwardChange && (settings.m_devSampleRate != 0))
    {
        int sampleRate = settings.m_devSampleRate;
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    m_androidSDRDriverInputTCPPHandler->getInputMessageQueue()->push(AndroidSDRDriverInputTCPHandler::MsgConfigureTcpHandler::create(m_settings, settingsKeys, force));
}

int AndroidSDRDriverInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int AndroidSDRDriverInput::webapiRun(
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

int AndroidSDRDriverInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setAndroidSdrDriverInputSettings(new SWGSDRangel::SWGAndroidSDRDriverInputSettings());
    response.getAndroidSdrDriverInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int AndroidSDRDriverInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    AndroidSDRDriverInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureAndroidSDRDriverInput *msg = MsgConfigureAndroidSDRDriverInput::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAndroidSDRDriverInput *msgToGUI = MsgConfigureAndroidSDRDriverInput::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void AndroidSDRDriverInput::webapiUpdateDeviceSettings(
    AndroidSDRDriverInputSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getAndroidSdrDriverInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("loPpmCorrection")) {
        settings.m_loPpmCorrection = response.getAndroidSdrDriverInputSettings()->getLoPpmCorrection();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getAndroidSdrDriverInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getAndroidSdrDriverInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("biasTee")) {
        settings.m_biasTee = response.getAndroidSdrDriverInputSettings()->getBiasTee() != 0;
    }
    if (deviceSettingsKeys.contains("directSampling")) {
        settings.m_directSampling = response.getAndroidSdrDriverInputSettings()->getDirectSampling() != 0;
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getAndroidSdrDriverInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("agc")) {
        settings.m_agc = response.getAndroidSdrDriverInputSettings()->getAgc() != 0;
    }
    if (deviceSettingsKeys.contains("rfBW")) {
        settings.m_rfBW = response.getAndroidSdrDriverInputSettings()->getRfBw();
    }
    if (deviceSettingsKeys.contains("sampleBits")) {
        settings.m_sampleBits = response.getAndroidSdrDriverInputSettings()->getSampleBits();
    }
    if (deviceSettingsKeys.contains("dataPort")) {
        settings.m_dataPort = response.getAndroidSdrDriverInputSettings()->getDataPort();
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAndroidSdrDriverInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAndroidSdrDriverInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAndroidSdrDriverInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAndroidSdrDriverInputSettings()->getReverseApiDeviceIndex();
    }
}

void AndroidSDRDriverInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const AndroidSDRDriverInputSettings& settings)
{
    response.getAndroidSdrDriverInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getAndroidSdrDriverInputSettings()->setLoPpmCorrection(settings.m_loPpmCorrection);
    response.getAndroidSdrDriverInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getAndroidSdrDriverInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getAndroidSdrDriverInputSettings()->setBiasTee(settings.m_biasTee ? 1 : 0);
    response.getAndroidSdrDriverInputSettings()->setDirectSampling(settings.m_directSampling ? 1 : 0);
    response.getAndroidSdrDriverInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getAndroidSdrDriverInputSettings()->setGain(settings.m_gain[0]);
    response.getAndroidSdrDriverInputSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getAndroidSdrDriverInputSettings()->setRfBw(settings.m_rfBW);
    response.getAndroidSdrDriverInputSettings()->setSampleBits(settings.m_sampleBits);
    response.getAndroidSdrDriverInputSettings()->setDataPort(settings.m_dataPort);

    response.getAndroidSdrDriverInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAndroidSdrDriverInputSettings()->getReverseApiAddress()) {
        *response.getAndroidSdrDriverInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAndroidSdrDriverInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAndroidSdrDriverInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAndroidSdrDriverInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int AndroidSDRDriverInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAndroidSdrDriverInputReport(new SWGSDRangel::SWGAndroidSDRDriverInputReport());
    response.getAndroidSdrDriverInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void AndroidSDRDriverInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getAndroidSdrDriverInputReport()->setSampleRate(m_settings.m_devSampleRate);
}

void AndroidSDRDriverInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AndroidSDRDriverInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AndroidSDRDriverInput"));
    swgDeviceSettings->setAndroidSdrDriverInputSettings(new SWGSDRangel::SWGAndroidSDRDriverInputSettings());
    SWGSDRangel::SWGAndroidSDRDriverInputSettings *swgAndroidSDRDriverInputSettings = swgDeviceSettings->getAndroidSdrDriverInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgAndroidSDRDriverInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgAndroidSDRDriverInputSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("biasTee") || force) {
        swgAndroidSDRDriverInputSettings->setBiasTee(settings.m_biasTee ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dataPort") || force) {
        swgAndroidSDRDriverInputSettings->setDataPort(settings.m_dataPort);
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

void AndroidSDRDriverInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("AndroidSDRDriverInput"));

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

void AndroidSDRDriverInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AndroidSDRDriverInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AndroidSDRDriverInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
