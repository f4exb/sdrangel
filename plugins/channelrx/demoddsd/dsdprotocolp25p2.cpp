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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool dsdDebug() { static int d = getenv("DSD_DEBUG") ? 1 : 0; return d != 0; }

#include "dsp/dv/dvbits.h"

#include "dsdprotocolp25p2.h"

DSDProtocolP25P2::DSDProtocolP25P2(const DSDSymbolHistory& history) :
    DSDProtocol(history),
    m_active(false),
    m_burstStart(-1),
    m_ischPending(false),
    m_pendingStart(-1),
    m_burstValid(false),
    m_timeslot(-1),
    m_timeslotStart(-1),
    m_misses(0),
    m_silent(0),
    m_absent(0),
    m_hasSystem(false),
    m_systemLearned(false),
    m_wacn(0),
    m_sysid(0),
    m_nac(0),
    m_lastDuid(-1)
{
}

std::vector<DVSync::Pattern> DSDProtocolP25P2::getSearchPatterns() const
{
    return { DVSync::P25P2 };
}

void DSDProtocolP25P2::setSystem(uint32_t wacn, uint16_t sysid, uint16_t nac)
{
    m_wacn = wacn;
    m_sysid = sysid;
    m_nac = nac;
    m_hasSystem = true;
    m_mask.resize(2 * DVP25P2::m_superframeDibits);
    DVP25P2::scramblerMask(wacn, sysid, nac, m_mask.data());
}

void DSDProtocolP25P2::start(DVSync::Pattern pattern, int64_t syncIndex)
{
    (void) pattern;
    m_active = true;
    m_burstStart = syncIndex - m_ischDibits + 1;
    m_ischPending = false;
    m_misses = 0;
    m_silent = 0;
    m_absent = 0;
    m_timeslot = -1;
    m_timeslotStart = -1;
    m_lastDuid = -1;
    m_channels[0] = Channel();
    m_channels[1] = Channel();
    m_deferred.clear();
    m_signallingText.clear();
    m_grantText.clear();
    m_trunking.resetSite();
}

void DSDProtocolP25P2::resync(DVSync::Pattern pattern, int64_t syncIndex)
{
    (void) pattern;
    m_burstStart = syncIndex - m_ischDibits + 1;
    m_ischPending = false;
    m_misses = 0;
    m_silent = 0;
}

bool DSDProtocolP25P2::getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const
{
    if (!m_active) {
        return false;
    }

    // The ISCH of the burst being received, or of the next one when it has already been seen
    syncIndex = m_burstStart + m_ischDibits - 1 + (m_ischPending ? 0 : m_burstDibits);
    patterns = { DVSync::P25P2 };
    return true;
}

void DSDProtocolP25P2::getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const
{
    first = syncIndex - m_ischDibits + 1;
    last = syncIndex;
}

int DSDProtocolP25P2::timeslotAt(int64_t burstStart) const
{
    if (m_timeslot < 0) {
        return -1;
    }

    // Resyncs may move a burst by a symbol or two: round to whole bursts
    int64_t bursts = std::lround((burstStart - m_timeslotStart) / (double) m_burstDibits);
    return (int) (((m_timeslot + bursts) % 12 + 12) % 12);
}

DVP25P2::ISCHType DSDProtocolP25P2::readISCH(int64_t burstStart, int& distance)
{
    distance = 40;

    if (!m_history.has(burstStart) || !m_history.has(burstStart + m_ischDibits - 1)) {
        return DVP25P2::ISCHNone;
    }

    float llr[40];
    uint8_t bits[40];
    gather(burstStart, 0, m_ischDibits, llr, bits);
    DVP25P2::ISCH isch;
    DVP25P2::ISCHType type = DVP25P2::ischDecode(llr, isch, distance);

    if (distance > 7) {
        return DVP25P2::ISCHNone;
    }

    if (type == DVP25P2::ISCHInfo)
    {
        int predicted = timeslotAt(burstStart);

        if (dsdDebug() && (predicted >= 0) && (predicted != isch.getTimeslot())) {
            fprintf(stderr, "P25P2 timeslot %d at %lld, %d expected\n", isch.getTimeslot(), burstStart, predicted);
        }

        m_timeslot = isch.getTimeslot();
        m_timeslotStart = burstStart;
    }
    else
    {
        int predicted = timeslotAt(burstStart);

        if ((predicted >= 0) && !DVP25P2::timeslotHasSync(predicted))
        {
            if (dsdDebug()) {
                fprintf(stderr, "P25P2 sync at %lld in timeslot %d: timeslot lost\n", burstStart, predicted);
            }

            m_timeslot = -1;
        }
    }

    return type;
}

