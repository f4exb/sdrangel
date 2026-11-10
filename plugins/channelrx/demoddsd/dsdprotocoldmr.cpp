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
#include <vector>

static bool dsdDebug() { static int d = getenv("DSD_DEBUG") ? 1 : 0; return d != 0; }

#include "dsp/dv/dvbits.h"
#include "dsp/dv/dvcrc.h"
#include "dsp/dv/dvdmr.h"

#include "dsdpackettext.h"
#include "dsdprotocoldmr.h"

namespace {

void appendUTF8(std::string& text, uint32_t codePoint)
{
    if ((codePoint < 0x20) || (codePoint == 0x7f)) {
        codePoint = ' '; // Keep status/event rows on one printable line.
    } else if ((codePoint > 0x10ffff) || ((codePoint >= 0xd800) && (codePoint <= 0xdfff))) {
        codePoint = 0xfffd;
    }

    if (codePoint < 0x80) {
        text.push_back((char) codePoint);
    } else if (codePoint < 0x800) {
        text.push_back((char) (0xc0 | (codePoint >> 6)));
        text.push_back((char) (0x80 | (codePoint & 0x3f)));
    } else if (codePoint < 0x10000) {
        text.push_back((char) (0xe0 | (codePoint >> 12)));
        text.push_back((char) (0x80 | ((codePoint >> 6) & 0x3f)));
        text.push_back((char) (0x80 | (codePoint & 0x3f)));
    } else {
        text.push_back((char) (0xf0 | (codePoint >> 18)));
        text.push_back((char) (0x80 | ((codePoint >> 12) & 0x3f)));
        text.push_back((char) (0x80 | ((codePoint >> 6) & 0x3f)));
        text.push_back((char) (0x80 | (codePoint & 0x3f)));
    }
}

uint8_t bitsToByte(const uint8_t *bits)
{
    uint8_t value = 0;

    for (int i = 0; i < 8; i++) {
        value = (uint8_t) ((value << 1) | (bits[i] & 1));
    }

    return value;
}

bool decodeUTF8(const std::vector<uint8_t>& bytes, int wantedCharacters, std::string& text)
{
    size_t i = 0;
    int characters = 0;

    while ((i < bytes.size()) && (characters < wantedCharacters))
    {
        uint8_t first = bytes[i];
        uint32_t codePoint;
        int length;

        if (first < 0x80) {
            codePoint = first; length = 1;
        } else if ((first & 0xe0) == 0xc0) {
            codePoint = first & 0x1f; length = 2;
        } else if ((first & 0xf0) == 0xe0) {
            codePoint = first & 0x0f; length = 3;
        } else if ((first & 0xf8) == 0xf0) {
            codePoint = first & 0x07; length = 4;
        } else {
            appendUTF8(text, 0xfffd); i++; characters++; continue;
        }

        if (i + length > bytes.size()) {
            return false; // The remainder may be in the next Talker Alias block.
        }

        bool valid = true;

        for (int j = 1; j < length; j++)
        {
            if ((bytes[i + j] & 0xc0) != 0x80) {
                valid = false;
                break;
            }

            codePoint = (codePoint << 6) | (bytes[i + j] & 0x3f);
        }

        const uint32_t minimum = length == 2 ? 0x80 : length == 3 ? 0x800 : length == 4 ? 0x10000 : 0;

        if (!valid || (codePoint < minimum) || (codePoint > 0x10ffff)
            || ((codePoint >= 0xd800) && (codePoint <= 0xdfff)))
        {
            appendUTF8(text, 0xfffd);
            i++;
        }
        else
        {
            appendUTF8(text, codePoint);
            i += length;
        }

        characters++;
    }

    return characters == wantedCharacters;
}

uint32_t dmrPacketCRC32(const uint8_t *bytes, int nbBytes)
{
    // ETSI B.3.9 presents the message as 16-bit words, least-significant octet
    // first within each word, before the normal MSB-first CRC-32 polynomial.
    std::vector<uint8_t> bits(nbBytes * 8);
    int output = 0;

    for (int i = 0; i < nbBytes; i += 2)
    {
        const int indices[2] = {i + 1, i};

        for (int index : indices) {
            for (int bit = 7; bit >= 0; bit--) {
                bits[output++] = (bytes[index] >> bit) & 1;
            }
        }
    }

    return DVCRC::compute(bits.data(), output, 32, 0x04c11db7, 0, 0);
}

} // namespace

DSDProtocolDMR::DSDProtocolDMR(const DSDSymbolHistory& history) :
    DSDProtocol(history),
    m_station(StationUnknown),
    m_period(144),
    m_centre(-1),
    m_nextCentre(-1),
    m_pendingCentre(-1),
    m_kind(BurstNone),
    m_active(false),
    m_colorCode(-1),
    m_currentSlot(0),
    m_misses(0),
    m_metadataRevision(0)
{
}

std::vector<DVSync::Pattern> DSDProtocolDMR::getSearchPatterns() const
{
    return {
        DVSync::DMRDataBS, DVSync::DMRVoiceBS, DVSync::DMRDataMS, DVSync::DMRVoiceMS,
        DVSync::DMRDataTS1, DVSync::DMRVoiceTS1, DVSync::DMRDataTS2, DVSync::DMRVoiceTS2
    };
}

