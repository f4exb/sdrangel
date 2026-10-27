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

#include "dabprofile.h"
#include "dabensemble.h"

namespace modemdab
{

namespace
{

inline uint32_t readSId(const uint8_t *p, bool longForm)
{
    if (longForm) {
        return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3];
    }
    return (static_cast<uint32_t>(p[0]) << 8) | p[1];
}

// Split a label into characters (byte ranges) for its character set, to apply character flags.
std::vector<std::pair<int, int>> labelCharacters(const uint8_t *data, int length, int charset)
{
    std::vector<std::pair<int, int>> chars;
    int i = 0;

    while (i < length)
    {
        int size = 1;
        if (charset == 6)
        {
            size = 2;
        }
        else if (charset == 15)
        {
            const uint8_t c = data[i];
            if ((c & 0xe0) == 0xc0) {
                size = 2;
            } else if ((c & 0xf0) == 0xe0) {
                size = 3;
            } else if ((c & 0xf8) == 0xf0) {
                size = 4;
            }
        }
        if (i + size > length) {
            break;
        }
        chars.emplace_back(i, size);
        i += size;
    }
    return chars;
}

std::string shortLabel(const uint8_t *data, int length, int charset, uint16_t flags)
{
    std::vector<std::pair<int, int>> chars = labelCharacters(data, length, charset);
    std::vector<uint8_t> selected;

    for (std::size_t i = 0; (i < chars.size()) && (i < 16); i++)
    {
        if (flags & (0x8000 >> i)) {
            selected.insert(selected.end(), data + chars[i].first, data + chars[i].first + chars[i].second);
        }
    }
    return DABTables::labelToUTF8(selected.data(), selected.size(), charset);
}

} // anonymous namespace

DABFICDecoder::DABFICDecoder() :
    m_profile(DABTables::ficProfile())
{
}

int DABFICDecoder::decodeBlock(
    const float *soft,
    std::array<std::array<uint8_t, ModeI::FIBBytes>, ModeI::FIBsPerCIF>& fibs,
    std::array<bool, ModeI::FIBsPerCIF>& valid,
    int& bitErrors,
    int& codedBits
)
{
    DABProfileScope profile(DABProfile::FIC);
    bitErrors = 0;
    codedBits = 0;
    m_viterbi.decodePunctured(soft, ModeI::FICBlockBits, m_profile, m_bits, &bitErrors, &codedBits);

    const std::vector<uint8_t>& prbs = DABTables::prbs();
    int good = 0;

    for (int f = 0; f < ModeI::FIBsPerCIF; f++)
    {
        for (int i = 0; i < ModeI::FIBBytes; i++)
        {
            uint8_t byte = 0;
            for (int b = 0; b < 8; b++)
            {
                const int index = (f * ModeI::FIBBytes + i) * 8 + b;
                byte = static_cast<uint8_t>((byte << 1) | ((m_bits[index] ^ prbs[index]) & 1));
            }
            fibs[f][i] = byte;
        }
        valid[f] = DABTables::crc16Check(fibs[f].data(), ModeI::FIBBytes);
        if (valid[f]) {
            good++;
        }
    }
    return good;
}

ProtectionProfile DABSubchannel::profile() const
{
    if (uep) {
        return DABTables::uepProfile(tableIndex);
    }
    return DABTables::eepProfile(eepSetB, protectionLevel - 1, sizeCU);
}

std::string DABSubchannel::protection() const
{
    if (uep) {
        return "UEP " + std::to_string(protectionLevel);
    }
    return "EEP " + std::to_string(protectionLevel) + (eepSetB ? "-B" : "-A");
}

bool DABSubchannel::operator==(const DABSubchannel& other) const
{
    return (id == other.id) && (startCU == other.startCU) && (sizeCU == other.sizeCU)
        && (uep == other.uep) && (tableIndex == other.tableIndex) && (eepSetB == other.eepSetB)
        && (protectionLevel == other.protectionLevel) && (bitrate == other.bitrate)
        && (fecScheme == other.fecScheme);
}

const DABServiceComponent *DABService::primaryComponent() const
{
    for (const auto& component : components)
    {
        if (component.primary) {
            return &component;
        }
    }
    return components.empty() ? nullptr : &components[0];
}

void DABDateTime::date(int& year, int& month, int& day) const
{
    // ETSI EN 300 468 annex C
    const int yp = static_cast<int>((mjd - 15078.2) / 365.25);
    const int mp = static_cast<int>((mjd - 14956.1 - static_cast<int>(yp * 365.25)) / 30.6001);
    day = mjd - 14956 - static_cast<int>(yp * 365.25) - static_cast<int>(mp * 30.6001);
    const int k = ((mp == 14) || (mp == 15)) ? 1 : 0;
    year = 1900 + yp + k;
    month = mp - 1 - k * 12;
}

DABEnsemble::DABEnsemble() :
    m_cifs(0),
    m_version(0),
    m_lastCIFCount(-1),
    m_cifCountUpdated(false)
{
}

void DABEnsemble::reset()
{
    m_current = MCI();
    m_next = MCI();
    m_info = DABEnsembleInfo();
    m_ensembleLabel = Label();
    m_serviceLabels.clear();
    m_componentLabels.clear();
    m_programmeTypes.clear();
    m_subchannelLanguages.clear();
    m_scidLanguages.clear();
    m_serviceECCs.clear();
    m_extendedLabels.clear();
    m_announcementSupport.clear();
    m_announcements.clear();
    m_announcementLastCIF.clear();
    m_linkageSets.clear();
    m_frequencies.clear();
    m_oeServices.clear();
    m_lastCIFCount = -1;
    m_cifCountUpdated = false;
    changed();
}

void DABEnsemble::processFIB(const uint8_t *fib)
{
    int pos = 0;

    while (pos < ModeI::FIBDataBytes)
    {
        const uint8_t header = fib[pos];
        if (header == 0xff) {
            break;  // End marker
        }
        const int type = header >> 5;
        const int length = header & 0x1f;
        pos++;
        if ((length == 0) || (pos + length > ModeI::FIBDataBytes)) {
            break;
        }

        switch (type)
        {
        case 0:
            fig0(&fib[pos], length);
            break;
        case 1:
            fig1(&fib[pos], length);
            break;
        case 2:
            fig2(&fib[pos], length);
            break;
        default:
            break;
        }
        pos += length;
    }
}

