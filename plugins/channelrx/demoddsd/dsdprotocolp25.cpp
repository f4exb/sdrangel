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
#include <algorithm>

static bool dsdDebug() { static int d = getenv("DSD_DEBUG") ? 1 : 0; return d != 0; }

#include "dsp/dv/dvbits.h"
#include "dsp/dv/dvp25.h"

#include "dsdpackettext.h"
#include "dsdprotocolp25.h"

DSDProtocolP25::DSDProtocolP25(const DSDSymbolHistory& history) :
    DSDProtocol(history),
    m_active(false),
    m_unitStart(-1),
    m_unitLength(0),
    m_blocks(1),
    m_lastBlock(false),
    m_packetCount(0),
    m_packetFailed(0),
    m_metadataRevision(0),
    m_blocksDecoded(0),
    m_tsbkCount(0),
    m_tsbkFailed(0),
    m_grantAt(0),
    m_duid(-1),
    m_nac(-1),
    m_nidValid(false),
    m_proven(false),
    m_pendingStart(-1),
    m_pendingLength(0),
    m_pendingDuid(-1),
    m_pendingValid(false),
    m_burstValid(false),
    m_lastDuid(-1),
    m_misses(0),
    m_silent(0),
    m_absent(0),
    m_lcValid(false),
    m_group(false),
    m_source(0),
    m_destination(0),
    m_mfid(0),
    m_algid(0x80),
    m_kid(0),
    m_typeText("---")
{
}

std::vector<DVSync::Pattern> DSDProtocolP25::getSearchPatterns() const
{
    return { DVSync::P25 };
}

void DSDProtocolP25::start(DVSync::Pattern pattern, int64_t syncIndex)
{
    (void) pattern;
    m_active = true;
    m_proven = false;
    m_misses = 0;
    m_silent = 0;
    m_absent = 0;
    m_nac = -1;
    m_duid = -1;
    m_lastDuid = -1;
    m_lcValid = false;
    m_algid = 0x80;
    m_typeText = "---";
    m_tsbkCount = 0;
    m_tsbkFailed = 0;
    m_trunking.resetSite();
    m_tsbkText.clear();
    m_grantText.clear();
    m_grantAt = 0;
    m_unitStart = syncIndex - m_syncDibits + 1;
    m_unitLength = 0;
    m_nidValid = false;
}

void DSDProtocolP25::resync(DVSync::Pattern pattern, int64_t syncIndex)
{
    (void) pattern;
    m_misses = 0;
    m_silent = 0;
    m_unitStart = syncIndex - m_syncDibits + 1;
    m_unitLength = 0;
    m_blocks = 1;
    m_lastBlock = false;
    m_blocksDecoded = 0;
    m_nidValid = false;
}

bool DSDProtocolP25::acceptUnexpectedSync(DVSync::Pattern pattern, int64_t syncIndex) const
{
    (void) pattern;

    if (!m_active) {
        return false;
    }

    int64_t unitStart = syncIndex - m_syncDibits + 1;

    // A trunking or data unit ends after one, two or three blocks
    if (isVariable(m_duid) && m_nidValid)
    {
        for (int blocks = 1; blocks <= maxBlocks(); blocks++)
        {
            // (at the end of the blocks, or of their last status symbol period)
            const int data = m_tsduDataDibits + blocks * m_blockDibits;

            if ((std::labs(unitStart - (m_unitStart + onAirLength(data))) <= 2) || (std::labs(unitStart - (m_unitStart + paddedLength(data))) <= 2)) {
                return true;
            }
        }
    }

    // The NID of the current unit could not be read: any unit boundary that is a whole number of
    // status symbol periods away and at least a TSDU long is as good as the guess
    return !m_nidValid && (unitStart - m_unitStart >= onAirLength(m_tsduDataDibits + m_blockDibits) - 2);
}

int DSDProtocolP25::unitLength(int duid) const
{
    switch (duid)
    {
    case DUIDHDU: return m_hduDibits;
    case DUIDTDU: return m_tduDibits;
    case DUIDLDU1:
    case DUIDLDU2: return m_lduDibits;
    case DUIDTDULC: return m_tdulcDibits;
    case DUIDTSDU:
    case DUIDPDU: return paddedLength(m_tsduDataDibits + m_blocks * m_blockDibits);
    default: return m_lduDibits;
    }
}