void DSDProtocolDMR::classify(DVSync::Pattern pattern)
{
    switch (pattern)
    {
    case DVSync::DMRDataBS:
        m_station = StationBase; m_kind = BurstData; break;
    case DVSync::DMRVoiceBS:
        m_station = StationBase; m_kind = BurstVoiceSync; break;
    case DVSync::DMRDataMS:
        m_station = StationMobile; m_kind = BurstData; break;
    case DVSync::DMRVoiceMS:
        m_station = StationMobile; m_kind = BurstVoiceSync; break;
    case DVSync::DMRDataTS1:
    case DVSync::DMRDataTS2:
        m_station = StationDirect; m_kind = BurstData; break;
    case DVSync::DMRVoiceTS1:
    case DVSync::DMRVoiceTS2:
        m_station = StationDirect; m_kind = BurstVoiceSync; break;
    default:
        m_kind = BurstNone; break;
    }

    m_period = (m_station == StationBase) ? m_burstDibits : 2 * m_burstDibits;
    m_currentSlot = (pattern == DVSync::DMRDataTS2 || pattern == DVSync::DMRVoiceTS2) ? 1 : 0;
}

void DSDProtocolDMR::start(DVSync::Pattern pattern, int64_t syncIndex)
{
    m_active = true;
    m_misses = 0;
    m_colorCode = -1;
    m_proven = false;
    m_strictEmbs = 0;
    m_slotKnown = false;
    m_lastBurstCentre = 0;
    m_lastBurstSlot = 0;
    m_slotDisagreements = 0;
    m_slots[0] = SlotState();
    m_slots[1] = SlotState();
    m_protocolFrames.clear();
    classify(pattern);
    m_centre = syncIndex;
    m_nextCentre = -1;
}

void DSDProtocolDMR::resync(DVSync::Pattern pattern, int64_t syncIndex)
{
    StationType previous = m_station;
    classify(pattern);

    if (previous != m_station) {
        m_period = (m_station == StationBase) ? m_burstDibits : 2 * m_burstDibits;
    }

    m_centre = syncIndex;
    m_nextCentre = -1;
    m_misses = 0;
}

void DSDProtocolDMR::getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const
{
    first = syncIndex - m_syncEndOffset;
    last = syncIndex;
}

bool DSDProtocolDMR::confirmWithoutSync(int64_t syncIndex, bool present)
{
    if (!present)
    {
        // A silent slot: nothing to decode, expect the next burst
        m_nextCentre = syncIndex + m_period;
        return true;
    }

    int colorCode, lcss;
    bool pi;

    if (checkEmb(syncIndex, colorCode, lcss, pi) && ((m_colorCode < 0) || (colorCode == m_colorCode)))
    {
        // Unproven, a near exact EMB still passes noise 3% of the time: it takes two in a row with
        // the same colour code (the first teaches it) before the lock is proven
        m_colorCode = colorCode;
        m_proven = m_proven || (++m_strictEmbs >= 2);
        m_kind = BurstVoiceEmb;
        m_centre = syncIndex;
        m_nextCentre = -1;
        m_misses = 0;
        return true;
    }

    m_misses++;
    m_nextCentre = syncIndex + m_period;
    return false;
}

bool DSDProtocolDMR::getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const
{
    if (!m_active) {
        return false;
    }

    syncIndex = (m_nextCentre >= 0) ? m_nextCentre : m_centre + m_period;
    patterns = getSearchPatterns();
    return true;
}

void DSDProtocolDMR::end()
{
    m_active = false;
    m_kind = BurstNone;
}

bool DSDProtocolDMR::pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex)
{
    (void) symbol;
    (void) sampleIndex;

    if (!m_active || (m_kind == BurstNone)) {
        return false;
    }

    if (index == m_centre + (m_burstDibits - 1 - m_syncEndOffset))
    {
        m_pendingCentre = m_centre;
        return true;
    }

    return false;
}

void DSDProtocolDMR::getBurstSpan(int64_t& first, int64_t& last) const
{
    first = m_pendingCentre - m_syncEndOffset;
    last = m_pendingCentre + (m_burstDibits - 1 - m_syncEndOffset);
}

void DSDProtocolDMR::processBurst()
{
    decodeBurst(m_pendingCentre);
    m_nextCentre = m_pendingCentre + m_period;
    m_kind = BurstNone;
}

