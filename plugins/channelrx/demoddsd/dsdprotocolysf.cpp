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
#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool dsdDebug() { static int d = getenv("DSD_DEBUG") ? 1 : 0; return d != 0; }

#include "dsp/dv/dvambe.h"
#include "dsp/dv/dvbits.h"

#include "dsdprotocolysf.h"

DSDProtocolYSF::DSDProtocolYSF(const DSDSymbolHistory& history) :
    DSDProtocol(history),
    m_active(false),
    m_syncIndex(-1),
    m_pendingSync(-1),
    m_misses(0),
    m_silent(0),
    m_absent(0),
    m_burstValid(false),
    m_fichValid(false),
    m_normalEnd(false),
    m_dataFRActive(false),
    m_gps{},
    m_gpsMask(0),
    m_vd1Data{},
    m_vd1DataMask(0)
{
}

std::vector<DVSync::Pattern> DSDProtocolYSF::getSearchPatterns() const
{
    return { DVSync::YSF };
}

std::string DSDProtocolYSF::callsign(const uint8_t *bytes)
{
    std::string s;

    for (int i = 0; i < 10; i++)
    {
        char c = (char) bytes[i];
        s += ((c >= 32) && (c < 127)) ? c : '?';
    }

    while (!s.empty() && (s.back() == ' ')) {
        s.pop_back();
    }

    return s;
}

void DSDProtocolYSF::start(DVSync::Pattern pattern, int64_t syncIndex)
{
    (void) pattern;
    m_active = true;
    m_misses = 0;
    m_silent = 0;
    m_absent = 0;
    m_fichValid = false;
    m_normalEnd = false;
    m_protocolFrames.clear();
    m_source.clear();
    m_destination.clear();
    m_downlink.clear();
    m_uplink.clear();

    for (std::string& r : m_rem) {
        r.clear();
    }

    m_dataFR.clear();
    m_dataFRValid.clear();
    m_dataFRActive = false;
    m_gpsMask = 0;
    m_vd1DataMask = 0;
    m_syncIndex = syncIndex;
}

void DSDProtocolYSF::resync(DVSync::Pattern pattern, int64_t syncIndex)
{
    (void) pattern;
    m_misses = 0;
    m_silent = 0;
    m_syncIndex = syncIndex;
}

bool DSDProtocolYSF::getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const
{
    if (!m_active) {
        return false;
    }

    syncIndex = m_syncIndex + m_frameDibits;
    patterns = { DVSync::YSF };
    return true;
}

void DSDProtocolYSF::getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const
{
    first = syncIndex - m_syncDibits + 1;
    last = syncIndex;
}

bool DSDProtocolYSF::confirmWithoutSync(int64_t syncIndex, bool present)
{
    if (!present)
    {
        if (++m_silent >= 3) {
            m_active = false;
        }

        m_syncIndex = syncIndex;
        return true;
    }

    // The sync is always there: check it as hard dibits
    const DVSync::Info& info = DVSync::get(DVSync::YSF);
    int64_t first = syncIndex - info.m_length + 1;
    int errors = 0;

    if (m_history.has(first) && m_history.has(syncIndex))
    {
        for (int i = 0; i < info.m_length; i++) {
            errors += m_history.at(first + i).m_dibit != DVSync::levelToDibit(info.m_levels[i]);
        }
    }
    else
    {
        errors = info.m_length;
    }

    if (errors <= 4)
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

void DSDProtocolYSF::end()
{
    m_active = false;
}

bool DSDProtocolYSF::pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex)
{
    (void) sampleIndex;

    if (!m_active) {
        return false;
    }

    m_absent = symbol.m_present ? 0 : m_absent + 1;

    if (m_absent >= m_frameDibits)
    {
        m_active = false;
        return false;
    }

    if (index == m_syncIndex + m_frameDibits - m_syncDibits)
    {
        m_pendingSync = m_syncIndex;
        return true;
    }

    return false;
}

void DSDProtocolYSF::getBurstSpan(int64_t& first, int64_t& last) const
{
    first = m_pendingSync - m_syncDibits + 1;
    last = m_pendingSync + m_frameDibits - m_syncDibits;
}

void DSDProtocolYSF::processBurst()
{
    m_burstValid = false;
    decodeFrame(m_pendingSync);
}