void DABEnsemble::cif(int cifCount)
{
    // An announcement is signalled at least once a second while it lasts, and its end ten
    // times a second for two seconds: one not heard of for a while has ended unseen
    m_cifs++;
    for (auto it = m_announcementLastCIF.begin(); it != m_announcementLastCIF.end();)
    {
        if (m_cifs - it->second > AnnouncementTimeoutCIFs)
        {
            m_announcements.erase(it->first);
            it = m_announcementLastCIF.erase(it);
            changed();
        }
        else
        {
            ++it;
        }
    }

    // Switch to the next configuration at the CIF given by the occurrence change
    if ((m_info.changeFlags != 0) && (m_info.occurrenceChange >= 0) && !m_next.empty())
    {
        if ((cifCount % 250) == m_info.occurrenceChange)
        {
            // Information not repeated for the next configuration is carried over
            MCI next = m_next;
            if (next.subchannels.empty()) {
                next.subchannels = m_current.subchannels;
            }
            if (next.services.empty()) {
                next.services = m_current.services;
            }
            for (const auto& p : m_current.packetComponents) {
                next.packetComponents.insert(p);
            }
            for (const auto& g : m_current.globalDefinitions) {
                next.globalDefinitions.insert(g);
            }
            for (const auto& u : m_current.userApplications) {
                next.userApplications.insert(u);
            }
            for (const auto& f : m_current.fecSchemes) {
                next.fecSchemes.insert(f);
            }
            m_current = next;
            m_next = MCI();
            m_info.changeFlags = 0;
            m_info.occurrenceChange = -1;
            changed();
        }
    }
}

void DABEnsemble::fig0(const uint8_t *data, int length)
{
    const bool next = (data[0] & 0x80) != 0;
    const bool oe = (data[0] & 0x40) != 0;
    const bool pd = (data[0] & 0x20) != 0;
    const int extension = data[0] & 0x1f;
    const uint8_t *field = data + 1;
    const int fieldLength = length - 1;

    if (fieldLength <= 0) {
        return;
    }

    switch (extension)
    {
    case 0:
        fig0_0(field, fieldLength);
        break;
    case 1:
        fig0_1(field, fieldLength, next);
        break;
    case 2:
        fig0_2(field, fieldLength, next, pd);
        break;
    case 3:
        fig0_3(field, fieldLength, next);
        break;
    case 5:
        fig0_5(field, fieldLength);
        break;
    case 7:
        fig0_7(field, fieldLength);
        break;
    case 8:
        fig0_8(field, fieldLength, next, pd);
        break;
    case 9:
        fig0_9(field, fieldLength);
        break;
    case 10:
        fig0_10(field, fieldLength);
        break;
    case 13:
        fig0_13(field, fieldLength, next, pd);
        break;
    case 14:
        fig0_14(field, fieldLength, next);
        break;
    case 17:
        if (!oe) {
            fig0_17(field, fieldLength);
        }
        break;
    case 6:
        fig0_6(field, fieldLength, next, pd);
        break;
    case 18:
        fig0_18(field, fieldLength);
        break;
    case 19:
        fig0_19(field, fieldLength);
        break;
    case 21:
        fig0_21(field, fieldLength, next, oe);
        break;
    case 24:
        fig0_24(field, fieldLength, next, oe, pd);
        break;
    default:
        break;
    }
}

// Ensemble information
void DABEnsemble::fig0_0(const uint8_t *data, int length)
{
    if (length < 4) {
        return;
    }
    const int eid = (data[0] << 8) | data[1];
    const int changeFlags = data[2] >> 6;
    const bool alarm = (data[2] & 0x20) != 0;
    const int cifCount = ((data[2] & 0x1f) * 250) + data[3];

    if (eid != m_info.eid)
    {
        // Another ensemble (a retune): nothing from the previous one applies
        if (m_info.eid >= 0) {
            reset();
        }
        m_info.eid = eid;
        changed();
    }
    m_info.alarm = alarm;
    m_info.changeFlags = changeFlags;
    if ((changeFlags != 0) && (length >= 5)) {
        m_info.occurrenceChange = data[4];
    } else if (changeFlags == 0) {
        m_info.occurrenceChange = -1;
    }
    if ((data[2] & 0x1f) < 20 && data[3] < 250)
    {
        m_lastCIFCount = cifCount;
        m_cifCountUpdated = true;
    }
}

// Sub-channel organization
void DABEnsemble::fig0_1(const uint8_t *data, int length, bool next)
{
    MCI& m = mci(next);
    int pos = 0;

    while (pos + 3 <= length)
    {
        DABSubchannel sc;
        sc.id = data[pos] >> 2;
        sc.startCU = ((data[pos] & 0x03) << 8) | data[pos + 1];
        const bool longForm = (data[pos + 2] & 0x80) != 0;

        if (!longForm)
        {
            const bool tableSwitch = (data[pos + 2] & 0x40) != 0;
            sc.uep = true;
            sc.tableIndex = data[pos + 2] & 0x3f;
            pos += 3;
            UEPTableEntry entry;
            if (tableSwitch || !DABTables::uepTableEntry(sc.tableIndex, entry)) {
                continue;
            }
            sc.sizeCU = entry.sizeCU;
            sc.protectionLevel = entry.protectionLevel;
            sc.bitrate = entry.bitrate;
        }
        else
        {
            if (pos + 4 > length) {
                break;
            }
            const int option = (data[pos + 2] >> 4) & 0x07;
            const int level = (data[pos + 2] >> 2) & 0x03;
            sc.uep = false;
            sc.sizeCU = ((data[pos + 2] & 0x03) << 8) | data[pos + 3];
            pos += 4;
            if (option > 1) {
                continue;
            }
            sc.eepSetB = option == 1;
            sc.protectionLevel = level + 1;
            sc.bitrate = DABTables::eepBitrate(sc.eepSetB, level, sc.sizeCU);
            if (sc.bitrate <= 0) {
                continue;
            }
        }
        if (sc.startCU + sc.sizeCU > ModeI::CUsPerCIF) {
            continue;
        }

        auto fec = m.fecSchemes.find(sc.id);
        if (fec != m.fecSchemes.end()) {
            sc.fecScheme = fec->second;
        }

        auto it = m.subchannels.find(sc.id);
        if ((it == m.subchannels.end()) || !(it->second == sc))
        {
            m.subchannels[sc.id] = sc;
            if (!next) {
                changed();
            }
        }
    }
}

