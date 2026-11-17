///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <algorithm>

#include "SWGChannelActions.h"
#include "SWGFeatureActions.h"
#include "SWGFeatureReport.h"
#include "SWGFeatureSettings.h"
#include "SWGMessageCenterActions.h"
#include "SWGMessageCenterPost.h"
#include "SWGMessageCenterRedecode.h"
#include "SWGMessageCenterReport.h"
#include "SWGMessageCenterSettings.h"
#include "SWGMapItem.h"
#include "SWGRadioPagingModActions.h"
#include "SWGRadioPagingModTx.h"

#include "channel/channelapi.h"
#include "maincore.h"
#include "util/units.h"
#include "radiopagingpage.h"
#include "radiopagingrecode.h"

#include "messagecenter.h"
#include "messagecenterarchive.h"

MESSAGE_CLASS_DEFINITION(MessageCenter::MsgConfigureMessageCenter, Message)
MESSAGE_CLASS_DEFINITION(MessageCenter::MsgPostPage, Message)
MESSAGE_CLASS_DEFINITION(MessageCenter::MsgPageAction, Message)
MESSAGE_CLASS_DEFINITION(MessageCenter::MsgUpdated, Message)
MESSAGE_CLASS_DEFINITION(MessageCenter::MsgRedecoded, Message)
MESSAGE_CLASS_DEFINITION(MessageCenter::MsgClearLog, Message)
MESSAGE_CLASS_DEFINITION(MessageCenter::MsgRemoveEndpoints, Message)
MESSAGE_CLASS_DEFINITION(MessageCenter::MsgImportFile, Message)
MESSAGE_CLASS_DEFINITION(MessageCenter::MsgIngestRecords, Message)
MESSAGE_CLASS_DEFINITION(MessageCenter::MsgSpeak, Message)

const char* const MessageCenter::m_featureIdURI = "sdrangel.feature.messagecenter";
const char* const MessageCenter::m_featureId = "MessageCenter";

namespace {

// The channels that send messages: the paging demodulator and modulator
const QStringList channelURIs = {"sdrangel.channel.radiopagingdemod", "sdrangel.channeltx.modradiopaging"};

const int tickMs = 500;

} // namespace

MessageCenter::MessageCenter(WebAPIAdapterInterface *webAPIAdapterInterface) :
    Feature(m_featureIdURI, webAPIAdapterInterface),
    m_availableChannelHandler(channelURIs, QStringList{MessageRecord::m_pipeName}, "RTMF", this)
{
    setObjectName(m_featureId);
    setState(StIdle);
    m_settings.setRollupState(nullptr);

    QObject::connect(&m_availableChannelHandler, &AvailableChannelOrFeatureHandler::messageEnqueued,
        this, &MessageCenter::handleChannelMessageQueue);
    m_availableChannelHandler.scanAvailableChannelsAndFeatures();

    QObject::connect(&m_timer, &QTimer::timeout, this, &MessageCenter::tick);
    applyEngineSettings();
    m_timer.start(tickMs);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(m_networkManager, &QNetworkAccessManager::finished, this, &MessageCenter::networkManagerFinished);
}

MessageCenter::~MessageCenter()
{
    m_timer.stop();
    QObject::disconnect(&m_availableChannelHandler, &AvailableChannelOrFeatureHandler::messageEnqueued,
        this, &MessageCenter::handleChannelMessageQueue);
    QObject::disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &MessageCenter::networkManagerFinished);
    delete m_networkManager;

    // The history file is free for another message center
    if (historyOwner() == this) {
        historyOwner() = nullptr;
    }
}

// The message center that writes the history file: two of them would write the same messages twice
MessageCenter*& MessageCenter::historyOwner()
{
    static MessageCenter *owner = nullptr;
    return owner;
}

QString MessageCenter::historyFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/messagecenter/history";
}

// Used by reference (std::min): defined once
const int MessageCenter::m_maxQueryLimit;

MessageCenterArchiveResult MessageCenter::queryMessages(qint64 from, qint64 to, const MessageCenterFilter& filter, int offset, int limit) const
{
    std::shared_ptr<const MessageCenterArchive> archive;
    {
        QMutexLocker mutexLocker(&m_mutex);
        archive = m_engine.archive();

        if (!archive) {
            return m_engine.queryLog(from, to, filter, offset, limit);
        }
    }

    // The archive reads its files under its own lock only: the engine keeps storing meanwhile
    MessageCenterArchive::Query query;
    query.m_from = from;
    query.m_to = to;
    query.m_filter = filter;
    query.m_offset = offset;
    query.m_limit = limit;
    return archive->query(query);
}

// Called with the engine locked
void MessageCenter::applyHistory()
{
    QString path;

    if (m_settings.m_historyEnabled)
    {
        if (historyOwner() && (historyOwner() != this))
        {
            m_historyError = "Another message center keeps the history";
        }
        else
        {
            path = historyFilePath();
            historyOwner() = this;
            m_historyError.clear();
        }
    }
    else
    {
        if (historyOwner() == this) {
            historyOwner() = nullptr;
        }

        m_historyError.clear();
    }

    QString error;

    if (!m_engine.setHistory(path, m_settings.m_historyDays, (qint64) m_settings.m_historyMaxMB * 1024 * 1024, error))
    {
        m_historyError = error;
        qWarning("MessageCenter::applyHistory: %s", qPrintable(error));
    }
    if (!m_historyError.isEmpty()) {
        qInfo("MessageCenter::applyHistory: %s", qPrintable(m_historyError));
    }
}

