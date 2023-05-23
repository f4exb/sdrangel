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
#include "SWGStarTrackerDisplaySettings.h"

#include "device/deviceset.h"
#include "dsp/dspengine.h"
#include "feature/featureset.h"
#include "util/weather.h"
#include "util/units.h"
#include "settings/serializable.h"
#include "maincore.h"

#include "startrackerreport.h"
#include "startrackerworker.h"
#include "startracker.h"

MESSAGE_CLASS_DEFINITION(StarTracker::MsgConfigureStarTracker, Message)
MESSAGE_CLASS_DEFINITION(StarTracker::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(StarTracker::MsgSetSolarFlux, Message)
MESSAGE_CLASS_DEFINITION(StarTracker::MsgReportAvailableSatelliteTrackers, Message)

const char* const StarTracker::m_featureIdURI = "sdrangel.feature.startracker";
const char* const StarTracker::m_featureId = "StarTracker";

StarTracker::StarTracker(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_thread(nullptr),
    m_worker(nullptr)
{
    qDebug("StarTracker::StarTracker: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "StarTracker error";
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &StarTracker::networkManagerFinished
    );
    m_weather = nullptr;
    m_solarFlux = 0.0f;
    // Unfortunately, can't seem to access resources in static global constructor
    m_temps.append(new FITS(":/startracker/startracker/150mhz_ra_dec.fits"));
    m_temps.append(new FITS(":/startracker/startracker/408mhz_ra_dec.fits"));
    m_temps.append(new FITS(":/startracker/startracker/1420mhz_ra_dec.fits"));
    m_spectralIndex = new FITS(":/startracker/startracker/408mhz_ra_dec_spectral_index.fits");
    scanAvailableChannels();
    scanAvailableFeatures();
    QObject::connect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &StarTracker::handleChannelAdded
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::featureAdded,
        this,
        &StarTracker::handleFeatureAdded
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::featureRemoved,
        this,
        &StarTracker::handleFeatureRemoved
    );
}

StarTracker::~StarTracker()
{
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &StarTracker::handleChannelAdded
    );
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &StarTracker::networkManagerFinished
    );
    delete m_networkManager;
    stop();

    if (m_weather)
    {
        disconnect(m_weather, &Weather::weatherUpdated, this, &StarTracker::weatherUpdated);
        delete m_weather;
    }
    qDeleteAll(m_temps);
    delete m_spectralIndex;
}

void StarTracker::start()
{
    qDebug("StarTracker::start");

    m_thread = new QThread();
    m_worker = new StarTrackerWorker(this, m_webAPIAdapterInterface);
    m_worker->moveToThread(m_thread);

    QObject::connect(m_thread, &QThread::started, m_worker, &StarTrackerWorker::startWork);
    QObject::connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    QObject::connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);

    m_worker->setMessageQueueToFeature(getInputMessageQueue());
    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    m_thread->start();
    m_thread->start();
    m_state = StRunning;

    m_worker->getInputMessageQueue()->push(StarTrackerWorker::MsgConfigureStarTrackerWorker::create(m_settings, QList<QString>(), true));
    m_worker->getInputMessageQueue()->push(MsgSetSolarFlux::create(m_solarFlux));
}

void StarTracker::stop()
{
    qDebug("StarTracker::stop");
    m_state = StIdle;
    if (m_thread)
    {
        m_thread->quit();
        m_thread->wait();
        m_thread = nullptr;
        m_worker = nullptr;
    }
}

