///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#include "feature/featureset.h"
#include "settings/serializable.h"
#include "maincore.h"
#include "map.h"

MESSAGE_CLASS_DEFINITION(Map::MsgConfigureMap, Message)
MESSAGE_CLASS_DEFINITION(Map::MsgFind, Message)
MESSAGE_CLASS_DEFINITION(Map::MsgSetDateTime, Message)
MESSAGE_CLASS_DEFINITION(Map::MsgReportAvailableChannelOrFeatures, Message)

const char* const Map::m_featureIdURI = "sdrangel.feature.map";
const char* const Map::m_featureId = "Map";

Map::Map(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_availableChannelOrFeatureHandler(MapSettings::m_pipeURIs, QStringList{"mapitems"}),
    m_multiplier(0.0)
{
    qDebug("Map::Map: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "Map error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Map::networkManagerFinished
    );
    QObject::connect(
        &m_availableChannelOrFeatureHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &Map::channelsOrFeaturesChanged
    );
    QObject::connect(
        &m_availableChannelOrFeatureHandler,
        &AvailableChannelOrFeatureHandler::messageEnqueued,
        this,
        &Map::handlePipeMessageQueue
    );
    m_availableChannelOrFeatureHandler.scanAvailableChannelsAndFeatures();
}

Map::~Map()
{
    QObject::disconnect(
        &m_availableChannelOrFeatureHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &Map::channelsOrFeaturesChanged
    );
    QObject::disconnect(
        &m_availableChannelOrFeatureHandler,
        &AvailableChannelOrFeatureHandler::messageEnqueued,
        this,
        &Map::handlePipeMessageQueue
    );
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Map::networkManagerFinished
    );
    delete m_networkManager;
}

bool Map::handleMessage(const Message& cmd)
{
    if (MsgConfigureMap::match(cmd))
    {
        MsgConfigureMap& cfg = (MsgConfigureMap&) cmd;
        qDebug() << "Map::handleMessage: MsgConfigureMap";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MainCore::MsgMapItem::match(cmd))
    {
        // qDebug() << "Map::handleMessage: MsgMapItem";
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

QByteArray Map::serialize() const
{
    return m_settings.serialize();
}

bool Map::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureMap *msg = MsgConfigureMap::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureMap *msg = MsgConfigureMap::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void Map::applySettings(const MapSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "Map::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

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

    MsgConfigureMap *msg = MsgConfigureMap::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMap *msgToGUI = MsgConfigureMap::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int Map::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMapReport(new SWGSDRangel::SWGMapReport());
    response.getMapReport()->init();
    webapiFormatFeatureReport(response);
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

            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(MsgFind::create(id));
            }
        }
        if (featureActionsKeys.contains("setDateTime"))
        {
            QString dateTimeString = *swgMapActions->getSetDateTime();
            QDateTime dateTime = QDateTime::fromString(dateTimeString, Qt::ISODateWithMs);

            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(MsgSetDateTime::create(dateTime));
            }
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
    response.getMapSettings()->setTerrain(new QString(settings.m_terrain));

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
    response.getMapSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getMapSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getMapSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getMapSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getMapSettings()->setRollupState(swgRollupState);
        }
    }
}

void Map::webapiUpdateFeatureSettings(
    MapSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("displayNames")) {
        settings.m_displayNames = response.getMapSettings()->getDisplayNames();
    }
    if (featureSettingsKeys.contains("terrain")) {
        settings.m_terrain = *response.getMapSettings()->getTerrain();
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
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getMapSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getMapSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getMapSettings()->getRollupState());
    }
}

void Map::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const MapSettings& settings, bool force)
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
    if (featureSettingsKeys.contains("terrain") || force) {
        swgMapSettings->setTerrain(new QString(settings.m_terrain));
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

void Map::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    QString mapDateTime = getMapDateTime().toString(Qt::ISODateWithMs);
    if (response.getMapReport()->getDateTime()) {
        *response.getMapReport()->getDateTime() = mapDateTime;
    } else {
        response.getMapReport()->setDateTime(new QString(mapDateTime));
    }
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

void Map::setMapDateTime(QDateTime mapDateTime, QDateTime systemDateTime, double multiplier)
{
    QMutexLocker mutexLocker(&m_dateTimeMutex);
    m_mapDateTime = mapDateTime;
    m_systemDateTime = systemDateTime;
    m_multiplier = multiplier;
}

QDateTime Map::getMapDateTime()
{
    QMutexLocker mutexLocker(&m_dateTimeMutex);
    if (m_multiplier == 0.0)
    {
        return m_mapDateTime;
    }
    else
    {
        // It's not possible to synchronously get the time from Cesium
        // so we calculate it based on the system clock difference from
        // when changes were made to the clock GUI elements
        // Should be accurate enough for satellite tracker
        qint64 diffMsecs = m_systemDateTime.msecsTo(QDateTime::currentDateTime());
        return m_mapDateTime.addMSecs(diffMsecs * m_multiplier);
    }
}

void Map::channelsOrFeaturesChanged(const QStringList& renameFrom, const QStringList& renameTo)
{
    m_availableChannelOrFeatures = m_availableChannelOrFeatureHandler.getAvailableChannelOrFeatureList();
    notifyUpdate(renameFrom, renameTo);
}

void Map::notifyUpdate(const QStringList& renameFrom, const QStringList& renameTo)
{
    if (getMessageQueueToGUI())
    {
        MsgReportAvailableChannelOrFeatures *msg = MsgReportAvailableChannelOrFeatures::create(renameFrom, renameTo);
        msg->getItems() = m_availableChannelOrFeatures;
        getMessageQueueToGUI()->push(msg);
    }
}

void Map::handlePipeMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}
