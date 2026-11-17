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

// Self test of the message center engine, not built by default:
//   cmake --build <build dir> --target messagecenterenginetest

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QUdpSocket>

#include <cstdio>

#include "radiopagingpagerformat.h"
#include "messagecenterengine.h"
#include "messagecenterarchive.h"
#include "messagecentersettings.h"

namespace {

int passed = 0;
int failed = 0;

void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? passed : failed)++;
}

MessageRecord rx(const QString& id, const QString& protocol, const QString& address, const QString& text, const QString& alias,
    int speed = 1200, MessageRecord::Polarity polarity = MessageRecord::PolarityNormal)
{
    MessageRecord r;
    r.m_id = id;
    r.m_timestamp = 1000;
    r.m_direction = MessageRecord::DirectionRX;
    r.m_kind = MessageRecord::KindMessage;
    r.m_protocol = protocol;
    r.m_address = address;
    r.m_text = text;
    r.m_msgType = "alphanumeric";
    r.m_channel = alias;
    r.m_speed = speed;
    r.m_polarity = polarity;
    r.m_frequency = 466000000;
    return r;
}

MessageEndpoint transmitter(const QString& alias, const QStringList& protocols)
{
    MessageEndpoint e;
    e.m_channel = alias;
    e.m_role = MessageEndpoint::Transmitter;
    e.m_status = MessageEndpoint::Idle;
    e.m_protocols = protocols;
    return e;
}

MessageRecord pageRecord(const QString& transmitter, const QString& protocol, const QString& address, const QString& text)
{
    MessageRecord r = rx(QString(), protocol, address, text, QString());
    r.m_direction = MessageRecord::DirectionTX;
    r.m_protocolData.insert("transmitter", transmitter);
    return r;
}

MessageRecord event(const QString& id, const QString& state, const QString& reason = QString())
{
    MessageRecord r;
    r.m_id = id;
    r.m_kind = MessageRecord::KindEvent;
    r.m_protocolData.insert("event", "page");
    r.m_protocolData.insert("state", state);
    r.m_protocolData.insert("reason", reason);
    return r;
}

void testFilter()
{
    MessageRecord a = rx("1", "pocsag", "0001234A", "HELLO WORLD", "dsA");
    MessageRecord b = rx("2", "flex", "1234567", "other", "dsB", 3200, MessageRecord::PolarityInverted);
    b.m_duplicate = MessageRecord::DuplicateDup;
    MessageCenterFilter f;

    check(f.matches(a) && f.matches(b) && f.isEmpty(), "Filter: empty selects everything");
    f.m_protocols = QStringList({"pocsag"});
    check(f.matches(a) && !f.matches(b), "Filter: protocol");
    f = MessageCenterFilter();
    f.m_address = "0001.*";
    check(f.matches(a) && !f.matches(b), "Filter: address, a regular expression on the whole address");
    f.m_address = "0001";
    check(!f.matches(a), "Filter: the expression must match the whole address");
    f.m_address = "([";
    check(!f.matches(a) && !f.errors().isEmpty(), "Filter: an address expression that is not valid matches nothing");
    f = MessageCenterFilter();
    f.m_text = "hello";
    check(f.matches(a) && !f.matches(b), "Filter: text, a regular expression found anywhere in it");
    f = MessageCenterFilter();
    f.m_polarity = (int) MessageRecord::PolarityInverted;
    f.m_speeds = {3200, 6400};
    f.m_channels = QStringList({"dsB"});
    check(!f.matches(a) && f.matches(b), "Filter: polarity, speed and channel together");
    f = MessageCenterFilter();
    f.m_hideDuplicates = true;
    check(f.matches(a) && !f.matches(b), "Filter: hide duplicates");
    f = MessageCenterFilter();
    f.m_frequencyMin = 465000000;
    f.m_frequencyMax = 467000000;
    f.m_direction = (int) MessageRecord::DirectionRX;
    check(f.matches(a) && f.matches(b), "Filter: frequency range and direction");
    f.m_frequencyMin = 470000000;
    check(!f.matches(a), "Filter: frequency below the range");

    MessageCenterFilter g;
    g.m_protocols = QStringList({"gsc", "flex"});
    g.m_text = "x y";
    g.m_voiceOnly = true;
    g.m_timeFrom = 5;
    MessageCenterFilter h = MessageCenterFilter::fromJson(g.toJson());
    check((h.m_protocols == g.m_protocols) && (h.m_text == g.m_text) && h.m_voiceOnly && (h.m_timeFrom == 5) && (h.m_timeTo == -1),
        "Filter: JSON round trip");
}

void testLogAndRoutes()
{
    MessageCenterEngine engine;
    QTemporaryDir directory;
    QString jsonPath = directory.path() + "/out.jsonl";
    QString csvPath = directory.path() + "/out.csv";

    engine.addSink(std::unique_ptr<MessageCenterSink>(new MessageCenterFileSink("json", jsonPath, MessageCenterFileSink::JsonLines)));
    engine.addSink(std::unique_ptr<MessageCenterSink>(new MessageCenterFileSink("csv", csvPath, MessageCenterFileSink::Csv)));

    // The default route: the log. Routes: flex to the csv file and stop (not logged); everything else to the log and the
    // json file
    MessageCenterEngine::Route flex;
    flex.m_name = "flex to csv";
    flex.m_filter.m_protocols = QStringList({"flex"});
    flex.m_sinks = QStringList({"csv"});
    flex.m_stop = true;
    MessageCenterEngine::Route all;
    all.m_name = "the rest";
    all.m_sinks = QStringList({"log", "json"});

    engine.setRoutes(MessageCenterEngine::defaultRoutes());
    engine.ingest(rx("1", "pocsag", "1", "no route", "A"), "channels", 1);
    check((engine.log().size() == 1) && !QFile::exists(jsonPath), "Engine: the default route logs a record");

    engine.setRoutes({flex, all});
    engine.ingest(rx("2", "flex", "2", "to csv", "A", 3200), "channels", 2);
    engine.ingest(rx("3", "gsc", "3", "to log and json", "A", 600), "channels", 3);
    QFile csv(csvPath);
    QFile json(jsonPath);
    csv.open(QIODevice::ReadOnly);
    json.open(QIODevice::ReadOnly);
    QList<QByteArray> csvLines = csv.readAll().split('\n');
    QList<QByteArray> jsonLines = json.readAll().split('\n');
    check((engine.log().size() == 2) && !engine.find("2") && engine.find("3") && (csvLines.size() == 3) && (jsonLines.size() == 2),
        "Engine: routes send a record to their sinks, a route can stop the others and keep it out of the log");

    // A record decoded again replaces the one of its id in the log, and the log tells it changed
    MessageRecord again = *engine.find("3");
    again.m_text = "DECODED AGAIN";
    const int loggedBefore = engine.logged();
    check(engine.replace(again) && (engine.find("3")->m_text == "DECODED AGAIN") && (engine.logged() > loggedBefore)
        && (engine.log().size() == 2), "Engine: a record decoded again replaces its record");
    again.m_id = "none";
    check(!engine.replace(again) && (engine.log().size() == 2), "Engine: a record decoded again without its record is not added");

    // A voice message is updated by a record of the same id; a source file gives the lines written to it
    MessageRecord voice = rx("v1", "gsc", "100123", "", "A", 600);
    voice.m_msgType = "voice";
    voice.m_partial = true;
    engine.ingest(voice, "channels", 4);
    MessageRecord update = rx("v1", "gsc", "100123", "", "A", 600);
    update.m_msgType = "voice";
    update.m_protocolData.insert("voice_file", "/x/a_rx_1.wav");
    update.m_protocolData.insert("voice_seconds", 4.0);
    update.m_timestamp = 5000;
    engine.ingest(update, "channels", 5);
    const MessageRecord *merged = engine.find("v1");
    MessageCenterFilter voiceOnly;
    voiceOnly.m_voiceOnly = true;
    check(merged && !merged->m_partial && (merged->m_timestamp == 1000) && (merged->m_protocolData.value("voice_file").toString() == "/x/a_rx_1.wav")
        && (engine.select(voiceOnly).size() == 1), "Engine: an update completes the record of the same id");

    MessageCenterEngine other;
    other.addSource(std::unique_ptr<MessageCenterSource>(new MessageCenterFileSource("import", jsonPath)));
    QStringList errors;
    int taken = other.pollSources(errors);
    check((taken >= 1) && errors.isEmpty() && other.find("3"), "Engine: a source file is read and its records ingested");

    QFile append(jsonPath);
    append.open(QIODevice::Append);
    append.write(rx("9", "pocsag", "9", "later", "A").toJsonLine());
    append.write("not json\n");
    append.write("{\"half\":");        // Not complete: not read yet
    append.close();
    QStringList errors2;
    int more = other.pollSources(errors2);
    check((more == 1) && other.find("9"), "Engine: a source reads only the lines added, a line being written waits");

    other.setLogSize(2);
    other.ingest(rx("a", "pocsag", "1", "a", "A"), "x", 1);
    other.ingest(rx("b", "pocsag", "1", "b", "A"), "x", 1);
    other.ingest(rx("c", "pocsag", "1", "c", "A"), "x", 1);
    check((other.log().size() == 2) && other.find("c") && !other.find("a"), "Engine: the log is bounded");
}

