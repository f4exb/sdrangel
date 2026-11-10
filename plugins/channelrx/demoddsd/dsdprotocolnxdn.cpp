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
#include "dsp/dv/dvcrc.h"
#include "dsp/dv/dvnxdn.h"

#include "dsdpackettext.h"

#include "dsdprotocolnxdn.h"

DSDProtocolNXDN::DSDProtocolNXDN(const DSDSymbolHistory& history, int symbolRate) :
    DSDProtocol(history),
    m_symbolRate(symbolRate),
    m_active(false),
    m_syncIndex(-1),
    m_pendingSync(-1),
    m_frameCount(0),
    m_good(0),
    m_misses(0),
    m_silent(0),
    m_absent(0),
    m_burstValid(false),
    m_ran(-1),
    m_messageMask(0),
    m_callValid(false),
    m_fullRate(false),
    m_group(false),
    m_source(0),
    m_destination(0),
    m_typeText("---"),
    m_dataCallCount(0),
    m_metadataRevision(0),
    m_cacCount(0)
{
    std::memset(m_message, 0, sizeof(m_message));
}

std::vector<DVSync::Pattern> DSDProtocolNXDN::getSearchPatterns() const
{
    return { DVSync::NXDNPreambleFSW, DVSync::NXDNFSW };
}

void DSDProtocolNXDN::start(DVSync::Pattern pattern, int64_t syncIndex)
{
    (void) pattern;
    m_active = true;
    m_misses = 0;
    m_silent = 0;
    m_absent = 0;
    m_frameCount = 0;
    m_good = 0;
    m_proven = false;
    m_lastFrameValid = false;
    m_ran = -1;
    m_messageMask = 0;
    m_callValid = false;
    m_fullRate = false;
    m_typeText = "---";
    m_syncIndex = syncIndex;
    m_cacCount = 0;
    m_siteText.clear();
    m_controlText.clear();
    m_grantText.clear();
}

void DSDProtocolNXDN::resync(DVSync::Pattern pattern, int64_t syncIndex)
{
    (void) pattern;
    m_misses = 0;
    m_silent = 0;
    m_syncIndex = syncIndex;
}

bool DSDProtocolNXDN::getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const
{
    if (!m_active) {
        return false;
    }

    syncIndex = m_syncIndex + m_frameSymbols;
    patterns = { DVSync::NXDNFSW, DVSync::NXDNPreambleFSW };
    return true;
}

void DSDProtocolNXDN::getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const
{
    first = syncIndex - m_fswSymbols + 1;
    last = syncIndex;
}

bool DSDProtocolNXDN::confirmWithoutSync(int64_t syncIndex, bool present)
{
    if (!present)
    {
        if (++m_silent >= 3) {
            m_active = false;
        }

        m_syncIndex = syncIndex;
        return true;
    }

    const DVSync::Info& info = DVSync::get(DVSync::NXDNFSW);
    int64_t first = syncIndex - info.m_length + 1;
    int errors = info.m_length;

    if (m_history.has(first) && m_history.has(syncIndex))
    {
        errors = 0;

        for (int i = 0; i < info.m_length; i++) {
            errors += m_history.at(first + i).m_dibit != DVSync::levelToDibit(info.m_levels[i]);
        }
    }

    if (errors <= 2)
    {
        m_misses = 0;
        m_syncIndex = syncIndex;
        return true;
    }

    if (++m_misses >= 3) {
        m_active = false;
    }

    m_syncIndex = syncIndex; // carry on; the next sync decides
    return false;
}

void DSDProtocolNXDN::end()
{
    m_active = false;
}

bool DSDProtocolNXDN::pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex)
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

    if (index == m_syncIndex + m_frameSymbols - m_fswSymbols)
    {
        m_pendingSync = m_syncIndex;
        return true;
    }

    return false;
}

void DSDProtocolNXDN::getBurstSpan(int64_t& first, int64_t& last) const
{
    first = m_pendingSync - m_fswSymbols + 1;
    last = m_pendingSync + m_frameSymbols - m_fswSymbols;
}

void DSDProtocolNXDN::processBurst()
{
    m_burstValid = false;
    decodeFrame(m_pendingSync);
}

