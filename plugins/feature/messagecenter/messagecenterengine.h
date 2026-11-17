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

#ifndef INCLUDE_MESSAGECENTERENGINE_H
#define INCLUDE_MESSAGECENTERENGINE_H

#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include "util/messagerecord.h"
#include "util/messageendpoint.h"
#include "radiopagingcharset.h"

class QUdpSocket;

// The message center without the user interface: where the messages come from (sources), where
// they go (sinks) and which way (routes), the log, the endpoints (the channels that registered)
// and the queue of the pages to transmit.
//
//   source ---> route (filter) ---> sink(s)
//   sources: the channels (through the "messages" pipe, fed with ingest), a JSON lines file, a CSV file (of this
//            message center or of the Pager demodulator), UDP datagrams (JSON lines or the Pager demodulator format)
//   sinks:   the log, a JSON lines file, a CSV file, UDP datagrams, the transmit queue, and the actions
//            on a message that matches: a command, speech, a highlight and a position on the map
//
// Endpoints (demodulators and modulators) announce themselves with event records (see
// MessageEndpoint::fromRecord). A page for transmission goes through the states
// pending -> queued -> claimed -> transmitted -> sent; the modulator claims it, and reports
// the states back with event records of the same id (see MessageCenterEngine::applyEvent).
// No Qt GUI and no thread: the caller (the feature) serializes the calls.

// A contact of the address book: a name for the addresses of one pager (or station), in any protocol, as a pager
// has several (POCSAG functions, FLEX capcodes). The records of its addresses get its name (protocolData contact),
// its icon (contact_icon) and its colour (contact_color)
struct MessageCenterContact
{
    //! A condition on a field of a record: the field is a field of the record as JSON (protocol, address, msgType,
    //! channel, frequency, mode...) or of its protocol data (ric, function, capcode, group_capcodes...). It is the
    //! value (numbers compared as numbers), or matches the regular expression (the whole value); a list (the members
    //! of a group message) when one of its values does
    struct Condition
    {
        QString m_field;
        QString m_value;
        bool m_regExp = false;
        mutable QRegularExpression m_compiled;      //!< Of m_value, made the first time

        //!< The value of the field in a record (record JSON and protocol data together, see recordFields)
        bool matches(const QJsonObject& fields) const;
        //!< Empty if it can match, else why not: a field, a valid regular expression
        QString check() const;
        QJsonObject toJson() const;
        static Condition fromJson(const QJsonObject& json);
    };
    //! A rule: all its conditions
    typedef QList<Condition> Rule;

    QString m_name;
    //! How a page is sent to the contact: when the page meets the conditions (on the fields of the page as the WebAPI
    //! posts it: protocol, type, text, priority...; none: always), the fields of page are set on it (transmitter,
    //! protocol, address, function...). The first one whose conditions the page meets
    struct SendTo
    {
        Rule m_when;
        QJsonObject m_page;

        //!< Empty if it can send, else why not: a transmitter and an address of its protocol
        QString check() const;
        QJsonObject toJson() const;
        static SendTo fromJson(const QJsonObject& json);
    };

    QList<Rule> m_rules;                //!< The messages of the contact: those of any of its rules
    QList<SendTo> m_sendTo;             //!< In order: the first one for the page
    QString m_icon;                     //!< A built-in icon (:name, see iconCategories), or the path of an image file; empty for none
    quint32 m_color = 0;                //!< ARGB, 0 for none
    QString m_note;

    //!< The record matches one of the rules (fields: recordFields of the record)
    bool matches(const QJsonObject& fields) const;
    //!< A page for the contact (the fields of a page as the WebAPI posts it): page with the fields of the first send to
    //!< whose conditions it meets. False and why if none
    bool pageFor(const QJsonObject& message, QJsonObject& page, QString *error = nullptr) const;
    QJsonObject toJson() const;
    static MessageCenterContact fromJson(const QJsonObject& json);
    static QByteArray listToJson(const QList<MessageCenterContact>& contacts);
    //!< The contacts of a JSON array; error says what is not valid (the contacts are not taken then)
    static QList<MessageCenterContact> listFromJson(const QByteArray& json, QString *error = nullptr);
    //!< The fields of a record the conditions look at: its JSON, the protocol data in it at the top too (a field of
    //!< the record first)
    static QJsonObject recordFields(const MessageRecord& record);
    //!< A rule as a text: protocol is pocsag and ric is 1234567
    static QString ruleText(const Rule& rule);
    //!< The built-in icons by category, in the order of the picker (Microsoft Fluent Emoji, flat, MIT License):
    //!< :name is :/messagecenter/icons/name.svg in the GUI
    static const QList<QPair<QString, QStringList>>& iconCategories();
    //!< The image of an icon: the resource of a built-in one (:name), else the path itself
    static QString iconPath(const QString& icon);
};

