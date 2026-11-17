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

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include "messagecenterarchive.h"

namespace {

const char *indexName = "index.json";

// The time of a record from its line, without reading the rest of it
qint64 lineTime(const QByteArray& line)
{
    QJsonObject json = QJsonDocument::fromJson(line).object();
    return json.contains("timestamp") ? (qint64) json.value("timestamp").toDouble(-1) : -1;
}

} // namespace

MessageCenterArchive::MessageCenterArchive()
{
}

MessageCenterArchive::~MessageCenterArchive()
{
    close();
}

QString MessageCenterArchive::fileName(const QDate& date, int sequence)
{
    return QString("%1-%2.jsonl").arg(date.toString("yyyyMMdd")).arg(sequence, 3, 10, QChar('0'));
}

bool MessageCenterArchive::parseFileName(const QString& name, QDate& date, int& sequence)
{
    static const QRegularExpression pattern("^(\\d{8})-(\\d{3})\\.jsonl$");
    QRegularExpressionMatch match = pattern.match(name);

    if (!match.hasMatch()) {
        return false;
    }

    date = QDate::fromString(match.captured(1), "yyyyMMdd");
    sequence = match.captured(2).toInt();
    return date.isValid();
}

QString MessageCenterArchive::path(const QString& name) const
{
    return QDir(m_folder).filePath(name);
}

bool MessageCenterArchive::scan(const QString& path, File& file)
{
    QFile f(path);

    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }

    file.m_first = -1;
    file.m_last = -1;
    file.m_lines = 0;
    qint64 size = 0;

    while (!f.atEnd())
    {
        QByteArray line = f.readLine();

        // A last line without its end is being written: not yet a line of the file
        if (!line.endsWith('\n')) {
            break;
        }

        size += line.size();
        const qint64 t = lineTime(line);

        if (t >= 0)
        {
            file.m_first = (file.m_first < 0) ? t : std::min(file.m_first, t);
            file.m_last = std::max(file.m_last, t);
        }

        file.m_lines++;
    }

    file.m_size = size;
    file.m_modified = QFileInfo(path).lastModified().toMSecsSinceEpoch();
    return true;
}

// The index: the time range of each file, as long as the file is the same (size and time of the last change)
void MessageCenterArchive::loadIndex()
{
    QHash<QString, File> cached;
    QFile indexFile(path(indexName));

    if (indexFile.open(QIODevice::ReadOnly))
    {
        for (const QJsonValue& v : QJsonDocument::fromJson(indexFile.readAll()).array())
        {
            QJsonObject o = v.toObject();
            File f;
            f.m_name = o.value("name").toString();
            f.m_size = (qint64) o.value("size").toDouble();
            f.m_modified = (qint64) o.value("modified").toDouble();
            f.m_first = (qint64) o.value("first").toDouble(-1);
            f.m_last = (qint64) o.value("last").toDouble(-1);
            f.m_lines = o.value("lines").toInt();
            cached.insert(f.m_name, f);
        }
    }

    m_files.clear();
    QStringList names = QDir(m_folder).entryList(QStringList("*.jsonl"), QDir::Files, QDir::Name);

    for (const QString& name : names)
    {
        File file;

        if (!parseFileName(name, file.m_date, file.m_sequence)) {
            continue;
        }

        file.m_name = name;
        QFileInfo info(path(name));
        auto it = cached.constFind(name);

        if ((it != cached.constEnd()) && (it->m_size == info.size()) && (it->m_modified == info.lastModified().toMSecsSinceEpoch()))
        {
            file.m_size = it->m_size;
            file.m_modified = it->m_modified;
            file.m_first = it->m_first;
            file.m_last = it->m_last;
            file.m_lines = it->m_lines;
        }
        else if (!scan(path(name), file))
        {
            continue;
        }

        m_files.append(file);
    }

    std::sort(m_files.begin(), m_files.end(), [](const File& a, const File& b) {
        return (a.m_date < b.m_date) || ((a.m_date == b.m_date) && (a.m_sequence < b.m_sequence));
    });
}