void DSDProtocolNXDN::parseMessage(const uint8_t *message, int nbBytes, int dataBytes)
{
    int type = message[0] & 0x3F;

    if ((type == 0x38) || (type == 0x09)) // SDCALL_REQ_HEADER, DCALL_REQ_HEADER
    {
        // Its packet information is in octets 8 and 9: a SACCH superframe's 9 octets cannot hold it
        if (nbBytes >= 10) {
            parseDataHeader(message);
        }

        m_typeText = type == 0x38 ? "SDCALL" : "DCALL";
    }
    else if ((type == 0x39) || (type == 0x0B)) // SDCALL_REQ_USERDATA, DCALL_REQ_USERDATA
    {
        parseDataBlock(message, nbBytes, dataBytes);
        m_typeText = type == 0x39 ? "SDCALL data" : "DCALL data";
    }
    else if (type == 0x01) // VCALL
    {
        int callType = message[2] >> 5;
        m_group = (callType & 4) == 0;
        m_fullRate = (m_symbolRate == 4800) && ((message[2] & 1) != 0); // voice call option bit 0: EFR
        m_source = ((uint32_t) message[3] << 8) | message[4];
        m_destination = ((uint32_t) message[5] << 8) | message[6];
        m_callValid = true;
        m_typeText = "VCALL";
    }
    else if (type == 0x08) // TX_REL
    {
        m_typeText = "TX_REL";
    }
    else
    {
        char text[16];
        snprintf(text, sizeof(text), "MSG %02X", type);
        m_typeText = text;
    }
}

void DSDProtocolNXDN::parseDataHeader(const uint8_t *message)
{
    // TS 1-A: call type and option, source and destination, cipher, then the packet information:
    // the number of the first data block (they count down to 0), the padding of the last one
    uint8_t bits[80];
    DVBits::bytesToBitsMSB(message, 10, bits);
    int callType = (int) DVBits::getBits(bits, 16, 3);
    DataCall call;
    call.m_headerValid = true;
    call.m_group = (callType & 4) == 0;
    call.m_source = (uint32_t) DVBits::getBits(bits, 24, 16);
    call.m_destination = (uint32_t) DVBits::getBits(bits, 40, 16);
    call.m_cipher = (int) DVBits::getBits(bits, 56, 2);
    call.m_blocks = (int) DVBits::getBits(bits, 68, 4);
    call.m_padOctets = (int) DVBits::getBits(bits, 72, 5);
    m_dataCall = call;

    if (dsdDebug()) {
        fprintf(stderr, "NXDN data call header: %s %u > %u blocks %d pad %d cipher %d\n", call.m_group ? "group" : "unit", call.m_source, call.m_destination, call.m_blocks, call.m_padOctets, call.m_cipher);
    }
}

void DSDProtocolNXDN::parseDataBlock(const uint8_t *message, int nbBytes, int dataBytes)
{
    // Two 4 bit counters (the blocks left, the number of this block; they differ in a selective
    // retry, which is not followed), then the user data
    DataCall& call = m_dataCall;
    int remaining = message[1] >> 4;
    int number = message[1] & 0x0F;

    if (!call.m_headerValid || (dataBytes <= 0) || (remaining != number) || (number > call.m_blocks) || (nbBytes < 2 + dataBytes)) {
        return;
    }

    if (call.m_byteLength == 0)
    {
        call.m_byteLength = dataBytes;
        call.m_data.assign((call.m_blocks + 1) * dataBytes, 0);
    }
    else if (call.m_byteLength != dataBytes)
    {
        return; // the blocks of a call come on one kind of channel
    }

    std::copy(message + 2, message + 2 + dataBytes, call.m_data.begin() + (call.m_blocks - number) * dataBytes);
    call.m_received |= 1u << number;

    if (dsdDebug()) {
        fprintf(stderr, "NXDN data block %d of %d (%d bytes)\n", number, call.m_blocks, dataBytes);
    }

    if (call.m_received == ((2u << call.m_blocks) - 1)) {
        finishDataCall();
    }
}