bool DSDProtocolDMR::checkEmb(int64_t centre, int& colorCode, int& lcss, bool& pi) const
{
    // EMB: 8 bits at dibits 66..69 and 8 bits at 86..89 of the burst (centre is dibit 89)
    if (!m_history.has(centre - 23)) {
        return false;
    }

    // Maximum likelihood decoding of the QR(16,7,6) code; the hard decoder corrects 2 errors,
    // the soft decision is trusted further when it gives the colour code already known
    float llr[16];
    m_history.llrs(centre - 23, 4, llr);
    m_history.llrs(centre - 3, 4, llr + 8);
    int differences = DVDMR::embDecodeSoft(llr, colorCode, pi, lcss);
    // Until the lock has proved itself the EMB must be all but exact: nearly every 16 bit word is
    // within 3 of one of the 128 codewords, and a voice sync matched by chance in noise was
    // confirmed by its first EMB (and played) before this
    bool ok = (m_proven && !isDoubtful())
        ? ((differences <= 3) || ((differences <= 5) && (m_colorCode >= 0) && (colorCode == m_colorCode)))
        : ((differences <= 1) && ((m_colorCode < 0) || (colorCode == m_colorCode)));

    if (dsdDebug() && !ok) {
        fprintf(stderr, "DMR EMB at %lld failed: %d differences CC %d (known %d) LCSS %d\n", centre, differences, colorCode, m_colorCode, lcss);
    }

    return ok;
}

void DSDProtocolDMR::parseLC(const uint8_t *lc, SlotState& slot)
{
    // Full LC: PF/reserved (1 bit), FLCO (6), FID (8), service options (8), destination (24), source (24)
    int flco = lc[0] & 0x3F;

    if ((flco == 0) || (flco == 3)) // group voice channel user, unit to unit voice channel user
    {
        uint8_t serviceOptions = lc[2];
        slot.m_linkControl.m_valid = true;
        slot.m_linkControl.m_group = flco == 0;
        slot.m_linkControl.m_emergency = (serviceOptions & 0x80) != 0;
        slot.m_linkControl.m_privacy = (serviceOptions & 0x40) != 0;
        slot.m_linkControl.m_broadcast = (serviceOptions & 0x08) != 0;
        slot.m_linkControl.m_openVoiceCallMode = (serviceOptions & 0x04) != 0;
        slot.m_linkControl.m_priority = serviceOptions & 0x03;
        slot.m_linkControl.m_featureSetId = lc[1];
        slot.m_linkControl.m_destination = ((uint32_t) lc[3] << 16) | ((uint32_t) lc[4] << 8) | lc[5];
        slot.m_linkControl.m_source = ((uint32_t) lc[6] << 16) | ((uint32_t) lc[7] << 8) | lc[8];
    }
    else if ((lc[1] == 0) && (flco >= 4) && (flco <= 7))
    {
        parseTalkerAlias(lc, flco, slot);
    }
    else if ((lc[1] == 0) && (flco == 8) && ((lc[2] & 0xf0) == 0))
    {
        uint32_t longitudeBits = ((uint32_t) (lc[2] & 1) << 24) | ((uint32_t) lc[3] << 16)
            | ((uint32_t) lc[4] << 8) | lc[5];
        uint32_t latitudeBits = ((uint32_t) lc[6] << 16) | ((uint32_t) lc[7] << 8) | lc[8];
        int32_t longitude = (longitudeBits & 0x01000000) ? (int32_t) (longitudeBits | 0xfe000000) : (int32_t) longitudeBits;
        int32_t latitude = (latitudeBits & 0x00800000) ? (int32_t) (latitudeBits | 0xff000000) : (int32_t) latitudeBits;
        GPSInfo gps;
        gps.m_valid = true;
        gps.m_positionError = (lc[2] >> 1) & 7;
        gps.m_longitude = longitude * (360.0 / 33554432.0);
        gps.m_latitude = latitude * (180.0 / 16777216.0);

        if (!slot.m_gpsInfo.m_valid || (gps.m_positionError != slot.m_gpsInfo.m_positionError)
            || (gps.m_longitude != slot.m_gpsInfo.m_longitude) || (gps.m_latitude != slot.m_gpsInfo.m_latitude))
        {
            gps.m_revision = ++m_metadataRevision;
            slot.m_gpsInfo = gps;
        }
    }
}

void DSDProtocolDMR::parseTalkerAlias(const uint8_t *lc, int flco, SlotState& slot)
{
    SlotState::TalkerAliasAssembly& assembly = slot.m_talkerAliasAssembly;

    if (flco == 4)
    {
        assembly = SlotState::TalkerAliasAssembly();
        assembly.m_active = true;
        assembly.m_format = (lc[2] >> 6) & 3;
        assembly.m_length = (lc[2] >> 1) & 0x1f;
        assembly.m_receivedMask = 1;
        int output = 0;

        if (assembly.m_format == 0) {
            assembly.m_bits[output++] = lc[2] & 1;
        }

        for (int byte = 3; byte <= 8; byte++) {
            for (int bit = 7; bit >= 0; bit--) {
                assembly.m_bits[output++] = (lc[byte] >> bit) & 1;
            }
        }
    }
    else
    {
        if (!assembly.m_active) {
            return;
        }

        int block = flco - 5;
        int output = (assembly.m_format == 0 ? 49 : 48) + block * 56;

        for (int byte = 2; byte <= 8; byte++) {
            for (int bit = 7; bit >= 0; bit--) {
                assembly.m_bits[output++] = (lc[byte] >> bit) & 1;
            }
        }

        assembly.m_receivedMask |= 1U << (block + 1);
    }

    publishTalkerAlias(slot);
}