// The records a filter selects: every field set must match
struct MessageCenterFilter
{
    QStringList m_protocols;            //!< Lower case protocol names, empty for all
    QStringList m_contacts;             //!< Names of contacts of the address book, empty for all
    QStringList m_channels;             //!< Channel names (titles) or transmitters of pages, empty for all
    int m_direction = -1;               //!< MessageRecord::Direction, -1 for both
    int m_kind = -1;                    //!< MessageRecord::Kind, -1 for all
    int m_polarity = -1;                //!< MessageRecord::Polarity, -1 for all
    QList<int> m_speeds;                //!< Bits per second, empty for all
    QString m_address;                  //!< Regular expression the whole address must match (as the filters of SDRangel), empty for all
    //! Regular expression found anywhere in the text, not case sensitive, . matches line ends too; its captures are
    //! those of the actions. Empty for all
    QString m_text;
    //! One more condition, on any field of the record or of its protocol data (capcode, ric, function, mode...): its
    //! whole value matches the regular expression m_value (a list: one of its values). Empty field: none
    QString m_field;
    QString m_value;
    QStringList m_sources;              //!< Where the record comes from: channels, api or the name of a source; empty for all
    QString m_msgType;                  //!< Message type (alphanumeric, numeric, tone, voice, short_message, binary, secure), empty for all
    qint64 m_frequencyMin = -1;         //!< Hz, -1: no lower limit
    qint64 m_frequencyMax = -1;
    qint64 m_timeFrom = -1;             //!< Milliseconds since the epoch, -1: no limit
    qint64 m_timeTo = -1;
    bool m_hideDuplicates = false;
    bool m_voiceOnly = false;           //!< Voice messages with a file

    //!< The record matches, wherever it comes from (the sources of the filter are not looked at)
    bool matches(const MessageRecord& record) const;
    //!< The record from a source matches: source is channels, api or the name of a source
    bool matches(const MessageRecord& record, const QString& source) const;
    //!< What the expression of the text captured (0 the whole match); empty if there is none or it did not match
    QStringList captures(const MessageRecord& record) const;
    bool isEmpty() const;
    QJsonObject toJson() const;
    static MessageCenterFilter fromJson(const QJsonObject& json);
    //!< The errors of the regular expressions of the address, the text and the field, empty if they are all valid
    QString errors() const;

private:
    // The expressions, compiled when they are first used after a change (a filter is copied around: they are
    // compiled again in the copy)
    struct Compiled
    {
        QString m_pattern;
        bool m_done = false;
        QRegularExpression m_expression;
    };
    mutable Compiled m_compiledAddress;     //!< m_address, the whole address
    mutable Compiled m_compiledText;        //!< m_text, anywhere in the text
    mutable Compiled m_compiledValue;       //!< m_value, the whole value of the field
    static const QRegularExpression& compile(Compiled& compiled, const QString& pattern, bool anchored,
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption);
};

//! ${address}, ${message}, ${protocol}, ${type}, ${channel}, ${mode}, ${id}, ${frequency}, ${time} (ISO, UTC) and
//! ${0}, ${1}... (the captures of the regular expression) replaced in a text; one pass, so that what is put in is
//! not replaced again; an unknown name stays as it is
QString messageCenterSubstitute(const QString& text, const MessageRecord& record, const QStringList& captures);

// A sink takes the records routed to it
class MessageCenterArchive;

//! The records a query of the log selects: the total, and those of the page, newest first
struct MessageCenterArchiveResult
{
    int m_total = 0;
    std::vector<MessageRecord> m_records;
};

class MessageCenterSink
{
public:
    virtual ~MessageCenterSink() {}
    virtual QString name() const = 0;
    //!< False and an error if the record could not be taken
    virtual bool accept(const MessageRecord& record, QString& error) = 0;
    //!< As above, with what the regular expression of the route captured in the message
    virtual bool accept(const MessageRecord& record, const QStringList& captures, QString& error)
    {
        (void) captures;
        return accept(record, error);
    }
};