void DSDProtocolNXDN::finishDataCall()
{
    // All blocks: the CRC-32 (the register all ones over the bits) of the user data is its last
    // four bytes; what a radio puts in the data is its own (NMEA positions, Kenwood formats, plain
    // text): the printable ones are shown as text, the others in hexadecimal
    DataCall& call = m_dataCall;
    int total = (int) call.m_data.size() - call.m_padOctets;
    call.m_headerValid = false;

    if (total < 4) {
        return;
    }

    const uint8_t *data = call.m_data.data();
    std::vector<uint8_t> bits((total - 4) * 8);
    DVBits::bytesToBitsMSB(data, total - 4, bits.data());
    uint32_t received = ((uint32_t) data[total - 4] << 24) | ((uint32_t) data[total - 3] << 16) | ((uint32_t) data[total - 2] << 8) | data[total - 1];

    if (DVCRC::nxdn32(bits.data(), (total - 4) * 8) != received)
    {
        if (dsdDebug()) {
            fprintf(stderr, "NXDN data call: CRC-32 failed (%d bytes)\n", total);
        }

        return;
    }

    m_dataCallCount++;
    TextMessage message;
    message.m_valid = true;
    message.m_group = call.m_group;
    message.m_source = call.m_source;
    message.m_destination = call.m_destination;

    if (call.m_cipher != 0)
    {
        message.m_isText = false;
        message.m_text = "encrypted " + DSDPacketText::hex(data, total - 4);
    }
    else if (DSDPacketText::printable(data, total - 4, message.m_text))
    {
        message.m_isText = true;
    }
    else
    {
        message.m_isText = false;
        message.m_text = DSDPacketText::hex(data, total - 4);
    }

    message.m_revision = ++m_metadataRevision;
    m_textMessage = message;

    if (dsdDebug()) {
        fprintf(stderr, "NXDN data call: %u > %u %s: %s\n", call.m_source, call.m_destination, message.m_isText ? "text" : "data", message.m_text.c_str());
    }
}

bool DSDProtocolNXDN::processUDCH(const float *llr)
{
    // The user data channel takes the SACCH and payload area: structure and RAN, then a 176 bit message
    uint8_t info[DVNXDN::m_udchInfoBits];

    if (!DVNXDN::udchDecodeSoft(llr, info)) {
        return false;
    }

    int ran = 0;

    for (int i = 0; i < 6; i++) {
        ran = (ran << 1) | info[2 + i];
    }

    m_ran = ran;
    uint8_t message[22];
    DVBits::bitsToBytesMSB(info + 8, 176, message);
    parseMessage(message, 22, 20);

    if (dsdDebug()) {
        fprintf(stderr, "NXDN UDCH: RAN %d type %02X [%02X %02X %02X %02X %02X %02X %02X %02X]\n", ran, message[0] & 0x3F, message[0], message[1], message[2], message[3], message[4], message[5], message[6], message[7]);
    }

    return true;
}