bool DSDProtocolP25::getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const
{
    if (!m_active) {
        return false;
    }

    int length = m_unitLength;

    if (length == 0)
    {
        // NID not read yet: the usual sequence
        int duid = (m_lastDuid == DUIDLDU1) ? DUIDLDU2 : DUIDLDU1;
        length = unitLength(duid);
    }

    syncIndex = m_unitStart + length + m_syncDibits - 1;
    patterns = { DVSync::P25 };
    return true;
}

void DSDProtocolP25::getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const
{
    first = syncIndex - m_syncDibits + 1;
    last = syncIndex;
}

bool DSDProtocolP25::confirmWithoutSync(int64_t syncIndex, bool present)
{
    if (!present)
    {
        if (++m_silent >= 3) {
            m_active = false;
        }

        return true;
    }

    const DVSync::Info& info = DVSync::get(DVSync::P25);
    int64_t first = syncIndex - info.m_length + 1;
    int errors = info.m_length;

    if (m_history.has(first) && m_history.has(syncIndex))
    {
        errors = 0;

        for (int i = 0; i < info.m_length; i++) {
            errors += m_history.at(first + i).m_dibit != DVSync::levelToDibit(info.m_levels[i]);
        }
    }

    if (errors <= 4)
    {
        m_misses = 0;
        m_unitStart = first;
        m_unitLength = 0;
        m_blocks = 1;
        m_lastBlock = false;
    m_blocksDecoded = 0;
        m_nidValid = false;
        return true;
    }

    // A trunking or data unit may run on for another block: look for the sync there before
    // calling this a miss
    if (isVariable(m_duid) && m_nidValid && (m_blocks < maxBlocks()))
    {
        m_blocks++;
        m_unitLength = unitLength(m_duid);
        return true;
    }

    if (++m_misses >= 3) {
        m_active = false;
    }

    // Carry on as if the unit were there; the next sync decides
    m_unitStart = first;
    m_unitLength = 0;
    m_nidValid = false;
    return false;
}

void DSDProtocolP25::end()
{
    m_active = false;
}

void DSDProtocolP25::readNID()
{
    int64_t first = onAir(m_unitStart, m_syncDibits);
    int64_t last = onAir(m_unitStart, m_syncDibits + m_nidDibits - 1);

    if (!m_history.has(first) || !m_history.has(last)) {
        return;
    }

    uint8_t bits[64];
    float llr[64];
    gatherLlrs(m_unitStart, m_syncDibits, m_nidDibits, llr, bits);
    int nac, duid;
    int corrected = DVP25::nidDecode(bits, nac, duid);

    if (corrected >= 0)
    {
        m_nac = nac;
        m_duid = duid;
        m_nidValid = true;
        m_proven = true;
        m_blocks = 1;
        m_lastBlock = false;
        m_blocksDecoded = 0;
        m_packet = Packet();
        m_unitLength = unitLength(duid);
    }
    else
    {
        // Assume the usual sequence
        m_duid = (m_lastDuid == DUIDLDU1) ? DUIDLDU2 : DUIDLDU1;
        m_nidValid = false;
        m_unitLength = unitLength(m_duid);
    }

    if (dsdDebug()) {
        fprintf(stderr, "P25 NID at %lld: %s NAC %03X DUID %X length %d\n", m_unitStart, corrected >= 0 ? "ok" : "failed", nac, m_duid, m_unitLength);
    }
}

