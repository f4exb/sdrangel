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

#include <QCryptographicHash>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <algorithm>

#include "maincore.h"
#include "channel/channelapi.h"
#include "device/deviceapi.h"
#include "feature/feature.h"

#include "mcpdatafeed.h"
#include "mcperror.h"
#include "mcpstreams.h"
#include "mcptools.h"

// ---------------------------------------------------------------------------
// MCPEventStream
// ---------------------------------------------------------------------------

MCPEventStream::MCPEventStream(const QString& sessionId) :
    m_sessionId(sessionId),
    m_closed(false)
{
    m_age.start();
}

qint64 MCPEventStream::ageMs() const
{
    return m_age.elapsed();
}

void MCPEventStream::push(const QByteArray& event)
{
    QMutexLocker locker(&m_mutex);

    if (m_closed) {
        return;
    }

    m_events.enqueue(event);

    while (m_events.size() > m_maxQueued) {
        m_events.dequeue();
    }

    m_wait.wakeAll();
}

QByteArray MCPEventStream::waitForEvent(int timeoutMs)
{
    QMutexLocker locker(&m_mutex);

    if (m_events.isEmpty() && !m_closed) {
        m_wait.wait(&m_mutex, timeoutMs);
    }

    if (m_events.isEmpty()) {
        return QByteArray();
    }

    return m_events.dequeue();
}

void MCPEventStream::close()
{
    QMutexLocker locker(&m_mutex);
    m_closed = true;
    m_wait.wakeAll();
}

bool MCPEventStream::isClosed() const
{
    QMutexLocker locker(&m_mutex);
    return m_closed;
}

// ---------------------------------------------------------------------------
// MCPStreams
// ---------------------------------------------------------------------------

MCPStreams::MCPStreams() :
    m_notificationsSent(0)
{
}

MCPStreams::~MCPStreams()
{
    closeAll();
}

QString MCPStreams::createSession()
{
    QMutexLocker locker(&m_mutex);
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_sessions.insert(id, QDateTime::currentDateTimeUtc());
    expireSessions();
    return id;
}

bool MCPStreams::sessionExists(const QString& sessionId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_sessions.contains(sessionId)) {
        return false;
    }

    m_sessions[sessionId] = QDateTime::currentDateTimeUtc();
    return true;
}

// Clients are not obliged to end their session, and a bridge that reconnects starts a new one
// every time, so idle sessions are dropped rather than kept for the life of the process.
// Sessions with an open stream are never dropped. Call with the mutex held.
void MCPStreams::expireSessions()
{
    QSet<QString> streaming;

    for (const StreamPtr& stream : m_streams) {
        streaming.insert(stream->getSessionId());
    }

    QDateTime now = QDateTime::currentDateTimeUtc();
    QList<QString> candidates;

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it)
    {
        if (!streaming.contains(it.key())) {
            candidates.append(it.key());
        }
    }

    // Oldest first, so that trimming to the cap drops the least recently used
    std::sort(candidates.begin(), candidates.end(), [this](const QString& a, const QString& b) {
        return m_sessions[a] < m_sessions[b];
    });

    int droppable = candidates.size() - qMax(0, m_maxSessions - (int) streaming.size());

    for (int i = 0; i < candidates.size(); i++)
    {
        const QString& id = candidates[i];
        bool tooOld = m_sessions[id].msecsTo(now) > m_sessionIdleMs;
        bool overCap = i < droppable;

        if (tooOld || overCap)
        {
            m_sessions.remove(id);
            m_subscriptions.remove(id);
        }
    }
}

void MCPStreams::endSession(const QString& sessionId)
{
    QList<StreamPtr> toClose;
    {
        QMutexLocker locker(&m_mutex);
        m_sessions.remove(sessionId);
        m_subscriptions.remove(sessionId);

        for (const StreamPtr& stream : m_streams)
        {
            if (stream->getSessionId() == sessionId) {
                toClose.append(stream);
            }
        }
    }

    // Closing wakes the HTTP thread serving the stream, which then removes it. Holding a
    // shared pointer keeps each one alive until this call is done with it.
    for (const StreamPtr& stream : toClose) {
        stream->close();
    }
}

