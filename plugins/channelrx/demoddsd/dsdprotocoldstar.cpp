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
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool dsdDebug() { static int d = getenv("DSD_DEBUG") ? 1 : 0; return d != 0; }

#include "dsp/dv/dvcrc.h"
#include "dsp/dv/dvdstar.h"

#include "dsdprotocoldstar.h"

DSDProtocolDStar::DSDProtocolDStar(const DSDSymbolHistory& history) :
    DSDProtocol(history),
    m_active(false),
    m_syncIndex(-1),
    m_frameBase(-1),
    m_frameIndex(0),
    m_burst(BurstNone),
    m_burstFirst(-1),
    m_burstLast(-1),
    m_completeAt(-1),
    m_misses(0),
    m_silent(0),
    m_absent(0),
    m_burstValid(false),
    m_headerValid(false),
    m_headerPublished(false),
    m_normalEnd(false),
    m_textMask(0),
    m_slowHeaderMask(0),
    m_blockLength(0),
    m_dprsRevision(0),
    m_positionValid(false),
    m_latitude(0.0),
    m_longitude(0.0)
{
    std::memset(m_header, 0, sizeof(m_header));
    std::memset(m_textBlocks, 0, sizeof(m_textBlocks));
    std::memset(m_slowHeader, 0, sizeof(m_slowHeader));
}

std::vector<DVSync::Pattern> DSDProtocolDStar::getSearchPatterns() const
{
    return { DVSync::DStarHeader, DVSync::DStarVoice };
}

std::string DSDProtocolDStar::field(int offset, int length) const
{
    if (!m_headerValid) {
        return std::string();
    }

    std::string s;

    for (int i = 0; i < length; i++)
    {
        char c = (char) m_header[offset + i];
        s += ((c >= 32) && (c < 127)) ? c : '?';
    }

    return s;
}

void DSDProtocolDStar::start(DVSync::Pattern pattern, int64_t syncIndex)
{
    m_active = true;
    m_misses = 0;
    m_silent = 0;
    m_absent = 0;
    m_headerValid = false;
    m_headerPublished = false;
    m_normalEnd = false;
    m_protocolFrames.clear();
    m_proven = false;
    m_lastSyncWithin3 = false;
    m_text.clear();
    m_textMask = 0;
    m_slowHeaderMask = 0;
    m_gpsLine.clear();
    m_dprs.clear(); // a new transmission: what was heard in the last one is not repeated
    m_positionValid = false;
    m_syncIndex = syncIndex;

    if (pattern == DVSync::DStarHeader)
    {
        // Header bits follow the sync; the first voice frame's sync ends 756 symbols later
        m_burst = BurstHeader;
        m_burstFirst = syncIndex + 1;
        m_burstLast = syncIndex + m_headerBits;
        m_frameBase = syncIndex + m_headerBits + m_frameBits;
        m_frameIndex = 0;
    }
    else
    {
        // Joined at a voice sync: the frame that carried it has no decodable header
        m_frameBase = syncIndex;
        m_frameIndex = 1;
        m_burst = BurstFrame;
        m_burstFirst = syncIndex + 1;
        m_burstLast = syncIndex + m_frameBits;
    }

    m_completeAt = m_burstLast;
    m_blockLength = 0;
}

void DSDProtocolDStar::resync(DVSync::Pattern pattern, int64_t syncIndex)
{
    (void) pattern;
    m_misses = 0;
    m_silent = 0;
    m_syncIndex = syncIndex;

    if ((syncIndex == m_frameBase) || (m_burst != BurstFrame)) {
        return;
    }

    // The sync is not where the symbol count put it: realign the superframe on it
    m_frameBase = syncIndex;

    if (m_frameIndex == 0)
    {
        // Frame 0 ends at the sync. When the sync came early the demodulator has sliced that symbol
        // already, so the burst completes with the next one
        m_burstFirst = syncIndex - m_frameBits + 1;
        m_burstLast = syncIndex;
        m_completeAt = std::max(syncIndex, m_history.newest() + 1);
    }
    else
    {
        m_burstFirst = m_frameBase + (m_frameIndex - 1) * m_frameBits + 1;
        m_burstLast = m_burstFirst + m_frameBits - 1;
        m_completeAt = m_burstLast;
    }
}

