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
#include <tuple>
#include <cstdlib>
#include <iterator>
#include <utility>

#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSaveFile>
#include <set>
#include <QStandardPaths>

#include "dvbdemodepg.h"
#include "dvbdemodtext.h"

const int DVBDemodEPG::m_freesatBouquets[3] = {272, 280, 256};

// Two BCD digits, -1 if either isn't a digit
int DVBDemodEPG::bcd2(uint8_t b)
{
    return ((b >> 4) > 9) || ((b & 0x0F) > 9) ? -1 : (b >> 4) * 10 + (b & 0x0F);
}

// Modified Julian Date and BCD time (EN 300 468 annex C) to seconds since 1970. -1 if undefined (all 1s, as an NVOD
// reference event's), or not valid
qint64 DVBDemodEPG::dvbTime(const uint8_t *t)
{
    const int mjd = (t[0] << 8) | t[1];

    if ((mjd < m_mjd2000) || (mjd >= m_mjd2100)) { // Also 0, and 0xFFFF (undefined)
        return -1;
    }

    const int hours = bcd2(t[2]), minutes = bcd2(t[3]), seconds = bcd2(t[4]);

    if ((hours < 0) || (hours > 23) || (minutes < 0) || (minutes > 59) || (seconds < 0) || (seconds > 59)) {
        return -1;
    }

    return (qint64) (mjd - 40587) * 86400 + hours * 3600 + minutes * 60 + seconds;
}

// BCD hours, minutes and seconds, -1 if not valid
int DVBDemodEPG::bcdDuration(const uint8_t *d)
{
    const int hours = bcd2(d[0]), minutes = bcd2(d[1]), seconds = bcd2(d[2]);

    if ((hours < 0) || (minutes < 0) || (minutes > 59) || (seconds < 0) || (seconds > 59)) {
        return -1;
    }

    return hours * 3600 + minutes * 60 + seconds;
}

// Keys of the sections whose versions are kept (m_tableVersions, and the NIT's and BAT's contributions), one layout per
// table, so they can't collide:
//   NIT: table_id (0x40, 0x41) << 32 | network_id << 8 | section_number
//   BAT: 0x4A << 40 | bouquet_id << 8 | section_number
//   SDT: Freesat's own PID << 48 | table_id (0x42, 0x46) << 40 | original_network_id << 24 | transport_stream_id << 8
//        | section_number
// And of an EIT section, within its service's (ServiceData::m_versions), and of its events (StoredEvent::m_table: the
// source table): (table_id | m_freesatSource if from Freesat's own PIDs) << 8 | section_number
uint64_t DVBDemodEPG::nitSectionKey(int table, int networkId, int section)
{
    return ((uint64_t) table << 32) | ((uint64_t) (networkId & 0xFFFF) << 8) | (uint64_t) section;
}

uint64_t DVBDemodEPG::batSectionKey(int bouquetId, int section)
{
    return ((uint64_t) 0x4A << 40) | ((uint64_t) (bouquetId & 0xFFFF) << 8) | (uint64_t) section;
}

int DVBDemodEPG::batSectionBouquet(uint64_t sectionKey)
{
    return (int) ((sectionKey >> 8) & 0xFFFF);
}

uint64_t DVBDemodEPG::sdtSectionKey(bool freesat, int table, int networkId, int transportStreamId, int section)
{
    return ((uint64_t) (freesat ? 1 : 0) << 48) | ((uint64_t) table << 40) | ((uint64_t) (networkId & 0xFFFF) << 24)
        | ((uint64_t) (transportStreamId & 0xFFFF) << 8) | (uint64_t) section;
}

int DVBDemodEPG::eitSourceTable(int table, bool freesat)
{
    return table | (freesat ? m_freesatSource : 0);
}

int DVBDemodEPG::eitVersionKey(int sourceTable, int section)
{
    return (sourceTable << 8) | section;
}

DVBDemodEPG::DVBDemodEPG() :
    m_freesatRegion(0),
    m_networkId(-1),
    m_transportStreamId(-1),
    m_time(0),
    m_lastExpiry(0),
    m_modified(false),
    m_revision(0)
{
    m_clock.start();
}

void DVBDemodEPG::clear()
{
    QMutexLocker locker(&m_mutex);
    m_services.clear();
    m_servicesByIds.clear();
    m_transportStreams.clear();
    m_tableVersions.clear();
    m_nitChannels.clear();
    m_batServices.clear();
    m_batRegions.clear();
    m_sdtTables.clear();
    m_freesatChannels.clear();
    m_freesatRegions.clear();
    m_networkId = -1;
    m_transportStreamId = -1;
    m_time = 0;
    m_pendingTime = 0;
    m_timeReceived.invalidate();
    m_lastExpiry = 0;
    m_modified = false;
    m_revision++;
}

void DVBDemodEPG::feed(const uint8_t *p)
{
    // Now and then (by the clock, not the packets, so not when a file's read at speed), events are expired even if
    // the TDT isn't sent
    if ((++m_packets & 0x3FFF) == 0)
    {
        if (!m_expiryCheck.isValid())
        {
            m_expiryCheck.start();
        }
        else if (m_expiryCheck.elapsed() >= m_expiryCheckMs)
        {
            m_expiryCheck.restart();
            QMutexLocker locker(&m_mutex);
            periodicExpiry();
        }
    }

    if ((p[0] != 0x47) || (p[1] & 0x80)) { // Lost sync, or uncorrectable errors
        return;
    }

    const int pid = ((p[1] & 0x1F) << 8) | p[2];
    DVBDemodSectionAssembler *assembler;

    switch (pid)
    {
    case m_nitPid: assembler = &m_nitSections; break;
    case m_sdtPid: assembler = &m_sdtSections; break;
    case m_eitPid: assembler = &m_eitSections; break;
    case m_timePid: assembler = &m_timeSections; break;
    case m_freesatSiPid: assembler = &m_freesatSiSections[0]; break;
    case m_freesatSiPidOther: assembler = &m_freesatSiSections[1]; break;
    case m_freesatEitPid: assembler = &m_freesatEitSections[0]; break;
    case m_freesatEitPidOther: assembler = &m_freesatEitSections[1]; break;
    default: return;
    }

    assembler->packet(p, [this, pid](const uint8_t *s, int length) {
        if ((pid != m_timePid) && !(s[1] & 0x80)) {
            return; // NIT, SDT, EIT and BAT have the long form, with a CRC
        }

        QMutexLocker locker(&m_mutex);

        switch (pid)
        {
        case m_nitPid: nit(s, length); break;
        case m_sdtPid: sdt(s, length); break;
        case m_eitPid: eit(s, length); break;
        case m_timePid: time(s, length); break;
        case m_freesatSiPid:
        case m_freesatSiPidOther:
            if (s[0] == 0x4A) {
                bat(s, length);
            } else {
                sdt(s, length, true);
            }
            break;
        default: eit(s, length, true); break;
        }
    });
}

DVBDemodEPG::ServiceData& DVBDemodEPG::service(int networkId, int transportStreamId, int serviceId)
{
    auto inserted = m_services.try_emplace(serviceKey(networkId, transportStreamId, serviceId));
    ServiceData& data = inserted.first->second;

    if (inserted.second)
    {
        data.m_info.m_networkId = networkId;
        data.m_info.m_transportStreamId = transportStreamId;
        data.m_info.m_serviceId = serviceId;
        changed(data);
        // Indexed in transport stream id order, as m_services is, so lookup() chooses among equals as before
        std::vector<ServiceData *>& same = m_servicesByIds[key(networkId, serviceId)];
        auto position = std::upper_bound(same.begin(), same.end(), transportStreamId & 0xFFFF, [](int id, const ServiceData *other) {
            return id < (other->m_info.m_transportStreamId & 0xFFFF);
        });
        same.insert(position, &data);
    }

    return data;
}

// The transport stream being received's service with the ids, else one with a name, not removed, else any
const DVBDemodEPG::ServiceData *DVBDemodEPG::lookup(int networkId, int serviceId) const
{
    if ((networkId == m_networkId) && (m_transportStreamId >= 0))
    {
        auto it = m_services.find(serviceKey(networkId, m_transportStreamId, serviceId));

        if ((it != m_services.end()) && !it->second.m_info.m_removed) {
            return &it->second;
        }
    }

    auto same = m_servicesByIds.find(key(networkId, serviceId));

    if (same == m_servicesByIds.end()) {
        return nullptr;
    }

    const ServiceData *best = nullptr;
    int bestRank = -1;

    for (const ServiceData *data : same->second)
    {
        if (data->m_info.m_serviceId == serviceId)
        {
            const int r = rank(*data);

            if (r > bestRank)
            {
                best = data;
                bestRank = r;
            }
        }
    }

    return best;
}