void DSDProtocolYSF::decodeFrame(int64_t syncIndex)
{
    int64_t fichStart = syncIndex + 1;
    int64_t payload = fichStart + m_fichDibits;

    if (!m_history.has(fichStart) || !m_history.has(payload + m_payloadDibits - 1)) {
        return;
    }

    float llr[2 * m_fichDibits];
    m_history.llrs(fichStart, m_fichDibits, llr);
    DVYSF::FICH fich;
    int corrected = DVYSF::fichDecodeSoft(llr, fich);

    if (dsdDebug()) {
        fprintf(stderr, "YSF frame at %lld: FICH %s FI %d DT %d FN %d FT %d\n", syncIndex, corrected >= 0 ? "ok" : "failed", fich.m_fi, fich.m_dt, fich.m_fn, fich.m_ft);
    }

    if (corrected < 0)
    {
        // Without the FICH the payload cannot be interpreted; keep the last one for voice if it
        // was a communication frame of the same type
        if (!m_fichValid || (m_fich.m_fi != 1)) {
            return;
        }

        fich = m_fich;
        fich.m_fn = (m_fich.m_fn + 1) % (m_fich.m_ft + 1);
    }

    m_fich = fich;
    m_fichValid = true;
    m_burstValid = corrected >= 0;

    if (corrected >= 0)
    {
        uint8_t bits[2 * m_frameDibits];
        uint8_t bytes[120];
        const int64_t first = syncIndex - m_syncDibits + 1;
        m_history.bits(first, m_frameDibits, bits);
        DVBits::bitsToBytesMSB(bits, 2 * m_frameDibits, bytes);
        DSDProtocolFrame frame;
        frame.m_sample = m_history.sampleAt(first + m_frameDibits - 1);
        frame.m_format = "ysf-frame";
        frame.m_type = fich.m_fi == 2 ? "end" : "frame";
        frame.m_sequence = fich.m_fn;
        frame.m_data.assign(bytes, bytes + sizeof(bytes));
        m_protocolFrames.push_back(frame);
    }

    switch (fich.m_fi)
    {
    case 0: // header
        processHeader(payload);
        break;
    case 1: // communication
        if (fich.m_dt == 2) {
            processVD2(payload, fich.m_fn, syncIndex);
        } else if (fich.m_dt == 1) {
            processDataFR(payload, fich);
        } else if (fich.m_dt == 0) {
            processVD1(payload, fich.m_fn);
        } else {
            processVoiceFullRate(payload, fich);
        }
        break;
    case 2: // terminator
        processHeader(payload);
        finishDataFR();
        m_normalEnd = true;
        m_active = false;
        break;
    default:
        break;
    }
}

void DSDProtocolYSF::decodeDCHPair(int64_t payload, uint8_t *dch1, bool& ok1, uint8_t *dch2, bool& ok2)
{
    // 36 dibit chunks alternating between the two data channels
    float llr1[2 * DVYSF::m_headerDCHDibits];
    float llr2[2 * DVYSF::m_headerDCHDibits];

    for (int chunk = 0; chunk < 5; chunk++)
    {
        m_history.llrs(payload + 72 * chunk, 36, llr1 + 72 * chunk);
        m_history.llrs(payload + 72 * chunk + 36, 36, llr2 + 72 * chunk);
    }

    ok1 = DVYSF::dchDecodeSoft(llr1, 20, dch1);
    ok2 = DVYSF::dchDecodeSoft(llr2, 20, dch2);
}

void DSDProtocolYSF::processHeader(int64_t payload)
{
    uint8_t csd1[20], csd2[20];
    bool ok1, ok2;
    decodeDCHPair(payload, csd1, ok1, csd2, ok2);

    if (ok1)
    {
        m_destination = callsign(csd1);
        m_source = callsign(csd1 + 10);
    }

    if (ok2)
    {
        m_downlink = callsign(csd2);
        m_uplink = callsign(csd2 + 10);
    }
}