void MessageCenter::handleChannelMessageQueue(MessageQueue *messageQueue)
{
    Message *message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool MessageCenter::handleMessage(const Message& cmd)
{
    QMutexLocker mutexLocker(&m_mutex);
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (MsgConfigureMessageCenter::match(cmd))
    {
        const MsgConfigureMessageCenter& cfg = (const MsgConfigureMessageCenter&) cmd;
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
        return true;
    }
    else if (MainCore::MsgMessageRecord::match(cmd))
    {
        const MainCore::MsgMessageRecord& msg = (const MainCore::MsgMessageRecord&) cmd;
        ingest(msg.getRecord(), msg.getPipeSource());
        return true;
    }
    else if (MsgPostPage::match(cmd))
    {
        const MsgPostPage& msg = (const MsgPostPage&) cmd;
        QString id = m_engine.addPage(msg.getRecord(), now);

        if (id.isEmpty()) {
            m_errors.append("A page needs a transmitter and an id of its own");
        }

        m_dirty = true;
        return true;
    }
    else if (MsgPageAction::match(cmd))
    {
        const MsgPageAction& msg = (const MsgPageAction&) cmd;

        if (msg.getAction() == MsgPageAction::Requeue) {
            m_engine.requeue(msg.getId(), now);
        } else if (msg.getAction() == MsgPageAction::Cancel) {
            m_engine.cancel(msg.getId());
        } else {
            m_engine.removeFinished();
        }

        m_dirty = true;
        return true;
    }
    else if (MsgRedecoded::match(cmd))
    {
        const MsgRedecoded& msg = (const MsgRedecoded&) cmd;
        m_engine.replace(msg.getRecord());
        m_dirty = true;
        return true;
    }
    else if (MsgClearLog::match(cmd))
    {
        m_engine.clearLog();

        if (((const MsgClearLog&) cmd).getHistory())
        {
            QString error;

            if (!m_engine.clearHistory(error)) {
                qWarning("MessageCenter::handleMessage: %s", qPrintable(error));
            }
        }

        clearMap();
        m_dirty = true;
        return true;
    }
    else if (MsgRemoveEndpoints::match(cmd))
    {
        const MsgRemoveEndpoints& msg = (const MsgRemoveEndpoints&) cmd;
        m_engine.removeEndpoints(msg.getKeys(), msg.getOffline(), QDateTime::currentMSecsSinceEpoch());
        m_dirty = true;
        return true;
    }
    else if (MsgImportFile::match(cmd))
    {
        const MsgImportFile& msg = (const MsgImportFile&) cmd;
        QString error;

        if (importFile(msg.getPath(), error) < 0) {
            m_errors.append(error);
        }

        m_dirty = true;
        return true;
    }
    else if (MsgIngestRecords::match(cmd))
    {
        const MsgIngestRecords& msg = (const MsgIngestRecords&) cmd;

        for (MessageRecord record : msg.getRecords())
        {
            // From the API: a message received, never a page for a transmitter
            record.m_direction = MessageRecord::DirectionRX;
            record.m_protocolData.remove("transmitter");

            if (record.m_kind != MessageRecord::KindEvent) {
                m_engine.ingest(record, "api", now);
            }
        }

        m_dirty = true;
        return true;
    }

    return false;
}

// A record of a channel. The channels that announce themselves as transmitters are kept to give them
// their pages
void MessageCenter::ingest(const MessageRecord& record, const QObject *source)
{
    MessageEndpoint endpoint;
    QString event;

    if (MessageEndpoint::fromRecord(record, endpoint, event) && (endpoint.m_role == MessageEndpoint::Transmitter))
    {
        ChannelAPI *channel = qobject_cast<ChannelAPI*>(const_cast<QObject*>(source));

        if ((event == "unregister") || !channel) {
            m_transmitters.remove(endpoint.m_channel);
        } else {
            m_transmitters[endpoint.m_channel] = channel;
        }
    }

    m_engine.ingest(record, "channels", QDateTime::currentMSecsSinceEpoch());
    m_dirty = true;
}

void MessageCenter::tick()
{
    QMutexLocker mutexLocker(&m_mutex);
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList errors;

    if (m_engine.pollSources(errors) > 0) {
        m_dirty = true;
    }

    m_errors.append(errors);
    m_engine.releaseHeld(now);      // The messages the limits of the sinks held that may go now
    m_engine.expireClaims(now, (qint64) m_settings.m_claimTimeoutSec * 1000);
    m_engine.sweep(now, (qint64) m_settings.m_silentSec * 1000);

    // The pages whose transmitter is ready: its channel takes (claims) the page or refuses it
    for (const MessageCenterPage& page : m_engine.dispatch(now))
    {
        QString error;
        int code = postToTransmitter(page, error);

        if (code / 100 == 2) {
            m_engine.applyEvent(page.m_record.m_id, "claimed", QString(), now);
        } else {
            m_engine.applyEvent(page.m_record.m_id, "rejected", error, now);
        }

        m_dirty = true;
    }

    m_errors.append(m_engine.takeSinkErrors());

    if (m_engine.syncLoggedPages() > 0) {
        m_dirty = true;
    }

    if (m_dirty && getMessageQueueToGUI())
    {
        getMessageQueueToGUI()->push(MsgUpdated::create());
        m_dirty = false;
    }
}

// The "tx" action of the paging modulator, as the WebAPI posts it, called in process
int MessageCenter::postToTransmitter(const MessageCenterPage& page, QString& error)
{
    ChannelAPI *channel = m_transmitters.value(page.transmitter());

    if (!channel)
    {
        error = "The transmitter is not there";
        return 404;
    }

    SWGSDRangel::SWGChannelActions query;
    SWGSDRangel::SWGRadioPagingModActions *actions = new SWGSDRangel::SWGRadioPagingModActions();
    SWGSDRangel::SWGRadioPagingModTx *tx = new SWGSDRangel::SWGRadioPagingModTx();

    RadioPagingPage::fromRecord(page.m_record).toTx(*tx);
    actions->setTx(tx);
    query.setRadioPagingModActions(actions);
    QStringList keys = {"tx"};
    return channel->webapiActionsPost(keys, query, error);
}

QStringList MessageCenter::takeErrors()
{
    QMutexLocker mutexLocker(&m_mutex);
    QStringList errors = m_errors;
    m_errors.clear();
    return errors;
}

QByteArray MessageCenter::serialize() const
{
    return m_settings.serialize();
}

bool MessageCenter::deserialize(const QByteArray& data)
{
    bool ok = m_settings.deserialize(data);

    if (!ok) {
        m_settings.resetToDefaults();
    }

    m_inputMessageQueue.push(MsgConfigureMessageCenter::create(m_settings, QStringList(), true));
    return ok;
}

void MessageCenter::applySettings(const MessageCenterSettings& settings, const QStringList& settingsKeys, bool force)
{
    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (force || settingsKeys.contains("logSize") || settingsKeys.contains("routes") || settingsKeys.contains("sinks")
        || settingsKeys.contains("sources") || settingsKeys.contains("historyEnabled") || settingsKeys.contains("historyMaxMB")
        || settingsKeys.contains("historyDays")) {
        applyEngineSettings();
    }
    else if (settingsKeys.contains("contacts"))
    {
        // The address book alone: the sinks and sources stay open
        QMutexLocker mutexLocker(&m_mutex);
        m_engine.setContacts(m_settings.m_contacts);
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
}

// The settings that changed (all of them but the reverse API's when force is on) to the peer, as a PATCH of
// the settings of its message center
void MessageCenter::webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const MessageCenterSettings& settings, bool force)
{
    SWGSDRangel::SWGFeatureSettings *swgFeatureSettings = new SWGSDRangel::SWGFeatureSettings();
    swgFeatureSettings->setFeatureType(new QString("MessageCenter"));
    swgFeatureSettings->setMessageCenterSettings(new SWGSDRangel::SWGMessageCenterSettings());
    SWGSDRangel::SWGMessageCenterSettings *swg = swgFeatureSettings->getMessageCenterSettings();

    if (featureSettingsKeys.contains("title") || force) {
        swg->setTitle(new QString(settings.m_title));
    }
    if (featureSettingsKeys.contains("rgbColor") || force) {
        swg->setRgbColor(settings.m_rgbColor);
    }
    if (featureSettingsKeys.contains("logSize") || force) {
        swg->setLogSize(settings.m_logSize);
    }
    if (featureSettingsKeys.contains("historyEnabled") || force) {
        swg->setHistoryEnabled(settings.m_historyEnabled ? 1 : 0);
    }
    if (featureSettingsKeys.contains("historyMaxMB") || force) {
        swg->setHistoryMaxMb(settings.m_historyMaxMB);
    }
    if (featureSettingsKeys.contains("historyDays") || force) {
        swg->setHistoryDays(settings.m_historyDays);
    }
    if (featureSettingsKeys.contains("claimTimeoutSec") || force) {
        swg->setClaimTimeoutSec(settings.m_claimTimeoutSec);
    }
    if (featureSettingsKeys.contains("silentSec") || force) {
        swg->setSilentSec(settings.m_silentSec);
    }
    if (featureSettingsKeys.contains("routes") || force) {
        swg->setRoutes(new QString(QString::fromUtf8(MessageCenterSettings::routesToJson(settings.m_routes))));
    }
    if (featureSettingsKeys.contains("contacts") || force) {
        swg->setContacts(new QString(QString::fromUtf8(MessageCenterContact::listToJson(settings.m_contacts))));
    }
    if (featureSettingsKeys.contains("sinks") || force) {
        swg->setSinks(new QString(QString::fromUtf8(MessageCenterSettings::filesToJson(settings.m_sinks))));
    }
    if (featureSettingsKeys.contains("sources") || force) {
        swg->setSources(new QString(QString::fromUtf8(MessageCenterSettings::filesToJson(settings.m_sources))));
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

void MessageCenter::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "MessageCenter::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("MessageCenter::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

// The sinks, sources and routes of the settings in the engine
void MessageCenter::applyEngineSettings()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_engine.setLogSize(m_settings.m_logSize);
    applyHistory();

    for (const QString& name : m_engine.sinkNames())
    {
        if ((name != "log") && (name != "queue")) {
            m_engine.removeSink(name);
        }
    }
    for (const QString& name : m_engine.sourceNames()) {
        m_engine.removeSource(name);
    }

    for (const MessageCenterFileSetting& sink : m_settings.m_sinks)
    {
        std::unique_ptr<MessageCenterSink> made = makeSink(sink);

        if (made)
        {
            m_engine.addSink(std::move(made));
            MessageCenterEngine::RateLimit limit;
            limit.m_count = sink.m_rateCount;
            limit.m_periodMs = (qint64) sink.m_ratePeriod * 1000;
            limit.m_hold = sink.m_rateHold;
            m_engine.setRateLimit(sink.m_name, limit);
        }
        else if (!sink.m_name.isEmpty())
        {
            m_errors.append(sink.check(true));
        }
    }
    for (const MessageCenterFileSetting& source : m_settings.m_sources)
    {
        std::unique_ptr<MessageCenterSource> made = makeSource(source);

        if (made) {
            m_engine.addSource(std::move(made));
        } else if (!source.m_name.isEmpty()) {
            m_errors.append(source.check(false));
        }
    }

    m_engine.setRoutes(m_settings.m_routes);
    m_engine.setContacts(m_settings.m_contacts);
    m_dirty = true;
}

std::unique_ptr<MessageCenterSink> MessageCenter::makeSink(const MessageCenterFileSetting& setting)
{
    if (!setting.check(true).isEmpty()) {
        return nullptr;
    }

    const QString type = setting.typeOrDefault();

    if (type == "file")
    {
        return std::unique_ptr<MessageCenterSink>(new MessageCenterFileSink(setting.m_name, setting.m_path,
            (MessageCenterFileSink::Format) std::max(0, std::min((int) MessageCenterFileSink::PagerCsv, setting.m_format))));
    }
    if (type == "udp")
    {
        return std::unique_ptr<MessageCenterSink>(new MessageCenterUdpSink(setting.m_name, setting.m_address, (quint16) setting.m_port,
            (setting.m_format == 1) ? MessageCenterUdpSink::Json : MessageCenterUdpSink::PagerDemod));
    }
    if (type == "command") {
        return std::unique_ptr<MessageCenterSink>(new MessageCenterCommandSink(setting.m_name, setting.m_text,
            setting.m_commandEnv, setting.m_commandStdin));
    }

    // speech and map: done by the feature, in the thread of the message center
    return std::unique_ptr<MessageCenterSink>(new MessageCenterActionSink(setting.m_name, type, setting.m_text, setting.m_color,
        [this](const QString& actionType, const MessageRecord& record, const QString& text, quint32 color) {
            doAction(actionType, record, text, color);
        }));
}

std::unique_ptr<MessageCenterSource> MessageCenter::makeSource(const MessageCenterFileSetting& setting)
{
    if (!setting.check(false).isEmpty()) {
        return nullptr;
    }

    const QString type = setting.typeOrDefault();

    if (type == "csv") {
        return std::unique_ptr<MessageCenterSource>(new MessageCenterCsvSource(setting.m_name, setting.m_path, setting.m_format == 1));
    }
    if (type == "udp") {
        return std::unique_ptr<MessageCenterSource>(new MessageCenterUdpSource(setting.m_name, setting.m_address, (quint16) setting.m_port));
    }

    return std::unique_ptr<MessageCenterSource>(new MessageCenterFileSource(setting.m_name, setting.m_path));
}

// Called with the engine locked
void MessageCenter::doAction(const QString& type, const MessageRecord& record, const QString& text, quint32 color)
{
    (void) color;

    if (type == "speech")
    {
        // Said by the GUI: no GUI, nobody to say it
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgSpeak::create(text));
        }
    }
    else if (type == "map")
    {
        float latitude;
        float longitude;

        if (Units::stringToLatitudeAndLongitude(record.m_text, latitude, longitude, false)) {
            sendToMap(record, latitude, longitude);
        }
    }
}