int DVBDemodEPG::rank(const ServiceData& data) const
{
    const Service& info = data.m_info;
    const bool known = m_transportStreams.count(key(info.m_networkId, info.m_transportStreamId)) > 0;
    return (info.m_removed ? 0 : 1 << 28) + (info.m_name.isEmpty() ? 0 : 1 << 27) + (known ? 1 << 26 : 0)
        + std::min((int) data.m_schedule.size(), (1 << 26) - 1);
}

// The service with the id in the network being received, else any with it
const DVBDemodEPG::ServiceData *DVBDemodEPG::findService(int serviceId) const
{
    if (m_networkId >= 0)
    {
        const ServiceData *data = lookup(m_networkId, serviceId);

        if (data) {
            return data;
        }
    }

    for (const auto& entry : m_services)
    {
        if ((entry.second.m_info.m_serviceId == serviceId) && !entry.second.m_info.m_removed) {
            return &entry.second;
        }
    }

    return nullptr;
}

DVBDemodEPG::Service DVBDemodEPG::describe(const ServiceData& data) const
{
    Service service = data.m_info;
    service.m_actual = !service.m_removed && (service.m_networkId == m_networkId) && (service.m_transportStreamId == m_transportStreamId);
    service.m_events = (int) data.m_schedule.size();
    auto ts = m_transportStreams.find(key(service.m_networkId, service.m_transportStreamId));
    service.m_multiplexKnown = (service.m_transportStreamId >= 0) && (ts != m_transportStreams.end());
    service.m_frequency = service.m_multiplexKnown ? ts->second.m_frequency : 0;
    return service;
}

// True if a section is new or has changed, or hasn't been received for a while
bool DVBDemodEPG::newVersion(uint64_t sectionKey, int version)
{
    const qint64 now = m_clock.elapsed();
    auto it = m_tableVersions.find(sectionKey);

    if ((it != m_tableVersions.end()) && (it->second.m_version == version) && (now - it->second.m_seen < m_versionTimeout))
    {
        it->second.m_seen = now;
        return false;
    }

    SeenVersion& seen = m_tableVersions[sectionKey];
    seen.m_version = version;
    seen.m_seen = now;
    return true;
}

// network_information_section (5.2.1): logical channel numbers, from the logical_channel_descriptor (0x83) of the
// UK, Europe and Australia, for each transport stream of the network
void DVBDemodEPG::nit(const uint8_t *s, int length)
{
    if (((s[0] != 0x40) && (s[0] != 0x41)) || (length < 16) || !(s[5] & 1)) {
        return;
    }

    const uint64_t sectionKey = nitSectionKey(s[0], (s[3] << 8) | s[4], s[6]);

    if (!newVersion(sectionKey, (s[5] >> 1) & 0x1F)) {
        return;
    }

    const int end = length - 4;
    int i = 8;
    i += 2 + (((s[i] & 0x0F) << 8) | s[i + 1]);

    if (i + 2 > end) {
        return;
    }

    // What the section's last version numbered: those this one doesn't lose their numbers, below
    std::map<uint64_t, int> previous;
    previous.swap(m_nitChannels[sectionKey]);
    std::map<uint64_t, int>& numbered = m_nitChannels[sectionKey];
    const int loopEnd = std::min(end, i + 2 + (((s[i] & 0x0F) << 8) | s[i + 1]));
    i += 2;

    while (i + 6 <= loopEnd)
    {
        const int transportStreamId = (s[i] << 8) | s[i + 1];
        const int networkId = (s[i + 2] << 8) | s[i + 3];
        const int descriptorsEnd = std::min(loopEnd, i + 6 + (((s[i + 4] & 0x0F) << 8) | s[i + 5]));
        i += 6;

        while (i + 2 <= descriptorsEnd)
        {
            const int descriptorEnd = std::min(descriptorsEnd, i + 2 + s[i + 1]);

            if (s[i] == 0x83)
            {
                for (int j = i + 2; j + 4 <= descriptorEnd; j += 4)
                {
                    const int serviceId = (s[j] << 8) | s[j + 1];
                    ServiceData& data = service(networkId, transportStreamId, serviceId);
                    Service& info = data.m_info;
                    info.m_channel = ((s[j + 2] & 0x03) << 8) | s[j + 3];
                    info.m_visible = (s[j + 2] & 0x80) != 0;
                    numbered[serviceKey(networkId, transportStreamId, serviceId)] = info.m_channel;
                    m_modified = true;
                    changed(data);
                }
            }
            else if ((s[i] == 0x5A) || ((s[i] == 0x7F) && (descriptorEnd > i + 2) && (s[i + 2] == 0x04)))
            {
                deliverySystem(networkId, transportStreamId, s[0] == 0x40, s + i + 2, descriptorEnd - i - 2, s[i]);
            }
            else if (s[i] == 0x43)
            {
                satelliteDeliverySystem(networkId, transportStreamId, s[0] == 0x40, s + i + 2, descriptorEnd - i - 2);
            }

            i = descriptorEnd;
        }

        i = descriptorsEnd;
    }

    // Services the last version numbered and this doesn't (nor another section), still with that number, have none now
    for (const auto& entry : previous)
    {
        if (numbered.count(entry.first)) {
            continue;
        }

        const bool elsewhere = std::any_of(m_nitChannels.begin(), m_nitChannels.end(), [&entry, sectionKey](const auto& other) {
            return (other.first != sectionKey) && other.second.count(entry.first);
        });
        auto it = m_services.find(entry.first);

        if (!elsewhere && (it != m_services.end()) && (it->second.m_info.m_channel == entry.second))
        {
            it->second.m_info.m_channel = -1;
            it->second.m_info.m_visible = true;
            m_modified = true;
            changed(it->second);
        }
    }
}

// terrestrial_delivery_system_descriptor (6.2.13.4) or T2_delivery_system_descriptor (6.4.6.3), of a transport stream
// in the NIT. A T2 descriptor without cells has only the PLP. The actual network's NIT takes precedence over others',
// and a NIT's frequency over one learned by tuning
void DVBDemodEPG::deliverySystem(int networkId, int transportStreamId, bool actual, const uint8_t *d, int length, int tag)
{
    TransportStream& ts = m_transportStreams[key(networkId, transportStreamId)];

    if (ts.m_actual && !actual) {
        return;
    }

    ts.m_networkId = networkId;
    ts.m_transportStreamId = transportStreamId;
    ts.m_actual = actual;
    static const int bandwidths[8] = {8000000, 7000000, 6000000, 5000000, 10000000, 1712000, 8000000, 8000000};

    if ((tag == 0x5A) && (length >= 5))
    {
        const qint64 frequency = (qint64) (((uint32_t) d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3]) * 10;

        if (frequency > 0)
        {
            ts.m_frequency = frequency;
            ts.m_learned = false;
            ts.m_bandwidth = bandwidths[d[4] >> 5];
            ts.m_lowPriority = !(d[4] & 0x10); // priority: 1 for HP (and non-hierarchical modes)
        }
    }
    else if ((tag == 0x7F) && (length >= 4)) // d[0] is the extension tag
    {
        ts.m_t2 = true;
        ts.m_plpId = d[1];

        if (length >= 8)
        {
            const bool tfs = d[5] & 0x01;
            ts.m_bandwidth = bandwidths[(d[4] >> 2) & 0x07];

            // The first cell's centre frequency, or the first of its time-frequency slicing frequencies
            qint64 frequency = 0;

            if (!tfs && (length >= 12)) {
                frequency = (qint64) (((uint32_t) d[8] << 24) | (d[9] << 16) | (d[10] << 8) | d[11]) * 10;
            } else if (tfs && (length >= 13) && (d[8] >= 4)) {
                frequency = (qint64) (((uint32_t) d[9] << 24) | (d[10] << 16) | (d[11] << 8) | d[12]) * 10;
            }

            if (frequency > 0)
            {
                ts.m_frequency = frequency;
                ts.m_learned = false;
            }
        }
    }

    m_modified = true;
    m_revision++;
}

// Binary coded decimal digits, MSB first
qint64 DVBDemodEPG::bcd(const uint8_t *d, int digits)
{
    qint64 value = 0;

    for (int i = 0; i < digits; i++)
    {
        const int digit = (d[i / 2] >> ((i & 1) ? 0 : 4)) & 0x0f;

        if (digit > 9) {
            return -1;
        }

        value = value * 10 + digit;
    }

    return value;
}