bool DSDProtocolDStar::getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const
{
    if (!m_active) {
        return false;
    }

    // The sync of the next superframe, or of this one if not yet reached
    syncIndex = (m_syncIndex < m_frameBase) ? m_frameBase : m_frameBase + m_superframeBits;
    patterns = { DVSync::DStarVoice };
    return true;
}

void DSDProtocolDStar::getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const
{
    first = syncIndex - m_frameBits + 1;
    last = syncIndex;
}

bool DSDProtocolDStar::matchPattern(int64_t lastIndex, DVSync::Pattern pattern, int maxErrors) const
{
    const DVSync::Info& info = DVSync::get(pattern);
    int64_t first = lastIndex - info.m_length + 1;

    if (!m_history.has(first) || !m_history.has(lastIndex)) {
        return false;
    }

    int errors = 0;

    for (int i = 0; i < info.m_length; i++)
    {
        int bit = (m_history.at(first + i).m_dibit >> 1) & 1;
        int expected = info.m_levels[i] < 0 ? 1 : 0;
        errors += bit != expected;
    }

    return errors <= maxErrors;
}

bool DSDProtocolDStar::confirmWithoutSync(int64_t syncIndex, bool present)
{
    if (!present)
    {
        if (++m_silent >= 2) {
            m_active = false;
        }

        m_syncIndex = syncIndex;
        return true;
    }

    if (matchPattern(syncIndex, DVSync::DStarVoice, 5))
    {
        m_misses = 0;
        m_syncIndex = syncIndex;
        m_frameBase = syncIndex;
        return true;
    }

    // Without a decoded header a lock is unproven: one missed voice sync ends it
    if ((++m_misses >= 2) || !m_headerValid) {
        m_active = false;
    }

    // Carry on as if the sync had been there; the next one decides
    m_syncIndex = syncIndex;
    m_frameBase = syncIndex;
    return false;
}

void DSDProtocolDStar::end()
{
    m_active = false;
    m_burst = BurstNone;
}

bool DSDProtocolDStar::pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex)
{
    (void) sampleIndex;

    if (!m_active) {
        return false;
    }

    // Carrier gone for a frame: stop emitting
    m_absent = symbol.m_present ? 0 : m_absent + 1;

    if (m_absent >= m_frameBits)
    {
        m_active = false;
        return false;
    }

    // The end of transmission pattern takes the place of the data field of the last voice frame -
    // any frame of the superframe, not only the one whose data field is the voice sync - and
    // runs 24 symbols past it, which is 24 symbols into the burst that would follow. Noise or
    // voice matches the 48 symbols within 8 errors 1.6e-6 of the time: at the sync, once a
    // superframe, as before; at the other 20 frames within 6 (5e-8, one false end in about
    // 110 hours of voice)
    if ((m_burst == BurstFrame) && (index == m_burstFirst + 23)
        && matchPattern(index, DVSync::DStarEnd, index == m_frameBase + 24 ? 8 : 6))
    {
        if (dsdDebug()) {
            fprintf(stderr, "D-STAR end at symbol %lld\n", index);
        }

        m_active = false;
        m_normalEnd = true;
        m_burst = BurstNone;
        return false;
    }

    return (m_burst != BurstNone) && (index == m_completeAt);
}

void DSDProtocolDStar::getBurstSpan(int64_t& first, int64_t& last) const
{
    first = m_burstFirst;
    last = m_burstLast;
}