void DSDProtocolDMR::publishTalkerAlias(SlotState& slot)
{
    const SlotState::TalkerAliasAssembly& assembly = slot.m_talkerAliasAssembly;

    if (!assembly.m_active || (assembly.m_length <= 0)) {
        return;
    }

    int availableBits = assembly.m_format == 0 ? 49 : 48;

    for (int block = 0; block < 3; block++)
    {
        if ((assembly.m_receivedMask & (1U << (block + 1))) == 0) {
            break;
        }

        availableBits += 56;
    }

    std::string text;
    bool complete = false;

    if (assembly.m_format == 0)
    {
        if (availableBits >= assembly.m_length * 7)
        {
            for (int character = 0; character < assembly.m_length; character++)
            {
                uint32_t value = 0;

                for (int bit = 0; bit < 7; bit++) {
                    value = (value << 1) | assembly.m_bits[character * 7 + bit];
                }

                appendUTF8(text, value);
            }

            complete = true;
        }
    }
    else
    {
        std::vector<uint8_t> bytes;

        for (int bit = 0; bit + 7 < availableBits; bit += 8) {
            bytes.push_back(bitsToByte(assembly.m_bits + bit));
        }

        if (assembly.m_format == 1)
        {
            if ((int) bytes.size() >= assembly.m_length)
            {
                for (int i = 0; i < assembly.m_length; i++) {
                    appendUTF8(text, bytes[i]);
                }

                complete = true;
            }
        }
        else if (assembly.m_format == 2)
        {
            complete = decodeUTF8(bytes, assembly.m_length, text);
        }
        else
        {
            size_t i = 0;
            int characters = 0;

            while ((i + 1 < bytes.size()) && (characters < assembly.m_length))
            {
                uint32_t codePoint = ((uint32_t) bytes[i] << 8) | bytes[i + 1];
                i += 2;

                if ((codePoint >= 0xd800) && (codePoint <= 0xdbff))
                {
                    if (i + 1 >= bytes.size()) {
                        break;
                    }

                    uint32_t low = ((uint32_t) bytes[i] << 8) | bytes[i + 1];

                    if ((low < 0xdc00) || (low > 0xdfff)) {
                        codePoint = 0xfffd;
                    } else {
                        codePoint = 0x10000 + ((codePoint - 0xd800) << 10) + (low - 0xdc00);
                        i += 2;
                    }
                }

                appendUTF8(text, codePoint);
                characters++;
            }

            complete = characters == assembly.m_length;
        }
    }

    if (complete && (!slot.m_talkerAlias.m_valid || (slot.m_talkerAlias.m_format != assembly.m_format)
        || (slot.m_talkerAlias.m_length != assembly.m_length) || (slot.m_talkerAlias.m_text != text)))
    {
        slot.m_talkerAlias.m_valid = true;
        slot.m_talkerAlias.m_format = assembly.m_format;
        slot.m_talkerAlias.m_length = assembly.m_length;
        slot.m_talkerAlias.m_text = text;
        slot.m_talkerAlias.m_revision = ++m_metadataRevision;
    }
}

void DSDProtocolDMR::decodeBurst(int64_t centre)
{
    int64_t start = centre - m_syncEndOffset;

    if (dsdDebug())
    {
        fprintf(stderr, "DMR burst: centre %lld kind %d station %d history %lld..%lld reliability", centre, (int) m_kind, (int) m_station, start, m_history.newest());

        for (int i = 10; i < 18 && m_history.has(start + i); i++) {
            fprintf(stderr, " %d:%.2f", i, m_history.at(start + i).m_reliability);
        }

        fprintf(stderr, " ...");

        for (int i = 138; i < 144 && m_history.has(start + i); i++) {
            fprintf(stderr, " %d:%.2f", i, m_history.at(start + i).m_reliability);
        }

        fprintf(stderr, "%s", "\n");
    }

    if (!m_history.has(start) || !m_history.has(centre + 54)) {
        return;
    }

    int slot = m_currentSlot;

    if (m_station == StationBase)
    {
        // The slot is in the CACH, whose Hamming(7,4) corrects one error and turns two or three
        // into a valid but wrong TC: at the edge of coverage a burst's voice then went to the
        // other slot's audio. A base station's slots alternate strictly every burst, so once a
        // slot has been read the alternation gives the others, and a CACH that contradicts it is
        // taken as the error it almost always is: two in a row re-establish the phase
        uint8_t cach[24];
        m_history.bits(start, 12, cach);
        bool at;
        int tc, lcss;
        uint8_t payload[17];
        bool cachOk = DVDMR::cachDecode(cach, at, tc, lcss, payload) >= 0;
        // (the bursts since the last, to the nearest: a resync a dibit or two early made 143 of 144
        // no burst at all, and the burst went to the other slot)
        const int64_t bursts = (centre - m_lastBurstCentre + m_burstDibits / 2) / m_burstDibits;
        int expected = m_slotKnown ? (m_lastBurstSlot ^ (int) (bursts & 1)) : -1;

        if (expected < 0)
        {
            if (cachOk)
            {
                slot = tc & 1;
                m_slotKnown = true;
            }
        }
        else if (cachOk && ((tc & 1) != expected))
        {
            slot = (++m_slotDisagreements >= 2) ? (tc & 1) : expected;
        }
        else
        {
            slot = expected;
            m_slotDisagreements = 0;
        }

        m_lastBurstCentre = centre;
        m_lastBurstSlot = slot;
    }

    if (m_kind == BurstData) {
        processData(start, slot);
    } else {
        processVoice(start, slot);
    }
}

