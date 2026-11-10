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
#include <cstring>

#include <QDebug>
#include <QString>

#include "dsp/dv/dvbits.h"
#include "dsp/dv/dvp25.h"
#include "dsp/dv/dvcrc.h"

#include "dsemodp25.h"

DSEModP25::DSEModP25() :
    m_state(StateIdle),
    m_stopRequested(false),
    m_nac(0x293),
    m_talkGroup(1),
    m_sourceId(1),
    m_lsd(0),
    m_voiceFrameCount(0),
    m_confirmedData(true)
{
    std::fill(m_lc, m_lc + 9, 0);
    std::fill(m_mi, m_mi + 9, 0);
    std::fill(m_lcBitsEncoded, m_lcBitsEncoded + m_lcBits, 0);
    std::fill(m_essBitsEncoded, m_essBitsEncoded + m_lcBits, 0);
    std::fill(m_lsdBitsLDU1, m_lsdBitsLDU1 + m_lsdBits, 0);
    std::fill(m_lsdBitsLDU2, m_lsdBitsLDU2 + m_lsdBits, 0);
}

DSEModP25::~DSEModP25()
{
}

// ---- Field encoders ------------------------------------------------------------------------------

void DSEModP25::buildLC(const DSEModSettings& settings, uint8_t *lc9)
{
    // Group voice channel user (TIA-102.AABF LCO 0): LCF = P(0) SF(0) LCO(000000)
    lc9[0] = 0x00; // LCF
    lc9[1] = 0x00; // MFID: standard
    lc9[2] = 0x00; // Service options: not emergency, not protected, half duplex, circuit mode, priority 0
    lc9[3] = 0x00; // Reserved
    lc9[4] = (settings.m_p25TalkGroup >> 8) & 0xFF;
    lc9[5] = settings.m_p25TalkGroup & 0xFF;
    lc9[6] = (settings.m_p25SourceId >> 16) & 0xFF;
    lc9[7] = (settings.m_p25SourceId >> 8) & 0xFF;
    lc9[8] = settings.m_p25SourceId & 0xFF;
}

void DSEModP25::encodeLC(const uint8_t *lc9, uint8_t *bits240)
{
    DVP25::lcEncode(lc9, bits240);
}

void DSEModP25::encodeESS(const uint8_t *mi9, uint8_t algid, uint16_t kid, uint8_t *bits240)
{
    DVP25::essEncode(mi9, algid, kid, bits240);
}

void DSEModP25::encodeHeader(const uint8_t *mi9, uint8_t mfid, uint8_t algid, uint16_t kid, uint16_t tgid, uint8_t *bits648)
{
    DVP25::headerEncode(mi9, mfid, algid, kid, tgid, bits648);
}

void DSEModP25::encodeLSD(uint16_t lsd, uint8_t *bits32)
{
    DVP25::lsdEncode(lsd, bits32);
}

void DSEModP25::encodeTDULC(const uint8_t *lc9, uint8_t *bits288)
{
    DVP25::tdulcEncode(lc9, bits288);
}

// ---- Data unit builders ----------------------------------------------------------------------------

int DSEModP25::putSyncNID(int nac, int duid, uint8_t *bits)
{
    int pos = DVBits::putBits(bits, 0, m_frameSync, m_syncBits);
    return DVBits::putBits(bits, pos, DVP25::nidEncode(nac, duid), m_nidBits);
}

int DSEModP25::buildHDU(int nac, const uint8_t *mi9, uint8_t mfid, uint8_t algid, uint16_t kid, uint16_t tgid, uint8_t *bits)
{
    int pos = putSyncNID(nac, DUIDHDU, bits);
    encodeHeader(mi9, mfid, algid, kid, tgid, &bits[pos]);
    return pos + m_headerBits;
}

int DSEModP25::buildLDU(int nac, int duid, const uint8_t *imbe, const uint8_t *encoded240, const uint8_t *lsd32, uint8_t *bits)
{
    int pos = putSyncNID(nac, duid, bits);

    for (int i = 0; i < m_imbeFramesPerLDU; i++)
    {
        std::copy(imbe + i * m_imbeBits, imbe + (i + 1) * m_imbeBits, &bits[pos]);
        pos += m_imbeBits;

        if ((i >= 1) && (i <= 6)) // 40 bits of LC/ESS after IMBE 2 .. 7
        {
            std::copy(encoded240 + 40 * (i - 1), encoded240 + 40 * i, &bits[pos]);
            pos += 40;
        }
        else if (i == 7) // LSD after IMBE 8
        {
            std::copy(lsd32, lsd32 + m_lsdBits, &bits[pos]);
            pos += m_lsdBits;
        }
    }

    return pos;
}

