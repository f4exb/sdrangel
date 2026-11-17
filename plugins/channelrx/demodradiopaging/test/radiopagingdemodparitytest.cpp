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

// Self test of what the radio paging demodulator has of the Pager demodulator: the datagram of its UDP output,
// the settings (UDP, log, notifications, scope) and the CSV log read back into records.

#include <cstdio>
#include <string>

#include <QByteArray>
#include <QDateTime>
#include <QTextStream>

#include "util/csv.h"
#include "util/messagerecord.h"

#include "radiopagingrecode.h"
#include "radiopagingpagerformat.h"
#include "radiopagingdemodsettings.h"
#include "radiopagingdemodsink.h"

namespace {

int passes = 0;
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what.c_str());

    if (ok) {
        passes++;
    } else {
        failures++;
    }
}

QStringList fields(const QByteArray& datagram)
{
    QStringList list;

    for (const QByteArray& field : datagram.split('\0')) {
        list.append(QString::fromLatin1(field));
    }

    list.removeLast();  // After the last NUL
    return list;
}

MessageRecord pocsagRecord()
{
    MessageRecord r;
    r.m_id = "id-1";
    r.m_timestamp = 1700000000000LL;
    r.m_protocol = "pocsag";
    r.m_mode = "1200";
    r.m_kind = MessageRecord::KindMessage;
    r.m_msgType = "alphanumeric";
    r.m_address = "0001234A";
    r.m_polarity = MessageRecord::PolarityNormal;
    r.m_speed = 1200;
    r.m_text = "HELLO, \"WORLD\"";
    r.m_errorsCorrected = 1;
    r.m_errorsUncorrectable = 0;
    r.m_frequency = 929613000;
    r.m_channel = "chan";
    r.m_protocolData.insert("ric", (qulonglong) 1234);
    r.m_protocolData.insert("function", 0);
    r.m_protocolData.insert("alt_type", "numeric");
    r.m_protocolData.insert("alt_text", "12345");
    return r;
}

void testDatagram()
{
    MessageRecord r = pocsagRecord();
    QStringList f = fields(RadioPagingPagerFormat::datagram(r));

    check(f.size() == 8, "datagram has the 8 fields of the Pager demodulator");
    check(f.value(2) == "0001234", "POCSAG address of 7 digits");
    check(f.value(3) == "0", "function bits");
    check(f.value(4) == r.m_text, "alphanumeric field");
    check(f.value(5) == "12345", "numeric field has the numeric interpretation");
    check(f.value(6) == "POCSAG", "protocol name");
    check(f.value(7) == "alphanumeric", "type");

    MessageRecord n = pocsagRecord();
    n.m_msgType = "numeric";
    n.m_text = "5551234";
    n.m_protocolData.insert("alt_type", "alphanumeric");
    n.m_protocolData.insert("alt_text", "UUUU");
    f = fields(RadioPagingPagerFormat::datagram(n));
    check(f.value(4) == "UUUU" && f.value(5) == "5551234", "numeric message: text in the numeric field");

    MessageRecord x;
    x.m_protocol = "flex";
    x.m_msgType = "alphanumeric";
    x.m_address = "000123456";
    x.m_text = "FLEX TEXT";
    x.m_protocolData.insert("capcode", (qulonglong) 123456);
    f = fields(RadioPagingPagerFormat::datagram(x));
    check(f.value(2) == "000123456", "FLEX address of 9 digits");
    check(f.value(3).isEmpty(), "FLEX has no function bits");
    check(f.value(6) == "FLEX", "FLEX protocol name");

    MessageRecord v;
    v.m_protocolData.insert("voice_end", "turn_off");
    check(RadioPagingPagerFormat::isCompletion(v), "the end of a voice message completes a record");
    check(!RadioPagingPagerFormat::isCompletion(r), "a message is not a completion");
}

void testAlternative()
{
    check(RadioPagingRecode::alternativeGiven("numeric", "123456789012345678"), "numeric alternative of 18 characters is given");
    check(!RadioPagingRecode::alternativeGiven("numeric", "1234567890123456789"), "numeric alternative of 19 characters is not given");
    check(RadioPagingRecode::alternativeGiven("alphanumeric", "THIS TEXT IS LONGER THAN EIGHTEEN"), "a long alphanumeric alternative is given");
}