void DSDProtocolDStar::processBurst()
{
    m_burstValid = false;

    if (m_burst == BurstHeader)
    {
        processHeader();
        // Frame 0 of the first superframe: voice then the voice sync
        m_frameIndex = 0;
        m_burst = BurstFrame;
        m_burstFirst = m_frameBase - m_frameBits + 1;
        m_burstLast = m_frameBase;
        m_completeAt = m_burstLast;
        return;
    }

    processFrame(m_frameIndex, m_burstFirst);

    if (m_frameIndex == 0)
    {
        // The sync of this superframe has just been received
        m_syncIndex = m_frameBase;
        // Three errors in the 15 symbol voice sync are allowed once the lock is proven (a weak
        // signal). Until then, as noise matches within three errors 2% of the time, it takes a
        // near exact sync or two syncs in a row within three (a weak recording has no near exact
        // sync at all)
        bool within3 = matchPattern(m_frameBase, DVSync::DStarVoice, 3);
        m_proven = m_proven || matchPattern(m_frameBase, DVSync::DStarVoice, 1) || (within3 && m_lastSyncWithin3);
        m_lastSyncWithin3 = within3;
        m_burstValid = within3 && m_proven;
    }

    m_frameIndex = (m_frameIndex + 1) % m_framesPerSuperframe;

    if (m_frameIndex == 0)
    {
        m_burstFirst = m_frameBase + m_superframeBits - m_frameBits + 1;
        m_burstLast = m_frameBase + m_superframeBits;
        m_frameBase += m_superframeBits;
    }
    else
    {
        m_burstFirst = m_frameBase + (m_frameIndex - 1) * m_frameBits + 1;
        m_burstLast = m_burstFirst + m_frameBits - 1;
    }

    m_completeAt = m_burstLast;
}

void DSDProtocolDStar::processHeader()
{
    if (!m_history.has(m_burstFirst) || !m_history.has(m_burstLast)) {
        return;
    }

    float llr[m_headerBits];

    for (int i = 0; i < m_headerBits; i++) {
        llr[i] = m_history.at(m_burstFirst + i).m_llrMsb;
    }

    uint8_t header[m_headerBytes];
    int corrected = DVDStar::headerDecodeSoft(llr, header);

    if (corrected >= 0)
    {
        std::memcpy(m_header, header, m_headerBytes);
        m_headerValid = true;
        publishHeader(m_history.sampleAt(m_burstLast));
        m_burstValid = true;
        m_proven = true;
    }

    if (dsdDebug()) {
        fprintf(stderr, "D-STAR header at %lld..%lld: %s (%d corrected) %s\n", m_burstFirst, m_burstLast, corrected >= 0 ? "ok" : "failed", corrected,
            corrected >= 0 ? getStatusText().c_str() : "");
    }
}

void DSDProtocolDStar::publishHeader(int64_t sample)
{
    if (!m_headerValid || m_headerPublished) {
        return;
    }

    DSDProtocolFrame frame;
    frame.m_sample = sample;
    frame.m_format = "dstar-header";
    frame.m_type = "header";
    frame.m_data.assign(m_header, m_header + m_headerBytes);
    m_protocolFrames.push_back(frame);
    m_headerPublished = true;
}

void DSDProtocolDStar::processFrame(int frameIndex, int64_t first)
{
    if (!m_history.has(first) || !m_history.has(first + m_frameBits - 1)) {
        return;
    }

    int present = 0;

    for (int i = 0; i < m_voiceBits; i++) {
        present += m_history.at(first + i).m_present;
    }

    if (present < m_voiceBits / 2) {
        return;
    }

    // Voice: 72 bits, sent LSB of each byte first
    DSDVoiceFrame frame;
    frame.m_sample = m_history.sampleAt(first + m_voiceBits - 1);
    frame.m_slot = 0;
    frame.m_rate = DSDVoiceFrame::RateAMBE3600x2400;
    frame.m_nbBits = m_voiceBits;

    for (int i = 0; i < m_voiceBits; i++)
    {
        const DSDSoftSymbol& s = m_history.at(first + i);
        int bit = (s.m_dibit >> 1) & 1;
        frame.m_bytes[i / 8] |= bit << (i % 8);
        frame.m_llr[8 * (i / 8) + 7 - (i % 8)] = s.m_llrMsb; // LLR index follows the MSB first numbering of the bytes
    }

    m_frames.push_back(frame);

    DSDProtocolFrame protocolFrame;
    protocolFrame.m_sample = m_history.sampleAt(first + m_frameBits - 1);
    protocolFrame.m_format = "dstar-voice";
    protocolFrame.m_type = "voice";
    protocolFrame.m_sequence = frameIndex;
    protocolFrame.m_data.assign(12, 0);

    for (int i = 0; i < m_frameBits; i++)
    {
        int bit = (m_history.at(first + i).m_dibit >> 1) & 1;
        protocolFrame.m_data[i / 8] |= bit << (i % 8);
    }

    m_protocolFrames.push_back(protocolFrame);

    // Slow data: 24 bits LSB first, scrambled
    if (frameIndex > 0)
    {
        uint8_t bytes[3] = { 0, 0, 0 };

        for (int i = 0; i < m_dataBits; i++)
        {
            int bit = (m_history.at(first + m_voiceBits + i).m_dibit >> 1) & 1;
            bytes[i / 8] |= bit << (i % 8);
        }

        bytes[0] ^= 0x70;
        bytes[1] ^= 0x4F;
        bytes[2] ^= 0x93;
        processSlowData(frameIndex, bytes);
    }
}

