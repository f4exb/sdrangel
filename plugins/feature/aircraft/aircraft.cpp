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
#include <QThread>
#include <QBuffer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <algorithm>
#include <type_traits>

#include "SWGMapItem.h"
#include "SWGFeatureSettings.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureActions.h"
#include "SWGAircraftSettings.h"
#include "SWGAircraftReport.h"
#include "SWGAircraftActions.h"
#include "SWGAircraftNotificationSettings.h"
#include "SWGAircraftProtocolReport.h"

#include "maincore.h"

#include "channel/channelapi.h"

#include "aircraft.h"

MESSAGE_CLASS_DEFINITION(Aircraft::MsgConfigureAircraft, Message)

const char* const Aircraft::m_featureIdURI = "sdrangel.feature.aircraft";
const char* const Aircraft::m_featureId = "Aircraft";

Aircraft::Aircraft(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_availableChannelHandler(AircraftSettings::sourceURIs(),
                              QStringList{"aircraftreport", "acarsfeed"})
{
    qDebug("Aircraft::Aircraft: webAPIAdapterInterface: %p", webAPIAdapterInterface);
    setObjectName(m_featureId);
    m_state = StIdle;
    m_errorMessage = "Aircraft error";
    QObject::connect(
        &m_availableChannelHandler,
        &AvailableChannelOrFeatureHandler::messageEnqueued,
        this,
        &Aircraft::handleChannelMessageQueue);
    QObject::connect(
        &m_availableChannelHandler,
        &AvailableChannelOrFeatureHandler::channelsOrFeaturesChanged,
        this,
        &Aircraft::updateReportPipes);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Aircraft::networkManagerFinished);

    // All processing runs on the tracker's thread, for the feature's lifetime,
    // so it works the same with or without a GUI
    m_thread = new QThread();
    m_tracker = new AircraftTracker(this);
    m_tracker->moveToThread(m_thread);
    QObject::connect(m_thread, &QThread::started, m_tracker, &AircraftTracker::startWork);
    QObject::connect(m_thread, &QThread::finished, m_tracker, &QObject::deleteLater);
    QObject::connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);
    m_thread->start();
}

Aircraft::~Aircraft()
{
    QObject::disconnect(
        &m_availableChannelHandler,
        &AvailableChannelOrFeatureHandler::messageEnqueued,
        this,
        &Aircraft::handleChannelMessageQueue);
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &Aircraft::networkManagerFinished);
    delete m_networkManager;
    m_thread->quit();
    m_thread->wait();
    m_tracker = nullptr;
    m_thread = nullptr;
}

void Aircraft::start()
{
    qDebug("Aircraft::start");
    m_state = StRunning;
}

void Aircraft::stop()
{
    qDebug("Aircraft::stop");
    m_state = StIdle;
}

bool Aircraft::handleMessage(const Message& cmd)
{
    if (MsgConfigureAircraft::match(cmd))
    {
        MsgConfigureAircraft& cfg = (MsgConfigureAircraft&) cmd;
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
        return true;
    }
    else if (MainCore::MsgAcarsBlock::match(cmd))
    {
        // Straight to the tracker's thread, which owns the archive and the connections.
        // Not filtered by m_disabledSources: that says which demodulators contribute to
        // the aircraft tables, and a receiver may reasonably want to archive and feed a
        // band it does not track.
        MainCore::MsgAcarsBlock& block = (MainCore::MsgAcarsBlock&) cmd;
        if (m_tracker) {
            m_tracker->getInputMessageQueue()->push(AircraftTracker::MsgAcarsBlock::create(
                block.getBlock(), block.getDateTime(), block.getFrequency(),
                block.getLevel(), block.getProtocol()));
        }
        return true;
    }
    else if (MainCore::MsgAircraftReport::match(cmd))
    {
        // All processing happens on the tracker's thread
        MainCore::MsgAircraftReport& report = (MainCore::MsgAircraftReport&) cmd;

        // The user can turn individual demodulators off. Their pipes are withdrawn, but
        // reports already queued still arrive. The source is only compared: it may have
        // been deleted since it queued this.
        const bool wanted = !m_disabledChannels.contains(report.getPipeSource());

        if (m_tracker && wanted)
        {
            m_tracker->getInputMessageQueue()->push(AircraftTracker::MsgReport::create(report.getReport()));
            report.getReport().m_mapItem = nullptr;     // Ownership moved with the copy
        }
        else if (report.getReport().m_mapItem)
        {
            delete report.getReport().m_mapItem;
            report.getReport().m_mapItem = nullptr;
        }
        return true;
    }
    else
    {
        return false;
    }
}