void DSDProtocolNXDN::decodeFrame(int64_t syncIndex)
{
    int64_t first = syncIndex + 1;
    int64_t last = syncIndex + m_frameSymbols - m_fswSymbols;

    if (!m_history.has(first) || !m_history.has(last)) {
        return;
    }

    // Descramble everything after the FSW
    float llr[2 * DVNXDN::m_scrambledSymbols];
    m_history.llrs(first, DVNXDN::m_scrambledSymbols, llr);
    DVNXDN::scrambleSoft(llr);

    // LICH: one information bit per symbol, in the MSB
    uint8_t lichBits[16];

    for (int i = 0; i < 16; i++) {
        lichBits[i] = llr[i] < 0.0f;
    }

    int rfChannel, functional, option, direction;
    bool lichOk = DVNXDN::lichDecode(lichBits, rfChannel, functional, option, direction);

    // The outbound trunked control channel (RCCH, RF channel type 0): a CAC after the LICH, its
    // CRC-16 validating the frame (a frame of noise passes it once in 65536)
    if (lichOk && (rfChannel == 0) && (direction == 1))
    {
        bool cacOk = processCAC(llr + 2 * m_lichSymbols);
        m_frameCount++;
        m_good += cacOk;
        m_burstValid = cacOk;
        m_proven = m_proven || cacOk;
        m_lastFrameValid = cacOk;

        if (dsdDebug()) {
            fprintf(stderr, "NXDN frame at %lld: LICH ok (rf 0 func %d option %d) CAC %s: %s\n", syncIndex, functional, option, cacOk ? "ok" : "bad", m_controlText.c_str());
        }

        return;
    }

    // A user data channel frame (LICH functional type 1) has no SACCH: the whole area after the
    // LICH is one coded block; its CRC-15 validates the frame as the SACCH does
    if (lichOk && (functional == 1))
    {
        bool udchOk = processUDCH(llr + 2 * m_lichSymbols);
        m_frameCount++;
        m_good += udchOk;
        m_burstValid = udchOk && (m_proven || m_lastFrameValid);
        m_proven = m_proven || m_burstValid;
        m_lastFrameValid = udchOk;

        if (dsdDebug()) {
            fprintf(stderr, "NXDN frame at %lld: LICH ok (rf %d func %d option %d) UDCH %s\n", syncIndex, rfChannel, functional, option, udchOk ? "ok" : "bad");
        }

        return;
    }

    // SACCH: structure countdown, RAN and a message fragment
    uint8_t info[DVNXDN::m_sacchInfoBits];
    bool sacchOk = DVNXDN::sacchDecodeSoft(llr + 2 * m_lichSymbols, info);
    m_frameCount++;

    if (lichOk || sacchOk) {
        m_good++;
    }

    // A frame checks out when its LICH (a parity bit) and SACCH (a 6 bit CRC) decode; noise
    // passes both once in 128 frames, so the first validation of a lock takes two frames in a row
    bool frameValid = lichOk && sacchOk;
    m_burstValid = frameValid && (m_proven || m_lastFrameValid);
    m_proven = m_proven || m_burstValid;
    m_lastFrameValid = frameValid;

    if (dsdDebug()) {
        fprintf(stderr, "NXDN frame at %lld: LICH %s (rf %d func %d option %d) SACCH %s\n", syncIndex, lichOk ? "ok" : "bad", rfChannel, functional, option, sacchOk ? "ok" : "bad");
    }

    if ((m_frameCount == 1) && !lichOk && !sacchOk)
    {
        // A sync word alone is a weak lock: the first frame must make sense
        m_active = false;
        return;
    }

    if (sacchOk)
    {
        int structure = (info[0] << 1) | info[1];
        int ran = 0;

        for (int i = 0; i < 6; i++) {
            ran = (ran << 1) | info[2 + i];
        }

        m_ran = ran;
        int fragment = 3 - structure;

        if (fragment == 0) {
            m_messageMask = 0;
        }

        for (int i = 0; i < 18; i++)
        {
            int bit = fragment * 18 + i;

            if (info[8 + i]) {
                m_message[bit / 8] |= 0x80 >> (bit % 8);
            } else {
                m_message[bit / 8] &= ~(0x80 >> (bit % 8));
            }
        }

        m_messageMask |= 1 << fragment;

        if (m_messageMask == 0x0F)
        {
            parseMessage(m_message, 9, 0);
            m_messageMask = 0;
        }
    }

    // Payload: voice channels and/or FACCH1 according to the LICH option (3 = 4 VCH, 0 = 2 FACCH1,
    // 1 = FACCH1 then 2 VCH, 2 = 2 VCH then FACCH1). Without a LICH assume voice. A full rate
    // voice call (NXDN96 EFR) carries 2 voice frames of 72 symbols instead of 4 of 36.
    const float *payload = llr + 2 * (m_lichSymbols + m_sacchSymbols);
    int64_t payloadSymbol = first + m_lichSymbols + m_sacchSymbols;

    if (!lichOk) {
        option = 3;
    }

    bool release = false;

    switch (option)
    {
    case 0:
        release = processFACCH(payload);
        release = processFACCH(payload + 2 * m_facchSymbols) || release;
        break;
    case 1:
        release = processFACCH(payload);

        if (m_fullRate) {
            processVoiceFullRate(payload + 2 * m_facchSymbols, payloadSymbol + m_facchSymbols, 1);
        } else {
            processVoice(payload + 2 * m_facchSymbols, payloadSymbol + m_facchSymbols, 2);
        }

        break;
    case 2:
        if (m_fullRate) {
            processVoiceFullRate(payload, payloadSymbol, 1);
        } else {
            processVoice(payload, payloadSymbol, 2);
        }

        release = processFACCH(payload + 2 * m_facchSymbols);
        break;
    default:
        if (m_fullRate) {
            processVoiceFullRate(payload, payloadSymbol, 2);
        } else {
            processVoice(payload, payloadSymbol, 4);
        }

        break;
    }

    if (release) {
        m_active = false;
    }
}