void DSDProtocolDMR::processData(int64_t start, int slot)
{
    uint8_t slotType[20];
    m_history.bits(start + 61, 5, slotType);
    m_history.bits(start + 90, 5, slotType + 10);
    int colorCode, dataType;

    // The slot type's Golay(20,8) corrects three errors, which a third of random words allow:
    // near exact until the lock is proven
    int corrected = DVDMR::slotTypeDecode(slotType, colorCode, dataType);

    if ((corrected < 0) || (!m_proven && (corrected > 1))) {
        return;
    }

    m_colorCode = colorCode;
    m_proven = true;
    SlotState& s = m_slots[slot];
    s.m_dataType = dataType;
    s.m_voice = false;
    s.m_embeddedFragments = 0;
    static const char *const names[12] = { "PIH", "VLC", "TLC", "CSB", "MBH", "MBC", "DAH", "D12", "D34", "IDL", "DB1", "USB" };
    s.m_typeText = (dataType >= 0 && dataType < 12) ? names[dataType] : "UNK";
    emitProtocolBurst(start, slot, "data-" + std::to_string(dataType), 0);

    if ((dataType == DVDMR::DataVoiceLCHeader) || (dataType == DVDMR::DataIdle))
    {
        // do not attribute a new or later call to the preceding call: its link control, nor a talker
        // alias half assembled (the next call's blocks were joined to it and shown as its alias)
        s.m_linkControl = LinkControl();
        s.m_talkerAliasAssembly = SlotState::TalkerAliasAssembly();
    }

    if ((dataType != DVDMR::DataHeader) && (dataType != DVDMR::DataRate12)
        && (dataType != DVDMR::DataIdle))
    {
        s.m_udtAssembly = SlotState::UDTAssembly();
        s.m_packetAssembly = SlotState::PacketAssembly();
    }

    if ((dataType == DVDMR::DataVoiceLCHeader) || (dataType == DVDMR::DataTerminatorWithLC)
        || (dataType == DVDMR::DataHeader) || (dataType == DVDMR::DataRate12))
    {
        uint8_t coded[196], info[96];
        m_history.bits(start + 12, 49, coded);
        m_history.bits(start + 95, 49, coded + 98);

        if (DVDMR::bptcDecode(coded, info) >= 0)
        {
            if ((dataType == DVDMR::DataVoiceLCHeader) || (dataType == DVDMR::DataTerminatorWithLC))
            {
                uint8_t lc[9];
                uint8_t mask = (dataType == DVDMR::DataVoiceLCHeader) ? DVDMR::m_maskVoiceLCHeader : DVDMR::m_maskTerminatorWithLC;

                if (DVDMR::fullLCDecode(info, mask, lc) >= 0) {
                    parseLC(lc, s);
                }
            }
            else
            {
                uint8_t bytes[12];
                DVBits::bitsToBytesMSB(info, 96, bytes);

                if (dataType == DVDMR::DataHeader) {
                    parseDataHeader(bytes, s);
                } else {
                    if (s.m_udtAssembly.m_active) {
                        parseDataBlock(bytes, s);
                    } else {
                        parsePacketBlock(bytes, s);
                    }
                }
            }
        }
        else if ((dataType == DVDMR::DataHeader) || (dataType == DVDMR::DataRate12))
        {
            s.m_udtAssembly = SlotState::UDTAssembly();
            s.m_packetAssembly = SlotState::PacketAssembly();
        }
    }
}