void testEndpoints()
{
    MessageCenterEngine engine;
    MessageEndpoint tx = transmitter("tx1", {"pocsag", "gsc"});
    tx.m_speeds = {1200};
    tx.m_queueDepth = 1;
    tx.m_capacity = 4;
    tx.m_capabilities.insert("maxText", 80);
    MessageRecord reg = MessageEndpoint::toRecord(tx, "register");
    engine.ingest(reg, "channels", 1);

    MessageEndpoint e;
    QString event;
    check(MessageEndpoint::fromRecord(reg, e, event) && (event == "register") && (e.m_channel == "tx1") && (e.m_speeds == QList<int>({1200}))
        && (e.m_capabilities.value("maxText").toInt() == 80), "Endpoint: the registration record round trip");
    check(engine.endpoints().contains("tx:tx1") && engine.endpoints()["tx:tx1"].ready(), "Endpoint: registered, ready with room in its queue");

    tx.m_status = MessageEndpoint::Busy;
    tx.m_queueDepth = 4;
    engine.ingest(MessageEndpoint::toRecord(tx, "status"), "channels", 2);
    check((engine.endpoints()["tx:tx1"].m_status == MessageEndpoint::Busy) && !engine.endpoints()["tx:tx1"].ready()
        && (engine.endpoints()["tx:tx1"].m_speeds == QList<int>({1200})), "Endpoint: a status record changes the status and the queue");
    // The protocol of the channel changed: its next status has the new one
    const QStringList protocols = tx.m_protocols;
    tx.m_protocols = QStringList({"flex"});
    tx.m_status = MessageEndpoint::Idle;
    tx.m_queueDepth = 0;
    engine.ingest(MessageEndpoint::toRecord(tx, "status"), "channels", 2);
    check(engine.endpoints()["tx:tx1"].canTransmit(pageRecord("tx1", "flex", "5", "x")), "Endpoint: a status record changes the protocols");
    tx.m_protocols = protocols;
    tx.m_status = MessageEndpoint::Busy;
    tx.m_queueDepth = 4;
    engine.ingest(MessageEndpoint::toRecord(tx, "status"), "channels", 2);

    MessageRecord flexPage = pageRecord("tx1", "flex", "5", "x");
    MessageRecord slow = pageRecord("tx1", "pocsag", "5", "x");
    slow.m_speed = 512;
    MessageRecord good = pageRecord("tx1", "pocsag", "5", "x");
    good.m_speed = 1200;
    check(!engine.endpoints()["tx:tx1"].canTransmit(flexPage) && !engine.endpoints()["tx:tx1"].canTransmit(slow) && engine.endpoints()["tx:tx1"].canTransmit(good)
        && (engine.eligible(good) == QStringList({"tx1"})), "Endpoint: protocol and speed decide what it can transmit");

    MessageRecord gone = MessageEndpoint::toRecord(tx, "unregister");
    engine.ingest(gone, "channels", 3);
    check(engine.eligible(good).isEmpty(), "Endpoint: unregistered, nothing can be sent to it");
}

void testPages()
{
    MessageCenterEngine engine;
    MessageRecord p = pageRecord("tx1", "pocsag", "1234", "HELLO");
    p.m_speed = 1200;
    QString id = engine.addPage(p, 100);

    check(engine.page(id) && (engine.page(id)->m_state == MessageCenterPage::Pending) && engine.dispatch(101).empty(),
        "Page: pending while no transmitter can take it");
    check(engine.find(id) && (engine.find(id)->m_direction == MessageRecord::DirectionTX), "Page: also in the log");
    const int versionBefore = engine.logged();
    check((engine.syncLoggedPages() == 1) && (engine.find(id)->m_protocolData.value("page_state").toString() == "pending")
        && (engine.find(id)->m_protocolData.value("page_reason").toString() == "waiting: no transmitter tx1") && (engine.syncLoggedPages() == 0),
        "Page: its record in the log has its state and reason, changed once");
    {
        // The views take the changes since the version they show, or show all again
        QStringList ids;
        check(engine.changedSince(versionBefore, ids) && (ids == QStringList({id})), "Log: a page state change is a change of its record");
        check(engine.changedSince(engine.logged(), ids) && ids.isEmpty(), "Log: no change since the last version");
        check(!engine.changedSince(-5, ids), "Log: a version older than the changes kept: all again");
    }

    MessageEndpoint tx = transmitter("tx1", {"pocsag"});
    tx.m_capacity = 2;
    engine.ingest(MessageEndpoint::toRecord(tx, "register"), "channels", 110);
    std::vector<MessageCenterPage> out = engine.dispatch(120);
    check((out.size() == 1) && (engine.page(id)->m_state == MessageCenterPage::Queued) && engine.page(id)->m_dispatched,
        "Page: queued once a transmitter exists, dispatched when it is ready");
    check(engine.dispatch(121).empty(), "Page: not dispatched twice");
    check(engine.page(id)->m_reason.isEmpty(), "Page: no reason once queued");

    // A page waits with the reason: no transmitter of its name, or one of another protocol
    QString lost = engine.addPage(pageRecord("tx9", "pocsag", "5", "x"), 122);
    QString flex = engine.addPage(pageRecord("tx1", "flex", "1100009", "x"), 122);
    engine.dispatch(123);
    check(engine.page(lost)->m_reason == "waiting: no transmitter tx9", "Page: waits for its transmitter, with the reason");
    check((engine.page(flex)->m_state == MessageCenterPage::Pending) && (engine.page(flex)->m_reason == "waiting: tx1 sends pocsag now, not flex"),
        "Page: waits for a transmitter of its protocol, with the reason");
    engine.cancel(lost);
    engine.cancel(flex);
    check(engine.find(lost) && (engine.find(lost)->m_protocolData.value("page_state").toString() == "cancelled"),
        "Page: a page cancelled is cancelled in the log");

    engine.ingest(event(id, "claimed"), "channels", 130);
    check((engine.page(id)->m_state == MessageCenterPage::Claimed) && (engine.page(id)->m_attempts == 1) && !engine.page(id)->m_dispatched
        && (engine.page(id)->m_claimedBy == "tx1"), "Page: claimed by the transmitter, one attempt");
    engine.ingest(event(id, "transmitted"), "channels", 140);
    check(engine.page(id)->m_state == MessageCenterPage::Transmitted, "Page: transmitted");
    engine.ingest(event(id, "sent"), "channels", 150);
    check(engine.page(id)->m_state == MessageCenterPage::Sent && (engine.removeFinished() == 1) && !engine.page(id), "Page: sent, then forgotten");

    // A page rejected goes to the dead letter queue and can be requeued
    QString rejected = engine.addPage(pageRecord("tx1", "pocsag", "1", "x"), 200);
    engine.dispatch(201);
    engine.ingest(event(rejected, "rejected", "message too long"), "channels", 202);
    check((engine.page(rejected)->m_state == MessageCenterPage::Dead) && engine.page(rejected)->m_reason.startsWith("rejected"),
        "Page: rejected by the transmitter, dead with the reason");
    check(engine.requeue(rejected, 210) && (engine.page(rejected)->m_state == MessageCenterPage::Pending) && (engine.page(rejected)->m_attempts == 0),
        "Page: requeued from the dead letter queue");

    // An aborted transmission is retransmitted twice, then the page is dead
    QString retry = engine.addPage(pageRecord("tx1", "pocsag", "2", "retry"), 300);
    bool ok = true;

    for (int attempt = 1; attempt <= MessageCenterEngine::m_maxAttempts; attempt++)
    {
        // Pending pages of the earlier tests may be dispatched first: only this page is followed
        engine.dispatch(300 + attempt * 10);
        ok = ok && engine.page(retry)->m_dispatched;
        engine.ingest(event(retry, "claimed"), "channels", 301 + attempt * 10);
        engine.ingest(event(retry, "transmitted"), "channels", 302 + attempt * 10);
        engine.ingest(event(retry, "aborted"), "channels", 303 + attempt * 10);
    }

    check(ok && (engine.page(retry)->m_state == MessageCenterPage::Dead) && engine.page(retry)->m_reason.startsWith("attempts")
        && (engine.page(retry)->m_attempts == MessageCenterEngine::m_maxAttempts) && (engine.page(retry)->m_retransmits == 2),
        "Page: retransmitted after a failure, dead when the attempts are used");

    // Claim not answered: back to the queue, an attempt, dead at the limit
    QString slow = engine.addPage(pageRecord("tx1", "pocsag", "3", "slow"), 500);
    engine.dispatch(500);
    engine.expireClaims(500 + 5000, 10000);
    bool notYet = engine.page(slow)->m_dispatched;
    engine.expireClaims(500 + 10000, 10000);
    check(notYet && !engine.page(slow)->m_dispatched && (engine.page(slow)->m_attempts == 1) && (engine.page(slow)->m_state == MessageCenterPage::Queued),
        "Page: a claim that does not come in time sends the page back to the queue");

    // The transmitter goes away: claimed pages wait for it, a page being transmitted is interrupted
    QString claimed = engine.addPage(pageRecord("tx2", "pocsag", "4", "claimed"), 600);
    QString onAir = engine.addPage(pageRecord("tx2", "pocsag", "5", "on air"), 600);
    MessageEndpoint tx2 = transmitter("tx2", {"pocsag"});
    engine.ingest(MessageEndpoint::toRecord(tx2, "register"), "channels", 600);
    engine.dispatch(601);
    engine.ingest(event(claimed, "claimed"), "channels", 602);
    engine.ingest(event(onAir, "claimed"), "channels", 602);
    engine.ingest(event(onAir, "transmitted"), "channels", 603);
    engine.ingest(MessageEndpoint::toRecord(tx2, "unregister"), "channels", 610);
    check((engine.page(claimed)->m_state == MessageCenterPage::Pending) && (engine.page(onAir)->m_state == MessageCenterPage::Queued)
        && (engine.page(onAir)->m_retransmits == 1), "Page: a transmitter that goes away releases its claims and interrupts what it was sending");

    // Capacity: the pages are dispatched as far as the transmitter has room
    MessageCenterEngine capacity;
    MessageEndpoint small = transmitter("small", {"pocsag"});
    small.m_capacity = 2;
    capacity.ingest(MessageEndpoint::toRecord(small, "register"), "x", 1);

    for (int i = 0; i < 5; i++) {
        capacity.addPage(pageRecord("small", "pocsag", QString::number(i), "x"), 1);
    }

    check((capacity.dispatch(2).size() == 2) && capacity.dispatch(3).empty() && capacity.pages(MessageCenterPage::Queued).size() == 5,
        "Page: no more pages are dispatched than the transmitter has room for");

    // A page for a transmitter that is not there is withdrawn only while nobody has it
    QString lonely = capacity.addPage(pageRecord("nobody", "pocsag", "1", "x"), 5);
    check(capacity.cancel(lonely) && !capacity.page(lonely), "Page: a page nobody claimed can be withdrawn");
}

