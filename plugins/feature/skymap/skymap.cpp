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
#include "skymap.h"

MESSAGE_CLASS_DEFINITION(SkyMap::MsgConfigureSkyMap, Message)
MESSAGE_CLASS_DEFINITION(SkyMap::MsgFind, Message)
MESSAGE_CLASS_DEFINITION(SkyMap::MsgSetDateTime, Message)
MESSAGE_CLASS_DEFINITION(SkyMap::MsgReportViewDetails, Message)

const char* const SkyMap::m_featureIdURI = "sdrangel.feature.skymap";
const char* const SkyMap::m_featureId = "SkyMap";

SkyMap::SkyMap(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_multiplier(0.0)
{
    qDebug("SkyMap::SkyMap: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "SkyMap error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SkyMap::networkManagerFinished
    );
}

SkyMap::~SkyMap()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &SkyMap::networkManagerFinished
    );
    delete m_networkManager;
}

bool SkyMap::handleMessage(const Message& cmd)
{
    if (MsgConfigureSkyMap::match(cmd))
    {
        MsgConfigureSkyMap& cfg = (MsgConfigureSkyMap&) cmd;
        qDebug() << "SkyMap::handleMessage: MsgConfigureSkyMap";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MsgReportViewDetails::match(cmd))
    {
        MsgReportViewDetails& report = (MsgReportViewDetails&) cmd;
        m_viewDetails = report.getViewDetails();
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray SkyMap::serialize() const
{
    return m_settings.serialize();
}

bool SkyMap::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSkyMap *msg = MsgConfigureSkyMap::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSkyMap *msg = MsgConfigureSkyMap::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SkyMap::applySettings(const SkyMapSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "SkyMap::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

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

int SkyMap::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) run;
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    return 202;
}

int SkyMap::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setSkyMapSettings(new SWGSDRangel::SWGSkyMapSettings());
    response.getSkyMapSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int SkyMap::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    SkyMapSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureSkyMap *msg = MsgConfigureSkyMap::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSkyMap *msgToGUI = MsgConfigureSkyMap::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);

    return 200;
}

int SkyMap::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setSkyMapReport(new SWGSDRangel::SWGSkyMapReport());
    response.getSkyMapReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

int SkyMap::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGSkyMapActions *swgSkyMapActions = query.getSkyMapActions();

    if (swgSkyMapActions)
    {
        if (featureActionsKeys.contains("find"))
        {
            QString id = *swgSkyMapActions->getFind();

            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(MsgFind::create(id));
            }
        }
        /*if (featureActionsKeys.contains("setDateTime"))
        {
            QString dateTimeString = *swgSkyMapActions->getSetDateTime();
            QDateTime dateTime = QDateTime::fromString(dateTimeString, Qt::ISODateWithMs);

            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(MsgSetDateTime::create(dateTime));
            }
        }*/
        return 202;
    }
    else
    {
        errorMessage = "Missing SkyMapActions in query";
        return 400;
    }
    return 400;
}

void SkyMap::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const SkyMapSettings& settings)
{
    response.getSkyMapSettings()->setDisplayNames(settings.m_displayNames ? 1 : 0);
    response.getSkyMapSettings()->setDisplayConstellations(settings.m_displayConstellations ? 1 : 0);
    response.getSkyMapSettings()->setDisplayReticle(settings.m_displayReticle ? 1 : 0);
    response.getSkyMapSettings()->setDisplayGrid(settings.m_displayGrid ? 1 : 0);
    response.getSkyMapSettings()->setDisplayAntennaFoV(settings.m_displayAntennaFoV ? 1 : 0);
    response.getSkyMapSettings()->setMap(new QString(settings.m_map));
    response.getSkyMapSettings()->setBackground(new QString(settings.m_background));
    response.getSkyMapSettings()->setProjection(new QString(settings.m_projection));
    response.getSkyMapSettings()->setSource(new QString(settings.m_source));
    response.getSkyMapSettings()->setTrack(settings.m_track ? 1 : 0);
    response.getSkyMapSettings()->setLatitude(settings.m_latitude);
    response.getSkyMapSettings()->setLongitude(settings.m_longitude);
    response.getSkyMapSettings()->setAltitude(settings.m_altitude);
    response.getSkyMapSettings()->setHpbw(settings.m_hpbw);
    response.getSkyMapSettings()->setUseMyPosition(settings.m_useMyPosition);

    if (response.getSkyMapSettings()->getTitle()) {
        *response.getSkyMapSettings()->getTitle() = settings.m_title;
    } else {
        response.getSkyMapSettings()->setTitle(new QString(settings.m_title));
    }

    response.getSkyMapSettings()->setRgbColor(settings.m_rgbColor);
    response.getSkyMapSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSkyMapSettings()->getReverseApiAddress()) {
        *response.getSkyMapSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSkyMapSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSkyMapSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSkyMapSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getSkyMapSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    if (settings.m_rollupState)
    {
        if (response.getSkyMapSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getSkyMapSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getSkyMapSettings()->setRollupState(swgRollupState);
        }
    }
}