// satellite_delivery_system_descriptor (6.2.13.2), of a transport stream in the NIT: the downlink frequency (8 digits,
// 10 kHz units), orbital position (4 digits, tenths of a degree), east or west, polarisation, the roll-off and DVB-S or
// S2, modulation, symbol rate (7 digits, 100 S/s units) and inner code rate
void DVBDemodEPG::satelliteDeliverySystem(int networkId, int transportStreamId, bool actual, const uint8_t *d, int length)
{
    if (length < 11) {
        return;
    }

    TransportStream& ts = m_transportStreams[key(networkId, transportStreamId)];

    if (ts.m_actual && !actual) {
        return;
    }

    const qint64 frequency = bcd(d, 8);
    const qint64 position = bcd(d + 4, 4);
    const qint64 symbolRate = bcd(d + 7, 7);

    if ((frequency <= 0) || (position < 0) || (symbolRate <= 0)) {
        return;
    }

    // An S2X low roll-off learned from the BBHEADERs is kept: the descriptor can't say it (0.35, 0.25 or 0.20 only)
    const bool lowRollOff = ts.m_satellite && ts.m_s2 && (ts.m_rollOff >= 4)
        && (std::abs(ts.m_frequency - frequency * 10000) <= symbolRate * 100 / 2);
    ts.m_networkId = networkId;
    ts.m_transportStreamId = transportStreamId;
    ts.m_actual = actual;
    ts.m_frequency = frequency * 10000;
    ts.m_learned = false;
    ts.m_satellite = true;
    ts.m_orbitalPosition = (int) position * ((d[6] & 0x80) ? 1 : -1);
    ts.m_polarisation = (d[6] >> 5) & 0x03;
    ts.m_s2 = (d[6] & 0x04) != 0;

    if (!lowRollOff || !ts.m_s2) {
        ts.m_rollOff = !ts.m_s2 ? 0 : ((d[6] >> 3) & 0x03) < 3 ? (d[6] >> 3) & 0x03 : -1; // 3 is reserved
    }

    ts.m_symbolRate = (int) (symbolRate * 100);
    ts.m_fecInner = d[10] & 0x0f;
    ts.m_t2 = false;
    ts.m_plpId = -1;
    m_modified = true;
    m_revision++;
}

void DVBDemodEPG::setSatelliteFrequency(qint64 frequency, int symbolRate, bool s2, int rollOff, int isi)
{
    QMutexLocker locker(&m_mutex);

    if ((m_networkId < 0) || (m_transportStreamId < 0) || (frequency <= 0)) {
        return;
    }

    TransportStream& ts = m_transportStreams[key(m_networkId, m_transportStreamId)];
    const bool sameCarrier = (ts.m_frequency > 0) && (std::abs(frequency - ts.m_frequency) <= symbolRate / 2);

    // The input stream it's in (a delivery system descriptor doesn't say) and the roll-off, if not known, whichever
    // the frequency's from. An S2X low roll-off (codes 4 to 6) is taken over the descriptor's, which can't say it
    const bool lowRollOff = (rollOff >= 4) && (ts.m_rollOff != rollOff);

    if (sameCarrier && ((ts.m_isi != isi) || ((ts.m_rollOff < 0) && (rollOff >= 0)) || lowRollOff))
    {
        ts.m_isi = isi;
        ts.m_rollOff = (ts.m_rollOff < 0) || lowRollOff ? rollOff : ts.m_rollOff;
        m_modified = true;
        m_revision++;
    }

    if ((ts.m_frequency > 0) && (!ts.m_learned || sameCarrier)) {
        return;
    }

    ts.m_networkId = m_networkId;
    ts.m_transportStreamId = m_transportStreamId;
    ts.m_frequency = frequency;
    ts.m_satellite = true;
    ts.m_s2 = s2;
    ts.m_symbolRate = symbolRate;
    ts.m_rollOff = s2 ? rollOff : 0;
    ts.m_isi = isi;
    ts.m_learned = true;
    m_modified = true;
    m_revision++;
}

// bouquet_association_section (5.2.2) on Freesat's PIDs: its regions' names (descriptor 0xD4 in the first loop: region
// id 16, language 24, name length 8, name), and for each transport stream, its services' channel numbers by region
// (descriptor 0xD3: service id 16, 16 unknown, length 8, then for each region 4 unknown, channel number 12, region 16)
void DVBDemodEPG::bat(const uint8_t *s, int length)
{
    if ((length < 16) || !(s[5] & 1)) {
        return;
    }

    const int bouquetId = (s[3] << 8) | s[4];
    const uint64_t sectionKey = batSectionKey(bouquetId, s[6]);

    if (!newVersion(sectionKey, (s[5] >> 1) & 0x1F)) {
        return;
    }

    // Whether another section of the bouquet names a region, or numbers a service, so it's kept when this one no
    // longer does
    auto elsewhere = [bouquetId, sectionKey](const auto& sections, auto item) {
        return std::any_of(sections.begin(), sections.end(), [&](const auto& other) {
            return (other.first != sectionKey) && (batSectionBouquet(other.first) == bouquetId) && other.second.count(item);
        });
    };

    const int end = length - 4;
    int i = 8;
    const int firstEnd = std::min(end, i + 2 + (((s[i] & 0x0F) << 8) | s[i + 1]));
    i += 2;
    std::set<int> previousRegions;
    previousRegions.swap(m_batRegions[sectionKey]);
    std::set<int>& regions = m_batRegions[sectionKey];

    while (i + 2 <= firstEnd)
    {
        const int descriptorEnd = std::min(firstEnd, i + 2 + s[i + 1]);

        if (s[i] == m_freesatRegionTag)
        {
            for (int j = i + 2; j + 6 <= descriptorEnd; )
            {
                const int region = (s[j] << 8) | s[j + 1];
                const int nameLength = std::min((int) s[j + 5], descriptorEnd - j - 6);
                m_freesatRegions[{bouquetId, region}] = DVBDemodText::decode(s + j + 6, nameLength);
                regions.insert(region);
                j += 6 + nameLength;
            }
        }

        i = descriptorEnd;
    }

    // Regions the section's last version named, and this doesn't
    for (int region : previousRegions)
    {
        if (!regions.count(region) && !elsewhere(m_batRegions, region)) {
            m_freesatRegions.erase({bouquetId, region});
        }
    }

    i = firstEnd;

    if (i + 2 > end) {
        return;
    }

    const int loopEnd = std::min(end, i + 2 + (((s[i] & 0x0F) << 8) | s[i + 1]));
    i += 2;
    bool changed = false;
    std::set<uint64_t> previousServices;
    previousServices.swap(m_batServices[sectionKey]);
    std::set<uint64_t>& numbered = m_batServices[sectionKey];

    while (i + 6 <= loopEnd)
    {
        const int transportStreamId = (s[i] << 8) | s[i + 1];
        const int networkId = (s[i + 2] << 8) | s[i + 3];
        const int descriptorsEnd = std::min(loopEnd, i + 6 + (((s[i + 4] & 0x0F) << 8) | s[i + 5]));
        i += 6;

        while (i + 2 <= descriptorsEnd)
        {
            const int descriptorEnd = std::min(descriptorsEnd, i + 2 + s[i + 1]);

            if (s[i] == m_freesatChannelTag)
            {
                for (int j = i + 2; j + 5 <= descriptorEnd; )
                {
                    const int serviceId = (s[j] << 8) | s[j + 1];
                    const int entriesEnd = std::min(descriptorEnd, j + 5 + s[j + 4]);
                    numbered.insert(serviceKey(networkId, transportStreamId, serviceId));
                    std::vector<FreesatChannel>& channels = m_freesatChannels[serviceKey(networkId, transportStreamId, serviceId)];
                    channels.erase(std::remove_if(channels.begin(), channels.end(), [bouquetId](const FreesatChannel& c) {
                        return c.m_bouquetId == bouquetId;
                    }), channels.end());

                    for (int k = j + 5; k + 4 <= entriesEnd; k += 4)
                    {
                        FreesatChannel channel;
                        channel.m_bouquetId = bouquetId;
                        channel.m_channel = ((s[k] & 0x0F) << 8) | s[k + 1];
                        channel.m_region = (s[k + 2] << 8) | s[k + 3];
                        channels.push_back(channel);
                    }

                    changed = true;
                    j = entriesEnd;
                }
            }

            i = descriptorEnd;
        }

        i = descriptorsEnd;
    }

    // Services the section's last version numbered, and this doesn't: the bouquet's numbers for them are removed
    std::set<uint64_t> dropped;

    for (uint64_t previous : previousServices)
    {
        if (numbered.count(previous) || elsewhere(m_batServices, previous)) {
            continue;
        }

        auto it = m_freesatChannels.find(previous);

        if (it != m_freesatChannels.end())
        {
            std::vector<FreesatChannel>& channels = it->second;
            channels.erase(std::remove_if(channels.begin(), channels.end(), [bouquetId](const FreesatChannel& c) {
                return c.m_bouquetId == bouquetId;
            }), channels.end());

            if (channels.empty()) {
                m_freesatChannels.erase(it);
            }
        }

        dropped.insert(previous);
        changed = true;
    }

    if (changed) {
        assignFreesatChannels(dropped);
    }
}

// The bouquet whose numbers are used: England HD's, G2's or SD's, else the first seen
int DVBDemodEPG::freesatBouquet(const std::set<int>& bouquets)
{
    for (int bouquet : m_freesatBouquets)
    {
        if (bouquets.count(bouquet)) {
            return bouquet;
        }
    }

    return bouquets.empty() ? -1 : *bouquets.begin();
}

