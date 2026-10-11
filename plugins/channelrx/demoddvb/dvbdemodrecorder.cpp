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

#include <algorithm>

#include <QDir>
#include <QFileInfo>

#include "dvbdemodrecorder.h"

DVBDemodRecorder::DVBDemodRecorder() :
    m_mode(Multiplex),
    m_serviceId(0),
    m_started(false),
    m_transportStreamId(-1),
    m_pmtPid(-1),
    m_pmtVersion(-1),
    m_patContinuity(0),
    m_patVersion(0),
    m_patInVersion(-1),
    m_patLastSection(0),
    m_patServiceSeen(false),
    m_queued(0),
    m_stop(false),
    m_bytes(0),
    m_dropped(0),
    m_running(false)
{
}

DVBDemodRecorder::~DVBDemodRecorder()
{
    stop();
}

bool DVBDemodRecorder::start(const QString& fileName, Mode mode, int serviceId, QString& error)
{
    stop();
    QDir().mkpath(QFileInfo(fileName).absolutePath());
    m_file.setFileName(fileName);

    if (!m_file.open(QIODevice::WriteOnly))
    {
        error = m_file.errorString();
        return false;
    }

    m_mode = mode;
    m_serviceId = serviceId;
    m_started = false;
    m_patSections.reset();
    m_pmtSections.reset();
    m_transportStreamId = -1;
    m_pmtPid = -1;
    m_pmtVersion = -1;
    m_pids.reset();
    m_patInVersion = -1;
    m_patLastSection = 0;
    m_patSectionsSeen.reset();
    m_patServiceSeen = false;
    m_block.clear();
    m_block.reserve(m_blockSize);
    m_queue.clear();
    m_queued = 0;
    m_stop = false;
    m_bytes = 0;
    m_dropped = 0;
    m_fileName = fileName;
    m_error.clear();
    m_timer.start();
    m_running = true;
    m_thread = std::thread(&DVBDemodRecorder::writer, this);
    return true;
}

void DVBDemodRecorder::stop()
{
    if (!m_running) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_block.empty())
        {
            m_queued += m_block.size();
            m_queue.push_back(std::move(m_block));
            m_block.clear();
        }

        m_stop = true;
    }

    m_available.notify_one();
    m_thread.join();
    m_file.close();
    m_running = false;
}

DVBDemodRecorder::Status DVBDemodRecorder::getStatus() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Status status;
    status.m_fileName = m_fileName;
    status.m_serviceId = m_mode == Service ? m_serviceId : 0;
    status.m_bytes = m_bytes;
    status.m_seconds = m_timer.isValid() ? m_timer.elapsed() / 1000.0 : 0.0;
    status.m_dropped = m_dropped;
    status.m_error = m_error;
    return status;
}

// Writes queued blocks until stopped, then what's left
void DVBDemodRecorder::writer()
{
    while (true)
    {
        std::vector<uint8_t> block;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_available.wait(lock, [this]() { return m_stop || !m_queue.empty(); });

            if (m_queue.empty()) {
                return; // Stopped, with everything written
            }

            block = std::move(m_queue.front());
            m_queue.pop_front();
            m_queued -= block.size();
        }

        const qint64 written = m_file.write(reinterpret_cast<const char *>(block.data()), (qint64) block.size());

        if (written != (qint64) block.size())
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_error = m_file.errorString();
        }

        m_bytes += std::max<qint64>(0, written);
    }
}

// A packet to record, queued a block at a time
void DVBDemodRecorder::write(const uint8_t *packet)
{
    m_block.insert(m_block.end(), packet, packet + 188);

    if (m_block.size() < m_blockSize) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_queued + m_block.size() > m_maxQueued)
        {
            m_dropped += (qint64) m_block.size();
            m_block.clear();
            return;
        }

        m_queued += m_block.size();
        m_queue.push_back(std::move(m_block));
    }

    m_block = std::vector<uint8_t>();
    m_block.reserve(m_blockSize);
    m_available.notify_one();
}

void DVBDemodRecorder::feed(const uint8_t *p)
{
    if (!m_running || (p[0] != 0x47)) {
        return;
    }

    const int pid = ((p[1] & 0x1F) << 8) | p[2];
    const bool errored = (p[1] & 0x80) != 0;

    if (m_mode == Multiplex)
    {
        // From a PAT, so players find the services straight away
        m_started = m_started || ((pid == m_patPid) && (p[1] & 0x40));

        if (m_started) {
            write(p);
        }

        return;
    }

    // A service: find its PMT from the PAT, and its streams from the PMT
    if (!errored)
    {
        if (pid == m_patPid) {
            m_patSections.packet(p, [this](const uint8_t *s, int length) { pat(s, length); });
        } else if (pid == m_pmtPid) {
            m_pmtSections.packet(p, [this](const uint8_t *s, int length) { pmt(s, length); });
        }
    }

    if (pid == m_patPid)
    {
        // Each PAT is replaced by one listing only the service. Recording starts with the first, once the PMT is known
        if ((p[1] & 0x40) && (m_pmtVersion >= 0))
        {
            m_started = true;
            writePAT();
        }

        return;
    }

    if (m_started && (m_pids.test(pid) || (pid == m_sdtPid) || (pid == m_timePid))) {
        write(p);
    }
}