void DSDProtocolYSF::voiceFrame(int64_t first, int nbDibits)
{
    int present = 0;

    for (int i = 0; i < nbDibits; i++) {
        present += m_history.at(first + i).m_present;
    }

    if (present < nbDibits / 2) {
        return;
    }

    DSDVoiceFrame frame;
    frame.m_sample = m_history.sampleAt(first + nbDibits - 1);
    frame.m_slot = 0;
    uint8_t bits[144];
    float llr[144];
    m_history.bits(first, nbDibits, bits);
    m_history.llrs(first, nbDibits, llr);

    if (nbDibits == 36)
    {
        // AMBE+2 3600x2450, 72 bits in DMR's order (mbevocoder's MBE_LAYOUT_DMR serves both)
        frame.m_rate = DSDVoiceFrame::RateAMBE3600x2450;
        frame.m_nbBits = 72;
        std::copy(llr, llr + 72, frame.m_llr);
        DVBits::bitsToBytesMSB(bits, 72, frame.m_bytes);
    }
    else
    {
        // IMBE 7200x4400 in P25 Phase 1's order, as the vocoders take it
        frame.m_rate = DSDVoiceFrame::RateIMBE7200x4400;
        frame.m_nbBits = 144;
        uint8_t p25[144];
        DVYSF::imbeYSFToP25(bits, p25);
        DVYSF::imbeYSFToP25(llr, frame.m_llr);
        DVBits::bitsToBytesMSB(p25, 144, frame.m_bytes);
    }

    m_frames.push_back(frame);
}

void DSDProtocolYSF::processVD1(int64_t payload, int fn)
{
    // V/D type 1 (MMDVMHost YSFPayload processVDMode1Data, dsdcc processVD1): five blocks of a 36
    // dibit slice of the data channel and a 36 dibit voice frame
    float dchLlr[2 * DVYSF::m_headerDCHDibits];

    for (int block = 0; block < 5; block++)
    {
        const int64_t p = payload + 72 * block;
        m_history.llrs(p, 36, dchLlr + 72 * block);
        voiceFrame(p + 36, 36);
    }

    uint8_t data[20];

    if (fn == 0) {
        m_vd1DataMask = 0;
    }

    if (!DVYSF::dchDecodeSoft(dchLlr, 20, data)) {
        return;
    }

    switch (fn)
    {
    case 0:     // CSD1
        m_destination = callsign(data);
        m_source = callsign(data + 10);
        break;
    case 1:     // CSD2
        m_downlink = callsign(data);
        m_uplink = callsign(data + 10);
        break;
    case 2:     // CSD3
        setRemarks(data, 0);
        setRemarks(data + 10, 2);
        break;
    default:    // DT1 to DT5: 100 bytes, a GPS message among them
    {
        std::copy(data, data + 20, m_vd1Data + 20 * (fn - 3));
        m_vd1DataMask |= 1 << (fn - 3);
        const int length = (m_vd1DataMask & 1) ? messageLength(m_vd1Data, 20 * (fn - 2)) : -1;
        Position p;

        if ((fn == 7) && (length > 0) && decodeGPS(m_vd1Data, length, p))
        {
            p.m_valid = true;
            p.m_source = m_source;
            p.m_revision = m_position.m_revision + 1;
            m_position = p;
        }

        break;
    }
    }
}

void DSDProtocolYSF::processVoiceFullRate(int64_t payload, const DVYSF::FICH& fich)
{
    // Voice full rate (MMDVMHost YSFControl): the first frame after the header (FN 0 and FT 1) is
    // CSD3 (180 dibits of the data channel), 36 dibits reserved and two voice frames; the others
    // five voice frames of 72 dibits
    if ((fich.m_fn == 0) && (fich.m_ft == 1))
    {
        float llr[2 * DVYSF::m_headerDCHDibits];
        m_history.llrs(payload, DVYSF::m_headerDCHDibits, llr);
        uint8_t csd3[20];

        if (DVYSF::dchDecodeSoft(llr, 20, csd3))
        {
            setRemarks(csd3, 0);
            setRemarks(csd3 + 10, 2);
        }

        voiceFrame(payload + 216, 72);
        voiceFrame(payload + 288, 72);
        return;
    }

    for (int v = 0; v < 5; v++) {
        voiceFrame(payload + 72 * v, 72);
    }
}