// Each service's channel number: its number in the region, else in the default region. A service numbered only in
// the default region, with a number the region gives another service, is hidden, as it's replaced there
void DVBDemodEPG::assignFreesatChannels(const std::set<uint64_t>& dropped)
{
    std::set<int> bouquets;

    for (const auto& entry : m_freesatChannels)
    {
        for (const FreesatChannel& c : entry.second) {
            bouquets.insert(c.m_bouquetId);
        }
    }

    const int bouquet = freesatBouquet(bouquets);
    const int region = m_freesatRegion > 0 ? m_freesatRegion : m_freesatDefaultRegion;
    std::set<int> regional;

    for (const auto& entry : m_freesatChannels)
    {
        for (const FreesatChannel& c : entry.second)
        {
            if ((c.m_bouquetId == bouquet) && (c.m_region == region) && (region != m_freesatDefaultRegion)) {
                regional.insert(c.m_channel);
            }
        }
    }

    std::set<uint64_t> assigned;

    for (const auto& entry : m_freesatChannels)
    {
        int inRegion = -1, inDefault = -1;

        for (const FreesatChannel& c : entry.second)
        {
            if (c.m_bouquetId != bouquet) {
                continue;
            }

            if (c.m_region == region) {
                inRegion = c.m_channel;
            } else if ((c.m_region == m_freesatDefaultRegion) || (c.m_region == 0)) {
                inDefault = c.m_channel;
            }
        }

        if ((inRegion < 0) && (inDefault < 0)) {
            continue;
        }

        ServiceData& data = service((int) (entry.first >> 32), (int) ((entry.first >> 16) & 0xFFFF), (int) (entry.first & 0xFFFF));
        Service& info = data.m_info;
        info.m_channel = inRegion >= 0 ? inRegion : inDefault;
        info.m_visible = (inRegion >= 0) || !regional.count(inDefault);
        assigned.insert(entry.first);
        changed(data);
    }

    // Those the BAT no longer numbers (in the bouquet used)
    for (uint64_t droppedKey : dropped)
    {
        auto it = m_services.find(droppedKey);

        if (!assigned.count(droppedKey) && (it != m_services.end()))
        {
            it->second.m_info.m_channel = -1;
            it->second.m_info.m_visible = true;
            changed(it->second);
        }
    }

    m_modified = true;
    m_revision++;
}

void DVBDemodEPG::setFreesatRegion(int region)
{
    QMutexLocker locker(&m_mutex);

    if (region != m_freesatRegion)
    {
        m_freesatRegion = region;
        assignFreesatChannels();
    }
}

std::vector<std::pair<int, QString>> DVBDemodEPG::getFreesatRegions() const
{
    QMutexLocker locker(&m_mutex);
    std::set<int> bouquets;

    for (const auto& region : m_freesatRegions) {
        bouquets.insert(region.first.first);
    }

    const int bouquet = freesatBouquet(bouquets);
    std::vector<std::pair<int, QString>> regions;

    for (const auto& region : m_freesatRegions)
    {
        if (region.first.first == bouquet) {
            regions.emplace_back(region.first.second, region.second);
        }
    }

    return regions;
}

// service_description_section (5.2.3): service names and types, of this transport stream (0x42) or others (0x46)
void DVBDemodEPG::sdt(const uint8_t *s, int length, bool freesat)
{
    // A section_number after the last_section_number is corrupt, and would make the table seem complete
    if (((s[0] != 0x42) && (s[0] != 0x46)) || (length < 15) || !(s[5] & 1) || (s[6] > s[7])) {
        return;
    }

    const int transportStreamId = (s[3] << 8) | s[4];
    const int networkId = (s[8] << 8) | s[9];

    if ((s[0] == 0x42) && !freesat && ((transportStreamId != m_transportStreamId) || (networkId != m_networkId)))
    {
        m_transportStreamId = transportStreamId;
        m_networkId = networkId;
        m_revision++;
    }

    // Freesat's own SDT's versions apart from the standard one's: it lists other services, so the same version of
    // one doesn't say the other's section is unchanged
    const uint64_t sectionKey = sdtSectionKey(freesat, s[0], networkId, transportStreamId, s[6]);
    const int version = (s[5] >> 1) & 0x1F;

    if (!newVersion(sectionKey, version)) {
        return;
    }

    // The SDT actual's version as its sections come, so once they all have, the services it left out are known (not
    // others': they may list only some of a transport stream's services, as Freesat's on its own PIDs does)
    const bool reconcile = (s[0] == 0x42) && !freesat;
    SdtTable unused;
    SdtTable& table = reconcile ? m_sdtTables[key(networkId, transportStreamId)] : unused;

    if (table.m_version != version)
    {
        table = SdtTable();
        table.m_version = version;
    }

    table.m_lastSection = s[7];
    table.m_sections.insert(s[6]);
    const int end = length - 4;
    int i = 11;

    while (i + 5 <= end)
    {
        const int serviceId = (s[i] << 8) | s[i + 1];
        const int descriptorsEnd = std::min(end, i + 5 + (((s[i + 3] & 0x0F) << 8) | s[i + 4]));
        ServiceData& data = service(networkId, transportStreamId, serviceId);
        table.m_serviceIds.insert(serviceId);

        if (data.m_info.m_removed)
        {
            data.m_info.m_removed = false;
            m_modified = true;
            changed(data);
        }

        i += 5;

        while (i + 2 <= descriptorsEnd)
        {
            const int descriptorEnd = std::min(descriptorsEnd, i + 2 + s[i + 1]);

            if ((s[i] == 0x48) && (i + 4 <= descriptorEnd)) // service_descriptor
            {
                const uint8_t *d = s + i + 2;
                const int dlength = descriptorEnd - i - 2;
                const int providerLength = d[1];

                if (2 + providerLength + 1 <= dlength)
                {
                    const int nameLength = std::min((int) d[2 + providerLength], dlength - 3 - providerLength);
                    data.m_info.m_type = d[0];
                    data.m_info.m_provider = DVBDemodText::decode(d + 2, providerLength);
                    data.m_info.m_name = DVBDemodText::decode(d + 3 + providerLength, nameLength);
                    m_modified = true;
                    changed(data);
                }
            }

            i = descriptorEnd;
        }

        i = descriptorsEnd;
    }

    // Complete, every section from 0 to the last: the transport stream's services it doesn't list are no longer
    // broadcast there (kept, with their schedules, in case they come back, but not listed, nor taken as in the
    // transport stream received)
    if (reconcile && ((int) table.m_sections.size() == table.m_lastSection + 1) && (*table.m_sections.rbegin() == table.m_lastSection))
    {
        auto it = m_services.lower_bound(serviceKey(networkId, transportStreamId, 0));
        const auto last = m_services.upper_bound(serviceKey(networkId, transportStreamId, 0xFFFF));

        for (; it != last; ++it)
        {
            Service& info = it->second.m_info;

            if (!info.m_removed && !table.m_serviceIds.count(info.m_serviceId))
            {
                info.m_removed = true;
                m_modified = true;
                changed(it->second);
            }
        }
    }
}

