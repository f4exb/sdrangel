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
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGDeviceState.h"

#include "dsp/dspengine.h"

#include "device/deviceset.h"
#include "channel/channelapi.h"
#include "feature/featureset.h"
#include "maincore.h"
#include "map.h"

MESSAGE_CLASS_DEFINITION(Map::MsgConfigureMap, Message)
MESSAGE_CLASS_DEFINITION(Map::MsgFind, Message)

const char* const Map::m_featureIdURI = "sdrangel.feature.map";
const char* const Map::m_featureId = "Map";

Map::Map(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface)
{
    qDebug("Map::Map: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "Map error";
    connect(&m_updatePipesTimer, SIGNAL(timeout()), this, SLOT(updatePipes()));
    m_updatePipesTimer.start(1000);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

Map::~Map()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
}

bool Map::handleMessage(const Message& cmd)
{
    if (MsgConfigureMap::match(cmd))
    {
        MsgConfigureMap& cfg = (MsgConfigureMap&) cmd;
        qDebug() << "Map::handleMessage: MsgConfigureMap";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MainCore::MsgMapItem::match(cmd))
    {
        MainCore::MsgMapItem& msgMapItem = (MainCore::MsgMapItem&) cmd;
        MainCore::MsgMapItem *copy = new MainCore::MsgMapItem(msgMapItem);
        getMessageQueueToGUI()->push(copy);
        return true;
    }
    else
    {
        return false;
    }
}

void Map::updatePipes()
{
    QList<AvailablePipeSource> availablePipes = updateAvailablePipeSources("mapitems", MapSettings::m_pipeTypes, MapSettings::m_pipeURIs, this);

    if (availablePipes != m_availablePipes)
    {
        m_availablePipes = availablePipes;
        if (getMessageQueueToGUI())
        {
            MsgReportPipes *msgToGUI = MsgReportPipes::create();
            QList<AvailablePipeSource>& msgAvailablePipes = msgToGUI->getAvailablePipes();
            msgAvailablePipes.append(availablePipes);
            getMessageQueueToGUI()->push(msgToGUI);
        }
    }
}

QByteArray Map::serialize() const
{
    return m_settings.serialize();
}

bool Map::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureMap *msg = MsgConfigureMap::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureMap *msg = MsgConfigureMap::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void Map::applySettings(const MapSettings& settings, bool force)
{
    qDebug() << "Map::applySettings:"
            << " m_displayNames: " << settings.m_displayNames
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIFeatureSetIndex: " << settings.m_reverseAPIFeatureSetIndex
            << " m_reverseAPIFeatureIndex: " << settings.m_reverseAPIFeatureIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_displayNames != settings.m_displayNames) || force) {
        reverseAPIKeys.append("displayNames");
    }
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

int Map::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) run;
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    return 202;
}

int Map::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMapSettings(new SWGSDRangel::SWGMapSettings());
    response.getMapSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int Map::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    MapSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureMap *msg = MsgConfigureMap::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMap *msgToGUI = MsgConfigureMap::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int Map::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGMapActions *swgMapActions = query.getMapActions();

    if (swgMapActions)
    {
        if (featureActionsKeys.contains("find"))
        {
            QString id = *swgMapActions->getFind();

            if (getMessageQueueToGUI())
                getMessageQueueToGUI()->push(MsgFind::create(id));
        }

        return 202;
    }
    else
    {
        errorMessage = "Missing MapActions in query";
        return 400;
    }
}

void Map::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const MapSettings& settings)
{
    response.getMapSettings()->setDisplayNames(settings.m_displayNames ? 1 : 0);

    if (response.getMapSettings()->getTitle()) {
        *response.getMapSettings()->getTitle() = settings.m_title;
    } else {
        response.getMapSettings()->setTitle(new QString(settings.m_title));
    }

    response.getMapSettings()->setRgbColor(settings.m_rgbColor);
    response.getMapSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getMapSettings()->getReverseApiAddress()) {
        *response.getMapSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getMapSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getMapSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getMapSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getMapSettings()->setReverseApiChannelIndex(settings.m_reverseAPIFeatureIndex);
}

void Map::webapiUpdateFeatureSettings(
    MapSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("displayNames")) {
        settings.m_displayNames = response.getMapSettings()->getDisplayNames();
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getMapSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getMapSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getMapSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getMapSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getMapSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getMapSettings()->getReverseApiDeviceIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getMapSettings()->getReverseApiChannelIndex();
    }
}

void Map::webapiReverseSendSettings(QList<QString>& featureSettingsKeys, const MapSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("Map"));
    swgFeatureSettings->setMapSettings(new SWGSDRangel::SWGMapSettings());
    SWGSDRangel::SWGMapSettings *swgMapSettings = swgFeatureSettings->getMapSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("displayNames") || force) {
        swgMapSettings->setDisplayNames(settings.m_displayNames);
    }
    if (featureSettingsKeys.contains("title") || force) {
        swgMapSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgMapSettings->setRgbColor(settings.m_rgbColor);
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

void Map::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "Map::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("Map::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
