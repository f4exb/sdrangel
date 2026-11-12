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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool dsdDebug() { static int d = getenv("DSD_DEBUG") ? 1 : 0; return d != 0; }

#include "dsp/dv/dvbits.h"
#include "dsp/dv/dvdpmr.h"

#include "dsdprotocoldpmr.h"

DSDProtocolDPMR::DSDProtocolDPMR(const DSDSymbolHistory& history) :
    DSDProtocol(history),
    m_active(false),
    m_syncIndex(-1),
    m_frameNumber(0),
    m_burst(BurstNone),
    m_burstFirst(-1),
    m_burstLast(-1),
    m_pendingFirst(-1),
    m_pendingLast(-1),
    m_pendingBurst(BurstNone),
    m_pendingFrame(0),
    m_misses(0),
    m_silent(0),
    m_absent(0),
    m_burstValid(false),
    m_colourCode(-1),
    m_headerValid(false),
    m_calledId(0),
    m_ownId(0),
    m_mode(0),
    m_format(0),
    m_typeText("---"),
    m_cchMode(-1),
    m_slowDataLost(false)
{
}

std::vector<DVSync::Pattern> DSDProtocolDPMR::getSearchPatterns() const
{
    return { DVSync::DPMRFS1, DVSync::DPMRFS2 };
}

void DSDProtocolDPMR::start(DVSync::Pattern pattern, int64_t syncIndex)
{
    m_active = true;
    m_misses = 0;
    m_silent = 0;
    m_absent = 0;
    m_colourCode = -1;
    m_headerValid = false;
    m_typeText = "---";
    m_syncIndex = syncIndex;
    m_cchMode = -1;
    m_slowData.clear();
    m_slowDataLost = false;

    if (pattern == DVSync::DPMRFS1)
    {
        m_burst = BurstHeader;
        m_burstFirst = syncIndex + 1;
        m_burstLast = syncIndex + m_headerSymbols;
        m_frameNumber = 0;
    }
    else
    {
        // Joined at FS2: the frame it starts is even
        m_burst = BurstPayload;
        m_burstFirst = syncIndex - m_fs2Symbols + 1;
        m_burstLast = m_burstFirst + m_frameSymbols - 1;
        m_frameNumber = 0;
    }
}

void DSDProtocolDPMR::resync(DVSync::Pattern pattern, int64_t syncIndex)
{
    m_misses = 0;
    m_silent = 0;
    m_syncIndex = syncIndex;

    if (pattern == DVSync::DPMRFS3)
    {
        m_burst = BurstEnd;
        m_burstFirst = syncIndex - m_fs2Symbols + 1;
        m_burstLast = syncIndex + m_endSymbols;
        return;
    }

    if (pattern == DVSync::DPMRFS1)
    {
        start(pattern, syncIndex);
        return;
    }

    // FS2: realign the payload frame on it
    m_burst = BurstPayload;
    m_burstFirst = syncIndex - m_fs2Symbols + 1;
    m_burstLast = m_burstFirst + m_frameSymbols - 1;

    if (m_frameNumber & 1) {
        m_frameNumber = 0;
    }
}

bool DSDProtocolDPMR::acceptUnexpectedSync(DVSync::Pattern pattern, int64_t syncIndex) const
{
    // Radios repeat the header frame (FS1 every 192 symbols) before the payload, and a new call
    // may start with a header at any time: a strong FS1 after the current burst began is one
    return m_active && (pattern == DVSync::DPMRFS1) && (syncIndex - m_fs1Symbols + 1 >= m_burstFirst + m_fs1Symbols);
}

bool DSDProtocolDPMR::getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const
{
    if (!m_active) {
        return false;
    }

    if (m_burst == BurstHeader)
    {
        // First payload frame follows the header
        syncIndex = m_burstLast + m_fs2Symbols;
    }
    else
    {
        // FS2 of the next even frame
        int64_t fs2 = m_burstFirst + m_fs2Symbols - 1;
        syncIndex = (m_frameNumber & 1) ? fs2 + m_frameSymbols : fs2 + 2 * m_frameSymbols;

        if ((m_frameNumber & 1) == 0 && (m_syncIndex < fs2)) {
            syncIndex = fs2; // the sync of the current even frame has not been seen yet
        }
    }

    patterns = { DVSync::DPMRFS2, DVSync::DPMRFS3 };
    return true;
}