// One event of an event_information_section, with its descriptors (s: from the event_id, length: of it all)
bool DVBDemodEPG::parseEvent(const uint8_t *s, int length, Event& event)
{
    event.m_eventId = (s[0] << 8) | s[1];
    event.m_start = dvbTime(s + 2);
    event.m_duration = bcdDuration(s + 7);
    event.m_runningStatus = s[10] >> 5;

    // Not an undefined start (as an NVOD reference event's), nor a time or duration that isn't BCD: as all 1s, it
    // would be about a week, and replace the events it overlaps
    if ((event.m_start < 0) || (event.m_duration < 0)) {
        return false;
    }

    QString shortText;
    // Extended event descriptors, by language and descriptor_number: their items, and the raw bytes of their text,
    // which continues from one descriptor to the next (a character table selected only in the first, a multibyte
    // character split), so is decoded once
    struct ExtendedPart { QString m_items; std::vector<uint8_t> m_text; };
    std::map<uint32_t, std::map<int, ExtendedPart>> extended;

    for (int i = 12; i + 2 <= length; )
    {
        const int tag = s[i];
        const int end = std::min(length, i + 2 + s[i + 1]);
        const uint8_t *d = s + i + 2;
        const int dlength = end - i - 2;

        if ((tag == 0x4D) && (dlength >= 5)) // short_event_descriptor: language, name, text
        {
            const int nameLength = std::min((int) d[3], dlength - 4);
            event.m_title = DVBDemodText::decode(d + 4, nameLength);

            if (4 + nameLength < dlength)
            {
                const int textLength = std::min((int) d[4 + nameLength], dlength - 5 - nameLength);
                shortText = DVBDemodText::decode(d + 5 + nameLength, textLength);
            }
        }
        else if ((tag == 0x4E) && (dlength >= 6)) // extended_event_descriptor: items, then text
        {
            const int number = d[0] >> 4;
            const uint32_t language = (d[1] << 16) | (d[2] << 8) | d[3];
            const int itemsLength = std::min((int) d[4], dlength - 5);
            ExtendedPart& part = extended[language][number];

            for (int j = 5; j < 5 + itemsLength; )
            {
                const int descriptionLength = d[j];
                const int itemLength = j + 1 + descriptionLength < 5 + itemsLength ? d[j + 1 + descriptionLength] : 0;

                if (j + 2 + descriptionLength + itemLength > 5 + itemsLength) {
                    break;
                }

                part.m_items += QString("%1: %2\n").arg(DVBDemodText::decode(d + j + 1, descriptionLength))
                    .arg(DVBDemodText::decode(d + j + 2 + descriptionLength, itemLength));
                j += 2 + descriptionLength + itemLength;
            }

            const int textStart = 5 + itemsLength;

            if (textStart < dlength)
            {
                const int textLength = std::max(0, std::min((int) d[textStart], dlength - textStart - 1));
                part.m_text.insert(part.m_text.end(), d + textStart + 1, d + textStart + 1 + textLength);
            }
        }
        else if ((tag == 0x54) && (dlength >= 2)) // content_descriptor
        {
            if (event.m_genre < 0) {
                event.m_genre = d[0];
            }
        }
        else if (tag == 0x55) // parental_rating_descriptor
        {
            for (int j = 0; j + 4 <= dlength; j += 4)
            {
                if ((d[j + 3] >= 0x01) && (d[j + 3] <= 0x0F)) {
                    event.m_minimumAge = d[j + 3] + 3;
                }
            }
        }
        else if ((tag == 0x50) && (dlength >= 6)) // component_descriptor
        {
            // stream_content_ext, stream_content, component_type (table 26). Content 9 is HEVC video with extension 0
            // (HD and UHD), AC-4 audio with 1, TTML subtitles with 2
            const int contentExt = d[0] >> 4;
            const int content = d[0] & 0x0F;
            const int type = d[1];

            if ((content == 0x03) && (((type >= 0x10) && (type <= 0x15)) || ((type >= 0x20) && (type <= 0x25)))) {
                event.m_subtitles = true;
            } else if ((content == 0x09) && (contentExt == 0x02)) {
                event.m_subtitles = true;
            } else if (((content == 0x02) || (content == 0x04) || (content == 0x06)) && ((type & 0xF0) == 0x40)) {
                event.m_audioDescription = true;
            } else if (((content == 0x01) && (type >= 0x09) && (type <= 0x10)) || ((content == 0x05) && (type >= 0x0B))
                || ((content == 0x09) && (contentExt == 0x00))) {
                event.m_hd = true;
            }
        }
        else if ((tag == 0x4A) && (dlength >= 7) && (d[6] == 0x0D)) // linkage_descriptor: event linkage
        {
            // transport_stream_id, original_network_id, service_id, linkage_type, then the target event. The UK
            // links SD programmes to the same in HD, which the linked transport stream may not say (another
            // region's), so the service is found by its network and id
            event.m_linkNetworkId = (d[2] << 8) | d[3];
            event.m_linkServiceId = (d[4] << 8) | d[5];
        }
        else if (tag == 0x76) // content_identifier_descriptor (TS 102 323 12.1): CRIDs
        {
            for (int j = 0; j + 1 <= dlength; )
            {
                const int type = d[j] >> 2;
                const int location = d[j] & 0x03;
                QString crid;

                if (location == 0)
                {
                    if (j + 2 > dlength) {
                        break;
                    }

                    const int cridLength = std::min((int) d[j + 1], dlength - j - 2);
                    crid = QString::fromLatin1(reinterpret_cast<const char *>(d + j + 2), cridLength).toLower();
                    j += 2 + cridLength;
                }
                else
                {
                    j += location == 1 ? 3 : 1; // A reference into a content identifier table: not read
                }

                // Types 1 and 2 (the D-Book's 0x31 and 0x32): the programme's and the series'
                if ((type == 0x01) || (type == 0x31)) {
                    event.m_programmeCrid = crid;
                } else if ((type == 0x02) || (type == 0x32)) {
                    event.m_seriesCrid = crid;
                }
            }
        }
        else if ((tag == 0x89) && (dlength >= 4)) // UK D-Book guidance_descriptor: type, language, text
        {
            const int type = d[0] & 0x03;
            const int offset = type == 0 ? 4 : (type == 1 ? 5 : dlength);

            if (offset < dlength) {
                event.m_guidance = DVBDemodText::decode(d + offset, dlength - offset);
            }
        }

        i = end;
    }

    event.m_text = shortText;

    for (const auto& parts : extended)
    {
        // A descriptor's text that starts by selecting a character table (or with Freesat compression) stands alone
        QString items, text;
        std::vector<uint8_t> raw;

        for (const auto& part : parts.second)
        {
            items += part.second.m_items;

            if (!part.second.m_text.empty() && (part.second.m_text[0] < 0x20) && !raw.empty())
            {
                text += DVBDemodText::decode(raw.data(), (int) raw.size());
                raw.clear();
            }

            raw.insert(raw.end(), part.second.m_text.begin(), part.second.m_text.end());
        }

        text = items + text + DVBDemodText::decode(raw.data(), (int) raw.size());

        if (!text.isEmpty() && (text != shortText)) {
            event.m_text += (event.m_text.isEmpty() ? "" : "\n") + text;
        }
    }

    return true;
}

// event_information_section (5.2.4): present/following (0x4E this transport stream, 0x4F others) or schedule (0x50 to
// 0x5F, 0x60 to 0x6F)
void DVBDemodEPG::eit(const uint8_t *s, int length, bool freesat)
{
    const int table = s[0];

    if ((table < 0x4E) || (table > 0x6F) || (length < 18) || !(s[5] & 1)) {
        return;
    }

    const int serviceId = (s[3] << 8) | s[4];
    const int version = (s[5] >> 1) & 0x1F;
    const int section = s[6];
    const int transportStreamId = (s[8] << 8) | s[9];
    const int networkId = (s[10] << 8) | s[11];
    ServiceData& data = service(networkId, transportStreamId, serviceId);

    if ((table == 0x4E) && !freesat && ((transportStreamId != m_transportStreamId) || (networkId != m_networkId)))
    {
        m_transportStreamId = transportStreamId;
        m_networkId = networkId;
        m_revision++;
    }

    // Freesat's own EIT apart from the standard one (each's versions, and its events' sections): they can differ, so
    // one's version doesn't say the other's section is unchanged, nor a segment of one replace the other's events
    // A section not received for a while is read again whatever its version (which may have wrapped since)
    const int sourceTable = eitSourceTable(table, freesat);
    const qint64 now = m_clock.elapsed();
    SeenVersion& seen = data.m_versions[eitVersionKey(sourceTable, section)];

    if ((seen.m_version == version) && (now - seen.m_seen < m_versionTimeout))
    {
        seen.m_seen = now;
        return;
    }

    seen.m_version = version;
    seen.m_seen = now;
    std::vector<Event> events;
    const int end = length - 4;

    for (int i = 14; i + 12 <= end; )
    {
        const int eventEnd = std::min(end, i + 12 + (((s[i + 10] & 0x0F) << 8) | s[i + 11]));
        Event event;

        if (parseEvent(s + i, eventEnd - i, event)) {
            events.push_back(event);
        }

        i = eventEnd;
    }

    if ((table == 0x4E) || (table == 0x4F))
    {
        // Section 0 is the event now, 1 the next
        if (section == 0) {
            data.m_present = events.empty() ? Event() : events[0];
        } else if (section == 1) {
            data.m_following = events.empty() ? Event() : events[0];
        }
    }
    else
    {
        // Replace what the section had before
        for (auto e = data.m_schedule.begin(); e != data.m_schedule.end(); )
        {
            if ((e->second.m_table == sourceTable) && (e->second.m_section == section)) {
                e = data.m_schedule.erase(e);
            } else {
                ++e;
            }
        }

        for (const Event& event : events) {
            insertEvent(data, event, sourceTable, section);
        }

        replaceSegment(data, sourceTable, section, s[12], version);
    }

    m_modified = true;
    changed(data);
}

// An event from a schedule section, replacing others it has replaced since they were received: the same event moved,
// or one it overlaps by at least half. Events can overlap by a few minutes as broadcast. One longer than a day (as a
// placeholder for days off air) replaces only an event with the same start: it would wipe out the days it spans
void DVBDemodEPG::insertEvent(ServiceData& data, const Event& event, int table, int section)
{
    auto it = data.m_schedule.lower_bound(event.m_start - m_longEvent);

    while ((event.m_duration <= m_longEvent) && (it != data.m_schedule.end()) && (it->first < event.end()))
    {
        const Event& other = it->second.m_event;
        const qint64 overlap = std::min(other.end(), event.end()) - std::max(other.m_start, event.m_start);

        if ((it->first != event.m_start) && (overlap > 0) && ((other.m_eventId == event.m_eventId) || (2 * overlap >= other.m_duration))) {
            it = data.m_schedule.erase(it);
        } else {
            ++it;
        }
    }

    StoredEvent& stored = data.m_schedule[event.m_start];
    stored.m_event = event;
    stored.m_table = table;
    stored.m_section = section;
}