int DSEModP25::buildTDU(int nac, uint8_t *bits)
{
    int pos = putSyncNID(nac, DUIDTDU, bits);
    std::fill(&bits[pos], &bits[m_tduDataBits], 0);
    return m_tduDataBits;
}

int DSEModP25::buildTDULC(int nac, const uint8_t *lc9, uint8_t *bits)
{
    int pos = putSyncNID(nac, DUIDTDULC, bits);
    encodeTDULC(lc9, &bits[pos]);
    return m_tdulcDataBits;
}

int DSEModP25::insertStatusSymbols(const uint8_t *data, int nbDataBits, uint8_t status, uint8_t *out)
{
    const int groupBits = 2 * m_dataDibitsPerStatus;
    int nbGroups = (nbDataBits + groupBits - 1) / groupBits;
    int pos = 0;

    for (int g = 0; g < nbGroups; g++)
    {
        for (int i = 0; i < groupBits; i++)
        {
            int k = g * groupBits + i;
            out[pos++] = (k < nbDataBits) ? data[k] : 0;
        }

        out[pos++] = (status >> 1) & 1;
        out[pos++] = status & 1;
    }

    return pos;
}

int DSEModP25::appendFrame(std::vector<int8_t>& levels, const uint8_t *data, int nbDataBits)
{
    uint8_t bits[m_maxFrameBits];
    int nbBits = insertStatusSymbols(data, nbDataBits, m_statusSymbol, bits);
    appendDibits(levels, bits, nbBits);
    return nbBits / 2;
}

void DSEModP25::vocoderFrameToBits(const uint8_t *frame, uint8_t *bits)
{
    for (int i = 0; i < m_imbeBits; i++) {
        bits[i] = (frame[i / 8] >> (7 - (i % 8))) & 1;
    }
}

// ---- Protocol -------------------------------------------------------------------------------------

void DSEModP25::start(const DSEModSettings& settings)
{
    m_nac = settings.m_p25Nac & 0xFFF;
    m_talkGroup = (uint16_t) (settings.m_p25TalkGroup & 0xFFFF);
    m_sourceId = settings.m_p25SourceId & 0xFFFFFF;
    std::fill(m_mi, m_mi + 9, 0); // clear call: null message indicator
    buildLC(settings, m_lc);
    encodeLC(m_lc, m_lcBitsEncoded);
    encodeESS(m_mi, 0x80, 0x0000, m_essBitsEncoded); // ALGID 0x80: unencrypted
    m_lsd = 0;
    encodeLSD((m_lsd >> 16) & 0xFFFF, m_lsdBitsLDU1);
    encodeLSD(m_lsd & 0xFFFF, m_lsdBitsLDU2);

    m_stopRequested = false;
    m_voiceFrameCount = 0;
    m_message.clear();
    m_state = StateHDU;

    qDebug("DSEModP25::start: NAC: 0x%03x TG: %u source: %u", m_nac, m_talkGroup, m_sourceId);
}

void DSEModP25::stop()
{
    m_stopRequested = true;
}

bool DSEModP25::queueMessage(const std::string& text)
{
    if (text.empty() || (m_state == StateIdle)) {
        return false;
    }

    // (500 characters of the UTF-8 text, not bytes: a cut through a character sent half of it)
    m_message = QString::fromStdString(text).left(500).toStdString();
    return true;
}