// Basic service and service component definition
void DABEnsemble::fig0_2(const uint8_t *data, int length, bool next, bool pd)
{
    MCI& m = mci(next);
    const int sidLength = pd ? 4 : 2;
    int pos = 0;

    while (pos + sidLength + 1 <= length)
    {
        const uint32_t sid = readSId(&data[pos], pd);
        pos += sidLength;
        const int caId = (data[pos] >> 4) & 0x07;
        const int count = data[pos] & 0x0f;
        pos++;

        if (pos + 2 * count > length) {
            break;
        }

        ServiceDescription service;
        service.dataService = pd;
        service.caId = caId;

        for (int i = 0; i < count; i++, pos += 2)
        {
            ComponentDescription c;
            c.transport = data[pos] >> 6;
            c.primary = (data[pos + 1] & 0x02) != 0;
            c.ca = (data[pos + 1] & 0x01) != 0;
            c.subchannelId = -1;
            c.scid = -1;
            c.type = -1;
            if ((c.transport == DABServiceComponent::StreamAudio) || (c.transport == DABServiceComponent::StreamData))
            {
                c.type = data[pos] & 0x3f;
                c.subchannelId = data[pos + 1] >> 2;
            }
            else if (c.transport == DABServiceComponent::PacketData)
            {
                c.scid = ((data[pos] & 0x3f) << 6) | (data[pos + 1] >> 2);
            }
            else
            {
                continue;
            }
            service.components.push_back(c);
        }

        auto it = m.services.find(sid);
        bool same = (it != m.services.end()) && (it->second.components.size() == service.components.size())
            && (it->second.caId == service.caId);
        if (same)
        {
            for (std::size_t i = 0; i < service.components.size(); i++)
            {
                const ComponentDescription& a = it->second.components[i];
                const ComponentDescription& b = service.components[i];
                if ((a.transport != b.transport) || (a.type != b.type) || (a.subchannelId != b.subchannelId)
                    || (a.scid != b.scid) || (a.primary != b.primary) || (a.ca != b.ca))
                {
                    same = false;
                    break;
                }
            }
        }
        if (!same)
        {
            m.services[sid] = service;
            if (!next) {
                changed();
            }
        }
    }
}

// Service component in packet mode
void DABEnsemble::fig0_3(const uint8_t *data, int length, bool next)
{
    MCI& m = mci(next);
    int pos = 0;

    while (pos + 5 <= length)
    {
        const int scid = (data[pos] << 4) | (data[pos + 1] >> 4);
        const bool caOrg = (data[pos + 1] & 0x01) != 0;
        PacketComponent pc;
        pc.dataGroups = (data[pos + 2] & 0x80) == 0;
        pc.dscty = data[pos + 2] & 0x3f;
        pc.subchannelId = data[pos + 3] >> 2;
        pc.packetAddress = ((data[pos + 3] & 0x03) << 8) | data[pos + 4];
        pos += 5 + (caOrg ? 2 : 0);

        auto it = m.packetComponents.find(scid);
        if ((it == m.packetComponents.end()) || (it->second.subchannelId != pc.subchannelId)
            || (it->second.packetAddress != pc.packetAddress) || (it->second.dscty != pc.dscty)
            || (it->second.dataGroups != pc.dataGroups))
        {
            m.packetComponents[scid] = pc;
            if (!next) {
                changed();
            }
        }
    }
}

// Service component language
void DABEnsemble::fig0_5(const uint8_t *data, int length)
{
    int pos = 0;

    while (pos + 2 <= length)
    {
        const bool longForm = (data[pos] & 0x80) != 0;
        if (!longForm)
        {
            const int subchannel = data[pos] & 0x3f;
            const int language = data[pos + 1];
            if (m_subchannelLanguages[subchannel] != language)
            {
                m_subchannelLanguages[subchannel] = language;
                changed();
            }
            pos += 2;
        }
        else
        {
            if (pos + 3 > length) {
                break;
            }
            const int scid = ((data[pos] & 0x0f) << 8) | data[pos + 1];
            const int language = data[pos + 2];
            if (m_scidLanguages[scid] != language)
            {
                m_scidLanguages[scid] = language;
                changed();
            }
            pos += 3;
        }
    }
}

// Configuration information
void DABEnsemble::fig0_7(const uint8_t *data, int length)
{
    if (length < 2) {
        return;
    }
    m_info.servicesInConfiguration = data[0] >> 2;
    m_info.reconfigurationCount = ((data[0] & 0x03) << 8) | data[1];
}

// Service component global definition
void DABEnsemble::fig0_8(const uint8_t *data, int length, bool next, bool pd)
{
    MCI& m = mci(next);
    const int sidLength = pd ? 4 : 2;
    int pos = 0;

    while (pos + sidLength + 2 <= length)
    {
        const uint32_t sid = readSId(&data[pos], pd);
        pos += sidLength;
        const bool extension = (data[pos] & 0x80) != 0;
        const int scids = data[pos] & 0x0f;
        pos++;

        GlobalDefinition gd;
        gd.longForm = (data[pos] & 0x80) != 0;
        gd.subchannelId = -1;
        gd.scid = -1;
        if (!gd.longForm)
        {
            gd.subchannelId = data[pos] & 0x3f;
            pos++;
        }
        else
        {
            if (pos + 2 > length) {
                break;
            }
            gd.scid = ((data[pos] & 0x0f) << 8) | data[pos + 1];
            pos += 2;
        }
        if (extension) {
            pos++;
        }

        const auto key = std::make_pair(sid, scids);
        auto it = m.globalDefinitions.find(key);
        if ((it == m.globalDefinitions.end()) || (it->second.subchannelId != gd.subchannelId) || (it->second.scid != gd.scid))
        {
            m.globalDefinitions[key] = gd;
            if (!next) {
                changed();
            }
        }
    }
}