void DSDProtocolYSF::setRemarks(const uint8_t *bytes, int first)
{
    // Two of CSD3's five character remark fields
    for (int r = 0; r < 2; r++)
    {
        std::string s;

        for (int i = 0; i < 5; i++)
        {
            char c = (char) bytes[5 * r + i];
            s += ((c >= 32) && (c < 127)) ? c : ' ';
        }

        while (!s.empty() && (s.back() == ' ')) {
            s.pop_back();
        }

        m_rem[first + r] = s;
    }
}

std::string DSDProtocolYSF::getRemarks() const
{
    std::string s;

    for (const std::string& r : m_rem)
    {
        if (!r.empty()) {
            s += (s.empty() ? "" : " ") + r;
        }
    }

    return s;
}

void DSDProtocolYSF::processDataFR(int64_t payload, const DVYSF::FICH& fich)
{
    // Data full rate (MMDVMHost YSFPayload processDataFRModeData, YSFGateway WiresX): FN 0 carries
    // CSD1 and CSD2, FN 1 CSD3 and the message's first 20 bytes, FN n its bytes 20 + 40 (n - 2) and
    // 40 + 40 (n - 2); block BN holds bytes 260 BN on, and the message ends at FN = FT of BN = BT
    uint8_t dch1[20], dch2[20];
    bool ok1, ok2;
    decodeDCHPair(payload, dch1, ok1, dch2, ok2);
    const int block = m_dataBlockBytes * std::min(fich.m_bn, m_maxDataBlocks - 1);

    if (fich.m_fn == 0)
    {
        if (fich.m_bn == 0)
        {
            finishDataFR();     // (a message not ended: cut short)
            m_dataFRActive = true;
        }

        if (ok1)
        {
            m_destination = callsign(dch1);
            m_source = callsign(dch1 + 10);
        }

        if (ok2)
        {
            m_downlink = callsign(dch2);
            m_uplink = callsign(dch2 + 10);
        }
    }
    else if (fich.m_fn == 1)
    {
        if (ok1)
        {
            setRemarks(dch1, 0);
            setRemarks(dch1 + 10, 2);
        }

        putDataFR(block, dch2, ok2);
    }
    else
    {
        putDataFR(block + 20 + 40 * (fich.m_fn - 2), dch1, ok1);
        putDataFR(block + 40 + 40 * (fich.m_fn - 2), dch2, ok2);
    }

    if ((fich.m_fn == fich.m_ft) && (fich.m_bn >= fich.m_bt)) {
        finishDataFR();
    }
}

void DSDProtocolYSF::putDataFR(int offset, const uint8_t *bytes, bool ok)
{
    m_dataFRActive = true;

    if ((offset < 0) || (offset + 20 > m_maxDataBlocks * m_dataBlockBytes)) {
        return;
    }

    if ((int) m_dataFR.size() < offset + 20)
    {
        m_dataFR.resize(offset + 20, 0);
        m_dataFRValid.resize((offset + 20) / 20, false);
    }

    if (ok) {
        std::copy(bytes, bytes + 20, m_dataFR.begin() + offset);
    }

    m_dataFRValid[offset / 20] = ok;
}

void DSDProtocolYSF::finishDataFR()
{
    if (!m_dataFRActive || m_dataFR.empty())
    {
        m_dataFRActive = false;
        return;
    }

    bool complete = true;

    for (bool ok : m_dataFRValid) {
        complete = complete && ok;
    }

    // The later blocks' leading 0x00 fillers out
    std::vector<uint8_t> bytes;

    for (int i = 0; i < (int) m_dataFR.size(); i++)
    {
        if ((i == 0) || (i % m_dataBlockBytes != 0)) {
            bytes.push_back(m_dataFR[i]);
        }
    }

    const int length = messageLength(bytes.data(), (int) bytes.size());
    DataMessage& d = m_dataMessage;
    d.m_valid = true;
    d.m_source = m_source;
    d.m_destination = m_destination;

    if (length > 0)
    {
        d.m_complete = complete;
        d.m_text = describe(bytes.data(), length, d.m_isText, d.m_kind);
    }
    else
    {
        // No end marker that checks: its bytes as they came (the first 40)
        d.m_complete = false;
        d.m_isText = false;
        d.m_kind = "data";
        d.m_text.clear();

        for (int i = 0; i < std::min(40, (int) bytes.size()); i++)
        {
            char hex[4];
            snprintf(hex, sizeof(hex), i ? " %02X" : "%02X", bytes[i]);
            d.m_text += hex;
        }
    }

    d.m_revision++;
    m_dataFR.clear();
    m_dataFRValid.clear();
    m_dataFRActive = false;

    if (dsdDebug()) {
        fprintf(stderr, "YSF data FR: %s%s: %s\n", d.m_kind.c_str(), d.m_complete ? "" : " (cut short)", d.m_text.c_str());
    }
}

