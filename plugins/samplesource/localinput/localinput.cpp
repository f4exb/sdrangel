///////////////////////////////////////////////////////////////////////////////////
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
#include <QBuffer>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGLocalInputReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"

#include "localinput.h"

MESSAGE_CLASS_DEFINITION(LocalInput::MsgConfigureLocalInput, Message)
MESSAGE_CLASS_DEFINITION(LocalInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(LocalInput::MsgReportSampleRateAndFrequency, Message)

LocalInput::LocalInput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_centerFrequency(0),
	m_deviceDescription("LocalInput")
{
    m_sampleFifo.setLabel(m_deviceDescription);
	m_sampleFifo.setSize(96000 * 4);

    m_deviceAPI->setNbSourceStreams(1);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LocalInput::networkManagerFinished
    );
}

LocalInput::~LocalInput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LocalInput::networkManagerFinished
    );
    delete m_networkManager;
	stop();
}

void LocalInput::destroy()
{
    delete this;
}

void LocalInput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool LocalInput::start()
{
	qDebug() << "LocalInput::start";
	return true;
}

void LocalInput::stop()
{
	qDebug() << "LocalInput::stop";
}

QByteArray LocalInput::serialize() const
{
    return m_settings.serialize();
}

bool LocalInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureLocalInput* message = MsgConfigureLocalInput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLocalInput* messageToGUI = MsgConfigureLocalInput::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

void LocalInput::setMessageQueueToGUI(MessageQueue *queue)
{
    m_guiMessageQueue = queue;
}

const QString& LocalInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int LocalInput::getSampleRate() const
{
    return m_sampleRate;
}

void LocalInput::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;

    DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency); // Frequency in Hz for the DSP engine
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

    if (getMessageQueueToGUI())
    {
        MsgReportSampleRateAndFrequency *msg = MsgReportSampleRateAndFrequency::create(m_sampleRate, m_centerFrequency);
        getMessageQueueToGUI()->push(msg);
    }
}

quint64 LocalInput::getCenterFrequency() const
{
    return m_centerFrequency;
}

void LocalInput::setCenterFrequency(qint64 centerFrequency)
{
    m_centerFrequency = centerFrequency;

    DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency); // Frequency in Hz for the DSP engine
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

    if (getMessageQueueToGUI())
    {
        MsgReportSampleRateAndFrequency *msg = MsgReportSampleRateAndFrequency::create(m_sampleRate, m_centerFrequency);
        getMessageQueueToGUI()->push(msg);
    }
}

bool LocalInput::handleMessage(const Message& message)
{
    if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "LocalInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (MsgConfigureLocalInput::match(message))
    {
        qDebug() << "LocalInput::handleMessage:" << message.getIdentifier();
        MsgConfigureLocalInput& conf = (MsgConfigureLocalInput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
	else
	{
		return false;
	}
}

void LocalInput::applySettings(const LocalInputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "LocalInput::applySettings: force: " << force << settings.getDebugString(settingsKeys, force);
    QMutexLocker mutexLocker(&m_mutex);
    std::ostringstream os;
    QString remoteAddress;

    if (settingsKeys.contains("dcBlock") || settingsKeys.contains("iqCorrection") || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
        qDebug("LocalInput::applySettings: corrections: DC block: %s IQ imbalance: %s",
                settings.m_dcBlock ? "true" : "false",
                settings.m_iqCorrection ? "true" : "false");
    }

    mutexLocker.unlock();

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

    m_remoteAddress = remoteAddress;
}

int LocalInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int LocalInput::webapiRun(
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

int LocalInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setLocalInputSettings(new SWGSDRangel::SWGLocalInputSettings());
    response.getLocalInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int LocalInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    LocalInputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureLocalInput *msg = MsgConfigureLocalInput::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLocalInput *msgToGUI = MsgConfigureLocalInput::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void LocalInput::webapiUpdateDeviceSettings(
        LocalInputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getLocalInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getLocalInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLocalInputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLocalInputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLocalInputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLocalInputSettings()->getReverseApiDeviceIndex();
    }
}

void LocalInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const LocalInputSettings& settings)
{
    response.getLocalInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getLocalInputSettings()->setIqCorrection(settings.m_iqCorrection);

    response.getLocalInputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLocalInputSettings()->getReverseApiAddress()) {
        *response.getLocalInputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLocalInputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLocalInputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLocalInputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int LocalInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLocalInputReport(new SWGSDRangel::SWGLocalInputReport());
    response.getLocalInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void LocalInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getLocalInputReport()->setCenterFrequency(m_centerFrequency);
    response.getLocalInputReport()->setSampleRate(m_sampleRate);
}

void LocalInput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const LocalInputSettings& settings, bool force)
{
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(0); // single Rx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("LocalInput"));
    swgDeviceSettings->setLocalInputSettings(new SWGSDRangel::SWGLocalInputSettings());
    SWGSDRangel::SWGLocalInputSettings *swgLocalInputSettings = swgDeviceSettings->getLocalInputSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (deviceSettingsKeys.contains("dcBlock") || force) {
        swgLocalInputSettings->setDcBlock(settings.m_dcBlock ? 1 : 0);
    }
    if (deviceSettingsKeys.contains("iqCorrection") || force) {
        swgLocalInputSettings->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
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

void LocalInput::webapiReverseSendStartStop(bool start)
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

void LocalInput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LocalInput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("LocalInput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