void DSDProtocolDStar::processSlowData(int frameIndex, const uint8_t *bytes)
{
    // Pairs of frames form 6 byte blocks: a type byte then 5 data bytes. 0x4n is block n of the
    // 20 character text message, 0x5n is n bytes of the radio header (9 blocks)
    uint8_t *block = m_block;

    if ((frameIndex & 1) == 1)
    {
        std::memcpy(block, bytes, 3);
        m_blockLength = 3;
        return;
    }

    if (m_blockLength != 3) {
        return;
    }

    std::memcpy(block + 3, bytes, 3);
    m_blockLength = 0;
    int type = block[0] >> 4;
    int n = block[0] & 0x0F;

    if ((type == 4) && (n < 4))
    {
        std::memcpy(m_textBlocks[n], block + 1, 5);
        m_textBlocks[n][5] = 0;
        m_textMask |= 1 << n;

        if (m_textMask == 0x0F)
        {
            m_text.clear();

            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 5; j++)
                {
                    char c = m_textBlocks[i][j];
                    m_text += ((c >= 32) && (c < 127)) ? c : ' ';
                }
            }

            m_textMask = 0;
        }
    }
    else if (type == 3)
    {
        // GPS: n bytes of D-PRS or NMEA text
        for (int i = 0; (i < n) && (i < 5); i++) {
            processGpsByte((char) block[1 + i]);
        }
    }
    else if ((type == 5) && (n <= 5))
    {
        // Header blocks arrive in order; the block index is the count so far
        int index = 0;

        while ((index < 9) && (m_slowHeaderMask & (1 << index))) {
            index++;
        }

        if (index < 9)
        {
            std::memcpy(m_slowHeader + 5 * index, block + 1, std::min(n, 41 - 5 * index));
            m_slowHeaderMask |= 1 << index;

            if (m_slowHeaderMask == 0x1FF)
            {
                if (!m_headerValid && DVDStar::headerCRCCheck(m_slowHeader))
                {
                    std::memcpy(m_header, m_slowHeader, m_headerBytes);
                    m_headerValid = true;
                    publishHeader(m_history.sampleAt(m_history.newest()));
                }

                m_slowHeaderMask = 0;
            }
        }
    }
    else
    {
        m_slowHeaderMask = 0;
    }
}

void DSDProtocolDStar::processGpsByte(char c)
{
    if ((c == 13) || (c == 10))
    {
        if (!m_gpsLine.empty()) {
            processGpsLine();
        }

        m_gpsLine.clear();
        return;
    }

    if (m_gpsLine.size() < 200) {
        m_gpsLine += c;
    }
}

bool DSDProtocolDStar::parseCoordinate(const char *text, int degreeDigits, char hemisphere, double& value)
{
    // DDMM.MMMM (latitude, 2 degree digits) or DDDMM.MMMM (longitude, 3)
    double raw = atof(text);
    double degrees = std::floor(raw / 100.0);
    double minutes = raw - 100.0 * degrees;
    (void) degreeDigits;

    if ((minutes < 0.0) || (minutes >= 60.0)) {
        return false;
    }

    value = degrees + minutes / 60.0;

    if ((hemisphere == 'S') || (hemisphere == 'W')) {
        value = -value;
    } else if ((hemisphere != 'N') && (hemisphere != 'E')) {
        return false;
    }

    return true;
}