int DSDProtocolYSF::messageLength(const uint8_t *bytes, int n)
{
    // The last 0x03 followed by the 8 bit sum of everything up to it (YSFGateway: CCRC::addCRC)
    for (int i = n - 2; i >= 4; i--)
    {
        if (bytes[i] != 0x03) {
            continue;
        }

        uint8_t sum = 0;

        for (int k = 0; k <= i; k++) {
            sum += bytes[k];
        }

        if (sum == bytes[i + 1]) {
            return i + 2;
        }
    }

    return -1;
}

const char *DSDProtocolYSF::radioName(int model)
{
    switch (model)
    {
    case 0x20: return "DR-2X";
    case 0x24: return "FT-1D";
    case 0x25: return "FTM-400D";
    case 0x26: return "DR-1X";
    case 0x27: return "FT-991A";
    case 0x28: return "FT-2D";
    case 0x29: return "FTM-100D";
    case 0x2B: return "FT-70D";
    case 0x30: return "FT-3D";
    case 0x31: return "FTM-300D";
    case 0x33: return "FT-5D";
    default: return nullptr;
    }
}

bool DSDProtocolYSF::decodeGPS(const uint8_t *m, int n, Position& position)
{
    // "b_ (22 62 5F), the radio at [4], then APRS Mic-E (YSFGateway GPS.cpp): [5..10] the latitude's
    // six digits (0-9, A-J or P-Y; K, L and Z an ambiguous digit), their high nibbles the north,
    // longitude offset and west flags at [8], [9] and [10]; [11..13] the longitude's degrees,
    // minutes and hundredths + 28; [14..16] speed and course
    if ((n < 17) || (m[1] != 0x22) || (m[2] != 0x62) || (m[3] != 0x5F)) {
        return false;
    }

    int d[6];

    for (int i = 0; i < 6; i++)
    {
        const uint8_t c = m[5 + i];

        if ((c >= '0') && (c <= '9')) {
            d[i] = c - '0';
        } else if ((c >= 'A') && (c <= 'J')) {
            d[i] = c - 'A';
        } else if ((c >= 'P') && (c <= 'Y')) {
            d[i] = c - 'P';
        } else if ((c == 'K') || (c == 'L') || (c == 'Z')) {
            d[i] = 0;
        } else {
            return false;
        }
    }

    const bool north = m[8] >= 'P';
    const bool offset = m[9] >= 'P';
    const bool west = m[10] >= 'P';
    double latitude = d[0] * 10 + d[1] + (d[2] * 10 + d[3] + (d[4] * 10 + d[5]) / 100.0) / 60.0;
    int degrees = m[11] - 28 + (offset ? 100 : 0);

    if ((degrees >= 180) && (degrees <= 189)) {
        degrees -= 80;
    } else if ((degrees >= 190) && (degrees <= 199)) {
        degrees -= 190;
    }

    int minutes = m[12] - 28;

    if (minutes >= 60) {
        minutes -= 60;
    }

    const int hundredths = m[13] - 28;

    if ((latitude > 90.0) || (degrees < 0) || (degrees > 180) || (minutes < 0) || (hundredths < 0) || (hundredths > 99)) {
        return false;
    }

    double longitude = degrees + (minutes + hundredths / 100.0) / 60.0;
    int speed = (m[14] - 28) * 10 + (m[15] - 28) / 10;
    int course = ((m[15] - 28) % 10) * 100 + (m[16] - 28);

    if (speed >= 800) {
        speed -= 800;
    }

    if (course >= 400) {
        course -= 400;
    }

    position.m_latitude = north ? latitude : -latitude;
    position.m_longitude = west ? -longitude : longitude;
    position.m_speedKnots = speed >= 0 ? speed : -1;
    position.m_course = (course >= 0) && (course < 360) ? course : -1;
    position.m_radio = m[4];
    return true;
}

