///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGDeviceState.h"

#include "dsp/dspengine.h"
#include "settings/serializable.h"

#include "rigctlserverworker.h"
#include "rigctlserver.h"

MESSAGE_CLASS_DEFINITION(RigCtlServer::MsgConfigureRigCtlServer, Message)
MESSAGE_CLASS_DEFINITION(RigCtlServer::MsgStartStop, Message)

const char* const RigCtlServer::m_featureIdURI = "sdrangel.feature.rigctlserver";
const char* const RigCtlServer::m_featureId = "RigCtlServer";

RigCtlServer::RigCtlServer(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    qDebug("RigCtlServer::RigCtlServer: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_worker = new RigCtlServerWorker(webAPIAdapterInterface);
    m_worker->moveToThread(&m_thread);
    m_state = StIdle;
    m_errorMessage = "RigCtlServer error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RigCtlServer::networkManagerFinished
    );
}

RigCtlServer::~RigCtlServer()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RigCtlServer::networkManagerFinished
    );
    delete m_networkManager;
    if (m_worker->isRunning()) {
        stop();
    }

    delete m_worker;
}

void RigCtlServer::start()
{
	qDebug("RigCtlServer::start");

    m_worker->reset();
    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    bool ok = m_worker->startWork();
    m_state = ok ? StRunning : StError;
    m_thread.start();

    RigCtlServerWorker::MsgConfigureRigCtlServerWorker *msg = RigCtlServerWorker::MsgConfigureRigCtlServerWorker::create(
        m_settings, QList<QString>(), true);
    m_worker->getInputMessageQueue()->push(msg);
}

void RigCtlServer::stop()
{
    qDebug("RigCtlServer::stop");
	m_worker->stopWork();
    m_state = StIdle;
	m_thread.quit();
	m_thread.wait();
}

bool RigCtlServer::handleMessage(const Message& cmd)
{
	if (MsgConfigureRigCtlServer::match(cmd))
	{
        MsgConfigureRigCtlServer& cfg = (MsgConfigureRigCtlServer&) cmd;
        qDebug() << "RigCtlServer::handleMessage: MsgConfigureRigCtlServer";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

		return true;
	}
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "RigCtlServer::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (RigCtlServerSettings::MsgChannelIndexChange::match(cmd))
    {
        RigCtlServerSettings::MsgChannelIndexChange& cfg = (RigCtlServerSettings::MsgChannelIndexChange&) cmd;
        int newChannelIndex = cfg.getIndex();
        qDebug() << "RigCtlServer::handleMessage: MsgChannelIndexChange: " << newChannelIndex;
        RigCtlServerSettings settings = m_settings;
        settings.m_channelIndex = newChannelIndex;
        applySettings(settings, QList<QString>{"channelIndex"}, false);

        if (getMessageQueueToGUI())
        {
            RigCtlServerSettings::MsgChannelIndexChange *msg = new RigCtlServerSettings::MsgChannelIndexChange(cfg);
            getMessageQueueToGUI()->push(msg);
        }

        return true;
    }
	else
	{
		return false;
	}
}

QByteArray RigCtlServer::serialize() const
{
    return m_settings.serialize();
}

bool RigCtlServer::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureRigCtlServer *msg = MsgConfigureRigCtlServer::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRigCtlServer *msg = MsgConfigureRigCtlServer::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void RigCtlServer::applySettings(const RigCtlServerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "RigCtlServer::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    RigCtlServerWorker::MsgConfigureRigCtlServerWorker *msg = RigCtlServerWorker::MsgConfigureRigCtlServerWorker::create(
        settings, settingsKeys, force
    );
    m_worker->getInputMessageQueue()->push(msg);

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
                settingsKeys.contains("m_reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

int RigCtlServer::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int RigCtlServer::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setRigCtlServerSettings(new SWGSDRangel::SWGRigCtlServerSettings());
    response.getRigCtlServerSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int RigCtlServer::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    RigCtlServerSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureRigCtlServer *msg = MsgConfigureRigCtlServer::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("RigCtlServer::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRigCtlServer *msgToGUI = MsgConfigureRigCtlServer::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int RigCtlServer::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setRigCtlServerReport(new SWGSDRangel::SWGRigCtlServerReport());
    response.getRigCtlServerReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

void RigCtlServer::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    response.getRigCtlServerReport()->setRunningState(getState());
}

int RigCtlServer::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGRigCtlServerActions *swgRigCtlServerActions = query.getRigCtlServerActions();

    if (swgRigCtlServerActions)
    {
        if (featureActionsKeys.contains("run"))
        {
            bool featureRun = swgRigCtlServerActions->getRun() != 0;
            MsgStartStop *msg = MsgStartStop::create(featureRun);
            getInputMessageQueue()->push(msg);
            return 202;
        }
        else
        {
            errorMessage = "Unknown action";
            return 400;
        }
    }
    else
    {
        errorMessage = "Missing RigCtlServerActions in query";
        return 400;
    }
}

