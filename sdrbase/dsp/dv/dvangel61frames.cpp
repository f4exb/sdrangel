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
#include <cmath>
#include <cstring>

#include "dvangel61.h"
#include "dvangel61tables.h"
#include "dvbits.h"
#include "dvblockcode.h"
#include "dvcrc.h"
#include "dvdtmf.h"
#include "dvscrambler.h"

static const uint64_t headerScrambler = 0x5B3A9C1EF0D2ull;

const char *DVAngel61::profileName(Profile profile)
{
    static const char *names[NbProfiles] = { "NB", "WB", "HF-NB", "HF-WB" };
    return names[profile & 3];
}

// ---- Frame types (2.4) ---------------------------------------------------------------------

int DVAngel61::tierCodebooks(Profile profile, int tier)
{
    static const int codebooks[NbProfiles][4] = { { 3, 4, 6, 12 }, { 6, 12, 16, 0 }, { 2, 3, 4, 6 }, { 3, 6, 8, 12 } };
    return (tier >= 0) && (tier < 4) ? codebooks[profile & 3][tier] : 0;
}

int DVAngel61::stepCodebooks(Profile profile)
{
    return profile == ProfileNB ? 2 : profile == ProfileWB ? 4 : 1;
}

int DVAngel61::voiceDataType(Profile profile, int tier, int steps)
{
    if ((steps < 1) || (steps > 2) || (tier < 0) || (tier > 3)) {
        return -1;
    }

    const int codebooks = tierCodebooks(profile, tier);

    if ((codebooks == 0) || (codebooks - steps * stepCodebooks(profile) < floorCodebooks(profile))) {
        return -1;
    }

    return typeOf(profile, (steps == 1 ? KindVoiceData1 : KindVoiceData2) + tier);
}

int DVAngel61::dataSteps(int type)
{
    const int k = kindOf(type);
    return (k >= KindVoiceData1) && (k < KindVoiceData1 + 4) ? 1 : (k >= KindVoiceData2) && (k < KindVoiceData2 + 4) ? 2 : 0;
}

bool DVAngel61::isVoice(int type)
{
    const int k = kindOf(type);
    return (k >= KindVoice) && (k < KindVoiceData2 + 4);
}

int DVAngel61::tierOf(int type)
{
    const int k = kindOf(type);

    if ((k >= KindVoice) && (k < KindVoice + 4)) {
        return k - KindVoice;
    }
    if (dataSteps(type) == 1) {
        return k - KindVoiceData1;
    }
    if (dataSteps(type) == 2) {
        return k - KindVoiceData2;
    }
    if (isGroupData(type)) {
        return k - KindGroupData;
    }
    if (k == KindIdle) {
        return 0;
    }

    return -1;
}

bool DVAngel61::isKnownType(int type)
{
    const Profile p = profileOf(type);
    const int k = kindOf(type);

    if ((k >= KindVoice) && (k < KindVoice + 4)) {
        return true;
    }
    if (dataSteps(type) > 0) {
        return voiceDataType(p, tierOf(type), dataSteps(type)) == type;
    }
    if ((k == KindData) || (k == KindDataQAM16)) {
        return !isHF(p);
    }
    if (isGroupData(type)) {
        return true;
    }

    return k == KindIdle;
}

int DVAngel61::codebooksOf(int type)
{
    if (!isVoice(type) || isOpus(type)) {
        return 0;
    }

    const Profile p = profileOf(type);
    return tierCodebooks(p, tierOf(type)) - dataSteps(type) * stepCodebooks(p);
}

const char *DVAngel61::typeName(int type)
{
    static const char *voice[NbProfiles][4] = {
        { "NB V1 2.25k", "NB V1 3.0k", "NB V1 4.5k", "NB V1 9.0k" },
        { "WB V1 4.5k", "WB V1 9.0k", "WB V1 12k", "WB Opus 18.4k" },
        { "HF-NB V1 1.5k", "HF-NB V1 2.25k", "HF-NB V1 3.0k", "HF-NB V1 4.5k" },
        { "HF-WB V1 2.25k", "HF-WB V1 4.5k", "HF-WB V1 6.0k", "HF-WB V1 9.0k" } };
    static const char *withData[NbProfiles] = { "NB voice + data", "WB voice + data", "HF-NB voice + data", "HF-WB voice + data" };
    static const char *data[NbProfiles][2] = { { "NB data", "NB data 16-QAM" }, { "WB data", "WB data 16-QAM" }, { "", "" }, { "", "" } };
    static const char *group[2][4] = {
        { "HF-NB data tier 0", "HF-NB data tier 1", "HF-NB data tier 2", "HF-NB data tier 3" },
        { "HF-WB data tier 0", "HF-WB data tier 1", "HF-WB data tier 2", "HF-WB data tier 3" } };
    static const char *idle[NbProfiles] = { "NB idle", "WB idle", "HF-NB idle", "HF-WB idle" };

    if (!isKnownType(type)) {
        return "unknown";
    }

    const Profile p = profileOf(type);
    const int k = kindOf(type);

    if ((k >= KindVoice) && (k < KindVoice + 4)) {
        return voice[p][k - KindVoice];
    }
    if (dataSteps(type) > 0) {
        return withData[p];
    }
    if (k == KindData) {
        return data[p][0];
    }
    if (k == KindDataQAM16) {
        return data[p][1];
    }
    if (isGroupData(type)) {
        return group[p - ProfileHFNB][k - KindGroupData];
    }

    return idle[p];
}

// ---- Codes (2.7) ---------------------------------------------------------------------------

DVAngel61::CodeId DVAngel61::codeOf(int type)
{
    if (!isKnownType(type)) {
        return CodeNone;
    }

    const Profile p = profileOf(type);
    const int k = kindOf(type);
    static const CodeId tiers[NbProfiles] = { CodeNB_T0, CodeWB_T0, CodeHN_T0, CodeHW_T0 };

    if (isVoice(type)) {
        return (CodeId) (tiers[p] + tierOf(type));
    }
    if (k == KindIdle) {
        return tiers[p];
    }
    if (k == KindData) {
        return p == ProfileNB ? CodeNB_DQ : CodeWB_DQ;
    }
    if (k == KindDataQAM16) {
        return p == ProfileNB ? CodeNB_DX : CodeWB_T3;      // WB's 16-QAM data frame shares the Opus tier's code
    }

    if (isGroupData(type)) {
        return (CodeId) ((p == ProfileHFNB ? CodeHN_D0 : CodeHW_D0) + (k - KindGroupData));
    }

    return CodeNone;
}

