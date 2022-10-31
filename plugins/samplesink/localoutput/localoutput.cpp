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
#include "SWGLocalOutputReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"

#include "localoutput.h"

MESSAGE_CLASS_DEFINITION(LocalOutput::MsgConfigureLocalOutput, Message)
MESSAGE_CLASS_DEFINITION(LocalOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(LocalOutput::MsgReportSampleRateAndFrequency, Message)

LocalOutput::LocalOutput(DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_centerFrequency(0),
    m_sampleRate(48000),
	m_deviceDescription("LocalOutput")
{
	m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_sampleRate));
    m_deviceAPI->setNbSinkStreams(1);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LocalOutput::networkManagerFinished
    );
}

LocalOutput::~LocalOutput()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &LocalOutput::networkManagerFinished
    );
    delete m_networkManager;
	stop();
}

void LocalOutput::destroy()
{
    delete this;
}

void LocalOutput::init()
{
    applySettings(m_settings, QList<QString>(), true);
}

bool LocalOutput::start()
{
	qDebug() << "LocalOutput::start";
	return true;
}

void LocalOutput::stop()
{
	qDebug() << "LocalOutput::stop";
}

QByteArray LocalOutput::serialize() const
{
    return m_settings.serialize();
}

bool LocalOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureLocalOutput* message = MsgConfigureLocalOutput::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLocalOutput* messageToGUI = MsgConfigureLocalOutput::create(m_settings, QList<QString>(), true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

void LocalOutput::setMessageQueueToGUI(MessageQueue *queue)
{
    m_guiMessageQueue = queue;
}

const QString& LocalOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int LocalOutput::getSampleRate() const
{
    return m_sampleRate;
}

void LocalOutput::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
    m_sampleSourceFifo.resize(SampleSourceFifo::getSizePolicy(m_sampleRate));

    DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency); // Frequency in Hz for the DSP engine
    m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

    if (getMessageQueueToGUI())
    {
        MsgReportSampleRateAndFrequency *msg = MsgReportSampleRateAndFrequency::create(m_sampleRate, m_centerFrequency);
        getMessageQueueToGUI()->push(msg);
    }
}

quint64 LocalOutput::getCenterFrequency() const
{
    return m_centerFrequency;
}

void LocalOutput::setCenterFrequency(qint64 centerFrequency)
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

bool LocalOutput::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        return false;
    }
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "LocalOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else if (MsgConfigureLocalOutput::match(message))
    {
        qDebug() << "LocalOutput::handleMessage:" << message.getIdentifier();
        MsgConfigureLocalOutput& conf = (MsgConfigureLocalOutput&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
	else
	{
		return false;
	}
}

void LocalOutput::applySettings(const LocalOutputSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "LocalOutput::applySettings: force:" << force << settings.getDebugString(settingsKeys, force);
    QMutexLocker mutexLocker(&m_mutex);
    std::ostringstream os;
    QString remoteAddress;
    QList<QString> reverseAPIKeys;

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

int LocalOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage)
{
    (void) errorMessage;
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int LocalOutput::webapiRun(
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

int LocalOutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setLocalOutputSettings(new SWGSDRangel::SWGLocalOutputSettings());
    response.getLocalOutputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int LocalOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage)
{
    (void) errorMessage;
    LocalOutputSettings settings = m_settings;
    webapiUpdateDeviceSettings(settings, deviceSettingsKeys, response);

    MsgConfigureLocalOutput *msg = MsgConfigureLocalOutput::create(settings, deviceSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLocalOutput *msgToGUI = MsgConfigureLocalOutput::create(settings, deviceSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void LocalOutput::webapiUpdateDeviceSettings(
        LocalOutputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response)
{
    if (deviceSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLocalOutputSettings()->getUseReverseApi() != 0;
    }
    if (deviceSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLocalOutputSettings()->getReverseApiAddress();
    }
    if (deviceSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLocalOutputSettings()->getReverseApiPort();
    }
    if (deviceSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLocalOutputSettings()->getReverseApiDeviceIndex();
    }
}

void LocalOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const LocalOutputSettings& settings)
{
    response.getLocalOutputSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLocalOutputSettings()->getReverseApiAddress()) {
        *response.getLocalOutputSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLocalOutputSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLocalOutputSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLocalOutputSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
}

int LocalOutput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLocalOutputReport(new SWGSDRangel::SWGLocalOutputReport());
    response.getLocalOutputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void LocalOutput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getLocalOutputReport()->setCenterFrequency(m_centerFrequency);
    response.getLocalOutputReport()->setSampleRate(m_sampleRate);
}

void LocalOutput::webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const LocalOutputSettings& settings, bool force)
{
    (void) deviceSettingsKeys;
    (void) force;
    SWGSDRangel::SWGDeviceSettings *swgDeviceSettings = new SWGSDRangel::SWGDeviceSettings();
    swgDeviceSettings->setDirection(1); // single Tx
    swgDeviceSettings->setOriginatorIndex(m_deviceAPI->getDeviceSetIndex());
    swgDeviceSettings->setDeviceHwType(new QString("LocalOutput"));
    swgDeviceSettings->setLocalOutputSettings(new SWGSDRangel::SWGLocalOutputSettings());

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

void LocalOutput::webapiReverseSendStartStop(bool start)
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

void LocalOutput::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LocalOutput::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("LocalOutput::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