void RigCtlServer::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const RigCtlServerSettings& settings)
{
    response.getRigCtlServerSettings()->setEnabled(settings.m_enabled ? 1 : 0);
    response.getRigCtlServerSettings()->setDeviceIndex(settings.m_deviceIndex);
    response.getRigCtlServerSettings()->setChannelIndex(settings.m_channelIndex);
    response.getRigCtlServerSettings()->setRigCtlPort(settings.m_rigCtlPort);
    response.getRigCtlServerSettings()->setMaxFrequencyOffset(settings.m_maxFrequencyOffset);

    if (response.getRigCtlServerSettings()->getTitle()) {
        *response.getRigCtlServerSettings()->getTitle() = settings.m_title;
    } else {
        response.getRigCtlServerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getRigCtlServerSettings()->setRgbColor(settings.m_rgbColor);
    response.getRigCtlServerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRigCtlServerSettings()->getReverseApiAddress()) {
        *response.getRigCtlServerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRigCtlServerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRigCtlServerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRigCtlServerSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getRigCtlServerSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getRigCtlServerSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getRigCtlServerSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getRigCtlServerSettings()->setRollupState(swgRollupState);
        }
    }
}

void RigCtlServer::webapiUpdateFeatureSettings(
    RigCtlServerSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("enabled")) {
        settings.m_enabled = response.getRigCtlServerSettings()->getEnabled();
    }
    if (featureSettingsKeys.contains("deviceIndex")) {
        settings.m_deviceIndex = response.getRigCtlServerSettings()->getDeviceIndex();
    }
    if (featureSettingsKeys.contains("channelIndex")) {
        settings.m_channelIndex = response.getRigCtlServerSettings()->getChannelIndex();
    }
    if (featureSettingsKeys.contains("rigCtlPort")) {
        settings.m_rigCtlPort = response.getRigCtlServerSettings()->getRigCtlPort();
    }
    if (featureSettingsKeys.contains("maxFrequencyOffset")) {
        settings.m_maxFrequencyOffset = response.getRigCtlServerSettings()->getMaxFrequencyOffset();
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getRigCtlServerSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRigCtlServerSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRigCtlServerSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRigCtlServerSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRigCtlServerSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getRigCtlServerSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getRigCtlServerSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getRigCtlServerSettings()->getRollupState());
    }
}

void RigCtlServer::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const RigCtlServerSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("RigCtlServer"));
    swgFeatureSettings->setRigCtlServerSettings(new SWGSDRangel::SWGRigCtlServerSettings());
    SWGSDRangel::SWGRigCtlServerSettings *swgRigCtlServerSettings = swgFeatureSettings->getRigCtlServerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("enabled") || force) {
        swgRigCtlServerSettings->setEnabled(settings.m_enabled ? 1 : 0);
    }
    if (featureSettingsKeys.contains("deviceIndex") || force) {
        swgRigCtlServerSettings->setDeviceIndex(settings.m_deviceIndex);
    }
    if (featureSettingsKeys.contains("channelIndex") || force) {
        swgRigCtlServerSettings->setChannelIndex(settings.m_channelIndex);
    }
    if (featureSettingsKeys.contains("rigCtlPort") || force) {
        swgRigCtlServerSettings->setRigCtlPort(settings.m_rigCtlPort);
    }
    if (featureSettingsKeys.contains("maxFrequencyOffset") || force) {
        swgRigCtlServerSettings->setMaxFrequencyOffset(settings.m_maxFrequencyOffset);
    }
    if (featureSettingsKeys.contains("title") || force) {
        swgRigCtlServerSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgRigCtlServerSettings->setRgbColor(settings.m_rgbColor);
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/featureset/%3/feature/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIFeatureSetIndex)
            .arg(settings.m_reverseAPIFeatureIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgFeatureSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgFeatureSettings;
}

void RigCtlServer::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RigCtlServer::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RigCtlServer::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