// Country, LTO and international table
void DABEnsemble::fig0_9(const uint8_t *data, int length)
{
    if (length < 3) {
        return;
    }
    const bool extended = (data[0] & 0x80) != 0;
    const int lto = (data[0] & 0x1f) * ((data[0] & 0x20) ? -1 : 1);
    const int ecc = data[1];
    const int table = data[2];

    if (!m_info.haveLTO || (m_info.ltoHalfHours != lto) || (m_info.ecc != ecc) || (m_info.internationalTable != table))
    {
        m_info.haveLTO = true;
        m_info.ltoHalfHours = lto;
        m_info.ecc = ecc;
        m_info.internationalTable = table;
        changed();
    }

    if (extended)
    {
        int pos = 3;
        while (pos + 2 <= length)
        {
            const int count = data[pos] >> 6;
            const int serviceECC = data[pos + 1];
            pos += 2;
            for (int i = 0; (i < count) && (pos + 2 <= length); i++, pos += 2)
            {
                const uint32_t sid = readSId(&data[pos], false);
                m_serviceECCs[sid] = serviceECC;
            }
        }
    }
}

// Date and time
void DABEnsemble::fig0_10(const uint8_t *data, int length)
{
    if (length < 4) {
        return;
    }
    DABDateTime t;
    t.mjd = ((data[0] & 0x7f) << 10) | (data[1] << 2) | (data[2] >> 6);
    const bool utcLong = (data[2] & 0x08) != 0;
    t.hours = ((data[2] & 0x07) << 2) | (data[3] >> 6);
    t.minutes = data[3] & 0x3f;
    if (utcLong && (length >= 6))
    {
        t.seconds = data[4] >> 2;
        t.milliseconds = ((data[4] & 0x03) << 8) | data[5];
    }
    t.valid = (t.hours < 24) && (t.minutes < 60) && (t.seconds < 60);
    if (t.valid) {
        m_info.time = t;
    }
}

// User application information
void DABEnsemble::fig0_13(const uint8_t *data, int length, bool next, bool pd)
{
    MCI& m = mci(next);
    const int sidLength = pd ? 4 : 2;
    int pos = 0;

    while (pos + sidLength + 1 <= length)
    {
        const uint32_t sid = readSId(&data[pos], pd);
        pos += sidLength;
        const int scids = data[pos] >> 4;
        const int count = data[pos] & 0x0f;
        pos++;

        std::vector<DABUserApplication> applications;
        for (int i = 0; (i < count) && (pos + 2 <= length); i++)
        {
            DABUserApplication ua;
            ua.type = (data[pos] << 3) | (data[pos + 1] >> 5);
            const int dataLength = data[pos + 1] & 0x1f;
            pos += 2;
            if (pos + dataLength > length) {
                break;
            }
            ua.data.assign(&data[pos], &data[pos] + dataLength);
            pos += dataLength;
            applications.push_back(ua);
        }

        const auto key = std::make_pair(sid, scids);
        auto it = m.userApplications.find(key);
        bool same = (it != m.userApplications.end()) && (it->second.size() == applications.size());
        if (same)
        {
            for (std::size_t i = 0; i < applications.size(); i++)
            {
                if ((applications[i].type != it->second[i].type) || (applications[i].data != it->second[i].data)) {
                    same = false;
                }
            }
        }
        if (!same)
        {
            m.userApplications[key] = applications;
            if (!next) {
                changed();
            }
        }
    }
}

// FEC sub-channel organization
void DABEnsemble::fig0_14(const uint8_t *data, int length, bool next)
{
    MCI& m = mci(next);

    for (int pos = 0; pos < length; pos++)
    {
        const int id = data[pos] >> 2;
        const int fec = data[pos] & 0x03;
        if (m.fecSchemes[id] != fec)
        {
            m.fecSchemes[id] = fec;
            auto it = m.subchannels.find(id);
            if (it != m.subchannels.end()) {
                it->second.fecScheme = fec;
            }
            if (!next) {
                changed();
            }
        }
    }
}

// Programme type
void DABEnsemble::fig0_17(const uint8_t *data, int length)
{
    int pos = 0;

    while (pos + 4 <= length)
    {
        const uint32_t sid = readSId(&data[pos], false);
        const bool dynamic = (data[pos + 2] & 0x80) != 0;
        // Earlier revisions had language and complementary code flags here
        const bool languageFlag = (data[pos + 2] & 0x20) != 0;
        const bool complementaryFlag = (data[pos + 2] & 0x10) != 0;
        int codePos = pos + 3 + (languageFlag ? 1 : 0);
        if (codePos >= length) {
            break;
        }
        const int pty = data[codePos] & 0x1f;
        pos = codePos + 1 + (complementaryFlag ? 1 : 0);

        auto it = m_programmeTypes.find(sid);
        if ((it == m_programmeTypes.end()) || (it->second.first != pty) || (it->second.second != dynamic))
        {
            m_programmeTypes[sid] = std::make_pair(pty, dynamic);
            changed();
        }
    }
}

void DABEnsemble::setLabel(int extension, const uint8_t *id, int idLength, const std::string& label, const std::string& shortLabel, bool extended)
{
    Label *target = nullptr;

    switch (extension)
    {
    case 0:
        if (idLength >= 2) {
            target = &m_ensembleLabel;
        }
        break;
    case 1:
        if (idLength >= 2) {
            target = &m_serviceLabels[readSId(id, false)];
        }
        break;
    case 4:
        if (idLength >= 1)
        {
            const bool pd = (id[0] & 0x80) != 0;
            const int scids = id[0] & 0x0f;
            if (idLength >= 1 + (pd ? 4 : 2)) {
                target = &m_componentLabels[std::make_pair(readSId(id + 1, pd), scids)];
            }
        }
        break;
    case 5:
        if (idLength >= 4) {
            target = &m_serviceLabels[readSId(id, true)];
        }
        break;
    default:
        break;
    }

    if (!target) {
        return;
    }
    // A FIG 2 label is preferred to the FIG 1 label
    if (target->extended && !extended) {
        return;
    }
    if ((target->label != label) || (target->shortLabel != shortLabel))
    {
        target->label = label;
        target->shortLabel = shortLabel;
        target->extended = extended;
        if (extension == 0)
        {
            m_info.label = label;
            m_info.shortLabel = shortLabel;
        }
        changed();
    }
}