DVAngel61::Constellation DVAngel61::constellationOf(int type)
{
    const CodeId c = codeOf(type);

    if (c == CodeNone) {
        return QPSK;
    }

    const DVAngel61Tables::Code& t = DVAngel61Tables::codes[c];
    const int frames = isGroupData(type) ? GroupFrames : 1;
    return t.n == frames * layout(profileOf(type)).payloadCount() * 2 ? QPSK : QAM16;
}

int DVAngel61::infoBits(int type)
{
    const CodeId c = codeOf(type);
    return c == CodeNone ? 0 : DVAngel61Tables::codes[c].k;
}

int DVAngel61::codedBits(int type)
{
    const CodeId c = codeOf(type);
    return c == CodeNone ? 0 : DVAngel61Tables::codes[c].n;
}

DVAngel61::LDPC::LDPC(CodeId code)
{
    const DVAngel61Tables::Code& t = DVAngel61Tables::codes[code];
    m_n = t.n;
    m_k = t.k;
    m_z = t.z;
    m_mb = t.mb;
    m_nb = t.nb;
    m_kb = m_k / m_z;
    m_base = t.base;
    m_perm = t.perm;
    m_rowStart.assign(m_mb * m_z + 1, 0);
    m_maxDegree = 0;

    // Check c = i Z + t of block row i connects variable j Z + (t + s) mod Z for each block (i, j) with shift s
    for (int i = 0; i < m_mb; i++)
    {
        for (int r = 0; r < m_z; r++)
        {
            const int check = i * m_z + r;
            for (int j = 0; j < m_nb; j++)
            {
                const int s = m_base[i * m_nb + j];
                if (s >= 0) {
                    m_column.push_back(j * m_z + (r + s) % m_z);
                }
            }
            m_rowStart[check + 1] = (int) m_column.size();
            m_maxDegree = std::max(m_maxDegree, m_rowStart[check + 1] - m_rowStart[check]);
        }
    }
}

void DVAngel61::LDPC::encode(const uint8_t *info, uint8_t *codeword) const
{
    const int Z = m_z;
    std::vector<uint8_t> lambda(m_mb * Z, 0);

    // lambda_i = sum over the information blocks of P^s u_j, (P^s x)[t] = x[(t + s) mod Z]
    for (int i = 0; i < m_mb; i++)
    {
        for (int j = 0; j < m_kb; j++)
        {
            const int s = m_base[i * m_nb + j];
            if (s < 0) {
                continue;
            }
            for (int t = 0; t < Z; t++) {
                lambda[i * Z + t] ^= info[j * Z + (t + s) % Z];
            }
        }
    }

    // Dual diagonal: p0 = sum of all lambda_i; p1 = lambda_0 + p0; p_{i+1} = lambda_i + p_i (+ p0 at the middle row)
    uint8_t *parity = codeword + m_k;
    const int mid = m_mb / 2;

    for (int t = 0; t < Z; t++)
    {
        uint8_t p0 = 0;
        for (int i = 0; i < m_mb; i++) {
            p0 ^= lambda[i * Z + t];
        }
        parity[t] = p0;
        parity[Z + t] = lambda[t] ^ p0;

        for (int i = 1; i < m_mb - 1; i++)
        {
            uint8_t next = lambda[i * Z + t] ^ parity[i * Z + t];
            if (i == mid) {
                next ^= p0;
            }
            parity[(i + 1) * Z + t] = next;
        }
    }

    memcpy(codeword, info, m_k);
}

bool DVAngel61::LDPC::syndromeZero(const uint8_t *codeword) const
{
    const int m = m_n - m_k;

    for (int c = 0; c < m; c++)
    {
        uint8_t acc = 0;
        for (int e = m_rowStart[c]; e < m_rowStart[c + 1]; e++) {
            acc ^= codeword[m_column[e]];
        }
        if (acc) {
            return false;
        }
    }

    return true;
}

int DVAngel61::LDPC::decode(const float *llr, uint8_t *info, int maxIterations, uint8_t *codeword, float *posteriorOut) const
{
    const float alpha = 0.8f;
    const int m = m_n - m_k;
    std::vector<float> posterior(llr, llr + m_n);
    std::vector<float> message(m_column.size(), 0.0f);
    std::vector<uint8_t> hard(m_n);
    std::vector<float> q(m_maxDegree);
    int result = -1;

    for (int iteration = 1; iteration <= maxIterations; iteration++)
    {
        for (int c = 0; c < m; c++)
        {
            const int start = m_rowStart[c];
            const int degree = m_rowStart[c + 1] - start;
            float min1 = 1e30f, min2 = 1e30f;
            int minIndex = 0;
            bool negative = false;

            for (int e = 0; e < degree; e++)
            {
                const float v = posterior[m_column[start + e]] - message[start + e];
                q[e] = v;
                const float mag = std::fabs(v);
                negative ^= (v < 0.0f);
                if (mag < min1) {
                    min2 = min1;
                    min1 = mag;
                    minIndex = e;
                } else if (mag < min2) {
                    min2 = mag;
                }
            }

            for (int e = 0; e < degree; e++)
            {
                const float mag = alpha * (e == minIndex ? min2 : min1);
                const bool sign = negative ^ (q[e] < 0.0f);
                const float r = sign ? -mag : mag;
                message[start + e] = r;
                posterior[m_column[start + e]] = q[e] + r;
            }
        }

        for (int v = 0; v < m_n; v++) {
            hard[v] = posterior[v] < 0.0f ? 1 : 0;
        }

        if (syndromeZero(hard.data()))
        {
            result = iteration;
            break;
        }
    }

    memcpy(info, hard.data(), m_k);
    if (codeword) {
        memcpy(codeword, hard.data(), m_n);
    }
    if (posteriorOut) {
        memcpy(posteriorOut, posterior.data(), m_n * sizeof(float));
    }

    return result;
}

const DVAngel61::LDPC& DVAngel61::ldpc(CodeId code)
{
    static const std::vector<LDPC> codes = [] {
        std::vector<LDPC> v;
        v.reserve(NbCodes);
        for (int c = 0; c < NbCodes; c++) {
            v.emplace_back((CodeId) c);
        }
        return v;
    }();
    return codes[code];
}

// ---- Layouts (2.2) -------------------------------------------------------------------------