bool DSDProtocolP25P2::confirmWithoutSync(int64_t syncIndex, bool present)
{
    int64_t burstStart = syncIndex - m_ischDibits + 1;

    if (!present)
    {
        if (++m_silent >= 3) {
            m_active = false;
        }

        m_burstStart = burstStart;
        m_ischPending = false;
        return true;
    }

    m_silent = 0;
    int distance;
    DVP25P2::ISCHType type = readISCH(burstStart, distance);
    m_burstStart = burstStart;
    m_ischPending = false;

    if (type != DVP25P2::ISCHNone)
    {
        m_misses = 0;
        return true;
    }

    if (++m_misses >= 3) {
        m_active = false;
    }

    return false;
}

void DSDProtocolP25P2::end()
{
    m_active = false;
}

bool DSDProtocolP25P2::pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex)
{
    (void) sampleIndex;

    if (!m_active) {
        return false;
    }

    m_absent = symbol.m_present ? 0 : m_absent + 1;

    if (m_absent >= 2 * m_burstDibits)
    {
        m_active = false;
        return false;
    }

    if (index == m_burstStart + m_burstDibits - 1)
    {
        m_pendingStart = m_burstStart;
        m_burstStart += m_burstDibits;
        m_ischPending = true;
        return true;
    }

    return false;
}

void DSDProtocolP25P2::getBurstSpan(int64_t& first, int64_t& last) const
{
    first = m_pendingStart;
    last = m_pendingStart + m_burstDibits - 1;
}

void DSDProtocolP25P2::gather(int64_t burstStart, int firstDibit, int nbDibits, float *llr, uint8_t *bits) const
{
    for (int i = 0; i < nbDibits; i++)
    {
        const DSDSoftSymbol& s = m_history.at(burstStart + firstDibit + i);
        llr[2 * i] = s.m_llrMsb;
        llr[2 * i + 1] = s.m_llrLsb;
        bits[2 * i] = (s.m_dibit >> 1) & 1;
        bits[2 * i + 1] = s.m_dibit & 1;
    }
}

void DSDProtocolP25P2::descramble(int timeslot, int firstDibit, int nbDibits, float *llr, uint8_t *bits) const
{
    for (int i = 0; i < nbDibits; i++)
    {
        int m = 2 * DVP25P2::maskDibit(timeslot, firstDibit + i);

        for (int b = 0; b < 2; b++)
        {
            if (m_mask[m + b])
            {
                llr[2 * i + b] = -llr[2 * i + b];
                bits[2 * i + b] ^= 1;
            }
        }
    }
}

void DSDProtocolP25P2::processBurst()
{
    int64_t burstStart = m_pendingStart;
    m_burstValid = false;

    if (!m_history.has(burstStart) || !m_history.has(burstStart + m_burstDibits - 1)) {
        return;
    }

    int ischDistance;
    DVP25P2::ISCHType ischType = readISCH(burstStart, ischDistance);

    float duidLlr[8];
    uint8_t duidBits[8];

    for (int i = 0; i < 4; i++) {
        gather(burstStart, DVP25P2::m_duidDibits[i], 1, duidLlr + 2 * i, duidBits + 2 * i);
    }

    int duidDistance;
    int duid = DVP25P2::duidDecode(duidLlr, duidDistance);
    int timeslot = timeslotAt(burstStart);
    int channel = timeslot >= 0 ? DVP25P2::timeslotChannel(timeslot) : -1;
    m_burstValid = (ischType != DVP25P2::ISCHNone) && (duidDistance <= 1);

    if (dsdDebug()) {
        fprintf(stderr, "P25P2 burst at %lld: ISCH %s (%d) timeslot %d DUID %s (%d)\n", burstStart,
            ischType == DVP25P2::ISCHSync ? "sync" : ischType == DVP25P2::ISCHInfo ? "info" : "none", ischDistance, timeslot,
            DVP25P2::duidName(duid), duidDistance);
    }

    if (duidDistance > 1) {
        return;
    }

    m_lastDuid = duid;

    if (channel < 0)
    {
        // The timeslot, hence the channel and the scrambling, is not known until an
        // information ISCH is read: keep the burst for when it is (the history holds it)
        if (m_deferred.size() < 4) {
            m_deferred.push_back({ burstStart, duid });
        }

        return;
    }

    for (const Deferred& d : m_deferred)
    {
        int ts = timeslotAt(d.m_burstStart);

        if ((ts >= 0) && m_history.has(d.m_burstStart)) {
            processPayload(d.m_burstStart, d.m_duid, DVP25P2::timeslotChannel(ts));
        }
    }

    m_deferred.clear();
    processPayload(burstStart, duid, channel);
}

