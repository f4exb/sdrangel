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

#include <cstring>

#include "util/reedsolomon.h"

#include "dvbtouter.h"

namespace dvbt {

// RS(204,188, t=8) shortened from RS(255,239): p(x) = x^8 + x^4 + x^3 + x^2 + 1, roots alpha^0..alpha^15 (4.3.2)
const ReedSolomon::RS<255, 239> OuterDecoder::m_rs;

// Most packets have no errors, which the syndromes, the received polynomial at the roots, being 0 shows. They're
// found by Horner's rule with a table for multiplying by each root, 16 independent chains, so quickly; the full
// decoder (which finds them again) is only needed if one isn't 0
struct SyndromeTables
{
    uint8_t m_multiply[16][256]; //!< By alpha^j

    SyndromeTables()
    {
        auto multiply = [](int a, int b) {
            int product = 0;

            while (b)
            {
                if (b & 1) {
                    product ^= a;
                }

                a <<= 1;

                if (a & 0x100) {
                    a ^= 0x11d;
                }

                b >>= 1;
            }

            return product;
        };

        int root = 1;

        for (int j = 0; j < 16; j++)
        {
            for (int x = 0; x < 256; x++) {
                m_multiply[j][x] = (uint8_t) multiply(x, root);
            }

            root = multiply(root, 2);
        }
    }
};

const SyndromeTables OuterDecoder::m_syndromeTables;

bool OuterDecoder::hasErrors(const uint8_t *packet)
{
    uint8_t s[16] = {};

    for (int i = 0; i < rsPacketSize; i++)
    {
        const uint8_t r = packet[i];

        for (int j = 0; j < 16; j++) {
            s[j] = m_syndromeTables.m_multiply[j][s[j]] ^ r;
        }
    }

    uint8_t any = 0;

    for (int j = 0; j < 16; j++) {
        any |= s[j];
    }

    return any != 0;
}

// Energy dispersal PRBS 1 + x^14 + x^15, initialised to 100101010000000 at each inverted sync byte (4.3.1)
std::vector<uint8_t> OuterDecoder::makePRBS()
{
    std::vector<uint8_t> prbs(8 * tsPacketSize);
    unsigned int st = 0251; // Initialisation sequence, bit-reversed

    for (int i = 1; i < 8 * tsPacketSize; i++)
    {
        uint8_t out = 0;

        for (int n = 0; n < 8; n++)
        {
            unsigned int bit = ((st >> 13) ^ (st >> 14)) & 1;
            out = (out << 1) | bit;
            st = (st << 1) | bit;
        }

        prbs[i] = (i % tsPacketSize) ? out : 0; // Sync bytes are not randomized, but the PRBS keeps running
    }

    return prbs;
}

OuterDecoder::OuterDecoder() :
    m_packet{},
    m_prbs(makePRBS())
{
    for (int j = 0; j < m_interleaverI; j++) {
        m_fifo[j].resize((m_interleaverI - 1 - j) * m_interleaverM);
    }

    reset();
}

void OuterDecoder::reset()
{
    m_bits.clear();
    m_synced = false;
    m_bitCount = 0;
    m_byte = 0;
    m_bytePos = 0;
    m_syncMisses = 0;
    m_packetPos = 0;
    m_packetsInDeinterleaver = 0;
    m_prbsPacket = 0;
    m_prbsSynced = false;

    for (int j = 0; j < m_interleaverI; j++)
    {
        std::fill(m_fifo[j].begin(), m_fifo[j].end(), 0);
        m_fifoPos[j] = 0;
    }
}

void OuterDecoder::decode(const uint8_t *bits, int nbBits)
{
    int i = 0;

    while (i < nbBits)
    {
        if (!m_synced)
        {
            m_bits.insert(m_bits.end(), bits + i, bits + nbBits);
            i = nbBits;
            search();
        }
        else if ((m_bitCount == 0) && (nbBits - i >= 8))
        {
            // Whole bytes, packed with a multiply: bit k of the eight (one per byte, little endian) moves to bit 63 - k
            while ((nbBits - i >= 8) && m_synced)
            {
                uint64_t x;
                memcpy(&x, bits + i, 8);
                i += 8;
                processByte((uint8_t) (((x & 0x0101010101010101ULL) * 0x8040201008040201ULL) >> 56));
            }

            // Lost sync: search the remaining bits
            if (!m_synced)
            {
                m_bits.assign(bits + i, bits + nbBits);
                i = nbBits;
                search();
            }
        }
        else
        {
            m_byte = (m_byte << 1) | (bits[i++] & 1);

            if (++m_bitCount == 8)
            {
                m_bitCount = 0;
                processByte(m_byte);

                // Lost sync: search the remaining bits
                if (!m_synced)
                {
                    m_bits.assign(bits + i, bits + nbBits);
                    i = nbBits;
                    search();
                }
            }
        }
    }
}

uint8_t OuterDecoder::getByte(const uint8_t *bits)
{
    uint8_t byte = 0;

    for (int i = 0; i < 8; i++) {
        byte = (byte << 1) | bits[i];
    }

    return byte;
}

// Sync bytes are carried by interleaver branch 0, which has no delay in the transmitter,
// so they appear every 204 bytes in the received stream
void OuterDecoder::search()
{
    const int packetBits = rsPacketSize * 8;
    const int window = packetBits * (m_syncPackets - 1) + 8;
    int start = 0; // Bits before it have been searched, and are discarded once at the end, not a packet at a time

    while ((int) m_bits.size() - start >= window + packetBits)
    {
        const uint8_t *bits = m_bits.data() + start;

        for (int offset = 0; offset < packetBits; offset++)
        {
            int p;

            for (p = 0; p < m_syncPackets; p++)
            {
                uint8_t byte = getByte(&bits[offset + p * packetBits]);

                if ((byte != m_syncByte) && (byte != m_invertedSyncByte)) {
                    break;
                }
            }

            if (p == m_syncPackets)
            {
                std::vector<uint8_t> remaining(m_bits.begin() + start + offset, m_bits.end());
                m_bits.clear();
                m_synced = true;
                m_bitCount = 0;
                m_bytePos = 0;
                m_syncMisses = 0;
                m_packetPos = 0;
                m_packetsInDeinterleaver = 0;
                m_prbsSynced = false;
                decode(remaining.data(), (int) remaining.size());
                return;
            }
        }

        start += packetBits;
    }

    m_bits.erase(m_bits.begin(), m_bits.begin() + start);
}

// Inline, as it's called for every byte (builds that only inline functions marked so wouldn't)
inline void OuterDecoder::processByte(uint8_t byte)
{
    if (m_bytePos == 0)
    {
        if ((byte == m_syncByte) || (byte == m_invertedSyncByte))
        {
            m_syncMisses = 0;
        }
        else if (++m_syncMisses >= m_maxSyncMisses)
        {
            m_synced = false;
            m_stats.m_syncLosses++;
            return;
        }
    }

    // De-interleave: branch j has delay (11 - j) * 17 bytes
    int j = m_bytePos % m_interleaverI;
    uint8_t out;

    if (m_fifo[j].empty())
    {
        out = byte;
    }
    else
    {
        out = m_fifo[j][m_fifoPos[j]];
        m_fifo[j][m_fifoPos[j]] = byte;

        if (++m_fifoPos[j] == (int) m_fifo[j].size()) {
            m_fifoPos[j] = 0;
        }
    }

    m_packet[m_bytePos] = out;

    if (++m_bytePos == rsPacketSize)
    {
        m_bytePos = 0;

        if (m_packetsInDeinterleaver < m_interleaverI - 1) {
            m_packetsInDeinterleaver++;
        } else {
            processPacket(m_packet);
        }
    }
}

void OuterDecoder::processPacket(uint8_t *packet)
{
    uint8_t received[rsPacketSize];
    int corrected = 0;

    if (hasErrors(packet))
    {
        memcpy(received, packet, rsPacketSize);
        corrected = m_rs.decode(packet, tsPacketSize, packet + tsPacketSize);
    }

    if (corrected < 0) {
        m_stats.m_rsFailed++;
    }
    else if (corrected > 0)
    {
        m_stats.m_rsCorrected++;
        m_stats.m_rsBytesCorrected += corrected;

        for (int i = 0; i < rsPacketSize; i++)
        {
            for (uint8_t diff = received[i] ^ packet[i]; diff; diff &= diff - 1) {
                m_stats.m_rsBitsCorrected++;
            }
        }
    }

    if (packet[0] == m_invertedSyncByte)
    {
        m_prbsPacket = 0;
        m_prbsSynced = true;
    }
    else if (!m_prbsSynced)
    {
        return; // Can't de-randomize until the start of a group of 8 packets is known
    }

    packet[0] = m_syncByte;
    const uint8_t *prbs = &m_prbs[m_prbsPacket * tsPacketSize];

    for (int i = 1; i < tsPacketSize; i++) {
        packet[i] ^= prbs[i];
    }

    m_prbsPacket = (m_prbsPacket + 1) % 8;

    if (corrected < 0) {
        packet[1] |= 0x80; // transport_error_indicator
    }

    m_stats.m_packets++;

    if (m_callback) {
        m_callback(packet);
    }
}

void OuterDecoder::randomize(uint8_t packets[8][tsPacketSize])
{
    static const std::vector<uint8_t> prbs = makePRBS();

    for (int p = 0; p < 8; p++)
    {
        packets[p][0] = p == 0 ? m_invertedSyncByte : m_syncByte;

        for (int i = 1; i < tsPacketSize; i++) {
            packets[p][i] ^= prbs[p * tsPacketSize + i];
        }
    }
}

void OuterDecoder::rsEncode(const uint8_t *packet, uint8_t *rsPacket)
{
    memcpy(rsPacket, packet, tsPacketSize);
    m_rs.encode(packet, tsPacketSize, rsPacket + tsPacketSize);
}

} // namespace dvbt