QSet<QString> MCPStreams::subscribedUris() const
{
    QMutexLocker locker(&m_mutex);
    QSet<QString> uris;

    for (const QSet<QString>& perSession : m_subscriptions) {
        uris.unite(perSession);
    }

    return uris;
}

bool MCPStreams::subscribe(const QString& sessionId, const QString& uri)
{
    QMutexLocker locker(&m_mutex);
    QSet<QString>& uris = m_subscriptions[sessionId];

    // Bounded: every subscription is polled for changes each second, and the anonymous
    // session's set has no DELETE to clear it
    if (!uris.contains(uri) && (uris.size() >= m_maxSubscriptions)) {
        return false;
    }

    uris.insert(uri);
    return true;
}

bool MCPStreams::unsubscribe(const QString& sessionId, const QString& uri)
{
    QMutexLocker locker(&m_mutex);

    if (!m_subscriptions.contains(sessionId)) {
        return false;
    }

    return m_subscriptions[sessionId].remove(uri);
}

QSet<QString> MCPStreams::subscriptions(const QString& sessionId) const
{
    QMutexLocker locker(&m_mutex);
    return m_subscriptions.value(sessionId);
}

MCPStreams::StreamPtr MCPStreams::open(const QString& sessionId, QString& error)
{
    QList<StreamPtr> superseded;
    QMutexLocker locker(&m_mutex);

    // A server message must reach a client on one stream only, so a session keeps a single
    // stream: opening another replaces the one before it. Clients that send no session id
    // already share the anonymous subscriptions and cannot be told apart, so they share one
    // stream on the same rule rather than each receiving every notification.
    for (int i = m_streams.size() - 1; i >= 0; i--)
    {
        if (m_streams[i]->getSessionId() == sessionId)
        {
            superseded.append(m_streams[i]);
            m_streams.removeAt(i);
        }
    }

    if (m_streams.size() >= m_maxStreams)
    {
        locker.unlock();

        for (const StreamPtr& stream : superseded) {
            stream->close();
        }

        error = QString("Too many open event streams (%1)").arg(m_maxStreams);
        return StreamPtr();
    }

    StreamPtr stream(new MCPEventStream(sessionId));
    m_streams.append(stream);
    qDebug("MCPStreams::open: stream opened, %d open, %d superseded", (int) m_streams.size(), (int) superseded.size());
    locker.unlock();

    // Wakes the HTTP thread serving each replaced stream so that it finishes its response
    for (const StreamPtr& older : superseded) {
        older->close();
    }

    return stream;
}