void DSEModP25::buildTextPacket(const std::string& text, uint32_t sourceId, uint32_t destinationId, bool confirmed, std::vector<uint8_t>& data, int& blocks, int& padOctets)
{
    // The TMS PDU: a length, the header (options follow, a simple text message), no address, two
    // option bytes (sequence number, UCS-2 little endian) and the text
    std::vector<uint8_t> tms;
    tms.push_back(0x80);                        // header: more headers, not a control message, type 0
    tms.push_back(0x00);                        // address length
    tms.push_back(0x80 | 0x01);                 // option 1: another follows, sequence number 1
    tms.push_back(0x04);                        // option 2: last, encoding UCS-2 LE

    // The UTF-8 text in UTF-16 units (UCS-2 beyond the BMP gets its surrogates, as radios show them)
    const QString utf16 = QString::fromStdString(text);

    for (QChar c : utf16)
    {
        tms.push_back((uint8_t) (c.unicode() & 0xFF));
        tms.push_back((uint8_t) (c.unicode() >> 8));
    }

    const int tmsLength = (int) tms.size();
    std::vector<uint8_t> udp;
    udp.push_back(0x0F); udp.push_back(0xA7);   // source port 4007
    udp.push_back(0x0F); udp.push_back(0xA7);   // destination port 4007
    int udpLength = 8 + 2 + tmsLength;
    udp.push_back((uint8_t) (udpLength >> 8)); udp.push_back((uint8_t) udpLength);
    udp.push_back(0); udp.push_back(0);         // no UDP checksum
    udp.push_back((uint8_t) (tmsLength >> 8)); udp.push_back((uint8_t) tmsLength);
    udp.insert(udp.end(), tms.begin(), tms.end());

    // IPv4: the radios' addresses from their IDs on the 12.0.0.0/8 network, as Motorola does
    int ipLength = 20 + (int) udp.size();
    uint8_t ip[20] = { 0x45, 0x00, (uint8_t) (ipLength >> 8), (uint8_t) ipLength, 0x00, 0x01, 0x00, 0x00, 64, 17, 0, 0,
        12, (uint8_t) (sourceId >> 16), (uint8_t) (sourceId >> 8), (uint8_t) sourceId,
        12, (uint8_t) (destinationId >> 16), (uint8_t) (destinationId >> 8), (uint8_t) destinationId };
    uint32_t sum = 0;

    for (int i = 0; i < 20; i += 2) {
        sum += ((uint32_t) ip[i] << 8) | ip[i + 1];
    }

    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    uint16_t checksum = (uint16_t) ~sum;
    ip[10] = (uint8_t) (checksum >> 8);
    ip[11] = (uint8_t) checksum;

    data.assign(ip, ip + 20);
    data.insert(data.end(), udp.begin(), udp.end());

    // Blocks of 16 (confirmed) or 12 bytes, the CRC-32 of everything before it in the last four
    const int blockBytes = confirmed ? 16 : 12;
    blocks = ((int) data.size() + 4 + blockBytes - 1) / blockBytes;
    padOctets = blocks * blockBytes - 4 - (int) data.size();
    data.resize(blocks * blockBytes - 4, 0);
    uint32_t crc = DVP25::packetCrc(data.data(), (int) data.size());
    data.push_back((uint8_t) (crc >> 24));
    data.push_back((uint8_t) (crc >> 16));
    data.push_back((uint8_t) (crc >> 8));
    data.push_back((uint8_t) crc);
}

void DSEModP25::buildPDU(int nac, uint32_t destinationId, bool confirmed, const std::vector<uint8_t>& data, int blocks, int padOctets, std::vector<uint8_t>& bits)
{
    // Sync and NID, the header block, then the data blocks
    bits.assign(m_syncBits + m_nidBits + (1 + blocks) * DVP25::m_blockBits, 0);
    int pos = DVBits::putBits(bits.data(), 0, m_frameSync, m_syncBits);
    pos = DVBits::putBits(bits.data(), pos, DVP25::nidEncode(nac, DUIDPDU), m_nidBits);

    uint8_t header[12] = { 0 };
    header[0] = (uint8_t) ((confirmed ? 0x40 : 0x00) | 0x20 | (confirmed ? 0x16 : 0x15)); // A/N, I/O outbound, format
    header[1] = 0x04;                           // SAP 4: packet data (IP)
    header[2] = 0x00;                           // MFID: standard
    header[3] = (uint8_t) (destinationId >> 16);
    header[4] = (uint8_t) (destinationId >> 8);
    header[5] = (uint8_t) destinationId;
    header[6] = (uint8_t) (0x80 | blocks);      // full message, block count
    header[7] = (uint8_t) padOctets;
    header[8] = 0x00;                           // Ns, FSNF
    header[9] = 0x00;                           // no data header offset
    DVP25::blockEncode(header, bits.data() + pos);
    pos += DVP25::m_blockBits;

    for (int b = 0; b < blocks; b++)
    {
        if (confirmed)
        {
            uint8_t block[18] = { 0 };
            block[0] = (uint8_t) (b << 1);      // serial number, the CRC-9 is computed by the encoder
            std::copy(data.begin() + b * 16, data.begin() + (b + 1) * 16, block + 2);
            DVP25::block34Encode(block, bits.data() + pos);
        }
        else
        {
            // All 12 bytes are data: the trellis of the trunking blocks, its CRC field carried as data
            uint8_t block[12];
            std::copy(data.begin() + b * 12, data.begin() + (b + 1) * 12, block);
            DVP25::blockEncodeRaw(block, bits.data() + pos);
        }

        pos += DVP25::m_blockBits;
    }
}