void SkyMap::webapiUpdateFeatureSettings(
    SkyMapSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("displayNames")) {
        settings.m_displayNames = response.getSkyMapSettings()->getDisplayNames();
    }
    if (featureSettingsKeys.contains("displayConstellations")) {
        settings.m_displayConstellations = response.getSkyMapSettings()->getDisplayConstellations();
    }
    if (featureSettingsKeys.contains("displayReticle")) {
        settings.m_displayReticle = response.getSkyMapSettings()->getDisplayReticle();
    }
    if (featureSettingsKeys.contains("displayGrid")) {
        settings.m_displayGrid = response.getSkyMapSettings()->getDisplayGrid();
    }
    if (featureSettingsKeys.contains("displayAntennaFoV")) {
        settings.m_displayAntennaFoV = response.getSkyMapSettings()->getDisplayAntennaFoV();
    }
    if (featureSettingsKeys.contains("map")) {
        settings.m_map = *response.getSkyMapSettings()->getMap();
    }
    if (featureSettingsKeys.contains("background")) {
        settings.m_background = *response.getSkyMapSettings()->getBackground();
    }
    if (featureSettingsKeys.contains("projection")) {
        settings.m_projection = *response.getSkyMapSettings()->getProjection();
    }
    if (featureSettingsKeys.contains("source")) {
        settings.m_source = *response.getSkyMapSettings()->getSource();
    }
    if (featureSettingsKeys.contains("track")) {
        settings.m_track = response.getSkyMapSettings()->getTrack();
    }
    if (featureSettingsKeys.contains("latitude")) {
        settings.m_latitude = response.getSkyMapSettings()->getLatitude();
    }
    if (featureSettingsKeys.contains("longitude")) {
        settings.m_longitude = response.getSkyMapSettings()->getLongitude();
    }
    if (featureSettingsKeys.contains("altitude")) {
        settings.m_altitude = response.getSkyMapSettings()->getAltitude();
    }
    if (featureSettingsKeys.contains("hpbw")) {
        settings.m_hpbw = response.getSkyMapSettings()->getHpbw();
    }
    if (featureSettingsKeys.contains("useMyPosition")) {
        settings.m_useMyPosition = response.getSkyMapSettings()->getUseMyPosition();
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getSkyMapSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getSkyMapSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSkyMapSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSkyMapSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSkyMapSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getSkyMapSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getSkyMapSettings()->getReverseApiFeatureIndex();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getSkyMapSettings()->getRollupState());
    }
}

