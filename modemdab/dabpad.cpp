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
#include "dabpad.h"
#include "dabtables.h"

namespace modemdab
{

namespace
{

// Length of a variable size X-PAD data sub-field (EN 300 401 clause 7.4.4.2)
const int subfieldLengths[8] = {4, 6, 8, 12, 16, 24, 32, 48};

// Split a label into characters (byte ranges), as DL Plus markers count characters
std::vector<std::pair<int, int>> characters(const std::vector<uint8_t>& data, int charset)
{
    std::vector<std::pair<int, int>> chars;
    int i = 0;
    const int length = static_cast<int>(data.size());

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

} // anonymous namespace

std::string DABDLPlusObject::contentTypeName(int contentType)
{
    static const char *names[64] = {
        "DUMMY", "ITEM.TITLE", "ITEM.ALBUM", "ITEM.TRACKNUMBER", "ITEM.ARTIST", "ITEM.COMPOSITION",
        "ITEM.MOVEMENT", "ITEM.CONDUCTOR", "ITEM.COMPOSER", "ITEM.BAND", "ITEM.COMMENT", "ITEM.GENRE",
        "INFO.NEWS", "INFO.NEWS.LOCAL", "INFO.STOCKMARKET", "INFO.SPORT", "INFO.LOTTERY", "INFO.HOROSCOPE",
        "INFO.DAILY_DIVERSION", "INFO.HEALTH", "INFO.EVENT", "INFO.SCENE", "INFO.CINEMA", "INFO.TV",
        "INFO.DATE_TIME", "INFO.WEATHER", "INFO.TRAFFIC", "INFO.ALARM", "INFO.ADVERTISEMENT", "INFO.URL",
        "INFO.OTHER", "STATIONNAME.SHORT", "STATIONNAME.LONG", "PROGRAMME.NOW", "PROGRAMME.NEXT",
        "PROGRAMME.PART", "PROGRAMME.HOST", "PROGRAMME.EDITORIAL_STAFF", "PROGRAMME.FREQUENCY",
        "PROGRAMME.HOMEPAGE", "PROGRAMME.SUBCHANNEL", "PHONE.HOTLINE", "PHONE.STUDIO", "PHONE.OTHER",
        "SMS.STUDIO", "SMS.OTHER", "EMAIL.HOTLINE", "EMAIL.STUDIO", "EMAIL.OTHER", "MMS.OTHER", "CHAT",
        "CHAT.CENTER", "VOTE.QUESTION", "VOTE.CENTRE", "", "", "", "", "", "DESCRIPTOR.PLACE",
        "DESCRIPTOR.APPOINTMENT", "DESCRIPTOR.IDENTIFIER", "DESCRIPTOR.PURCHASE", "DESCRIPTOR.GET_DATA"
    };

    if ((contentType >= 0) && (contentType < 64)) {
        return names[contentType];
    }
    return "";
}

void DABPADDecoder::Segments::clear()
{
    toggle = -1;
    last = -1;
    charset = 0;
    for (auto& d : data) {
        d.clear();
    }
    received.fill(false);
}

bool DABPADDecoder::Segments::complete() const
{
    if (last < 0) {
        return false;
    }
    for (int i = 0; i <= last; i++)
    {
        if (!received[i]) {
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> DABPADDecoder::Segments::join() const
{
    std::vector<uint8_t> joined;

    for (int i = 0; i <= last; i++) {
        joined.insert(joined.end(), data[i].begin(), data[i].end());
    }
    return joined;
}

DABPADDecoder::DABPADDecoder() :
    m_motAppType(12),
    m_journalineAppType(-1)
{
    reset();
}

void DABPADDecoder::reset()
{
    m_lastAppType = -1;
    m_lastXPadLength = 0;
    for (auto& g : m_groupData) {
        g.clear();
    }
    m_groupActive.fill(false);
    m_nextDataGroupLength = 0;
    m_motGroupLength = 0;
    m_journalineGroupLength = 0;
    m_labelSegments.clear();
    m_commandSegments.clear();
    m_labelBytes.clear();
    m_labelCharset = 0;
    m_label = DABDynamicLabel();
    m_labelUpdated = false;
    m_motDataGroups.clear();
    m_journalineDataGroups.clear();
    m_crcErrors = 0;
}

void DABPADDecoder::setJournalineApplicationType(int xpadAppType)
{
    m_journalineAppType = ((xpadAppType > 3) && (xpadAppType < 31) && (xpadAppType != 12) && (xpadAppType != 13)) ? xpadAppType : -1;
}

void DABPADDecoder::setMOTApplicationType(int xpadAppType)
{
    m_motAppType = ((xpadAppType > 3) && (xpadAppType < 31)) ? xpadAppType : 12;
}

bool DABPADDecoder::labelUpdated()
{
    bool updated = m_labelUpdated;
    m_labelUpdated = false;
    return updated;
}

int DABPADDecoder::continuationOf(int appType) const
{
    if ((appType == 2) || (appType == 3)) {
        return 3;
    }
    if ((appType == 12) || (appType == 13)) {
        return 13;
    }
    if ((appType == m_motAppType) || (appType == m_motAppType + 1)) {
        return m_motAppType + 1;
    }
    if ((m_journalineAppType > 0) && ((appType == m_journalineAppType) || (appType == m_journalineAppType + 1))) {
        return m_journalineAppType + 1;
    }
    return appType;
}

void DABPADDecoder::process(const uint8_t *xpadEnd, int xpadAvailable, const uint8_t *fpad)
{
    DABProfileScope profile(DABProfile::Data);
    const int fpadType = fpad[0] >> 6;
    const int xpadIndicator = (fpad[0] >> 4) & 0x03;
    const bool contentsIndicator = (fpad[1] & 0x02) != 0;

    if (fpadType != 0) {
        return;
    }

    // X-PAD bytes are transmitted in reverse order
    auto logical = [xpadEnd](int i) { return xpadEnd[-1 - i]; };
    std::vector<uint8_t> field;

    if (xpadIndicator == 1)
    {
        // Short X-PAD: four bytes
        if (xpadAvailable < 4) {
            return;
        }
        for (int i = 0; i < 4; i++) {
            field.push_back(logical(i));
        }
        if (contentsIndicator)
        {
            const int appType = field[0] & 0x1f;
            if (appType != 0) {
                subfield(appType, &field[1], 3);
            }
            m_lastAppType = appType == 0 ? -1 : appType;
        }
        else if (m_lastAppType > 0)
        {
            const int appType = continuationOf(m_lastAppType);
            subfield(appType, field.data(), 4);
            m_lastAppType = appType;
        }
        m_lastXPadLength = 4;
    }
    else if (xpadIndicator == 2)
    {
        // Variable size X-PAD
        if (contentsIndicator)
        {
            std::vector<std::pair<int, int>> list;
            int position = 0;

            while ((position < 4) && (position < xpadAvailable))
            {
                const uint8_t ci = logical(position++);
                const int appType = ci & 0x1f;
                if (appType == 0) {
                    break;
                }
                list.emplace_back(appType, subfieldLengths[ci >> 5]);
            }

            int total = position;
            for (const auto& entry : list) {
                total += entry.second;
            }
            if (total > xpadAvailable)
            {
                m_lastAppType = -1;
                return;
            }

            for (int i = 0; i < total; i++) {
                field.push_back(logical(i));
            }
            for (const auto& entry : list)
            {
                subfield(entry.first, &field[position], entry.second);
                position += entry.second;
                m_lastAppType = entry.first;
            }
            // An X-PAD without contents indicators continues the last data sub-field, with its
            // length (EN 300 401 clause 7.4.2.2)
            m_lastXPadLength = list.empty() ? 0 : list.back().second;
            if (list.empty()) {
                m_lastAppType = -1;
            }
        }
        else if ((m_lastAppType > 0) && (m_lastXPadLength > 0) && (m_lastXPadLength <= xpadAvailable))
        {
            // One sub-field continuing the last application, as long as its last sub-field
            for (int i = 0; i < m_lastXPadLength; i++) {
                field.push_back(logical(i));
            }
            const int appType = continuationOf(m_lastAppType);
            subfield(appType, field.data(), m_lastXPadLength);
            m_lastAppType = appType;
        }
    }
}

void DABPADDecoder::startGroup(Group group)
{
    m_groupData[group].clear();
    m_groupActive[group] = true;
}

void DABPADDecoder::subfield(int appType, const uint8_t *data, int length)
{
    if (appType == 1)
    {
        // Data group length indicator; a short X-PAD can split it over two fields
        if (!m_groupActive[GroupLengthIndicator] || m_groupData[GroupLengthIndicator].size() >= 4) {
            startGroup(GroupLengthIndicator);
        }
        appendGroup(GroupLengthIndicator, data, length);
    }
    else if (appType == 2)
    {
        startGroup(GroupDynamicLabel);
        appendGroup(GroupDynamicLabel, data, length);
    }
    else if (appType == 3)
    {
        appendGroup(GroupDynamicLabel, data, length);
    }
    else if ((appType == 12) || (appType == m_motAppType))
    {
        startGroup(GroupMOT);
        m_motGroupLength = m_nextDataGroupLength;
        m_nextDataGroupLength = 0;
        appendGroup(GroupMOT, data, length);
    }
    else if ((appType == 13) || (appType == m_motAppType + 1))
    {
        appendGroup(GroupMOT, data, length);
    }
    else if ((m_journalineAppType > 0) && (appType == m_journalineAppType))
    {
        startGroup(GroupJournaline);
        m_journalineGroupLength = m_nextDataGroupLength;
        m_nextDataGroupLength = 0;
        appendGroup(GroupJournaline, data, length);
    }
    else if ((m_journalineAppType > 0) && (appType == m_journalineAppType + 1))
    {
        appendGroup(GroupJournaline, data, length);
    }
}

void DABPADDecoder::appendGroup(Group group, const uint8_t *data, int length)
{
    if (!m_groupActive[group]) {
        return;
    }

    std::vector<uint8_t>& groupData = m_groupData[group];
    groupData.insert(groupData.end(), data, data + length);

    switch (group)
    {
    case GroupLengthIndicator:
        if (groupData.size() >= 4)
        {
            if (DABTables::crc16Check(groupData.data(), 4)) {
                m_nextDataGroupLength = ((groupData[0] & 0x3f) << 8) | groupData[1];
            } else {
                m_crcErrors++;
            }
            m_groupActive[group] = false;
        }
        break;

    case GroupDynamicLabel:
        if (groupData.size() >= 2)
        {
            const bool command = (groupData[0] & 0x10) != 0;
            int fieldLength;

            if (!command) {
                fieldLength = (groupData[0] & 0x0f) + 1;
            } else if ((groupData[0] & 0x0f) == 1) {
                fieldLength = 0;    // Clear display
            } else {
                fieldLength = (groupData[1] & 0x0f) + 1;
            }
            const std::size_t total = 2 + fieldLength + 2;
            if (groupData.size() >= total)
            {
                std::vector<uint8_t> segment(groupData.begin(), groupData.begin() + total);
                m_groupActive[group] = false;
                if (DABTables::crc16Check(segment.data(), segment.size())) {
                    dynamicLabelSegment(segment);
                } else {
                    m_crcErrors++;
                }
            }
        }
        break;

    case GroupJournaline:
        if (m_journalineGroupLength <= 0)
        {
            m_groupActive[group] = false;
        }
        else if (static_cast<int>(groupData.size()) >= m_journalineGroupLength)
        {
            groupData.resize(m_journalineGroupLength);
            m_journalineDataGroups.push_back(groupData);
            if (m_journalineDataGroups.size() > 64) {
                m_journalineDataGroups.erase(m_journalineDataGroups.begin());
            }
            m_groupActive[group] = false;
        }
        break;

    case GroupMOT:
        if (m_motGroupLength <= 0)
        {
            // Without its length the data group cannot be delimited
            m_groupActive[group] = false;
        }
        else if (static_cast<int>(groupData.size()) >= m_motGroupLength)
        {
            groupData.resize(m_motGroupLength);
            m_motDataGroups.push_back(groupData);
            if (m_motDataGroups.size() > 64) {
                m_motDataGroups.erase(m_motDataGroups.begin());
            }
            m_groupActive[group] = false;
        }
        break;

    default:
        break;
    }
}

void DABPADDecoder::dynamicLabelSegment(const std::vector<uint8_t>& segment)
{
    const int toggle = segment[0] >> 7;
    const bool first = (segment[0] & 0x40) != 0;
    const bool last = (segment[0] & 0x20) != 0;
    const bool command = (segment[0] & 0x10) != 0;
    const int field2 = segment[1] >> 4;

    if (!command)
    {
        const int length = (segment[0] & 0x0f) + 1;
        const int number = first ? 0 : (field2 & 0x07);

        if (m_labelSegments.toggle != toggle)
        {
            m_labelSegments.clear();
            m_labelSegments.toggle = toggle;
        }
        if (first) {
            m_labelSegments.charset = field2;
        }
        m_labelSegments.data[number].assign(segment.begin() + 2, segment.begin() + 2 + length);
        m_labelSegments.received[number] = true;
        if (last) {
            m_labelSegments.last = number;
        }

        if (m_labelSegments.complete() && m_labelSegments.received[0])
        {
            std::vector<uint8_t> bytes = m_labelSegments.join();
            if ((bytes != m_labelBytes) || (toggle != m_label.toggle))
            {
                const bool sameText = bytes == m_labelBytes;
                m_labelBytes = bytes;
                m_labelCharset = m_labelSegments.charset;
                m_label.text = DABTables::labelToUTF8(bytes.data(), bytes.size(), m_labelCharset);
                m_label.toggle = toggle;
                if (!sameText) {
                    m_label.objects.clear();
                }
                m_labelUpdated = true;
            }
        }
    }
    else
    {
        const int code = segment[0] & 0x0f;

        if (code == 1)
        {
            // Clear display
            if (!m_label.text.empty())
            {
                m_label.text.clear();
                m_label.objects.clear();
                m_labelBytes.clear();
                m_labelUpdated = true;
            }
        }
        else if (code == 2)
        {
            // DL Plus command (TS 102 980 clause 7.1)
            const int link = (field2 >> 3) & 1;
            const int number = first ? 0 : (field2 & 0x07);
            const int length = (segment[1] & 0x0f) + 1;

            if (m_commandSegments.toggle != toggle)
            {
                m_commandSegments.clear();
                m_commandSegments.toggle = toggle;
            }
            m_commandSegments.data[number].assign(segment.begin() + 2, segment.begin() + 2 + length);
            m_commandSegments.received[number] = true;
            if (last) {
                m_commandSegments.last = number;
            }
            if (m_commandSegments.complete()) {
                dlPlusCommand(m_commandSegments.join(), link);
            }
        }
    }
}

void DABPADDecoder::dlPlusCommand(const std::vector<uint8_t>& command, int link)
{
    if (command.empty() || ((command[0] >> 4) != 0)) {
        return;     // Only the tags command is defined
    }
    // The tags refer to the label with the same toggle bit
    if (link != m_label.toggle) {
        return;
    }

    const bool itemToggle = (command[0] & 0x08) != 0;
    const bool itemRunning = (command[0] & 0x04) != 0;
    const int tags = (command[0] & 0x03) + 1;
    std::vector<DABDLPlusObject> objects;
    const std::vector<std::pair<int, int>> chars = characters(m_labelBytes, m_labelCharset);

    for (int t = 0; t < tags; t++)
    {
        const std::size_t p = 1 + 3 * t;
        if (p + 3 > command.size()) {
            break;
        }
        DABDLPlusObject object;
        object.contentType = command[p] & 0x7f;
        const int start = command[p + 1] & 0x7f;
        const int length = (command[p + 2] & 0x7f) + 1;

        if ((object.contentType == 0) || (start >= static_cast<int>(chars.size()))) {
            continue;
        }
        const int end = std::min(static_cast<int>(chars.size()), start + length);
        std::vector<uint8_t> bytes(m_labelBytes.begin() + chars[start].first,
            m_labelBytes.begin() + chars[end - 1].first + chars[end - 1].second);
        object.text = DABTables::labelToUTF8(bytes.data(), bytes.size(), m_labelCharset);
        objects.push_back(object);
    }

    const bool changed = !m_label.dlPlus || (m_label.itemToggle != itemToggle) || (m_label.itemRunning != itemRunning)
        || (objects.size() != m_label.objects.size())
        || !std::equal(objects.begin(), objects.end(), m_label.objects.begin(),
            [](const DABDLPlusObject& a, const DABDLPlusObject& b) { return (a.contentType == b.contentType) && (a.text == b.text); });

    m_label.dlPlus = true;
    m_label.itemToggle = itemToggle;
    m_label.itemRunning = itemRunning;
    m_label.objects = objects;
    if (changed) {
        m_labelUpdated = true;
    }
}

} // namespace modemdab
