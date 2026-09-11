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

#ifndef INCLUDE_FEATURE_MCPSTREAMS_H_
#define INCLUDE_FEATURE_MCPSTREAMS_H_

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <QTimer>
#include <QWaitCondition>

class MCPDataFeed;
class MCPTools;
class DeviceAPI;
class ChannelAPI;
class Feature;

// One open server to client SSE connection. The HTTP thread serving the GET request waits on
// this for events to send; any other thread can push events into it.
class MCPEventStream
{
public:
    explicit MCPEventStream(const QString& sessionId);

    const QString& getSessionId() const { return m_sessionId; }
    qint64 ageMs() const;

    void push(const QByteArray& event);
    QByteArray waitForEvent(int timeoutMs); //!< Empty if nothing arrived before the timeout
    void close();
    bool isClosed() const;

private:
    QString m_sessionId;
    mutable QMutex m_mutex;
    QWaitCondition m_wait;
    QQueue<QByteArray> m_events;
    QElapsedTimer m_age;
    bool m_closed;

    static const int m_maxQueued = 256; //!< A client that cannot keep up loses the oldest events
};

// Sessions and their open event streams. A session is created by initialize and identified by
// the Mcp-Session-Id header. Clients that do not use the header share the anonymous session,
// so a single client works fully and several session-less clients share their subscriptions.
class MCPStreams
{
public:
    MCPStreams();
    ~MCPStreams();

    QString createSession();
    bool sessionExists(const QString& sessionId); //!< Also marks the session as recently used
    void endSession(const QString& sessionId);

    void subscribe(const QString& sessionId, const QString& uri);
    QSet<QString> subscribedUris() const;
    bool unsubscribe(const QString& sessionId, const QString& uri);
    QSet<QString> subscriptions(const QString& sessionId) const;

    typedef QSharedPointer<MCPEventStream> StreamPtr;

    StreamPtr open(const QString& sessionId, QString& error);
    void close(const StreamPtr& stream);
    void closeAll(); //!< Must be called before the HTTP listener is destroyed, or its threads will not exit

    //!< Sends notifications/resources/updated to every stream subscribed to the URI
    void notifyResourceUpdated(const QString& uri);
    //!< The same for every subscribed URI starting with the prefix
    void notifyResourcesMatching(const QString& prefix);

    QJsonObject status() const;
    int streamCount() const;
    qint64 getNotificationsSent() const;

    static const int m_maxStreams = 8;
    static const int m_maxSessions = 32;                        //!< Oldest idle sessions are dropped beyond this
    static const qint64 m_sessionIdleMs = 24 * 60 * 60 * 1000;  //!< Sessions unused for this long are dropped

    static const int m_maxStreamLifetimeMs = 10 * 60 * 1000; //!< Backstop against streams whose client vanished

private:
    mutable QMutex m_mutex;
    QList<StreamPtr> m_streams;
    QMap<QString, QSet<QString>> m_subscriptions; //!< Session id (empty for anonymous) to subscribed URIs
    QMap<QString, QDateTime> m_sessions;           //!< Session id to when it was last used
    qint64 m_notificationsSent;

    static QByteArray resourceUpdatedEvent(const QString& uri);
    void expireSessions(); //!< Call with the mutex held
};

// Watches SDRangel for changes worth telling subscribed clients about, and coalesces them so
// that a busy decoder cannot flood the stream.
class MCPNotifier : public QObject
{
    Q_OBJECT
public:
    MCPNotifier(MCPStreams *streams, MCPDataFeed *dataFeed, MCPTools *tools, QObject *parent = nullptr);

    static const int m_coalesceMs = 1000;

private slots:
    void onDeviceSetAdded(int index, DeviceAPI *device);
    void onDeviceStateChanged(int index, DeviceAPI *device);
    void onDeviceSetRemoved(int index);
    void onDeviceChanged(int index);
    void onChannelAdded(int deviceSetIndex, ChannelAPI *channel);
    void onChannelRemoved(int deviceSetIndex, ChannelAPI *channel);
    void onFeatureAdded(int featureSetIndex, Feature *feature);
    void onFeatureRemoved(int featureSetIndex, Feature *feature);
    void onPacketsChanged();
    void onMapItemsChanged();
    void flush();

private:
    MCPStreams *m_streams;
    MCPTools *m_tools;
    QTimer m_timer;
    QMutex m_mutex;
    QSet<QString> m_pending;
    bool m_pendingDeviceSets; //!< A structural change, so every subscribed device set is stale too
    QMap<QString, QByteArray> m_digests; //!< Subscribed resource URI to a hash of what it last held

    void markChanged(const QString& uri);
    void markStructuralChange();
    void pollForContentChanges();
};

#endif // INCLUDE_FEATURE_MCPSTREAMS_H_