void MessageCenter::sendToMap(const MessageRecord& record, float latitude, float longitude)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "mapitems", mapPipes);
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(record.m_timestamp);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(record.m_address));
        swgMapItem->setLatitude(latitude);
        swgMapItem->setLongitude(longitude);
        swgMapItem->setAltitude(0);
        swgMapItem->setAltitudeReference(1); // CLAMP_TO_GROUND
        swgMapItem->setFixedPosition(false);
        swgMapItem->setPositionDateTime(new QString(dateTime.toUTC().toString(Qt::ISODateWithMs)));
        swgMapItem->setImageRotation(0);
        swgMapItem->setText(new QString(record.m_text));
        swgMapItem->setImage(new QString("pager.png"));
        messageQueue->push(MainCore::MsgMapItem::create(this, swgMapItem));
    }

    m_mapItems.insert(record.m_address);
}

// The items of the messages leave the Map with the log
void MessageCenter::clearMap()
{
    for (const QString& address : m_mapItems)
    {
        QList<ObjectPipe*> mapPipes;
        MainCore::instance()->getMessagePipes().getMessagePipes(this, "mapitems", mapPipes);

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString(address));
            swgMapItem->setImage(new QString(""));
            messageQueue->push(MainCore::MsgMapItem::create(this, swgMapItem));
        }
    }

    m_mapItems.clear();
}