void DSDProtocolDPMR::getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const
{
    first = syncIndex - m_fs2Symbols + 1;
    last = syncIndex;
}

bool DSDProtocolDPMR::confirmWithoutSync(int64_t syncIndex, bool present)
{
    if (!present)
    {
        if (++m_silent >= 3) {
            m_active = false;
        }

        m_syncIndex = syncIndex;
        return true;
    }

    // FS2 continues the call; FS3 (searched here as hard symbols: it is not a lock pattern) ends it
    if (matchPattern(syncIndex, DVSync::DPMRFS2, 3))
    {
        m_misses = 0;
        m_syncIndex = syncIndex;
        return true;
    }

    if (matchPattern(syncIndex, DVSync::DPMRFS3, 2))
    {
        resync(DVSync::DPMRFS3, syncIndex);
        return true;
    }

    if (++m_misses >= 3) {
        m_active = false;
    }

    m_syncIndex = syncIndex; // carry on; the next sync decides
    return false;
}

bool DSDProtocolDPMR::matchPattern(int64_t lastIndex, DVSync::Pattern pattern, int maxErrors) const
{
    const DVSync::Info& info = DVSync::get(pattern);
    int64_t first = lastIndex - info.m_length + 1;

    if (!m_history.has(first) || !m_history.has(lastIndex)) {
        return false;
    }

    int errors = 0;

    for (int i = 0; i < info.m_length; i++) {
        errors += m_history.at(first + i).m_dibit != DVSync::levelToDibit(info.m_levels[i]);
    }

    return errors <= maxErrors;
}

void DSDProtocolDPMR::end()
{
    finishSlowData(false);
    m_active = false;
    m_burst = BurstNone;
}

void DSDProtocolDPMR::slowDataByte(uint8_t byte, bool last)
{
    // Between messages the field is zeros (the standard marks a message's end, not its start)
    if (m_slowData.empty() && (byte == 0) && !last) {
        return;
    }

    m_slowData.push_back(byte);

    if (last || ((int) m_slowData.size() >= m_maxSlowDataBytes)) {
        finishSlowData(last);
    }
}

void DSDProtocolDPMR::finishSlowData(bool complete)
{
    if (m_slowData.empty()) {
        return;
    }

    // Latin-1 text when every byte is printable (a lost CCH's bytes are shown as ?), else hexadecimal
    bool text = true;

    for (uint8_t c : m_slowData) {
        text = text && (((c >= 0x20) && (c < 0x7F)) || (c >= 0xA0) || (c == '\t'));
    }

    std::string s;

    for (uint8_t c : m_slowData)
    {
        if (!text)
        {
            char hex[4];
            snprintf(hex, sizeof(hex), s.empty() ? "%02X" : " %02X", c);
            s += hex;
        }
        else if (c < 0x80)
        {
            s += (char) c;
        }
        else
        {
            s += (char) (0xC0 | (c >> 6));
            s += (char) (0x80 | (c & 0x3F));
        }
    }

    m_textMessage.m_valid = true;
    m_textMessage.m_source = m_ownId;
    m_textMessage.m_destination = m_calledId;
    m_textMessage.m_isText = text;
    m_textMessage.m_complete = complete && !m_slowDataLost;
    m_textMessage.m_text = s;
    m_textMessage.m_revision++;
    m_slowData.clear();
    m_slowDataLost = false;

    if (dsdDebug()) {
        fprintf(stderr, "dPMR slow data message%s: %s\n", m_textMessage.m_complete ? "" : " (cut short)", s.c_str());
    }
}

bool DSDProtocolDPMR::pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex)
{
    (void) sampleIndex;

    if (!m_active) {
        return false;
    }

    m_absent = symbol.m_present ? 0 : m_absent + 1;

    if (m_absent >= m_frameSymbols)
    {
        m_active = false;
        return false;
    }

    if ((m_burst != BurstNone) && (index == m_burstLast))
    {
        m_pendingFirst = m_burstFirst;
        m_pendingLast = m_burstLast;
        m_pendingBurst = m_burst;
        m_pendingFrame = m_frameNumber;
        // The next burst by default: a payload frame following this one
        m_burst = BurstPayload;
        m_burstFirst = m_burstLast + 1;
        m_burstLast = m_burstFirst + m_frameSymbols - 1;
        m_frameNumber = (m_pendingBurst == BurstPayload) ? (m_pendingFrame + 1) & 3 : 0;
        return true;
    }

    return false;
}