QByteArray Aircraft::serialize() const
{
    return m_settings.serialize();
}

bool Aircraft::deserialize(const QByteArray& data)
{
    bool ok = m_settings.deserialize(data);
    if (!ok) {
        m_settings.resetToDefaults();
    }
    MsgConfigureAircraft *msg = MsgConfigureAircraft::create(m_settings, QList<QString>(), true);
    m_inputMessageQueue.push(msg);
    return ok;
}

void Aircraft::applySettings(const AircraftSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "Aircraft::applySettings:" << settings.getDebugString(settingsKeys, force) << force;

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (settingsKeys.contains("disabledSources") || force) {
        updateReportPipes();
    }

    if (m_settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && m_settings.m_useReverseAPI) ||
                settingsKeys.contains("reverseAPIAddress") ||
                settingsKeys.contains("reverseAPIPort") ||
                settingsKeys.contains("reverseAPIFeatureSetIndex") ||
                settingsKeys.contains("reverseAPIFeatureIndex");
        webapiReverseSendSettings(settingsKeys, m_settings, fullUpdate || force);
    }

    if (m_tracker) {
        m_tracker->getInputMessageQueue()->push(AircraftTracker::MsgConfigureTracker::create(settings, settingsKeys, force));
    }
}

// The settings that changed, or all of them when forced, to the remote instance - built
// by the same code as a settings GET, then cut down to those keys. The reverse API's own
// settings are never sent: they say where to send, not what.
void Aircraft::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const AircraftSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings swgFeatureSettings;
    swgFeatureSettings.setAircraftSettings(new SWGSDRangel::SWGAircraftSettings());
    swgFeatureSettings.getAircraftSettings()->init();
    webapiFormatFeatureSettings(swgFeatureSettings, settings);

    QJsonObject *all = swgFeatureSettings.getAircraftSettings()->asJsonObject();
    QJsonObject sent;
    static const QStringList reverseAPIKeys = {
        "useReverseAPI", "reverseAPIAddress", "reverseAPIPort",
        "reverseAPIFeatureSetIndex", "reverseAPIFeatureIndex"
    };
    for (auto it = all->constBegin(); it != all->constEnd(); ++it)
    {
        // The JSON names are the settings keys, but for the casing of "API"
        auto sameKey = [&it](const QString& key) { return key.compare(it.key(), Qt::CaseInsensitive) == 0; };
        const bool reverseAPIKey = std::any_of(reverseAPIKeys.begin(), reverseAPIKeys.end(), sameKey);
        const bool changed = std::any_of(featureSettingsKeys.begin(), featureSettingsKeys.end(), sameKey);
        if (!reverseAPIKey && (force || changed)) {
            sent.insert(it.key(), it.value());
        }
    }
    delete all;
    // asJsonObject() leaves out an empty list or string altogether, so emptying one -
    // re-enabling the last disabled source, deleting the last rule - would never be sent
    // and the remote would keep the old value for good
    static const QHash<QString, QJsonValue> emptyValues = {
        {"disabledSources", QJsonArray()}, {"notificationSettings", QJsonArray()},
        {"title", QString()}, {"databaseFilename", QString()}, {"feedStationId", QString()},
        {"feedAirframesHost", QString()}, {"feedAvdelphiHost", QString()}
    };
    for (auto it = emptyValues.constBegin(); it != emptyValues.constEnd(); ++it)
    {
        if (!sent.contains(it.key()) && (force || featureSettingsKeys.contains(it.key()))) {
            sent.insert(it.key(), it.value());
        }
    }
    if (sent.isEmpty()) {
        return;
    }

    QJsonObject body;
    body.insert("featureType", "Aircraft");
    body.insert("AircraftSettings", sent);

    QString featureSettingsURL = QString("http://%1:%2/sdrangel/featureset/%3/feature/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIFeatureSetIndex)
            .arg(settings.m_reverseAPIFeatureIndex);
    m_networkRequest.setUrl(QUrl(featureSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(QJsonDocument(body).toJson(QJsonDocument::Compact));
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);
}

void Aircraft::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "Aircraft::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("Aircraft::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

// A disabled demodulator loses its "aircraftreport" pipe rather than having its reports
// dropped here: ADS-B sends to the Map itself only when it has no such pipe, so dropping
// them took ADS-B aircraft off the Map altogether.
void Aircraft::updateReportPipes()
{
    const AvailableChannelOrFeatureList& available = m_availableChannelHandler.getAvailableChannelOrFeatureList();

    // A channel that has gone took its pipe with it, and a new one may reuse its address
    for (auto it = m_disabledChannels.begin(); it != m_disabledChannels.end(); )
    {
        if (available.indexOfObject(*it) < 0) {
            it = m_disabledChannels.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& channelOrFeature : available)
    {
        const ChannelAPI *channel = qobject_cast<const ChannelAPI *>(channelOrFeature.m_object);
        const bool disable = channel && m_settings.m_disabledSources.contains(channel->getURI());
        const bool disabled = m_disabledChannels.contains(channelOrFeature.m_object);

        if (disable)
        {
            // Every time, not only when first disabled: a channel whose index changes
            // looks new to the handler's rescan, which registers its pipes again
            m_availableChannelHandler.deregisterPipes(channelOrFeature.m_object, {"aircraftreport"});
            m_disabledChannels.insert(channelOrFeature.m_object);
        }
        else if (disabled)
        {
            m_availableChannelHandler.registerPipes(channelOrFeature.getLongId(), {"aircraftreport"});
            m_disabledChannels.remove(channelOrFeature.m_object);
        }
    }
}

void Aircraft::handleChannelMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

int Aircraft::webapiSettingsGet(
        SWGSDRangel::SWGFeatureSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAircraftSettings(new SWGSDRangel::SWGAircraftSettings());
    response.getAircraftSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

int Aircraft::webapiSettingsPutPatch(
        bool force,
        const QStringList& featureSettingsKeys,
        SWGSDRangel::SWGFeatureSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    AircraftSettings settings = m_settings;
    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    MsgConfigureAircraft *msg = MsgConfigureAircraft::create(settings, featureSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue)      // forward to GUI if any
    {
        MsgConfigureAircraft *msgToGUI = MsgConfigureAircraft::create(settings, featureSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatFeatureSettings(response, settings);
    return 200;
}

int Aircraft::webapiReportGet(
        SWGSDRangel::SWGFeatureReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setAircraftReport(new SWGSDRangel::SWGAircraftReport());
    response.getAircraftReport()->init();
    webapiFormatFeatureReport(response);
    return 200;
}

// Selecting an aircraft or a flight from outside. This is what the links in an aircraft's
// Map popup come back through: the Map has no idea what an aircraft is, so it posts the
// action and this feature does the work.
int Aircraft::webapiActionsPost(
        const QStringList& featureActionsKeys,
        SWGSDRangel::SWGFeatureActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGAircraftActions *swgAircraftActions = query.getAircraftActions();

    if (!swgAircraftActions)
    {
        errorMessage = "Missing AircraftActions in query";
        return 400;
    }

    struct Action {
        const char *m_key;
        AircraftTracker::MsgSelect::Kind m_kind;
        QString *m_value;
    };
    const Action actions[] = {
        { "selectIcao", AircraftTracker::MsgSelect::Icao, swgAircraftActions->getSelectIcao() },
        { "selectRegistration", AircraftTracker::MsgSelect::Registration,
          swgAircraftActions->getSelectRegistration() },
        { "selectFlight", AircraftTracker::MsgSelect::Flight, swgAircraftActions->getSelectFlight() },
    };

    for (const Action& action : actions)
    {
        if (!featureActionsKeys.contains(action.m_key) || !action.m_value) {
            continue;
        }
        // Through the tracker's queue rather than straight to the GUI, so that a select
        // arriving while the tracker is mid-flush is applied in order with everything
        // else - and so that it works the same way whether or not a GUI exists
        if (m_tracker) {
            m_tracker->getInputMessageQueue()->push(
                AircraftTracker::MsgSelect::create(action.m_kind, *action.m_value));
        }
    }
    return 202;
}

void Aircraft::webapiFormatFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& response,
        const AircraftSettings& settings)
{
    SWGSDRangel::SWGAircraftSettings *swg = response.getAircraftSettings();

    swg->setRemovalMins(settings.m_removalMins);
    swg->setAdsbPositionMins(settings.m_adsbPositionMins);
    swg->setAcarsPositionMins(settings.m_acarsPositionMins);
    swg->setRetentionDays(settings.m_retentionDays);
    swg->setFlightRetentionDays(settings.m_flightRetentionDays);
    swg->setAtcLabels(settings.m_atcLabels ? 1 : 0);
    swg->setAtcCallsigns(settings.m_atcCallsigns ? 1 : 0);
    swg->setDisplayMaxRangeOnMap(settings.m_displayMaxRangeOnMap ? 1 : 0);
    swg->setFavourLivery(settings.m_favourLivery ? 1 : 0);
    swg->setDisplayStatistics(settings.m_displayStatistics ? 1 : 0);
    swg->setDisplayChart(settings.m_displayChart ? 1 : 0);
    swg->setDisplayCoverage(settings.m_displayCoverage ? 1 : 0);
    swg->setUseLiveryIcons(settings.m_useLiveryIcons ? 1 : 0);
    swg->setFeedEnabled(settings.m_feedEnabled ? 1 : 0);
    swg->setFeedAirframes(settings.m_feedAirframes ? 1 : 0);
    swg->setFeedAirframesPort(settings.m_feedAirframesPort);
    swg->setFeedAirframesTcp(settings.m_feedAirframesTcp ? 1 : 0);
    swg->setFeedAvdelphi(settings.m_feedAvdelphi ? 1 : 0);
    swg->setFeedAvdelphiPort(settings.m_feedAvdelphiPort);
    swg->setFeedAvdelphiTcp(settings.m_feedAvdelphiTcp ? 1 : 0);
    swg->setRgbColor(settings.m_rgbColor);
    swg->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    swg->setReverseApiPort(settings.m_reverseAPIPort);
    swg->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    swg->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);
    swg->setWorkspaceIndex(settings.m_workspaceIndex);

    // The generated setters take ownership of what they are given, and the default
    // constructor leaves every pointer null - so assign in place where one is already
    // there and allocate where it is not
    auto setString = [](QString **held, const QString& value)
    {
        if (*held) {
            **held = value;
        } else {
            *held = new QString(value);
        }
    };
    QString *title = swg->getTitle();
    setString(&title, settings.m_title);
    swg->setTitle(title);
    QString *database = swg->getDatabaseFilename();
    setString(&database, settings.m_databaseFilename);
    swg->setDatabaseFilename(database);
    QString *stationId = swg->getFeedStationId();
    setString(&stationId, settings.m_feedStationId);
    swg->setFeedStationId(stationId);
    QString *airframesHost = swg->getFeedAirframesHost();
    setString(&airframesHost, settings.m_feedAirframesHost);
    swg->setFeedAirframesHost(airframesHost);
    QString *avdelphiHost = swg->getFeedAvdelphiHost();
    setString(&avdelphiHost, settings.m_feedAvdelphiHost);
    swg->setFeedAvdelphiHost(avdelphiHost);

    // The lists likewise. init() has usually allocated them already, and on a PUT or
    // PATCH the response is the request, holding the lists that were parsed into it - so
    // replacing them through the setters leaked them, and everything in them
    auto emptyList = [](auto *held)
    {
        using List = std::remove_pointer_t<decltype(held)>;
        if (!held) {
            return new List();
        }
        qDeleteAll(*held);
        held->clear();
        return held;
    };

    QList<QString *> *disabled = emptyList(swg->getDisabledSources());
    for (const QString& uri : settings.m_disabledSources) {
        disabled->append(new QString(uri));
    }
    swg->setDisabledSources(disabled);

    QList<SWGSDRangel::SWGAircraftNotificationSettings *> *rules =
        emptyList(swg->getNotificationSettings());
    for (const auto& rule : settings.m_notificationSettings)
    {
        if (!rule) {
            continue;
        }
        SWGSDRangel::SWGAircraftNotificationSettings *swgRule =
            new SWGSDRangel::SWGAircraftNotificationSettings();
        swgRule->init();
        swgRule->setMatchColumn(rule->m_matchColumn);
        swgRule->setRegExp(new QString(rule->m_regExp));
        swgRule->setSpeech(new QString(rule->m_speech));
        swgRule->setCommand(new QString(rule->m_command));
        rules->append(swgRule);
    }
    swg->setNotificationSettings(rules);

    QString *address = swg->getReverseApiAddress();
    setString(&address, settings.m_reverseAPIAddress);
    swg->setReverseApiAddress(address);
}

void Aircraft::webapiUpdateFeatureSettings(
        AircraftSettings& settings,
        const QStringList& featureSettingsKeys,
        SWGSDRangel::SWGFeatureSettings& response)
{
    SWGSDRangel::SWGAircraftSettings *swg = response.getAircraftSettings();
    if (!swg) {
        return;
    }

    if (featureSettingsKeys.contains("removalMins")) {
        settings.m_removalMins = swg->getRemovalMins();
    }
    if (featureSettingsKeys.contains("adsbPositionMins")) {
        settings.m_adsbPositionMins = swg->getAdsbPositionMins();
    }
    if (featureSettingsKeys.contains("acarsPositionMins")) {
        settings.m_acarsPositionMins = swg->getAcarsPositionMins();
    }
    if (featureSettingsKeys.contains("retentionDays")) {
        settings.m_retentionDays = swg->getRetentionDays();
    }
    if (featureSettingsKeys.contains("flightRetentionDays")) {
        settings.m_flightRetentionDays = swg->getFlightRetentionDays();
    }
    if (featureSettingsKeys.contains("atcLabels")) {
        settings.m_atcLabels = swg->getAtcLabels() != 0;
    }
    if (featureSettingsKeys.contains("atcCallsigns")) {
        settings.m_atcCallsigns = swg->getAtcCallsigns() != 0;
    }
    if (featureSettingsKeys.contains("displayMaxRangeOnMap")) {
        settings.m_displayMaxRangeOnMap = swg->getDisplayMaxRangeOnMap() != 0;
    }
    if (featureSettingsKeys.contains("favourLivery")) {
        settings.m_favourLivery = swg->getFavourLivery() != 0;
    }
    if (featureSettingsKeys.contains("displayStatistics")) {
        settings.m_displayStatistics = swg->getDisplayStatistics() != 0;
    }
    if (featureSettingsKeys.contains("displayChart")) {
        settings.m_displayChart = swg->getDisplayChart() != 0;
    }
    if (featureSettingsKeys.contains("displayCoverage")) {
        settings.m_displayCoverage = swg->getDisplayCoverage() != 0;
    }
    if (featureSettingsKeys.contains("useLiveryIcons")) {
        settings.m_useLiveryIcons = swg->getUseLiveryIcons() != 0;
    }
    if (featureSettingsKeys.contains("feedEnabled")) {
        settings.m_feedEnabled = swg->getFeedEnabled() != 0;
    }
    if (featureSettingsKeys.contains("feedStationId") && swg->getFeedStationId()) {
        settings.m_feedStationId = *swg->getFeedStationId();
    }
    if (featureSettingsKeys.contains("feedAirframes")) {
        settings.m_feedAirframes = swg->getFeedAirframes() != 0;
    }
    if (featureSettingsKeys.contains("feedAirframesHost") && swg->getFeedAirframesHost()) {
        settings.m_feedAirframesHost = *swg->getFeedAirframesHost();
    }
    if (featureSettingsKeys.contains("feedAirframesPort")) {
        settings.m_feedAirframesPort = swg->getFeedAirframesPort();
    }
    if (featureSettingsKeys.contains("feedAirframesTcp")) {
        settings.m_feedAirframesTcp = swg->getFeedAirframesTcp() != 0;
    }
    if (featureSettingsKeys.contains("feedAvdelphi")) {
        settings.m_feedAvdelphi = swg->getFeedAvdelphi() != 0;
    }
    if (featureSettingsKeys.contains("feedAvdelphiHost") && swg->getFeedAvdelphiHost()) {
        settings.m_feedAvdelphiHost = *swg->getFeedAvdelphiHost();
    }
    if (featureSettingsKeys.contains("feedAvdelphiPort")) {
        settings.m_feedAvdelphiPort = swg->getFeedAvdelphiPort();
    }
    if (featureSettingsKeys.contains("feedAvdelphiTcp")) {
        settings.m_feedAvdelphiTcp = swg->getFeedAvdelphiTcp() != 0;
    }
    if (featureSettingsKeys.contains("databaseFilename") && swg->getDatabaseFilename()) {
        settings.m_databaseFilename = *swg->getDatabaseFilename();
    }
    if (featureSettingsKeys.contains("disabledSources"))
    {
        settings.m_disabledSources.clear();
        if (swg->getDisabledSources())
        {
            for (const QString *uri : *swg->getDisabledSources())
            {
                if (uri) {
                    settings.m_disabledSources.append(*uri);
                }
            }
        }
    }
    if (featureSettingsKeys.contains("notificationSettings"))
    {
        settings.m_notificationSettings.clear();
        if (swg->getNotificationSettings())
        {
            for (const SWGSDRangel::SWGAircraftNotificationSettings *swgRule : *swg->getNotificationSettings())
            {
                if (!swgRule) {
                    continue;
                }
                QSharedPointer<AircraftSettings::NotificationSettings> rule(
                    new AircraftSettings::NotificationSettings());
                rule->m_matchColumn =
                    const_cast<SWGSDRangel::SWGAircraftNotificationSettings *>(swgRule)->getMatchColumn();
                auto text = [](QString *p) { return p ? *p : QString(); };
                rule->m_regExp = text(const_cast<SWGSDRangel::SWGAircraftNotificationSettings *>(swgRule)->getRegExp());
                rule->m_speech = text(const_cast<SWGSDRangel::SWGAircraftNotificationSettings *>(swgRule)->getSpeech());
                rule->m_command = text(const_cast<SWGSDRangel::SWGAircraftNotificationSettings *>(swgRule)->getCommand());
                rule->updateRegularExpression();
                settings.m_notificationSettings.append(rule);
            }
        }
    }
    if (featureSettingsKeys.contains("title") && swg->getTitle()) {
        settings.m_title = *swg->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = swg->getRgbColor();
    }
    if (featureSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = swg->getUseReverseApi() != 0;
    }
    if (featureSettingsKeys.contains("reverseAPIAddress") && swg->getReverseApiAddress()) {
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
    if (featureSettingsKeys.contains("workspaceIndex")) {
        settings.m_workspaceIndex = swg->getWorkspaceIndex();
    }
}

void Aircraft::webapiFormatFeatureReport(SWGSDRangel::SWGFeatureReport& response)
{
    SWGSDRangel::SWGAircraftReport *swg = response.getAircraftReport();
    if (!m_tracker) {
        return;
    }

    const AircraftTracker::Totals totals = m_tracker->totals();
    swg->setAircraftCount(totals.m_aircraft);
    swg->setActiveAircraftCount(totals.m_activeAircraft);
    swg->setFlightCount(totals.m_flights);
    swg->setMessageCount(totals.m_messages);

    QList<SWGSDRangel::SWGAircraftProtocolReport *> *protocols =
        new QList<SWGSDRangel::SWGAircraftProtocolReport *>();
    for (int i = 0; i < AircraftReport::ProtocolCount; i++)
    {
        // Not init(): that allocates the protocol string the setter would then replace
        SWGSDRangel::SWGAircraftProtocolReport *swgProtocol =
            new SWGSDRangel::SWGAircraftProtocolReport();
        swgProtocol->setProtocol(new QString(m_tracker->protocolName(i)));
        swgProtocol->setMessageCount(totals.m_messagesByProtocol[i]);
        protocols->append(swgProtocol);
    }
    // Into the list init() has already allocated, rather than leaking it on every GET
    if (swg->getProtocols())
    {
        qDeleteAll(*swg->getProtocols());
        *swg->getProtocols() = *protocols;
        delete protocols;
    }
    else
    {
        swg->setProtocols(protocols);
    }
}