// A file read once into the log, from its start: JSON lines, or a CSV log (by its name). Returns the number of messages
int MessageCenter::importFile(const QString& path, QString& error)
{
    QFileInfo info(path);

    if (!info.isFile())
    {
        error = QString("Cannot import %1: not a file").arg(path);
        return -1;
    }

    const bool csv = info.suffix().compare("csv", Qt::CaseInsensitive) == 0;
    std::unique_ptr<MessageCenterSource> source = csv ? std::unique_ptr<MessageCenterSource>(new MessageCenterCsvSource("import", path))
        : std::unique_ptr<MessageCenterSource>(new MessageCenterFileSource("import", path));
    int count = 0;

    // The source reads a number of lines at a time: until it has nothing more
    do
    {
        std::vector<MessageRecord> records;
        QString pollError;

        if (!source->poll(records, pollError))
        {
            error = pollError;
            return -1;
        }

        for (const MessageRecord& record : records)
        {
            if (record.m_kind != MessageRecord::KindEvent)
            {
                m_engine.importRecord(record);
                count++;
            }
        }
    }
    while (source->more());

    return count;
}

// ---------------------------------------------------------------------------
// WebAPI
// ---------------------------------------------------------------------------

void MessageCenter::webapiFormatFeatureSettings(SWGSDRangel::SWGFeatureSettings& response, const MessageCenterSettings& settings)
{
    SWGSDRangel::SWGMessageCenterSettings *swg = response.getMessageCenterSettings();

    swg->setTitle(new QString(settings.m_title));
    swg->setRgbColor(settings.m_rgbColor);
    swg->setLogSize(settings.m_logSize);
    swg->setHistoryEnabled(settings.m_historyEnabled ? 1 : 0);
    swg->setHistoryMaxMb(settings.m_historyMaxMB);
    swg->setHistoryDays(settings.m_historyDays);
    swg->setClaimTimeoutSec(settings.m_claimTimeoutSec);
    swg->setSilentSec(settings.m_silentSec);
    swg->setRoutes(new QString(QString::fromUtf8(MessageCenterSettings::routesToJson(settings.m_routes))));
    swg->setContacts(new QString(QString::fromUtf8(MessageCenterContact::listToJson(settings.m_contacts))));
    swg->setSinks(new QString(QString::fromUtf8(MessageCenterSettings::filesToJson(settings.m_sinks))));
    swg->setSources(new QString(QString::fromUtf8(MessageCenterSettings::filesToJson(settings.m_sources))));
    swg->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (swg->getReverseApiAddress()) {
        *swg->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        swg->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    swg->setReverseApiPort(settings.m_reverseAPIPort);
    swg->setReverseApiFeatureSetIndex(settings.m_reverseAPIFeatureSetIndex);
    swg->setReverseApiFeatureIndex(settings.m_reverseAPIFeatureIndex);
}

void MessageCenter::webapiUpdateFeatureSettings(MessageCenterSettings& settings, const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response)
{
    SWGSDRangel::SWGMessageCenterSettings *swg = response.getMessageCenterSettings();

    if (featureSettingsKeys.contains("title") && swg->getTitle()) {
        settings.m_title = *swg->getTitle();
    }
    if (featureSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = (quint32) swg->getRgbColor();
    }
    if (featureSettingsKeys.contains("logSize")) {
        settings.m_logSize = std::max(MessageCenterEngine::m_minLogSize, std::min(MessageCenterEngine::m_maxLogSize, swg->getLogSize()));
    }
    if (featureSettingsKeys.contains("historyEnabled")) {
        settings.m_historyEnabled = swg->getHistoryEnabled() != 0;
    }
    if (featureSettingsKeys.contains("historyMaxMB")) {
        settings.m_historyMaxMB = std::max(1, std::min(10000, swg->getHistoryMaxMb()));
    }
    if (featureSettingsKeys.contains("historyDays")) {
        settings.m_historyDays = std::max(1, std::min(3650, swg->getHistoryDays()));
    }
    if (featureSettingsKeys.contains("claimTimeoutSec")) {
        settings.m_claimTimeoutSec = std::max(1, std::min(3600, swg->getClaimTimeoutSec()));
    }
    if (featureSettingsKeys.contains("silentSec")) {
        settings.m_silentSec = std::max(6, std::min(3600, swg->getSilentSec()));
    }
    if (featureSettingsKeys.contains("routes") && swg->getRoutes()) {
        settings.m_routes = MessageCenterSettings::routesFromJson(swg->getRoutes()->toUtf8());
    }
    if (featureSettingsKeys.contains("contacts") && swg->getContacts()) {
        settings.m_contacts = MessageCenterContact::listFromJson(swg->getContacts()->toUtf8());
    }
    if (featureSettingsKeys.contains("sinks") && swg->getSinks()) {
        settings.m_sinks = MessageCenterSettings::filesFromJson(swg->getSinks()->toUtf8());
    }
    if (featureSettingsKeys.contains("sources") && swg->getSources()) {
        settings.m_sources = MessageCenterSettings::filesFromJson(swg->getSources()->toUtf8());
    }

    // Rules of before (sends to mark sinks) as rules of now
    MessageCenterSettings::upgradeRoutes(settings.m_routes, settings.m_sinks);
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
}

int MessageCenter::webapiSettingsGet(SWGSDRangel::SWGFeatureSettings& response, QString& errorMessage)
{
    (void) errorMessage;
    QMutexLocker mutexLocker(&m_mutex);
    response.setMessageCenterSettings(new SWGSDRangel::SWGMessageCenterSettings());
    response.getMessageCenterSettings()->init();
    webapiFormatFeatureSettings(response, m_settings);
    return 200;
}

// What is wrong with the sinks, the sources and the routes of the settings, empty if nothing
QString MessageCenter::checkRouting(const MessageCenterSettings& settings)
{
    QStringList sinkNames = {"log", "queue"};
    QStringList sourceNames;

    for (const MessageCenterFileSetting& sink : settings.m_sinks)
    {
        QString error = sink.check(true);

        if (!error.isEmpty()) {
            return error;
        }
        if (sinkNames.contains(sink.m_name)) {
            return QString("Two sinks are named %1").arg(sink.m_name);
        }

        sinkNames.append(sink.m_name);
    }
    for (const MessageCenterFileSetting& source : settings.m_sources)
    {
        QString error = source.check(false);

        if (!error.isEmpty()) {
            return error;
        }
        if (sourceNames.contains(source.m_name)) {
            return QString("Two sources are named %1").arg(source.m_name);
        }

        sourceNames.append(source.m_name);
    }
    for (const MessageCenterEngine::Route& route : settings.m_routes)
    {
        // A send to a sink of the settings (log and queue are the log and page actions)
        if ((route.m_action == MessageCenterEngine::Route::ActionSend) && !route.m_target.isEmpty()
            && (!sinkNames.contains(route.m_target) || (route.m_target == "log") || (route.m_target == "queue"))) {
            return QString("Rule %1: there is no sink %2").arg(route.m_name).arg(route.m_target);
        }

        // A page to a contact of the address book
        if ((route.m_action == MessageCenterEngine::Route::ActionPage) && (route.m_pageTo == MessageCenterEngine::Route::PageContact)
            && !route.m_target.isEmpty()
            && std::none_of(settings.m_contacts.begin(), settings.m_contacts.end(),
                [&route](const MessageCenterContact& c) { return c.m_name == route.m_target; })) {
            return QString("Rule %1: there is no contact %2").arg(route.m_name).arg(route.m_target);
        }

        QString error = route.check();

        if (!error.isEmpty()) {
            return error;
        }

        // The codepage a rule decodes with: one of a protocol
        if (!route.m_decodeCharset.empty())
        {
            bool known = false;

            for (const char *protocol : {"pocsag", "flex", "gsc"}) {
                known = known || (RadioPagingCharset::find(protocol, route.m_decodeCharset) != nullptr);
            }
            if (!known) {
                return QString("Rule %1: there is no codepage %2").arg(route.m_name).arg(RadioPagingPage::charsetText(route.m_decodeCharset));
            }
        }
        if (!route.m_decodeType.isEmpty() && (route.m_decodeType != "alphanumeric") && (route.m_decodeType != "numeric")) {
            return QString("Rule %1: decodeType is alphanumeric or numeric").arg(route.m_name);
        }

        // A source a rule names: channels, api or a source of the settings
        for (const QString& source : route.m_filter.m_sources)
        {
            if ((source != "channels") && (source != "api") && !sourceNames.contains(source)) {
                return QString("Rule %1: there is no source %2").arg(route.m_name).arg(source);
            }
        }
    }

    return QString();
}

int MessageCenter::webapiSettingsPutPatch(bool force, const QStringList& featureSettingsKeys,
    SWGSDRangel::SWGFeatureSettings& response, QString& errorMessage)
{
    SWGSDRangel::SWGMessageCenterSettings *swg = response.getMessageCenterSettings();

    // The lists are JSON texts: refused if they are not
    for (const char *key : {"routes", "sinks", "sources"})
    {
        QString *text = (QString(key) == "routes") ? swg->getRoutes() : (QString(key) == "sinks") ? swg->getSinks() : swg->getSources();

        if (featureSettingsKeys.contains(key) && text && !QJsonDocument::fromJson(text->toUtf8()).isArray())
        {
            errorMessage = QString("%1 must be a JSON array").arg(key);
            return 400;
        }
    }

    // The contacts: a name each, addresses that are addresses
    if (featureSettingsKeys.contains("contacts") && swg->getContacts())
    {
        QString error;
        MessageCenterContact::listFromJson(swg->getContacts()->toUtf8(), &error);

        if (!error.isEmpty())
        {
            errorMessage = error;
            return 400;
        }
    }

    MessageCenterSettings settings;
    {
        QMutexLocker mutexLocker(&m_mutex);
        settings = m_settings;
    }

    webapiUpdateFeatureSettings(settings, featureSettingsKeys, response);

    // What is wrong with the sinks, the sources and the routes is said now, not later in the errors of the report
    if (featureSettingsKeys.contains("sinks") || featureSettingsKeys.contains("sources") || featureSettingsKeys.contains("routes"))
    {
        errorMessage = checkRouting(settings);

        if (!errorMessage.isEmpty()) {
            return 400;
        }
    }
    m_inputMessageQueue.push(MsgConfigureMessageCenter::create(settings, featureSettingsKeys, force));

    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(MsgConfigureMessageCenter::create(settings, featureSettingsKeys, force));
    }

    webapiFormatFeatureSettings(response, settings);
    return 200;
}