static int labelIdentifierLength(int extension, const uint8_t *data, int length)
{
    switch (extension)
    {
    case 0:
    case 1:
        return 2;
    case 4:
        return length >= 1 ? ((data[0] & 0x80) ? 5 : 3) : -1;
    case 5:
        return 4;
    default:
        return -1;
    }
}

// Labels
void DABEnsemble::fig1(const uint8_t *data, int length)
{
    const int charset = data[0] >> 4;
    const int extension = data[0] & 0x07;
    const uint8_t *id = data + 1;
    const int idLength = labelIdentifierLength(extension, id, length - 1);

    if ((idLength < 0) || (1 + idLength + 16 + 2 > length)) {
        return;
    }

    const uint8_t *chars = id + idLength;
    const uint16_t flags = static_cast<uint16_t>((chars[16] << 8) | chars[17]);
    std::string label = DABTables::labelToUTF8(chars, 16, charset);
    std::string shortText = shortLabel(chars, 16, charset, flags);

    // Labels are padded with spaces to 16 characters
    while (!label.empty() && (label.back() == ' ')) {
        label.pop_back();
    }
    setLabel(extension, id, idLength, label, shortText, false);
}

// Extended labels
void DABEnsemble::fig2(const uint8_t *data, int length)
{
    const int toggle = data[0] >> 7;
    const int segmentIndex = (data[0] >> 4) & 0x07;
    const int extension = data[0] & 0x07;
    const uint8_t *id = data + 1;
    const int idLength = labelIdentifierLength(extension, id, length - 1);

    if ((idLength < 0) || (1 + idLength > length)) {
        return;
    }

    uint32_t key = 0;
    for (int i = 0; i < idLength; i++) {
        key = (key << 8) ^ id[i];
    }
    ExtendedLabelSegments& label = m_extendedLabels[std::make_pair(extension, key)];

    if (label.toggle != toggle)
    {
        label = ExtendedLabelSegments();
        label.toggle = toggle;
    }

    const uint8_t *segment = id + idLength;
    const int segmentLength = length - 1 - idLength;
    label.segments[segmentIndex].assign(segment, segment + segmentLength);
    label.received[segmentIndex] = true;

    if (!label.received[0] || label.segments[0].size() < 3) {
        return;
    }
    const int count = ((label.segments[0][0] >> 4) & 0x07) + 1;
    for (int i = 0; i < count; i++)
    {
        if (!label.received[i]) {
            return;
        }
    }

    std::vector<uint8_t> field;
    for (int i = 0; i < count; i++) {
        field.insert(field.end(), label.segments[i].begin(), label.segments[i].end());
    }

    if (field.size() < 3) {
        return;
    }

    const bool ucs2 = (field[0] & 0x80) != 0;
    const uint16_t flags = static_cast<uint16_t>((field[1] << 8) | field[2]);
    const int charset = ucs2 ? 6 : 15;
    const uint8_t *chars = field.data() + 3;
    const int charsLength = static_cast<int>(field.size()) - 3;

    setLabel(extension, id, idLength, DABTables::labelToUTF8(chars, charsLength, charset),
        shortLabel(chars, charsLength, charset, flags), true);
}

bool DABEnsemble::northAmericanPTy() const
{
    return m_info.internationalTable == 2;
}

bool DABEnsemble::subchannel(int id, DABSubchannel& subchannel) const
{
    auto it = m_current.subchannels.find(id);
    if (it == m_current.subchannels.end()) {
        return false;
    }
    subchannel = it->second;
    return true;
}

std::vector<DABSubchannel> DABEnsemble::subchannels() const
{
    std::vector<DABSubchannel> list;

    for (const auto& s : m_current.subchannels) {
        list.push_back(s.second);
    }
    return list;
}

bool DABEnsemble::service(uint32_t sid, DABService& service) const
{
    for (const auto& s : services())
    {
        if (s.sid == sid)
        {
            service = s;
            return true;
        }
    }
    return false;
}

std::vector<DABService> DABEnsemble::services() const
{
    std::vector<DABService> list;

    for (const auto& entry : m_current.services)
    {
        const uint32_t sid = entry.first;
        const ServiceDescription& description = entry.second;
        DABService service;

        service.sid = sid;
        service.dataService = description.dataService;
        service.caId = description.caId;

        auto label = m_serviceLabels.find(sid);
        if (label != m_serviceLabels.end())
        {
            service.label = label->second.label;
            service.shortLabel = label->second.shortLabel;
        }
        auto pty = m_programmeTypes.find(sid);
        if (pty != m_programmeTypes.end())
        {
            service.pty = pty->second.first;
            service.ptyDynamic = pty->second.second;
        }
        if (description.dataService)
        {
            service.ecc = static_cast<int>(sid >> 24);
        }
        else
        {
            auto ecc = m_serviceECCs.find(sid);
            service.ecc = ecc != m_serviceECCs.end() ? ecc->second : m_info.ecc;
        }

        for (const auto& cd : description.components)
        {
            DABServiceComponent c;
            c.transport = cd.transport;
            c.primary = cd.primary;
            c.ca = cd.ca;
            c.subchannelId = cd.subchannelId;
            c.scid = cd.scid;
            if (cd.transport == DABServiceComponent::StreamAudio) {
                c.ascty = cd.type;
            } else if (cd.transport == DABServiceComponent::StreamData) {
                c.dscty = cd.type;
            }

            if (cd.transport == DABServiceComponent::PacketData)
            {
                auto pc = m_current.packetComponents.find(cd.scid);
                if (pc != m_current.packetComponents.end())
                {
                    c.subchannelId = pc->second.subchannelId;
                    c.packetAddress = pc->second.packetAddress;
                    c.dscty = pc->second.dscty;
                    c.dataGroups = pc->second.dataGroups;
                }
            }

            // SCIdS: 0 for the primary component, otherwise from FIG 0/8
            if (cd.primary) {
                c.scids = 0;
            }
            for (const auto& gd : m_current.globalDefinitions)
            {
                if (gd.first.first != sid) {
                    continue;
                }
                const bool match = gd.second.longForm
                    ? ((cd.transport == DABServiceComponent::PacketData) && (gd.second.scid == cd.scid))
                    : ((cd.transport != DABServiceComponent::PacketData) && (gd.second.subchannelId == cd.subchannelId));
                if (match)
                {
                    c.scids = gd.first.second;
                    break;
                }
            }

            if (c.scids >= 0)
            {
                auto ua = m_current.userApplications.find(std::make_pair(sid, c.scids));
                if (ua != m_current.userApplications.end())
                {
                    for (DABUserApplication app : ua->second)
                    {
                        // Applications of audio components are in the X-PAD, with an X-PAD header
                        if (c.isAudio() && (app.data.size() >= 2))
                        {
                            app.xpad = true;
                            const bool caOrg = (app.data[0] & 0x40) != 0;
                            app.xpadAppType = app.data[0] & 0x1f;
                            app.dataGroups = (app.data[1] & 0x80) == 0;
                            app.dscty = app.data[1] & 0x3f;
                            const std::size_t skip = 2 + (caOrg ? 2 : 0);
                            app.data.erase(app.data.begin(), app.data.begin() + std::min(skip, app.data.size()));
                        }
                        else
                        {
                            app.dscty = c.dscty;
                            app.dataGroups = c.dataGroups;
                        }
                        c.userApplications.push_back(app);
                    }
                }
                auto cl = m_componentLabels.find(std::make_pair(sid, c.scids));
                if (cl != m_componentLabels.end()) {
                    c.label = cl->second.label;
                }
            }

            if (c.transport == DABServiceComponent::PacketData)
            {
                auto lang = m_scidLanguages.find(c.scid);
                if (lang != m_scidLanguages.end()) {
                    c.language = lang->second;
                }
            }
            else
            {
                auto lang = m_subchannelLanguages.find(c.subchannelId);
                if (lang != m_subchannelLanguages.end()) {
                    c.language = lang->second;
                }
            }

            service.components.push_back(c);
        }

        // Primary component first
        std::stable_sort(service.components.begin(), service.components.end(),
            [](const DABServiceComponent& a, const DABServiceComponent& b) { return a.primary && !b.primary; });

        list.push_back(service);
    }
    return list;
}