void DSDProtocolDMR::parseDataHeader(const uint8_t *header, SlotState& slot)
{
    slot.m_udtAssembly = SlotState::UDTAssembly();
    slot.m_packetAssembly = SlotState::PacketAssembly();
    uint16_t receivedCRC = (uint16_t) ((((uint16_t) header[10] << 8) | header[11]) ^ DVDMR::m_maskDataHeader);

    if (DVCRC::dmrCcitt(header, 10) != receivedCRC) {
        return;
    }

    // Every data header (ETSI TS 102 361-1 9.2.6): octet 0 G/I, A, HC, the pad octet count's top
    // bit, then the data packet format in the low nibble; octet 1 the SAP in the high nibble
    const int dpf = header[0] & 0x0f;

    if (dpf == 0)
    {
        // UDT (TS 102 361-4): octet 1's low nibble the format; octet 8 the pad nibbles, a
        // reserved bit and the appended blocks less one (MMDVMHost, DSD-FME)
        const int sap = header[1] >> 4;
        const int format = header[1] & 0x0f;
        const int blocks = (header[8] & 3) + 1;
        const bool supplementary = (header[9] & 0x80) != 0;
        const bool protectedData = (header[9] & 0x40) != 0;

        // ETSI UDT short data: no response, reserved bits zero, UDT SAP, one to four
        // appended rate-1/2 blocks, not supplementary or protected data.
        if ((sap != 0) || ((header[0] & 0x70) != 0) || ((header[8] & 4) != 0)
            || supplementary || protectedData)
        {
            return;
        }

        SlotState::UDTAssembly& assembly = slot.m_udtAssembly;
        assembly.m_active = true;
        assembly.m_group = (header[0] & 0x80) != 0;
        assembly.m_format = format;
        assembly.m_padNibbles = (header[8] >> 3) & 0x1f;
        assembly.m_blocksExpected = blocks;
        assembly.m_source = ((uint32_t) header[5] << 16) | ((uint32_t) header[6] << 8) | header[7];
        assembly.m_destination = ((uint32_t) header[2] << 16) | ((uint32_t) header[3] << 8) | header[4];

        int usefulBits = blocks * 96 - 16 - assembly.m_padNibbles * 4;

        if (usefulBits <= 0) {
            assembly = SlotState::UDTAssembly();
        }

        return;
    }

    // Unconfirmed (DPF 2) and confirmed (3) packet data: the pad octet count's five bits are
    // octet 0's bit 4 and octet 1's low nibble. (Read before 30 September 2026 from bits 4 to 1
    // of octet 0, the DPF the dispatch above had just read from the low nibble: no real
    // header ever passed)
    const bool confirmed = dpf == 3;

    if ((dpf != 2) && !confirmed) {
        return;
    }

    const bool responseRequested = (header[0] & 0x40) != 0;
    const bool compressed = (header[0] & 0x20) != 0;
    const int sap = header[1] >> 4;
    const int padOctets = (header[0] & 0x10) | (header[1] & 0x0f);
    const int blocks = header[8] & 0x7f;
    const bool fullMessage = (header[8] & 0x80) != 0;

    const bool supportedHeader = (compressed && (sap == 3)) || (!compressed && (sap == 4));

    if ((responseRequested != confirmed) || !fullMessage || (blocks <= 0)
        || (blocks > (confirmed ? 153 : 128)) || !supportedHeader
        || (!confirmed && ((header[9] & 0xf0) != 0)))
    {
        return;
    }

    SlotState::PacketAssembly& packet = slot.m_packetAssembly;
    packet.m_active = true;
    packet.m_confirmed = confirmed;
    packet.m_group = (header[0] & 0x80) != 0;
    packet.m_sap = sap;
    packet.m_padOctets = padOctets;
    packet.m_blocksExpected = blocks;
    packet.m_source = ((uint32_t) header[5] << 16) | ((uint32_t) header[6] << 8) | header[7];
    packet.m_destination = ((uint32_t) header[2] << 16) | ((uint32_t) header[3] << 8) | header[4];
}

void DSDProtocolDMR::parseDataBlock(const uint8_t *block, SlotState& slot)
{
    SlotState::UDTAssembly& assembly = slot.m_udtAssembly;

    if (!assembly.m_active || (assembly.m_blocksReceived >= assembly.m_blocksExpected)) {
        return;
    }

    std::memcpy(assembly.m_data + assembly.m_blocksReceived * 12, block, 12);
    assembly.m_blocksReceived++;

    if (assembly.m_blocksReceived != assembly.m_blocksExpected) {
        return;
    }

    const int totalBytes = assembly.m_blocksExpected * 12;
    const uint16_t receivedCRC = ((uint16_t) assembly.m_data[totalBytes - 2] << 8) | assembly.m_data[totalBytes - 1];

    if (DVCRC::dmrCcitt(assembly.m_data, totalBytes - 2) != receivedCRC)
    {
        assembly = SlotState::UDTAssembly();
        return;
    }

    const int usefulBits = (totalBytes - 2) * 8 - assembly.m_padNibbles * 4;
    std::string text;
    bool supported = false;

    if (assembly.m_format == 3) // ISO 7-bit, packed MSB first
    {
        const int characters = usefulBits / 7;

        for (int character = 0; character < characters; character++)
        {
            uint32_t value = 0;

            for (int bit = 0; bit < 7; bit++)
            {
                int position = character * 7 + bit;
                value = (value << 1) | ((assembly.m_data[position / 8] >> (7 - (position & 7))) & 1);
            }

            appendUTF8(text, value);
        }

        supported = true;
    }
    else if ((assembly.m_format == 4) && ((usefulBits & 7) == 0)) // ISO-8859-1
    {
        for (int i = 0; i < usefulBits / 8; i++) {
            appendUTF8(text, assembly.m_data[i]);
        }

        supported = true;
    }
    else if ((assembly.m_format == 7) && ((usefulBits & 15) == 0)) // 16-bit Unicode, big endian on air
    {
        DSDPacketText::decodeUTF16(assembly.m_data, usefulBits / 8, true, text);
        supported = true;
    }

    if (supported && !text.empty())
    {
        TextMessage message;
        message.m_valid = true;
        message.m_group = assembly.m_group;
        message.m_format = assembly.m_format;
        message.m_source = assembly.m_source;
        message.m_destination = assembly.m_destination;
        message.m_text = text;
        message.m_revision = ++m_metadataRevision;
        slot.m_textMessage = message;
    }

    assembly = SlotState::UDTAssembly();
}