int MessageCenter::webapiReportGet(SWGSDRangel::SWGFeatureReport& response, QString& errorMessage)
{
    (void) errorMessage;
    static const char *roles[] = {"rx", "tx"};
    static const char *statuses[] = {"offline", "idle", "busy"};
    QMutexLocker mutexLocker(&m_mutex);
    response.setMessageCenterReport(new SWGSDRangel::SWGMessageCenterReport());
    response.getMessageCenterReport()->init();
    SWGSDRangel::SWGMessageCenterReport *swg = response.getMessageCenterReport();

    QJsonArray endpoints;

    for (const MessageEndpoint& e : m_engine.endpoints())
    {
        QJsonObject o;
        o["channel"] = e.m_channel;
        o["role"] = roles[(e.m_role == MessageEndpoint::Transmitter) ? 1 : 0];
        o["status"] = statuses[(int) e.m_status];
        o["protocols"] = QJsonArray::fromStringList(e.m_protocols);
        QJsonArray speeds, polarities;

        for (int s : e.m_speeds) {
            speeds.append(s);
        }
        for (int p : e.m_polarities) {
            polarities.append(p);
        }

        o["speeds"] = speeds;
        o["polarities"] = polarities;
        o["queueDepth"] = e.m_queueDepth;
        o["capacity"] = e.m_capacity;
        o["frequency"] = (double) e.m_frequency;
        o["lastSeen"] = (double) e.m_lastSeen;
        endpoints.append(o);
    }

    QJsonArray pages;

    for (const MessageCenterPage& p : m_engine.pages())
    {
        QJsonObject o;
        o["id"] = p.m_record.m_id;
        o["transmitter"] = p.transmitter();
        o["protocol"] = p.m_record.m_protocol;
        o["address"] = p.m_record.m_address;
        o["text"] = p.m_record.m_text;
        o["state"] = MessageCenterPage::stateName(p.m_state);
        o["attempts"] = p.m_attempts;
        o["retransmits"] = p.m_retransmits;
        o["reason"] = p.m_reason;
        o["claimedBy"] = p.m_claimedBy;
        pages.append(o);
    }

    QJsonArray log;
    const std::deque<MessageRecord>& records = m_engine.log();
    size_t first = (records.size() > 50) ? records.size() - 50 : 0;

    for (size_t i = first; i < records.size(); i++) {
        log.append(records[i].toJson());
    }

    QJsonArray sinks;

    for (auto it = m_engine.sinkStats().begin(); it != m_engine.sinkStats().end(); ++it)
    {
        QJsonObject o;
        o["name"] = it.key();
        o["accepted"] = it.value().m_accepted;
        o["failed"] = it.value().m_failed;
        o["dropped"] = it.value().m_dropped;
        o["waiting"] = it.value().m_waiting;
        sinks.append(o);
    }

    swg->setLogged(m_engine.logged());
    swg->setRouted(m_engine.routed());
    swg->setEndpoints(new QString(QString::fromUtf8(QJsonDocument(endpoints).toJson(QJsonDocument::Compact))));
    swg->setPages(new QString(QString::fromUtf8(QJsonDocument(pages).toJson(QJsonDocument::Compact))));
    swg->setLog(new QString(QString::fromUtf8(QJsonDocument(log).toJson(QJsonDocument::Compact))));
    swg->setSinks(new QString(QString::fromUtf8(QJsonDocument(sinks).toJson(QJsonDocument::Compact))));
    swg->setErrors(new QString(takeErrors().join('\n')));
    swg->setQuery(new QString(QString::fromUtf8(m_lastQuery)));
    return 200;
}

