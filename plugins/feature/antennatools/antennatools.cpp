///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "dsp/dspengine.h"

#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "feature/featureset.h"
#include "settings/serializable.h"
#include "maincore.h"
#include "antennatools.h"

MESSAGE_CLASS_DEFINITION(AntennaTools::MsgConfigureAntennaTools, Message)

const char* const AntennaTools::m_featureIdURI = "sdrangel.feature.antennatools";
const char* const AntennaTools::m_featureId = "AntennaTools";

AntennaTools::AntennaTools(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    qDebug("AntennaTools::AntennaTools: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "AntennaTools error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AntennaTools::networkManagerFinished
    );
}

AntennaTools::~AntennaTools()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &AntennaTools::networkManagerFinished
    );
    delete m_networkManager;
}

bool AntennaTools::handleMessage(const Message& cmd)
{
    if (MsgConfigureAntennaTools::match(cmd))
    {
        MsgConfigureAntennaTools& cfg = (MsgConfigureAntennaTools&) cmd;
        qDebug() << "AntennaTools::handleMessage: MsgConfigureAntennaTools";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray AntennaTools::serialize() const
{
    return m_settings.serialize();
}

bool AntennaTools::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAntennaTools *msg = MsgConfigureAntennaTools::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAntennaTools *msg = MsgConfigureAntennaTools::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void AntennaTools::applySettings(const AntennaToolsSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "AntennaTools::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
+                settingsKeys.contains("m_reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

int AntennaTools::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setAntennaToolsSettings(new SWGSDRangel::SWGAntennaToolsSettings());
    response.getAntennaToolsSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int AntennaTools::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    AntennaToolsSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureAntennaTools *msg = MsgConfigureAntennaTools::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAntennaTools *msgToGUI = MsgConfigureAntennaTools::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void AntennaTools::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const AntennaToolsSettings& settings)
{
    response.getAntennaToolsSettings()->setDipoleFrequencyMHz(settings.m_dipoleFrequencyMHz);
    response.getAntennaToolsSettings()->setDipoleEndEffectFactor(settings.m_dipoleEndEffectFactor);
    response.getAntennaToolsSettings()->setDishFrequencyMHz(settings.m_dishFrequencyMHz);
    response.getAntennaToolsSettings()->setDishDiameter(settings.m_dishDiameter);
    response.getAntennaToolsSettings()->setDishDepth(settings.m_dishDepth);
    response.getAntennaToolsSettings()->setDishEfficiency(settings.m_dishEfficiency);
    if (response.getAntennaToolsSettings()->getTitle()) {
        *response.getAntennaToolsSettings()->getTitle() = settings.m_title;
    } else {
        response.getAntennaToolsSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAntennaToolsSettings()->setRgbColor(settings.m_rgbColor);
    response.getAntennaToolsSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAntennaToolsSettings()->getReverseApiAddress()) {
        *response.getAntennaToolsSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAntennaToolsSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAntennaToolsSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getAntennaToolsSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getAntennaToolsSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getAntennaToolsSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getAntennaToolsSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getAntennaToolsSettings()->setRollupState(swgRollupState);
        }
    }
}

void AntennaTools::webapiUpdateFeatureSettings(
    AntennaToolsSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("dipoleFrequencyMHz")) {
        settings.m_dipoleFrequencyMHz = response.getAntennaToolsSettings()->getDipoleFrequencyMHz();
    }
    if (featureSettingsKeys.contains("dipoleEndEffectFactor")) {
        settings.m_dipoleEndEffectFactor = response.getAntennaToolsSettings()->getDipoleEndEffectFactor();
    }
    if (featureSettingsKeys.contains("dishFrequencyMHz")) {
        settings.m_dishFrequencyMHz = response.getAntennaToolsSettings()->getDishFrequencyMHz();
    }
    if (featureSettingsKeys.contains("dishDiameter")) {
        settings.m_dishDiameter = response.getAntennaToolsSettings()->getDishDiameter();
    }
    if (featureSettingsKeys.contains("dishDepth")) {
        settings.m_dishDepth = response.getAntennaToolsSettings()->getDishDepth();
    }
    if (featureSettingsKeys.contains("dishEfficiency")) {
        settings.m_dishEfficiency = response.getAntennaToolsSettings()->getDishEfficiency();
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getAntennaToolsSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAntennaToolsSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAntennaToolsSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAntennaToolsSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAntennaToolsSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getAntennaToolsSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getAntennaToolsSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getAntennaToolsSettings()->getRollupState());
    }
}

void AntennaTools::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const AntennaToolsSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("AntennaTools"));
    swgFeatureSettings->setAntennaToolsSettings(new SWGSDRangel::SWGAntennaToolsSettings());
    SWGSDRangel::SWGAntennaToolsSettings *swgAntennaToolsSettings = swgFeatureSettings->getAntennaToolsSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("dipoleFrequencyMHz") || force) {
        swgAntennaToolsSettings->setDipoleFrequencyMHz(settings.m_dipoleFrequencyMHz);
    }
    if (featureSettingsKeys.contains("dipoleEndEffectFactor") || force) {
        swgAntennaToolsSettings->setDipoleEndEffectFactor(settings.m_dipoleEndEffectFactor);
    }
    if (featureSettingsKeys.contains("dishFrequencyMHz") || force) {
        swgAntennaToolsSettings->setDishFrequencyMHz(settings.m_dishFrequencyMHz);
    }
    if (featureSettingsKeys.contains("dishDiameter") || force) {
        swgAntennaToolsSettings->setDishDiameter(settings.m_dishDiameter);
    }
    if (featureSettingsKeys.contains("dishDepth") || force) {
        swgAntennaToolsSettings->setDishDepth(settings.m_dishDepth);
    }
    if (featureSettingsKeys.contains("dishEfficiency") || force) {
        swgAntennaToolsSettings->setDishEfficiency(settings.m_dishEfficiency);
    }
    if (featureSettingsKeys.contains("title") || force) {
        swgAntennaToolsSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgAntennaToolsSettings->setRgbColor(settings.m_rgbColor);
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

void AntennaTools::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AntennaTools::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AntennaTools::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