bool StarTracker::handleMessage(const Message& cmd)
{
    if (MsgConfigureStarTracker::match(cmd))
    {
        MsgConfigureStarTracker& cfg = (MsgConfigureStarTracker&) cmd;
        qDebug() << "StarTracker::handleMessage: MsgConfigureStarTracker";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MsgStartStop::match(cmd))
    {
        MsgStartStop& cfg = (MsgStartStop&) cmd;
        qDebug() << "StarTracker::handleMessage: MsgStartStop: start:" << cfg.getStartStop();

        if (cfg.getStartStop()) {
            start();
        } else {
            stop();
        }

        return true;
    }
    else if (MsgSetSolarFlux::match(cmd))
    {
        MsgSetSolarFlux& msg = (MsgSetSolarFlux&) cmd;
        m_solarFlux = msg.getFlux();
        if (m_worker) {
            m_worker->getInputMessageQueue()->push(new MsgSetSolarFlux(msg));
        }
        return true;
    }
    else if (MainCore::MsgStarTrackerDisplaySettings::match(cmd))
    {
        MainCore::MsgStarTrackerDisplaySettings& settings = (MainCore::MsgStarTrackerDisplaySettings&) cmd;
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new MainCore::MsgStarTrackerDisplaySettings(settings));
        }
        return true;
    }
    else if (MainCore::MsgStarTrackerDisplayLoSSettings::match(cmd))
    {
        MainCore::MsgStarTrackerDisplayLoSSettings& settings = (MainCore::MsgStarTrackerDisplayLoSSettings&) cmd;
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new MainCore::MsgStarTrackerDisplayLoSSettings(settings));
        }
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray StarTracker::serialize() const
{
    return m_settings.serialize();
}