static DVAngel61::Layout buildLayout(DVAngel61::Profile profile)
{
    using namespace DVAngel61Tables;
    DVAngel61::Layout l;
    l.m_profile = profile;
    const signed char *kind = nullptr, *header = nullptr, *known = nullptr, *uw = nullptr, *pilots = nullptr;
    const short *order = nullptr;
    int orderLength = 0;

    switch (profile)
    {
    case DVAngel61::ProfileNB:
        l.m_cells = 384; l.m_carriers = 1; l.m_symbols = 384; l.m_headerCopies = 2;
        kind = kindNB; header = headerNB; uw = uwNB; pilots = pilotsNB;
        break;
    case DVAngel61::ProfileWB:
        l.m_cells = 768; l.m_carriers = 1; l.m_symbols = 768; l.m_headerCopies = 2;
        kind = kindWB; header = headerWB; uw = uwWB; pilots = pilotsWB;
        break;
    case DVAngel61::ProfileHFNB:
        l.m_carriers = ncHN; l.m_symbols = symbolsHN; l.m_cells = ncHN * symbolsHN; l.m_headerCopies = headerCopiesHN;
        kind = kindHN; header = headerHN; known = pilotHN; order = orderHN; orderLength = (int) (sizeof(orderHN) / sizeof(orderHN[0]));
        break;
    default:
        l.m_carriers = ncHW; l.m_symbols = symbolsHW; l.m_cells = ncHW * symbolsHW; l.m_headerCopies = headerCopiesHW;
        kind = kindHW; header = headerHW; known = pilotHW; order = orderHW; orderLength = (int) (sizeof(orderHW) / sizeof(orderHW[0]));
        break;
    }

    l.m_kind.assign(kind, kind + l.m_cells);
    l.m_known.assign(l.m_cells, std::complex<float>(0.0f, 0.0f));
    l.m_headerCells.assign(DVAngel61::HeaderCells * l.m_headerCopies, -1);
    int nbUW = 0, nbPilots = 0;

    for (int c = 0; c < l.m_cells; c++)
    {
        switch (l.m_kind[c])
        {
        case DVAngel61::CellUW:
            l.m_known[c] = DVAngel61::qpskSymbol(uw[nbUW++]);
            l.m_uwCells.push_back(c);
            break;
        case DVAngel61::CellPilot:
            l.m_known[c] = DVAngel61::qpskSymbol(known ? known[c] : pilots[nbPilots]);
            nbPilots++;
            l.m_pilotCells.push_back(c);
            break;
        case DVAngel61::CellHeader:
            l.m_headerCells[header[c]] = c;
            break;
        default:
            if (!order) {
                l.m_payloadCells.push_back(c);      // VHF: frame order
            }
            break;
        }
    }

    if (order) {
        l.m_payloadCells.assign(order, order + orderLength);   // HF: the layout's order (2.2)
    }

    return l;
}

const DVAngel61::Layout& DVAngel61::layout(Profile profile)
{
    static const Layout layouts[NbProfiles] = {
        buildLayout(ProfileNB), buildLayout(ProfileWB), buildLayout(ProfileHFNB), buildLayout(ProfileHFWB)
    };
    return layouts[profile & 3];
}

// ---- Payload block (2.3) -------------------------------------------------------------------
// Every block: the voice or audio, the slots, the message field, reserved bits (zero), the CRC-12

int DVAngel61::voiceBits(int type)
{
    if (isOpus(type)) {
        return OpusBits;
    }

    return packetsPerFrame(profileOf(type)) * packetBits(type);
}

int DVAngel61::slotsOf(int type)
{
    if (!isKnownType(type)) {
        return 0;
    }
    if (isGroupData(type)) {
        return GroupFrames;
    }
    if (isIdle(type)) {
        return 2;
    }

    // WB's tiers 0 and 2 carry two slots (2.3), with or without data under them
    const Profile p = profileOf(type);
    const int tier = tierOf(type);
    return (p == ProfileWB) && isVoice(type) && ((tier == 0) || (tier == 2)) ? 2 : 1;
}

int DVAngel61::messageFieldBits(int type)
{
    if (!isKnownType(type)) {
        return 0;
    }

    const int steps = dataSteps(type);

    if (steps > 0) {
        return packetsPerFrame(profileOf(type)) * steps * stepCodebooks(profileOf(type)) * CodebookBits;
    }

    if (isData(type))
    {
        const int k = infoBits(type);
        return k > 0 ? k - 12 - slotsOf(type) * SlotBits : 0;
    }

    return 0;
}

void DVAngel61::putVoicePacket(uint8_t *block, int type, int packet, const uint8_t *bits)
{
    const int n = packetBits(type);
    std::copy(bits, bits + n, block + packet * n);
}

void DVAngel61::getVoicePacket(const uint8_t *block, int type, int packet, uint8_t *bits)
{
    const int n = packetBits(type);
    std::copy(block + packet * n, block + (packet + 1) * n, bits);
}

void DVAngel61::packMessageField(const DVAngel61::MessageFragment& fragment, int bytes, uint8_t *bits)
{
    int pos = DVBits::putBits(bits, 0, fragment.m_contentType & 0xF, 4);
    pos = DVBits::putBits(bits, pos, fragment.m_flags & 0xF, 4);
    pos = DVBits::putBits(bits, pos, fragment.m_id & 0xFF, 8);
    pos = DVBits::putBits(bits, pos, fragment.m_index & 0xFF, 8);
    pos = DVBits::putBits(bits, pos, fragment.m_total & 0xFF, 8);

    for (int i = 0; i < bytes; i++) {
        pos = DVBits::putBits(bits, pos, i < (int) fragment.m_payload.size() ? fragment.m_payload[i] : 0, 8);
    }
}

void DVAngel61::unpackMessageField(const uint8_t *bits, int bytes, DVAngel61::MessageFragment& fragment)
{
    fragment.m_contentType = (int) DVBits::getBits(bits, 0, 4);
    fragment.m_flags = (int) DVBits::getBits(bits, 4, 4);
    fragment.m_id = (int) DVBits::getBits(bits, 8, 8);
    fragment.m_index = (int) DVBits::getBits(bits, 16, 8);
    fragment.m_total = (int) DVBits::getBits(bits, 24, 8);
    fragment.m_payload.resize(bytes);

    for (int i = 0; i < bytes; i++) {
        fragment.m_payload[i] = (uint8_t) DVBits::getBits(bits, FieldHeaderBits + 8 * i, 8);
    }
}