void DSDProtocolP25P2::processPayload(int64_t burstStart, int duid, int channel)
{
    m_channels[channel].m_typeText = DVP25P2::duidName(duid);

    switch (duid)
    {
    case DVP25P2::DUID4V:
    case DVP25P2::DUID2V:
        if (m_hasSystem) {
            processVoice(burstStart, duid, channel);
        }
        break;
    case DVP25P2::DUIDSACCHScrambled:
    case DVP25P2::DUIDFACCHScrambled:
        if (m_hasSystem) {
            processACCH(burstStart, duid, channel);
        }
        break;
    case DVP25P2::DUIDSACCH:
    case DVP25P2::DUIDFACCH:
        processACCH(burstStart, duid, channel);
        break;
    case DVP25P2::DUIDLCCH:
        processLCCH(burstStart, channel);
        break;
    default:
        // The scrambled LCCH (DUID 4): inbound only (OP25), reserved (SDRTrunk)
        break;
    }
}

void DSDProtocolP25P2::processVoice(int64_t burstStart, int duid, int channel)
{
    int timeslot = timeslotAt(burstStart);
    Channel& ch = m_channels[channel];
    int nbCodewords = (duid == DVP25P2::DUID4V) ? 4 : 2;

    for (int c = 0; c < nbCodewords; c++)
    {
        int first = DVP25P2::m_voiceDibits[c];
        int present = 0;

        for (int k = 0; k < 36; k++) {
            present += m_history.at(burstStart + first + k).m_present;
        }

        if (present < 18) {
            continue;
        }

        DSDVoiceFrame frame;
        frame.m_sample = m_history.sampleAt(burstStart + first + 35);
        frame.m_slot = channel;
        frame.m_rate = DSDVoiceFrame::RateAMBE3600x2450;
        frame.m_nbBits = 72;
        uint8_t bits[72];
        gather(burstStart, first, 36, frame.m_llr, bits);
        descramble(timeslot, first, 36, frame.m_llr, bits);
        DVBits::bitsToBytesMSB(bits, 72, frame.m_bytes);
        m_frames.push_back(frame);
    }

    // Encryption sync: ESS-B in the four 4V bursts, ESS-A and the check in the 2V burst
    if (duid == DVP25P2::DUID4V)
    {
        if (ch.m_voiceBurst >= 4)
        {
            // A 2V burst was missed
            ch.m_voiceBurst = 0;
            ch.m_essBursts = 0;
        }

        float llr[24];
        uint8_t bits[24];
        gather(burstStart, DVP25P2::m_essBDibit, 12, llr, bits);
        descramble(timeslot, DVP25P2::m_essBDibit, 12, llr, bits);

        for (int i = 0; i < 4; i++) {
            ch.m_essB[4 * ch.m_voiceBurst + i] = (uint8_t) DVBits::getBits(bits, 6 * i, 6);
        }

        ch.m_essBursts |= 1 << ch.m_voiceBurst;
        ch.m_voiceBurst++;
    }
    else
    {
        if (ch.m_essBursts == 0xF)
        {
            float llr[170];
            uint8_t bits[170];
            gather(burstStart, DVP25P2::m_essBDibit, 85, llr, bits);
            descramble(timeslot, DVP25P2::m_essBDibit, 85, llr, bits);
            uint8_t essA[28];
            int j = 0;

            for (int i = 0; i < 28; i++)
            {
                essA[i] = (uint8_t) DVBits::getBits(bits, 2 * j, 6);
                j += (i == 15) ? 4 : 3; // the DUID dibit sits between hexbits 15 and 16
            }

            uint8_t algid, mi[9];
            uint16_t keyid;
            int corrected = DVP25P2::essDecode(ch.m_essB, essA, algid, keyid, mi);

            if ((corrected >= 0) && (corrected <= 14))
            {
                ch.m_algid = algid;
                ch.m_keyid = keyid;
                std::memcpy(ch.m_mi, mi, 9);
            }

            if (dsdDebug()) {
                fprintf(stderr, "P25P2 ESS channel %d: %s algid %02X keyid %04X (%d)\n", channel, corrected >= 0 ? "ok" : "failed", algid, keyid, corrected);
            }
        }

        ch.m_voiceBurst = 0;
        ch.m_essBursts = 0;
    }
}

