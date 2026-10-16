///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DTVRECORDER_H
#define INCLUDE_DTVRECORDER_H

#include <atomic>
#include <bitset>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include <QElapsedTimer>
#include <QFile>
#include <QString>

#include "export.h"
#include "dtv/dtvsections.h"

// Records the transport stream to a .ts file: the whole multiplex, or one service. A service's recording has its
// PMT, elementary streams (video, every audio track, subtitles, teletext, but not interactive data) and PCR, the system's
// tables (setSystemPids(): DVB's SDT and time tables by default), and a
// PAT rewritten to list only it, so players don't offer services that aren't there. It starts at a PAT (once the
// service's PMT is known), so the file can be played from its start. Fed with packets in the demodulator's thread;
// the file is written in a thread of its own, so a slow disk never holds up demodulation (data is dropped instead)
class SDRBASE_API DTVRecorder
{
public:
    enum Mode { Multiplex = 0, Service = 1 };

    struct Status
    {
        QString m_fileName;
        int m_serviceId = 0;        //!< 0 for the multiplex
        qint64 m_bytes = 0;         //!< Written
        double m_seconds = 0.0;
        qint64 m_dropped = 0;       //!< Bytes the disk didn't keep up with
        QString m_error;
    };

    DTVRecorder();
    ~DTVRecorder();    //!< Stops, if recording

    //! The PIDs of the system's tables recorded with a service: DVB's SDT and TDT (0x11, 0x14) unless set (ATSC's
    //! PSIP base PID, 0x1FFB). Before start()
    void setSystemPids(const std::vector<int>& pids);
    //! False with the error if the file can't be created
    bool start(const QString& fileName, Mode mode, int serviceId, QString& error);
    void stop();
    void feed(const uint8_t *packet);
    Status getStatus() const;

private:
    static constexpr int m_patPid = 0x0000;
    static constexpr size_t m_blockSize = 188 * 512;         // Packets are queued for the writer this many bytes at a time
    static constexpr size_t m_maxQueued = 64 * 1024 * 1024;  // Beyond which data is dropped, rather than wait for the disk

    void writer();
    void write(const uint8_t *packet);
    void pat(const uint8_t *s, int length);
    void pmt(const uint8_t *s, int length);
    void writePAT();

    // Demodulator's thread
    Mode m_mode;
    int m_serviceId;
    bool m_started;             //!< Writing: from the first PAT, once the PMT is known
    DTVSectionAssembler m_patSections;
    DTVSectionAssembler m_pmtSections;
    int m_transportStreamId;
    int m_pmtPid;               //!< Of the service, -1 until known
    int m_pmtVersion;
    std::bitset<8192> m_pids;   //!< Recorded
    std::bitset<8192> m_systemPids; //!< Also recorded, with a service
    int m_patContinuity;
    int m_patVersion;           //!< Of the PAT written
    // The PAT received's current version, as its sections come: once all have, whether the service is in it
    int m_patInVersion;
    int m_patLastSection;
    std::bitset<256> m_patSectionsSeen;
    bool m_patServiceSeen;
    //! Forget the service's PMT and streams (none recorded until its PMT's read again)
    void forgetService();
    std::vector<uint8_t> m_block;   //!< Packets to queue for the writer

    // Writer's thread, and between
    QFile m_file;
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::condition_variable m_available;
    std::deque<std::vector<uint8_t>> m_queue;
    size_t m_queued;
    bool m_stop;
    std::atomic<qint64> m_bytes;
    std::atomic<qint64> m_dropped;
    QString m_fileName;
    QString m_error;
    QElapsedTimer m_timer;
    bool m_running;
};

#endif // INCLUDE_DTVRECORDER_H
