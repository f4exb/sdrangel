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

#ifndef INCLUDE_FEATURE_MESSAGECENTER_H_
#define INCLUDE_FEATURE_MESSAGECENTER_H_

#include <QMap>
#include <QPointer>
#include <QSet>
#include <QRecursiveMutex>
#include <QTimer>
#include <QNetworkRequest>

#include <memory>
#include <vector>

class QNetworkAccessManager;
class QNetworkReply;

#include "availablechannelorfeaturehandler.h"
#include "feature/feature.h"
#include "util/message.h"
#include "util/messagerecord.h"

#include "messagecenterengine.h"
#include "messagecentersettings.h"

class WebAPIAdapterInterface;
class ChannelAPI;

namespace SWGSDRangel {
    class SWGFeatureSettings;
    class SWGFeatureReport;
    class SWGFeatureActions;
}

// The message center feature: the messages of the channels (the "messages" pipe) and of the files
// are routed to the log, the files and the queue; the pages in the queue are given to the
// modulators that announced themselves, and their states come back on the pipe (see
// MessageCenterEngine). One thread: the one of the feature; the GUI reads the engine.
class MessageCenter : public Feature
{
    Q_OBJECT
public:
    class MsgConfigureMessageCenter : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MessageCenterSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureMessageCenter* create(const MessageCenterSettings& settings, const QStringList& settingsKeys, bool force) {
            return new MsgConfigureMessageCenter(settings, settingsKeys, force);
        }

    private:
        MessageCenterSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureMessageCenter(const MessageCenterSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    //! A page to transmit, composed in the GUI
    class MsgPostPage : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MessageRecord& getRecord() const { return m_record; }

        static MsgPostPage* create(const MessageRecord& record) {
            return new MsgPostPage(record);
        }

    private:
        MessageRecord m_record;

        MsgPostPage(const MessageRecord& record) : Message(), m_record(record) { }
    };

    //! Something to do with a page of the queue
    class MsgPageAction : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        enum Action { Requeue, Cancel, ClearSent };

        const QString& getId() const { return m_id; }
        Action getAction() const { return m_action; }

        static MsgPageAction* create(const QString& id, Action action) {
            return new MsgPageAction(id, action);
        }

    private:
        QString m_id;
        Action m_action;

