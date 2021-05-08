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
#include <QTimer>

#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGDeviceState.h"

#include "dsp/dspengine.h"
#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "feature/featureset.h"
#include "maincore.h"

#include "ais.h"

MESSAGE_CLASS_DEFINITION(AIS::MsgConfigureAIS, Message)

const char* const AIS::m_featureIdURI = "sdrangel.feature.ais";
const char* const AIS::m_featureId = "AIS";

AIS::AIS(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    qDebug("AIS::AIS: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "AIS error";
    connect(&m_updatePipesTimer, SIGNAL(timeout()), this, SLOT(updatePipes()));
    m_updatePipesTimer.start(1000);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

AIS::~AIS()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
}

void AIS::start()
{
    qDebug("AIS::start");
    m_state = StRunning;
}

void AIS::stop()
{
    qDebug("AIS::stop");
    m_state = StIdle;
}

bool AIS::handleMessage(const Message& cmd)
{
    if (MsgConfigureAIS::match(cmd))
    {
        MsgConfigureAIS& cfg = (MsgConfigureAIS&) cmd;
        qDebug() << "AIS::handleMessage: MsgConfigureAIS";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MainCore::MsgPacket::match(cmd))
    {
        MainCore::MsgPacket& report = (MainCore::MsgPacket&) cmd;
        if (getMessageQueueToGUI())
        {
            MainCore::MsgPacket *copy = new MainCore::MsgPacket(report);
            getMessageQueueToGUI()->push(copy);
        }
        return true;
    }
    else
    {
        return false;
    }
}

void AIS::updatePipes()
{
    QList<AvailablePipeSource> availablePipes = updateAvailablePipeSources("ais", AISSettings::m_pipeTypes, AISSettings::m_pipeURIs, this);

    if (availablePipes != m_availablePipes) {
        m_availablePipes = availablePipes;
    }
}

QByteArray AIS::serialize() const
{
    return m_settings.serialize();
}

bool AIS::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureAIS *msg = MsgConfigureAIS::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureAIS *msg = MsgConfigureAIS::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void AIS::applySettings(const AISSettings& settings, bool force)
{
    qDebug() << "AIS::applySettings:"
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIFeatureSetIndex: " << settings.m_reverseAPIFeatureSetIndex
            << " m_reverseAPIFeatureIndex: " << settings.m_reverseAPIFeatureIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((m_settings.m_rgbColor != settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIFeatureSetIndex != settings.m_reverseAPIFeatureSetIndex) ||
                (m_settings.m_reverseAPIFeatureIndex != settings.m_reverseAPIFeatureIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

int AIS::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setAisSettings(new SWGSDRangel::SWGAISSettings());
    response.getAisSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int AIS::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    AISSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureAIS *msg = MsgConfigureAIS::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureAIS *msgToGUI = MsgConfigureAIS::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

void AIS::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const AISSettings& settings)
{
    if (response.getAisSettings()->getTitle()) {
        *response.getAisSettings()->getTitle() = settings.m_title;
    } else {
        response.getAisSettings()->setTitle(new QString(settings.m_title));
    }

    response.getAisSettings()->setRgbColor(settings.m_rgbColor);
    response.getAisSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getAisSettings()->getReverseApiAddress()) {
        *response.getAisSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getAisSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getAisSettings()->setReverseApiPort(settings.m_reverseAPIPort);
}

void AIS::webapiUpdateFeatureSettings(
    AISSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getAisSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getAisSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAisSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAisSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAisSettings()->getReverseApiPort();
    }
}

void AIS::webapiReverseSendSettings(QList<QString>& featureSettingsKeys, const AISSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("AIS"));
    swgFeatureSettings->setAisSettings(new SWGSDRangel::SWGAISSettings());
    SWGSDRangel::SWGAISSettings *swgAISSettings = swgFeatureSettings->getAisSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("title") || force) {
        swgAISSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgAISSettings->setRgbColor(settings.m_rgbColor);
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

void AIS::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "AIS::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("AIS::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
