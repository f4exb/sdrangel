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

// Self test of the table of paging messages: rows, updates by id, chips, columns. Run with
// QT_QPA_PLATFORM=offscreen. Prints one line per check and returns non-zero if any check fails.

#include <QHeaderView>
#include <QApplication>
#include <QScrollBar>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <cstdio>
#include <vector>

#include "dsp/wavfilerecord.h"
#include "gui/pagingmessagetable.h"
#include "gui/pagingvoiceplayer.h"

namespace {

int passes = 0;
int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (ok) {
        passes++;
    } else {
        failures++;
    }
}

MessageRecord record(const QString& id, const QString& protocol, const QString& address, const QString& text)
{
    MessageRecord r;
    r.m_id = id;
    r.m_timestamp = 1000000;
    r.m_protocol = protocol;
    r.m_mode = "1200";
    r.m_msgType = "alphanumeric";
    r.m_address = address;
    r.m_text = text;
    r.m_errorsCorrected = 0;
    r.m_errorsUncorrectable = 0;
    return r;
}

QString cell(const PagingMessageTable& table, int row, PagingMessageTable::Column column)
{
    return table.item(row, column) ? table.item(row, column)->text() : QString();
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    PagingMessageTable table;
    table.setMaxRows(3);

    // A row per record, the same id updates its row
    MessageRecord a = record("a", "pocsag", "1234567A", "HELLO");
    a.m_direction = MessageRecord::DirectionTX;
    a.m_errorsCorrected = -1;
    a.m_errorsUncorrectable = -1;
    table.addRecord(a, "queued");
    table.addRecord(record("b", "flex", "0001234567", "FLEX"));
    check(table.rowCount() == 2, "two records are two rows");
    table.addRecord(a, "sent");
    check((table.rowCount() == 2) && (cell(table, 0, PagingMessageTable::ColState) == "sent")
        && (cell(table, 0, PagingMessageTable::ColDirection) == "TX") && cell(table, 0, PagingMessageTable::ColErrors).isEmpty(),
        "the same id updates its row: the state, and no errors for a message that was transmitted");
    check((cell(table, 1, PagingMessageTable::ColErrors) == "ok") && (cell(table, 1, PagingMessageTable::ColDirection) == "RX"),
        "a received message shows its errors");

    // The quality: corrected bits, lost codewords, and a failed check of a message whose bits were all corrected
    {
        PagingMessageTable quality;
        MessageRecord corrected = record("c", "flex", "0001234567", "FLEX");
        corrected.m_errorsCorrected = 3;
        quality.addRecord(corrected);
        MessageRecord lost = record("l", "flex", "0001234567", "FLEX");
        lost.m_errorsUncorrectable = 2;
        quality.addRecord(lost);
        MessageRecord failed = record("f", "flex", "0001234567", "FLEX");
        failed.m_errorsCorrected = 9;
        failed.m_protocolData.insert("checksum", "failed");
        quality.addRecord(failed);
        check(cell(quality, 0, PagingMessageTable::ColErrors) == "~3", "corrected bits: ~n");
        check(cell(quality, 1, PagingMessageTable::ColErrors) == "X2", "lost codewords: Xn");
        check((cell(quality, 2, PagingMessageTable::ColErrors) == "damaged")
            && quality.item(2, PagingMessageTable::ColErrors)->toolTip().contains("checksum K failed"),
            "a failed check of a message whose bits were corrected: damaged, and why");
    }

    // A page of the Message Center not sent yet: QU in the queue, NS not sent; TX once its channel sent it
    {
        PagingMessageTable pages;
        MessageRecord p = record("p", "pocsag", "0000005A", "PAGE");
        p.m_direction = MessageRecord::DirectionTX;
        p.m_protocolData.insert("transmitter", "tx1");
        p.m_protocolData.insert("page_state", "pending");
        p.m_protocolData.insert("page_reason", "waiting: no transmitter tx1");
        pages.addRecord(p);
        const QTableWidgetItem *dir = pages.item(0, PagingMessageTable::ColDirection);
        check((cell(pages, 0, PagingMessageTable::ColDirection) == "QU") && dir && dir->toolTip().contains("waiting: no transmitter tx1"),
            "a page in the queue is QU, its tooltip has the reason");
        p.m_protocolData.insert("page_state", "dead");
        pages.addRecord(p);
        check(cell(pages, 0, PagingMessageTable::ColDirection) == "NS", "a page that failed is NS");
        p.m_protocolData.remove("page_state");
        pages.addRecord(p);
        check(cell(pages, 0, PagingMessageTable::ColDirection) == "NS", "a page no longer queued (after a restart) is NS");
        p.m_channel = "tx1";
        pages.addRecord(p);
        check(cell(pages, 0, PagingMessageTable::ColDirection) == "TX", "a page its channel sent is TX");
    }

    // Speed: 1000 rows of FLEX messages with chips, then the same records again (a view made again), shown
    {
        PagingMessageTable big;
        big.setMaxRows(1000);
        big.resize(1200, 600);
        big.show();
        std::vector<MessageRecord> records;

        for (int i = 0; i < 1200; i++)
        {
            MessageRecord r = record(QString("big%1").arg(i), "flex", QString("%1").arg(1000000 + i, 10, 10, QChar('0')),
                QString("Message %1 of a large FLEX transmission with some text to wrap in its column").arg(i));
            r.m_mode = "1600/2";
            r.m_polarity = MessageRecord::PolarityNormal;
            r.m_protocolData.insert("capcode", 1000000 + i);
            r.m_protocolData.insert("frame", i % 128);
            r.m_protocolData.insert("phase", "A");
            r.m_protocolData.insert("char_status", QString(r.m_text.size(), QChar('.')));
            records.push_back(r);
        }

        QElapsedTimer timer;
        timer.start();
        for (const MessageRecord& r : records) {
            big.addRecord(r);
        }
        QApplication::processEvents();
        const qint64 addMs = timer.restart();
        big.setRowCount(0);
        for (size_t i = 200; i < records.size(); i++) {
            big.addRecord(records[i]);
        }
        QApplication::processEvents();
        const qint64 rebuildMs = timer.elapsed();
        std::printf("      1200 records added in %lld ms, 1000 rows made again in %lld ms\n", (long long) addMs, (long long) rebuildMs);
        check((big.rowCount() == 1000) && (addMs < 3000) && (rebuildMs < 2000),
            "a large transmission: 1200 records and a view of 1000 rows made again in less than 3 s and 2 s");
    }

    // A FLEX short instruction of a reserved type: its data bits in the message, the type explained in the info
    {
        PagingMessageTable si;
        MessageRecord r = record("si", "flex", "0000289168", QString());
        r.m_msgType = "short_instruction";
        r.m_protocolData.insert("instruction_type", 5);
        r.m_protocolData.insert("instruction_data", (0x2a5 << 3) | 5);
        si.addRecord(r);
        const QTableWidgetItem *info = si.item(0, PagingMessageTable::ColInfo);
        check(cell(si, 0, PagingMessageTable::ColMessage) == "(short instruction type 5, reserved, data 0x2a5)",
            "a reserved short instruction shows its data bits");
        check(cell(si, 0, PagingMessageTable::ColInfo).contains("SI:5") && info && info->toolTip().contains("reserved for future use")
            && info->toolTip().contains("01010100101"), "its info explains the type and has the bits");
    }

    // FLEX short messages without digits: a source (type 1), a source with a message number (type 2), reserved (type 3)
    {
        PagingMessageTable sm;
        auto shortMessage = [](const char *id, int type, int source) {
            MessageRecord r = record(id, "flex", "0000374296", QString());
            r.m_msgType = "short_message";
            r.m_rawData = QByteArray::fromHex("950948");
            r.m_protocolData.insert("short_type", type);
            r.m_protocolData.insert("short_source", source);
            return r;
        };
        MessageRecord numbered = shortMessage("s2", 2, 3);
        numbered.m_protocolData.insert("message_number", 17);
        numbered.m_protocolData.insert("retrieval", true);
        sm.addRecord(shortMessage("s1", 1, 0));
        sm.addRecord(numbered);
        sm.addRecord(shortMessage("s3", 3, 0));
        check(cell(sm, 0, PagingMessageTable::ColMessage) == "(type:source source:0)", "a short message of type 1 shows its source");
        check(cell(sm, 1, PagingMessageTable::ColMessage) == "(type:numbered source:3 number:17 retrieval)",
            "a short message of type 2 shows its source, its number and the retrieval flag");
        check(cell(sm, 2, PagingMessageTable::ColMessage) == "(type:reserved(3) data:0x950948)",
            "a reserved short message shows its data");
    }

    // A row selected: new messages wait, the rows shown do not move; they come when the selection goes
    {
        PagingMessageTable live;
        live.setMaxRows(5);
        live.resize(600, 400);
        for (int i = 0; i < 5; i++) {
            live.addRecord(record(QString("l%1").arg(i), "pocsag", QString("000000%1A").arg(i), QString("old %1").arg(i)));
        }
        live.selectId("l2");
        for (int i = 5; i < 8; i++) {
            live.addRecord(record(QString("l%1").arg(i), "pocsag", QString("000000%1A").arg(i), QString("new %1").arg(i)));
        }
        MessageRecord update = record("l3", "pocsag", "0000003A", "old 3 updated");
        live.addRecord(update);
        check((live.rowCount() == 5) && (live.waiting() == 3) && (cell(live, 0, PagingMessageTable::ColMessage) == "old 0")
            && (live.selectedId() == "l2"), "a row selected: the new messages wait, no row moves, the selection stays");
        check(cell(live, 3, PagingMessageTable::ColMessage) == "old 3 updated", "a row shown is updated in place");
        live.clearSelection();
        QApplication::processEvents();
        check((live.waiting() == 0) && (live.rowCount() == 5) && (cell(live, 4, PagingMessageTable::ColMessage) == "new 7")
            && (cell(live, 0, PagingMessageTable::ColMessage) == "old 3 updated"), "no selection: they come, the oldest go");
    }

    // Following the newest: records that come one after the other keep following (the bottom moves after the rows
    // are added: that is no scroll of the user); the user scrolling up holds them, scrolling to the bottom shows them
    {
        PagingMessageTable follow;
        follow.setMaxRows(100);
        follow.resize(600, 200);
        follow.show();
        int held = 0;

        for (int i = 0; i < 60; i++)
        {
            follow.addRecord(record(QString("f%1").arg(i), "pocsag", QString("000000%1A").arg(i % 10), QString("row %1").arg(i)));
            held += follow.waiting();
        }

        QApplication::processEvents();
        check((held == 0) && (follow.rowCount() == 60), "records one after the other: the table follows, none waits");

        // Rows that get higher after they were added (a message that wraps, laid out later): the bottom moves down,
        // which is no scroll of the user; the next record still follows
        for (int i = 50; i < 60; i++) {
            follow.setRowHeight(i, 60);
        }

        QApplication::processEvents();      // The layout of the table: its scroll range grows
        follow.addRecord(record("fh", "pocsag", "0000003A", "after the rows got higher"));
        QApplication::processEvents();
        check(follow.waiting() == 0, "rows made higher after they were added: the table still follows");
        follow.removeRow(follow.rowCount() - 1);
        follow.verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);
        QApplication::processEvents();
        follow.addRecord(record("f60", "pocsag", "0000001A", "after the scroll up"));
        check(follow.waiting() == 1, "the user scrolled up: a new record waits");
        follow.verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMaximum);
        QApplication::processEvents();
        follow.addRecord(record("f61", "pocsag", "0000002A", "following again"));
        QApplication::processEvents();
        check((follow.waiting() == 0) && (follow.rowCount() == 62), "the user scrolled to the bottom: they come, the table follows again");
    }

    // Marks: their icons before the message, the colour of its text and of the row
    {
        PagingMessageTable marked;
        MessageRecord r = record("m", "flex", "0001234567", "P 1 Brand, ongeval");
        QVariantMap fire;
        fire.insert("icon", ":fire_engine");
        fire.insert("background", 0xff802020u);
        QVariantMap accident;
        accident.insert("icon", ":collision");
        accident.insert("color", 0xffffcc00u);
        r.m_protocolData.insert("marks", QVariantList({fire, accident}));
        marked.addRecord(r);
        marked.addRecord(record("n", "flex", "0001234568", "plain"));
        const QTableWidgetItem *message = marked.item(0, PagingMessageTable::ColMessage);
        const QTableWidgetItem *address = marked.item(0, PagingMessageTable::ColAddress);
        const QTableWidgetItem *plain = marked.item(1, PagingMessageTable::ColMessage);
        check(message && !message->icon().isNull() && (message->icon().availableSizes().value(0).width() > message->icon().availableSizes().value(0).height()),
            "a message with two marks has their two icons side by side");
        check(message && message->data(Qt::UserRole + 1).toString().contains("#ffcc00"), "its text has the colour of its mark");
        check(address && (address->background().color().rgb() == QColor(0x80, 0x20, 0x20).rgb()), "its row has the colour of its mark");
        check(plain && plain->icon().isNull() && (plain->background().style() == Qt::NoBrush), "a message without marks: none");
    }

    // The oldest rows go first
    table.addRecord(record("c", "gsc", "123456/1", "C"));
    table.addRecord(record("d", "gsc", "123456/2", "D"));
    check((table.rowCount() == 3) && (cell(table, 0, PagingMessageTable::ColAddress) == "0001234567"), "the oldest row goes at the maximum");

    // The end of a voice message completes its row
    MessageRecord voice = record("v", "gsc", "123456/3", QString());
    voice.m_msgType = "voice";
    table.addRecord(voice);
    check(cell(table, 2, PagingMessageTable::ColMessage) == "(voice)", "a voice message without its end");
    MessageRecord end = record("v", "gsc", "1234563", QString());
    end.m_msgType = "voice";
    end.m_protocolData.insert("voice_end", "timeout");
    end.m_protocolData.insert("voice_seconds", 4.5);
    table.addRecord(end);
    check((table.rowCount() == 3) && cell(table, 2, PagingMessageTable::ColMessage).startsWith("voice ended: timeout after 4.5 s"),
        "the end of a voice message updates its row");
    check(table.item(2, PagingMessageTable::ColMessage)->icon().isNull(), "no voice file: no play icon");

    // The voice file: a play icon in the message cell, the file in its tool tip
    QTemporaryDir dir;
    WavFileRecord recorder(8000);
    recorder.setMono(true);
    recorder.setFileName(dir.path() + "/voice");
    recorder.startRecording();
    std::vector<qint16> samples(8000, 1000);
    recorder.writeMono(samples.data(), (int) samples.size());
    recorder.stopRecording();
    QString file = recorder.getCurrentFileName();
    end.m_protocolData.insert("voice_file", file);
    table.addRecord(end);
    QTableWidgetItem *message = table.item(2, PagingMessageTable::ColMessage);
    QString voiceTip = message->toolTip();
    check(!message->icon().isNull() && voiceTip.contains(file) && voiceTip.contains("1.0 s, 8000 Hz, 1 channel, 16 bit")
        && voiceTip.contains("Click the icon to play"), qPrintable("a voice file: play icon, path, length and format in the tool tip: " + voiceTip));
    check(PagingVoicePlayer::fileInfo(dir.path() + "/none.wav").contains("not there any more"), "a file that is not there");
    check(!cell(table, 2, PagingMessageTable::ColMessage).contains("double-click"), "no double-click hint");

    // Chips: the fields of a transmitted message are the ones it has
    MessageRecord flexTx = record("f", "flex", "0001234567", "X");
    flexTx.m_direction = MessageRecord::DirectionTX;
    flexTx.m_protocolData.insert("priority", true);
    flexTx.m_protocolData.insert("retransmission", 2);
    QString plain, html, tip;
    PagingMessageTable::recordInfo(flexTx, plain, html, tip);
    check((plain == "PRI:1 COPY:2") && html.contains("COPY:") && html.contains(">2<"), "a transmitted FLEX message: priority and copy, no decode fields");

    MessageRecord flexRx = record("g", "flex", "0001234567", "X");
    flexRx.m_protocolData.insert("phase", "A");
    flexRx.m_protocolData.insert("cycle", 3);
    flexRx.m_protocolData.insert("frame", 31);
    flexRx.m_protocolData.insert("message_number", 0);
    flexRx.m_protocolData.insert("retrieval", true);
    flexRx.m_protocolData.insert("checksum", "ok");
    PagingMessageTable::recordInfo(flexRx, plain, html, tip);
    check(plain == "PH:A C:3 F:31 N:1 R:1 K:ok", "a received FLEX message: phase, cycle, frame, number as shown by pagers, R, checksum");

    // Each chip has its own tool tip, saying what it is and its value
    QStringList chipTexts, chipHtml, chipTips;
    PagingMessageTable::recordChips(flexRx, chipTexts, chipHtml, chipTips);
    bool tipsOk = (chipTexts.size() == 6) && (chipTips.size() == 6) && (chipHtml.size() == 6);
    for (const QString& t : chipTips) {
        tipsOk = tipsOk && !t.isEmpty();
    }
    check(tipsOk && chipTips[0].contains("Phase A") && chipTips[1].contains("Cycle 3") && chipTips[2].contains("Frame 31")
        && chipTips[3].contains("Message number 1") && chipTips[5].contains("Checksum"), "a tool tip for each chip, with its value");

    MessageRecord pocsagRx = record("p", "pocsag", "1234567D", "X");
    pocsagRx.m_protocolData.insert("batch", 1);
    pocsagRx.m_protocolData.insert("end", "cut");
    chipTexts.clear(); chipHtml.clear(); chipTips.clear();
    PagingMessageTable::recordChips(pocsagRx, chipTexts, chipHtml, chipTips);
    check((chipTexts == QStringList({"B:2", "END:cut"})) && chipTips[0].contains("Batch 2"), "POCSAG: batch counted from 1, cut");
    pocsagRx.m_protocolData.insert("end", "batch");
    chipTexts.clear(); chipHtml.clear(); chipTips.clear();
    PagingMessageTable::recordChips(pocsagRx, chipTexts, chipHtml, chipTips);
    check((chipTexts == QStringList({"B:2", "END:batch"})) && chipTips[1].contains("whole"),
        "POCSAG: a message that filled the last batch, no idle codeword after it: END:batch, whole");
    pocsagRx.m_protocolData.insert("end", "idle");
    chipTexts.clear(); chipHtml.clear(); chipTips.clear();
    PagingMessageTable::recordChips(pocsagRx, chipTexts, chipHtml, chipTips);
    check(chipTexts == QStringList({"B:2"}), "POCSAG: a message ended by an idle codeword: no END chip");

    MessageRecord gscRx = record("q", "gsc", "123456", "X");
    gscRx.m_protocolData.insert("slot", 0);
    chipTexts.clear(); chipHtml.clear(); chipTips.clear();
    PagingMessageTable::recordChips(gscRx, chipTexts, chipHtml, chipTips);
    check(chipTexts == QStringList({"SLOT:1"}), "GSC: the address slot is SLOT, not S as the FLEX signature");

    // Columns: hidden ones, saved and restored
    table.setDefaultColumns({PagingMessageTable::ColTime, PagingMessageTable::ColMessage});
    check(table.isColumnHidden(PagingMessageTable::ColAddress) && !table.isColumnHidden(PagingMessageTable::ColMessage),
        "the default columns are shown, the others hidden");
    table.setColumnHidden(PagingMessageTable::ColAddress, false);
    QByteArray saved = table.columns();
    PagingMessageTable other;
    check(other.setColumns(saved) && !other.isColumnHidden(PagingMessageTable::ColAddress)
        && other.isColumnHidden(PagingMessageTable::ColState), "the columns are restored");
    check(!other.setColumns(QByteArray()), "no saved columns: false, the defaults stay");

    // Date column and row numbers, as in the Pager demodulator: the date before the time
    PagingMessageTable dated;
    dated.addRecord(record("d", "pocsag", "1234567D", "X"));
    check((cell(dated, 0, PagingMessageTable::ColDate) == QDateTime::fromMSecsSinceEpoch(1000000).date().toString(Qt::TextDate))
        && !dated.isColumnHidden(PagingMessageTable::ColDate)
        && (dated.horizontalHeader()->visualIndex(PagingMessageTable::ColDate) < dated.horizontalHeader()->visualIndex(PagingMessageTable::ColTime))
        && !dated.verticalHeader()->isHidden(), "the date, shown before the time, and row numbers");

    // Columns saved before the date column (13 columns): restored, the date column shown after them
    QTableWidget old(0, 13);
    old.horizontalHeader()->hideSection(PagingMessageTable::ColState);
    old.horizontalHeader()->resizeSection(PagingMessageTable::ColMessage, 345);
    PagingMessageTable upgraded;
    check(upgraded.setColumns(old.horizontalHeader()->saveState()) && upgraded.isColumnHidden(PagingMessageTable::ColState)
        && (upgraded.columnWidth(PagingMessageTable::ColMessage) == 345) && !upgraded.isColumnHidden(PagingMessageTable::ColDate),
        qPrintable(QString("columns saved by the 13 column table restored (hidden %1, width %2, date hidden %3)")
            .arg(upgraded.isColumnHidden(PagingMessageTable::ColState)).arg(upgraded.columnWidth(PagingMessageTable::ColMessage))
            .arg(upgraded.isColumnHidden(PagingMessageTable::ColDate))));

    // Details: every field of the record, protocolData and the raw payload as bits
    MessageRecord detailed = record("h", "pocsag", "1234567D", "HI");
    detailed.m_protocolData.insert("function", 3);
    detailed.m_rawData = QByteArray::fromHex("a5f0");
    detailed.m_rawBits = 12;
    PagingMessageTable details;
    details.addRecord(detailed);
    details.showDetails(0);
    QTreeWidget *tree = details.findChild<QTreeWidget*>();
    bool function = false, bits = false, address = false;

    for (QTreeWidgetItemIterator it(tree); tree && *it; ++it)
    {
        function = function || (((*it)->text(0) == "function") && ((*it)->text(1) == "3") && (*it)->parent()
            && ((*it)->parent()->text(0) == "protocolData"));
        bits = bits || (((*it)->text(0) == "rawData (bits)") && ((*it)->text(1) == "10100101 1111"));
        address = address || (((*it)->text(0) == "address") && ((*it)->text(1) == "1234567D"));
    }

    check(tree && function && bits && address, "details: the address, protocolData and the raw payload as bits");

    // Character status: corrected amber, lost red on a red tint, received as the text; no status, escaped text
    QString statusRow = PagingMessageTable::statusHtml("AB?<", ".cXX");
    check(statusRow.startsWith("A<span style=\"color:#ffca28\">B</span>") && statusRow.contains("#ff5252") && statusRow.contains("?&lt;</span>"),
        "char status: corrected and lost characters coloured, lost ones in one span");
    check(PagingMessageTable::statusHtml("A<B", "..") == "A&lt;B", "char status: a status that does not fit the text is not used");

    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
