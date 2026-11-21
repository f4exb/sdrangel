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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QThreadPool>

#include <algorithm>

#include "radiopagingvoicestore.h"

namespace {

// The voice files of the directory (<alias>_rx_<time>.wav or <alias>_tx_<time>.wav), the oldest first
QFileInfoList voiceFiles(const QString& directory)
{
    QFileInfoList files = QDir(directory).entryInfoList(QStringList({"*_rx_*.wav", "*_tx_*.wav"}), QDir::Files);

    std::sort(files.begin(), files.end(), [](const QFileInfo& a, const QFileInfo& b) {
        if (a.lastModified() != b.lastModified()) {
            return a.lastModified() < b.lastModified();
        }

        return a.fileName() < b.fileName();
    });

    return files;
}

// The files that may be deleted: all but the newest keepMin, and older than keepDays
QFileInfoList expiredFiles(const QString& directory, int keepDays, int keepMin, const QDateTime& now, const QFileInfoList& files)
{
    (void) directory;
    QFileInfoList expired;

    if (keepDays <= 0) {
        return expired;
    }

    int candidates = (int) files.size() - std::max(0, keepMin);

    for (int i = 0; i < candidates; i++)
    {
        if (files[i].lastModified().secsTo(now) > (qint64) keepDays * 86400) {
            expired.append(files[i]);
        }
    }

    return expired;
}

} // namespace

QString RadioPagingVoiceStore::fileAlias(const QString& alias)
{
    QString out;

    for (const QChar& c : alias)
    {
        bool plain = (c.unicode() < 128) && (c.isLetterOrNumber() || (c == '.') || (c == '-') || (c == '_'));
        out.append(plain ? c : QChar('_'));
    }

    return out.isEmpty() ? QString("rx") : out;
}

QString RadioPagingVoiceStore::filePrefix(const QString& directory, const QString& alias, const QString& kind)
{
    return directory + "/" + fileAlias(alias) + "_" + kind;
}

QString RadioPagingVoiceStore::sizeText(qint64 bytes)
{
    if (bytes < 0) {
        return "unknown";
    }

    double value = bytes;
    const char *units[] = {"B", "kB", "MB", "GB", "TB"};
    int unit = 0;

    while ((value >= 1024.0) && (unit < 4))
    {
        value /= 1024.0;
        unit++;
    }

    return QString("%1 %2").arg(value, 0, 'f', unit == 0 ? 0 : 1).arg(units[unit]);
}

QString RadioPagingVoiceStore::defaultDirectory(const QString& kind)
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/radiopaging/voice/" + kind;
}

RadioPagingVoiceStore::Stats RadioPagingVoiceStore::stats(const QString& directory, int keepDays, int keepMin, const QDateTime& now)
{
    Stats stats;
    QFileInfoList files = voiceFiles(directory);

    for (const QFileInfo& file : files)
    {
        stats.m_files++;
        stats.m_bytes += file.size();

        if (!stats.m_oldest.isValid() || (file.lastModified() < stats.m_oldest)) {
            stats.m_oldest = file.lastModified();
        }
        if (!stats.m_newest.isValid() || (file.lastModified() > stats.m_newest)) {
            stats.m_newest = file.lastModified();
        }
    }

    for (const QFileInfo& file : expiredFiles(directory, keepDays, keepMin, now, files))
    {
        stats.m_expired++;
        stats.m_expiredBytes += file.size();
    }

    QStorageInfo storage(QDir(directory).exists() ? directory : QDir::homePath());

    if (storage.isValid()) {
        stats.m_freeBytes = storage.bytesAvailable();
    }

    return stats;
}

int RadioPagingVoiceStore::cleanup(const QString& directory, int keepDays, int keepMin, int maxDelete,
    const QString& keepFile, const QDateTime& now)
{
    int deleted = 0;
    QFileInfoList files = voiceFiles(directory);

    for (const QFileInfo& file : expiredFiles(directory, keepDays, keepMin, now, files))
    {
        if ((maxDelete >= 0) && (deleted >= maxDelete)) {
            break;
        }
        if (!keepFile.isEmpty() && (file.absoluteFilePath() == QFileInfo(keepFile).absoluteFilePath())) {
            continue;
        }
        if (QFile::remove(file.absoluteFilePath())) {
            deleted++;
        }
    }

    return deleted;
}

void RadioPagingVoiceStore::cleanupInBackground(const QString& directory, int keepDays, int keepMin, int maxDelete,
    const QString& keepFile)
{
    // A file deleted by another cleanup meanwhile is not deleted again (QFile::remove fails)
    QThreadPool::globalInstance()->start([directory, keepDays, keepMin, maxDelete, keepFile]() {
        cleanup(directory, keepDays, keepMin, maxDelete, keepFile);
    });
}