// Once every section of a segment has been received, it has every event starting in its 3 hours, so others there
// (from the cache, or sections since renumbered, as segments are counted from midnight UTC) are removed. Each
// table has 4 days, in 32 segments of 8 sections, from midnight of the TDT's day (EN 300 468 5.2.4, TR 101 211).
// Not near midnight, when the broadcaster and the TDT may disagree on the day, or if an event is out of the window,
// nor events that have ended, which may be dropped from the current segment
void DVBDemodEPG::replaceSegment(ServiceData& data, int table, int section, int lastSection, int version)
{
    const qint64 now = timeLocked();
    const qint64 secondOfDay = now % 86400;

    if ((now <= 0) || (secondOfDay < 600) || (secondOfDay > 86400 - 600) || (lastSection < section)
        || (lastSection / 8 != section / 8)) {
        return;
    }

    const int first = section & ~7;

    for (int s = first; s <= lastSection; s++)
    {
        auto it = data.m_versions.find(eitVersionKey(table, s));

        if ((it == data.m_versions.end()) || (it->second.m_version != version)) {
            return; // Not complete
        }
    }

    const qint64 start = (now - secondOfDay) + (table & 0x0F) * 4 * 86400 + (section / 8) * 3 * 3600;
    const qint64 end = start + 3 * 3600;
    auto inSegment = [table, first, lastSection](const StoredEvent& e) {
        return (e.m_table == table) && (e.m_section >= first) && (e.m_section <= lastSection);
    };

    for (const auto& entry : data.m_schedule)
    {
        if (inSegment(entry.second) && ((entry.first < start) || (entry.first >= end))) {
            return; // Not the segment it's thought to be
        }
    }

    // The other source's events aren't this one's to remove (Freesat's own EIT or the standard one): it may well have
    // them still, and as its sections are unchanged, they wouldn't be put back. Those from the cache are anyone's
    auto otherSource = [table](const StoredEvent& e) {
        return (e.m_table >= 0) && ((e.m_table & m_freesatSource) != (table & m_freesatSource));
    };

    for (auto it = data.m_schedule.lower_bound(start); (it != data.m_schedule.end()) && (it->first < end); )
    {
        if (!inSegment(it->second) && !otherSource(it->second) && (it->second.m_event.end() > now)) {
            it = data.m_schedule.erase(it);
        } else {
            ++it;
        }
    }
}

// time_date_section (0x70, without a CRC) and time_offset_section (0x73, with one): UTC. As the TDT has no CRC, a time
// far from that expected (extrapolated from the last, else the computer's clock) may be corrupt: it's used only once
// the next agrees with it. A TOT is taken as is, as its CRC is correct (a broadcaster's clock that's wrong would be in
// the TDT too). Events are expired by the time used only
void DVBDemodEPG::time(const uint8_t *s, int length)
{
    if (((s[0] != 0x70) && (s[0] != 0x73)) || (length < 8)) {
        return;
    }

    const qint64 utc = dvbTime(s + 3);  // -1 unless valid BCD, from 2000 to 2100

    if (utc <= 0) {
        return;
    }

    m_timeReceived.start();
    const qint64 expected = m_time > 0 ? timeLocked() : QDateTime::currentSecsSinceEpoch();
    const bool confirmed = (m_pendingTime > 0) && (std::abs(utc - (m_pendingTime + m_pendingSince.elapsed() / 1000)) <= m_timeJump);

    if ((std::abs(utc - expected) > m_timeJump) && !confirmed && (s[0] != 0x73))
    {
        m_pendingTime = utc;
        m_pendingSince.start();
        return;
    }

    m_pendingTime = 0;
    m_time = utc;
    m_timeSince.start(); // Not restart(): may never have been started
    expire(utc);
}

void DVBDemodEPG::expire(qint64 now)
{
    // Not too often, but again straight away if the time's gone back (as when a recording's played)
    if ((now - m_lastExpiry < m_expiryInterval) && (now >= m_lastExpiry)) {
        return;
    }

    m_lastExpiry = now;
    const qint64 before = now - m_keepEnded;

    for (auto& entry : m_services)
    {
        auto& schedule = entry.second.m_schedule;
        bool erased = false;

        // Those starting before can have ended: not just the first, which may be a long one that hasn't
        for (auto it = schedule.begin(); (it != schedule.end()) && (it->first < before); )
        {
            if (it->second.m_event.end() < before)
            {
                it = schedule.erase(it);
                erased = true;
            }
            else
            {
                ++it;
            }
        }

        if (erased) {
            changed(entry.second);
        }
    }
}

void DVBDemodEPG::periodicExpiry()
{
    if (m_timeReceived.isValid() && (m_timeReceived.elapsed() < m_timeAbsent * 1000)) {
        return; // time() does
    }

    expire(m_time > 0 ? timeLocked() : QDateTime::currentSecsSinceEpoch());
}

qint64 DVBDemodEPG::timeLocked() const
{
    return m_time > 0 ? m_time + m_timeSince.elapsed() / 1000 : 0;
}

qint64 DVBDemodEPG::getTime() const
{
    QMutexLocker locker(&m_mutex);
    return timeLocked();
}

int DVBDemodEPG::getTransportStreamId() const
{
    QMutexLocker locker(&m_mutex);
    return m_transportStreamId;
}

int DVBDemodEPG::getNetworkId() const
{
    QMutexLocker locker(&m_mutex);
    return m_networkId;
}

std::vector<DVBDemodEPG::Service> DVBDemodEPG::getServices() const
{
    QMutexLocker locker(&m_mutex);
    std::vector<Service> services;
    // Each group's (ids and name) chosen entry, as an index into services, and its rank. One without a name joins a
    // named group with its ids, if there is one
    std::map<std::tuple<int, int, QString>, std::pair<size_t, int>> groups;
    std::set<std::pair<int, int>> named;

    for (const auto& entry : m_services)
    {
        const Service& info = entry.second.m_info;

        if (!info.m_removed && !info.m_name.isEmpty()) {
            named.insert({info.m_networkId, info.m_serviceId});
        }
    }

    for (const auto& entry : m_services)
    {
        const Service& info = entry.second.m_info;

        if (info.m_removed) {
            continue;
        }

        // A named group's entry without a name ranks under any named one, so never replaces it
        const bool join = info.m_name.isEmpty() && named.count({info.m_networkId, info.m_serviceId});
        const Service service = describe(entry.second);
        const int r = service.m_actual ? 1 << 30 : rank(entry.second);
        auto group = groups.end();

        if (join)
        {
            for (auto g = groups.lower_bound({info.m_networkId, info.m_serviceId, QString()});
                (g != groups.end()) && (std::get<0>(g->first) == info.m_networkId) && (std::get<1>(g->first) == info.m_serviceId); ++g)
            {
                if (!std::get<2>(g->first).isEmpty())
                {
                    group = g;
                    break;
                }
            }

            if (group == groups.end()) {
                continue;   // Its named group's entry comes later: ranked under it anyway
            }
        }
        else
        {
            group = groups.find({info.m_networkId, info.m_serviceId, info.m_name});
        }

        if (group == groups.end())
        {
            groups[{info.m_networkId, info.m_serviceId, info.m_name}] = {services.size(), r};
            services.push_back(service);
        }
        else if (r > group->second.second)
        {
            services[group->second.first] = service;
            group->second.second = r;
        }
    }

    return services;
}

std::vector<DVBDemodEPG::TransportStream> DVBDemodEPG::getTransportStreams() const
{
    QMutexLocker locker(&m_mutex);
    std::vector<TransportStream> transportStreams;

    for (const auto& entry : m_transportStreams) {
        transportStreams.push_back(entry.second);
    }

    return transportStreams;
}

bool DVBDemodEPG::getTransportStream(int networkId, int transportStreamId, TransportStream& transportStream) const
{
    QMutexLocker locker(&m_mutex);
    auto it = m_transportStreams.find(key(networkId, transportStreamId));

    if ((it == m_transportStreams.end()) || (it->second.m_frequency <= 0)) {
        return false;
    }

    transportStream = it->second;
    return true;
}

