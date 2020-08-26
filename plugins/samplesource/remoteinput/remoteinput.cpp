///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGRemoteInputReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"

#include "remoteinput.h"
#include "remoteinputudphandler.h"

MESSAGE_CLASS_DEFINITION(RemoteInput::MsgConfigureRemoteInput, Message)
MESSAGE_CLASS_DEFINITION(RemoteInput::MsgConfigureRemoteInputTiming, Message)
MESSAGE_CLASS_DEFINITION(RemoteInput::MsgReportRemoteInputAcquisition, Message)
MESSAGE_CLASS_DEFINITION(RemoteInput::MsgReportRemoteInputStreamData, Message)
MESSAGE_CLASS_DEFINITION(RemoteInput::MsgReportRemoteInputStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(RemoteInput::MsgStartStop, Message)

RemoteInput::RemoteInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_sampleRate(48000),
    m_mutex(QMutex::Recursive),
    m_settings(),
	m_remoteInputUDPHandler(nullptr),
	m_deviceDescription(),
	m_startingTimeStamp(0)
{
	m_sampleFifo.setSize(m_sampleRate * 8);
	m_remoteInputUDPHandler = new RemoteInputUDPHandler(&m_sampleFifo, m_deviceAPI);
    m_remoteInputUDPHandler->setMessageQueueToInput(&m_inputMessageQueue);

    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

RemoteInput::~RemoteInput()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
	stop();
    delete m_remoteInputUDPHandler;
}

void RemoteInput::destroy()
{
    delete this;
}

void RemoteInput::init()
{
    applySettings(m_settings, true);
}

bool RemoteInput::start()
{
	qDebug() << "RemoteInput::start";
    m_remoteInputUDPHandler->start();
	return true;
}

void RemoteInput::stop()
{
	qDebug() << "RemoteInput::stop";
}

QByteArray RemoteInput::serialize() const
{
    return m_settings.serialize();
}

bool RemoteInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureRemoteInput* message = MsgConfigureRemoteInput::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureRemoteInput* messageToGUI = MsgConfigureRemoteInput::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

void RemoteInput::setMessageQueueToGUI(MessageQueue *queue)
{
    m_guiMessageQueue = queue;
    m_remoteInputUDPHandler->setMessageQueueToGUI(queue);
}

const QString& RemoteInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int RemoteInput::getSampleRate() const
{
    return m_remoteInputUDPHandler->getSampleRate();
}

quint64 RemoteInput::getCenterFrequency() const
{
    return m_remoteInputUDPHandler->getCenterFrequency();
}

void RemoteInput::setCenterFrequency(qint64 centerFrequency)
{
    (void) centerFrequency;
}

std::time_t RemoteInput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool RemoteInput::isStreaming() const
{
    return m_remoteInputUDPHandler->isStreaming();
}

bool RemoteInput::handleMessage(const Message& message)
{
    if (RemoteInputUDPHandler::MsgReportSampleRateChange::match(message))
    {
        RemoteInputUDPHandler::MsgReportSampleRateChange& notif = (RemoteInputUDPHandler::MsgReportSampleRateChange&) message;
        int sampleRate = notif.getSampleRate();

        if (sampleRate != m_sampleRate)
        {
            qDebug("RemoteInput::handleMessage: RemoteInputUDPHandler::MsgReportSampleRateChange: %d", sampleRate);
            QMutexLocker mutexLocker(&m_mutex);
            m_sampleFifo.setSize(sampleRate * 8);
            m_sampleRate = sampleRate;
        }

        return true;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "RemoteInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (MsgConfigureRemoteInput::match(message))
    {
        qDebug() << "RemoteInput::handleMessage:" << message.getIdentifier();
        MsgConfigureRemoteInput& conf = (MsgConfigureRemoteInput&) message;
        applySettings(conf.getSettings(), conf.getForce());
        return true;
    }
	else
	{
		return false;
	}
}

void RemoteInput::applySettings(const RemoteInputSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);
    std::ostringstream os;
    QString remoteAddress;
    m_remoteInputUDPHandler->getRemoteAddress(remoteAddress);
    QList<QString> reverseAPIKeys;

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force) {
        reverseAPIKeys.append("dcBlock");
    }
    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force) {
        reverseAPIKeys.append("iqCorrection");
    }
    if ((m_settings.m_dataAddress != settings.m_dataAddress) || force) {
        reverseAPIKeys.append("dataAddress");
    }
    if ((m_settings.m_dataPort != settings.m_dataPort) || force) {
        reverseAPIKeys.append("dataPort");
    }
    if ((m_settings.m_apiAddress != settings.m_apiAddress) || force) {
        reverseAPIKeys.append("apiAddress");
    }
    if ((m_settings.m_apiPort != settings.m_apiPort) || force) {
        reverseAPIKeys.append("apiPort");
    }
    if ((m_settings.m_multicastAddress != settings.m_multicastAddress) || force) {
        reverseAPIKeys.append("multicastAddress");
    }
    if ((m_settings.m_multicastJoin != settings.m_multicastJoin) || force) {
        reverseAPIKeys.append("multicastJoin");
    }

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || (m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
        qDebug("RemoteInput::applySettings: corrections: DC block: %s IQ imbalance: %s",
                settings.m_dcBlock ? "true" : "false",
                settings.m_iqCorrection ? "true" : "false");
    }

    if ((m_settings.m_dataAddress != settings.m_dataAddress) || 
        (m_settings.m_dataPort != settings.m_dataPort) || 
        (m_settings.m_multicastAddress != settings.m_multicastAddress) || 
        (m_settings.m_multicastJoin != settings.m_multicastJoin) || force) 
    {
        m_remoteInputUDPHandler->configureUDPLink(settings.m_dataAddress, settings.m_dataPort, settings.m_multicastAddress, settings.m_multicastJoin);
        m_remoteInputUDPHandler->getRemoteAddress(remoteAddress);
    }

    mutexLocker.unlock();

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
    m_remoteAddress = remoteAddress;

    qDebug() << "RemoteInput::applySettings: "
            << " m_dataAddress: " << m_settings.m_dataAddress
            << " m_dataPort: " << m_settings.m_dataPort
            << " m_multicastAddress: " << m_settings.m_multicastAddress
            << " m_multicastJoin: " << m_settings.m_multicastJoin
            << " m_apiAddress: " << m_settings.m_apiAddress
            << " m_apiPort: " << m_settings.m_apiPort
            << " m_remoteAddress: " << m_remoteAddress;
}

int RemoteInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int RemoteInput::webapiRun(
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

int RemoteInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteInputSettings(new SWGSDRangel::SWGRemoteInputSettings());
    response.getRemoteInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int RemoteInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    RemoteInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureRemoteInput *msg = MsgConfigureRemoteInput::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRemoteInput *msgToGUI = MsgConfigureRemoteInput::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void RemoteInput::webapiUpdateDeviceSettings(
    RemoteInputSettings& settings,
    const QStringList& deviceSettingsKeys,
    SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("apiAddress")) {
        settings.m_apiAddress = *response.getRemoteInputSettings()->getApiAddress();
    }
    if (deviceSettingsKeys.contains("apiPort")) {
        settings.m_apiPort = response.getRemoteInputSettings()->getApiPort();
    }
    if (deviceSettingsKeys.contains("dataAddress")) {
        settings.m_dataAddress = *response.getRemoteInputSettings()->getDataAddress();
    }
    if (deviceSettingsKeys.contains("dataPort")) {
        settings.m_dataPort = response.getRemoteInputSettings()->getDataPort();
    }
    if (deviceSettingsKeys.contains("multicastAddress")) {
        settings.m_multicastAddress = *response.getRemoteInputSettings()->getMulticastAddress();
    }
    if (deviceSettingsKeys.contains("multicastAddress")) {
        settings.m_multicastJoin = response.getRemoteInputSettings()->getMulticastJoin() != 0;
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getRemoteInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getRemoteInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRemoteInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRemoteInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRemoteInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRemoteInputSettings()->getReverseApiDeviceIndex();
    }
}

void RemoteInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const RemoteInputSettings& settings)
{
    response.getRemoteInputSettings()->setApiAddress(new QString(settings.m_apiAddress));
    response.getRemoteInputSettings()->setApiPort(settings.m_apiPort);
    response.getRemoteInputSettings()->setDataAddress(new QString(settings.m_dataAddress));
    response.getRemoteInputSettings()->setDataPort(settings.m_dataPort);
    response.getRemoteInputSettings()->setMulticastAddress(new QString(settings.m_multicastAddress));
    response.getRemoteInputSettings()->setMulticastJoin(settings.m_multicastJoin ? 1 : 0);
    response.getRemoteInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getRemoteInputSettings()->setIqCorrection(settings.m_iqCorrection);

    response.getRemoteInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRemoteInputSettings()->getReverseApiAddress()) {
        *response.getRemoteInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRemoteInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRemoteInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRemoteInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int RemoteInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRemoteInputReport(new SWGSDRangel::SWGRemoteInputReport());
    response.getRemoteInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void RemoteInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getRemoteInputReport()->setCenterFrequency(m_remoteInputUDPHandler->getCenterFrequency());
    response.getRemoteInputReport()->setSampleRate(m_remoteInputUDPHandler->getSampleRate());
    response.getRemoteInputReport()->setBufferRwBalance(m_remoteInputUDPHandler->getBufferGauge());

    QDateTime dt = QDateTime::fromMSecsSinceEpoch(m_remoteInputUDPHandler->getTVmSec());
    response.getRemoteInputReport()->setRemoteTimestamp(new QString(dt.toString("yyyy-MM-dd  HH:mm:ss.zzz")));

    response.getRemoteInputReport()->setMinNbBlocks(m_remoteInputUDPHandler->getMinNbBlocks());
    response.getRemoteInputReport()->setMaxNbRecovery(m_remoteInputUDPHandler->getMaxNbRecovery());
}

void RemoteInput::webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const RemoteInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("RemoteInput"));
    swgDeviceSettings->setRemoteInputSettings(new SWGSDRangel::SWGRemoteInputSettings());
    SWGSDRangel::SWGRemoteInputSettings *swgRemoteInputSettings = swgDeviceSettings->getRemoteInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("apiAddress") || force) {
        swgRemoteInputSettings->setApiAddress(new QString(settings.m_apiAddress));
    }
    if (deviceSettingsKeys.contains("apiPort") || force) {
        swgRemoteInputSettings->setApiPort(settings.m_apiPort);
    }
    if (deviceSettingsKeys.contains("dataAddress") || force) {
        swgRemoteInputSettings->setDataAddress(new QString(settings.m_dataAddress));
    }
    if (deviceSettingsKeys.contains("dataPort") || force) {
        swgRemoteInputSettings->setDataPort(settings.m_dataPort);
    }
    if (deviceSettingsKeys.contains("multicastAddress") || force) {
        swgRemoteInputSettings->setMulticastAddress(new QString(settings.m_multicastAddress));
    }
    if (deviceSettingsKeys.contains("multicastJoin") || force) {
        swgRemoteInputSettings->setMulticastJoin(settings.m_multicastJoin ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgRemoteInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgRemoteInputSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
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

void RemoteInput::webapiReverseSendStartStop(bool start)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("RemoteInput"));

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

void RemoteInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RemoteInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RemoteInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