// Reports that come late or twice, and endpoints that share an alias
// Waits for a datagram on the socket, up to a second
bool waitDatagram(QUdpSocket& socket, QByteArray& datagram)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < 1000)
    {
        QCoreApplication::processEvents();

        if (socket.hasPendingDatagrams())
        {
            datagram.resize(socket.pendingDatagramSize());
            socket.readDatagram(datagram.data(), datagram.size());
            return true;
        }

        QThread::msleep(5);
    }

    return false;
}

void testRegExp()
{
    MessageCenterFilter f;
    MessageRecord r = rx("a", "pocsag", "0001234A", "Fire at 12 Main St", "ch1");

    r.m_protocolData.insert("ric", 1234);
    r.m_protocolData.insert("function", 0);

    // The text: found anywhere, not case sensitive, its captures those of the actions
    f.m_text = "at (\\d+)";
    check(f.matches(r), "the expression of the text is found anywhere in it");
    check(f.captures(r) == QStringList({"at 12", "12"}), "captures of the expression of the text");
    f.m_text = "fire";
    check(f.matches(r), "the text is not case sensitive");
    f.m_text = "^main";
    check(!f.matches(r) && f.captures(r).isEmpty(), "^ anchors it at the start");
    f.m_text = "([";
    check(!f.matches(r) && !f.errors().isEmpty(), "a text expression that is not valid matches nothing and says why");
    f.m_text.clear();

    // One more field: any field of the record or of its protocol data, its whole value
    f.m_field = "ric";
    f.m_value = "12..";
    check(f.matches(r), "a field of the protocol data, its whole value");
    f.m_value = "123";
    check(!f.matches(r), "the whole value: a part does not match");
    f.m_field = "channel";
    f.m_value = "ch\\d";
    check(f.matches(r), "a field of the record");
    f.m_field = "capcode";
    f.m_value = ".*";
    check(!f.matches(r), "a field the record does not have: no match");
    f.m_field = "ric";
    f.m_value = "([";
    check(!f.matches(r) && !f.errors().isEmpty(), "a field expression that is not valid matches nothing and says why");

    f.m_value = "1234";
    f.m_text = "fire";
    MessageCenterFilter g = MessageCenterFilter::fromJson(f.toJson());
    check((g.m_field == "ric") && (g.m_value == "1234") && (g.m_text == "fire") && g.matches(r), "text and field in the JSON of the filter");
    check(!MessageCenterFilter().isEmpty() || MessageCenterFilter().toJson().isEmpty(), "nothing set, empty filter");

    MessageRecord q = rx("b", "flex", "000999", "${0} text", "c");
    check(messageCenterSubstitute("${address}: ${message} (${protocol}/${type}/${channel}) ${1} ${2}", q, QStringList({"x", "y"}))
        == "000999: ${0} text (flex/alphanumeric/c) y ", "substitutions in one pass");
    check(messageCenterSubstitute("${unknown} ${", q, QStringList()) == "${unknown} ${", "an unknown name stays as it is");
}

void testCommand()
{
    QList<QStringList> launched;
    MessageCenterCommandSink sink("cmd", "notify --to ops --text \"${message} from ${address}\" ${1}",
        [&](const QString& program, const QStringList& arguments) {
            launched.append(QStringList(program) + arguments);
            return true;
        });
    MessageRecord r = rx("a", "pocsag", "0001234A", "x\" -rf / ; echo $(id) ${0}", "ch1");
    QString error;

    check(sink.accept(r, QStringList({"all", "cap one"}), error), "command is run");
    check(launched.size() == 1 && launched[0].size() == 6, "the message does not add arguments");

    if (!launched.isEmpty())
    {
        check(launched[0][0] == "notify", "the program is the one set");
        check(launched[0][4] == "x\" -rf / ; echo $(id) ${0} from 0001234A", "the text of the message is one argument, as it is");
        check(launched[0][5] == "cap one", "a capture is one argument");
    }

    MessageRecord event = r;
    event.m_kind = MessageRecord::KindEvent;
    check(sink.accept(event, error) && launched.size() == 1, "an event is not given to the command");

    for (int i = 0; i < 10; i++) {
        sink.accept(r, QStringList({"a", "b"}), error);
    }

    check(launched.size() == MessageCenterCommandSink::m_maxPerSecond, "commands per second are limited");
    check(!sink.accept(r, QStringList(), error) && error.contains("commands in a second"), "a command over the limit is refused");

    MessageCenterCommandSink empty("none", "   ");
    check(!empty.accept(r, error), "an empty command is refused");
}