int DVAngel61::fragmentMessage(const std::vector<uint8_t>& message, int contentType, int id, int bytes,
    std::vector<DVAngel61::MessageFragment>& fragments, int flags)
{
    fragments.clear();

    if ((bytes <= 0) || (message.size() > 65535)) {
        return 0;
    }

    // The byte length leads the first fragment (3.2)
    std::vector<uint8_t> stream;
    stream.push_back((uint8_t) (message.size() >> 8));
    stream.push_back((uint8_t) (message.size() & 0xFF));
    stream.insert(stream.end(), message.begin(), message.end());
    const int total = ((int) stream.size() + bytes - 1) / bytes;

    if (total > 255) {
        return 0;
    }

    for (int i = 0; i < total; i++)
    {
        DVAngel61::MessageFragment f;
        f.m_contentType = contentType;
        f.m_flags = flags & 0xF;
        f.m_id = id & 0xFF;
        f.m_index = i;
        f.m_total = total;
        const int start = i * bytes;
        const int end = std::min((int) stream.size(), start + bytes);
        f.m_payload.assign(stream.begin() + start, stream.begin() + end);
        f.m_payload.resize(bytes, 0);
        fragments.push_back(f);
    }

    return total;
}

// ---- CRCs (2.7) ----------------------------------------------------------------------------

uint8_t DVAngel61::crc8(const uint8_t *bits, int nbBits)
{
    return (uint8_t) DVCRC::compute(bits, nbBits, 8, 0x07, 0xFF, 0);
}

uint16_t DVAngel61::crc12(const uint8_t *bits, int nbBits)
{
    return (uint16_t) DVCRC::compute(bits, nbBits, 12, 0x80F, 0xFFF, 0);
}

uint16_t DVAngel61::blockCrc(const Header *headers, int nbHeaders, const uint8_t *block, int type)
{
    const int n = crcOffset(type);
    std::vector<uint8_t> bits(HeaderFieldBits * nbHeaders + n);

    for (int h = 0; h < nbHeaders; h++) {
        headerFieldBits(headers[h], bits.data() + h * HeaderFieldBits);
    }

    std::copy(block, block + n, bits.begin() + HeaderFieldBits * nbHeaders);
    return crc12(bits.data(), (int) bits.size());
}

void DVAngel61::finishBlock(uint8_t *block, const Header *headers, int nbHeaders)
{
    const int type = headers[0].m_type;
    DVBits::putBits(block, crcOffset(type), blockCrc(headers, nbHeaders, block, type), 12);
}

std::vector<uint8_t> DVAngel61::packRecord(const Header& header, const uint8_t *block)
{
    std::vector<uint8_t> record((size_t) recordBytes(header.m_type), 0);
    record[0] = (uint8_t) header.m_type;
    record[1] = (uint8_t) ((header.m_counter & 0x3F) | (header.m_lastFrame ? 0x40 : 0));
    record[2] = (uint8_t) (header.m_tier & 3);
    DVBits::bitsToBytesMSB(block, infoBits(header.m_type), record.data() + 3);
    return record;
}

bool DVAngel61::unpackRecord(const uint8_t *record, int size, Header& header, std::vector<uint8_t>& block)
{
    if ((size < 3) || !isKnownType(record[0]) || isGroupData(record[0]) || (size != recordBytes(record[0])) || (record[2] > 3)) {
        return false;
    }

    header.m_type = record[0];
    header.m_counter = record[1] & 0x3F;
    header.m_lastFrame = (record[1] & 0x40) != 0;
    header.m_tier = record[2];
    const int bits = infoBits(header.m_type);
    std::vector<uint8_t> bytesAsBits((size_t) (size - 3) * 8);
    DVBits::bytesToBitsMSB(record + 3, size - 3, bytesAsBits.data());
    block.assign(bytesAsBits.begin(), bytesAsBits.begin() + bits);
    return checkBlock(block.data(), header);
}

std::vector<uint8_t> DVAngel61::packRelayMessage(int contentType, int flags, const std::vector<uint8_t>& message)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(message.size() + 2);
    bytes.push_back((uint8_t) contentType);
    bytes.push_back((uint8_t) (flags & 0x0F));
    bytes.insert(bytes.end(), message.begin(), message.end());
    return bytes;
}

bool DVAngel61::unpackRelayMessage(const uint8_t *bytes, int size, int& contentType, int& flags, std::vector<uint8_t>& message)
{
    if ((size < 3) || (size > MaxRelayMessageBytes + 2) || !relayable(bytes[0]) || (bytes[1] > 0x0F)) {
        return false;
    }

    contentType = bytes[0];
    flags = bytes[1];
    message.assign(bytes + 2, bytes + size);
    return true;
}

bool DVAngel61::checkBlock(const uint8_t *block, const Header *headers, int nbHeaders)
{
    const int type = headers[0].m_type;
    return DVBits::getBits(block, crcOffset(type), 12) == blockCrc(headers, nbHeaders, block, type);
}

// ---- Header (2.4, 2.7): type 7, counter 6, last frame 1, tier 2, then the CRC-8 -------------

void DVAngel61::headerFieldBits(const Header& header, uint8_t *bits)
{
    int pos = DVBits::putBits(bits, 0, header.m_type & 0x7F, 7);
    pos = DVBits::putBits(bits, pos, header.m_counter & 0x3F, 6);
    pos = DVBits::putBits(bits, pos, header.m_lastFrame ? 1 : 0, 1);
    DVBits::putBits(bits, pos, header.m_tier & 0x3, 2);
}

void DVAngel61::encodeHeader(const Header& header, uint8_t *bits)
{
    uint8_t info[HeaderInfoBits];
    headerFieldBits(header, info);
    DVBits::putBits(info, HeaderFieldBits, crc8(info, HeaderFieldBits), 8);

    // Two Golay(24,12) words, bit interleaved: bit 2i is bit i of the first word, 2i + 1 of the second
    const DVBlockCode& golay = DVBlockCode::golay2412();
    uint8_t a[24], b[24];
    golay.encode(info, a);
    golay.encode(info + 12, b);

    for (int i = 0; i < 24; i++)
    {
        bits[2 * i] = a[i] ^ ((headerScrambler >> (47 - 2 * i)) & 1);
        bits[2 * i + 1] = b[i] ^ ((headerScrambler >> (46 - 2 * i)) & 1);
    }
}

bool DVAngel61::decodeHeader(const float *llr, Header& header, int *errors)
{
    const DVBlockCode& golay = DVBlockCode::golay2412();
    float a[24], b[24];

    for (int i = 0; i < 24; i++)
    {
        a[i] = ((headerScrambler >> (47 - 2 * i)) & 1) ? -llr[2 * i] : llr[2 * i];
        b[i] = ((headerScrambler >> (46 - 2 * i)) & 1) ? -llr[2 * i + 1] : llr[2 * i + 1];
    }

    uint8_t info[HeaderInfoBits];
    int e = golay.decodeSoft(a, info);
    e += golay.decodeSoft(b, info + 12);

    if (errors) {
        *errors = e;
    }

    if (DVBits::getBits(info, HeaderFieldBits, 8) != crc8(info, HeaderFieldBits)) {
        return false;
    }

    header.m_type = (int) DVBits::getBits(info, 0, 7);
    header.m_counter = (int) DVBits::getBits(info, 7, 6);
    header.m_lastFrame = info[13] != 0;
    header.m_tier = (int) DVBits::getBits(info, 14, 2);
    return true;
}