void SkyMap::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const SkyMapSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("SkyMap"));
    swgFeatureSettings->setSkyMapSettings(new SWGSDRangel::SWGSkyMapSettings());
    SWGSDRangel::SWGSkyMapSettings *swgSkyMapSettings = swgFeatureSettings->getSkyMapSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("displayNames") || force) {
        swgSkyMapSettings->setDisplayNames(settings.m_displayNames);
    }
    if (featureSettingsKeys.contains("displayConstellations") || force) {
        swgSkyMapSettings->setDisplayConstellations(settings.m_displayConstellations);
    }
    if (featureSettingsKeys.contains("displayReticle") || force) {
        swgSkyMapSettings->setDisplayReticle(settings.m_displayReticle);
    }
    if (featureSettingsKeys.contains("displayGrid") || force) {
        swgSkyMapSettings->setDisplayGrid(settings.m_displayGrid);
    }
    if (featureSettingsKeys.contains("displayAntennaFoV") || force) {
        swgSkyMapSettings->setDisplayAntennaFoV(settings.m_displayAntennaFoV);
    }
    if (featureSettingsKeys.contains("map") || force) {
        swgSkyMapSettings->setMap(new QString(settings.m_map));
    }
    if (featureSettingsKeys.contains("background") || force) {
        swgSkyMapSettings->setBackground(new QString(settings.m_background));
    }
    if (featureSettingsKeys.contains("projection") || force) {
        swgSkyMapSettings->setProjection(new QString(settings.m_projection));
    }
    if (featureSettingsKeys.contains("source") || force) {
        swgSkyMapSettings->setSource(new QString(settings.m_source));
    }
    if (featureSettingsKeys.contains("track") || force) {
        swgSkyMapSettings->setTrack(settings.m_track);
    }
    if (featureSettingsKeys.contains("latitude") || force) {
        swgSkyMapSettings->setLatitude(settings.m_latitude);
    }
    if (featureSettingsKeys.contains("longitude") || force) {
        swgSkyMapSettings->setLongitude(settings.m_longitude);
    }
    if (featureSettingsKeys.contains("altitude") || force) {
        swgSkyMapSettings->setAltitude(settings.m_altitude);
    }
    if (featureSettingsKeys.contains("hpbw") || force) {
        swgSkyMapSettings->setHpbw(settings.m_hpbw);
    }
    if (featureSettingsKeys.contains("useMyPosition") || force) {
        swgSkyMapSettings->setTrack(settings.m_useMyPosition);
    }
    if (featureSettingsKeys.contains("title") || force) {
        swgSkyMapSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgSkyMapSettings->setRgbColor(settings.m_rgbColor);
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

void SkyMap::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    QString skymapDateTime = getSkyMapDateTime().toString(Qt::ISODateWithMs);
    if (response.getSkyMapReport()->getDateTime()) {
        //*response.getSkyMapReport()->getDateTime() = skymapDateTime;
        *response.getSkyMapReport()->getDateTime() = m_viewDetails.m_dateTime.toString(Qt::ISODateWithMs);
    } else {
        //response.getSkyMapReport()->setDateTime(new QString(skymapDateTime));
        response.getSkyMapReport()->setDateTime(new QString(m_viewDetails.m_dateTime.toString(Qt::ISODateWithMs)));
    }
    response.getSkyMapReport()->setRa(m_viewDetails.m_ra);
    response.getSkyMapReport()->setDec(m_viewDetails.m_dec);
    response.getSkyMapReport()->setAzimuth(m_viewDetails.m_azimuth);
    response.getSkyMapReport()->setElevation(m_viewDetails.m_elevation);
    response.getSkyMapReport()->setFov(m_viewDetails.m_fov);
    response.getSkyMapReport()->setLatitude(m_viewDetails.m_latitude);
    response.getSkyMapReport()->setLongitude(m_viewDetails.m_longitude);
}

void SkyMap::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SkyMap::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SkyMap::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void SkyMap::setSkyMapDateTime(QDateTime skymapDateTime, QDateTime systemDateTime, double multiplier)
{
    QMutexLocker mutexLocker(&m_dateTimeMutex);
    m_skymapDateTime = skymapDateTime;
    m_systemDateTime = systemDateTime;
    m_multiplier = multiplier;
}

QDateTime SkyMap::getSkyMapDateTime()
{
    QMutexLocker mutexLocker(&m_dateTimeMutex);
    if (m_multiplier == 0.0)
    {
        return m_skymapDateTime;
    }
    else
    {
        // It's not possible to synchronously get the time from Cesium
        // so we calculate it based on the system clock difference from
        // when changes were made to the clock GUI elements
        // Should be accurate enough for satellite tracker
        qint64 diffMsecs = m_systemDateTime.msecsTo(QDateTime::currentDateTime());
        return m_skymapDateTime.addMSecs(diffMsecs * m_multiplier);
    }
}