// Printable ASCII of bytes [first, last), others as '.', trailing spaces off
static std::string asciiField(const uint8_t *m, int length, int first, int last)
{
    std::string s;

    for (int i = first; (i < last) && (i < length); i++) {
        s += ((m[i] >= 32) && (m[i] < 127)) ? (char) m[i] : '.';
    }

    while (!s.empty() && (s.back() == ' ')) {
        s.pop_back();
    }

    while (!s.empty() && (s.front() == ' ')) {
        s.erase(s.begin());
    }

    return s;
}

std::string DSDProtocolYSF::describe(const uint8_t *m, int length, bool& isText, std::string& kind)
{
    // The opcodes of YSFGateway (WiresX.cpp) and, for messages and pictures, EA7EE's MMDVM_CM
    const int end = length - 2;     // the 0x03 end marker
    isText = false;
    const uint32_t op = ((uint32_t) m[1] << 16) | ((uint32_t) m[2] << 8) | m[3];
    const bool reply = m[4] == 0x26;
    const char *radio = radioName(m[4]);
    std::string from = (!reply && radio) ? std::string(" (") + radio + ")" : std::string();
    static const char *const status[] = { "offline", "busy", "disconnected", "connected" };

    switch (op)
    {
    case 0x5D715F:
        kind = reply ? "Wires-X" : "Wires-X DX request";
        return kind + from;
    case 0x5D235F:
        kind = "Wires-X connect request";
        return "connect to room " + asciiField(m, end, 5, 10) + from;
    case 0x5D2A5F:
        kind = "Wires-X disconnect request";
        return "disconnect" + from;
    case 0x5D665F:
        kind = "Wires-X room list request";
        return asciiField(m, end, 5, 7) == "11" ? "search \"" + asciiField(m, end, 10, 26) + "\"" + from
            : "all rooms from " + asciiField(m, end, 7, 10) + from;
    case 0x5D675F:
        kind = "Wires-X category request";
        return "category" + from;
    case 0x5D635F:
        kind = "Wires-X news request";
        return "news of " + asciiField(m, end, 5, 10) + from;
    case 0x5D515F:
    case 0x5D415F:
    {
        // DX and connect replies: the node, then its room
        kind = op == 0x5D515F ? "Wires-X DX reply" : "Wires-X connect reply";
        std::string s = "node " + asciiField(m, end, 5, 10) + " " + asciiField(m, end, 10, 20) + " \"" + asciiField(m, end, 20, 34) + "\"";
        const int st = (end > 35) ? m[35] - '0' : -1;

        if ((st >= 0) && (st < 4)) {
            s += std::string(", ") + status[st];
        }

        if (end >= 60) {
            s += ", room " + asciiField(m, end, 36, 41) + " \"" + asciiField(m, end, 41, 57) + "\" " + asciiField(m, end, 57, 60) + " users";
        }

        return s;
    }
    case 0x5D465F:
    {
        // Room list: records of 50 bytes from 29, '5' or '1', ID (5), name (16), users (3)
        kind = "Wires-X room list";
        std::string s = "node " + asciiField(m, end, 7, 12) + " " + asciiField(m, end, 12, 22) + ":";
        int rooms = 0;

        for (int r = 29; (r + 25 <= end) && (rooms < 20) && ((m[r] == '5') || (m[r] == '1')); r += 50, rooms++) {
            s += (rooms ? "; " : " ") + asciiField(m, end, r + 1, r + 6) + " " + asciiField(m, end, r + 6, r + 22) + " (" + asciiField(m, end, r + 22, r + 25) + ")";
        }

        return s;
    }
    default:
        break;
    }

    if ((op == 0x47655F) || (op == 0x47665F))
    {
        // A text message: an 18 byte GPS block first when it has one (Gf_), then serial (6), two
        // times (12 each), the room (5), 10 bytes, and 80 characters of text
        const int body = 5 + (op == 0x47665F ? 18 : 0);
        kind = "text message";
        isText = true;
        return asciiField(m, end, body + 45, body + 125);
    }

    if ((op == 0x47675F) || (op == 0x47685F))
    {
        const int body = 5 + (op == 0x47685F ? 18 : 0);
        kind = "picture upload";
        return "picture \"" + asciiField(m, end, body + 45, body + 61) + "\"" + from;
    }

    if ((op == 0x4E625F) || (op == 0x4E635F) || (op == 0x4E655F))
    {
        kind = op == 0x4E625F ? "picture data" : op == 0x4E635F ? "picture begin" : "picture end";
        return kind + " " + std::to_string(end > 7 ? m[7] : 0);
    }

    if (op == 0x22625F)
    {
        Position p;
        kind = "position";

        if (decodeGPS(m, end, p))
        {
            char text[96];
            snprintf(text, sizeof(text), "%.5f %.5f%s", p.m_latitude, p.m_longitude, from.c_str());
            return text;
        }

        return "position (not decoded)" + from;
    }

    if (op == 0x22615F)
    {
        kind = "position";
        return "no position" + from;
    }

    // Anything else: its opcode and body as printable ASCII
    kind = "data";
    return asciiField(m, end, 1, 4) + " " + asciiField(m, end, 4, end);
}