void DSDProtocolNXDN::processVoice(const float *llr, int64_t firstSymbol, int count)
{
    for (int v = 0; v < count; v++)
    {
        int present = 0;

        for (int i = 0; i < m_vchSymbols; i++) {
            present += m_history.at(firstSymbol + v * m_vchSymbols + i).m_present;
        }

        if (present < m_vchSymbols / 2) {
            continue;
        }

        DSDVoiceFrame frame;
        frame.m_sample = m_history.sampleAt(firstSymbol + (v + 1) * m_vchSymbols - 1);
        frame.m_slot = 0;
        frame.m_rate = DSDVoiceFrame::RateAMBE3600x2450;
        frame.m_nbBits = 72;
        const float *p = llr + 2 * v * m_vchSymbols;
        uint8_t bits[72];

        for (int i = 0; i < 72; i++)
        {
            frame.m_llr[i] = p[i];
            bits[i] = p[i] < 0.0f;
        }

        DVBits::bitsToBytesMSB(bits, 72, frame.m_bytes);
        m_frames.push_back(frame);
    }
}

void DSDProtocolNXDN::processVoiceFullRate(const float *llr, int64_t firstSymbol, int count)
{
    // AMBE+2 full rate: 144 bits per 72 symbol voice frame, passed on in the on air order for a
    // hardware vocoder (mbevocoder has no full rate AMBE)
    const int symbols = 2 * m_vchSymbols;

    for (int v = 0; v < count; v++)
    {
        int present = 0;

        for (int i = 0; i < symbols; i++) {
            present += m_history.at(firstSymbol + v * symbols + i).m_present;
        }

        if (present < symbols / 2) {
            continue;
        }

        DSDVoiceFrame frame;
        frame.m_sample = m_history.sampleAt(firstSymbol + (v + 1) * symbols - 1);
        frame.m_slot = 0;
        frame.m_rate = DSDVoiceFrame::RateAMBE7200x4400;
        frame.m_nbBits = 144;
        const float *p = llr + 2 * v * symbols;
        uint8_t bits[144];

        for (int i = 0; i < 144; i++)
        {
            frame.m_llr[i] = p[i];
            bits[i] = p[i] < 0.0f;
        }

        DVBits::bitsToBytesMSB(bits, 144, frame.m_bytes);
        m_frames.push_back(frame);
    }
}

bool DSDProtocolNXDN::processFACCH(const float *llr)
{
    uint8_t info[DVNXDN::m_facch1InfoBits];

    if (!DVNXDN::facch1DecodeSoft(llr, info)) {
        return false;
    }

    uint8_t message[10];
    DVBits::bitsToBytesMSB(info, 80, message);
    parseMessage(message, 10, 8);

    if (dsdDebug()) {
        fprintf(stderr, "NXDN FACCH1: type %02X [%02X %02X %02X %02X %02X %02X %02X %02X %02X]%c", message[0] & 0x3F, message[0], message[1], message[2], message[3], message[4], message[5], message[6], message[7], message[8], 10);
    }

    return (message[0] & 0x3F) == 0x08;
}

bool DSDProtocolNXDN::getCall(bool& group, uint32_t& source, uint32_t& destination) const
{
    group = m_group;
    source = m_source;
    destination = m_destination;
    return m_callValid;
}