// A source gives records when polled
class MessageCenterSource
{
public:
    virtual ~MessageCenterSource() {}
    virtual QString name() const = 0;
    //!< The records new since the last call. False and an error if it can not be read
    virtual bool poll(std::vector<MessageRecord>& records, QString& error) = 0;
    //!< More is waiting after the last poll (a big file is read over several polls)
    virtual bool more() const { return false; }
};

// Records appended to a file, one JSON object per line (lossless), CSV (the columns of MessageRecord), or the CSV log of
// the Pager demodulator (its messages only)
class MessageCenterFileSink : public MessageCenterSink
{
public:
    enum Format { JsonLines, Csv, PagerCsv };

    MessageCenterFileSink(const QString& name, const QString& path, Format format);
    QString name() const override { return m_name; }
    using MessageCenterSink::accept;
    bool accept(const MessageRecord& record, QString& error) override;
    const QString& path() const { return m_path; }
    Format format() const { return m_format; }

private:
    QString m_name;
    QString m_path;
    Format m_format;
};

// Records read from a JSON lines file; the lines added to it later are read by the next poll
class MessageCenterFileSource : public MessageCenterSource
{
public:
    MessageCenterFileSource(const QString& name, const QString& path);
    QString name() const override { return m_name; }
    bool poll(std::vector<MessageRecord>& records, QString& error) override;
    bool more() const override { return m_more; }
    int skipped() const { return m_skipped; }       //!< Lines that were not records
    static const int m_maxLinesPerPoll = 1000;
    static const int m_maxLineLength = 1 << 20;

private:
    QString m_name;
    QString m_path;
    qint64 m_offset = 0;
    int m_skipped = 0;
    bool m_more = false;
};

// Records read from a CSV file: the log of a message center (the columns of MessageRecord) or of the Pager
// demodulator. The first row names the columns; the rows added later are read by the next poll
class MessageCenterCsvSource : public MessageCenterSource
{
public:
    //!< newRowsOnly: the rows already in the file when it is first read are skipped (a log another program writes to)
    MessageCenterCsvSource(const QString& name, const QString& path, bool newRowsOnly = false);
    QString name() const override { return m_name; }
    bool poll(std::vector<MessageRecord>& records, QString& error) override;
    bool more() const override { return m_more; }
    int skipped() const { return m_skipped; }       //!< Rows that were not messages
    static const int m_maxLinesPerPoll = 1000;
    static const int m_maxLineLength = 1 << 20;

private:
    QString m_name;
    QString m_path;
    qint64 m_offset = 0;
    int m_skipped = 0;
    bool m_more = false;
    bool m_headerRead = false;
    bool m_newRowsOnly = false;
    bool m_skipped0 = false;                        //!< The rows there were at the start are skipped
    bool m_pagerFormat = false;
    bool m_notReported = true;                      //!< The header is wrong, said once
    QHash<QString, int> m_columns;
};

// Datagrams received: a message record as JSON (one or several lines) or the Pager demodulator format. What comes
// from the network is a message received: its direction is rx and it has no transmitter, so that it cannot
// become a page to transmit
class MessageCenterUdpSource : public MessageCenterSource
{
public:
    MessageCenterUdpSource(const QString& name, const QString& address, quint16 port);
    ~MessageCenterUdpSource() override;
    // It owns its socket: not copied
    MessageCenterUdpSource(const MessageCenterUdpSource&) = delete;
    MessageCenterUdpSource& operator=(const MessageCenterUdpSource&) = delete;
    QString name() const override { return m_name; }
    bool poll(std::vector<MessageRecord>& records, QString& error) override;
    int skipped() const { return m_skipped; }       //!< Datagrams that were not messages
    static const int m_maxDatagramsPerPoll = 1000;
    static const int m_maxDatagramSize = 65535;

private:
    QString m_name;
    std::unique_ptr<QUdpSocket> m_socket;
    QString m_bindError;
    bool m_errorReported = false;
    int m_skipped = 0;
};

// Each message as a datagram: the message record as one line of JSON, or the Pager demodulator format
class MessageCenterUdpSink : public MessageCenterSink
{
public:
    enum Format { PagerDemod, Json };