void MessageCenterArchive::saveIndex() const
{
    QJsonArray array;

    for (const File& f : m_files)
    {
        QJsonObject o;
        o["name"] = f.m_name;
        o["size"] = (double) f.m_size;
        o["modified"] = (double) f.m_modified;
        o["first"] = (double) f.m_first;
        o["last"] = (double) f.m_last;
        o["lines"] = f.m_lines;
        array.append(o);
    }

    QSaveFile file(path(indexName));

    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

bool MessageCenterArchive::open(const QString& folder, const Limits& limits, qint64 now, QString& error)
{
    QMutexLocker locker(&m_mutex);

    if (!QDir().mkpath(folder))
    {
        error = QString("%1: the folder cannot be made").arg(folder);
        return false;
    }

    m_folder = folder;
    m_limits = limits;
    loadIndex();
    retain(now);
    saveIndex();
    return true;
}

void MessageCenterArchive::close()
{
    QMutexLocker locker(&m_mutex);

    if (!m_folder.isEmpty()) {
        saveIndex();
    }

    m_folder.clear();
    m_files.clear();
}

void MessageCenterArchive::setLimits(const Limits& limits, qint64 now)
{
    QMutexLocker locker(&m_mutex);
    m_limits = limits;

    if (!m_folder.isEmpty())
    {
        retain(now);
        saveIndex();
    }
}

// The files of the days older than kept go, then the oldest files over the size kept; the file being written stays.
// Only the files of the archive (their names) in its folder
void MessageCenterArchive::retain(qint64 now)
{
    const QDate today = QDateTime::fromMSecsSinceEpoch(now).date();
    const QDate oldest = today.addDays(-std::max(1, m_limits.m_days) + 1);
    qint64 total = 0;

    for (const File& f : m_files) {
        total += f.m_size;
    }

    while (m_files.size() > 1)
    {
        const File& f = m_files.first();

        if ((f.m_date >= oldest) && (total <= m_limits.m_bytes)) {
            break;
        }

        QFile::remove(path(f.m_name));
        total -= f.m_size;
        m_files.removeFirst();
    }
}

bool MessageCenterArchive::append(const MessageRecord& record, qint64 now, QString *error)
{
    QMutexLocker locker(&m_mutex);

    if (m_folder.isEmpty()) {
        return true;
    }

    const QDate today = QDateTime::fromMSecsSinceEpoch(now).date();
    const QByteArray line = record.toJsonLine().trimmed() + "\n";

    // A new file for a new day, or over the size of a file
    if (m_files.isEmpty() || (m_files.last().m_date != today) || (m_files.last().m_size + line.size() > m_limits.m_fileBytes))
    {
        File file;
        file.m_date = today;
        file.m_sequence = (!m_files.isEmpty() && (m_files.last().m_date == today)) ? m_files.last().m_sequence + 1 : 0;
        file.m_name = fileName(file.m_date, file.m_sequence);
        m_files.append(file);
        retain(now);
        saveIndex();
    }

    File& file = m_files.last();
    QFile f(path(file.m_name));

    if (!f.open(QIODevice::Append) || (f.write(line) != line.size()))
    {
        if (error) {
            *error = QString("%1: %2").arg(path(file.m_name), f.errorString());
        }
        return false;
    }

    f.close();
    file.m_size += line.size();
    file.m_lines++;
    file.m_modified = QFileInfo(path(file.m_name)).lastModified().toMSecsSinceEpoch();
    file.m_first = (file.m_first < 0) ? record.m_timestamp : std::min(file.m_first, record.m_timestamp);
    file.m_last = std::max(file.m_last, record.m_timestamp);
    return true;
}

QList<QByteArray> MessageCenterArchive::linesNewestFirst(const QString& path, qint64 size)
{
    QList<QByteArray> lines;
    QFile f(path);

    if (!f.open(QIODevice::ReadOnly)) {
        return lines;
    }

    const QByteArray data = f.read(size);
    int end = data.size();

    // Complete lines only
    while ((end > 0) && (data[end - 1] != '\n')) {
        end--;
    }

    int stop = end;

    for (int i = end - 2; i >= -1; i--)
    {
        if ((i < 0) || (data[i] == '\n'))
        {
            if (stop - (i + 1) > 1) {
                lines.append(data.mid(i + 1, stop - (i + 1)));
            }
            stop = i + 1;
        }
    }

    return lines;
}

// Newest file first, newest line first: the first line of an id is the record (its newest version), whether it is
// selected or not; the older lines of that id are its older versions
MessageCenterArchive::Result MessageCenterArchive::query(const Query& query) const
{
    QList<File> files;
    QString folder;
    {
        QMutexLocker locker(&m_mutex);
        files = m_files;
        folder = m_folder;
    }

    Result result;
    QSet<QString> seen;
    const QDir dir(folder);
    // The newest offset + limit records by their time (a record updated later has its first time): a heap, its top the
    // oldest of them
    const size_t keep = (size_t) std::max(0, query.m_offset) + (size_t) std::max(0, query.m_limit);
    // Records of the same time: the one written later (its newest version) is the newer. The heap keeps the order they
    // were read in (newest written first)
    typedef std::pair<MessageRecord, long> Read;
    auto newer = [](const Read& a, const Read& b) {
        return (a.first.m_timestamp > b.first.m_timestamp) || ((a.first.m_timestamp == b.first.m_timestamp) && (a.second < b.second));
    };
    std::vector<Read> heap;
    long order = 0;

    for (int i = files.size() - 1; i >= 0; i--)
    {
        const File& f = files[i];

        // A file whose records are all outside the range has none to show; its versions of the records of the range are
        // older than those of the files written after it (the range of a file is that of the times of its records)
        if (f.m_lines == 0) {
            continue;
        }
        if (((query.m_from >= 0) && (f.m_last >= 0) && (f.m_last < query.m_from)) || ((query.m_to >= 0) && (f.m_first > query.m_to))) {
            continue;
        }

        for (const QByteArray& line : linesNewestFirst(dir.filePath(f.m_name), f.m_size))
        {
            MessageRecord record;

            if (!MessageRecord::fromJson(QJsonDocument::fromJson(line).object(), record)) {
                continue;
            }
            if (!record.m_id.isEmpty())
            {
                if (seen.contains(record.m_id)) {
                    continue;
                }
                seen.insert(record.m_id);
            }
            if (((query.m_from >= 0) && (record.m_timestamp < query.m_from)) || ((query.m_to >= 0) && (record.m_timestamp > query.m_to))) {
                continue;
            }
            if (!query.m_filter.matches(record)) {
                continue;
            }

            result.m_total++;

            if (keep == 0) {
                continue;
            }
            Read read(record, order++);

            if (heap.size() < keep)
            {
                heap.push_back(read);
                std::push_heap(heap.begin(), heap.end(), newer);
            }
            else if (newer(read, heap.front()))
            {
                std::pop_heap(heap.begin(), heap.end(), newer);
                heap.back() = read;
                std::push_heap(heap.begin(), heap.end(), newer);
            }
        }
    }

    std::sort_heap(heap.begin(), heap.end(), newer);

    for (size_t i = (size_t) std::max(0, query.m_offset); i < heap.size(); i++) {
        result.m_records.push_back(heap[i].first);
    }

    return result;
}

std::vector<MessageRecord> MessageCenterArchive::newest(int count) const
{
    Query q;
    q.m_limit = count;
    Result r = query(q);
    std::reverse(r.m_records.begin(), r.m_records.end());
    return r.m_records;
}

bool MessageCenterArchive::clear(QString& error)
{
    QMutexLocker locker(&m_mutex);
    bool ok = true;

    for (const File& f : m_files)
    {
        if (!QFile::remove(path(f.m_name)))
        {
            error = QString("%1 cannot be removed").arg(path(f.m_name));
            ok = false;
        }
    }

    m_files.clear();

    if (!m_folder.isEmpty()) {
        saveIndex();
    }

    return ok;
}

qint64 MessageCenterArchive::bytes() const
{
    QMutexLocker locker(&m_mutex);
    qint64 total = 0;

    for (const File& f : m_files) {
        total += f.m_size;
    }

    return total;
}

int MessageCenterArchive::files() const
{
    QMutexLocker locker(&m_mutex);
    return m_files.size();
}