void DSDProtocolDPMR::getBurstSpan(int64_t& first, int64_t& last) const
{
    first = m_pendingFirst;
    last = m_pendingLast;
}

void DSDProtocolDPMR::processBurst()
{
    m_burstValid = false;

    switch (m_pendingBurst)
    {
    case BurstHeader:
        processHeader(m_pendingFirst);
        break;
    case BurstPayload:
        processPayload(m_pendingFirst, m_pendingFrame);
        break;
    case BurstEnd:
        processEnd(m_pendingFirst);
        m_active = false;
        m_burst = BurstNone;
        break;
    default:
        break;
    }
}

bool DSDProtocolDPMR::checkColourCode(int64_t lastIndex) const
{
    int64_t first = lastIndex - m_ccSymbols + 1;

    if (!m_history.has(first) || !m_history.has(lastIndex)) {
        return false;
    }

    uint8_t bits[24];
    m_history.bits(first, m_ccSymbols, bits);
    int errors;
    int cc = DVDPMR::colourCodeMatch(bits, errors);
    return (cc >= 0) && ((m_colourCode < 0) || (cc == m_colourCode));
}

void DSDProtocolDPMR::parseHeader(const uint8_t *hi72)
{
    // HT(4) called ID (24) own ID (24) M(3) V(2) F(2) RES(2) CI(11)
    m_calledId = (uint32_t) DVBits::getBits(hi72, 4, 24);
    m_ownId = (uint32_t) DVBits::getBits(hi72, 28, 24);
    m_mode = (int) DVBits::getBits(hi72, 52, 3);
    m_format = (int) DVBits::getBits(hi72, 57, 2);
    m_headerValid = true;
    m_cchMode = m_mode;     // until a CCH says otherwise
}

void DSDProtocolDPMR::processHeader(int64_t first)
{
    if (!m_history.has(first) || !m_history.has(first + m_headerSymbols - 1)) {
        return;
    }

    uint8_t coded[120];
    uint8_t hi[72];
    int corrected = -1;

    for (int copy = 0; (copy < 2) && (corrected < 0); copy++)
    {
        m_history.bits(first + copy * (m_hiSymbols + m_ccSymbols), m_hiSymbols, coded);
        corrected = DVDPMR::headerInfoDecode(coded, hi);
    }

    if (corrected >= 0)
    {
        parseHeader(hi);
        m_typeText = "HEADER";
        m_burstValid = true;
    }

    uint8_t ccBits[24];
    m_history.bits(first + m_hiSymbols, m_ccSymbols, ccBits);
    int errors;
    int cc = DVDPMR::colourCodeMatch(ccBits, errors);

    if (cc >= 0) {
        m_colourCode = cc;
    }

    if (dsdDebug()) {
        fprintf(stderr, "dPMR header at %lld: HI %s CC %d called %u own %u\n", (long long) first, corrected >= 0 ? "ok" : "failed", cc, m_calledId, m_ownId);
    }
}