void DSDProtocolDMR::parsePacketBlock(const uint8_t *block, SlotState& slot)
{
    SlotState::PacketAssembly& packet = slot.m_packetAssembly;

    if (!packet.m_active || (packet.m_blocksReceived >= packet.m_blocksExpected)) {
        return;
    }

    const uint8_t *data = block;
    int dataBytes = 12;

    if (packet.m_confirmed)
    {
        const int serial = block[0] >> 1;
        const uint16_t receivedCRC = (uint16_t) ((((uint16_t) (block[0] & 1) << 8) | block[1]) ^ 0x0f0);
        uint8_t crcBits[87];
        DVBits::bytesToBitsMSB(block + 2, 10, crcBits);
        DVBits::putBits(crcBits, 80, serial, 7);

        if ((DVCRC::dmr9(crcBits, 87) != receivedCRC)
            || ((packet.m_nextSerial >= 0) && (serial != packet.m_nextSerial)))
        {
            packet = SlotState::PacketAssembly();
            return;
        }

        packet.m_nextSerial = (serial + 1) & 0x7f;
        data = block + 2;
        dataBytes = 10;
    }

    if (packet.m_dataBytes + dataBytes > (int) sizeof(packet.m_data))
    {
        packet = SlotState::PacketAssembly();
        return;
    }

    std::memcpy(packet.m_data + packet.m_dataBytes, data, dataBytes);
    packet.m_dataBytes += dataBytes;
    packet.m_blocksReceived++;

    if (packet.m_blocksReceived == packet.m_blocksExpected) {
        publishPacketText(slot);
    }
}

void DSDProtocolDMR::publishPacketText(SlotState& slot)
{
    SlotState::PacketAssembly& packet = slot.m_packetAssembly;

    if ((packet.m_dataBytes < 4) || (packet.m_padOctets > packet.m_dataBytes - 4))
    {
        packet = SlotState::PacketAssembly();
        return;
    }

    const int crcPosition = packet.m_dataBytes - 4;
    const uint32_t receivedCRC = (uint32_t) packet.m_data[crcPosition]
        | ((uint32_t) packet.m_data[crcPosition + 1] << 8)
        | ((uint32_t) packet.m_data[crcPosition + 2] << 16)
        | ((uint32_t) packet.m_data[crcPosition + 3] << 24);

    if (dmrPacketCRC32(packet.m_data, crcPosition) != receivedCRC)
    {
        packet = SlotState::PacketAssembly();
        return;
    }

    const int payloadBytes = crcPosition - packet.m_padOctets;
    std::string decoded, kind;
    bool isText = false;

    if (packet.m_sap == 4)
    {
        // IPv4/UDP: the ETSI text service on port 5016 or Motorola's TMS on 4007
        isText = DSDPacketText::fromIPv4(packet.m_data, payloadBytes, decoded, kind);
    }
    else if ((packet.m_sap == 3) && (payloadBytes >= 5))
    {
        // The DMR compressed UDP/IPv4 header: port identifiers, or the ports themselves
        const uint8_t *compressed = packet.m_data;
        const int opcode = ((compressed[3] >> 7) << 1) | (compressed[4] >> 7);
        const int sourcePortId = compressed[3] & 0x7f;
        const int destinationPortId = compressed[4] & 0x7f;
        int position = 5;
        int sourcePort = sourcePortId == 1 ? 5016 : sourcePortId == 2 ? 5017 : -1;
        int destinationPort = destinationPortId == 1 ? 5016 : destinationPortId == 2 ? 5017 : -1;

        if ((opcode == 0) && (sourcePortId == 0) && (position + 2 <= payloadBytes)) {
            sourcePort = ((int) compressed[position] << 8) | compressed[position + 1];
            position += 2;
        }

        if ((opcode == 0) && (destinationPortId == 0) && (position + 2 <= payloadBytes)) {
            destinationPort = ((int) compressed[position] << 8) | compressed[position + 1];
            position += 2;
        }

        if ((opcode == 0) && (position <= payloadBytes)) {
            isText = DSDPacketText::fromUdpPayload(sourcePort, destinationPort, compressed + position, payloadBytes - position, decoded, kind);
        }
    }

    if (isText && !decoded.empty())
    {
        TextMessage message;
        message.m_valid = true;
        message.m_group = packet.m_group;
        message.m_format = kind == "TMS" ? 17 : 16;
        message.m_source = packet.m_source;
        message.m_destination = packet.m_destination;
        message.m_text = decoded;
        message.m_revision = ++m_metadataRevision;
        slot.m_textMessage = message;
    }

    packet = SlotState::PacketAssembly();
}