void DSDProtocolYSF::processVD2(int64_t payload, int fn, int64_t syncIndex)
{
    (void) syncIndex;
    float dchLlr[2 * DVYSF::m_vd2DCHDibits];

    for (int block = 0; block < 5; block++)
    {
        int64_t p = payload + 72 * block;
        m_history.llrs(p, 20, dchLlr + 40 * block);
        float voiceLlr[2 * DVYSF::m_vd2VoiceDibits];
        m_history.llrs(p + 20, DVYSF::m_vd2VoiceDibits, voiceLlr);
        int present = 0;

        for (int i = 0; i < DVYSF::m_vd2VoiceDibits; i++) {
            present += m_history.at(p + 20 + i).m_present;
        }

        if (present < DVYSF::m_vd2VoiceDibits / 2) {
            continue;
        }

        uint8_t params[49];
        int outvoted = 0;
        DVYSF::vd2VoiceDecodeSoft(voiceLlr, params, &outvoted);
        DSDVoiceFrame frame;
        frame.m_channelErrors = outvoted;   // of the 81 repeated bits, the only ones a code checks
        frame.m_channelBits = 81;
        frame.m_sample = m_history.sampleAt(p + 72 - 1);
        frame.m_slot = 0;
        frame.m_rate = DSDVoiceFrame::RateAMBE2450;
        frame.m_nbBits = 49;
        DVAMBE::paramsToDvsi(params, frame.m_bytes);
        DVBits::bitsToLlr(params, 49, frame.m_llr, 10.0f);
        m_frames.push_back(frame);
    }

    uint8_t data[10];

    if (DVYSF::dchDecodeSoft(dchLlr, 10, data))
    {
        switch (fn)
        {
        case 0: m_destination = callsign(data); m_gpsMask = 0; break;
        case 1: m_source = callsign(data); break;
        case 2: m_downlink = callsign(data); break;
        case 3: m_uplink = callsign(data); break;
        case 4: setRemarks(data, 0); break;     // Rem1, Rem2
        case 5: setRemarks(data, 2); break;     // Rem3, Rem4
        case 6: std::copy(data, data + 10, m_gps); m_gpsMask |= 1; break;
        case 7: std::copy(data, data + 10, m_gps + 10); m_gpsMask |= 2; break;
        default: break;
        }

        // DT1 and DT2: the radio's GPS position, when it sends one
        const int length = (m_gpsMask == 3) ? messageLength(m_gps, 20) : -1;

        if ((fn == 7) && (length > 0))
        {
            Position p;

            if (decodeGPS(m_gps, length, p))
            {
                p.m_valid = true;
                p.m_source = m_source;
                p.m_revision = m_position.m_revision + 1;
                m_position = p;
            }

            m_gpsMask = 0;
        }
    }
}

std::string DSDProtocolYSF::getStatusText() const
{
    static const char *dataTypes[4] = { "V/D 1", "data FR", "V/D 2", "voice FR" };
    std::string s = m_fichValid ? std::string(dataTypes[m_fich.m_dt & 3]) : std::string("no FICH");

    if (!m_source.empty()) {
        s += " src: " + m_source;
    }

    if (!m_destination.empty()) {
        s += " dest: " + m_destination;
    }

    if (!m_downlink.empty()) {
        s += " down: " + m_downlink;
    }

    if (!m_uplink.empty()) {
        s += " up: " + m_uplink;
    }

    return s;
}