void DSEModP25::appendPDU(std::vector<int8_t>& levels)
{
    std::vector<uint8_t> data, bits;
    int blocks, padOctets;
    buildTextPacket(m_message, m_sourceId, m_talkGroup, m_confirmedData, data, blocks, padOctets);
    buildPDU(m_nac, m_talkGroup, m_confirmedData, data, blocks, padOctets, bits);
    // insertStatusSymbols emits full groups of 2*m_dataDibitsPerStatus data bits plus 2 status bits
    const int groupBits = 2 * m_dataDibitsPerStatus;
    std::vector<uint8_t> onAir((size_t) (((bits.size() + groupBits - 1) / groupBits) * (groupBits + 2)));
    int nbBits = insertStatusSymbols(bits.data(), (int) bits.size(), m_statusSymbol, onAir.data());
    appendDibits(levels, onAir.data(), nbBits);
    qDebug("DSEModP25::appendPDU: %d characters in %d %s blocks", (int) m_message.size(), blocks, m_confirmedData ? "confirmed" : "unconfirmed");
    m_message.clear();
}

void DSEModP25::abort()
{
    m_state = StateIdle;
    m_stopRequested = false;
}

void DSEModP25::buildLDUFrame(int duid, uint8_t *bits)
{
    uint8_t frame[DSEModVocoder::m_maxFrameBytes];
    uint8_t imbe[m_imbeFramesPerLDU * m_imbeBits];

    for (int i = 0; i < m_imbeFramesPerLDU; i++)
    {
        pullVocoderFrame(frame);
        vocoderFrameToBits(frame, &imbe[i * m_imbeBits]);
        m_voiceFrameCount++;
    }

    if (duid == DUIDLDU1) {
        buildLDU(m_nac, DUIDLDU1, imbe, m_lcBitsEncoded, m_lsdBitsLDU1, bits);
    } else {
        buildLDU(m_nac, DUIDLDU2, imbe, m_essBitsEncoded, m_lsdBitsLDU2, bits);
    }
}

bool DSEModP25::nextFrame(std::vector<int8_t>& levels)
{
    levels.clear();

    if (m_state == StateIdle) {
        return false;
    }

    uint8_t bits[m_maxDataBits];
    int nbBits;

    switch (m_state)
    {
    case StateHDU:
        nbBits = buildHDU(m_nac, m_mi, 0x00, 0x80, 0x0000, m_talkGroup, bits);
        appendFrame(levels, bits, nbBits);
        m_state = StateLDU1;
        break;

    case StateLDU1:
        buildLDUFrame(DUIDLDU1, bits);
        appendFrame(levels, bits, m_lduDataBits);
        m_state = m_stopRequested ? (m_message.empty() ? StateTDU : StatePDU) : StateLDU2; // a TDU may follow either LDU
        break;

    case StateLDU2:
        buildLDUFrame(DUIDLDU2, bits);
        appendFrame(levels, bits, m_lduDataBits);
        m_state = m_stopRequested ? (m_message.empty() ? StateTDU : StatePDU) : StateLDU1;
        break;

    case StatePDU:
        appendPDU(levels);
        m_state = StateTDU;
        break;

    case StateTDU:
        nbBits = buildTDU(m_nac, bits);
        appendFrame(levels, bits, nbBits);
        m_state = StateIdle;
        m_stopRequested = false;
        break;

    case StateIdle:
    default:
        return false;
    }

    return true;
}