void testActionSink()
{
    QString type, text;
    quint32 color = 0;
    int calls = 0;
    MessageCenterActionSink sink("act", "speech", "${message} at ${address}", 0xff00ff00,
        [&](const QString& t, const MessageRecord&, const QString& tx, quint32 c) {
            type = t;
            text = tx;
            color = c;
            calls++;
        });
    QString error;
    MessageRecord r = rx("a", "pocsag", "0001234A", "hello", "ch1");

    check(sink.accept(r, error) && (type == "speech") && (text == "hello at 0001234A") && (color == 0xff00ff00), "action gets the text and the color");

    MessageRecord update = r;
    update.m_text.clear();
    update.m_protocolData.insert("voice_end", "turn_off");
    sink.accept(update, error);
    check(calls == 1, "the end of a voice message does not repeat the action");
}

void testRoutesWithActions()
{
    MessageCenterEngine engine;
    int calls = 0;
    QStringList seen;
    engine.addSink(std::unique_ptr<MessageCenterSink>(new MessageCenterActionSink("alert", "speech", "${1}", 0,
        [&](const QString&, const MessageRecord&, const QString& text, quint32) {
            calls++;
            seen.append(text);
        })));

    MessageCenterEngine::Route route;
    route.m_name = "fire";
    route.m_filter.m_text = "^Fire (\\w+)";
    route.m_filter.m_hideDuplicates = true;
    route.m_sinks = QStringList({"alert"});
    engine.setRoutes({MessageCenterEngine::defaultRoutes()[0], route});

    engine.ingest(rx("1", "pocsag", "1", "Fire alpha", "c"), "channels", 1);
    engine.ingest(rx("2", "pocsag", "1", "Flood", "c"), "channels", 2);
    MessageRecord dup = rx("3", "pocsag", "1", "Fire beta", "c");
    dup.m_duplicate = MessageRecord::DuplicateDup;
    engine.ingest(dup, "channels", 3);

    check(calls == 1 && seen == QStringList({"alpha"}), "the route runs the action on a message that matches, with its capture");
    // All three are logged by the default route
    check(engine.log().size() == 3, "the messages the route does not select are logged");
    check(engine.sinkStats().value("alert").m_accepted == 1 && engine.sinkStats().value("log").m_accepted == 3, "sink statistics");

    engine.importRecord(rx("9", "pocsag", "1", "Fire gamma", "c"));
    check(calls == 1 && engine.log().size() == 4, "an imported message is logged and not routed");
}

// All the routes that select a message act on it, in their order, until one that stops; the source is part of the filter
void testRouteOrderAndSources()
{
    MessageCenterEngine engine;
    QStringList order;
    auto recorder = [&order](const QString& name) {
        return std::unique_ptr<MessageCenterSink>(new MessageCenterActionSink(name, "speech", name, 0,
            [&order](const QString&, const MessageRecord&, const QString& text, quint32) { order.append(text); }));
    };
    engine.addSink(recorder("first"));
    engine.addSink(recorder("second"));
    engine.addSink(recorder("third"));
    engine.addSink(recorder("udp only"));

    MessageCenterEngine::Route r1, r2, r3, r4;
    r1.m_name = "1";
    r1.m_sinks = QStringList({"first"});
    r2.m_name = "2";
    r2.m_filter.m_protocols = QStringList({"flex"});
    r2.m_sinks = QStringList({"second"});
    r2.m_stop = true;
    r3.m_name = "3";
    r3.m_sinks = QStringList({"third"});
    r4.m_name = "4";
    r4.m_filter.m_sources = QStringList({"udp in"});
    r4.m_sinks = QStringList({"udp only"});
    engine.setRoutes({r1, r2, r3, r4});

    engine.ingest(rx("p", "pocsag", "1", "x", "c"), "channels", 1);
    check(order == QStringList({"first", "third"}), "every route that selects a message acts, in order");
    order.clear();
    engine.ingest(rx("f", "flex", "1", "x", "c"), "channels", 2);
    check(order == QStringList({"first", "second"}), "a route that stops ends the check");
    order.clear();
    engine.ingest(rx("u", "pocsag", "1", "x", "c"), "udp in", 3);
    check(order == QStringList({"first", "third", "udp only"}), "a route can select the source");
    check(engine.log().empty(), "without a route to the log nothing is logged");

    // The log is a sink: the routes stopped before it keep a message out of it
    r3.m_sinks = QStringList({"third", "log"});
    engine.setRoutes({r1, r2, r3, r4});
    engine.ingest(rx("q", "pocsag", "1", "x", "c"), "channels", 4);
    engine.ingest(rx("g", "flex", "1", "x", "c"), "channels", 5);
    check(engine.find("q") && !engine.find("g"), "a message goes to the log through a route; one stopped before it is not logged");

    engine.setRoutes(MessageCenterEngine::defaultRoutes());
    engine.ingest(rx("d", "gsc", "1", "x", "c"), "channels", 6);
    check(engine.find("d") && (engine.routes().size() == 1) && (engine.routes()[0].m_sinks == QStringList("log"))
        && engine.routes()[0].m_filter.isEmpty(), "the default route sends every message to the log");

    MessageCenterFilter f = MessageCenterFilter::fromJson(r4.m_filter.toJson());
    check(f.m_sources == QStringList({"udp in"}), "the sources of a filter in its JSON");
    check(f.matches(rx("z", "pocsag", "1", "x", "c")), "matching without a source looks at the record only");
}

// A filter on an address also selects the group messages of which it is a member
// Marks: written into the record before it is routed, so the log has them; the routes in order, until one that stops
void testMarks()
{
    MessageCenterEngine engine;
    engine.addSink(std::unique_ptr<MessageCenterSink>(new MessageCenterMarkSink("fire", ":fire_engine", 0, 0xff401010)));
    engine.addSink(std::unique_ptr<MessageCenterSink>(new MessageCenterMarkSink("accident", ":collision", 0xffffcc00, 0)));

    MessageCenterEngine::Route fire;
    fire.m_name = "Fire";
    fire.m_filter.m_text = "brand|fire";
    fire.m_sinks = QStringList("fire");
    MessageCenterEngine::Route accident;
    accident.m_name = "Accident";
    accident.m_filter.m_text = "ongeval";
    accident.m_sinks = QStringList("accident");
    engine.setRoutes({MessageCenterEngine::defaultRoutes()[0], fire, accident});

    engine.ingest(rx("1", "flex", "1", "P 1 Brand woning, ongeval", "c"), "channels", 1);
    engine.ingest(rx("2", "flex", "1", "A1 Ambu", "c"), "channels", 2);
    engine.ingest(rx("3", "flex", "1", "Fire alarm", "c"), "channels", 3);

    const MessageRecord *both = engine.find("1");
    const MessageRecord *none = engine.find("2");
    const MessageRecord *one = engine.find("3");
    const QVariantList marks = both ? both->m_protocolData.value("marks").toList() : QVariantList();
    check((marks.size() == 2) && (marks[0].toMap().value("icon").toString() == ":fire_engine")
        && (marks[0].toMap().value("background").toUInt() == 0xff401010) && !marks[0].toMap().contains("color")
        && (marks[1].toMap().value("icon").toString() == ":collision") && (marks[1].toMap().value("color").toUInt() == 0xffffcc00),
        "a message two routes mark has both marks in the log, in the order of the routes, with what each sets");
    check(none && !none->m_protocolData.contains("marks"), "a message no route marks has none");
    check(one && (one->m_protocolData.value("marks").toList().size() == 1), "a message one route marks has one");

    // A route that stops ends the marks too
    fire.m_stop = true;
    engine.setRoutes({MessageCenterEngine::defaultRoutes()[0], fire, accident});
    engine.ingest(rx("4", "flex", "1", "P 1 Brand, ongeval", "c"), "channels", 4);
    const MessageRecord *stopped = engine.find("4");
    check(stopped && (stopped->m_protocolData.value("marks").toList().size() == 1), "the routes after one that stops do not mark");

    // A mark sink sets something
    MessageCenterFileSetting setting;
    setting.m_name = "m";
    setting.m_type = "mark";
    check(!setting.check(true).isEmpty(), "a mark without an icon or a colour is refused");
    setting.m_background = 0xff000080;
    check(setting.check(true).isEmpty(), "a mark with a colour of the row only is valid");
    QList<MessageCenterFileSetting> back = MessageCenterSettings::filesFromJson(MessageCenterSettings::filesToJson({setting}));
    check((back.size() == 1) && (back[0].m_background == 0xff000080) && (back[0].m_color == 0) && back[0].m_icon.isEmpty(),
        "a mark in the JSON of the sinks: what it sets only");
}