bool DSDProtocolNXDN::processCAC(const float *llr)
{
    // SR (structure 2: superframe head, dual message format; RAN 6), 18 octets of layer 3 data: one
    // message, or two of 9 octets in the dual format (which OP25, SDRTrunk and dsd-fme leave half read)
    uint8_t info[DVNXDN::m_cacInfoBits];

    if (!DVNXDN::cacDecodeSoft(llr, info)) {
        return false;
    }

    const int structure = (int) DVBits::getBits(info, 0, 2);
    m_ran = (int) DVBits::getBits(info, 2, 6);
    uint8_t bytes[18];
    DVBits::bitsToBytesMSB(info + 8, 144, bytes);
    const int messages = (structure & 1) ? 2 : 1;

    for (int k = 0; k < messages; k++)
    {
        bool grant;
        std::string site;
        std::string text = controlMessageText(bytes + 9 * k, messages == 2 ? 9 : 18, grant, site);

        if (!site.empty()) {
            m_siteText = site;
        }

        if (!text.empty()) {
            m_controlText = text;
        }

        if (grant) {
            m_grantText = text;
        }
    }

    m_cacCount++;
    m_typeText = "RCCH";
    return true;
}

std::string DSDProtocolNXDN::locationText(uint32_t location)
{
    // Category 00 global (system 10 bits, site 12), 10 regional (14, 8), 01 local (17, 5)
    char text[40];
    const int category = (location >> 22) & 3;

    if (category == 0) {
        snprintf(text, sizeof(text), "SYS G:%u site %u", (location >> 12) & 0x3FF, location & 0xFFF);
    } else if (category == 2) {
        snprintf(text, sizeof(text), "SYS R:%u site %u", (location >> 8) & 0x3FFF, location & 0xFF);
    } else if (category == 1) {
        snprintf(text, sizeof(text), "SYS L:%u site %u", (location >> 5) & 0x1FFFF, location & 0x1F);
    } else {
        snprintf(text, sizeof(text), "location %06X", location);
    }

    return text;
}