void DSDProtocolP25P2::processACCH(int64_t burstStart, int duid, int channel)
{
    bool fast = (duid == DVP25P2::DUIDFACCH) || (duid == DVP25P2::DUIDFACCHScrambled);
    bool scrambled = (duid == DVP25P2::DUIDSACCHScrambled) || (duid == DVP25P2::DUIDFACCHScrambled);
    int timeslot = timeslotAt(burstStart);
    float llr[360];
    uint8_t bits[360];
    gather(burstStart, 0, m_burstDibits, llr, bits);

    if (scrambled) {
        descramble(timeslot, m_ischDibits, m_burstDibits - m_ischDibits, llr + 2 * m_ischDibits, bits + 2 * m_ischDibits);
    }

    int nbBits = fast ? DVP25P2::m_facchBits : DVP25P2::m_sacchBits;
    uint8_t coded[312];

    for (int k = 0; k < nbBits / 2; k++)
    {
        int d = fast ? DVP25P2::facchDibit(k) : DVP25P2::sacchDibit(k);
        coded[2 * k] = bits[2 * d];
        coded[2 * k + 1] = bits[2 * d + 1];
    }

    uint8_t pdu[168];
    int corrected = fast ? DVP25P2::facchDecode(coded, pdu) : DVP25P2::sacchDecode(coded, pdu);

    if (corrected < 0)
    {
        if (dsdDebug()) {
            fprintf(stderr, "P25P2 %s at %lld channel %d: failed\n", DVP25P2::duidName(duid), burstStart, channel);
        }

        return;
    }

    int pduBits = fast ? DVP25P2::m_facchPduBits : DVP25P2::m_sacchPduBits;
    uint8_t bytes[21];
    DVBits::bitsToBytesMSB(pdu, pduBits, bytes);

    Channel& ch = m_channels[channel];
    parseMAC(bytes, pduBits / 8, ch);
    m_burstValid = true;

    if (dsdDebug()) {
        fprintf(stderr, "P25P2 %s at %lld channel %d: %s (%d) TG %u from %u\n", DVP25P2::duidName(duid), burstStart, channel, ch.m_macText, corrected, ch.m_group, ch.m_source);
    }
}

void DSDProtocolP25P2::processLCCH(int64_t burstStart, int channel)
{
    // Coded as the SACCH, never scrambled: a 164 bit MAC PDU, its MAC messages in octets 1-18
    // and the colour code (the NAC) in bits 152-163
    float llr[360];
    uint8_t bits[360];
    gather(burstStart, 0, m_burstDibits, llr, bits);
    uint8_t coded[312];

    for (int k = 0; k < DVP25P2::m_sacchBits / 2; k++)
    {
        int d = DVP25P2::sacchDibit(k);
        coded[2 * k] = bits[2 * d];
        coded[2 * k + 1] = bits[2 * d + 1];
    }

    uint8_t pdu[168] = { 0 };
    int corrected = DVP25P2::lcchDecode(coded, pdu);

    if (corrected < 0)
    {
        if (dsdDebug()) {
            fprintf(stderr, "P25P2 LCCH at %lld channel %d: failed\n", burstStart, channel);
        }

        return;
    }

    uint8_t bytes[21];
    DVBits::bitsToBytesMSB(pdu, 168, bytes);
    m_channels[channel].m_macText = DVP25P2::macOpcodeName(bytes[0] >> 5);
    parseMCOs(bytes + 1, 18);
    learnSystem((int) DVBits::getBits(pdu, 152, 12));
    m_burstValid = true;

    if (dsdDebug()) {
        fprintf(stderr, "P25P2 LCCH at %lld channel %d: %s (%d) NAC %03X: %s\n", burstStart, channel, m_channels[channel].m_macText, corrected,
            (unsigned) DVBits::getBits(pdu, 152, 12), m_signallingText.c_str());
    }
}

void DSDProtocolP25P2::parseMCOs(const uint8_t *area, int nbBytes)
{
    // Each message's length from its opcode (its own length field for the manufacturer and null
    // avoid zero bias messages); the walk ends at null information, or at a length not known
    for (int p = 0; p < nbBytes; )
    {
        const int length = DSDP25Trunking::mcoLength(area + p, nbBytes - p);

        if ((length <= 0) || (p + length > nbBytes)) {
            break;
        }

        if (area[p] != 0x08)
        {
            bool grant;
            m_signallingText = m_trunking.parseMCO(area + p, length, grant);

            if (grant) {
                m_grantText = m_signallingText;
            }

            if (dsdDebug()) {
                fprintf(stderr, "P25P2 MCO %02X (%d octets): %s\n", area[p], length, m_signallingText.c_str());
            }
        }

        p += length;
    }
}