void DVAngel61::groupHeaders(const Header& any, bool lastOfFourth, Header *headers)
{
    const int first = (any.m_counter - groupIndex(any.m_counter)) & 0x3F;

    for (int f = 0; f < GroupFrames; f++)
    {
        headers[f] = any;
        headers[f].m_counter = (first + f) & 0x3F;
        headers[f].m_lastFrame = (f == GroupFrames - 1) && lastOfFourth;
    }
}

// ---- Symbols -------------------------------------------------------------------------------

// 16-QAM axis: bits (sign, magnitude) Gray mapped: 00 +3, 01 +1, 11 -1, 10 -3 (over sqrt(10)), as revision 2
static inline float qam16Level(int sign, int mag)
{
    const float a = 0.31622777f;
    const float level = mag ? a : 3.0f * a;
    return sign ? -level : level;
}

static inline void qam16AxisLLR(float y, float scale, float& llrSign, float& llrMag)
{
    const float a = 0.31622777f;
    const float d3p = (y - 3 * a) * (y - 3 * a);
    const float d1p = (y - a) * (y - a);
    const float d1n = (y + a) * (y + a);
    const float d3n = (y + 3 * a) * (y + 3 * a);
    llrSign = (std::min(d1n, d3n) - std::min(d1p, d3p)) * scale;
    llrMag = (std::min(d1p, d1n) - std::min(d3p, d3n)) * scale;
}

void DVAngel61::mapCells(Constellation c, const uint8_t *bits, int nbCells, std::complex<float> *cells)
{
    if (c == QPSK)
    {
        for (int i = 0; i < nbCells; i++) {
            cells[i] = qpskSymbol((bits[2 * i] << 1) | bits[2 * i + 1]);
        }
    }
    else
    {
        for (int i = 0; i < nbCells; i++)
        {
            const uint8_t *b = bits + 4 * i;
            cells[i] = std::complex<float>(qam16Level(b[0], b[2]), qam16Level(b[1], b[3]));
        }
    }
}

void DVAngel61::demapCells(Constellation c, const std::complex<float> *cells, int nbCells, float noiseVariance, float *llr)
{
    const float nv = std::max(noiseVariance, 1e-6f);

    if (c == QPSK)
    {
        const float scale = 2.8284271f / nv;
        for (int i = 0; i < nbCells; i++)
        {
            llr[2 * i] = cells[i].real() * scale;
            llr[2 * i + 1] = cells[i].imag() * scale;
        }
    }
    else
    {
        const float scale = 1.0f / nv;
        for (int i = 0; i < nbCells; i++)
        {
            qam16AxisLLR(cells[i].real(), scale, llr[4 * i], llr[4 * i + 2]);
            qam16AxisLLR(cells[i].imag(), scale, llr[4 * i + 1], llr[4 * i + 3]);
        }
    }
}

void DVAngel61::demapCellsAmplitude(Constellation c, const std::complex<float> *cells, int nbCells, float noiseVariance,
    float estimateError, float *llr)
{
    if ((c == QPSK) || (estimateError <= 0.0f))
    {
        demapCells(c, cells, nbCells, noiseVariance + std::max(0.0f, estimateError), llr);
        return;
    }

    // The 16 points' axis levels and bits (mapCells: I from bits 0 and 2, Q from 1 and 3)
    float metric[16];
    const float nv = std::max(noiseVariance, 1e-6f);

    for (int i = 0; i < nbCells; i++)
    {
        const std::complex<float> y = cells[i];

        for (int point = 0; point < 16; point++)
        {
            const int b0 = (point >> 3) & 1, b1 = (point >> 2) & 1, b2 = (point >> 1) & 1, b3 = point & 1;
            const std::complex<float> x(qam16Level(b0, b2), qam16Level(b1, b3));
            const float v = nv + estimateError * std::norm(x);
            metric[point] = std::norm(y - x) / v + std::log(v);
        }

        for (int bit = 0; bit < 4; bit++)
        {
            float m0 = 1e30f, m1 = 1e30f;

            for (int point = 0; point < 16; point++)
            {
                if ((point >> (3 - bit)) & 1) {
                    m1 = std::min(m1, metric[point]);
                } else {
                    m0 = std::min(m0, metric[point]);
                }
            }

            llr[4 * i + bit] = m1 - m0;   // positive for a 0, as demapCells
        }
    }
}

// ---- Coding chain (2.7) --------------------------------------------------------------------

void DVAngel61::codewordToPositions(const uint8_t *codeword, int counter, CodeId code, uint8_t *positions)
{
    const LDPC& c = ldpc(code);
    const short *perm = c.getPerm();
    DVScrambler::PN15 pn(counter);

    for (int i = 0; i < c.getN(); i++) {
        positions[perm[i]] = codeword[i] ^ pn.next();
    }
}

void DVAngel61::positionsToCodeword(const float *positions, int counter, CodeId code, float *coded)
{
    const LDPC& c = ldpc(code);
    const short *perm = c.getPerm();
    DVScrambler::PN15 pn(counter);

    for (int i = 0; i < c.getN(); i++)
    {
        const float v = positions[perm[i]];
        coded[i] = pn.next() ? -v : v;
    }
}

// ---- Frames --------------------------------------------------------------------------------

void DVAngel61::buildFrame(const Header& header, const uint8_t *block, std::complex<float> *cells)
{
    const Profile p = profileOf(header.m_type);
    const Layout& l = layout(p);
    const CodeId code = codeOf(header.m_type);
    const LDPC& c = ldpc(code);
    const Constellation k = constellationOf(header.m_type);
    std::vector<uint8_t> codeword(c.getN()), positions(c.getN());
    std::vector<std::complex<float>> payload(l.payloadCount());

    c.encode(block, codeword.data());
    codewordToPositions(codeword.data(), header.m_counter, code, positions.data());
    mapCells(k, positions.data(), l.payloadCount(), payload.data());

    uint8_t headerBits[HeaderCodedBits];
    std::complex<float> headerSymbols[HeaderCells];
    encodeHeader(header, headerBits);
    mapCells(QPSK, headerBits, HeaderCells, headerSymbols);

    for (int i = 0; i < l.m_cells; i++) {
        cells[i] = l.m_known[i];
    }
    for (size_t h = 0; h < l.m_headerCells.size(); h++) {
        cells[l.m_headerCells[h]] = headerSymbols[h % HeaderCells];
    }
    for (int i = 0; i < l.payloadCount(); i++) {
        cells[l.m_payloadCells[i]] = payload[i];
    }
}

