///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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
#include <QCoreApplication>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGDeviceState.h"

#include "httplistener.h"
#include "settings/serializable.h"

#include "mcpprotocol.h"
#include "mcprequesthandler.h"
#include "mcpstreams.h"
#include "mcpserver.h"

MESSAGE_CLASS_DEFINITION(MCPServer::MsgConfigureMCPServer, Message)
MESSAGE_CLASS_DEFINITION(MCPServer::MsgStartStop, Message)

const char* const MCPServer::m_featureIdURI = "sdrangel.feature.mcpserver";
const char* const MCPServer::m_featureId = "MCPServer";

MCPServer::MCPServer(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_listener(nullptr)
{
    qDebug("MCPServer::MCPServer: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_protocol = new MCPProtocol(webAPIAdapterInterface);
    m_protocol->setCaptureDir(m_settings.m_captureDir);
    m_protocol->setStreams(&m_streams);
    m_protocol->setOwnerFeature(this);
    m_requestHandler = new MCPRequestHandler(m_protocol, &m_streams);
    m_notifier = new MCPNotifier(&m_streams, &m_protocol->getDataFeed(), &m_protocol->getTools());
    setState(StIdle);
    m_errorMessage = "MCPServer error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MCPServer::networkManagerFinished
    );
}

MCPServer::~MCPServer()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &MCPServer::networkManagerFinished
    );
    delete m_networkManager;
    stop();
    delete m_notifier;
    delete m_requestHandler;
    delete m_protocol;
}

void MCPServer::start()
{
    if (m_listener) {
        return;
    }

    qtwebapp::HttpListenerSettings listenerSettings;
    listenerSettings.host = m_settings.m_address;
    listenerSettings.port = m_settings.m_port;
    listenerSettings.maxRequestSize = 1 << 22; // 4 MB. Tool arguments and settings blobs can be large
    listenerSettings.readTimeout = 60000;      // Keep idle connections open a while, as clients pipeline requests
    m_requestHandler->setToken(m_settings.m_token);
    m_requestHandler->resetStatistics();
    m_listener = new qtwebapp::HttpListener(listenerSettings, m_requestHandler, qApp);

    if (m_listener->isListening())
    {
        qInfo("MCPServer::start: MCP server listening at %s", qPrintable(getServerURL()));
        setState(StRunning);
    }
    else
    {
        m_errorMessage = QString("Cannot listen on %1:%2: %3")
            .arg(m_settings.m_address)
            .arg(m_settings.m_port)
            .arg(m_listener->errorString());
        qWarning("MCPServer::start: %s", qPrintable(m_errorMessage));
        delete m_listener;
        m_listener = nullptr;
        setState(StError);
    }
}

void MCPServer::stop()
{
    // Deleting the listener waits for every HTTP thread to finish, on this, the main thread,
    // so anything those threads are waiting for from the main thread must be given up first:
    // the event streams, each served by a thread blocked inside the request handler, and the
    // tool calls in progress, which wait for channels and devices that only the main thread
    // creates and, for an audio capture, for the main thread itself
    m_protocol->setStopping(true);
    m_streams.closeAll();

    if (m_listener)
    {
        qInfo("MCPServer::stop: stopping MCP server at %s", qPrintable(getServerURL()));
        delete m_listener;
        m_listener = nullptr;
    }

    // No HTTP thread is left, so the next start() begins with the tools able to wait again
    m_protocol->setStopping(false);
    setState(StIdle);
}

bool MCPServer::handleMessage(const Message& cmd)
{
	if (MsgConfigureMCPServer::match(cmd))
	{
        MsgConfigureMCPServer& cfg = (MsgConfigureMCPServer&) cmd;
        qDebug() << "MCPServer::handleMessage: MsgConfigureMCPServer";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

		return true;
	}
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "MCPServer::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
	else
	{
		return false;
	}
}

QByteArray MCPServer::serialize() const
{
    return m_settings.serialize();
}

