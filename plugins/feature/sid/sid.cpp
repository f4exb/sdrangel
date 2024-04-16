///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include "SWGDeviceState.h"

#include "feature/featureset.h"
#include "settings/serializable.h"

#include "sid.h"
#include "sidworker.h"

MESSAGE_CLASS_DEFINITION(SIDMain::MsgConfigureSID, Message)
MESSAGE_CLASS_DEFINITION(SIDMain::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(SIDMain::MsgReportWorker, Message)
MESSAGE_CLASS_DEFINITION(SIDMain::MsgMeasurement, Message)

const char* const SIDMain::m_featureIdURI = "sdrangel.feature.sid";
const char* const SIDMain::m_featureId = "SID";

SIDMain::SIDMain(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_thread(nullptr),
    m_worker(nullptr)
{
    qDebug("SIDMain::SID: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "SID error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SIDMain::networkManagerFinished
    );
}

SIDMain::~SIDMain()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SIDMain::networkManagerFinished
    );
    delete m_networkManager;
}

void SIDMain::start()
{
    qDebug("SIDMain::start");
    m_thread = new QThread();
    m_worker = new SIDWorker(this, m_webAPIAdapterInterface);
    m_worker->moveToThread(m_thread);
    QObject::connect(m_thread, &QThread::started, m_worker, &SIDWorker::startWork);
    QObject::connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    QObject::connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);
    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    m_thread->start();
    m_state = StRunning;
    MsgConfigureSID *msg = MsgConfigureSID::create(m_settings, QList<QString>(), true);
    m_worker->getInputMessageQueue()->push(msg);
}

void SIDMain::stop()
{
    qDebug("SIDMain::stop");
    m_state = StIdle;
    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait();
        m_thread = nullptr;
        m_worker = nullptr;
    }
}

bool SIDMain::handleMessage(const Message& cmd)
{
    if (MsgConfigureSID::match(cmd))
    {
        MsgConfigureSID& cfg = (MsgConfigureSID&) cmd;
        qDebug() << "SIDMain::handleMessage: MsgConfigureSID";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "SIDMain::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (MsgReportWorker::match(cmd))
    {
        MsgReportWorker& report = (MsgReportWorker&) cmd;
        m_state = StError;
        m_errorMessage = report.getMessage();
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray SIDMain::serialize() const
{
    return m_settings.serialize();
}

bool SIDMain::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSID *msg = MsgConfigureSID::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSID *msg = MsgConfigureSID::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SIDMain::applySettings(const SIDSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "SIDMain::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;


    if (m_worker)
    {
        MsgConfigureSID *msg = MsgConfigureSID::create(settings, settingsKeys, force);
        m_worker->getInputMessageQueue()->push(msg);
    }

    if (settings.m_useReverseAPI)
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

int SIDMain::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int SIDMain::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setSidSettings(new SWGSDRangel::SWGSIDSettings());
    response.getSidSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int SIDMain::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    SIDSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureSID *msg = MsgConfigureSID::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSID *msgToGUI = MsgConfigureSID::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void SIDMain::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const SIDSettings& settings)
{
    if (response.getSidSettings()->getTitle()) {
        *response.getSidSettings()->getTitle() = settings.m_title;
    } else {
        response.getSidSettings()->setTitle(new QString(settings.m_title));
    }

    response.getSidSettings()->setRgbColor(settings.m_rgbColor);
    response.getSidSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSidSettings()->getReverseApiAddress()) {
        *response.getSidSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSidSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSidSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSidSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getSidSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getSidSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getSidSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getSidSettings()->setRollupState(swgRollupState);
        }
    }
}

void SIDMain::webapiUpdateFeatureSettings(
    SIDSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getSidSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getSidSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSidSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSidSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSidSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getSidSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getSidSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getSidSettings()->getRollupState());
    }
}

void SIDMain::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const SIDSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("SID"));
    swgFeatureSettings->setSidSettings(new SWGSDRangel::SWGSIDSettings());
    SWGSDRangel::SWGSIDSettings *swgSIDSettings = swgFeatureSettings->getSidSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("title") || force) {
        swgSIDSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgSIDSettings->setRgbColor(settings.m_rgbColor);
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

void SIDMain::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SIDMain::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SIDMain::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