void testCsv()
{
    MessageRecord r = pocsagRecord();
    r.m_charStatus = "ooooooooooooco";
    r.m_protocolData.insert("note", "a,b \"c\"");

    QString text = MessageRecord::csvHeader().join(',') + "\n" + r.toCSV() + "\n";
    QTextStream in(&text);
    QString error;
    QHash<QString, int> columns = CSV::readHeader(in, {"id", "timestamp", "protocol", "kind"}, error);
    check(error.isEmpty(), "log header is read");

    QStringList row;
    check(CSV::readRow(in, &row), "log row is read");

    MessageRecord back;
    check(MessageRecord::fromCSV(columns, row, back), "record is read from the row");
    check(back.m_id == r.m_id && back.m_timestamp == r.m_timestamp, "id and time");
    check(back.m_protocol == "pocsag" && back.m_mode == "1200" && back.m_msgType == "alphanumeric", "protocol, mode, type");
    check(back.m_address == r.m_address && back.m_polarity == r.m_polarity && back.m_speed == 1200, "address, polarity, speed");
    check(back.m_text == r.m_text, "text with a comma and quotes");
    check(back.m_charStatus == r.m_charStatus, "character status");
    check(back.m_errorsCorrected == 1 && back.m_errorsUncorrectable == 0, "errors");
    check(back.m_ber < 0.0f && back.m_rawBits < 0, "absent values stay absent");
    check(back.m_frequency == r.m_frequency && back.m_channel == "chan", "frequency and channel");
    check(back.m_kind == MessageRecord::KindMessage, "kind");
    check(back.m_protocolData.value("ric").toULongLong() == 1234 && back.m_protocolData.value("note").toString() == "a,b \"c\"",
        "protocol data");

    QHash<QString, int> none;
    MessageRecord bad;
    check(!MessageRecord::fromCSV(none, QStringList({"x"}), bad), "a row without id, time and protocol is refused");
}

void testSettings()
{
    typedef RadioPagingDemodSettings::NotificationSettings Notification;

    RadioPagingDemodSettings s;
    check(!s.m_udpEnabled && s.m_udpPort == 9999 && s.m_udpFormat == RadioPagingDemodSettings::UdpPagerDemod, "UDP defaults");
    check(!s.m_logEnabled && !s.m_logFilename.isEmpty(), "log defaults");

    s.m_udpEnabled = true;
    s.m_udpAddress = "10.1.2.3";
    s.m_udpPort = 4242;
    s.m_udpFormat = RadioPagingDemodSettings::UdpJson;
    s.m_logEnabled = true;
    s.m_logFilename = "x.csv";
    s.m_scopeCh1 = RadioPagingDemodSink::ScopeFlexFm;
    s.m_scopeCh2 = RadioPagingDemodSink::ScopeGscSync;

    QSharedPointer<Notification> n = QSharedPointer<Notification>::create();
    n->m_matchField = Notification::MatchProtocol;
    n->m_regExp = "^fl.x$";
    n->m_speech = "${message}";
    n->m_command = "echo ${address}";
    n->m_highlight = true;
    n->m_highlightColor = 0xff00ff00;
    n->m_plotOnMap = true;
    n->updateRegularExpression();
    s.m_notificationSettings.append(n);

    RadioPagingDemodSettings t;
    check(t.deserialize(s.serialize()), "settings are read back");
    check(t.m_udpEnabled && t.m_udpAddress == "10.1.2.3" && t.m_udpPort == 4242, "UDP settings");
    check(t.m_udpFormat == RadioPagingDemodSettings::UdpJson, "UDP format");
    check(t.m_logEnabled && t.m_logFilename == "x.csv", "log settings");
    check(t.m_scopeCh1 == RadioPagingDemodSink::ScopeFlexFm && t.m_scopeCh2 == RadioPagingDemodSink::ScopeGscSync, "scope signals");
    check(t.m_notificationSettings.size() == 1, "one notification");

    if (t.m_notificationSettings.size() == 1)
    {
        const Notification *m = t.m_notificationSettings[0].data();
        check(m->m_matchField == Notification::MatchProtocol && m->m_regExp == "^fl.x$", "notification match");
        check(m->m_speech == "${message}" && m->m_command == "echo ${address}", "notification speech and command");
        check(m->m_highlight && m->m_highlightColor == (qint32) 0xff00ff00 && m->m_plotOnMap, "notification highlight and map");
        check(m->m_regularExpression.isValid() && m->m_regularExpression.match("flex").hasMatch()
            && !m->m_regularExpression.match("pocsag").hasMatch(), "notification expression is compiled");
    }

    // A setting saved before these existed reads the defaults
    RadioPagingDemodSettings d;
    d.m_udpPort = 1;
    check(d.deserialize(RadioPagingDemodSettings().serialize()) && d.m_udpPort == 9999, "defaults round trip");

    // Keys of the WebAPI and the GUI
    RadioPagingDemodSettings u;
    u.applySettings({"udpEnabled", "udpPort", "udpFormat", "logFilename", "scopeCh2"}, s);
    check(u.m_udpEnabled && u.m_udpPort == 4242 && u.m_udpFormat == 1 && u.m_logFilename == "x.csv" && u.m_scopeCh2 == s.m_scopeCh2,
        "applySettings keys");
    check(!u.m_logEnabled && u.m_udpAddress == "127.0.0.1", "applySettings leaves the other settings");

    check(RadioPagingDemodSink::scopeSignalName(RadioPagingDemodSink::ScopeFlexFm) == "FLEX FM", "scope signal name");
    check(RadioPagingDemodSink::scopeSignalName(RadioPagingDemodSink::ScopeSignalCount).isEmpty(), "no scope signal beyond the last");
}

} // namespace

int main()
{
    testDatagram();
    testAlternative();
    testCsv();
    testSettings();
    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
