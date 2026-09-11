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
    setState(StIdle);
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
                settingsKeys.contains("reverseAPIFeatureIndex");
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
    response.getMapSettings()->setMap2DEnabled(settings.m_map2DEnabled ? 1 : 0);
    response.getMapSettings()->setMap3DEnabled(settings.m_map3DEnabled ? 1 : 0);
    if (response.getMapSettings()->getMapProvider()) {
        *response.getMapSettings()->getMapProvider() = settings.m_mapProvider;
    } else {
        response.getMapSettings()->setMapProvider(new QString(settings.m_mapProvider));
    }
    if (response.getMapSettings()->getMapType()) {
        *response.getMapSettings()->getMapType() = settings.m_mapType;
    } else {
        response.getMapSettings()->setMapType(new QString(settings.m_mapType));
    }
    if (response.getMapSettings()->getOsmUrl()) {
        *response.getMapSettings()->getOsmUrl() = settings.m_osmURL;
    } else {
        response.getMapSettings()->setOsmUrl(new QString(settings.m_osmURL));
    }
    if (response.getMapSettings()->getMapBoxStyles()) {
        *response.getMapSettings()->getMapBoxStyles() = settings.m_mapBoxStyles;
    } else {
        response.getMapSettings()->setMapBoxStyles(new QString(settings.m_mapBoxStyles));
    }
    response.getMapSettings()->setDisplaySelectedGroundTracks(settings.m_displaySelectedGroundTracks ? 1 : 0);
    response.getMapSettings()->setDisplayAllGroundTracks(settings.m_displayAllGroundTracks ? 1 : 0);
    if (response.getMapSettings()->getBuildings()) {
        *response.getMapSettings()->getBuildings() = settings.m_buildings;
    } else {
        response.getMapSettings()->setBuildings(new QString(settings.m_buildings));
    }
    response.getMapSettings()->setSunLightEnabled(settings.m_sunLightEnabled ? 1 : 0);
    response.getMapSettings()->setLightIntensity(settings.m_lightIntensity);
    response.getMapSettings()->setEciCamera(settings.m_eciCamera ? 1 : 0);
    response.getMapSettings()->setFxaa(settings.m_fxaa ? 1 : 0);
    response.getMapSettings()->setMsaa(settings.m_msaa);
    response.getMapSettings()->setTerrainLighting(settings.m_terrainLighting ? 1 : 0);
    response.getMapSettings()->setWater(settings.m_water ? 1 : 0);
    response.getMapSettings()->setHdr(settings.m_hdr ? 1 : 0);
    response.getMapSettings()->setFog(settings.m_fog ? 1 : 0);
    response.getMapSettings()->setFps(settings.m_fps ? 1 : 0);
    response.getMapSettings()->setDisplayMuf(settings.m_displayMUF ? 1 : 0);
    response.getMapSettings()->setDisplayfoF2(settings.m_displayfoF2 ? 1 : 0);
    response.getMapSettings()->setDisplayRain(settings.m_displayRain ? 1 : 0);
    response.getMapSettings()->setDisplayClouds(settings.m_displayClouds ? 1 : 0);
    response.getMapSettings()->setDisplaySeaMarks(settings.m_displaySeaMarks ? 1 : 0);
    response.getMapSettings()->setDisplayRailways(settings.m_displayRailways ? 1 : 0);
    response.getMapSettings()->setDisplayNasaGlobalImagery(settings.m_displayNASAGlobalImagery ? 1 : 0);
    if (response.getMapSettings()->getNasaGlobalImageryIdentifier()) {
        *response.getMapSettings()->getNasaGlobalImageryIdentifier() = settings.m_nasaGlobalImageryIdentifier;
    } else {
        response.getMapSettings()->setNasaGlobalImageryIdentifier(new QString(settings.m_nasaGlobalImageryIdentifier));
    }
    response.getMapSettings()->setNasaGlobalImageryOpacity(settings.m_nasaGlobalImageryOpacity);
    response.getMapSettings()->setDisplayAurora(settings.m_displayAurora ? 1 : 0);
    response.getMapSettings()->setDisplayMagDec(settings.m_displayMagDec ? 1 : 0);
    response.getMapSettings()->setDisplayMaidenheadGrid(settings.m_displayMaidenheadGrid ? 1 : 0);
    response.getMapSettings()->setDisplayPfd(settings.m_displayPFD ? 1 : 0);
    response.getMapSettings()->setViewFirstPerson(settings.m_viewFirstPerson ? 1 : 0);
    if (response.getMapSettings()->getDefaultImagery()) {
        *response.getMapSettings()->getDefaultImagery() = settings.m_defaultImagery;
    } else {
        response.getMapSettings()->setDefaultImagery(new QString(settings.m_defaultImagery));
    }
    if (response.getMapSettings()->getTerrain()) {
        *response.getMapSettings()->getTerrain() = settings.m_terrain;
    } else {
        response.getMapSettings()->setTerrain(new QString(settings.m_terrain));
    }

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
    if (featureSettingsKeys.contains("map2DEnabled")) {
        settings.m_map2DEnabled = response.getMapSettings()->getMap2DEnabled() != 0;
    }
    if (featureSettingsKeys.contains("map3DEnabled")) {
        settings.m_map3DEnabled = response.getMapSettings()->getMap3DEnabled() != 0;
    }
    if (featureSettingsKeys.contains("mapProvider")) {
        settings.m_mapProvider = *response.getMapSettings()->getMapProvider();
    }
    if (featureSettingsKeys.contains("mapType")) {
        settings.m_mapType = *response.getMapSettings()->getMapType();
    }
    if (featureSettingsKeys.contains("osmURL")) {
        settings.m_osmURL = *response.getMapSettings()->getOsmUrl();
    }
    if (featureSettingsKeys.contains("mapBoxStyles")) {
        settings.m_mapBoxStyles = *response.getMapSettings()->getMapBoxStyles();
    }
    if (featureSettingsKeys.contains("displaySelectedGroundTracks")) {
        settings.m_displaySelectedGroundTracks = response.getMapSettings()->getDisplaySelectedGroundTracks() != 0;
    }
    if (featureSettingsKeys.contains("displayAllGroundTracks")) {
        settings.m_displayAllGroundTracks = response.getMapSettings()->getDisplayAllGroundTracks() != 0;
    }
    if (featureSettingsKeys.contains("buildings")) {
        settings.m_buildings = *response.getMapSettings()->getBuildings();
    }
    if (featureSettingsKeys.contains("sunLightEnabled")) {
        settings.m_sunLightEnabled = response.getMapSettings()->getSunLightEnabled() != 0;
    }
    if (featureSettingsKeys.contains("lightIntensity")) {
        settings.m_lightIntensity = response.getMapSettings()->getLightIntensity();
    }
    if (featureSettingsKeys.contains("eciCamera")) {
        settings.m_eciCamera = response.getMapSettings()->getEciCamera() != 0;
    }
    if (featureSettingsKeys.contains("fxaa")) {
        settings.m_fxaa = response.getMapSettings()->getFxaa() != 0;
    }
    if (featureSettingsKeys.contains("msaa")) {
        settings.m_msaa = response.getMapSettings()->getMsaa();
    }
    if (featureSettingsKeys.contains("terrainLighting")) {
        settings.m_terrainLighting = response.getMapSettings()->getTerrainLighting() != 0;
    }
    if (featureSettingsKeys.contains("water")) {
        settings.m_water = response.getMapSettings()->getWater() != 0;
    }
    if (featureSettingsKeys.contains("hdr")) {
        settings.m_hdr = response.getMapSettings()->getHdr() != 0;
    }
    if (featureSettingsKeys.contains("fog")) {
        settings.m_fog = response.getMapSettings()->getFog() != 0;
    }
    if (featureSettingsKeys.contains("fps")) {
        settings.m_fps = response.getMapSettings()->getFps() != 0;
    }
    if (featureSettingsKeys.contains("displayMUF")) {
        settings.m_displayMUF = response.getMapSettings()->getDisplayMuf() != 0;
    }
    if (featureSettingsKeys.contains("displayfoF2")) {
        settings.m_displayfoF2 = response.getMapSettings()->getDisplayfoF2() != 0;
    }
    if (featureSettingsKeys.contains("displayRain")) {
        settings.m_displayRain = response.getMapSettings()->getDisplayRain() != 0;
    }
    if (featureSettingsKeys.contains("displayClouds")) {
        settings.m_displayClouds = response.getMapSettings()->getDisplayClouds() != 0;
    }
    if (featureSettingsKeys.contains("displaySeaMarks")) {
        settings.m_displaySeaMarks = response.getMapSettings()->getDisplaySeaMarks() != 0;
    }
    if (featureSettingsKeys.contains("displayRailways")) {
        settings.m_displayRailways = response.getMapSettings()->getDisplayRailways() != 0;
    }
    if (featureSettingsKeys.contains("displayNASAGlobalImagery")) {
        settings.m_displayNASAGlobalImagery = response.getMapSettings()->getDisplayNasaGlobalImagery() != 0;
    }
    if (featureSettingsKeys.contains("nasaGlobalImageryIdentifier")) {
        settings.m_nasaGlobalImageryIdentifier = *response.getMapSettings()->getNasaGlobalImageryIdentifier();
    }
    if (featureSettingsKeys.contains("nasaGlobalImageryOpacity")) {
        settings.m_nasaGlobalImageryOpacity = response.getMapSettings()->getNasaGlobalImageryOpacity();
    }
    if (featureSettingsKeys.contains("displayAurora")) {
        settings.m_displayAurora = response.getMapSettings()->getDisplayAurora() != 0;
    }
    if (featureSettingsKeys.contains("displayMagDec")) {
        settings.m_displayMagDec = response.getMapSettings()->getDisplayMagDec() != 0;
    }
    if (featureSettingsKeys.contains("displayMaidenheadGrid")) {
        settings.m_displayMaidenheadGrid = response.getMapSettings()->getDisplayMaidenheadGrid() != 0;
    }
    if (featureSettingsKeys.contains("displayPFD")) {
        settings.m_displayPFD = response.getMapSettings()->getDisplayPfd() != 0;
    }
    if (featureSettingsKeys.contains("viewFirstPerson")) {
        settings.m_viewFirstPerson = response.getMapSettings()->getViewFirstPerson() != 0;
    }
    if (featureSettingsKeys.contains("defaultImagery")) {
        settings.m_defaultImagery = *response.getMapSettings()->getDefaultImagery();
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
    if (featureSettingsKeys.contains("map2DEnabled") || force) {
        swgMapSettings->setMap2DEnabled(settings.m_map2DEnabled);
    }
    if (featureSettingsKeys.contains("map3DEnabled") || force) {
        swgMapSettings->setMap3DEnabled(settings.m_map3DEnabled);
    }
    if (featureSettingsKeys.contains("mapProvider") || force) {
        swgMapSettings->setMapProvider(new QString(settings.m_mapProvider));
    }
    if (featureSettingsKeys.contains("mapType") || force) {
        swgMapSettings->setMapType(new QString(settings.m_mapType));
    }
    if (featureSettingsKeys.contains("osmURL") || force) {
        swgMapSettings->setOsmUrl(new QString(settings.m_osmURL));
    }
    if (featureSettingsKeys.contains("mapBoxStyles") || force) {
        swgMapSettings->setMapBoxStyles(new QString(settings.m_mapBoxStyles));
    }
    if (featureSettingsKeys.contains("displaySelectedGroundTracks") || force) {
        swgMapSettings->setDisplaySelectedGroundTracks(settings.m_displaySelectedGroundTracks);
    }
    if (featureSettingsKeys.contains("displayAllGroundTracks") || force) {
        swgMapSettings->setDisplayAllGroundTracks(settings.m_displayAllGroundTracks);
    }
    if (featureSettingsKeys.contains("buildings") || force) {
        swgMapSettings->setBuildings(new QString(settings.m_buildings));
    }
    if (featureSettingsKeys.contains("sunLightEnabled") || force) {
        swgMapSettings->setSunLightEnabled(settings.m_sunLightEnabled);
    }
    if (featureSettingsKeys.contains("lightIntensity") || force) {
        swgMapSettings->setLightIntensity(settings.m_lightIntensity);
    }
    if (featureSettingsKeys.contains("eciCamera") || force) {
        swgMapSettings->setEciCamera(settings.m_eciCamera);
    }
    if (featureSettingsKeys.contains("fxaa") || force) {
        swgMapSettings->setFxaa(settings.m_fxaa);
    }
    if (featureSettingsKeys.contains("msaa") || force) {
        swgMapSettings->setMsaa(settings.m_msaa);
    }
    if (featureSettingsKeys.contains("terrainLighting") || force) {
        swgMapSettings->setTerrainLighting(settings.m_terrainLighting);
    }
    if (featureSettingsKeys.contains("water") || force) {
        swgMapSettings->setWater(settings.m_water);
    }
    if (featureSettingsKeys.contains("hdr") || force) {
        swgMapSettings->setHdr(settings.m_hdr);
    }
    if (featureSettingsKeys.contains("fog") || force) {
        swgMapSettings->setFog(settings.m_fog);
    }
    if (featureSettingsKeys.contains("fps") || force) {
        swgMapSettings->setFps(settings.m_fps);
    }
    if (featureSettingsKeys.contains("displayMUF") || force) {
        swgMapSettings->setDisplayMuf(settings.m_displayMUF);
    }
    if (featureSettingsKeys.contains("displayfoF2") || force) {
        swgMapSettings->setDisplayfoF2(settings.m_displayfoF2);
    }
    if (featureSettingsKeys.contains("displayRain") || force) {
        swgMapSettings->setDisplayRain(settings.m_displayRain);
    }
    if (featureSettingsKeys.contains("displayClouds") || force) {
        swgMapSettings->setDisplayClouds(settings.m_displayClouds);
    }
    if (featureSettingsKeys.contains("displaySeaMarks") || force) {
        swgMapSettings->setDisplaySeaMarks(settings.m_displaySeaMarks);
    }
    if (featureSettingsKeys.contains("displayRailways") || force) {
        swgMapSettings->setDisplayRailways(settings.m_displayRailways);
    }
    if (featureSettingsKeys.contains("displayNASAGlobalImagery") || force) {
        swgMapSettings->setDisplayNasaGlobalImagery(settings.m_displayNASAGlobalImagery);
    }
    if (featureSettingsKeys.contains("nasaGlobalImageryIdentifier") || force) {
        swgMapSettings->setNasaGlobalImageryIdentifier(new QString(settings.m_nasaGlobalImageryIdentifier));
    }
    if (featureSettingsKeys.contains("nasaGlobalImageryOpacity") || force) {
        swgMapSettings->setNasaGlobalImageryOpacity(settings.m_nasaGlobalImageryOpacity);
    }
    if (featureSettingsKeys.contains("displayAurora") || force) {
        swgMapSettings->setDisplayAurora(settings.m_displayAurora);
    }
    if (featureSettingsKeys.contains("displayMagDec") || force) {
        swgMapSettings->setDisplayMagDec(settings.m_displayMagDec);
    }
    if (featureSettingsKeys.contains("displayMaidenheadGrid") || force) {
        swgMapSettings->setDisplayMaidenheadGrid(settings.m_displayMaidenheadGrid);
    }
    if (featureSettingsKeys.contains("displayPFD") || force) {
        swgMapSettings->setDisplayPfd(settings.m_displayPFD);
    }
    if (featureSettingsKeys.contains("viewFirstPerson") || force) {
        swgMapSettings->setViewFirstPerson(settings.m_viewFirstPerson);
    }
    if (featureSettingsKeys.contains("defaultImagery") || force) {
        swgMapSettings->setDefaultImagery(new QString(settings.m_defaultImagery));
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
