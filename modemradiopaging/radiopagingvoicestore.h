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

#ifndef INCLUDE_RADIOPAGINGVOICESTORE_H
#define INCLUDE_RADIOPAGINGVOICESTORE_H

#include <QDateTime>
#include <QString>

#include "export.h"

// The voice files of the GSC voice pages in a directory: <alias>_rx_<time>.wav (received) or
// <alias>_tx_<time>.wav (sent).
// A file may be deleted when it is older than the days kept AND is not among the newest
// files kept (the days are 0: no file is deleted for its age). The files of every
// demodulator that shares the directory count together
class MODEMRADIOPAGING_API RadioPagingVoiceStore
{
public:
    struct Stats
    {
        int m_files = 0;
        qint64 m_bytes = 0;
        QDateTime m_oldest;                 //!< Invalid if no file
        QDateTime m_newest;
        int m_expired = 0;                  //!< Files that may be deleted
        qint64 m_expiredBytes = 0;
        qint64 m_freeBytes = -1;            //!< Free space of the disk, -1 if not known
    };

    //!< The alias as part of a file name: letters, digits, '.', '-' and '_' (others are '_')
    static QString fileAlias(const QString& alias);
    //!< Path of the file name up to the time, to give to WavFileRecord: <directory>/<alias>_<kind>
    //!< (kind "rx" for the voice received, "tx" for the voice sent)
    static QString filePrefix(const QString& directory, const QString& alias, const QString& kind);
    //!< radiopaging/voice/<kind> in the application data: received and sent voice are in
    //!< separate directories, with their own statistics and retention
    static QString defaultDirectory(const QString& kind);
    static Stats stats(const QString& directory, int keepDays, int keepMin, const QDateTime& now = QDateTime::currentDateTime());
    //!< Deletes the files that may be, the oldest first, at most maxDelete of them (-1: all);
    //!< the file given is never deleted. Returns the number deleted
    static int cleanup(const QString& directory, int keepDays, int keepMin, int maxDelete,
        const QString& keepFile = QString(), const QDateTime& now = QDateTime::currentDateTime());
    //!< The same in a thread of the global thread pool: the directory is listed and the files deleted away
    //!< from the thread of the caller (a DSP thread when a recording ends)
    static void cleanupInBackground(const QString& directory, int keepDays, int keepMin, int maxDelete, const QString& keepFile);
    static const int m_autoCleanupBatch = 5;    //!< Files deleted at most when a recording ends
    //!< A size for display: 1.5 MB
    static QString sizeText(qint64 bytes);
};

#endif // INCLUDE_RADIOPAGINGVOICESTORE_H