    MessageCenterUdpSink(const QString& name, const QString& address, quint16 port, Format format);
    ~MessageCenterUdpSink() override;
    // It owns its socket: not copied
    MessageCenterUdpSink(const MessageCenterUdpSink&) = delete;
    MessageCenterUdpSink& operator=(const MessageCenterUdpSink&) = delete;
    QString name() const override { return m_name; }
    using MessageCenterSink::accept;
    bool accept(const MessageRecord& record, QString& error) override;
    static const int m_maxDatagramSize = 65507;

private:
    QString m_name;
    QString m_addressText;
    quint16 m_port;
    Format m_format;
    std::unique_ptr<QUdpSocket> m_socket;
    bool m_addressValid;
    QHostAddress m_address;
};

// Runs a command for each message: the command is split into the program and its arguments first, then the text
// of the message is put into each argument, so that a message cannot add arguments or change the program. At
// most m_maxPerSecond are started in a second, the others are refused
class MessageCenterCommandSink : public MessageCenterSink
{
public:
    typedef std::function<bool(const QString& program, const QStringList& arguments)> Launcher;

    static const int m_maxPerSecond = 5;

    MessageCenterCommandSink(const QString& name, const QString& command, Launcher launcher = Launcher());
    QString name() const override { return m_name; }
    using MessageCenterSink::accept;
    bool accept(const MessageRecord& record, QString& error) override { return accept(record, QStringList(), error); }
    bool accept(const MessageRecord& record, const QStringList& captures, QString& error) override;

private:
    QString m_name;
    QString m_command;
    Launcher m_launcher;
    std::deque<qint64> m_started;       //!< When the last ones started, ms since the epoch
};

// What the feature does with a message: speech, a highlight in the log, a position on the map
class MessageCenterActionSink : public MessageCenterSink
{
public:
    //! type: speech, highlight or map. text: the text with the substitutions done. color: the highlight, ARGB
    typedef std::function<void(const QString& type, const MessageRecord& record, const QString& text, quint32 color)> Action;

    MessageCenterActionSink(const QString& name, const QString& type, const QString& text, quint32 color, Action action);
    QString name() const override { return m_name; }
    using MessageCenterSink::accept;
    bool accept(const MessageRecord& record, QString& error) override { return accept(record, QStringList(), error); }
    bool accept(const MessageRecord& record, const QStringList& captures, QString& error) override;

private:
    QString m_name;
    QString m_type;
    QString m_text;
    quint32 m_color;
    Action m_action;
};

// A mark on the messages a route sends to it: an icon, a colour of the text and a colour of the row, each optional. The
// engine writes it into the record before the record goes to the sinks (protocolData "marks", a list: a message of
// several routes has their marks), so the log, the history and the API have it
class MessageCenterMarkSink : public MessageCenterSink
{
public:
    //! icon: a built-in icon (:name) or the path of an image, empty for none; textColor, background: ARGB, 0 for none
    MessageCenterMarkSink(const QString& name, const QString& icon, quint32 textColor, quint32 background);
    QString name() const override { return m_name; }
    using MessageCenterSink::accept;
    bool accept(const MessageRecord& record, QString& error) override;
    //!< The mark as it is in the record: icon, color, background (those set), and the name of the sink
    QVariantMap mark() const;

private:
    QString m_name;
    QString m_icon;
    quint32 m_textColor;
    quint32 m_background;
};

// A page to transmit
struct MessageCenterPage
{
    enum State { Pending, Queued, Claimed, Transmitted, Sent, Dead };

    MessageRecord m_record;             //!< The message; its id is the id of the page; protocolData "transmitter" is the alias
    State m_state = Pending;
    int m_attempts = 0;                 //!< Claims so far
    int m_retransmits = 0;              //!< Times it went back to the queue after a failure
    bool m_dispatched = false;          //!< Sent to the transmitter, not claimed yet
    qint64 m_dispatchedAt = 0;
    QString m_claimedBy;
    QString m_reason;                   //!< Why it is dead (rejected, aborted, underrun, interrupted, expired, attempts)
    qint64 m_changed = 0;

    QString transmitter() const { return m_record.m_protocolData.value("transmitter").toString(); }
    static QString stateName(State state);
};