void DVBDemodEPG::setFrequency(qint64 frequency, int bandwidth)
{
    QMutexLocker locker(&m_mutex);

    if ((m_networkId < 0) || (m_transportStreamId < 0) || (frequency <= 0)) {
        return;
    }

    TransportStream& ts = m_transportStreams[key(m_networkId, m_transportStreamId)];

    // From the NIT, or learned before at much the same frequency (a channel's offset may differ)
    if ((ts.m_frequency > 0) && (!ts.m_learned || (std::abs(frequency - ts.m_frequency) <= 1000000))) {
        return;
    }

    ts.m_networkId = m_networkId;
    ts.m_transportStreamId = m_transportStreamId;
    ts.m_frequency = frequency;
    ts.m_bandwidth = bandwidth;
    ts.m_learned = true;
    m_modified = true;
    m_revision++;
}

void DVBDemodEPG::retuned()
{
    QMutexLocker locker(&m_mutex);
    m_networkId = -1;
    m_transportStreamId = -1;
    m_revision++;
}

bool DVBDemodEPG::getService(int networkId, int serviceId, Service& service) const
{
    QMutexLocker locker(&m_mutex);
    const ServiceData *data = lookup(networkId, serviceId);

    if (!data) {
        return false;
    }

    service = describe(*data);
    return true;
}

bool DVBDemodEPG::getEvent(int networkId, int serviceId, int eventId, Event& event) const
{
    QMutexLocker locker(&m_mutex);
    const ServiceData *found = lookup(networkId, serviceId);

    if (!found) {
        return false;
    }

    const ServiceData& data = *found;

    for (const Event *e : {&data.m_present, &data.m_following})
    {
        if (e->isValid() && (e->m_eventId == eventId))
        {
            event = *e;
            return true;
        }
    }

    for (const auto& entry : data.m_schedule)
    {
        if (entry.second.m_event.m_eventId == eventId)
        {
            event = entry.second.m_event;
            return true;
        }
    }

    return false;
}

int DVBDemodEPG::getRunningStatus(int networkId, int serviceId, int eventId) const
{
    QMutexLocker locker(&m_mutex);
    const ServiceData *data = lookup(networkId, serviceId);

    // Only the transport stream received's present and following table is kept up to date
    if (!data || data->m_info.m_removed || (data->m_info.m_networkId != m_networkId)
        || (data->m_info.m_transportStreamId != m_transportStreamId)) {
        return -1;
    }

    for (const Event *e : {&data->m_present, &data->m_following})
    {
        if (e->isValid() && (e->m_eventId == eventId)) {
            return e->m_runningStatus;
        }
    }

    return -1;
}

bool DVBDemodEPG::getSimulcast(int networkId, int serviceId, Service& simulcast) const
{
    QMutexLocker locker(&m_mutex);
    const ServiceData *found = lookup(networkId, serviceId);

    if (!found) {
        return false;
    }

    // The link of the programme on now: the present event, if it's on (or the time isn't known), else the schedule's
    // event at the time. The schedule's copy of the present event may have the link if it doesn't
    const ServiceData& data = *found;
    const qint64 t = timeLocked();
    const bool presentOn = data.m_present.isValid() && ((t == 0) || ((data.m_present.m_start <= t) && (t < data.m_present.end())));
    int linkNetworkId = -1, linkServiceId = -1;

    if (presentOn && (data.m_present.m_linkServiceId >= 0))
    {
        linkNetworkId = data.m_present.m_linkNetworkId;
        linkServiceId = data.m_present.m_linkServiceId;
    }
    else if (t > 0)
    {
        auto e = data.m_schedule.upper_bound(t);

        if (e != data.m_schedule.begin())
        {
            const Event& event = std::prev(e)->second.m_event;

            if ((event.m_linkServiceId >= 0) && (t < event.end()) && (!presentOn || (event.m_eventId == data.m_present.m_eventId)))
            {
                linkNetworkId = event.m_linkNetworkId;
                linkServiceId = event.m_linkServiceId;
            }
        }
    }

    const ServiceData *linked = linkServiceId >= 0 ? lookup(linkNetworkId, linkServiceId) : nullptr;

    if (!linked || linked->m_info.m_name.isEmpty() || linked->m_info.m_removed) {
        return false;
    }

    simulcast = describe(*linked);
    return simulcast.m_actual || simulcast.m_multiplexKnown;
}

bool DVBDemodEPG::getNowNext(int serviceId, Event& now, Event& next) const
{
    QMutexLocker locker(&m_mutex);
    return nowNextLocked(findService(serviceId), now, next);
}

bool DVBDemodEPG::getNowNext(int networkId, int serviceId, Event& now, Event& next) const
{
    QMutexLocker locker(&m_mutex);
    return nowNextLocked(lookup(networkId, serviceId), now, next);
}

bool DVBDemodEPG::nowNextLocked(const ServiceData *data, Event& now, Event& next) const
{
    now = Event();
    next = Event();

    if (!data) {
        return false;
    }

    const qint64 t = timeLocked();

    // From the present/following table, if it's current, else the schedule
    if (data->m_present.isValid() && ((t == 0) || ((data->m_present.m_start <= t) && (t < data->m_present.end()))))
    {
        now = data->m_present;
        next = data->m_following;
        return true;
    }

    for (const auto& entry : data->m_schedule)
    {
        const Event& event = entry.second.m_event;

        if (now.isValid())
        {
            next = event;
            break;
        }

        if ((event.m_start <= t) && (t < event.end())) {
            now = event;
        } else if (event.m_start > t) {
            next = event;
            break;
        }
    }

    if (!now.isValid() && !next.isValid() && data->m_following.isValid() && (data->m_following.m_start > t)) {
        next = data->m_following;
    }

    return now.isValid() || next.isValid();
}

std::vector<DVBDemodEPG::Event> DVBDemodEPG::getEvents(int networkId, int serviceId) const
{
    QMutexLocker locker(&m_mutex);
    std::vector<Event> events;
    eventsLocked(lookup(networkId, serviceId), timeLocked(), events);
    return events;
}

std::vector<std::vector<DVBDemodEPG::Event>> DVBDemodEPG::getEvents(const std::vector<std::pair<int, int>>& services) const
{
    QMutexLocker locker(&m_mutex);
    std::vector<std::vector<Event>> events(services.size());
    const qint64 t = timeLocked();

    for (size_t i = 0; i < services.size(); i++) {
        eventsLocked(lookup(services[i].first, services[i].second), t, events[i]);
    }

    return events;
}

// Each service's choice by lookup() and its m_changed: as m_changed is from m_revision, it differs for each service
// and change, so says when another's chosen too
std::vector<quint64> DVBDemodEPG::getServiceRevisions(const std::vector<std::pair<int, int>>& services) const
{
    QMutexLocker locker(&m_mutex);
    std::vector<quint64> revisions(services.size(), 0);

    for (size_t i = 0; i < services.size(); i++)
    {
        const ServiceData *data = lookup(services[i].first, services[i].second);
        revisions[i] = data ? data->m_changed : 0;
    }

    return revisions;
}

void DVBDemodEPG::eventsLocked(const ServiceData *data, qint64 t, std::vector<Event>& events) const
{
    events.clear();

    if (!data) {
        return;
    }

    const qint64 from = t > 0 ? t - m_keepEnded : 0;
    const Event& present = data->m_present;
    const bool withPresent = present.isValid() && (present.end() >= from);
    events.reserve(data->m_schedule.size() + 1);

    for (const auto& entry : data->m_schedule)
    {
        const Event& event = entry.second.m_event;

        // The present event instead of the schedule's copy, which may have the time as planned rather than as broadcast
        if ((event.end() >= from) && !(withPresent && ((event.m_eventId == present.m_eventId) || (event.m_start == present.m_start)))) {
            events.push_back(event);
        }
    }

    if (withPresent)
    {
        events.push_back(present);
        std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.m_start < b.m_start; });
    }
}

int DVBDemodEPG::getEventCount() const
{
    QMutexLocker locker(&m_mutex);
    int count = 0;

    for (const auto& entry : m_services) {
        count += (int) entry.second.m_schedule.size();
    }

    return count;
}

bool DVBDemodEPG::isModified() const
{
    QMutexLocker locker(&m_mutex);
    return m_modified;
}