// The presets: Empty is a new message center; Emergency marks the messages of a kind, its sinks those its routes send to
void testPresets()
{
    std::vector<MessageCenterEngine::Route> routes;
    QList<MessageCenterFileSetting> sinks;
    check(MessageCenterSettings::preset("Empty", routes, sinks) && (routes.size() == 1) && sinks.isEmpty()
        && (routes[0].m_sinks == QStringList("log")), "the Empty preset: every message to the log");
    check(!MessageCenterSettings::preset("None such", routes, sinks), "a preset that does not exist");
    check(MessageCenterSettings::preset("Emergency", routes, sinks) && (routes.size() == (size_t) sinks.size() + 1),
        "the Emergency preset: the log and a route for each of its marks");

    bool valid = true;
    MessageCenterEngine engine;

    for (const MessageCenterFileSetting& sink : sinks)
    {
        valid = valid && sink.check(true).isEmpty() && (sink.m_type == "mark") && !sink.m_icon.isEmpty();
        engine.addSink(std::unique_ptr<MessageCenterSink>(new MessageCenterMarkSink(sink.m_name, sink.m_icon, sink.m_color, sink.m_background)));
    }

    check(valid, "its sinks are valid marks with an icon");
    engine.setRoutes(routes);
    engine.ingest(rx("p", "flex", "1", "P 1 BDH-01 Brand woning, Ongeval", "c"), "channels", 1);
    engine.ingest(rx("t", "flex", "1", "TESTOPROEP MOB", "c"), "channels", 2);
    engine.ingest(rx("x", "flex", "1", "Test", "c"), "channels", 3);
    auto icons = [&engine](const char *id) {
        QStringList out;
        const MessageRecord *r = engine.find(id);
        for (const QVariant& m : r ? r->m_protocolData.value("marks").toList() : QVariantList()) {
            out << m.toMap().value("icon").toString();
        }
        return out;
    };
    check(icons("p") == QStringList({":fire_engine", ":collision"}), "a fire accident gets the icons of fire and of an accident");
    check(icons("t") == QStringList(":satellite_antenna"), "a test call gets the icon of a test");
    check(icons("x").isEmpty(), "the word test alone is not a test page");
}

void testGroupMembers()
{
    MessageRecord group = rx("g", "flex", "0002029570", "group text", "c");
    group.m_protocolData.insert("group_slot", 2);
    group.m_protocolData.insert("group_capcodes", QVariantList({"0000002020", "0002101249"}));
    MessageCenterFilter f;
    f.m_address = "0002101249";
    check(f.matches(group) && !f.matches(rx("o", "flex", "0002029570", "other", "c")), "an address filter selects a group message to a member");
    MessageCenterFilter r;
    r.m_field = "group_capcodes";
    r.m_value = "0*2020";
    check(f.matches(group) && r.matches(group), "a field that is a list: one of its values (the members of a group)");
}

// The channels: one entry for a channel whose alias changed; removed by key or when offline
void testEndpointRemoval()
{
    MessageCenterEngine engine;
    auto announce = [&engine](const QString& alias, int channel, MessageEndpoint::Status status, qint64 now) {
        MessageEndpoint e;
        e.m_channel = alias;
        e.m_role = MessageEndpoint::Receiver;
        e.m_status = status;
        e.m_protocols = QStringList({"pocsag"});
        MessageRecord r = MessageEndpoint::toRecord(e, "status");
        r.m_deviceSetIndex = 0;
        r.m_channelIndex = channel;
        engine.ingest(r, "channels", now);
    };

    announce("150000000", 0, MessageEndpoint::Idle, 1);
    announce("150025000", 0, MessageEndpoint::Idle, 2);
    check((engine.endpoints().size() == 1) && engine.endpoints().contains("rx:150025000"),
        "a channel whose alias changed has one entry, of its new alias");
    announce("151000000", 1, MessageEndpoint::Offline, 3);
    announce("152000000", 2, MessageEndpoint::Idle, 4);
    check(engine.removeEndpoints(QStringList(), true, 5) == 1, "the channels offline are removed");
    check((engine.removeEndpoints(QStringList("rx:152000000"), false, 6) == 1) && (engine.endpoints().size() == 1),
        "a channel is removed by its key");
}

// A route that decodes again: its sinks get the message with its codepage, the routes after it the message as received
void testDecodingRoute()
{
    MessageCenterEngine engine;
    QStringList texts;
    engine.addSink(std::unique_ptr<MessageCenterSink>(new MessageCenterActionSink("after", "speech", "${message}", 0,
        [&texts](const QString&, const MessageRecord&, const QString& text, quint32) { texts.append(text); })));

    const RadioPagingCharset::Codepage *cyrillic = nullptr;

    for (const RadioPagingCharset::Codepage *codepage : RadioPagingCharset::codepages("flex"))
    {
        if (std::string(codepage->m_codepage) == "cyrillic") {
            cyrillic = codepage;
        }
    }

    check(cyrillic != nullptr, "a FLEX Cyrillic codepage");

    if (!cyrillic) {
        return;
    }

    MessageCenterEngine::Route decode, after;
    decode.m_name = "flex 1 cyrillic";
    decode.m_filter.m_protocols = QStringList({"flex"});
    decode.m_filter.m_address = "1";
    decode.m_sinks = QStringList({"log"});
    decode.m_decodeCharset = cyrillic->key();
    after.m_name = "after";
    after.m_sinks = QStringList({"after"});
    engine.setRoutes({decode, after});

    MessageRecord flex = rx("f", "flex", "1", "AB", "c");
    flex.m_protocolData.insert("alpha_codes", QString("4142"));
    engine.ingest(flex, "channels", 1);
    check(engine.find("f") && (engine.find("f")->m_text == QString::fromUtf8("\xD0\xB0\xD0\xB1"))
        && engine.find("f")->m_protocolData.value("redecoded").toBool(), "the sinks of a decoding route get the message decoded");
    check(texts == QStringList({"AB"}), "the routes after it get the message as received");

    // A message that cannot be decoded so (no characters kept) goes as it is
    engine.ingest(rx("g", "flex", "1", "CD", "c"), "channels", 2);
    check(engine.find("g") && (engine.find("g")->m_text == "CD"), "a message that cannot be decoded goes as received");
}

void testUdp()
{
    QUdpSocket receiver;
    check(receiver.bind(QHostAddress(QHostAddress::LocalHost), 0), "a socket to receive on");
    quint16 port = receiver.localPort();
    QByteArray datagram;
    QString error;

    MessageRecord r = rx("a", "pocsag", "0001234A", "HELLO", "ch1");
    r.m_timestamp = 1700000000000LL;
    r.m_protocolData.insert("ric", (qulonglong) 1234);
    r.m_protocolData.insert("function", 0);

    MessageCenterUdpSink pager("udp", "127.0.0.1", port, MessageCenterUdpSink::PagerDemod);
    check(pager.accept(r, error) && waitDatagram(receiver, datagram), "datagram in the Pager format is sent");
    MessageRecord back;
    check(RadioPagingPagerFormat::fromDatagram(datagram, 0, back) && (back.m_address == "0001234") && (back.m_text == "HELLO")
        && (back.m_protocol == "pocsag"), "and read back");

    MessageCenterUdpSink json("udpj", "127.0.0.1", port, MessageCenterUdpSink::Json);
    check(json.accept(r, error) && waitDatagram(receiver, datagram) && datagram.endsWith('\n') && datagram.startsWith('{'), "datagram in JSON is sent");

    MessageRecord update = r;
    update.m_text.clear();
    update.m_protocolData.insert("voice_end", "turn_off");
    check(pager.accept(update, error) && !waitDatagram(receiver, datagram), "nothing is sent for the end of a voice message");

    MessageCenterUdpSink bad("bad", "not an address", port, MessageCenterUdpSink::Json);
    check(!bad.accept(r, error) && !error.isEmpty(), "an address that is not valid is refused");

    // The source: a datagram of each format, and one that is not a message
    QUdpSocket probe;
    probe.bind(QHostAddress(QHostAddress::LocalHost), 0);
    quint16 sourcePort = probe.localPort();
    probe.close();

    MessageCenterUdpSource source("in", "127.0.0.1", sourcePort);
    QUdpSocket sender;
    MessageRecord tx = pageRecord("tx1", "pocsag", "1234", "page");
    tx.m_protocolData.insert("transmitter", "tx1");
    sender.writeDatagram(tx.toJsonLine(), QHostAddress::LocalHost, sourcePort);
    sender.writeDatagram(RadioPagingPagerFormat::datagram(r), QHostAddress::LocalHost, sourcePort);
    sender.writeDatagram(QByteArray("garbage"), QHostAddress::LocalHost, sourcePort);
    std::vector<MessageRecord> records;
    QElapsedTimer timer;
    timer.start();

    while ((records.size() < 2) && (timer.elapsed() < 1000))
    {
        QString pollError;
        source.poll(records, pollError);
        QThread::msleep(5);
    }

    check(records.size() == 2, "UDP source takes the JSON and the Pager datagrams");
    check(source.skipped() == 1, "and counts the datagram that is not a message");

    if (records.size() == 2)
    {
        check(records[0].m_direction == MessageRecord::DirectionRX && !records[0].m_protocolData.contains("transmitter"),
            "what comes from the network is a message received, not a page");
        check(records[1].m_text == "HELLO" && records[1].m_address == "0001234", "Pager datagram becomes a message");
    }

    MessageCenterUdpSource inUse("busy", "127.0.0.1", sourcePort);
    std::vector<MessageRecord> none;
    QString first, second;
    check(!inUse.poll(none, first) && !first.isEmpty(), "a port that cannot be listened on is reported");
    check(inUse.poll(none, second), "once");
}