// The stream is only destroyed once every holder has released its shared pointer, so a
// notification in flight on another thread cannot be left pointing at freed memory.
void MCPStreams::close(const StreamPtr& stream)
{
    if (stream.isNull()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_streams.removeAll(stream);
    qDebug("MCPStreams::close: stream closed, %d open", (int) m_streams.size());
}

void MCPStreams::closeAll()
{
    QList<StreamPtr> streams;
    {
        QMutexLocker locker(&m_mutex);
        streams = m_streams;
    }

    // Only ask them to stop: the HTTP thread serving each stream removes it when it exits
    for (const StreamPtr& stream : streams) {
        stream->close();
    }
}

QByteArray MCPStreams::resourceUpdatedEvent(const QString& uri)
{
    QJsonObject params;
    params["uri"] = uri;
    QJsonObject notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "notifications/resources/updated";
    notification["params"] = params;
    return QJsonDocument(notification).toJson(QJsonDocument::Compact);
}

void MCPStreams::notifyResourceUpdated(const QString& uri)
{
    QList<StreamPtr> targets;
    {
        QMutexLocker locker(&m_mutex);

        for (const StreamPtr& stream : m_streams)
        {
            if (m_subscriptions.value(stream->getSessionId()).contains(uri)) {
                targets.append(stream);
            }
        }

        m_notificationsSent += targets.size();
    }

    if (targets.isEmpty()) {
        return;
    }

    QByteArray event = resourceUpdatedEvent(uri);

    for (const StreamPtr& stream : targets) {
        stream->push(event);
    }
}

void MCPStreams::notifyResourcesMatching(const QString& prefix)
{
    QSet<QString> uris;
    {
        QMutexLocker locker(&m_mutex);

        for (const StreamPtr& stream : m_streams)
        {
            for (const QString& uri : m_subscriptions.value(stream->getSessionId()))
            {
                if (uri.startsWith(prefix)) {
                    uris.insert(uri);
                }
            }
        }
    }

    for (const QString& uri : uris) {
        notifyResourceUpdated(uri);
    }
}

int MCPStreams::streamCount() const
{
    QMutexLocker locker(&m_mutex);
    return (int) m_streams.size();
}

qint64 MCPStreams::getNotificationsSent() const
{
    QMutexLocker locker(&m_mutex);
    return m_notificationsSent;
}

QJsonObject MCPStreams::status() const
{
    QMutexLocker locker(&m_mutex);
    QJsonObject result;
    result["openStreams"] = (int) m_streams.size();
    result["maxStreams"] = m_maxStreams;
    result["sessions"] = (int) m_sessions.size();
    result["notificationsSent"] = (double) m_notificationsSent;
    QJsonArray subscribed;

    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it)
    {
        for (const QString& uri : it.value())
        {
            QJsonObject entry;
            entry["session"] = it.key().isEmpty() ? QString("(anonymous)") : it.key();
            entry["uri"] = uri;
            subscribed.append(entry);
        }
    }

    result["subscriptions"] = subscribed;
    return result;
}

// ---------------------------------------------------------------------------
// MCPNotifier
// ---------------------------------------------------------------------------

MCPNotifier::MCPNotifier(MCPStreams *streams, MCPDataFeed *dataFeed, MCPTools *tools, QObject *parent) :
    QObject(parent),
    m_streams(streams),
    m_tools(tools),
    m_pendingDeviceSets(false)
{
    MainCore *mainCore = MainCore::instance();
    connect(mainCore, &MainCore::deviceSetAdded, this, &MCPNotifier::onDeviceSetAdded);
    connect(mainCore, &MainCore::deviceStateChanged, this, &MCPNotifier::onDeviceStateChanged);
    connect(mainCore, &MainCore::deviceSetRemoved, this, &MCPNotifier::onDeviceSetRemoved);
    connect(mainCore, &MainCore::deviceChanged, this, &MCPNotifier::onDeviceChanged);
    connect(mainCore, &MainCore::channelAdded, this, &MCPNotifier::onChannelAdded);
    connect(mainCore, &MainCore::channelRemoved, this, &MCPNotifier::onChannelRemoved);
    connect(mainCore, &MainCore::featureAdded, this, &MCPNotifier::onFeatureAdded);
    connect(mainCore, &MainCore::featureRemoved, this, &MCPNotifier::onFeatureRemoved);

    if (dataFeed)
    {
        connect(dataFeed, &MCPDataFeed::packetsChanged, this, &MCPNotifier::onPacketsChanged);
        connect(dataFeed, &MCPDataFeed::mapItemsChanged, this, &MCPNotifier::onMapItemsChanged);
    }

    // Changes are collected and sent at most this often, so that a busy decoder does not
    // turn every decoded packet into a notification
    m_timer.setInterval(m_coalesceMs);
    connect(&m_timer, &QTimer::timeout, this, &MCPNotifier::flush);
    m_timer.start();
}

void MCPNotifier::markChanged(const QString& uri)
{
    QMutexLocker locker(&m_mutex);
    m_pending.insert(uri);
}

void MCPNotifier::markStructuralChange()
{
    QMutexLocker locker(&m_mutex);
    m_pending.insert("sdrangel://instance");
    m_pendingDeviceSets = true;
}