void DVAngel61::headerLLR(Profile profile, const std::complex<float> *cells, float noiseVariance, float *llr)
{
    const Layout& l = layout(profile);
    std::fill(llr, llr + HeaderCodedBits, 0.0f);

    for (size_t h = 0; h < l.m_headerCells.size(); h++)
    {
        float two[2];
        demapCells(QPSK, cells + l.m_headerCells[h], 1, noiseVariance, two);
        llr[2 * (h % HeaderCells)] += two[0];
        llr[2 * (h % HeaderCells) + 1] += two[1];
    }
}

void DVAngel61::payloadLLR(const std::complex<float> *cells, int type, float noiseVariance, float *llr)
{
    const Layout& l = layout(profileOf(type));
    const Constellation k = constellationOf(type);
    const int bps = bitsPerCell(k);

    for (int i = 0; i < l.payloadCount(); i++) {
        demapCells(k, cells + l.m_payloadCells[i], 1, noiseVariance, llr + bps * i);
    }
}

void DVAngel61::payloadCells(const uint8_t *codeword, int counter, int type, std::complex<float> *cells, const float *posterior, float *confidence)
{
    const CodeId code = codeOf(type);
    const LDPC& c = ldpc(code);
    const Layout& l = layout(profileOf(type));
    const Constellation k = constellationOf(type);
    std::vector<uint8_t> positions(c.getN());
    codewordToPositions(codeword, counter, code, positions.data());
    mapCells(k, positions.data(), l.payloadCount(), cells);

    if (posterior && confidence)
    {
        // The scrambler flips signs, not magnitudes: a position is as sure as its coded bit
        const short *perm = c.getPerm();
        const int bps = bitsPerCell(k);
        std::vector<float> magnitude(c.getN());

        for (int i = 0; i < c.getN(); i++) {
            magnitude[perm[i]] = std::fabs(posterior[i]);
        }

        for (int p = 0; p < l.payloadCount(); p++)
        {
            float least = 1e30f;
            for (int b = 0; b < bps; b++) {
                least = std::min(least, magnitude[(size_t) bps * p + b]);
            }
            confidence[p] = least;
        }
    }
}

bool DVAngel61::decodePayload(const float *llr, const Header& header, uint8_t *block, int *iterations, uint8_t *codeword, float *posterior)
{
    const CodeId code = codeOf(header.m_type);

    if (code == CodeNone) {
        return false;
    }

    const LDPC& c = ldpc(code);
    std::vector<float> coded(c.getN());
    positionsToCodeword(llr, header.m_counter, code, coded.data());
    const int it = c.decode(coded.data(), block, 30, codeword, posterior);

    if (iterations) {
        *iterations = it;
    }

    return (it > 0) && checkBlock(block, header);
}

// ---- Slot queue (2.8): revision 2's, with the profile's frame length, routing cadence and periods ----

DVAngel61SlotQueue::DVAngel61SlotQueue(DVAngel61::Profile profile) :
    m_profile(profile),
    m_hasPath(false),
    m_hasPosition(false),
    m_moving(false),
    m_repeatPending(false),
    m_nextTextId(0),
    m_dtmfSequence(-1),
    m_dtmfNextIndex(0),
    m_dtmfSlotsSent(0),
    m_routingThisFrame(false)
{
    reset();
}

void DVAngel61SlotQueue::reset()
{
    m_timeMs = 0;
    m_missedDeadlines = 0;
    m_fragmentsSent = 0;
    m_routingSent = 0;
    m_idleSent = 0;
    m_repeatPending = false;
    m_dtmfSlotsSent = 0;

    // DTMF queued before the key down goes out in the transmission: its keys are due at once
    for (DTMFKey& key : m_dtmf) {
        key.m_lastSentMs = -1;
    }

    for (int i = 0; i < ItemCount; i++) {
        m_deadline[i] = periodOf((Item) i);
    }

    // The long callsigns are due at once: the preamble sends the first
    for (int i = ItemLong0; i <= ItemLong3; i++) {
        m_deadline[i] = LookaheadMs;
    }
}

void DVAngel61SlotQueue::setPosition(const DVAngel61::Position& position, bool moving)
{
    m_position = position;
    m_hasPosition = true;

    if (moving && !m_moving) {
        m_deadline[ItemPosition] = std::min(m_deadline[ItemPosition], m_timeMs + (DVAngel61::isHF(m_profile) ? 5000 : 1000));   // the moving period
    }

    m_moving = moving;
}

bool DVAngel61SlotQueue::queueText(const std::string& text)
{
    std::vector<DVAngel61::TextFragment> fragments;

    if (DVAngel61::fragmentText(text, m_nextTextId, fragments) == 0) {
        return false;
    }

    if ((int) (m_fragments.size() + fragments.size()) > MaxTextFragments) {
        return false;
    }

    m_nextTextId = (m_nextTextId + 1) & 0xF;
    m_fragments.insert(m_fragments.end(), fragments.begin(), fragments.end());
    m_lastMessage = fragments;
    m_repeatPending = true;
    return true;
}

void DVAngel61SlotQueue::updateLongCallsigns()
{
    // The long callsigns (2.6) of the routing and the path, each once, the source first
    std::vector<uint64_t> codes = { m_routing.m_source, m_routing.m_destination };

    if (m_hasPath)
    {
        codes.push_back(m_path.m_repeater1);
        codes.push_back(m_path.m_repeater2);
    }

    std::vector<uint64_t> longCallsigns;

    for (uint64_t code : codes)
    {
        if (DVAngel61::isLongCallsign(code) && (std::find(longCallsigns.begin(), longCallsigns.end(), code) == longCallsigns.end())) {
            longCallsigns.push_back(code);
        }
    }

    if (longCallsigns != m_longCallsigns)
    {
        m_longCallsigns = longCallsigns;

        // Due at once (within the lookahead), until sent
        for (int i = ItemLong0; i <= ItemLong3; i++) {
            m_deadline[i] = m_timeMs + LookaheadMs;
        }
    }
}