bool StarTracker::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureStarTracker *msg = MsgConfigureStarTracker::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureStarTracker *msg = MsgConfigureStarTracker::create(m_settings, QList<QString>(), true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void StarTracker::applySettings(const StarTrackerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "StarTracker::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (settingsKeys.contains("owmAPIKey") || force)
    {
        if (m_weather)
        {
            disconnect(m_weather, &Weather::weatherUpdated, this, &StarTracker::weatherUpdated);
            delete m_weather;
            m_weather = nullptr;
        }
        if (!settings.m_owmAPIKey.isEmpty())
        {
            m_weather = Weather::create(settings.m_owmAPIKey);
            if (m_weather) {
                connect(m_weather, &Weather::weatherUpdated, this, &StarTracker::weatherUpdated);
            }
        }
    }

    if (settingsKeys.contains("owmAPIKey")
        || settingsKeys.contains("latitude")
        || settingsKeys.contains("longitude")
        || settingsKeys.contains("weatherUpdatePeriod")
        || force)
    {
        if (m_weather) {
            m_weather->getWeatherPeriodically(m_settings.m_latitude, m_settings.m_longitude, settings.m_weatherUpdatePeriod);
        }
    }

    StarTrackerWorker::MsgConfigureStarTrackerWorker *msg = StarTrackerWorker::MsgConfigureStarTrackerWorker::create(
        settings, settingsKeys, force
    );
    if (m_worker) {
        m_worker->getInputMessageQueue()->push(msg);
    }

    if (settingsKeys.contains("useReverseAPI"))
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
                settingsKeys.contains("m_reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

int StarTracker::webapiRun(bool run,
    SWGSDRangel::SWGDeviceState& response,
    QString& errorMessage)
{
    (void) errorMessage;
    getFeatureStateStr(*response.getState());
    MsgStartStop *msg = MsgStartStop::create(run);
    getInputMessageQueue()->push(msg);
    return 202;
}

int StarTracker::webapiSettingsGet(
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setStarTrackerSettings(new SWGSDRangel::SWGStarTrackerSettings());
    response.getStarTrackerSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int StarTracker::webapiSettingsPutPatch(
    bool force,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    StarTrackerSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureStarTracker *msg = MsgConfigureStarTracker::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    qDebug("StarTracker::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureStarTracker *msgToGUI = MsgConfigureStarTracker::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);
    return 200;
}

int StarTracker::webapiReportGet(
    SWGSDRangel::SWGFeatureReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setStarTrackerReport(new SWGSDRangel::SWGStarTrackerReport());
    response.getStarTrackerReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

void StarTracker::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    response.getStarTrackerReport()->setRunningState(getState());
}

int StarTracker::webapiActionsPost(
    const QStringList& featureActionsKeys,
    SWGSDRangel::SWGFeatureActions& query,
    QString& errorMessage)
{
    SWGSDRangel::SWGStarTrackerActions *swgSWGStarTrackerActions = query.getStarTrackerActions();

    if (swgSWGStarTrackerActions)
    {
        if (featureActionsKeys.contains("run"))
        {
            bool featureRun = swgSWGStarTrackerActions->getRun() != 0;
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
        errorMessage = "Missing StarTrackerActions in query";
        return 400;
    }
}

void StarTracker::webapiFormatFeatureSettings(
    SWGSDRangel::SWGFeatureSettings& response,
    const StarTrackerSettings& settings)
{
    response.getStarTrackerSettings()->setTarget(new QString(settings.m_target));
    response.getStarTrackerSettings()->setRa(new QString(settings.m_ra));
    response.getStarTrackerSettings()->setDec(new QString(settings.m_dec));
    response.getStarTrackerSettings()->setLatitude(settings.m_latitude);
    response.getStarTrackerSettings()->setLongitude(settings.m_longitude);
    response.getStarTrackerSettings()->setDateTime(new QString(settings.m_dateTime));
    response.getStarTrackerSettings()->setRefraction(new QString(settings.m_refraction));
    response.getStarTrackerSettings()->setPressure(settings.m_pressure);
    response.getStarTrackerSettings()->setTemperature(settings.m_temperature);
    response.getStarTrackerSettings()->setHumidity(settings.m_humidity);
    response.getStarTrackerSettings()->setHeightAboveSeaLevel(settings.m_heightAboveSeaLevel);
    response.getStarTrackerSettings()->setTemperatureLapseRate(settings.m_temperatureLapseRate);
    response.getStarTrackerSettings()->setFrequency(settings.m_frequency/1000000.0);
    response.getStarTrackerSettings()->setStellariumServerEnabled(settings.m_enableServer ? 1 : 0);
    response.getStarTrackerSettings()->setStellariumPort(settings.m_serverPort);
    response.getStarTrackerSettings()->setUpdatePeriod(settings.m_updatePeriod);
    response.getStarTrackerSettings()->setEpoch(settings.m_jnow ? new QString("JNOW") : new QString("J2000"));

    if (response.getStarTrackerSettings()->getTitle()) {
        *response.getStarTrackerSettings()->getTitle() = settings.m_title;
    } else {
        response.getStarTrackerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getStarTrackerSettings()->setRgbColor(settings.m_rgbColor);
    response.getStarTrackerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getStarTrackerSettings()->getReverseApiAddress()) {
        *response.getStarTrackerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getStarTrackerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getStarTrackerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getStarTrackerSettings()->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    response.getStarTrackerSettings()->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);

    response.getStarTrackerSettings()->setAzimuth(settings.m_az);
    response.getStarTrackerSettings()->setElevation(settings.m_el);
    response.getStarTrackerSettings()->setL(settings.m_l);
    response.getStarTrackerSettings()->setB(settings.m_b);
    response.getStarTrackerSettings()->setAzimuthOffset(settings.m_azimuthOffset);
    response.getStarTrackerSettings()->setElevationOffset(settings.m_elevationOffset);

    if (settings.m_rollupState)
    {
        if (response.getStarTrackerSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getStarTrackerSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getStarTrackerSettings()->setRollupState(swgRollupState);
        }
    }
}

void StarTracker::webapiUpdateFeatureSettings(
    StarTrackerSettings& settings,
    const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    if (featureSettingsKeys.contains("target")) {
        settings.m_target = *response.getStarTrackerSettings()->getTarget();
    }
    if (featureSettingsKeys.contains("ra")) {
        settings.m_ra = *response.getStarTrackerSettings()->getRa();
    }
    if (featureSettingsKeys.contains("dec")) {
        settings.m_dec = *response.getStarTrackerSettings()->getDec();
    }
    if (featureSettingsKeys.contains("latitude")) {
        settings.m_latitude = response.getStarTrackerSettings()->getLatitude();
    }
    if (featureSettingsKeys.contains("longitude")) {
        settings.m_longitude = response.getStarTrackerSettings()->getLongitude();
    }
    if (featureSettingsKeys.contains("dateTime")) {
        settings.m_dateTime = *response.getStarTrackerSettings()->getDateTime();
    }
    if (featureSettingsKeys.contains("pressure")) {
        settings.m_pressure = response.getStarTrackerSettings()->getPressure();
    }
    if (featureSettingsKeys.contains("temperature")) {
        settings.m_temperature = response.getStarTrackerSettings()->getTemperature();
    }
    if (featureSettingsKeys.contains("humidity")) {
        settings.m_humidity = response.getStarTrackerSettings()->getHumidity();
    }
    if (featureSettingsKeys.contains("heightAboveSeaLevel")) {
        settings.m_heightAboveSeaLevel = response.getStarTrackerSettings()->getHeightAboveSeaLevel();
    }
    if (featureSettingsKeys.contains("temperatureLapseRate")) {
        settings.m_temperatureLapseRate = response.getStarTrackerSettings()->getTemperatureLapseRate();
    }
    if (featureSettingsKeys.contains("frequency")) {
        settings.m_frequency = response.getStarTrackerSettings()->getFrequency() * 100000.0;
    }
    if (featureSettingsKeys.contains("stellariumServerEnabled")) {
        settings.m_enableServer = response.getStarTrackerSettings()->getStellariumServerEnabled() == 1;
    }
    if (featureSettingsKeys.contains("stellariumPort")) {
        settings.m_serverPort = response.getStarTrackerSettings()->getStellariumPort();
    }
    if (featureSettingsKeys.contains("updatePeriod")) {
        settings.m_updatePeriod = response.getStarTrackerSettings()->getUpdatePeriod();
    }
    if (featureSettingsKeys.contains("epoch")) {
        settings.m_jnow = *response.getStarTrackerSettings()->getEpoch() == "JNOW";
    }
    if (featureSettingsKeys.contains("title")) {
        settings.m_title = *response.getStarTrackerSettings()->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getStarTrackerSettings()->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getStarTrackerSettings()->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getStarTrackerSettings()->getReverseApiAddress();
    }
    if (featureSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getStarTrackerSettings()->getReverseApiPort();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureSetIndex")) {
        settings.m_reverseAPIFeatureSetIndex = response.getStarTrackerSettings()->getReverseApiFeatureSetIndex();
    }
    if (featureSettingsKeys.contains("reverseAPIFeatureIndex")) {
        settings.m_reverseAPIFeatureIndex = response.getStarTrackerSettings()->getReverseApiFeatureIndex();
    }
    if (featureSettingsKeys.contains("azimuth")) {
        settings.m_az = response.getStarTrackerSettings()->getAzimuth();
    }
    if (featureSettingsKeys.contains("elevation")) {
        settings.m_el = response.getStarTrackerSettings()->getElevation();
    }
    if (featureSettingsKeys.contains("l")) {
        settings.m_l = response.getStarTrackerSettings()->getL();
    }
    if (featureSettingsKeys.contains("b")) {
        settings.m_b = response.getStarTrackerSettings()->getB();
    }
    if (featureSettingsKeys.contains("azimuthOffset")) {
        settings.m_azimuthOffset = response.getStarTrackerSettings()->getAzimuthOffset();
    }
    if (featureSettingsKeys.contains("elevationOffset")) {
        settings.m_elevationOffset = response.getStarTrackerSettings()->getElevationOffset();
    }
    if (settings.m_rollupState && featureSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(featureSettingsKeys, response.getStarTrackerSettings()->getRollupState());
    }
}

void StarTracker::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const StarTrackerSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    // swgFeatureSettings->setOriginatorFeatureIndex(getIndexInDeviceSet());
    // swgFeatureSettings->setOriginatorFeatureSetIndex(getDeviceSetIndex());
    swgFeatureSettings->setFeatureType(new QString("StarTracker"));
    swgFeatureSettings->setStarTrackerSettings(new SWGSDRangel::SWGStarTrackerSettings());
    SWGSDRangel::SWGStarTrackerSettings *swgStarTrackerSettings = swgFeatureSettings->getStarTrackerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (featureSettingsKeys.contains("target") || force) {
        swgStarTrackerSettings->setTarget(new QString(settings.m_target));
    }
    if (featureSettingsKeys.contains("ra") || force) {
        swgStarTrackerSettings->setRa(new QString(settings.m_ra));
    }
    if (featureSettingsKeys.contains("dec") || force) {
        swgStarTrackerSettings->setDec(new QString(settings.m_dec));
    }
    if (featureSettingsKeys.contains("latitude") || force) {
        swgStarTrackerSettings->setLatitude(settings.m_latitude);
    }
    if (featureSettingsKeys.contains("longitude") || force) {
        swgStarTrackerSettings->setLongitude(settings.m_longitude);
    }
    if (featureSettingsKeys.contains("dateTime") || force) {
        swgStarTrackerSettings->setDateTime(new QString(settings.m_dateTime));
    }
    if (featureSettingsKeys.contains("pressure") || force) {
        swgStarTrackerSettings->setPressure(settings.m_pressure);
    }
    if (featureSettingsKeys.contains("temperature") || force) {
        swgStarTrackerSettings->setTemperature(settings.m_temperature);
    }
    if (featureSettingsKeys.contains("humidity") || force) {
        swgStarTrackerSettings->setHumidity(settings.m_humidity);
    }
    if (featureSettingsKeys.contains("heightAboveSeaLevel") || force) {
        swgStarTrackerSettings->setHeightAboveSeaLevel(settings.m_heightAboveSeaLevel);
    }
    if (featureSettingsKeys.contains("temperatureLapseRate") || force) {
        swgStarTrackerSettings->setTemperatureLapseRate(settings.m_temperatureLapseRate);
    }
    if (featureSettingsKeys.contains("frequency") || force) {
        swgStarTrackerSettings->setFrequency(settings.m_frequency / 1000000.0);
    }
    if (featureSettingsKeys.contains("stellariumServerEnabled") || force) {
        swgStarTrackerSettings->setStellariumServerEnabled(settings.m_enableServer ? 1 : 0);
    }
    if (featureSettingsKeys.contains("stellariumPort") || force) {
        swgStarTrackerSettings->setStellariumPort(settings.m_serverPort);
    }
    if (featureSettingsKeys.contains("updatePeriod") || force) {
        swgStarTrackerSettings->setUpdatePeriod(settings.m_updatePeriod);
    }
    if (featureSettingsKeys.contains("epoch") || force) {
        swgStarTrackerSettings->setEpoch(settings.m_jnow ? new QString("JNOW") : new QString("J2000"));
    }
    if (featureSettingsKeys.contains("title") || force) {
        swgStarTrackerSettings->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swgStarTrackerSettings->setRgbColor(settings.m_rgbColor);
    }
    if (featureSettingsKeys.contains("azimuth") || force) {
        swgStarTrackerSettings->setAzimuth(settings.m_az);
    }
    if (featureSettingsKeys.contains("elevation") || force) {
        swgStarTrackerSettings->setElevation(settings.m_el);
    }
    if (featureSettingsKeys.contains("l") || force) {
        swgStarTrackerSettings->setL(settings.m_l);
    }
    if (featureSettingsKeys.contains("b") || force) {
        swgStarTrackerSettings->setB(settings.m_b);
    }
    if (featureSettingsKeys.contains("azimuthOffset") || force) {
        swgStarTrackerSettings->setAzimuthOffset(settings.m_azimuthOffset);
    }
    if (featureSettingsKeys.contains("elevationOffset") || force) {
        swgStarTrackerSettings->setElevationOffset(settings.m_elevationOffset);
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

void StarTracker::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "StarTracker::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("StarTracker::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void StarTracker::weatherUpdated(float temperature, float pressure, float humidity)
{
    QList<QString> settingsKeys;

    if (!std::isnan(temperature))
    {
        m_settings.m_temperature = temperature;
        settingsKeys.append("temperature");
    }

    if (!std::isnan(pressure))
    {
        m_settings.m_pressure = pressure;
        settingsKeys.append("pressure");
    }

    if (!std::isnan(humidity))
    {
        m_settings.m_humidity = humidity;
        settingsKeys.append("humidity");
    }

    if (m_worker) {
        m_worker->getInputMessageQueue()->push(StarTrackerWorker::MsgConfigureStarTrackerWorker::create(m_settings, settingsKeys, false));
    }

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureStarTracker::create(m_settings, settingsKeys, false));
    }
}

double StarTracker::applyBeam(const FITS *fits, double beamwidth, double ra, double dec, int& imgX, int& imgY) const
{
    const double halfBeamwidth = beamwidth/2.0;
    // Use cos^p(x) for approximation of radiation pattern
    // (Essentially the same as Gaussian of exp(-4*ln(theta^2/beamwidth^2))
    // (See a2 in https://arxiv.org/pdf/1812.10084.pdf for Elliptical equivalent))
    // We have gain of 0dB (1) at 0 degrees, and -3dB (~0.5) at half-beamwidth degrees
    // Find exponent that correponds to -3dB at that angle
    double minus3dBLinear = pow(10.0, -3.0/10.0);
    double p = log(minus3dBLinear)/log(cos(Units::degreesToRadians(halfBeamwidth)));
    // Create an matrix with gain as a function of angle
    double degreesPerPixelH = abs(fits->degreesPerPixelH());
    double degreesPerPixelV = abs(fits->degreesPerPixelV());
    int numberOfCoeffsH = ceil(beamwidth/degreesPerPixelH);
    int numberOfCoeffsV = ceil(beamwidth/degreesPerPixelV);
    if ((numberOfCoeffsH & 1) == 0) {
        numberOfCoeffsH++;
    }
    if ((numberOfCoeffsV & 1) == 0) {
        numberOfCoeffsV++;
    }
    double *beam = new double[numberOfCoeffsH*numberOfCoeffsV];
    double sum = 0.0;
    int y0 = numberOfCoeffsV/2;
    int x0 =  numberOfCoeffsH/2;
    int nonZeroCount = 0;
    for (int y = 0; y < numberOfCoeffsV; y++)
    {
        for (int x = 0; x < numberOfCoeffsH; x++)
        {
            double xp = (x - x0) * degreesPerPixelH;
            double yp = (y - y0) * degreesPerPixelV;
            double r = sqrt(xp*xp+yp*yp);
            if (r < halfBeamwidth)
            {
                beam[y*numberOfCoeffsH+x] = pow(cos(Units::degreesToRadians(r)), p);
                sum += beam[y*numberOfCoeffsH+x];
                nonZeroCount++;
            }
            else
            {
                beam[y*numberOfCoeffsH+x] = 0.0;
            }
        }
    }

    // Get centre pixel coordinates
    double centreX;
    if (ra <= 12.0) {
        centreX = (12.0 - ra) / 24.0;
    } else {
        centreX = (24 - ra + 12) / 24.0;
    }
    double centreY = (90.0-dec) / 180.0;
    imgX = centreX * fits->width();
    imgY = centreY * fits->height();

    // Apply weighting to temperature data
    double weightedSum = 0.0;
    for (int y = 0; y < numberOfCoeffsV; y++)
    {
        for (int x = 0; x < numberOfCoeffsH; x++)
        {
            weightedSum += beam[y*numberOfCoeffsH+x] * fits->scaledWrappedValue(imgX + (x-x0), imgY + (y-y0));
        }
    }
    // From: https://www.cv.nrao.edu/~sransom/web/Ch3.html
    // The antenna temperature equals the source brightness temperature multiplied by the fraction of the beam solid angle filled by the source
    // So we scale the sum by the total number of non-zero pixels (i.e. beam area)
    // If we compare to some maps with different beamwidths here: https://www.cv.nrao.edu/~demerson/radiosky/sky_jun96.pdf
    // The values we've computed are a bit higher..
    double temp = weightedSum/nonZeroCount;

    delete[] beam;

    return temp;
}

bool StarTracker::calcSkyTemperature(double frequency, double beamwidth, double ra, double dec, double& temp) const
{
    const FITS *fits;
    int imgX, imgY;

    if ((frequency >= 1.4e9) && (frequency <= 1.45e9))
    {
        // Adjust temperature from 1420MHz FITS file, just using beamwidth
        fits = getTempFITS(2);
        if (fits && fits->valid())
        {
            temp = applyBeam(fits, beamwidth, ra, dec, imgX, imgY);
            return true;
        }
        else
        {
            qDebug() << "StarTracker::calcSkyTemperature: 1420MHz FITS temperature file not valid";
            return false;
        }
    }
    else
    {
        // Adjust temperature from 408MHz FITS file, taking in to account
        // observation frequency and beamwidth
        fits = getTempFITS(1);
        if (fits && fits->valid())
        {
            double temp408 = applyBeam(fits, beamwidth, ra, dec, imgX, imgY);

            // Scale according to frequency - CMB contribution constant
            // Power law at low frequencies, with slight variation in spectral index
            // See:
            // Global Sky Model: https://ascl.net/1011.010
            // An improved Model of Diffuse Galactic Radio Emission: https://arxiv.org/pdf/1605.04920.pdf
            // A high-resolution self-consistent whole sky foreground model: https://arxiv.org/abs/1812.10084
            // (De-striping:) Full sky study of diffuse Galactic emission at decimeter wavelength  https://www.aanda.org/articles/aa/pdf/2003/42/aah4363.pdf
            //               Data here: http://cdsarc.u-strasbg.fr/viz-bin/cat/J/A+A/410/847
            // LFmap: https://www.faculty.ece.vt.edu/swe/lwa/memo/lwa0111.pdf
            double iso408 = 50 * pow(150e6/408e6, 2.75);                 // Extra-galactic isotropic in reference map at 408MHz
            double isoT = 50 * pow(150e6/frequency, 2.75);  // Extra-galactic isotropic at target frequency
            double cmbT = 2.725; // Cosmic microwave backgroud;
            double spectralIndex;
            const FITS *spectralIndexFITS = getSpectralIndexFITS();
            if (spectralIndexFITS && spectralIndexFITS->valid())
            {
                 // See https://www.aanda.org/articles/aa/pdf/2003/42/aah4363.pdf
                 spectralIndex = spectralIndexFITS->scaledValue(imgX, imgY);
            }
            else
            {
                // See https://arxiv.org/abs/1812.10084 fig 2
                if (frequency < 200e6) {
                    spectralIndex = 2.55;
                } else if (frequency < 20e9) {
                     spectralIndex = 2.695;
                } else {
                     spectralIndex = 3.1;
                }
            }
            double galactic480 = temp408 - cmbT - iso408;
            double galacticT = galactic480 * pow(408e6/frequency, spectralIndex); // Scale galactic contribution by frequency
            temp = galacticT + cmbT + isoT;      // Final temperature

            return true;
        }
        else
        {
            qDebug() << "StarTracker::calcSkyTemperature: 408MHz FITS temperature file not valid";
            return false;
        }
    }
}

void StarTracker::scanAvailableChannels()
{
    MainCore *mainCore = MainCore::instance();
    MessagePipes& messagePipes = mainCore->getMessagePipes();
    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
    m_availableChannels.clear();

    for (const auto& deviceSet : deviceSets)
    {
        DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

        if (deviceSourceEngine)
        {
            for (int chi = 0; chi < deviceSet->getNumberOfChannels(); chi++)
            {
                ChannelAPI *channel = deviceSet->getChannelAt(chi);

                if (StarTrackerSettings::m_pipeURIs.contains(channel->getURI()) && !m_availableChannels.contains(channel))
                {
                    qDebug("StarTracker::scanAvailableChannels: register %d:%d %s (%p)",
                        deviceSet->getIndex(), chi, qPrintable(channel->getURI()), channel);
                    ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, this, "startracker.display");
                    MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                    QObject::connect(
                        messageQueue,
                        &MessageQueue::messageEnqueued,
                        this,
                        [=](){ this->handleChannelMessageQueue(messageQueue); },
                        Qt::QueuedConnection
                    );
                    QObject::connect(
                        pipe,
                        &ObjectPipe::toBeDeleted,
                        this,
                        &StarTracker::handleMessagePipeToBeDeleted
                    );
                    m_availableChannels.insert(channel);
                }
            }
        }
    }
}

void StarTracker::handleChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    qDebug("StarTracker::handleChannelAdded: deviceSetIndex: %d:%d channel: %s (%p)",
        deviceSetIndex, channel->getIndexInDeviceSet(), qPrintable(channel->getURI()), channel);
    DeviceSet *deviceSet = MainCore::instance()->getDeviceSets()[deviceSetIndex];
    DSPDeviceSourceEngine *deviceSourceEngine =  deviceSet->m_deviceSourceEngine;

    if (deviceSourceEngine && StarTrackerSettings::m_pipeURIs.contains(channel->getURI()))
    {
        if (!m_availableChannels.contains(channel))
        {
            MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
            ObjectPipe *pipe = messagePipes.registerProducerToConsumer(channel, this, "startracker.display");
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            QObject::connect(
                messageQueue,
                &MessageQueue::messageEnqueued,
                this,
                [=](){ this->handleChannelMessageQueue(messageQueue); },
                Qt::QueuedConnection
            );
            QObject::connect(
                pipe,
                &ObjectPipe::toBeDeleted,
                this,
                &StarTracker::handleMessagePipeToBeDeleted
            );
            m_availableChannels.insert(channel);
        }
    }
}

void StarTracker::handleMessagePipeToBeDeleted(int reason, QObject* object)
{
    if ((reason == 0) && m_availableChannels.contains((ChannelAPI*) object)) // producer (channel)
    {
        qDebug("StarTracker::handleMessagePipeToBeDeleted: removing channel at (%p)", object);
        m_availableChannels.remove((ChannelAPI*) object);
    }
}

void StarTracker::scanAvailableFeatures()
{
    qDebug("StarTracker::scanAvailableFeatures");
    MainCore *mainCore = MainCore::instance();
    std::vector<FeatureSet*>& featureSets = mainCore->getFeatureeSets();
    m_satelliteTrackers.clear();

    for (const auto& featureSet : featureSets)
    {
        for (int fei = 0; fei < featureSet->getNumberOfFeatures(); fei++)
        {
            Feature *feature = featureSet->getFeatureAt(fei);

            if (feature->getURI() == "sdrangel.feature.satellitetracker")
            {
                StarTrackerSettings::AvailableFeature satelliteTracker =
                    StarTrackerSettings::AvailableFeature{featureSet->getIndex(), fei, feature->getIdentifier()};
                m_satelliteTrackers[feature] = satelliteTracker;
            }
        }
    }

    notifyUpdateSatelliteTrackers();
}

void StarTracker::handleFeatureAdded(int featureSetIndex, Feature *feature)
{
    (void) featureSetIndex;
    (void) feature;

    scanAvailableFeatures();
}

void StarTracker::handleFeatureRemoved(int featureSetIndex, Feature *feature)
{
    (void) featureSetIndex;
    (void) feature;

    scanAvailableFeatures();
}

void StarTracker::notifyUpdateSatelliteTrackers()
{
    if (getMessageQueueToGUI())
    {
        MsgReportAvailableSatelliteTrackers *msg = MsgReportAvailableSatelliteTrackers::create();
        msg->getFeatures() = m_satelliteTrackers.values();
        getMessageQueueToGUI()->push(msg);
    }
}

void StarTracker::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}