MessageCenterContact rulesOnly(MessageCenterContact contact)
{
    contact.m_sendTo.clear();
    return contact;
}

// The address book: the addresses of a contact in several protocols, its name on the records, the filters by it
void testContacts()
{
    QString error;

    // Rules of conditions on the fields of the records; the first contact one of whose rules a record meets names it
    // (Duty, the function A of the RIC, before Fire 3, all the functions)
    QString json = "["
        "{\"name\":\"Duty\",\"rules\":["
            "[{\"field\":\"protocol\",\"is\":\"pocsag\"},{\"field\":\"ric\",\"is\":\"1234567\"},{\"field\":\"function\",\"is\":\"0\"}],"
            "[{\"field\":\"protocol\",\"is\":\"gsc\"},{\"field\":\"address\",\"is\":\"012345/2\"}]]},"
        "{\"name\":\"Fire 3\",\"icon\":\":fire_engine\",\"color\":4294901760,\"rules\":["
            "[{\"field\":\"protocol\",\"is\":\"pocsag\"},{\"field\":\"ric\",\"is\":\"1234567\"}],"
            "[{\"field\":\"protocol\",\"is\":\"flex\"},{\"field\":\"capcode\",\"is\":\"2101249\"}],"
            "[{\"field\":\"group_capcodes\",\"is\":\"2101249\"}],"
            "[{\"field\":\"protocol\",\"is\":\"flex\"},{\"field\":\"capcode\",\"matches\":\"12345..\"}]]}"
        "]";
    QList<MessageCenterContact> contacts = MessageCenterContact::listFromJson(json.toUtf8(), &error);
    check((contacts.size() == 2) && (contacts[1].m_rules.size() == 4) && (contacts[0].m_rules[0].size() == 3) && error.isEmpty(),
        "Contacts: rules of conditions read from JSON");
    check(MessageCenterContact::listFromJson(MessageCenterContact::listToJson(contacts)).size() == 2, "Contacts: written to JSON and read again");
    check(MessageCenterContact::listFromJson("[{\"name\":\"\"}]", &error).isEmpty() && !error.isEmpty(), "Contacts: a contact without a name is refused");
    check(MessageCenterContact::listFromJson("[{\"name\":\"A\"},{\"name\":\"A\"}]", &error).isEmpty() && error.contains("two"),
        "Contacts: two contacts of the same name are refused");
    check(MessageCenterContact::listFromJson("[{\"name\":\"A\",\"rules\":[[{\"field\":\"ric\",\"matches\":\"12(\"}]]}]", &error).isEmpty()
        && error.contains("A"), "Contacts: a regular expression that is not valid is refused, with the name of the contact");

    MessageCenterEngine engine;
    engine.setContacts(contacts);

    // POCSAG: the fields of the record, numbers as numbers
    MessageRecord a = rx("a", "pocsag", "1234567A", "A", "ch");
    a.m_protocolData.insert("ric", 1234567);
    a.m_protocolData.insert("function", 0);
    MessageRecord d = rx("d", "pocsag", "1234567D", "D", "ch");
    d.m_protocolData.insert("ric", 1234567);
    d.m_protocolData.insert("function", 3);
    const MessageCenterContact *ca = engine.contactOf(a);
    const MessageCenterContact *cd = engine.contactOf(d);
    check(ca && (ca->m_name == "Duty") && cd && (cd->m_name == "Fire 3"), "Contacts: the first contact a record is of: a function, then all of the RIC");

    // FLEX: the capcode (10 digits in the address, a number in the data), a group message by a list field, a match
    MessageRecord f = rx("f", "flex", "0002101249", "F", "ch");
    f.m_protocolData.insert("capcode", 2101249);
    MessageRecord g = rx("g", "flex", "0002029568", "G", "ch");
    g.m_protocolData.insert("capcode", 2029568);
    g.m_protocolData.insert("group_capcodes", QVariantList({"0000002020", "0002101249"}));
    MessageRecord m = rx("m", "flex", "0001234599", "M", "ch");
    m.m_protocolData.insert("capcode", 1234599);
    check(engine.contactOf(f) && (engine.contactOf(f)->m_name == "Fire 3") && engine.contactOf(g) && (engine.contactOf(g)->m_name == "Fire 3")
        && engine.contactOf(m) && (engine.contactOf(m)->m_name == "Fire 3"),
        "Contacts: a FLEX capcode, a group message by one of the values of its list, a regular expression");

    // GSC: the address as the log shows it; another address number is nobody's
    MessageRecord s2 = rx("s2", "gsc", "012345/2", "S", "ch");
    MessageRecord s3 = rx("s3", "gsc", "012345/3", "S", "ch");
    check(engine.contactOf(s2) && (engine.contactOf(s2)->m_name == "Duty") && !engine.contactOf(s3), "Contacts: a field of the record (address)");

    // The log has the name, icon and colour; a route selects by the name
    std::vector<MessageCenterEngine::Route> routes = MessageCenterEngine::defaultRoutes();
    routes[0].m_filter.m_contacts = QStringList({"Fire 3"});
    engine.setRoutes(routes);
    engine.ingest(a, "channels", 100);
    engine.ingest(d, "channels", 101);
    const MessageRecord *logged = engine.find("d");
    check(!engine.find("a") && logged && (logged->m_protocolData.value("contact").toString() == "Fire 3")
        && (logged->m_protocolData.value("contact_icon").toString() == ":fire_engine")
        && (logged->m_protocolData.value("contact_color").toULongLong() == 4294901760ULL),
        "Contacts: a route selects the messages of a contact, the log has its name, icon and colour");
    check(MessageCenterFilter::fromJson(routes[0].m_filter.toJson()).m_contacts == QStringList({"Fire 3"}), "Contacts: the filter keeps them in JSON");

    // Send to: the first one whose conditions the page meets sets its fields (voice by GSC, binary by FLEX, the rest by
    // POCSAG)
    QString sendJson = "[{\"name\":\"Pager\",\"rules\":[[{\"field\":\"ric\",\"is\":\"1234567\"}]],\"sendTo\":["
        "{\"when\":[{\"field\":\"type\",\"is\":\"voice\"}],\"page\":{\"transmitter\":\"tg\",\"protocol\":\"gsc\",\"address\":\"012345\",\"function\":1}},"
        "{\"when\":[{\"field\":\"type\",\"matches\":\"binary|secure\"}],\"page\":{\"transmitter\":\"tf\",\"protocol\":\"flex\",\"address\":\"1100001\"}},"
        "{\"page\":{\"transmitter\":\"tp\",\"protocol\":\"pocsag\",\"address\":\"1234567\",\"function\":3}}]}]";
    QList<MessageCenterContact> pagers = MessageCenterContact::listFromJson(sendJson.toUtf8(), &error);
    check((pagers.size() == 1) && (pagers[0].m_sendTo.size() == 3) && error.isEmpty(), "Send to: read from JSON");
    check(MessageCenterContact::listFromJson(MessageCenterContact::listToJson(pagers))[0].m_sendTo.size() == 3, "Send to: written to JSON and read again");

    QJsonObject page;
    const bool voice = pagers[0].pageFor(QJsonObject({{"contact", "Pager"}, {"type", "voice"}, {"text", "/tmp/v.wav"}}), page);
    check(voice && (page["transmitter"] == "tg") && (page["protocol"] == "gsc") && (page["address"] == "012345") && (page["text"] == "/tmp/v.wav")
        && !page.contains("contact"), "Send to: a voice page goes by GSC, its text kept");
    pagers[0].pageFor(QJsonObject({{"type", "binary"}, {"text", "DEAD"}}), page);
    check((page["transmitter"] == "tf") && (page["protocol"] == "flex"), "Send to: a binary page goes by FLEX (a regular expression)");
    pagers[0].pageFor(QJsonObject({{"type", "alphanumeric"}, {"text", "FIRE"}}), page);
    check((page["transmitter"] == "tp") && (page["protocol"] == "pocsag") && (page["function"] == 3), "Send to: the others by POCSAG (no condition)");
    check(!rulesOnly(pagers[0]).pageFor(QJsonObject({{"type", "tone"}}), page, &error) && error.contains("no send to"),
        "Send to: a contact without send to is refused with the reason");

    check(MessageCenterContact::listFromJson("[{\"name\":\"A\",\"sendTo\":[{\"page\":{\"transmitter\":\"t\",\"protocol\":\"pocsag\",\"address\":\"9999999\"}}]}]",
        &error).isEmpty() && error.contains("POCSAG"), "Send to: an address that is not one of its protocol is refused (the check of the pages)");
    check(MessageCenterContact::listFromJson("[{\"name\":\"A\",\"sendTo\":[{\"page\":{\"protocol\":\"pocsag\",\"address\":\"1\"}}]}]", &error).isEmpty()
        && error.contains("transmitter"), "Send to: a send to without a transmitter is refused");
}