void DSDProtocolP25P2::learnSystem(int colourCode)
{
    // Nothing set (the settings' zeros): the network status broadcast's WACN and system ID, with its
    // colour code (else the LCCH's), are what the scrambling needs (as dsd-fme takes them)
    const bool unset = !m_hasSystem || ((m_wacn == 0) && (m_sysid == 0));
    const int nac = m_trunking.getColourCode() >= 0 ? m_trunking.getColourCode() : colourCode;

    if (unset && m_trunking.isNetworkValid() && (nac > 0))
    {
        setSystem(m_trunking.getWacn(), m_trunking.getSysid(), (uint16_t) nac);
        m_systemLearned = true;

        if (dsdDebug()) {
            fprintf(stderr, "P25P2 system learned: WACN %05X SYS %03X NAC %03X\n", m_wacn, m_sysid, m_nac);
        }
    }
}

void DSDProtocolP25P2::parseMAC(const uint8_t *pdu, int nbBytes, Channel& channel)
{
    int opcode = pdu[0] >> 5;
    channel.m_macText = DVP25P2::macOpcodeName(opcode);

    // SIGNAL, IDLE, ACTIVE and HANGTIME carry a list of MAC messages after the opcode octet
    if ((opcode == DVP25P2::MacSignal) || (opcode == DVP25P2::MacIdle) || (opcode == DVP25P2::MacActive) || (opcode == DVP25P2::MacHangtime))
    {
        parseMCOs(pdu + 1, nbBytes - 1);
        learnSystem(-1);
    }

    switch (opcode)
    {
    case DVP25P2::MacPTT:
        if (nbBytes >= 18)
        {
            std::memcpy(channel.m_mi, pdu + 1, 9);
            channel.m_algid = pdu[10];
            channel.m_keyid = ((uint16_t) pdu[11] << 8) | pdu[12];
            channel.m_source = ((uint32_t) pdu[13] << 16) | ((uint32_t) pdu[14] << 8) | pdu[15];
            channel.m_group = ((uint16_t) pdu[16] << 8) | pdu[17];
            channel.m_callValid = true;
        }
        break;
    case DVP25P2::MacEndPTT:
        if (nbBytes >= 18)
        {
            channel.m_source = ((uint32_t) pdu[13] << 16) | ((uint32_t) pdu[14] << 8) | pdu[15];
            channel.m_group = ((uint16_t) pdu[16] << 8) | pdu[17];
            channel.m_callValid = true;
        }
        break;
    case DVP25P2::MacIdle:
        channel.m_callValid = false;
        channel.m_algid = 0x80;
        break;
    default:
        break;
    }
}

bool DSDProtocolP25P2::getCall(int channel, uint32_t& source, uint16_t& group) const
{
    const Channel& ch = m_channels[channel & 1];
    source = ch.m_source;
    group = ch.m_group;
    return ch.m_callValid;
}

std::string DSDProtocolP25P2::getStatusText() const
{
    std::string text = m_timeslot >= 0 ? "" : "TS ? ";

    if (!m_hasSystem) {
        text += "no system ";
    }

    for (int c = 0; c < 2; c++)
    {
        const Channel& ch = m_channels[c];
        char part[96];

        if (ch.m_callValid) {
            snprintf(part, sizeof(part), "S%d: %s %s TG %u from %u%s%s ", c + 1, ch.m_typeText, ch.m_macText, ch.m_group, ch.m_source,
                ch.m_algid == 0x80 ? "" : " ", ch.m_algid == 0x80 ? "" : DVP25P2::algorithmName(ch.m_algid));
        } else {
            snprintf(part, sizeof(part), "S%d: %s %s ", c + 1, ch.m_typeText, ch.m_macText);
        }

        text += part;
    }

    // The control signalling: the last grant, else the last message (site, network...)
    if (!m_grantText.empty()) {
        text += "| " + m_grantText + " ";
    } else if (!m_signallingText.empty()) {
        text += "| " + m_signallingText + " ";
    }

    if (m_systemLearned) {
        text += "(system learned) ";
    }

    if (!text.empty()) {
        text.pop_back();
    }

    return text;
}