bool DSDProtocolP25::pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex)
{
    (void) sampleIndex;

    if (!m_active) {
        return false;
    }

    // The carrier gone: an LDU's time without it. A TDU's (15 ms) was a fade's: at 5 Hz Doppler a
    // fade below the presence threshold lasts that long, and the LDU it ended was lost whole, the
    // frames received before it too (processLDU skips the frames the fade took)
    m_absent = symbol.m_present ? 0 : m_absent + 1;

    if (m_absent >= m_lduDibits)
    {
        m_active = false;
        return false;
    }

    if (index == onAir(m_unitStart, m_syncDibits + m_nidDibits - 1)) {
        readNID();
    }

    // A trunking or data unit is one to three blocks on air, and the blocks are decoded and
    // parsed as they complete. The last block flag says where the messages end, not where the
    // unit does (control channels pad to three blocks with the flag set earlier), so the unit
    // length only grows while no sync turns up (confirmWithoutSync) and a sync at any block
    // boundary ends it (resync), or it completes at three blocks.
    if (isVariable(m_duid) && m_nidValid && !m_lastBlock && (m_blocksDecoded < maxBlocks())
        && (index == onAir(m_unitStart, m_tsduDataDibits + (m_blocksDecoded + 1) * m_blockDibits - 1)))
    {
        int k = m_blocksDecoded++;

        if (m_duid == DUIDPDU)
        {
            decodePacketBlock(k);
        }
        else if (m_duid == DUIDTSDU)
        {
            uint8_t block[12];
            int last = decodeBlock(m_unitStart, k, block);

            if (last < 0)
            {
                m_tsbkFailed++;

                if (dsdDebug()) {
                    fprintf(stderr, "P25 TSBK %d at %lld: failed\n", k, m_unitStart);
                }
            }
            else
            {
                m_tsbkCount++;
                parseTSBK(block);
                m_lastBlock = last == 1;

                if (dsdDebug()) {
                    fprintf(stderr, "P25 TSBK %d at %lld: op %02X mfid %02X %s [%02X %02X %02X %02X %02X %02X %02X %02X]\n", k, m_unitStart, block[0] & 0x3F, block[1], m_tsbkText.c_str(),
                        block[2], block[3], block[4], block[5], block[6], block[7], block[8], block[9]);
                }
            }
        }
    }

    if ((m_unitLength > 0) && (index == m_unitStart + m_unitLength - 1) && !(isVariable(m_duid) && m_nidValid && (m_blocks < maxBlocks())))
    {
        m_pendingStart = m_unitStart;
        m_pendingLength = m_unitLength;
        m_pendingDuid = m_duid;
        m_pendingValid = m_nidValid;
        m_lastDuid = m_duid;
        // The next unit follows directly unless a sync says otherwise
        m_unitStart += m_unitLength;
        m_unitLength = 0;
        m_nidValid = false;
        return true;
    }

    return false;
}

void DSDProtocolP25::getBurstSpan(int64_t& first, int64_t& last) const
{
    first = m_pendingStart;
    last = m_pendingStart + m_pendingLength - 1;
}

void DSDProtocolP25::processBurst()
{
    m_burstValid = m_pendingValid;

    switch (m_pendingDuid)
    {
    case DUIDHDU:
        processHDU(m_pendingStart);
        break;
    case DUIDLDU1:
    case DUIDLDU2:
        processLDU(m_pendingStart, m_pendingDuid);
        break;
    case DUIDTDU:
        m_typeText = "TDU";
        m_active = false;
        break;
    case DUIDTDULC:
        processTDULC(m_pendingStart);
        m_active = false;
        break;
    case DUIDTSDU:
        m_typeText = "TSDU"; // its blocks were parsed as they arrived
        break;
    case DUIDPDU:
        m_typeText = m_packet.m_headerValid ? (m_packet.m_confirmed ? "PDU confirmed" : "PDU") : "PDU (header failed)";
        break;
    default:
        break;
    }
}

void DSDProtocolP25::gatherLlrs(int64_t unitStart, int firstDataDibit, int nbDibits, float *llr, uint8_t *bits) const
{
    for (int i = 0; i < nbDibits; i++)
    {
        const DSDSoftSymbol& s = m_history.at(onAir(unitStart, firstDataDibit + i));
        llr[2 * i] = s.m_llrMsb;
        llr[2 * i + 1] = s.m_llrLsb;
        bits[2 * i] = (s.m_dibit >> 1) & 1;
        bits[2 * i + 1] = s.m_dibit & 1;
    }
}

void DSDProtocolP25::parseLC(const uint8_t *lc9)
{
    int lco = lc9[0] & 0x3F;
    m_mfid = lc9[1];

    if (lco == 0) // group voice channel user
    {
        m_group = true;
        m_destination = ((uint32_t) lc9[4] << 8) | lc9[5];
        m_source = ((uint32_t) lc9[6] << 16) | ((uint32_t) lc9[7] << 8) | lc9[8];
        m_lcValid = true;
    }
    else if (lco == 3) // unit to unit voice channel user
    {
        m_group = false;
        m_destination = ((uint32_t) lc9[3] << 16) | ((uint32_t) lc9[4] << 8) | lc9[5];
        m_source = ((uint32_t) lc9[6] << 16) | ((uint32_t) lc9[7] << 8) | lc9[8];
        m_lcValid = true;
    }
}