void DSDProtocolDMR::emitVoiceFrame(int64_t firstDibit, int64_t secondDibit, int nbFirst, int slot, int64_t sample)
{
    DSDVoiceFrame frame;
    frame.m_sample = sample;
    frame.m_slot = slot;
    frame.m_rate = DSDVoiceFrame::RateAMBE3600x2450;
    frame.m_nbBits = 72;
    uint8_t bits[72];
    m_history.bits(firstDibit, nbFirst, bits);
    m_history.llrs(firstDibit, nbFirst, frame.m_llr);

    if (nbFirst < 36)
    {
        m_history.bits(secondDibit, 36 - nbFirst, bits + 2 * nbFirst);
        m_history.llrs(secondDibit, 36 - nbFirst, frame.m_llr + 2 * nbFirst);
    }

    DVBits::bitsToBytesMSB(bits, 72, frame.m_bytes);
    m_frames.push_back(frame);

    if (dsdDebug()) {
        fprintf(stderr, "DMR voice frame at sample %lld slot %d\n", sample, slot);
    }
}

void DSDProtocolDMR::processVoice(int64_t start, int slot)
{
    SlotState& s = m_slots[slot];
    s.m_voice = true;
    s.m_typeText = "VOX";
    int64_t centre = start + m_syncEndOffset;

    // Embedded signalling in bursts B to E (LCSS 1 first, 3 continuation, 2 last)
    if (m_kind == BurstVoiceEmb)
    {
        int colorCode, lcss;
        bool pi;

        if (checkEmb(centre, colorCode, lcss, pi))
        {
            m_colorCode = colorCode;
            uint8_t fragment[32];
            m_history.bits(start + 70, 16, fragment);

            if (lcss == 1) {
                s.m_embeddedFragments = 0;
            }

            if (((lcss == 1) && (s.m_embeddedFragments == 0)) || ((lcss == 3) && (s.m_embeddedFragments == 1 || s.m_embeddedFragments == 2))
                || ((lcss == 2) && (s.m_embeddedFragments == 3)))
            {
                std::memcpy(&s.m_embedded[32 * s.m_embeddedFragments], fragment, 32);
                s.m_embeddedFragments++;

                if (lcss == 2)
                {
                    uint8_t lc[9];

                    if (DVDMR::embeddedLCDecode(s.m_embedded, lc) >= 0) {
                        parseLC(lc, s);
                    }

                    s.m_embeddedFragments = 0;
                }
            }
            else
            {
                s.m_embeddedFragments = 0;
            }
        }
    }
    else
    {
        s.m_embeddedFragments = 0;
    }

    if (m_kind == BurstVoiceSync) {
        s.m_voiceSequence = 0;
    } else {
        s.m_voiceSequence = (s.m_voiceSequence + 1) % 6;
    }

    emitProtocolBurst(start, slot, m_kind == BurstVoiceSync ? "voice-sync" : "voice", s.m_voiceSequence);

    emitVoiceFrame(start + 12, 0, 36, slot, m_history.sampleAt(start + 47));
    emitVoiceFrame(start + 48, start + 90, 18, slot, m_history.sampleAt(start + 107));
    emitVoiceFrame(start + 108, 0, 36, slot, m_history.sampleAt(start + 143));
}

void DSDProtocolDMR::emitProtocolBurst(int64_t start, int slot, const std::string& type, int sequence)
{
    uint8_t bits[264];
    uint8_t bytes[33];
    m_history.bits(start + 12, 132, bits); // MMDVM payload excludes the 12-dibit CACH.
    DVBits::bitsToBytesMSB(bits, 264, bytes);
    DSDProtocolFrame frame;
    frame.m_sample = m_history.sampleAt(start + 143);
    frame.m_format = "dmr-burst";
    frame.m_type = type;
    frame.m_slot = slot + 1;
    frame.m_sequence = sequence;
    frame.m_data.assign(bytes, bytes + sizeof(bytes));
    m_protocolFrames.push_back(frame);
}

DSDProtocolDMR::LinkControl DSDProtocolDMR::getLinkControl(int slot) const
{
    return m_slots[slot & 1].m_linkControl;
}

DSDProtocolDMR::TalkerAlias DSDProtocolDMR::getTalkerAlias(int slot) const
{
    return m_slots[slot & 1].m_talkerAlias;
}

DSDProtocolDMR::GPSInfo DSDProtocolDMR::getGPSInfo(int slot) const
{
    return m_slots[slot & 1].m_gpsInfo;
}

DSDProtocolDMR::TextMessage DSDProtocolDMR::getTextMessage(int slot) const
{
    return m_slots[slot & 1].m_textMessage;
}

const char *DSDProtocolDMR::getSlotTypeText(int slot) const
{
    return m_slots[slot & 1].m_typeText.c_str();
}

std::string DSDProtocolDMR::getStatusText() const
{
    char text[128];
    const char *station = m_station == StationBase ? "BS" : m_station == StationMobile ? "MS" : m_station == StationDirect ? "DM" : "NA";
    snprintf(text, sizeof(text), "%s CC:%2d S1: %s %08u>%c%08u S2: %s %08u>%c%08u", station, m_colorCode,
        m_slots[0].m_typeText.c_str(), m_slots[0].m_linkControl.m_source, m_slots[0].m_linkControl.m_group ? 'G' : 'U', m_slots[0].m_linkControl.m_destination,
        m_slots[1].m_typeText.c_str(), m_slots[1].m_linkControl.m_source, m_slots[1].m_linkControl.m_group ? 'G' : 'U', m_slots[1].m_linkControl.m_destination);
    return text;
}
