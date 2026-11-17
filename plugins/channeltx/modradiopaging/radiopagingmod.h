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

#ifndef INCLUDE_RADIOPAGINGMOD_H
#define INCLUDE_RADIOPAGINGMOD_H

#include <QNetworkRequest>
#include <QRecursiveMutex>

#include <atomic>
#include <map>
#include <vector>

#include "dsp/basebandsamplesource.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "util/messagerecord.h"

#include "radiopagingpocsag.h"
#include "radiopagingflexscheduler.h"
#include "radiopagingpage.h"
#include "radiopagingmodsettings.h"
#include "radiopagingmodsource.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class QTimer;
class DeviceAPI;
class ObjectPipe;
class RadioPagingModBaseband;

class RadioPagingMod : public BasebandSampleSource, public ChannelAPI {
public:
    //! A message to transmit in the protocol of the channel (the message of that protocol is
    //! taken). Speed and polarity default to the channel settings; FLEX is sent in the mode of
    //! the network, in the other polarity in the frames the network leaves free
    struct TxRequest
    {
        int m_protocol = -1;            //!< RadioPagingModSettings::Protocol the message was made for, -1 for any
        RadioPagingPocsag::TxMessage m_pocsag;
        int m_speed = 0;                //!< POCSAG baud, 0 for the channel setting
        int m_polarity = -1;            //!< -1 for the channel setting, 0 normal, 1 inverted
        RadioPagingFlexEncoder::TxMessage m_flex;
        RadioPagingGsc::TxMessage m_gsc;
        QString m_voiceFile;            //!< GSC voice page: the path of the WAV file
        RadioPagingGscTxStream::Voice m_voice;  //!< The file loaded when the request was checked, not loaded again
        bool m_live = false;            //!< GSC voice page from the microphone: the voice comes with MsgLiveVoice, up to MsgLiveEnd
        QString m_pageId;               //!< Page of the message center: its id is the id of the record, its states are reported
        // When the message is sent, network mode only: retransmissions (FLEX: numbered messages of a network with
        // message numbering, again with the number and R = 0; POCSAG: repeats), seconds between them (-1: 240 s FLEX,
        // 10 s POCSAG) and seconds before the first transmission. FLEX takes them in frames of 1.875 s, rounded up
        int m_retransmit = 0;
        double m_retransmitInterval = -1.0;
        double m_sendDelay = 0.0;
        //!< Alphanumeric (FLEX secure type 0 too): the codepage of the text in the protocol, none for the one of the
        //!< channel. The text is UTF-8, encoded to the codes of the codepage when the request is checked
        RadioPagingCharset::Key m_charset;
    };

    //! The request of a page checked with RadioPagingPage::check() (the channel and the encoder check the rest)
    static TxRequest requestFromPage(const RadioPagingPage& page);

    class MsgConfigureRadioPagingMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RadioPagingModSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureRadioPagingMod* create(const QStringList& settingsKeys, const RadioPagingModSettings& settings, bool force) {
            return new MsgConfigureRadioPagingMod(settingsKeys, settings, force);
        }

    private:
        RadioPagingModSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureRadioPagingMod(const QStringList& settingsKeys, const RadioPagingModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    //! Transmit a message (from the GUI); the result comes back as MsgReportTx
    class MsgTx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const TxRequest& getRequest() const { return m_request; }

        static MsgTx* create(const TxRequest& request) {
            return new MsgTx(request);
        }

    private:
        TxRequest m_request;

        MsgTx(const TxRequest& request) :
            Message(),
            m_request(request)
        { }
    };

    //! Result of a transmit request: the id it was queued with, or an error
    class MsgReportTx : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        quint64 getId() const { return m_id; }
        const QString& getError() const { return m_error; }
        const QString& getWarning() const { return m_warning; }

        static MsgReportTx* create(quint64 id, const QString& error, const QString& warning = QString()) {
            return new MsgReportTx(id, error, warning);
        }

    private:
        quint64 m_id;
        QString m_error;
        QString m_warning;              //!< The message was queued but not as asked (a sound file cut)