        MsgPageAction(const QString& id, Action action) : Message(), m_id(id), m_action(action) { }
    };

    //! A record of the log decoded again in the GUI (another codepage or type)
    class MsgRedecoded : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MessageRecord& getRecord() const { return m_record; }

        static MsgRedecoded* create(const MessageRecord& record) {
            return new MsgRedecoded(record);
        }

    private:
        MessageRecord m_record;

        MsgRedecoded(const MessageRecord& record) : Message(), m_record(record) { }
    };

    //! Empty the log; the history too if asked
    class MsgClearLog : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getHistory() const { return m_history; }
        static MsgClearLog* create(bool history = false) { return new MsgClearLog(history); }

    private:
        bool m_history;
        MsgClearLog(bool history) : Message(), m_history(history) { }
    };

    //! Remove channels from the list: by their key (endpointKey), or all those offline
    class MsgRemoveEndpoints : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QStringList& getKeys() const { return m_keys; }
        bool getOffline() const { return m_offline; }
        static MsgRemoveEndpoints* create(const QStringList& keys, bool offline) { return new MsgRemoveEndpoints(keys, offline); }

    private:
        QStringList m_keys;
        bool m_offline;
        MsgRemoveEndpoints(const QStringList& keys, bool offline) : Message(), m_keys(keys), m_offline(offline) { }
    };

    //! Read a file once into the log (JSON lines, or a CSV log): not routed
    class MsgImportFile : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getPath() const { return m_path; }

        static MsgImportFile* create(const QString& path) { return new MsgImportFile(path); }

    private:
        QString m_path;

        MsgImportFile(const QString& path) : Message(), m_path(path) { }
    };

    //! A message record from the WebAPI, as a message received
    class MsgIngestRecords : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const std::vector<MessageRecord>& getRecords() const { return m_records; }

        static MsgIngestRecords* create(const std::vector<MessageRecord>& records) { return new MsgIngestRecords(records); }

    private:
        std::vector<MessageRecord> m_records;

        MsgIngestRecords(const std::vector<MessageRecord>& records) : Message(), m_records(records) { }
    };

    //! To the GUI: say a text
    class MsgSpeak : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getText() const { return m_text; }

        static MsgSpeak* create(const QString& text) { return new MsgSpeak(text); }

    private:
        QString m_text;

        MsgSpeak(const QString& text) : Message(), m_text(text) { }
    };

    //! To the GUI: the log, the endpoints or the queue changed
    class MsgUpdated : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgUpdated* create() { return new MsgUpdated(); }

    private:
        MsgUpdated() : Message() { }
    };

    MessageCenter(WebAPIAdapterInterface *webAPIAdapterInterface);
    ~MessageCenter() override;

    void destroy() override { delete this; }
    bool handleMessage(const Message& cmd) override;

    void getIdentifier(QString& id) const override { id = objectName(); }
    QString getIdentifier() const override { return objectName(); }
    void getTitle(QString& title) const override { title = m_settings.m_title; }

    QByteArray serialize() const override;
    bool deserialize(const QByteArray& data) override;

    int webapiSettingsGet(
        SWGSDRangel::SWGFeatureSettings& response,
        QString& errorMessage) override;

    int webapiSettingsPutPatch(
        bool force,
        const QStringList& featureSettingsKeys,
        SWGSDRangel::SWGFeatureSettings& response,
        QString& errorMessage) override;

    int webapiReportGet(
        SWGSDRangel::SWGFeatureReport& response,
        QString& errorMessage) override;

    int webapiActionsPost(
        const QStringList& featureActionsKeys,
        SWGSDRangel::SWGFeatureActions& query,
        QString& errorMessage) override;

    static void webapiFormatFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& response,
        const MessageCenterSettings& settings);

    static void webapiUpdateFeatureSettings(
        MessageCenterSettings& settings,
        const QStringList& featureSettingsKeys,
        SWGSDRangel::SWGFeatureSettings& response);

    //! The engine, for the GUI to read (same thread)
    const MessageCenterEngine& engine() const { return m_engine; }
    //!< The file of the history of the log (in the application data folder)
    static QString historyFilePath();
    //!< Why the history is not kept although it is on (another message center keeps it, a file error); empty if it is
    QString historyError() const { QMutexLocker mutexLocker(&m_mutex); return m_historyError; }
    qint64 historyBytes() const { QMutexLocker mutexLocker(&m_mutex); return m_engine.historyBytes(); }
    //! A query of the messages: over the history when it is kept (its files are read without holding the engine),
    //! else over the log in memory
    MessageCenterArchiveResult queryMessages(qint64 from, qint64 to, const MessageCenterFilter& filter, int offset, int limit) const;
    const MessageCenterSettings& settings() const { return m_settings; }
    //! Errors of the sinks and sources since the last call
    QStringList takeErrors();
    //! What is wrong with the sinks, sources and routes of the settings, empty if nothing
    static QString checkRouting(const MessageCenterSettings& settings);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    MessageCenterSettings m_settings;
    MessageCenterEngine m_engine;
    AvailableChannelOrFeatureHandler m_availableChannelHandler;
    QTimer m_timer;
    QMap<QString, QPointer<ChannelAPI>> m_transmitters;     //!< By alias, the channels that announced themselves as transmitters
    QStringList m_errors;
    bool m_dirty = false;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    mutable QRecursiveMutex m_mutex;        //!< The engine: the feature thread writes it, the WebAPI reads it

    void applySettings(const MessageCenterSettings& settings, const QStringList& settingsKeys, bool force = false);
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const MessageCenterSettings& settings, bool force);
    void applyEngineSettings();
    //! The sink of a setting (action sinks reach the GUI and the Map through the feature); null if it is not valid
    std::unique_ptr<MessageCenterSink> makeSink(const MessageCenterFileSetting& setting);
    std::unique_ptr<MessageCenterSource> makeSource(const MessageCenterFileSetting& setting);
    void doAction(const QString& type, const MessageRecord& record, const QString& text, quint32 color);
    void sendToMap(const MessageRecord& record, float latitude, float longitude);
    void clearMap();
    QSet<QString> m_mapItems;               //!< Names of the items sent to the Map
    //! A file read once into the log
    int importFile(const QString& path, QString& error);
    void applyHistory();                    //!< The history file of the settings in the engine (one owner a file)
    QString m_historyError;                 //!< Why the history is not kept, empty if it is
    QByteArray m_lastQuery;                 //!< The result of the last query action, with the query (report query)
    static const int m_maxQueryLimit = 1000;    //!< Records of a page of a query action at most
    static MessageCenter*& historyOwner();
    void ingest(const MessageRecord& record, const QObject *source);
    void tick();
    //! The page to the transmitter named on it, as the "tx" action of its channel. The HTTP code and the error
    int postToTransmitter(const MessageCenterPage& page, QString& error);

private slots:
    void handleChannelMessageQueue(MessageQueue *messageQueue);
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FEATURE_MESSAGECENTER_H_