class MessageCenterEngine
{
public:
    static const int m_maxAttempts = 3;         //!< First try and two retransmits
    static const int m_defaultLogSize = 10000;
    static const int m_minLogSize = 100;
    //!< A FLEX record is a few kB (its words, codes and raw data): 10000 of them are some tens of MB
    static const int m_maxLogSize = 10000;
    static const int m_defaultHistoryMB = 500;          //!< Size kept of the history (the archive)
    static const int m_defaultHistoryDays = 30;         //!< Days kept of the history

    MessageCenterEngine();
    ~MessageCenterEngine();

    // Sinks and sources by name. The log and the transmit queue are always there
    void addSink(std::unique_ptr<MessageCenterSink> sink);
    bool removeSink(const QString& name);
    QStringList sinkNames() const;
    void addSource(std::unique_ptr<MessageCenterSource> source);
    bool removeSource(const QString& name);
    QStringList sourceNames() const;
    //!< Poll the sources and ingest what they give; returns the records taken, errors are collected
    int pollSources(QStringList& errors);

    // Routes: every message is checked against all the routes in their order; each route that selects it sends it to
    // its sinks; stop: the routes after it do not see it. The log is a sink: a message no route sends to it is not
    // logged (the default route sends everything to the log)
    struct Route
    {
        QString m_name;
        MessageCenterFilter m_filter;
        QStringList m_sinks;            //!< Names: "log", "queue", or a sink added
        bool m_stop = false;
        bool m_enabled = true;
        //! The message decoded again before it goes to the sinks of the route: with this codepage (none: the one of
        //! the record) as this type (empty: the one of the record). A message that cannot be is sent as it is
        RadioPagingCharset::Key m_decodeCharset;
        QString m_decodeType;
        bool decodes() const { return !m_decodeCharset.empty() || !m_decodeType.isEmpty(); }
    };
    //! The routes of a new message center: every message to the log
    static std::vector<Route> defaultRoutes();
    void setRoutes(const std::vector<Route>& routes) { m_routes = routes; }
    const std::vector<Route>& routes() const { return m_routes; }

    // The address book
    void setContacts(const QList<MessageCenterContact>& contacts);
    const QList<MessageCenterContact>& contacts() const { return m_contacts; }
    //!< The first contact one of whose rules the record matches, nullptr if none
    const MessageCenterContact *contactOf(const MessageRecord& record) const;
    //!< The record with the name, icon and colour of its contact in protocolData (contact, contact_icon,
    //!< contact_color), or without them when it has none
    void nameContact(MessageRecord& record) const;
    //! The marks of the routes that select the record (from source) and send it to a mark sink, into its protocol data
    void markRecord(MessageRecord& record, const QString& source) const;

    //!< A record from a source: events update the endpoints and the pages, the others are routed
    void ingest(const MessageRecord& record, const QString& source, qint64 now);

    // The log
    void setLogSize(int size);
    const std::deque<MessageRecord>& log() const { return m_log; }
    //!< The log records a filter selects, the newest last
    std::vector<MessageRecord> select(const MessageCenterFilter& filter) const;
    const MessageRecord *find(const QString& id) const;
    //!< The records of the pages in the log get the state of their page (protocolData page_state and page_reason)
    //!< until the transmitter's record replaces them. Returns the number of records changed
    int syncLoggedPages();
    //!< A record of the log decoded again (another codepage or type): it replaces the one of its id. False if none
    bool replace(const MessageRecord& record);
    //!< Empties the log (in memory); the history stays
    void clearLog();

    // The history: every record stored (a later version of a message too) kept in the files of a folder, a day
    // each up to a size, for days (MessageCenterArchive). The log is the newest of them when the message center
    // starts, and the queries go over all of it
    //!< Folder empty: no history. An empty log takes the newest records of the history. False and an error if the
    //!< folder cannot be used
    bool setHistory(const QString& folder, int days, qint64 maxBytes, QString& error);
    QString historyFolder() const;
    qint64 historyBytes() const;
    //!< Removes the files of the history
    bool clearHistory(QString& error);
    //!< The history, when it is kept (null if not): its queries can be made from any thread, it lives as long as
    //!< they need it
    std::shared_ptr<const MessageCenterArchive> archive() const;
    //!< A query over the log in memory, as the archive does (the history not kept)
    MessageCenterArchiveResult queryLog(qint64 from, qint64 to, const MessageCenterFilter& filter, int offset, int limit) const;