void DSDProtocolP25::processHDU(int64_t unitStart)
{
    if (!m_history.has(unitStart) || !m_history.has(unitStart + m_hduDibits - 1)) {
        return;
    }

    float llr[648];
    uint8_t bits[648];
    gatherLlrs(unitStart, m_syncDibits + m_nidDibits, 324, llr, bits);
    uint8_t mi[9];
    uint8_t mfid, algid;
    uint16_t kid, tgid;
    int corrected = DVP25::headerDecode(bits, mi, mfid, algid, kid, tgid);

    if (corrected >= 0)
    {
        m_mfid = mfid;
        m_algid = algid;
        m_kid = kid;
        m_group = true;
        m_destination = tgid;
        m_source = 0;   // the header has none: the LDU1's link control gives it (not the last call's)
        m_lcValid = true;
    }

    m_typeText = "HDU";

    if (dsdDebug()) {
        fprintf(stderr, "P25 HDU at %lld: %s TG %u ALGID %02X\n", unitStart, corrected >= 0 ? "ok" : "failed", tgid, algid);
    }
}

void DSDProtocolP25::processLDU(int64_t unitStart, int duid)
{
    if (!m_history.has(unitStart) || !m_history.has(unitStart + m_lduDibits - 1)) {
        return;
    }

    // Data dibit offsets of the nine IMBE frames and the six LC/ESS fragments and the LSD
    static const int imbeAt[9] = { 56, 128, 220, 312, 404, 496, 588, 680, 768 };
    static const int lcAt[6] = { 200, 292, 384, 476, 568, 660 };
    const int lsdAt = 752;

    for (int i = 0; i < 9; i++)
    {
        int present = 0;

        for (int k = 0; k < 72; k++) {
            present += m_history.at(onAir(unitStart, imbeAt[i] + k)).m_present;
        }

        if (present < 36) {
            continue;
        }

        DSDVoiceFrame frame;
        frame.m_sample = m_history.sampleAt(onAir(unitStart, imbeAt[i] + 71));
        frame.m_slot = 0;
        frame.m_rate = DSDVoiceFrame::RateIMBE7200x4400;
        frame.m_nbBits = 144;
        uint8_t bits[144];
        gatherLlrs(unitStart, imbeAt[i], 72, frame.m_llr, bits);
        DVBits::bitsToBytesMSB(bits, 144, frame.m_bytes);
        m_frames.push_back(frame);
    }

    float llr[240];
    uint8_t bits[240];

    for (int i = 0; i < 6; i++) {
        gatherLlrs(unitStart, lcAt[i], 20, llr + 40 * i, bits + 40 * i);
    }

    if (duid == DUIDLDU1)
    {
        uint8_t lc[9];

        if (DVP25::lcDecode(bits, lc) >= 0) {
            parseLC(lc);
        }

        m_typeText = "LDU1";
    }
    else
    {
        uint8_t mi[9];
        uint8_t algid;
        uint16_t kid;

        if (DVP25::essDecode(bits, mi, algid, kid) >= 0)
        {
            m_algid = algid;
            m_kid = kid;
        }

        m_typeText = "LDU2";
    }

    float lsdLlr[32];
    uint8_t lsdBits[32];
    gatherLlrs(unitStart, lsdAt, 16, lsdLlr, lsdBits);
    uint16_t lsd;
    DVP25::lsdDecode(lsdBits, lsd);

    if (dsdDebug()) {
        fprintf(stderr, "P25 %s at %lld: LC %s TG %u from %u\n", m_typeText.c_str(), unitStart, m_lcValid ? "ok" : "none", m_destination, m_source);
    }
}

void DSDProtocolP25::processTDULC(int64_t unitStart)
{
    if (!m_history.has(unitStart) || !m_history.has(unitStart + m_tdulcDibits - 1)) {
        return;
    }

    float llr[288];
    uint8_t bits[288];
    gatherLlrs(unitStart, m_syncDibits + m_nidDibits, 144, llr, bits);
    uint8_t lc[9];

    if (DVP25::tdulcDecode(bits, lc) >= 0) {
        parseLC(lc);
    }

    m_typeText = "TDULC";
}

int DSDProtocolP25::decodeBlock(int64_t unitStart, int k, uint8_t *bytes12) const
{
    int firstDibit = m_tsduDataDibits + k * m_blockDibits;

    if (!m_history.has(onAir(unitStart, firstDibit)) || !m_history.has(onAir(unitStart, firstDibit + m_blockDibits - 1))) {
        return -1;
    }

    float llr[196];
    uint8_t bits[196];
    gatherLlrs(unitStart, firstDibit, m_blockDibits, llr, bits);

    if (DVP25::blockDecode(llr, bytes12) < 0) {
        return -1;
    }

    return bytes12[0] >> 7;
}