void testCsvSources()
{
    QTemporaryDir dir;
    check(dir.isValid(), "a temporary directory");

    // A CSV written by the sink is read back by the source: one cell has a comma, quotes and non-Latin letters
    QString path = dir.filePath("log.csv");
    MessageCenterFileSink sink("csv", path, MessageCenterFileSink::Csv);
    QString error;
    MessageRecord r = rx("a", "pocsag", "0001234A", "ALPHA, \"quoted\" \xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82 two  spaces", "ch1");
    r.m_text = QString::fromUtf8("ALPHA, \"quoted\" \xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82 two  spaces");
    check(sink.accept(r, error) && sink.accept(rx("b", "flex", "000999", "FLEX", "ch1"), error), "records written to a CSV");

    MessageCenterCsvSource source("src", path);
    std::vector<MessageRecord> records;
    check(source.poll(records, error) && records.size() == 2, "the CSV source reads them");

    if (records.size() == 2)
    {
        check(records[0].m_text == r.m_text, "text with a comma, quotes, spaces and non-Latin letters is as it was");
        check(records[0].m_id == "a" && records[0].m_address == "0001234A" && records[1].m_protocol == "flex", "fields");
    }

    // Later rows are read by the next poll; a row being written is not
    QFile file(path);
    file.open(QIODevice::Append | QIODevice::Text);
    file.write((rx("c", "gsc", "100123", "GSC", "ch1").toCSV() + "\n").toUtf8());
    file.write(rx("d", "gsc", "100124", "PART", "ch1").toCSV().left(30).toUtf8());
    file.close();
    records.clear();
    check(source.poll(records, error) && records.size() == 1 && records[0].m_id == "c", "rows added are read, a partial row waits");

    // The log of the Pager demodulator
    QString pagerPath = dir.filePath("pager.csv");
    QFile pager(pagerPath);
    pager.open(QIODevice::WriteOnly | QIODevice::Text);
    pager.write("Date,Time,Address,Function Bits,Alpha,Numeric,Even Parity Errors,BCH Parity Errors,Baud,Protocol,Type,Frame\n");
    pager.write("Tue Nov 18 2025,10:20:30,0001234,2,\"HELLO, WORLD\",,0,1,1200,POCSAG,Alpha,\n");
    pager.write("Tue Nov 18 2025,10:20:31,000123456,,,5551234,0,0,0,FLEX,Numeric,001/A\n");
    pager.close();

    MessageCenterCsvSource pagerSource("pager", pagerPath);
    records.clear();
    check(pagerSource.poll(records, error) && records.size() == 2, "the log of the Pager demodulator is read");

    if (records.size() == 2)
    {
        check(records[0].m_protocol == "pocsag" && records[0].m_address == "0001234" && records[0].m_text == "HELLO, WORLD"
            && records[0].m_speed == 1200 && records[0].m_errorsUncorrectable == 1 && records[0].m_protocolData.value("function").toInt() == 2,
            "Pager POCSAG row");
        check(records[1].m_protocol == "flex" && records[1].m_msgType == "numeric" && records[1].m_text == "5551234", "Pager FLEX row");
    }

    // The Pager CSV sink writes that log: its header, its messages only, and it reads back
    QString pagerOutPath = dir.filePath("pager-out.csv");
    MessageCenterFileSink pagerSink("pagercsv", pagerOutPath, MessageCenterFileSink::PagerCsv);
    MessageRecord p = rx("p", "pocsag", "1234568", "HELLO, \"PAGER\"", "ch1");
    p.m_msgType = "alphanumeric";
    p.m_speed = 1200;
    p.m_errorsCorrected = 2;
    p.m_errorsUncorrectable = 1;
    p.m_protocolData["ric"] = 1234568;
    p.m_protocolData["function"] = 3;
    MessageRecord event = rx("e", "flex", "", "", "ch1");
    event.m_kind = MessageRecord::KindEvent;
    check(pagerSink.accept(p, error) && pagerSink.accept(event, error), "Pager CSV: written");
    QFile pagerOut(pagerOutPath);
    pagerOut.open(QIODevice::ReadOnly | QIODevice::Text);
    QStringList lines = QString::fromUtf8(pagerOut.readAll()).split('\n', Qt::SkipEmptyParts);
    pagerOut.close();
    check(lines.size() == 2 && lines[0] == "Date,Time,Address,Function Bits,Alpha,Numeric,Even Parity Errors,BCH Parity Errors,Baud,Protocol,Type,Frame",
        "Pager CSV: the header of the Pager demodulator, then the message only (not the event)");
    MessageCenterCsvSource pagerOutSource("pagerout", pagerOutPath);
    records.clear();
    check(pagerOutSource.poll(records, error) && (records.size() == 1) && (records[0].m_address == "1234568")
        && (records[0].m_text == p.m_text) && (records[0].m_speed == 1200) && (records[0].m_errorsUncorrectable == 1)
        && (records[0].m_protocolData.value("function").toInt() == 3), "Pager CSV: read back as the log of the Pager demodulator");

    // A CSV that is not a log of messages
    QString otherPath = dir.filePath("other.csv");
    QFile other(otherPath);
    other.open(QIODevice::WriteOnly | QIODevice::Text);
    other.write("a,b,c\n1,2,3\n");
    other.close();
    MessageCenterCsvSource otherSource("other", otherPath);
    records.clear();
    QString first, second;
    check(!otherSource.poll(records, first) && !first.isEmpty() && records.empty(), "a CSV that is not a log is refused, saying so");
    check(otherSource.poll(records, second) && records.empty(), "once");
}