int MessageCenter::webapiActionsPost(const QStringList& featureActionsKeys, SWGSDRangel::SWGFeatureActions& query, QString& errorMessage)
{
    SWGSDRangel::SWGMessageCenterActions *swg = query.getMessageCenterActions();

    if (!swg)
    {
        errorMessage = "Missing MessageCenterActions in query";
        return 400;
    }

    int actions = (featureActionsKeys.contains("post") ? 1 : 0) + (featureActionsKeys.contains("requeue") ? 1 : 0)
        + (featureActionsKeys.contains("cancel") ? 1 : 0) + (featureActionsKeys.contains("clearSent") ? 1 : 0)
        + (featureActionsKeys.contains("redecode") ? 1 : 0) + (featureActionsKeys.contains("ingest") ? 1 : 0)
        + (featureActionsKeys.contains("importFile") ? 1 : 0) + (featureActionsKeys.contains("query") ? 1 : 0);

    if (actions != 1)
    {
        errorMessage = "One action expected: post, requeue, cancel, clearSent, redecode, ingest, importFile or query";
        return 400;
    }

    // A query of the messages, over the history (or the log): made now, its result in the report (query), with the query
    if (featureActionsKeys.contains("query"))
    {
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(swg->getQuery() ? swg->getQuery()->toUtf8() : QByteArray(), &parseError);

        if (!doc.isObject())
        {
            errorMessage = QString("query: %1").arg(parseError.error != QJsonParseError::NoError ? parseError.errorString() : QString("a JSON object is expected"));
            return 400;
        }

        const QJsonObject q = doc.object();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 from = q.contains("from") ? (qint64) q.value("from").toDouble(-1) : -1;
        qint64 to = q.contains("to") ? (qint64) q.value("to").toDouble(-1) : -1;

        if (q.contains("last"))
        {
            from = now - (qint64) (q.value("last").toDouble(0) * 1000.0);
            to = now;
        }

        const MessageCenterFilter filter = MessageCenterFilter::fromJson(q.value("filter").toObject());
        const QString filterErrors = filter.errors();

        if (!filterErrors.isEmpty())
        {
            errorMessage = QString("query: %1").arg(filterErrors);
            return 400;
        }

        const int offset = std::max(0, q.value("offset").toInt(0));
        const int limit = std::max(1, std::min(m_maxQueryLimit, q.value("limit").toInt(100)));
        const MessageCenterArchiveResult result = queryMessages(from, to, filter, offset, limit);
        QJsonObject echo = q;
        echo["from"] = (double) from;
        echo["to"] = (double) to;
        echo["offset"] = offset;
        echo["limit"] = limit;
        QJsonArray records;

        for (const MessageRecord& record : result.m_records) {
            records.append(record.toJson());
        }

        QJsonObject out;
        out["query"] = echo;
        out["total"] = result.m_total;
        out["records"] = records;
        QMutexLocker mutexLocker(&m_mutex);
        m_lastQuery = QJsonDocument(out).toJson(QJsonDocument::Compact);
        return 202;
    }

    // Message records from the API: a message received, as if a channel had published it
    if (featureActionsKeys.contains("ingest"))
    {
        std::vector<MessageRecord> records;
        int lines = 0;

        for (const QByteArray& line : swg->getIngest() ? swg->getIngest()->toUtf8().split('\n') : QList<QByteArray>())
        {
            if (line.trimmed().isEmpty()) {
                continue;
            }

            QJsonDocument document = QJsonDocument::fromJson(line.trimmed());
            MessageRecord record;
            lines++;

            if (!document.isObject())
            {
                errorMessage = QString("Line %1 is not a JSON object").arg(lines);
                return 400;
            }

            // A message from outside (a script, an agent) needs only its protocol: the time it is given and an id
            // of its own when it has none
            QJsonObject json = document.object();

            if (!json.contains("protocol"))
            {
                errorMessage = QString("Line %1: protocol missing (pocsag, flex or gsc)").arg(lines);
                return 400;
            }
            if (!json.contains("timestamp")) {
                json["timestamp"] = QDateTime::currentMSecsSinceEpoch();
            }
            if (!json.contains("id")) {
                json["id"] = QString("api-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(lines);
            }
            if (!MessageRecord::fromJson(json, record))
            {
                errorMessage = QString("Line %1 is not a message record").arg(lines);
                return 400;
            }
            if (record.m_kind == MessageRecord::KindEvent)
            {
                errorMessage = QString("Line %1: an event is not a message").arg(lines);
                return 400;
            }

            records.push_back(record);
        }

        if (records.empty())
        {
            errorMessage = "No message record";
            return 400;
        }

        m_inputMessageQueue.push(MsgIngestRecords::create(records));
        return 202;
    }
    if (featureActionsKeys.contains("importFile"))
    {
        QString path = swg->getImportFile() ? swg->getImportFile()->trimmed() : QString();

        if (!QFileInfo(path).isFile())
        {
            errorMessage = QString("%1 is not a file").arg(path);
            return 400;
        }

        m_inputMessageQueue.push(MsgImportFile::create(path));
        return 202;
    }

    // A received message of the log decoded again: checked here, the log changes in the thread of the feature
    if (featureActionsKeys.contains("redecode"))
    {
        SWGSDRangel::SWGMessageCenterRedecode *redecode = swg->getRedecode();
        const QString id = (redecode && redecode->getId()) ? *redecode->getId() : QString();
        const QString type = (redecode && redecode->getType()) ? redecode->getType()->toLower() : QString();
        const MessageRecord *record = m_engine.find(id);
        MessageRecord out;

        if (!redecode)
        {
            errorMessage = "No redecode in the request";
            return 400;
        }
        if (!record)
        {
            errorMessage = QString("No record %1 in the log").arg(id);
            return 404;
        }
        if (!RadioPagingRecode::redecode(*record, RadioPagingPage::charsetKey(redecode->getCharset()), type, out, errorMessage)) {
            return 400;
        }

        m_inputMessageQueue.push(MsgRedecoded::create(out));
        return 202;
    }

    if (featureActionsKeys.contains("post"))
    {
        SWGSDRangel::SWGMessageCenterPost *post = swg->getPost();

        if (!post)
        {
            errorMessage = "Missing post";
            return 400;
        }

        auto text = [](QString *s) { return s ? *s : QString(); };

        // To a contact: the fields of its send to for this page (transmitter, address...) set on it
        if (!text(post->getContact()).trimmed().isEmpty())
        {
            const QString name = text(post->getContact()).trimmed();
            MessageCenterContact contact;
            bool found = false;
            {
                QMutexLocker mutexLocker(&m_mutex);

                for (const MessageCenterContact& c : m_settings.m_contacts)
                {
                    if (c.m_name == name)
                    {
                        contact = c;
                        found = true;
                    }
                }
            }

            if (!found)
            {
                errorMessage = QString("No contact %1").arg(name);
                return 400;
            }

            QJsonObject *message = post->asJsonObject();
            QJsonObject sent;
            const bool sendable = contact.pageFor(*message, sent, &errorMessage);
            delete message;

            if (!sendable) {
                return 400;
            }

            post->fromJsonObject(sent);
        }

        QString polarity = text(post->getPolarity()).toLower();
        RadioPagingPage page;
        page.m_id = text(post->getId());
        page.m_transmitter = text(post->getTransmitter());
        page.m_protocol = text(post->getProtocol()).toLower();
        page.m_type = text(post->getType()).toLower();
        page.m_address = text(post->getAddress()).trimmed();
        page.m_function = post->getFunction();
        page.m_priority = post->getPriority() != 0;
        page.m_groupMembers = text(post->getGroupMembers());
        page.m_text = text(post->getText());
        page.m_voiceFile = text(post->getVoiceFile());
        page.m_speed = post->getSpeed();
        page.m_polarity = polarity.isEmpty() ? -1 : (polarity == "normal") ? 0 : (polarity == "inverted") ? 1 : -2;
        page.m_shortType = post->getShortType();
        page.m_source = post->getSource();
        page.m_blocking = (post->getBlocking() == 0) ? 1 : post->getBlocking();
        page.m_mailDrop = post->getMailDrop() != 0;
        page.m_rightToLeft = post->getRightToLeft() != 0;
        page.m_headerMessage = post->getHeaderMessage() != 0;
        page.m_statusInfo = post->getStatusInfo() != 0;
        page.m_secureType = post->getSecureType();
        page.m_retransmit = post->getRetransmit();
        // 0 (not given) or negative: the default of the transmitter
        page.m_retransmitInterval = (post->getRetransmitInterval() > 0.0f) ? post->getRetransmitInterval() : -1.0f;
        page.m_sendDelay = post->getSendDelay();
        page.m_charset = RadioPagingPage::charsetKey(post->getCharset());

        if (page.m_transmitter.isEmpty())
        {
            errorMessage = "A page needs a transmitter (the name of its channel)";
            return 400;
        }

        // The limits of the page here; the transmitter checks the rest (its mode, numbering, the encoder)
        errorMessage = page.check();

        if (!errorMessage.isEmpty()) {
            return 400;
        }

        m_inputMessageQueue.push(MsgPostPage::create(page.toRecord()));
        return 202;
    }
    if (featureActionsKeys.contains("requeue") && swg->getRequeue())
    {
        m_inputMessageQueue.push(MsgPageAction::create(*swg->getRequeue(), MsgPageAction::Requeue));
        return 202;
    }
    if (featureActionsKeys.contains("cancel") && swg->getCancel())
    {
        m_inputMessageQueue.push(MsgPageAction::create(*swg->getCancel(), MsgPageAction::Cancel));
        return 202;
    }
    if (featureActionsKeys.contains("clearSent"))
    {
        m_inputMessageQueue.push(MsgPageAction::create(QString(), MsgPageAction::ClearSent));
        return 202;
    }

    errorMessage = "Missing the id of the page";
    return 400;
}