// Copied with the lock held (cheaply: the strings are shared), then serialised and written without it, so the
// demodulator's thread isn't held up
bool DVBDemodEPG::save(const QString& fileName)
{
    struct SavedService
    {
        Service m_info;
        std::vector<Event> m_events;
    };

    std::vector<TransportStream> transportStreams;
    std::vector<SavedService> services;

    {
        QMutexLocker locker(&m_mutex);
        transportStreams.reserve(m_transportStreams.size());

        for (const auto& entry : m_transportStreams) {
            transportStreams.push_back(entry.second);
        }

        // Not services removed from their transport stream (they'd load as if still broadcast)
        for (const auto& entry : m_services)
        {
            if (entry.second.m_info.m_removed) {
                continue;
            }

            services.emplace_back();
            SavedService& saved = services.back();
            saved.m_info = entry.second.m_info;
            saved.m_events.reserve(entry.second.m_schedule.size());

            for (const auto& stored : entry.second.m_schedule) {
                saved.m_events.push_back(stored.second.m_event);
            }
        }

        m_modified = false; // Changes from now on are saved next time
    }

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << m_cacheMagic << m_cacheVersion << (quint32) transportStreams.size();

    for (const TransportStream& ts : transportStreams)
    {
        out << (qint32) ts.m_networkId << (qint32) ts.m_transportStreamId << ts.m_frequency << (qint32) ts.m_bandwidth
            << ts.m_t2 << (qint32) ts.m_plpId << ts.m_lowPriority << ts.m_actual << ts.m_learned
            << ts.m_satellite << ts.m_s2 << (qint32) ts.m_symbolRate << (qint32) ts.m_polarisation
            << (qint32) ts.m_orbitalPosition << (qint32) ts.m_fecInner << (qint32) ts.m_rollOff << (qint32) ts.m_isi;
    }

    out << (quint32) services.size();

    for (const SavedService& saved : services)
    {
        const Service& s = saved.m_info;
        out << (qint32) s.m_networkId << (qint32) s.m_transportStreamId << (qint32) s.m_serviceId << s.m_name
            << s.m_provider << (qint32) s.m_type << (qint32) s.m_channel << s.m_visible << (quint32) saved.m_events.size();

        for (const Event& e : saved.m_events)
        {
            out << (qint32) e.m_eventId << e.m_start << (qint32) e.m_duration << e.m_title << e.m_text << (qint32) e.m_genre
                << (qint32) e.m_minimumAge << e.m_hd << e.m_subtitles << e.m_audioDescription << e.m_guidance
                << (qint32) e.m_linkNetworkId << (qint32) e.m_linkServiceId << e.m_programmeCrid << e.m_seriesCrid;
        }
    }

    QDir().mkpath(QFileInfo(fileName).absolutePath());
    QSaveFile file(fileName);

    if (file.open(QIODevice::WriteOnly) && (file.write(data) == data.size()) && file.commit()) {
        return true;
    }

    QMutexLocker locker(&m_mutex);
    m_modified = true;
    return false;
}

// Records are used only if read completely, and ids out of range are skipped. Read without the lock, then merged
// with what's been received, which takes precedence
bool DVBDemodEPG::load(const QString& fileName, qint64 now)
{
    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QDataStream in(&file);
    quint32 magic, version;
    in >> magic >> version;

    if ((magic != m_cacheMagic) || (version < 1) || (version > m_cacheVersion) || (in.status() != QDataStream::Ok)) {
        return false;
    }

    auto validId = [](qint32 id) { return (id >= 0) && (id <= 0xFFFF); };
    std::vector<TransportStream> transportStreams;

    if (version >= 2)
    {
        quint32 nbTransportStreams;
        in >> nbTransportStreams;

        for (quint32 i = 0; (i < nbTransportStreams) && (in.status() == QDataStream::Ok); i++)
        {
            TransportStream ts;
            qint32 networkId, transportStreamId, bandwidth, plpId;
            in >> networkId >> transportStreamId >> ts.m_frequency >> bandwidth >> ts.m_t2 >> plpId;

            if (version >= 4) {
                in >> ts.m_lowPriority >> ts.m_actual >> ts.m_learned;
            }

            if (version >= 5)
            {
                qint32 symbolRate, polarisation, position, fec, rollOff;
                in >> ts.m_satellite >> ts.m_s2 >> symbolRate >> polarisation >> position >> fec >> rollOff;
                ts.m_symbolRate = symbolRate;
                ts.m_polarisation = polarisation;
                ts.m_orbitalPosition = position;
                ts.m_fecInner = fec;
                ts.m_rollOff = rollOff;
            }

            if (version >= 6)
            {
                qint32 isi;
                in >> isi;
                ts.m_isi = (isi >= 0) && (isi <= 255) ? isi : -1;
            }

            if ((in.status() != QDataStream::Ok) || !validId(networkId) || !validId(transportStreamId)) {
                continue;
            }

            ts.m_networkId = networkId;
            ts.m_transportStreamId = transportStreamId;
            ts.m_bandwidth = bandwidth;
            ts.m_plpId = plpId;
            transportStreams.push_back(ts);
        }
    }

    struct LoadedService
    {
        qint32 m_networkId = 0;
        qint32 m_transportStreamId = 0;
        qint32 m_serviceId = 0;
        qint32 m_type = 0;
        qint32 m_channel = -1;
        QString m_name;
        QString m_provider;
        bool m_visible = true;
        std::vector<Event> m_events;
    };

    std::vector<LoadedService> services;
    quint32 nbServices = 0;
    in >> nbServices;
    now = now > 0 ? now : QDateTime::currentSecsSinceEpoch();

    for (quint32 i = 0; (i < nbServices) && (in.status() == QDataStream::Ok); i++)
    {
        LoadedService loaded;
        quint32 nbEvents;
        in >> loaded.m_networkId >> loaded.m_transportStreamId >> loaded.m_serviceId >> loaded.m_name >> loaded.m_provider
            >> loaded.m_type >> loaded.m_channel;

        if (version >= 4) {
            in >> loaded.m_visible;
        }

        in >> nbEvents;

        if (in.status() != QDataStream::Ok) {
            break;
        }

        // The events are read even if the service is skipped, to get to the next. One whose transport stream wasn't
        // known isn't kept: a service is its transport stream's (a service id can be in more than one), and comes
        // again with it
        const bool valid = validId(loaded.m_networkId) && validId(loaded.m_serviceId) && validId(loaded.m_transportStreamId);

        for (quint32 j = 0; (j < nbEvents) && (in.status() == QDataStream::Ok); j++)
        {
            Event e;
            qint32 eventId, duration, genre, minimumAge;
            in >> eventId >> e.m_start >> duration >> e.m_title >> e.m_text >> genre >> minimumAge >> e.m_hd
                >> e.m_subtitles >> e.m_audioDescription >> e.m_guidance;

            if (version >= 3)
            {
                qint32 linkNetworkId, linkServiceId;
                in >> linkNetworkId >> linkServiceId;
                e.m_linkNetworkId = linkNetworkId;
                e.m_linkServiceId = linkServiceId;
            }

            if (version >= 7) {
                in >> e.m_programmeCrid >> e.m_seriesCrid;
            }

            e.m_eventId = eventId;
            e.m_duration = duration;
            e.m_genre = genre;
            e.m_minimumAge = minimumAge;

            if (valid && (in.status() == QDataStream::Ok) && (e.end() >= now - m_keepEnded)) {
                loaded.m_events.push_back(e);
            }
        }

        if (valid) {
            services.push_back(std::move(loaded));
        }
    }

    QMutexLocker locker(&m_mutex);

    // What's been received takes precedence
    for (const TransportStream& ts : transportStreams)
    {
        if (m_transportStreams.find(key(ts.m_networkId, ts.m_transportStreamId)) == m_transportStreams.end()) {
            m_transportStreams[key(ts.m_networkId, ts.m_transportStreamId)] = ts;
        }
    }

    for (const LoadedService& loaded : services)
    {
        ServiceData& data = service(loaded.m_networkId, loaded.m_transportStreamId, loaded.m_serviceId);

        if (data.m_info.m_name.isEmpty())
        {
            data.m_info.m_name = loaded.m_name;
            data.m_info.m_provider = loaded.m_provider;
            data.m_info.m_type = loaded.m_type;
        }

        if (data.m_info.m_channel < 0)
        {
            data.m_info.m_channel = loaded.m_channel;
            data.m_info.m_visible = loaded.m_visible;
        }

        for (const Event& e : loaded.m_events)
        {
            if (data.m_schedule.find(e.m_start) == data.m_schedule.end())
            {
                StoredEvent& stored = data.m_schedule[e.m_start];
                stored.m_event = e;
                stored.m_table = -1; // Replaced by events received that replace it, or the segment for its time
            }
        }

        changed(data);
    }

    m_modified = false;
    m_revision++;
    return in.status() == QDataStream::Ok;
}

QString DVBDemodEPG::defaultCacheFileName()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/dvbdemod_epg.dat";
}

// Content level 1: EN 300 468 table 29, or the UK D-Book's
QString DVBDemodEPG::genreText(int genre, int networkId)
{
    static const char *const names[16] = {
        "", "Film/Drama", "News/Current affairs", "Entertainment", "Sport", "Children's", "Music", "Arts/Culture",
        "Social/Political", "Education/Factual", "Leisure/Hobbies", "", "", "", "", ""
    };
    static const char *const ukNames[16] = {
        "", "Film", "News and Factual", "Entertainment", "Sport", "Children's", "Education", "Lifestyle",
        "Drama", "", "", "", "", "", "", ""
    };

    if (genre < 0) {
        return QString();
    }

    return QString((networkId == 0x233A ? ukNames : names)[(genre >> 4) & 0x0F]);
}