void DSDProtocolDStar::processGpsLine()
{
    const std::string& line = m_gpsLine;
    double latitude, longitude;

    if ((line.size() > 10) && (line.compare(0, 5, "$$CRC") == 0) && (line[9] == ','))
    {
        // D-PRS: $$CRChhhh,CALL>API,DSTAR*:/hhmmsshDDMM.MMN/DDDMM.MME...<CR><LF>. The CRC covers the text after
        // the comma including the CR LF (D-PRS specification, dsdcc, the gateways); the line terminators were
        // consumed on the way here, so they are put back for the check. A CRC over the bare text is accepted too.
        unsigned int crc = (unsigned int) strtoul(line.substr(5, 4).c_str(), nullptr, 16);
        const std::string payload = line.substr(10);
        const std::string payloadCrLf = payload + "\r\n";

        if ((DVCRC::dstar16((const uint8_t *) payloadCrLf.data(), (int) payloadCrLf.size()) != crc)
            && (DVCRC::dstar16((const uint8_t *) payload.data(), (int) payload.size()) != crc)) {
            return;
        }

        // The payload is an APRS packet in TNC2 form, whatever it carries (a position, a message,
        // an object, telemetry): it is kept for the consumers, and a position parsed from it below
        m_dprs = payload;
        m_dprsRevision++;

        size_t p = payload.find("DSTAR*:/");

        if ((p == std::string::npos) || (payload.size() < p + 33)) {
            return;
        }

        const char *position = payload.c_str() + p + 15; // after the time
        std::string latText(position, 7);
        std::string lonText(position + 9, 8);

        if (parseCoordinate(latText.c_str(), 2, position[7], latitude) && parseCoordinate(lonText.c_str(), 3, position[17], longitude))
        {
            m_latitude = latitude;
            m_longitude = longitude;
            m_positionValid = true;
            m_gpsText = line;
        }

        return;
    }

    // NMEA: $GPGGA,time,lat,N,lon,E,... and $GPRMC,time,status,lat,N,lon,E,...
    bool gga = (line.compare(0, 6, "$GPGGA") == 0) || (line.compare(0, 6, "$GNGGA") == 0);
    bool rmc = (line.compare(0, 6, "$GPRMC") == 0) || (line.compare(0, 6, "$GNRMC") == 0);

    if (!gga && !rmc) {
        return;
    }

    std::vector<std::string> fields;
    size_t start = 0;

    while (start <= line.size())
    {
        size_t comma = line.find(',', start);

        if (comma == std::string::npos) {
            comma = line.size();
        }

        fields.push_back(line.substr(start, comma - start));
        start = comma + 1;
    }

    int latField = gga ? 2 : 3;

    if ((int) fields.size() < latField + 4) {
        return;
    }

    if (rmc && (fields[2] != "A")) {
        return; // no fix
    }

    if (fields[latField].empty() || fields[latField + 1].empty() || fields[latField + 2].empty() || fields[latField + 3].empty()) {
        return;
    }

    if (parseCoordinate(fields[latField].c_str(), 2, fields[latField + 1][0], latitude)
        && parseCoordinate(fields[latField + 2].c_str(), 3, fields[latField + 3][0], longitude))
    {
        m_latitude = latitude;
        m_longitude = longitude;
        m_positionValid = true;
        m_gpsText = line;
    }
}

bool DSDProtocolDStar::getPosition(double& latitude, double& longitude) const
{
    latitude = m_latitude;
    longitude = m_longitude;
    return m_positionValid;
}

std::string DSDProtocolDStar::getStatusText() const
{
    std::string s;

    if (m_headerValid) {
        s = "MY: " + getMyCall() + "/" + getMySuffix() + " UR: " + getUrCall() + " RPT1: " + getRpt1() + " RPT2: " + getRpt2();
    } else {
        s = "no header";
    }

    if (!m_text.empty()) {
        s += " TEXT: " + m_text;
    }

    return s;
}