int DVAngel61SlotQueue::periodOf(Item item) const
{
    // VHF as revision 2; HF's are longer, since one slot per 240 ms cannot carry VHF's (2.8)
    const bool hf = DVAngel61::isHF(m_profile);

    switch (item)
    {
    case ItemStation: return hf ? 10000 : 5000;
    case ItemPosition: return m_moving ? (hf ? 5000 : 1000) : (hf ? 10000 : 5000);
    case ItemPath: return hf ? 6000 : 2000;
    case ItemLong0: case ItemLong1: case ItemLong2: case ItemLong3: return hf ? 6000 : 2000;
    default: return 0;
    }
}

bool DVAngel61SlotQueue::enabled(Item item) const
{
    switch (item)
    {
    case ItemStation: return true;
    case ItemPosition: return m_hasPosition;
    case ItemPath: return m_hasPath;
    case ItemLong0: case ItemLong1: case ItemLong2: case ItemLong3: return item - ItemLong0 < (int) m_longCallsigns.size();
    default: return false;
    }
}

void DVAngel61SlotQueue::fillItem(Item item, uint8_t *slot)
{
    switch (item)
    {
    case ItemStation: DVAngel61::packStationSlot(m_station, slot); break;
    case ItemPosition: DVAngel61::packPositionSlot(m_position, slot); break;
    case ItemPath: DVAngel61::packPathSlot(m_path, slot); break;
    case ItemLong0: case ItemLong1: case ItemLong2: case ItemLong3:
        if (!DVAngel61::packLongCallsignSlot(DVAngel61::decodeCallsign(m_longCallsigns[item - ItemLong0]), slot)) {
            DVAngel61::packIdleSlot(slot);
        }
        break;
    default: DVAngel61::packIdleSlot(slot); break;
    }

    if (m_deadline[item] < m_timeMs)
    {
        m_missedDeadlines++;
        m_deadline[item] = m_timeMs + periodOf(item);
    }
    else if (m_timeMs >= m_deadline[item] - LookaheadMs)
    {
        // A scheduled send: the cadence is kept from the deadline, not from the (early) send
        m_deadline[item] += periodOf(item);
    }
    else
    {
        // Sent ahead of the schedule (the preamble): the period restarts from now
        m_deadline[item] = m_timeMs + periodOf(item);
    }
}

bool DVAngel61SlotQueue::queueDTMF(const std::string& keys, int toneMs, int gapMs, bool newSequence)
{
    const std::string valid = DVDTMF::validKeys(keys);

    if (valid.empty() || ((int) m_dtmf.size() >= MaxDTMFKeys)) {
        return false;
    }

    if (newSequence || (m_dtmfSequence < 0))
    {
        m_dtmfSequence = (m_dtmfSequence + 1) & 0xF;
        m_dtmfNextIndex = 0;
    }

    for (char key : valid)
    {
        if ((int) m_dtmf.size() >= MaxDTMFKeys) {
            break;
        }

        DTMFKey k;
        k.m_sequence = m_dtmfSequence;
        k.m_index = m_dtmfNextIndex;
        k.m_key = key;
        k.m_toneUnits = DVAngel61::dtmfUnits(toneMs);
        k.m_gapUnits = DVAngel61::dtmfUnits(gapMs);
        k.m_sends = 0;
        k.m_lastSentMs = -1;
        m_dtmf.push_back(k);
        m_dtmfNextIndex = (m_dtmfNextIndex + 1) & 0x3F;
    }

    return true;
}

bool DVAngel61SlotQueue::fillDTMF(uint8_t *slot)
{
    // The earliest key still owed a send and not sent in this frame; the window runs on from it over
    // the keys of its sequence with its lengths, consecutive in index, up to 17
    auto first = m_dtmf.begin();

    while ((first != m_dtmf.end()) && (first->m_lastSentMs == m_timeMs)) {
        ++first;
    }

    if (first == m_dtmf.end()) {
        return false;
    }

    DVAngel61::DTMF window;
    window.m_sequence = first->m_sequence;
    window.m_first = first->m_index;
    window.m_toneMs = first->m_toneUnits * DVAngel61::DTMF::UnitMs;
    window.m_gapMs = first->m_gapUnits * DVAngel61::DTMF::UnitMs;
    int expected = first->m_index;

    for (auto it = first; (it != m_dtmf.end()) && ((int) window.m_keys.size() < DVAngel61::DTMF::MaxKeys); ++it)
    {
        if ((it->m_sequence != first->m_sequence) || (it->m_index != expected)
            || (it->m_toneUnits != first->m_toneUnits) || (it->m_gapUnits != first->m_gapUnits)) {
            break;
        }

        window.m_keys += it->m_key;
        it->m_sends++;
        it->m_lastSentMs = m_timeMs;
        expected = (expected + 1) & 0x3F;
    }

    DVAngel61::packDTMFSlot(window, slot);
    m_dtmfSlotsSent++;

    // Keys sent twice are done
    while (!m_dtmf.empty() && (m_dtmf.front().m_sends >= 2)) {
        m_dtmf.pop_front();
    }

    return true;
}

void DVAngel61SlotQueue::fillFree(uint8_t *slot)
{
    // DTMF first (3.8)
    if (fillDTMF(slot)) {
        return;
    }

    // Earliest deadline within the lookahead
    int best = -1;

    for (int i = 0; i < ItemCount; i++)
    {
        if (enabled((Item) i) && (m_deadline[i] <= m_timeMs + LookaheadMs) && ((best < 0) || (m_deadline[i] < m_deadline[best]))) {
            best = i;
        }
    }

    if (best >= 0)
    {
        fillItem((Item) best, slot);
        return;
    }

    if (m_fragments.empty() && m_repeatPending)
    {
        // The last message once more, when the transmission goes on long enough
        m_fragments.insert(m_fragments.end(), m_lastMessage.begin(), m_lastMessage.end());
        m_repeatPending = false;
    }

    if (!m_fragments.empty())
    {
        DVAngel61::packTextSlot(m_fragments.front(), slot);
        m_fragments.pop_front();
        m_fragmentsSent++;
        return;
    }

    // Routing again, or idle when it was just sent in this frame's first slot
    if (m_routingThisFrame)
    {
        DVAngel61::packIdleSlot(slot);
        m_idleSent++;
    }
    else
    {
        DVAngel61::packRoutingSlot(m_routing, slot);
        m_routingSent++;
        m_routingThisFrame = true;
    }
}