// program_association_section (ISO/IEC 13818-1 2.4.4.3): the service's PMT PID. In another transport stream (the
// device retuned while recording), the service's PMT PID and version can be the same as before but its streams not:
// its state's forgotten, so its PMT there is read, and if it's not there, nothing more of it is recorded
void DVBDemodRecorder::pat(const uint8_t *s, int length)
{
    // With section_syntax_indicator set, so its CRC was checked; current, not one to apply next
    if ((s[0] != 0x00) || !(s[1] & 0x80) || !(s[5] & 0x01) || (length < 12)) {
        return;
    }

    const int transportStreamId = (s[3] << 8) | s[4];
    const int version = (s[5] >> 1) & 0x1F;

    if ((m_transportStreamId >= 0) && (transportStreamId != m_transportStreamId))
    {
        forgetService();
        m_patInVersion = -1;
    }

    m_transportStreamId = transportStreamId;

    // This version's sections so far, and whether the service is in them
    if (version != m_patInVersion)
    {
        m_patInVersion = version;
        m_patSectionsSeen.reset();
        m_patServiceSeen = false;
    }

    m_patLastSection = s[7];
    m_patSectionsSeen.set(s[6]);

    for (int i = 8; i + 4 <= length - 4; i += 4)
    {
        const int program = (s[i] << 8) | s[i + 1];
        const int pid = ((s[i + 2] & 0x1F) << 8) | s[i + 3];

        if (program != m_serviceId) {
            continue;
        }

        m_patServiceSeen = true;

        if (pid != m_pmtPid)
        {
            // Its PMT moved (or the service is back): its streams may have too, and the old PIDs be another
            // service's now, so none are recorded until the new PMT's read
            forgetService();
            m_pmtPid = pid;
        }
    }

    // The whole PAT without the service (a part time service ending, the multiplex rearranged): its PIDs may be
    // another's now, so nothing more of it's recorded, until it comes back. Not from one section of several
    if (!m_patServiceSeen && (m_pmtPid >= 0) && ((int) m_patSectionsSeen.count() == m_patLastSection + 1))
    {
        bool all = true;

        for (int k = 0; k <= m_patLastSection; k++) {
            all = all && m_patSectionsSeen.test(k);
        }

        if (all) {
            forgetService();
        }
    }
}

void DVBDemodRecorder::forgetService()
{
    m_pids.reset();
    m_pmtPid = -1;
    m_pmtVersion = -1;
    m_pmtSections.reset();
    m_patVersion = (m_patVersion + 1) & 0x1F;
}

// TS_program_map_section (2.4.4.8): the PCR PID and elementary stream PIDs
void DVBDemodRecorder::pmt(const uint8_t *s, int length)
{
    // Current, not one to apply next: its streams aren't sent yet
    if ((s[0] != 0x02) || !(s[1] & 0x80) || !(s[5] & 0x01) || (length < 16) || ((((s[3] << 8) | s[4])) != m_serviceId)) {
        return;
    }

    const int version = (s[5] >> 1) & 0x1F;

    if (version == m_pmtVersion) {
        return;
    }

    m_pmtVersion = version;
    m_pids.reset();
    m_pids.set(m_pmtPid);
    const int pcrPid = ((s[8] & 0x1F) << 8) | s[9];

    if (pcrPid != 0x1FFF) {
        m_pids.set(pcrPid);
    }

    const int end = length - 4;
    int i = 12 + (((s[10] & 0x0F) << 8) | s[11]);

    while (i + 5 <= end)
    {
        // Not interactive data (MHEG, the Red Button): DSM-CC carousels and private sections, which can be a fifth of
        // the service's data. Private PES (0x06) is kept, as it carries subtitles, teletext and AC-3 audio
        const int streamType = s[i];
        const bool data = (streamType == 0x05) || ((streamType >= 0x0A) && (streamType <= 0x0D));

        if (!data) {
            m_pids.set(((s[i + 1] & 0x1F) << 8) | s[i + 2]);
        }

        i += 5 + (((s[i + 3] & 0x0F) << 8) | s[i + 4]);
    }
}

// A PAT with just the service (and its CRC), in one packet
void DVBDemodRecorder::writePAT()
{
    uint8_t packet[188];
    std::fill(packet, packet + 188, 0xFF);
    packet[0] = 0x47;
    packet[1] = 0x40;   // payload_unit_start_indicator, PID 0
    packet[2] = 0x00;
    packet[3] = 0x10 | (m_patContinuity & 0x0F);
    m_patContinuity++;
    packet[4] = 0x00;   // pointer_field

    uint8_t *s = packet + 5;
    const int sectionLength = 5 + 4 + 4; // After the length: header, one program, CRC
    s[0] = 0x00;
    s[1] = 0xB0 | ((sectionLength >> 8) & 0x0F);
    s[2] = sectionLength & 0xFF;
    s[3] = (m_transportStreamId >> 8) & 0xFF;
    s[4] = m_transportStreamId & 0xFF;
    s[5] = 0xC1 | ((m_patVersion & 0x1F) << 1);
    s[6] = 0x00;    // section_number
    s[7] = 0x00;    // last_section_number
    s[8] = (m_serviceId >> 8) & 0xFF;
    s[9] = m_serviceId & 0xFF;
    s[10] = 0xE0 | ((m_pmtPid >> 8) & 0x1F);
    s[11] = m_pmtPid & 0xFF;
    const uint32_t crc = dvbSectionCRC(s, 12);
    s[12] = (crc >> 24) & 0xFF;
    s[13] = (crc >> 16) & 0xFF;
    s[14] = (crc >> 8) & 0xFF;
    s[15] = crc & 0xFF;
    write(packet);
}