int DABAnnouncementSwitch::type() const
{
    for (int bit = 0; bit < 16; bit++)
    {
        if (flags & (1U << bit)) {
            return bit;
        }
    }
    return -1;
}

// Service linking (clause 8.1.15). The first Id of a set's database is the key service, a
// service of this ensemble; the Id list qualifier applies to the others.
void DABEnsemble::fig0_6(const uint8_t *data, int length, bool cn, bool pd)
{
    int pos = 0;

    while (pos + 2 <= length)
    {
        const bool idList = (data[pos] & 0x80) != 0;
        const bool active = (data[pos] & 0x40) != 0;
        const bool hard = (data[pos] & 0x20) != 0;
        const bool international = (data[pos] & 0x10) != 0;
        const int lsn = ((data[pos] & 0x0f) << 8) | data[pos + 1];
        pos += 2;
        const LinkageKey key{pd, hard, international, lsn};

        if (!idList)
        {
            // Change event indication: the set is being changed
            if (m_linkageSets.erase(key) > 0) {
                changed();
            }
            continue;
        }
        if (pos >= length) {
            return;
        }

        const int idlq = (data[pos] >> 5) & 0x03;
        const int count = data[pos] & 0x0f;
        pos++;
        const int idSize = pd ? 4 : (international ? 3 : 2);
        if (pos + count * idSize > length) {
            return;
        }

        std::vector<DABLinkedId> ids;
        for (int i = 0; i < count; i++, pos += idSize)
        {
            DABLinkedId id;
            id.kind = idlq;
            if (pd)
            {
                id.id = (static_cast<uint32_t>(data[pos]) << 24) | (data[pos + 1] << 16) | (data[pos + 2] << 8) | data[pos + 3];
                if (idlq == DABLinkedId::DRMService) {
                    id.id &= 0xffffff;
                }
            }
            else if (international)
            {
                id.id = (data[pos + 1] << 8) | data[pos + 2];
                if (idlq == DABLinkedId::DRMService) {
                    id.id |= data[pos] << 16;       // The ECC field holds the identifier's MSB
                } else {
                    id.ecc = data[pos];
                }
            }
            else
            {
                id.id = (data[pos] << 8) | data[pos + 1];
            }
            ids.push_back(id);
        }

        if ((lsn == 0) || ids.empty() || ((m_linkageSets.size() >= MaximumDatabaseEntries) && !m_linkageSets.count(key))) {
            continue;
        }

        DABLinkageSet set = m_linkageSets.count(key) ? m_linkageSets[key] : DABLinkageSet();
        set.lsn = lsn;
        set.active = active;
        set.hard = hard;
        set.international = international;
        std::size_t first = 0;
        if (!cn)
        {
            // The start of the set's database: the key service, then the others
            set.keyService = ids[0].id;
            set.ids.clear();
            first = 1;
        }
        for (std::size_t i = first; (i < ids.size()) && (set.ids.size() < MaximumDatabaseIds); i++)
        {
            if (std::find(set.ids.begin(), set.ids.end(), ids[i]) == set.ids.end()) {
                set.ids.push_back(ids[i]);
            }
        }

        const auto old = m_linkageSets.find(key);
        if ((old == m_linkageSets.end()) || (old->second.keyService != set.keyService) || !(old->second.ids == set.ids)
            || (old->second.active != set.active))
        {
            m_linkageSets[key] = set;
            changed();
        }
    }
}

// Announcement support (clause 8.1.6.1)
void DABEnsemble::fig0_18(const uint8_t *data, int length)
{
    int pos = 0;

    while (pos + 5 <= length)
    {
        const uint32_t sid = (data[pos] << 8) | data[pos + 1];
        DABAnnouncementSupport support;
        support.flags = static_cast<uint16_t>((data[pos + 2] << 8) | data[pos + 3]);
        const int clusters = data[pos + 4] & 0x1f;    // Rfa (3 bits), then the number of clusters (5 bits)
        pos += 5;
        if (pos + clusters > length) {
            return;
        }
        for (int i = 0; i < clusters; i++) {
            support.clusters.push_back(data[pos + i]);
        }
        pos += clusters;

        const auto old = m_announcementSupport.find(sid);
        if ((old == m_announcementSupport.end()) || (old->second.flags != support.flags) || (old->second.clusters != support.clusters))
        {
            if ((old == m_announcementSupport.end()) && (m_announcementSupport.size() >= MaximumDatabaseEntries)) {
                continue;
            }
            m_announcementSupport[sid] = support;
            changed();
        }
    }
}