void DSDProtocolP25::decodePacketBlock(int k)
{
    // A packet data unit (TIA-102.BAAA 7): a header block (rate 1/2 trellis, a CRC-16) that says
    // the format, service access point, logical link ID and block count, then the data blocks:
    // rate 1/2 of 12 bytes (unconfirmed, no CRC) or rate 3/4 of 16 bytes behind a serial number
    // and a CRC-9 (confirmed); the last four bytes of the data are the packet CRC-32
    int firstDibit = m_tsduDataDibits + k * m_blockDibits;

    if (!m_history.has(onAir(m_unitStart, firstDibit)) || !m_history.has(onAir(m_unitStart, firstDibit + m_blockDibits - 1))) {
        return;
    }

    float llr[196];
    uint8_t bits[196];
    gatherLlrs(m_unitStart, firstDibit, m_blockDibits, llr, bits);

    if (k == 0)
    {
        uint8_t header[12];

        if (DVP25::blockDecode(llr, header) < 0)
        {
            m_packetFailed++;

            if (dsdDebug()) {
                fprintf(stderr, "P25 PDU header at %lld: failed\n", m_unitStart);
            }

            return;
        }

        m_packet = Packet();
        m_packet.m_headerValid = true;
        m_packet.m_confirmed = ((header[0] >> 6) & 1) && ((header[0] & 0x1F) == 0x16);
        m_packet.m_outbound = ((header[0] >> 5) & 1) != 0;
        m_packet.m_format = header[0] & 0x1F;
        m_packet.m_sap = header[1] & 0x3F;
        m_packet.m_mfid = header[2];
        m_packet.m_address = ((uint32_t) header[3] << 16) | ((uint32_t) header[4] << 8) | header[5];
        m_packet.m_blocks = header[6] & 0x7F;
        m_packet.m_padOctets = header[7] & 0x1F;
        m_packet.m_offset = header[9] & 0x3F;
        // The unit grows one block at a time (confirmWithoutSync) up to maxBlocks(), which the
        // header's block count now sets; the blocks are decoded here as they complete

        if (dsdDebug()) {
            fprintf(stderr, "P25 PDU header at %lld: format %02X sap %02X mfid %02X llid %u %s blocks %d pad %d offset %d\n", m_unitStart,
                m_packet.m_format, m_packet.m_sap, m_packet.m_mfid, m_packet.m_address, m_packet.m_outbound ? "out" : "in", m_packet.m_blocks, m_packet.m_padOctets, m_packet.m_offset);
        }

        if (m_packet.m_blocks == 0) {
            finishPacket(); // a response or a trunking packet with no data
        }

        return;
    }

    if (dsdDebug()) {
        fprintf(stderr, "P25 PDU block %d: headerValid %d failed %d blocks %d\n", k, m_packet.m_headerValid, m_packet.m_failed, m_packet.m_blocks);
    }

    if (!m_packet.m_headerValid || m_packet.m_failed || (k > m_packet.m_blocks)) {
        return;
    }

    if (m_packet.m_confirmed)
    {
        uint8_t block[18];

        if (DVP25::block34Decode(llr, block) < 0) {
            m_packet.m_failed = true;
        } else {
            m_packet.m_data.insert(m_packet.m_data.end(), block + 2, block + 18);
        }
    }
    else
    {
        uint8_t block[12];

        if (DVP25::blockDecode(llr, block, false) < 0) {
            m_packet.m_failed = true;
        } else {
            m_packet.m_data.insert(m_packet.m_data.end(), block, block + 12);
        }
    }

    if (m_packet.m_failed)
    {
        m_packetFailed++;

        if (dsdDebug()) {
            fprintf(stderr, "P25 PDU block %d at %lld: failed\n", k, m_unitStart);
        }

        return;
    }

    m_packet.m_blocksReceived++;

    if (m_packet.m_blocksReceived == m_packet.m_blocks) {
        finishPacket();
    }
}