void MCPNotifier::onDeviceSetAdded(int index, DeviceAPI *device)
{
    (void) index;
    (void) device;
    markStructuralChange();
}

void MCPNotifier::onDeviceSetRemoved(int index)
{
    (void) index;
    markStructuralChange();
}

void MCPNotifier::onDeviceStateChanged(int index, DeviceAPI *device)
{
    (void) index;
    (void) device;
    markStructuralChange();
}

void MCPNotifier::onDeviceChanged(int index)
{
    (void) index;
    markStructuralChange();
}

void MCPNotifier::onChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    (void) deviceSetIndex;
    (void) channel;
    markStructuralChange();
}

void MCPNotifier::onChannelRemoved(int deviceSetIndex, ChannelAPI *channel)
{
    (void) deviceSetIndex;
    (void) channel;
    markStructuralChange();
}

void MCPNotifier::onFeatureAdded(int featureSetIndex, Feature *feature)
{
    (void) featureSetIndex;
    (void) feature;
    markStructuralChange();
}

void MCPNotifier::onFeatureRemoved(int featureSetIndex, Feature *feature)
{
    (void) featureSetIndex;
    (void) feature;
    markStructuralChange();
}

void MCPNotifier::onPacketsChanged()
{
    markChanged("sdrangel://packets");
}

void MCPNotifier::onMapItemsChanged()
{
    markChanged("sdrangel://map/items");
}

// MainCore announces device sets, channels and features appearing and disappearing, but nothing
// at all when a setting changes, so a title or a frequency edited in the GUI or over the API would
// leave a subscriber holding a stale resource. The rendered content is compared instead, for the
// subscribed resources only, so a session that asked for nothing costs nothing
void MCPNotifier::pollForContentChanges()
{
    if (!m_tools) {
        return;
    }

    QSet<QString> uris = m_streams->subscribedUris();

    for (QMap<QString, QByteArray>::iterator digest = m_digests.begin(); digest != m_digests.end(); )
    {
        if (uris.contains(digest.key())) {
            ++digest;
        } else {
            digest = m_digests.erase(digest);
        }
    }

    for (const QString& uri : uris)
    {
        QJsonObject content;
        int index = -1;

        if (uri.startsWith("sdrangel://deviceset/"))
        {
            bool ok = false;
            index = uri.mid(QString("sdrangel://deviceset/").size()).toInt(&ok);

            // A device set that is not there is not read: the read would only throw, once a
            // second for as long as the subscription lasts
            if (!ok || (index < 0) || (index >= m_tools->deviceSetCount())) {
                continue;
            }
        }
        else if (uri != "sdrangel://instance")
        {
            continue; // the others have their own signals
        }

        // Reading a resource throws when it is not there any more, and this runs in a timer slot,
        // where an exception would escape into the event loop and take the application down with
        // it. A device set that has just gone is reported by the structural signal anyway
        try {
            content = (index < 0) ? m_tools->getInstanceSummary() : m_tools->getDeviceSet(index);
        } catch (const MCPToolError&) {
            continue;
        } catch (const std::exception&) {
            continue;
        }

        QByteArray hash = QCryptographicHash::hash(
            QJsonDocument(content).toJson(QJsonDocument::Compact), QCryptographicHash::Md5);

        if (m_digests.contains(uri) && (m_digests[uri] != hash)) {
            markChanged(uri);
        }

        m_digests[uri] = hash;
    }
}

void MCPNotifier::flush()
{
    pollForContentChanges();

    QSet<QString> pending;
    bool deviceSets;
    {
        QMutexLocker locker(&m_mutex);

        if (m_pending.isEmpty() && !m_pendingDeviceSets) {
            return;
        }

        pending = m_pending;
        deviceSets = m_pendingDeviceSets;
        m_pending.clear();
        m_pendingDeviceSets = false;
    }

    for (const QString& uri : pending) {
        m_streams->notifyResourceUpdated(uri);
    }

    if (deviceSets) {
        m_streams->notifyResourcesMatching("sdrangel://deviceset/");
    }
}