// Announcement switching (clause 8.1.6.2, and TS 103 176 clause 7.3)
void DABEnsemble::fig0_19(const uint8_t *data, int length)
{
    int pos = 0;

    while (pos + 4 <= length)
    {
        DABAnnouncementSwitch announcement;
        announcement.cluster = data[pos];
        announcement.flags = static_cast<uint16_t>((data[pos + 1] << 8) | data[pos + 2]);
        announcement.newFlag = (data[pos + 3] & 0x80) != 0;
        const bool region = (data[pos + 3] & 0x40) != 0;   // EN 300 401 V1: a region Id follows
        announcement.subchannelId = data[pos + 3] & 0x3f;
        pos += region ? 5 : 4;

        const uint16_t flags = announcement.flags;
        const bool singleType = (flags != 0) && ((flags & (flags - 1)) == 0);

        if (announcement.cluster == 0x00) {
            continue;   // Not used
        }
        if (flags == 0)
        {
            // The end of the announcement
            m_announcementLastCIF.erase(announcement.cluster);
            if (m_announcements.erase(announcement.cluster) > 0) {
                changed();
            }
            continue;
        }
        // One type only; alarms only in, and only, cluster 0xff
        if (!singleType || ((announcement.cluster == 0xff) != (flags == 1))) {
            continue;
        }

        m_announcementLastCIF[announcement.cluster] = m_cifs;
        const auto old = m_announcements.find(announcement.cluster);
        if ((old == m_announcements.end()) || (old->second.flags != flags) || (old->second.subchannelId != announcement.subchannelId))
        {
            m_announcements[announcement.cluster] = announcement;
            changed();
        }
    }
}

// Frequency information (clause 8.1.8)
void DABEnsemble::fig0_21(const uint8_t *data, int length, bool cn, bool oe)
{
    int pos = 0;

    while (pos + 2 <= length)
    {
        const int end = std::min(length, pos + 2 + (data[pos + 1] & 0x1f));
        pos += 2;

        while (pos + 3 <= end)
        {
            DABFrequencyInformation fi;
            fi.id = (data[pos] << 8) | data[pos + 1];
            fi.rm = data[pos + 2] >> 4;
            fi.continuity = (data[pos + 2] & 0x08) != 0;
            fi.otherEnsemble = oe;
            const int listLength = data[pos + 2] & 0x07;
            pos += 3;
            if (pos + listLength > end) {
                return;
            }
            const uint8_t *list = data + pos;
            pos += listLength;

            switch (fi.rm)
            {
            case DABFrequencyInformation::Ensemble:
                for (int i = 0; i + 3 <= listLength; i += 3)
                {
                    const int control = list[i] >> 3;
                    const int freq = ((list[i] & 0x07) << 16) | (list[i + 1] << 8) | list[i + 2];
                    if (freq > 0) {
                        fi.frequencies.push_back({freq * 16000LL, (control & 0x01) == 0});
                    }
                }
                break;
            case DABFrequencyInformation::FMRDS:
                for (int i = 0; i < listLength; i++)
                {
                    if ((list[i] >= 1) && (list[i] <= 204)) {
                        fi.frequencies.push_back({87500000LL + list[i] * 100000LL, true});
                    }
                }
                break;
            case DABFrequencyInformation::DRM:
            case DABFrequencyInformation::AMSS:
                // The identifier's MSB, then 16-bit frequencies
                if (listLength >= 1)
                {
                    fi.id |= list[0] << 16;
                    for (int i = 1; i + 2 <= listLength; i += 2)
                    {
                        const int freq = ((list[i] & 0x7f) << 8) | list[i + 1];
                        const int64_t step = ((fi.rm == DABFrequencyInformation::DRM) && (list[i] & 0x80)) ? 10000 : 1000;
                        if (freq > 0) {
                            fi.frequencies.push_back({freq * step, true});
                        }
                    }
                }
                break;
            default:
                continue;   // Other R&M values are not used now
            }

            const auto key = std::make_tuple(oe, fi.rm, fi.id);
            if (listLength == 0)
            {
                // Change event indication
                if (m_frequencies.erase(key) > 0) {
                    changed();
                }
                continue;
            }

            auto old = m_frequencies.find(key);
            if (old == m_frequencies.end())
            {
                if (m_frequencies.size() < MaximumDatabaseEntries)
                {
                    m_frequencies[key] = fi;
                    changed();
                }
                continue;
            }

            DABFrequencyInformation& entry = old->second;
            if (!cn) {
                entry.continuity = fi.continuity;
            }
            std::vector<DABFrequencyInformation::Frequency> frequencies = cn ? entry.frequencies : std::vector<DABFrequencyInformation::Frequency>();
            for (const auto& f : fi.frequencies)
            {
                const bool known = std::any_of(frequencies.begin(), frequencies.end(),
                    [&f](const DABFrequencyInformation::Frequency& g) { return g.hz == f.hz; });
                if (!known && (frequencies.size() < MaximumDatabaseIds)) {
                    frequencies.push_back(f);
                }
            }
            const bool same = (frequencies.size() == entry.frequencies.size())
                && std::equal(frequencies.begin(), frequencies.end(), entry.frequencies.begin(),
                    [](const DABFrequencyInformation::Frequency& a, const DABFrequencyInformation::Frequency& b) {
                        return (a.hz == b.hz) && (a.adjacent == b.adjacent);
                    });
            if (!same)
            {
                entry.frequencies = frequencies;
                changed();
            }
        }
        pos = end;
    }
}