void testGuards()
{
    MessageCenterEngine engine;
    MessageEndpoint tx = transmitter("146500000", {"pocsag"});
    MessageEndpoint receiver = tx;
    receiver.m_role = MessageEndpoint::Receiver;       // The alias of a receiver and a transmitter is the frequency: the same
    MessageRecord registration = MessageEndpoint::toRecord(tx, "register");
    registration.m_timestamp = 1;                       // The clock of the test
    engine.ingest(registration, "channels", 1);
    engine.ingest(MessageEndpoint::toRecord(receiver, "register"), "channels", 2);
    check((engine.endpoints().size() == 2) && engine.endpoints()["tx:146500000"].canTransmit(pageRecord("146500000", "pocsag", "1", "x")),
        "Guards: a receiver and a transmitter with the same alias are two endpoints");
    engine.ingest(MessageEndpoint::toRecord(receiver, "unregister"), "channels", 3);
    check(engine.endpoints()["tx:146500000"].m_status == MessageEndpoint::Idle, "Guards: the receiver going away leaves the transmitter");

    QString id = engine.addPage(pageRecord("146500000", "pocsag", "1", "x"), 10);
    check(engine.addPage(pageRecord("", "pocsag", "1", "x"), 10).isEmpty(), "Guards: a page needs a transmitter");
    MessageRecord same = pageRecord("146500000", "pocsag", "1", "x");
    same.m_id = id;
    check(engine.addPage(same, 10).isEmpty(), "Guards: a page id is used once");

    engine.dispatch(11);
    engine.ingest(event(id, "claimed"), "channels", 12);
    check(!engine.applyEvent(id, "released", QString(), 13) == false && (engine.page(id)->m_state == MessageCenterPage::Pending),
        "Guards: a claimed page can be released");
    engine.dispatch(14);
    engine.ingest(event(id, "claimed"), "channels", 15);
    engine.ingest(event(id, "transmitted"), "channels", 16);
    check(!engine.applyEvent(id, "released", QString(), 17) && (engine.page(id)->m_state == MessageCenterPage::Transmitted),
        "Guards: a page being transmitted is not released");
    engine.ingest(event(id, "sent"), "channels", 18);
    check(!engine.applyEvent(id, "aborted", QString(), 19) && !engine.applyEvent(id, "claimed", QString(), 19)
        && !engine.applyEvent(id, "transmitted", QString(), 19) && (engine.page(id)->m_state == MessageCenterPage::Sent),
        "Guards: late reports do not move a page that was sent");

    QString dead = engine.addPage(pageRecord("146500000", "pocsag", "2", "x"), 20);
    engine.dispatch(21);
    engine.ingest(event(dead, "rejected", "no"), "channels", 22);
    check(!engine.applyEvent(dead, "aborted", QString(), 23) && !engine.applyEvent(dead, "sent", QString(), 23)
        && (engine.page(dead)->m_state == MessageCenterPage::Dead), "Guards: a dead page stays dead for late reports");
    engine.requeue(dead, 24);
    check(!engine.applyEvent(dead, "aborted", QString(), 25) && (engine.page(dead)->m_state == MessageCenterPage::Pending),
        "Guards: a requeued page ignores the report of its earlier life");

    QString skipped = engine.addPage(pageRecord("146500000", "pocsag", "3", "x"), 30);
    engine.dispatch(31);
    check(!engine.applyEvent(skipped, "transmitted", QString(), 32) && (engine.page(skipped)->m_state == MessageCenterPage::Queued),
        "Guards: transmitted needs the claim first");

    // A transmitter that stops announcing is offline
    engine.ingest(event(skipped, "claimed"), "channels", 33);
    engine.sweep(33 + 20000, 15000);
    check((engine.endpoints()["tx:146500000"].m_status == MessageEndpoint::Offline) && (engine.page(skipped)->m_state == MessageCenterPage::Pending),
        "Guards: a transmitter silent for too long is offline, its claims are released");

    // Events from a file that is read do not move live pages or endpoints
    MessageCenterEngine replay;
    QTemporaryDir directory;
    QString path = directory.path() + "/replay.jsonl";
    QFile file(path);
    file.open(QIODevice::WriteOnly);
    file.write(MessageEndpoint::toRecord(tx, "register").toJsonLine());
    file.write(rx("r1", "pocsag", "1", "a message", "A").toJsonLine());
    file.close();
    replay.addSource(std::unique_ptr<MessageCenterSource>(new MessageCenterFileSource("replay", path)));
    QStringList errors;
    replay.pollSources(errors);
    check(replay.endpoints().isEmpty() && replay.find("r1"), "Guards: the announcements in a file that is read are ignored, its messages are logged");

    // An update record that leaves fields out does not erase them
    MessageCenterEngine log;
    MessageRecord full = rx("m1", "gsc", "100123", "", "A", 600);
    full.m_msgType = "voice";
    full.m_partial = true;
    log.ingest(full, "channels", 1);
    MessageRecord bare;
    bare.m_id = "m1";
    bare.m_msgType = "voice";
    bare.m_protocolData.insert("voice_end", "turn_off");
    log.ingest(bare, "channels", 2);
    const MessageRecord *kept = log.find("m1");
    check(kept && (kept->m_protocol == "gsc") && (kept->m_address == "100123") && (kept->m_frequency == 466000000) && (kept->m_speed == 600)
        && (kept->m_channel == "A") && !kept->m_partial, "Guards: an update with fields left out keeps the earlier record's fields");

    // Endpoint records
    MessageEndpoint e;
    QString what;
    MessageRecord notEvent = rx("x", "pocsag", "1", "a", "A");
    MessageEndpoint busy = transmitter("b", {});
    busy.m_status = MessageEndpoint::Busy;
    check(!MessageEndpoint::fromRecord(notEvent, e, what) && !busy.ready() && busy.canTransmit(pageRecord("b", "flex", "1", "x")),
        "Endpoint: a busy one that gives no capacity is not ready, no protocol listed means any");

    // History: the log is there again at the next start, a message once in its last version
    QTemporaryDir historyDirectory;
    const QString historyPath = historyDirectory.path() + "/history";
    QString error;
    {
        MessageCenterEngine first;
        check(first.setHistory(historyPath, 30, 1 << 20, error) && first.log().empty(), "History: a new folder, an empty log");
        first.ingest(rx("h1", "pocsag", "1", "first", "A"), "channels", 1);
        first.ingest(rx("h2", "flex", "2", "second", "A"), "channels", 2);
        MessageRecord update;
        update.m_id = "h1";
        update.m_protocolData.insert("note", "updated");
        first.ingest(update, "channels", 3);
        MessageRecord redecoded = *first.find("h2");
        redecoded.m_text = "decoded again";
        first.replace(redecoded);
    }
    {
        MessageCenterEngine second;
        check(second.setHistory(historyPath, 30, 1 << 20, error) && (second.log().size() == 2) && second.find("h1")
            && (second.find("h1")->m_protocolData.value("note").toString() == "updated") && (second.find("h1")->m_text == "first")
            && (second.find("h2")->m_text == "decoded again") && (second.log().front().m_id == "h1"),
            "History: the messages again in their order, the last version of each (update, decoded again)");

        // A query over the history; the same over the log when there is none
        MessageCenterFilter flex;
        flex.m_protocols = QStringList("flex");
        MessageCenterArchive::Query q;
        q.m_filter = flex;
        MessageCenterArchiveResult fromHistory = second.archive()->query(q);
        MessageCenterArchiveResult fromLog = second.queryLog(-1, -1, flex, 0, 100);
        check((fromHistory.m_total == 1) && (fromHistory.m_records[0].m_text == "decoded again") && (fromLog.m_total == 1)
            && (fromLog.m_records[0].m_id == "h2"), "History: a query over it, and over the log, give the same");

        // Clearing the log keeps the history; the history is cleared on its own
        second.clearLog();
        check(second.log().empty() && (second.archive()->query(MessageCenterArchive::Query()).m_total == 2),
            "History: clearing the log keeps the history");
        check(second.clearHistory(error) && (second.archive()->query(MessageCenterArchive::Query()).m_total == 0),
            "History: cleared when asked");
    }
    {
        // Turned on with messages in the log: they go into it
        MessageCenterEngine later;
        later.ingest(rx("l1", "gsc", "100123", "earlier", "A"), "channels", 1);
        check(later.setHistory(historyPath, 30, 1 << 20, error) && (later.log().size() == 1), "History: turned on later, the log stays");
        MessageCenterEngine next;
        next.setHistory(historyPath, 30, 1 << 20, error);
        check((next.log().size() == 1) && next.find("l1"), "History: turned on later, it starts with the log");
        MessageCenterEngine none;
        check(none.setHistory(QString(), 30, 1 << 20, error) && none.log().empty() && !none.archive(), "History: no folder, no history");
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testFilter();
    testLogAndRoutes();
    testEndpoints();
    testPages();
    testGuards();
    testRegExp();
    testCommand();
    testActionSink();
    testRoutesWithActions();
    testRouteOrderAndSources();
    testDecodingRoute();
    testEndpointRemoval();
    testGroupMembers();
    testMarks();
    testPresets();
    testUdp();
    testCsvSources();
    testContacts();
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