void DSDProtocolDPMR::processPayload(int64_t first, int frameNumber)
{
    if (!m_history.has(first) || !m_history.has(first + m_frameSymbols - 1)) {
        return;
    }

    if (frameNumber & 1)
    {
        uint8_t ccBits[24];
        m_history.bits(first, m_ccSymbols, ccBits);
        int errors;
        int cc = DVDPMR::colourCodeMatch(ccBits, errors);

        if ((cc >= 0) && (m_colourCode < 0)) {
            m_colourCode = cc;
        }
    }

    // CCH: frame number, half of an identity, mode and format, and the slow data field
    uint8_t coded[72];
    uint8_t cch[41];
    m_history.bits(first + m_fs2Symbols, m_cchSymbols, coded);
    bool voice = true;

    if (DVDPMR::cchDecode(coded, cch) >= 0)
    {
        int fn = (cch[0] << 1) | cch[1];

        if (fn != frameNumber)
        {
            // Realign the superframe count on the CCH
            m_frameNumber = (fn + 1) & 3;
        }

        if (!m_headerValid)
        {
            m_mode = (int) DVBits::getBits(cch, 14, 3);
            m_format = (int) DVBits::getBits(cch, 19, 2);
        }

        m_cchMode = (int) DVBits::getBits(cch, 14, 3);
        m_typeText = "VOICE";
        m_burstValid = true;

        if (m_cchMode == 1)
        {
            // Voice + slow data (5.9.1): CONT1 byte1 CONT2 byte2, a set CONT marking a message's last byte
            slowDataByte((uint8_t) DVBits::getBits(cch, 24, 8), cch[23] != 0);
            slowDataByte((uint8_t) DVBits::getBits(cch, 33, 8), cch[32] != 0);
        }
        else
        {
            finishSlowData(false);
        }

        // The data modes (5.9.2): data type 1 and 2 carry no voice, and voice + appended data
        // marks each frame's traffic channels as voice (data position 00) or data (11)
        if ((m_cchMode == 2) || (m_cchMode == 3) || (m_cchMode == 4)) {
            voice = false;
        } else if (m_cchMode == 5) {
            voice = DVBits::getBits(cch, 28, 2) != 3;
        }

        if (m_cchMode != 0) {
            m_typeText = voice ? "VOICE+DATA" : "DATA";
        }
    }
    else
    {
        // A lost CCH in a slow data message: its two bytes unknown
        if (!m_slowData.empty())
        {
            m_slowData.push_back('?');
            m_slowData.push_back('?');
            m_slowDataLost = true;
        }

        voice = (m_cchMode != 2) && (m_cchMode != 3) && (m_cchMode != 4);
    }

    if (dsdDebug()) {
        fprintf(stderr, "dPMR payload frame %d at %lld\n", frameNumber, (long long) first);
    }

    // Four traffic channels
    int64_t tch = first + m_fs2Symbols + m_cchSymbols;

    for (int i = 0; voice && (i < 4); i++)
    {
        int64_t f = tch + i * m_tchSymbols;
        int present = 0;

        for (int k = 0; k < m_tchSymbols; k++) {
            present += m_history.at(f + k).m_present;
        }

        if (present < m_tchSymbols / 2) {
            continue;
        }

        DSDVoiceFrame frame;
        frame.m_sample = m_history.sampleAt(f + m_tchSymbols - 1);
        frame.m_slot = 0;
        frame.m_rate = DSDVoiceFrame::RateAMBE3600x2450;
        frame.m_nbBits = 72;
        uint8_t bits[72];
        m_history.bits(f, m_tchSymbols, bits);
        m_history.llrs(f, m_tchSymbols, frame.m_llr);
        DVBits::bitsToBytesMSB(bits, 72, frame.m_bytes);
        m_frames.push_back(frame);
    }
}

void DSDProtocolDPMR::processEnd(int64_t first)
{
    if (!m_history.has(first) || !m_history.has(first + m_fs2Symbols + m_endSymbols - 1)) {
        return;
    }

    uint8_t coded[72];
    uint8_t endInfo[17];
    m_history.bits(first + m_fs2Symbols, m_endSymbols, coded);
    int corrected = DVDPMR::endDecode(coded, endInfo);
    m_typeText = "END";
    finishSlowData(false);
    m_burstValid = corrected >= 0;

    if (dsdDebug()) {
        fprintf(stderr, "dPMR end at %lld: %s\n", (long long) first, corrected >= 0 ? "ok" : "failed");
    }
}

bool DSDProtocolDPMR::getHeader(uint32_t& calledId, uint32_t& ownId, int& mode, int& format) const
{
    calledId = m_calledId;
    ownId = m_ownId;
    mode = m_mode;
    format = m_format;
    return m_headerValid;
}

std::string DSDProtocolDPMR::getStatusText() const
{
    char text[96];

    if (m_headerValid) {
        snprintf(text, sizeof(text), "CC %d %s %u > %u%s", m_colourCode, m_typeText.c_str(), m_ownId, m_calledId, m_format == 3 ? " (all call)" : "");
    } else {
        snprintf(text, sizeof(text), "CC %d %s", m_colourCode, m_typeText.c_str());
    }

    return text;
}