// OE services: the ensembles that carry a service (clause 8.1.10)
void DABEnsemble::fig0_24(const uint8_t *data, int length, bool cn, bool oe, bool pd)
{
    const int sidLength = pd ? 4 : 2;
    int pos = 0;

    while (pos + sidLength + 1 <= length)
    {
        uint32_t sid = 0;
        for (int i = 0; i < sidLength; i++) {
            sid = (sid << 8) | data[pos + i];
        }
        pos += sidLength;
        const int count = data[pos] & 0x0f;
        pos++;
        if (pos + 2 * count > length) {
            return;
        }

        const auto key = std::make_pair(pd, sid);
        if (count == 0)
        {
            // Change event indication
            if (m_oeServices.erase(key) > 0) {
                changed();
            }
            continue;
        }

        if (!m_oeServices.count(key) && (m_oeServices.size() >= MaximumDatabaseEntries))
        {
            pos += 2 * count;
            continue;
        }
        OEServices& entry = m_oeServices[key];
        std::vector<int> eids = cn ? entry.eids : std::vector<int>();
        for (int i = 0; i < count; i++, pos += 2)
        {
            const int eid = (data[pos] << 8) | data[pos + 1];
            if ((std::find(eids.begin(), eids.end(), eid) == eids.end()) && (eids.size() < MaximumDatabaseIds)) {
                eids.push_back(eid);
            }
        }
        if ((eids != entry.eids) || (entry.otherEnsemble != oe))
        {
            entry.eids = eids;
            entry.otherEnsemble = oe;
            changed();
        }
    }
}

DABAnnouncementSupport DABEnsemble::announcementSupport(uint32_t sid) const
{
    const auto it = m_announcementSupport.find(sid);
    return it == m_announcementSupport.end() ? DABAnnouncementSupport() : it->second;
}

std::vector<DABLinkageSet> DABEnsemble::linkageSets() const
{
    std::vector<DABLinkageSet> sets;
    for (const auto& set : m_linkageSets) {
        sets.push_back(set.second);
    }
    return sets;
}

std::vector<DABFrequencyInformation> DABEnsemble::frequencyInformation() const
{
    std::vector<DABFrequencyInformation> list;
    for (const auto& fi : m_frequencies) {
        list.push_back(fi.second);
    }
    return list;
}

std::vector<int> DABEnsemble::otherEnsembles(uint32_t sid) const
{
    std::vector<int> eids;
    for (bool pd : {false, true})
    {
        const auto it = m_oeServices.find(std::make_pair(pd, sid));
        if (it == m_oeServices.end()) {
            continue;
        }
        for (int eid : it->second.eids)
        {
            if ((eid != m_info.eid) && (std::find(eids.begin(), eids.end(), eid) == eids.end())) {
                eids.push_back(eid);
            }
        }
    }
    return eids;
}

std::vector<DABServiceLink> DABEnsemble::serviceLinks(uint32_t sid) const
{
    std::vector<DABServiceLink> links;

    // The frequencies of an ensemble, or of a service on another system
    const auto frequencies = [this](int rm, uint32_t id) {
        std::vector<int64_t> hz;
        for (const auto& fi : m_frequencies)
        {
            if ((fi.second.rm != rm) || (fi.second.id != id)) {
                continue;
            }
            for (const auto& f : fi.second.frequencies)
            {
                if (std::find(hz.begin(), hz.end(), f.hz) == hz.end()) {
                    hz.push_back(f.hz);
                }
            }
        }
        return hz;
    };
    const auto ensembleFrequencies = [&frequencies](const std::vector<int>& eids) {
        std::vector<int64_t> hz;
        for (int eid : eids)
        {
            for (int64_t f : frequencies(DABFrequencyInformation::Ensemble, static_cast<uint32_t>(eid)))
            {
                if (std::find(hz.begin(), hz.end(), f) == hz.end()) {
                    hz.push_back(f);
                }
            }
        }
        return hz;
    };

    const std::vector<int> others = otherEnsembles(sid);
    if (!others.empty())
    {
        DABServiceLink link;
        link.kind = DABServiceLink::SameService;
        link.id = sid;
        link.hard = true;
        link.active = true;
        link.eids = others;
        link.frequencies = ensembleFrequencies(others);
        links.push_back(link);
    }

    for (const auto& entry : m_linkageSets)
    {
        const DABLinkageSet& set = entry.second;
        if (set.keyService != sid) {
            continue;
        }
        for (const DABLinkedId& id : set.ids)
        {
            DABServiceLink link;
            link.id = id.id;
            link.ecc = id.ecc;
            link.hard = set.hard;
            link.active = set.active;

            if (id.kind == DABLinkedId::Service)
            {
                if (id.id == sid) {
                    continue;
                }
                link.kind = DABServiceLink::LinkedDAB;
                const auto label = m_serviceLabels.find(id.id);
                if (label != m_serviceLabels.end()) {
                    link.label = label->second.label;
                }
                link.eids = otherEnsembles(id.id);
                link.frequencies = ensembleFrequencies(link.eids);
            }
            else if (id.kind == DABLinkedId::RDSPI)
            {
                link.kind = DABServiceLink::LinkedFM;
                link.frequencies = frequencies(DABFrequencyInformation::FMRDS, id.id);
            }
            else if (id.kind == DABLinkedId::DRMService)
            {
                link.kind = DABServiceLink::LinkedDRM;
                link.frequencies = frequencies(DABFrequencyInformation::DRM, id.id);
                for (int64_t f : frequencies(DABFrequencyInformation::AMSS, id.id)) {
                    link.frequencies.push_back(f);
                }
            }
            else
            {
                continue;
            }

            // A service in two sets: the stronger link
            const auto same = std::find_if(links.begin(), links.end(), [&link](const DABServiceLink& l) {
                return (l.kind == link.kind) && (l.id == link.id);
            });
            if (same == links.end())
            {
                links.push_back(link);
            }
            else
            {
                same->hard = same->hard || link.hard;
                same->active = same->active || link.active;
            }
        }
    }
    return links;
}

int DABEnsemble::subchannelASCTy(int subchannelId) const
{
    for (const auto& service : m_current.services)
    {
        for (const auto& component : service.second.components)
        {
            if ((component.transport == DABServiceComponent::StreamAudio) && (component.subchannelId == subchannelId)) {
                return component.type;
            }
        }
    }
    return -1;
}

std::string DABEnsemble::subchannelServiceLabel(int subchannelId) const
{
    std::string label;
    for (const auto& service : m_current.services)
    {
        for (const auto& component : service.second.components)
        {
            if ((component.transport == DABServiceComponent::StreamAudio) && (component.subchannelId == subchannelId))
            {
                const auto it = m_serviceLabels.find(service.first);
                if ((it != m_serviceLabels.end()) && (component.primary || label.empty())) {
                    label = it->second.label;
                }
            }
        }
    }
    return label;
}

} // namespace modemdab