    // Endpoints
    const QMap<QString, MessageEndpoint>& endpoints() const { return m_endpoints; }
    //!< Removes endpoints by their key (role and alias, endpointKey), or all those offline; the pages of a transmitter
    //!< removed are given back as when it went offline. The number removed
    int removeEndpoints(const QStringList& keys, bool offline, qint64 now);
    //!< The transmitters that could send a page
    QStringList eligible(const MessageRecord& page) const;

    // Pages
    //!< A page: record (direction TX) with protocolData "transmitter" (the alias of the transmitter). Returns its id
    QString addPage(MessageRecord record, qint64 now);
    const MessageCenterPage *page(const QString& id) const;
    std::vector<MessageCenterPage> pages(int state = -1) const;     //!< state: MessageCenterPage::State, -1 for all
    //!< Pending pages with a transmitter now go to queued; the queued pages whose transmitter is ready are
    //!< marked dispatched and returned, for the feature to give them to the transmitter
    std::vector<MessageCenterPage> dispatch(qint64 now);
    //!< The state of a page as the transmitter reports it. state: claimed, transmitted, sent, rejected, aborted,
    //!< underrun, interrupted, released
    bool applyEvent(const QString& id, const QString& state, const QString& reason, qint64 now);
    //!< Dispatched pages not claimed for this long go back to the queue (a claim counts as an attempt)
    void expireClaims(qint64 now, qint64 timeoutMs);
    //!< Channels that have not announced themselves for silentMs are offline
    void sweep(qint64 now, qint64 silentMs);
    //!< The errors of the sinks since the last call (at most m_maxSinkErrors are kept)
    QStringList takeSinkErrors();
    //! A message of an import (a file read once): in the log, not routed, so that no action is taken again on it
    void importRecord(const MessageRecord& record);
    //! The records each sink took, and those it refused, by name ("log" and "queue" too)
    struct SinkStat
    {
        int m_accepted = 0;
        int m_failed = 0;
    };
    const QMap<QString, SinkStat>& sinkStats() const { return m_sinkStats; }
    static QString endpointKey(MessageEndpoint::Role role, const QString& alias);
    static const int m_maxSinkErrors = 100;
    //!< Dead page back to pending with its attempts reset
    bool requeue(const QString& id, qint64 now);
    bool cancel(const QString& id);
    int removeFinished();                       //!< Sent pages are forgotten; returns how many

    // Counters
    //!< The version of the log: one more for each record stored or changed, and when it is emptied or loaded
    int logged() const { return m_logged; }
    int routed() const { return m_routed; }
    //!< The ids of the records stored or changed since the version given, oldest first (an id may be there more
    //!< than once). False when the log was emptied or loaded since, or the changes are no longer all kept: the
    //!< views show the log again
    bool changedSince(int version, QStringList& ids) const;

private:
    std::deque<MessageRecord> m_log;
    int m_logSize;
    int m_logged = 0;
    QList<MessageCenterContact> m_contacts;
    std::deque<std::pair<int, QString>> m_changes;  //!< Version and id of the last records stored or changed
    int m_resetVersion = 0;                         //!< Version of the last time the log was emptied or loaded
    static const int m_maxChanges = 4096;

    void noteChange(const QString& id);             //!< A record stored or changed: a new version
    void noteReset();                               //!< The log emptied, loaded or cut: a new version
    std::shared_ptr<MessageCenterArchive> m_archive;    //!< The history: open or not (a query being made keeps it)

    void appendHistory(const MessageRecord& record);
    int m_routed = 0;
    std::vector<Route> m_routes;
    std::vector<std::unique_ptr<MessageCenterSink>> m_sinks;
    std::vector<std::unique_ptr<MessageCenterSource>> m_sources;
    QMap<QString, MessageEndpoint> m_endpoints;
    QMap<QString, QString> m_endpointChannels;  //!< The channel (device set:channel index) of each endpoint key
    std::vector<MessageCenterPage> m_pages;
    QStringList m_sinkErrors;
    QMap<QString, SinkStat> m_sinkStats;

    void store(const MessageRecord& record);
    void deliver(const MessageRecord& record, const QString& sinkName, const QStringList& captures, qint64 now);
    MessageCenterPage *findPage(const QString& id);
    void endpointGone(const QString& alias, qint64 now);
    bool markLogged(const QString& id, const QString& state, const QString& reason);
};

#endif // INCLUDE_MESSAGECENTERENGINE_H