        MsgReportTx(quint64 id, const QString& error, const QString& warning) :
            Message(),
            m_id(id),
            m_error(error),
            m_warning(warning)
        { }
    };

    //! A message is being transmitted, was sent or will not be (from the source, through the input queue)
    class MsgTxEvent : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        enum Event { Transmitted, Sent, Dropped, Failed, Repeated };

        quint64 getId() const { return m_id; }
        Event getEvent() const { return m_event; }

        static MsgTxEvent* create(quint64 id, Event event) {
            return new MsgTxEvent(id, event);
        }

    private:
        quint64 m_id;
        Event m_event;

        MsgTxEvent(quint64 id, Event event) : Message(), m_id(id), m_event(event) { }
    };

    //! FLEX: how a message or a copy (repeat) went: its message number, fragments and R flag
    class MsgTxStatus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        quint64 getId() const { return m_id; }
        bool getRepeat() const { return m_repeat; }
        const RadioPagingFlexEncoder::SentStatus& getStatus() const { return m_status; }

        static MsgTxStatus* create(quint64 id, bool repeat, const RadioPagingFlexEncoder::SentStatus& status) {
            return new MsgTxStatus(id, repeat, status);
        }

    private:
        quint64 m_id;
        bool m_repeat;
        RadioPagingFlexEncoder::SentStatus m_status;

        MsgTxStatus(quint64 id, bool repeat, const RadioPagingFlexEncoder::SentStatus& status) :
            Message(), m_id(id), m_repeat(repeat), m_status(status) { }
    };

    //! To the GUI: a message of the log of the messages transmitted, as it was queued, sent or dropped
    class MsgTxLog : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const MessageRecord& getRecord() const { return m_record; }
        const QString& getStatus() const { return m_status; }    //!< queued, sent or dropped

        static MsgTxLog* create(const MessageRecord& record, const QString& status) {
            return new MsgTxLog(record, status);
        }

    private:
        MessageRecord m_record;
        QString m_status;

        MsgTxLog(const MessageRecord& record, const QString& status) : Message(), m_record(record), m_status(status) { }
    };

    //! Voice of the live voice page id, at the rate of the modulator
    class MsgLiveVoice : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        quint64 getId() const { return m_id; }
        const std::vector<float>& getSamples() const { return m_samples; }

        static MsgLiveVoice* create(quint64 id, const std::vector<float>& samples) {
            return new MsgLiveVoice(id, samples);
        }

    private:
        quint64 m_id;
        std::vector<float> m_samples;

        MsgLiveVoice(quint64 id, const std::vector<float>& samples) : Message(), m_id(id), m_samples(samples) { }
    };

    //! The live voice page id ends
    class MsgLiveEnd : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        quint64 getId() const { return m_id; }

        static MsgLiveEnd* create(quint64 id) {
            return new MsgLiveEnd(id);
        }

    private:
        quint64 m_id;

        MsgLiveEnd(quint64 id) : Message(), m_id(id) { }
    };

    //! Drop the messages waiting; with abort (one-shot), end the transmission being sent as well
    class MsgClear : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getAbort() const { return m_abort; }

        static MsgClear* create(bool abort) {
            return new MsgClear(abort);
        }

    private:
        bool m_abort;

        MsgClear(bool abort) :
            Message(),
            m_abort(abort)
        { }
    };

    static const int m_maxTextLength = 2000;    //!< Characters of a message
    static const qint64 m_copySlackMs = 3600000;    //!< Retransmissions: longest wait of a copy past its interval (queue, frames)

    RadioPagingMod(DeviceAPI *deviceAPI);
    virtual ~RadioPagingMod();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    virtual void start();
    virtual void stop();
    virtual void pull(SampleVector::iterator& begin, unsigned int nbSamples);
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSourceName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);
    int getChannelSampleRate() const override;

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    virtual int webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage);

    virtual int webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage);

    virtual int webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage);

    virtual int webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage);

    virtual int webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const RadioPagingModSettings& settings);

    static void webapiUpdateChannelSettings(
        RadioPagingModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response);

    double getMagSq() const;
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;
    int getSourceChannelSampleRate() const;
    int getAudioSampleRate() const override;
    //! Status of the source; queued counts the messages accepted and not being sent yet
    RadioPagingModSource::Status getStatus() const;

    //! Requests refused since the channel was created, from the GUI, the Message Center or the WebAPI, and the last reason
    quint64 getRefused(QString *lastReason = nullptr) const
    {
        QMutexLocker mutexLocker(&m_refusedMutex);

        if (lastReason) {
            *lastReason = m_lastRefusal;
        }

        return m_refused;
    }

    //! Queue a message: its id, or 0 and an error. Main thread
    quint64 transmit(const TxRequest& request, QString& error, QString *warning = nullptr);
    void clear(bool abort);
    //! FLEX network: the System Event Notification of the event flags (1-2047, 0 cancels) in each frame for frames
    //! (128-512, STD-43A 3.8.2.4). False and an error for another protocol, one-shot or out of range
    bool notifySystemEvent(unsigned events, int frames, QString& error);

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    RadioPagingModBaseband* m_basebandSource;
    RadioPagingModSettings m_settings;
    mutable QRecursiveMutex m_settingsMutex;    //!< m_settings: written in the main thread, read by the WebAPI
    std::atomic<quint64> m_nextId;
    std::atomic<quint64> m_accepted;            //!< Messages passed to the baseband

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    qint64 m_centerFrequency = 0;               //!< Centre frequency of the device, Hz

    // Messages queued and not sent or dropped yet (main thread only): the record they will be
    // logged with, and the voice to record
    struct PendingTx
    {
        MessageRecord m_record;
        RadioPagingGscTxStream::Voice m_voice;  //!< Voice page from a file
        std::vector<float> m_live;              //!< Live voice page: the voice sent, at the rate of the modulator
        bool m_isVoice = false;
        QString m_pageId;                       //!< Page of the message center, empty if none
        bool m_transmitted = false;             //!< Its blocks were pushed to the modulator
        int m_retransmit = 0;                   //!< Copies sent after it
        qint64 m_copySpanMs = 0;                //!< Longest time from one copy to the next
    };
    std::map<quint64, PendingTx> m_pending;
    // Requests refused (getRefused): counted in the main thread and in the WebAPI thread
    mutable QMutex m_refusedMutex;
    quint64 m_refused = 0;
    QString m_lastRefusal;
    void refused(const QString& error)
    {
        QMutexLocker mutexLocker(&m_refusedMutex);
        m_refused++;
        m_lastRefusal = error;
    }
    // Messages sent with copies to go (main thread only): the record of the copies. A copy that is not sent (the
    // queue cleared, the transmission cut) is not reported: the entry goes when no copy can come any more
    struct RepeatingTx
    {
        MessageRecord m_record;
        int m_copies = 0;                       //!< Copies to go
        int m_sent = 0;                         //!< Copies sent
        qint64 m_copySpanMs = 0;
        qint64 m_expiresMs = 0;                 //!< No copy after this time
        RadioPagingFlexEncoder::SentStatus m_status;    //!< FLEX: how the next copy went (R = 0)
        bool m_hasStatus = false;
    };
    std::map<quint64, RepeatingTx> m_repeating;
    void repeated(quint64 id);
    void expireRepeating(qint64 nowMs);
    void fillRecord(const TxRequest& request, const RadioPagingModSource::Request& sourceRequest, quint64 id, PendingTx& pending);
    void txEvent(quint64 id, MsgTxEvent::Event event);
    //! The state of a page of the message center, on the messages pipe (see MessageCenterEngine::applyEvent)
    void publishPageEvent(const QString& pageId, const QString& state, const QString& reason = QString());
    //! This transmitter in the message center: its alias, protocols, speeds and polarities, and its queue
    MessageRecord endpointRecord(const QString& event);
    void announce(const QString& event = "status");
    QTimer *m_announceTimer = nullptr;
    QString m_lastName;                         //!< The channel name announced last
    QString recordVoice(const PendingTx& pending, double& seconds);
    void publishRecord(const MessageRecord& record);

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const RadioPagingModSettings& settings, bool force = false);
    //! Checks of a request against the channel (protocol, speed or mode, polarity, message,
    //! queue) and its source request; an error, empty if it can be sent
    QString checkRequest(const TxRequest& request, RadioPagingModSource::Request& sourceRequest, QString *warning = nullptr);
    //!< The codepage of an alphanumeric message, nullptr if the one of the request is not a codepage of the protocol
    const RadioPagingCharset::Codepage *requestCodepage(const TxRequest& request, RadioPagingModSettings::Protocol protocol) const;
    int outstanding() const;
    void sendSampleRateToDemodAnalyzer();
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const RadioPagingModSettings& settings, bool force);
    void sendChannelSettings(
        const QList<ObjectPipe*>& pipes,
        const QList<QString>& channelSettingsKeys,
        const RadioPagingModSettings& settings,
        bool force
    );
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RadioPagingModSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_RADIOPAGINGMOD_H
