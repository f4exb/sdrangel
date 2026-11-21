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

#ifndef INCLUDE_FEATURE_MESSAGECENTERARCHIVE_H_
#define INCLUDE_FEATURE_MESSAGECENTERARCHIVE_H_

#include <vector>

#include <QDate>
#include <QList>
#include <QMutex>
#include <QString>

#include "util/messagerecord.h"
#include "messagecenterengine.h"

// The messages of the log kept in files of a folder, for days: one line of JSON for each version of a record stored (a
// message updated later has a line again; the newest line of an id is the record). A file is a day (by the time it is
// written) up to a size: YYYYMMDD-NNN.jsonl, NNN from 000 for the files of a day. The oldest files go over the days or
// the size kept. A query reads only the files whose records are in its time range (an index of the time range of each
// file is kept in the folder, made again from the files when it does not match them).
class MessageCenterArchive
{
public:
    struct Limits
    {
        qint64 m_fileBytes = 4 * 1024 * 1024;       //!< A file is closed at this size, the next one starts
        int m_days = 30;                            //!< Files of older days go
        qint64 m_bytes = 500LL * 1024 * 1024;       //!< Then the oldest files go over this size
    };

    struct Query
    {
        qint64 m_from = -1;                 //!< Time of the records, ms since the epoch, -1: no limit
        qint64 m_to = -1;
        MessageCenterFilter m_filter;
        int m_offset = 0;                   //!< Of the newest first
        int m_limit = 100;
    };

    //! The total the query selects, and its records from m_offset, newest first, at most m_limit
    typedef MessageCenterArchiveResult Result;

    MessageCenterArchive();
    ~MessageCenterArchive();

    //! The folder of the files (made if needed); its index is checked against them. False and an error if it cannot be
    bool open(const QString& folder, const Limits& limits, qint64 now, QString& error);
    void close();
    bool isOpen() const { return !m_folder.isEmpty(); }
    const QString& folder() const { return m_folder; }
    void setLimits(const Limits& limits, qint64 now);

    //! A record stored (written at now, ms since the epoch: the day of its file)
    bool append(const MessageRecord& record, qint64 now, QString *error = nullptr);
    //! The records a query selects. Reads the files, not under the lock of the archive: can be called from any thread
    Result query(const Query& query) const;
    //! The newest records (the newest version of each), oldest first: the log when the message center starts
    std::vector<MessageRecord> newest(int count) const;
    //! Removes all its files
    bool clear(QString& error);
    qint64 bytes() const;
    int files() const;

    //! The name of a file: YYYYMMDD-NNN.jsonl
    static QString fileName(const QDate& date, int sequence);
    static bool parseFileName(const QString& name, QDate& date, int& sequence);

private:
    struct File
    {
        QString m_name;
        QDate m_date;
        int m_sequence = 0;
        qint64 m_size = 0;
        qint64 m_modified = 0;          //!< ms since the epoch, of the file system
        qint64 m_first = -1;            //!< Time range of its records, -1 if it has none
        qint64 m_last = -1;
        int m_lines = 0;
    };

    QString m_folder;
    Limits m_limits;
    QList<File> m_files;                //!< Oldest first; the last one is written
    mutable QMutex m_mutex;

    QString path(const QString& name) const;
    //! Reads a whole file: its time range and lines (an unterminated last line, being written, is not one)
    static bool scan(const QString& path, File& file);
    void loadIndex();
    void saveIndex() const;
    void retain(qint64 now);
    //! The lines of a file up to size, newest first
    static QList<QByteArray> linesNewestFirst(const QString& path, qint64 size);
};

#endif // INCLUDE_FEATURE_MESSAGECENTERARCHIVE_H_