void DVAngel61SlotQueue::nextSlots(int counter, int nbSlots, bool lastFrame, uint8_t *slots)
{
    m_routingThisFrame = false;

    if (m_timeMs == 0)
    {
        // Preamble frame 0: routing and station
        DVAngel61::packRoutingSlot(m_routing, slots);
        m_routingSent++;
        if (nbSlots > 1) {
            fillItem(ItemStation, slots + DVAngel61::SlotBits);
        }
    }
    else if ((m_timeMs == DVAngel61::frameMs(m_profile)) && !m_longCallsigns.empty())
    {
        // Preamble frame 1 with long callsigns (2.6): the first of them, then the path or the
        // next of them, or the position; what this displaces is due at once
        fillItem(ItemLong0, slots);
        Item second = ItemCount;

        if (nbSlots > 1)
        {
            second = m_hasPath ? ItemPath : (m_longCallsigns.size() > 1) ? ItemLong1 : m_hasPosition ? ItemPosition : ItemCount;

            if (second != ItemCount)
            {
                fillItem(second, slots + DVAngel61::SlotBits);
            }
            else
            {
                DVAngel61::packIdleSlot(slots + DVAngel61::SlotBits);
                m_idleSent++;
            }
        }

        for (Item item : { ItemPath, ItemPosition })
        {
            if ((item != second) && enabled(item) && (m_deadline[item] > m_timeMs + LookaheadMs)) {
                m_deadline[item] = m_timeMs + LookaheadMs;
            }
        }
    }
    else if (m_timeMs == DVAngel61::frameMs(m_profile))
    {
        // Preamble frame 1: path or routing, and position or idle
        if (m_hasPath) {
            fillItem(ItemPath, slots);
        } else {
            DVAngel61::packRoutingSlot(m_routing, slots);
            m_routingSent++;
        }
        if (nbSlots > 1)
        {
            if (m_hasPosition) {
                fillItem(ItemPosition, slots + DVAngel61::SlotBits);
            } else {
                DVAngel61::packIdleSlot(slots + DVAngel61::SlotBits);
                m_idleSent++;
            }
        }
    }
    else if (lastFrame)
    {
        DVAngel61::packRoutingSlot(m_routing, slots);
        m_routingSent++;
        m_routingThisFrame = true;
        for (int s = 1; s < nbSlots; s++) {
            fillFree(slots + s * DVAngel61::SlotBits);
        }
    }
    else
    {
        int s = 0;

        if (DVAngel61::routingFrame(m_profile, counter))
        {
            // Routing is reserved in the first slot of every even frame (VHF), every third (HF)
            DVAngel61::packRoutingSlot(m_routing, slots);
            m_routingSent++;
            m_routingThisFrame = true;
            s = 1;
        }

        for (; s < nbSlots; s++) {
            fillFree(slots + s * DVAngel61::SlotBits);
        }
    }

    m_timeMs += DVAngel61::frameMs(m_profile);
}


// ---- Relaying V1 between profiles (3.1.1) ---------------------------------------------------

bool DVAngel61Repacker::pushFrame(const uint8_t *block, int inType)
{
    const int out = DVAngel61::codebooksOf(m_outType);
    const int in = DVAngel61::codebooksOf(inType);

    if ((m_outType < 0) || (out == 0) || (in < out)) {
        return false;
    }

    const int packets = DVAngel61::packetsPerFrame(DVAngel61::profileOf(inType));
    std::vector<uint8_t> packet(DVAngel61::CodebookBits * in);

    for (int p = 0; p < packets; p++)
    {
        DVAngel61::getVoicePacket(block, inType, p, packet.data());
        m_packets.emplace_back(packet.begin(), packet.begin() + DVAngel61::CodebookBits * out);
    }

    return true;
}

int DVAngel61Repacker::framesReady() const
{
    return m_outType < 0 ? 0 : (int) m_packets.size() / DVAngel61::packetsPerFrame(DVAngel61::profileOf(m_outType));
}

bool DVAngel61Repacker::popFrame(uint8_t *block)
{
    if (framesReady() == 0) {
        return false;
    }

    const int packets = DVAngel61::packetsPerFrame(DVAngel61::profileOf(m_outType));

    for (int p = 0; p < packets; p++)
    {
        DVAngel61::putVoicePacket(block, m_outType, p, m_packets.front().data());
        m_packets.pop_front();
    }

    return true;
}

// ---- Data groups (2.3) -----------------------------------------------------------------------

void DVAngel61::buildGroup(const Header *headers, const uint8_t *block, std::complex<float> *cells)
{
    const int type = headers[0].m_type;
    const Layout& l = layout(profileOf(type));
    const CodeId code = codeOf(type);
    const LDPC& c = ldpc(code);
    const Constellation k = constellationOf(type);
    const int perFrame = frameCodedBits(type);
    std::vector<uint8_t> codeword(c.getN()), positions(c.getN());
    std::vector<std::complex<float>> payload(l.payloadCount());

    c.encode(block, codeword.data());
    codewordToPositions(codeword.data(), headers[0].m_counter, code, positions.data());

    for (int f = 0; f < GroupFrames; f++)
    {
        std::complex<float> *frame = cells + (size_t) f * l.m_cells;
        uint8_t headerBits[HeaderCodedBits];
        std::complex<float> headerSymbols[HeaderCells];
        encodeHeader(headers[f], headerBits);
        mapCells(QPSK, headerBits, HeaderCells, headerSymbols);
        mapCells(k, positions.data() + (size_t) f * perFrame, l.payloadCount(), payload.data());

        for (int i = 0; i < l.m_cells; i++) {
            frame[i] = l.m_known[i];
        }
        for (size_t h = 0; h < l.m_headerCells.size(); h++) {
            frame[l.m_headerCells[h]] = headerSymbols[h % HeaderCells];
        }
        for (int i = 0; i < l.payloadCount(); i++) {
            frame[l.m_payloadCells[i]] = payload[i];
        }
    }
}

bool DVAngel61::decodeGroup(const float *llr, const Header& any, int fourthLast, uint8_t *block, Header *headers, int *iterations)
{
    const CodeId code = codeOf(any.m_type);

    if ((code == CodeNone) || !isGroupData(any.m_type)) {
        return false;
    }

    // The block is scrambled with the first frame's counter, which any header gives
    Header group[GroupFrames];
    groupHeaders(any, fourthLast == 1, group);
    const LDPC& c = ldpc(code);
    std::vector<float> coded(c.getN());
    positionsToCodeword(llr, group[0].m_counter, code, coded.data());
    const int it = c.decode(coded.data(), block, 30);

    if (iterations) {
        *iterations = it;
    }

    if (it <= 0) {
        return false;
    }

    for (int last = 0; last < 2; last++)
    {
        if ((fourthLast >= 0) && (last != fourthLast)) {
            continue;
        }

        groupHeaders(any, last == 1, group);

        if (checkBlock(block, group, GroupFrames))
        {
            if (headers) {
                std::copy(group, group + GroupFrames, headers);
            }
            return true;
        }
    }

    return false;
}