std::string DSDProtocolNXDN::controlMessageText(const uint8_t *m, int nbBytes, bool& grant, std::string& site)
{
    // TS 1-A 6.4 (RCCH outbound meanings, not the traffic channel's), cross-checked with SDRTrunk
    // and dsd-fme: bit offsets from the MSB of the message's first octet
    grant = false;
    site.clear();
    const int bits = 8 * nbBytes;
    auto b = [&](int first, int n) -> uint32_t {
        uint32_t v = 0;

        for (int k = first; k < first + n; k++) {
            v = (v << 1) | ((m[k >> 3] >> (7 - (k & 7))) & 1);
        }

        return v;
    };
    const int type = m[0] & 0x3F;
    char text[160];

    switch (type)
    {
    case 0x10:  // IDLE
        return std::string();
    case 0x01:  // VCALL_RESP, VCALL_REC_REQ: call type, source, destination
    case 0x02:
    case 0x04:  // VCALL_ASSGN, VCALL_ASSGN_DUP, DCALL_ASSGN_DUP, DCALL_ASSGN: the channel too
    case 0x05:
    case 0x0D:
    case 0x0E:
    {
        if (bits < 56) {
            break;
        }

        const int callType = (int) b(16, 3);
        const int mode = (int) b(19, 5) & 7;
        const char *target = (callType <= 1) ? "TG" : (callType >= 6) ? "PSTN" : "unit";
        const char *name = type == 0x01 ? "VCALL_RESP" : type == 0x02 ? "VCALL_REC_REQ" : type == 0x04 ? "VCALL_ASSGN"
            : type == 0x05 ? "VCALL_ASSGN_DUP" : type == 0x0D ? "DCALL_ASSGN_DUP" : "DCALL_ASSGN";
        const bool assignment = (type == 0x04) || (type == 0x05) || (type == 0x0D) || (type == 0x0E);

        if (assignment && (bits >= 72))
        {
            snprintf(text, sizeof(text), "%s %s %u from %u ch %u%s", name, target, b(40, 16), b(24, 16), b(62, 10),
                ((type == 0x04) || (type == 0x05)) && (mode == 3) ? " EFR" : "");
            grant = true;
        }
        else
        {
            snprintf(text, sizeof(text), "%s %s %u from %u", name, target, b(40, 16), b(24, 16));
        }

        return text;
    }
    case 0x11:  // DISC: call type, source, destination
        if (bits < 56) {
            break;
        }

        snprintf(text, sizeof(text), "DISC %s %u", b(16, 3) <= 1 ? "TG" : "unit", b(40, 16));
        return text;
    case 0x18:  // SITE_INFO: location, channel structure, services, restrictions, access, version, adjacent sites, control channels
        if (bits < 144) {
            break;
        }

        site = locationText(b(8, 24));
        snprintf(text, sizeof(text), "SITE_INFO %s cc %u %u", site.c_str(), b(124, 10), b(134, 10));
        return text;
    case 0x19:  // SRV_INFO: location, services, restrictions
    {
        if (bits < 48) {
            break;
        }

        site = locationText(b(8, 24));
        static const char *const services[12] = { "multi-site", "multi-system", "location registration", "group registration",
            "authentication", "composite control", "voice", "data", "short data", "status", "PSTN", "IP" };
        std::string s = "SRV_INFO " + site;
        const uint32_t flags = b(32, 16);

        for (int i = 0; i < 12; i++)
        {
            if (flags & (0x8000 >> i)) {
                s += std::string(" ") + services[i];
            }
        }

        return s;
    }
    case 0x1A:  // CCH_INFO: location, current/new/add/delete, control channels
        if (bits < 64) {
            break;
        }

        snprintf(text, sizeof(text), "CCH_INFO %s %s cc %u%s", locationText(b(8, 24)).c_str(),
            b(32, 1) ? "current" : b(33, 1) ? "new" : b(34, 1) ? "add" : b(35, 1) ? "delete" : "-", b(38, 10),
            b(32, 1) ? (" " + std::to_string(b(54, 10))).c_str() : "");
        return text;
    case 0x1B:  // ADJ_SITE_INFO: entries of location, site number, channel (1 in a dual frame, 3 in a single)
    {
        std::string s = "ADJ_SITE_INFO";

        for (int i = 0; 8 + 40 * (i + 1) <= bits; i++)
        {
            const uint32_t location = b(8 + 40 * i, 24);

            if (location == 0) {
                continue;
            }

            snprintf(text, sizeof(text), " %u: %s ch %u", b(34 + 40 * i, 4), locationText(location).c_str(), b(38 + 40 * i, 10));
            s += text;
        }

        return s;
    }
    case 0x1C:  // FAIL_STAT_INFO
        return "FAIL_STAT_INFO " + (bits >= 32 ? locationText(b(8, 24)) : std::string()) + " fail-soft";
    case 0x20:  // REG_RESP: option, home location, unit, group, cause
    case 0x24:  // GRP_REG_RESP: option, unit, group, cause
    {
        const bool group = type == 0x24;

        if (bits < (group ? 56 : 72)) {
            break;
        }

        const uint32_t cause = group ? b(48, 8) : b(64, 8);
        snprintf(text, sizeof(text), "%s unit %u TG %u %s", group ? "GRP_REG_RESP" : "REG_RESP", group ? b(16, 16) : b(32, 16), group ? b(32, 16) : b(48, 16),
            cause == 0x01 ? "accepted" : (cause == 0x06) || (cause == 0x08) ? "refused" : (cause == 0x04) || (cause == 0x05) ? "group refused" : "");
        return text;
    }
    default:
        break;
    }

    snprintf(text, sizeof(text), "RCCH message %02X", type);
    return text;
}

std::string DSDProtocolNXDN::getStatusText() const
{
    char text[160];

    if (m_cacCount > 0)
    {
        // A control channel: the site, then the last assignment (else the last message)
        snprintf(text, sizeof(text), "RAN %d RCCH %s%s%s", m_ran, m_siteText.c_str(), m_siteText.empty() ? "" : " | ",
            !m_grantText.empty() ? m_grantText.c_str() : m_controlText.c_str());
        return text;
    }

    if (m_callValid) {
        snprintf(text, sizeof(text), "RAN %d %s %s%s %u > %u", m_ran, m_typeText.c_str(), m_fullRate ? "EFR " : "", m_group ? "group" : "unit", m_source, m_destination);
    } else if (m_ran >= 0) {
        snprintf(text, sizeof(text), "RAN %d %s", m_ran, m_typeText.c_str());
    } else {
        snprintf(text, sizeof(text), "%s", m_typeText.c_str());
    }

    return text;
}