bool MCPServer::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureMCPServer *msg = MsgConfigureMCPServer::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureMCPServer *msg = MsgConfigureMCPServer::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void MCPServer::applySettings(const MCPServerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "MCPServer::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    bool restart = m_listener && (force
        || settingsKeys.contains("address")
        || settingsKeys.contains("port"));

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (restart)
    {
        stop();
        start();
    }
    else if (settingsKeys.contains("token") || force)
    {
        m_requestHandler->setToken(m_settings.m_token);
    }

    if (settingsKeys.contains("captureDir") || force) {
        m_protocol->setCaptureDir(m_settings.m_captureDir);
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
                settingsKeys.contains("reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }
}

int MCPServer::getRequestCount() const
{
    return m_requestHandler->getRequestCount();
}

int MCPServer::getStreamCount() const
{
    return m_streams.streamCount();
}

QString MCPServer::getLastRequest() const
{
    return m_requestHandler->getLastRequest();
}

QString MCPServer::getServerURL() const
{
    QString host = m_settings.m_address.isEmpty() ? "0.0.0.0" : m_settings.m_address;
    return QString("http://%1:%2/mcp").arg(host).arg(m_settings.m_port);
}

int MCPServer::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int MCPServer::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMcpServerSettings(new SWGSDRangel::SWGMCPServerSettings());
    response.getMcpServerSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int MCPServer::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    if (featureSettingsKeys.contains("port") && response.getMcpServerSettings())
    {
        qint32 port = response.getMcpServerSettings()->getPort();

        if (!MCPServerSettings::isValidPort((quint32) port))
        {
            errorMessage = QString("port must be between %1 and %2")
                .arg(MCPServerSettings::m_minPort).arg(MCPServerSettings::m_maxPort);
            return 400;
        }
    }

    MCPServerSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureMCPServer *msg = MsgConfigureMCPServer::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMCPServer *msgToGUI = MsgConfigureMCPServer::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int MCPServer::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMcpServerReport(new SWGSDRangel::SWGMCPServerReport());
    response.getMcpServerReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

void MCPServer::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    response.getMcpServerReport()->setRunningState(getState());
    response.getMcpServerReport()->setRequestCount(getRequestCount());
}

int MCPServer::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGMCPServerActions *swgMCPServerActions = query.getMcpServerActions();

    if (swgMCPServerActions)
    {
        if (featureActionsKeys.contains("run"))
        {
            bool featureRun = swgMCPServerActions->getRun() != 0;
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
        errorMessage = "Missing MCPServerActions in query";
        return 400;
    }
}

void MCPServer::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const MCPServerSettings& settings)
{
    SWGSDRangel::SWGMCPServerSettings *swg = response.getMcpServerSettings();

    if (swg->getAddress()) {
        *swg->getAddress() = settings.m_address;
    } else {
        swg->setAddress(new QString(settings.m_address));
    }

    swg->setPort(settings.m_port);

    if (swg->getToken()) {
        *swg->getToken() = settings.m_token;
    } else {
        swg->setToken(new QString(settings.m_token));
    }

    if (swg->getCaptureDir()) {
        *swg->getCaptureDir() = settings.m_captureDir;
    } else {
        swg->setCaptureDir(new QString(settings.m_captureDir));
    }

    if (swg->getTitle()) {
        *swg->getTitle() = settings.m_title;
    } else {
        swg->setTitle(new QString(settings.m_title));
    }

    swg->setRgbColor(settings.m_rgbColor);
    swg->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (swg->getReverseApiAddress()) {
        *swg->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        swg->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    swg->setReverseApiPort(settings.m_reverseAPIPort);
    swg->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    swg->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (swg->getRollupState())
        {
            settings.m_rollupState->formatTo(swg->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            swg->setRollupState(swgRollupState);
        }
    }
}

void MCPServer::webapiUpdateFeatureSettings(
    MCPServerSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    SWGSDRangel::SWGMCPServerSettings *swg = response.getMcpServerSettings();

    if (featureSettingsKeys.contains("address")) {
        settings.m_address = *swg->getAddress();
    }
    if (featureSettingsKeys.contains("port")) {
        // Out of range values are rejected by webapiSettingsPutPatch before this is reached;
        // ignore any that arrive by another route rather than handing them to the listener
        qint32 port = swg->getPort();

        if (MCPServerSettings::isValidPort((quint32) port)) {
            settings.m_port = (quint32) port;
        }
    }
    if (featureSettingsKeys.contains("token")) {
        settings.m_token = swg->getToken() ? *swg->getToken() : "";
    }
    if (featureSettingsKeys.contains("captureDir")) {
        settings.m_captureDir = swg->getCaptureDir() ? *swg->getCaptureDir() : QString();
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *swg->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = swg->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = swg->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *swg->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = swg->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = swg->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = swg->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, swg->getRollupState());
    }
}

void MCPServer::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const MCPServerSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    swgFeatureSettings->setFeatureType(new QString(m_featureId));
    swgFeatureSettings->setMcpServerSettings(new SWGSDRangel::SWGMCPServerSettings());
    SWGSDRangel::SWGMCPServerSettings *swg = swgFeatureSettings->getMcpServerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("address") || force) {
        swg->setAddress(new QString(settings.m_address));
    }
    if (featureSettingsKeys.contains("port") || force) {
        swg->setPort(settings.m_port);
    }
    if (featureSettingsKeys.contains("token") || force) {
        swg->setToken(new QString(settings.m_token));
    }
    if (featureSettingsKeys.contains("captureDir") || force) {
        swg->setCaptureDir(new QString(settings.m_captureDir));
    }
    if (featureSettingsKeys.contains("title") || force) {
        swg->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swg->setRgbColor(settings.m_rgbColor);
    }

    QString featureSettingsURL = QString("http://%1:%2/sdrangel/featureset/%3/feature/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIFeatureSetIndex)
            .arg(settings.m_reverseAPIFeatureIndex);
    m_networkRequest.setUrl(QUrl(featureSettingsURL));
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

void MCPServer::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "MCPServer::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("MCPServer::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