void DSDProtocolP25::finishPacket()
{
    Packet& packet = m_packet;
    const int total = (int) packet.m_data.size();

    if (total < 4 + packet.m_padOctets)
    {
        m_typeText = "PDU";
        return;
    }

    const uint32_t received = ((uint32_t) packet.m_data[total - 4] << 24) | ((uint32_t) packet.m_data[total - 3] << 16)
        | ((uint32_t) packet.m_data[total - 2] << 8) | packet.m_data[total - 1];

    if (DVP25::packetCrc(packet.m_data.data(), total - 4) != received)
    {
        m_packetFailed++;

        if (dsdDebug()) {
            fprintf(stderr, "P25 PDU at %lld: CRC-32 failed (%d bytes)\n", m_unitStart, total);
        }

        return;
    }

    m_packetCount++;
    const uint8_t *data = packet.m_data.data();
    int nbBytes = total - 4 - packet.m_padOctets;
    int sap = packet.m_sap;
    uint32_t source = packet.m_outbound ? 0 : packet.m_address;
    uint32_t destination = packet.m_outbound ? packet.m_address : 0;

    // The secondary headers the data header offset covers: an extended address (SAP 31: the
    // other logical link ID and the SAP of what follows), an encryption sync header (SAP 1)
    if (packet.m_offset > 0)
    {
        int position = 0;

        if ((sap == 31) && (nbBytes >= 5))
        {
            // The extended address header: a reserved byte, the SAP of what follows (6 bits) and
            // the other logical link ID (24 bits), as DSD-FME reads it
            sap = data[1] & 0x3F;
            uint32_t extended = ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 8) | data[4];

            if (packet.m_outbound) {
                source = extended;
            } else {
                destination = extended;
            }

            position += 4;
        }

        if (sap == 1)
        {
            m_typeText = "PDU encrypted";
            return;
        }

        int skip = std::min(nbBytes, packet.m_offset * (packet.m_confirmed ? 16 : 12));
        position = std::max(position, skip);
        data += position;
        nbBytes -= position;
    }

    if (dsdDebug()) {
        fprintf(stderr, "P25 PDU at %lld: %d bytes sap %02X %s\n", m_unitStart, nbBytes, sap, packet.m_confirmed ? "confirmed" : "unconfirmed");
    }

    std::string text, kind;

    if (((sap == 0) || (sap == 4)) && DSDPacketText::fromIPv4(data, nbBytes, text, kind) && !text.empty())
    {
        m_textMessage.m_valid = true;
        m_textMessage.m_source = source;
        m_textMessage.m_destination = destination;
        m_textMessage.m_confirmed = packet.m_confirmed;
        m_textMessage.m_kind = kind;
        m_textMessage.m_text = text;
        m_textMessage.m_revision = ++m_metadataRevision;
    }
}

void DSDProtocolP25::parseTSBK(const uint8_t *b)
{
    bool grant;
    m_tsbkText = m_trunking.parseTSBK(b, grant);

    // A channel grant is what a listener wants to see: it stays on display for a few seconds
    if (grant)
    {
        m_grantText = m_tsbkText;
        m_grantAt = m_tsbkCount;
    }
}

bool DSDProtocolP25::getNetworkStatus(uint32_t& wacn, uint16_t& sysid) const
{
    wacn = m_trunking.getWacn();
    sysid = m_trunking.getSysid();
    return m_trunking.isNetworkValid();
}

bool DSDProtocolP25::getLinkControl(bool& group, uint32_t& source, uint32_t& destination) const
{
    group = m_group;
    source = m_source;
    destination = m_destination;
    return m_lcValid;
}

std::string DSDProtocolP25::getStatusText() const
{
    char text[128];

    if (m_lcValid)
    {
        char from[24] = "";

        if (m_source != 0) {
            snprintf(from, sizeof(from), " from %u", m_source);
        }

        snprintf(text, sizeof(text), "NAC %03X %s %s %u%s%s", m_nac & 0xFFF, m_typeText.c_str(), m_group ? "TG" : "unit", m_destination, from, m_algid == 0x80 ? "" : " encrypted");
    } else if (m_tsbkCount > 0) {
        // The system identity in the form the P25 Phase 2 setting takes (WACN:SYS:NAC)
        char system[32] = "";

        if (m_trunking.isNetworkValid()) {
            snprintf(system, sizeof(system), " system %05X:%03X:%03X", m_trunking.getWacn(), m_trunking.getSysid(), m_nac & 0xFFF);
        }

        bool grant = !m_grantText.empty() && (m_tsbkCount - m_grantAt < 40); // about 3 seconds of blocks
        snprintf(text, sizeof(text), "NAC %03X%s %s %s", m_nac & 0xFFF, system, m_typeText.c_str(), grant ? m_grantText.c_str() : m_tsbkText.c_str());
    } else {
        snprintf(text, sizeof(text), "NAC %03X %s", m_nac & 0xFFF, m_typeText.c_str());
    }

    return text;
}
