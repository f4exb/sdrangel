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

// Self test of the archive of the Message Center: files by day and size, the days and the size kept, queries

#include <cstdio>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "messagecenterarchive.h"

namespace {

int passes = 0;
int failures = 0;

void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? passes : failures)++;
}

const qint64 day = 24LL * 3600 * 1000;
const qint64 t0 = QDateTime(QDate(2026, 11, 1), QTime(12, 0)).toMSecsSinceEpoch();

MessageRecord rec(const QString& id, qint64 t, const QString& text, const QString& protocol = "pocsag")
{
    MessageRecord r;
    r.m_id = id;
    r.m_timestamp = t;
    r.m_direction = MessageRecord::DirectionRX;
    r.m_kind = MessageRecord::KindMessage;
    r.m_protocol = protocol;
    r.m_address = "1234567";
    r.m_msgType = "alphanumeric";
    r.m_text = text;
    return r;
}

QStringList ids(const MessageCenterArchive::Result& r)
{
    QStringList out;
    for (const MessageRecord& m : r.m_records) {
        out << m.m_id;
    }
    return out;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir tmp;
    QString error;
    MessageCenterArchive::Limits limits;
    limits.m_fileBytes = 2000;

    // Files: a day each, then by size
    {
        MessageCenterArchive a;
        check(a.open(tmp.filePath("a"), limits, t0, error), "an archive opens, its folder made");

        for (int i = 0; i < 10; i++) {
            a.append(rec(QString("d0-%1").arg(i), t0 + i * 1000, "first day"), t0 + i * 1000);
        }
        for (int i = 0; i < 10; i++) {
            a.append(rec(QString("d1-%1").arg(i), t0 + day + i * 1000, "second day"), t0 + day + i * 1000);
        }

        const QStringList names = QDir(tmp.filePath("a")).entryList(QStringList("*.jsonl"), QDir::Files, QDir::Name);
        check((names.size() >= 4) && names.first().startsWith("20261101-000") && names.contains("20261101-001.jsonl")
            && names.contains("20261102-000.jsonl"), "a file for each day, a new one of the day over the size of a file");

        // Queries: all, newest first; a range; a filter; pages
        MessageCenterArchive::Query q;
        MessageCenterArchive::Result r = a.query(q);
        check((r.m_total == 20) && (r.m_records.size() == 20) && (r.m_records.front().m_id == "d1-9") && (r.m_records.back().m_id == "d0-0"),
            "a query of all: every record, newest first");
        q.m_from = t0 + day;
        r = a.query(q);
        check((r.m_total == 10) && (r.m_records.back().m_id == "d1-0"), "a time range: its records only");
        q = MessageCenterArchive::Query();
        q.m_filter.m_text = "first";
        q.m_limit = 3;
        q.m_offset = 3;
        r = a.query(q);
        check((r.m_total == 10) && (ids(r) == QStringList({"d0-6", "d0-5", "d0-4"})), "a filter, a page: the total and the records of the page");

        // A later version (written later, in a newer file) is the record, whether the filter selects it or not
        MessageRecord updated = rec("d0-5", t0 + 5000, "changed");
        a.append(updated, t0 + 2 * day);
        r = a.query(q);
        check((r.m_total == 9) && !ids(r).contains("d0-5"), "a record whose newer version the filter does not select: not selected");
        q.m_filter.m_text = "changed";
        q.m_offset = 0;
        r = a.query(q);
        check((r.m_total == 1) && (r.m_records.size() == 1) && (r.m_records[0].m_text == "changed"), "its newer version is");
        q = MessageCenterArchive::Query();
        q.m_limit = 6;
        r = a.query(q);
        check((r.m_total == 20) && (ids(r) == QStringList({"d1-9", "d1-8", "d1-7", "d1-6", "d1-5", "d1-4"})),
            "versions count once; pages in the order of the times of the records");

        std::vector<MessageRecord> last = a.newest(3);
        check((last.size() == 3) && (last.front().m_id == "d1-7") && (last.back().m_id == "d1-9"), "the newest records, oldest first");
        a.close();

        // The index is a cache: without it, or wrong, the same results
        MessageCenterArchive b;
        b.open(tmp.filePath("a"), limits, t0 + 2 * day, error);
        MessageCenterArchive::Result withIndex = b.query(MessageCenterArchive::Query());
        b.close();
        QFile::remove(QDir(tmp.filePath("a")).filePath("index.json"));
        MessageCenterArchive c;
        c.open(tmp.filePath("a"), limits, t0 + 2 * day, error);
        MessageCenterArchive::Result withoutIndex = c.query(MessageCenterArchive::Query());
        check((withIndex.m_total == 20) && (ids(withIndex) == ids(withoutIndex)), "without the index: the same records");

        // A line being written (no end yet) is not a record
        c.close();
        QFile part(QDir(tmp.filePath("a")).filePath(MessageCenterArchive::fileName(QDate(2026, 11, 3), 0)));
        part.open(QIODevice::Append);
        part.write("{\"id\":\"partial\",\"timestamp\":");
        part.close();
        MessageCenterArchive d;
        d.open(tmp.filePath("a"), limits, t0 + 2 * day, error);
        check((d.query(MessageCenterArchive::Query()).m_total == 20), "a line not ended is not read");
    }

    // The days kept, the size kept; only the files of the archive go
    {
        MessageCenterArchive::Limits keep = limits;
        keep.m_days = 2;
        MessageCenterArchive a;
        a.open(tmp.filePath("b"), keep, t0, error);
        QFile other(QDir(tmp.filePath("b")).filePath("notes.txt"));
        other.open(QIODevice::WriteOnly);
        other.write("not of the archive");
        other.close();

        for (int d = 0; d < 4; d++) {
            a.append(rec(QString("day%1").arg(d), t0 + d * day, "x"), t0 + d * day);
        }

        MessageCenterArchive::Result r = a.query(MessageCenterArchive::Query());
        check((ids(r) == QStringList({"day3", "day2"})) && QFile::exists(QDir(tmp.filePath("b")).filePath("notes.txt")),
            "the days kept: the older files go, another file of the folder stays");

        keep.m_days = 30;
        keep.m_bytes = 300;
        a.setLimits(keep, t0 + 4 * day);
        r = a.query(MessageCenterArchive::Query());
        check((r.m_total == 1) && (ids(r) == QStringList("day3")), "over the size kept: the oldest files go, the one being written stays");

        check(a.clear(error) && (a.query(MessageCenterArchive::Query()).m_total == 0) && (a.files() == 0), "cleared: no records");
    }

    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
