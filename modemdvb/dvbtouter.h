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

#ifndef MODEMDVB_DVBTOUTER_H
#define MODEMDVB_DVBTOUTER_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "dvbt.h"

namespace ReedSolomon {
template <std::size_t SYMBOLS, std::size_t PAYLOAD>
struct RS;
}

namespace dvbt {

struct SyndromeTables;

// Outer decoding (4.3.1 and 4.3.2), from the bit stream out of the Viterbi decoder:
// find the packet sync bytes, outer (Forney) de-interleave, RS(204,188) decode, de-randomize
class MODEMDVB_API OuterDecoder
{
public:
    typedef std::function<void(const uint8_t *packet)> PacketCallback; //!< 188-byte TS packet

    struct Stats
    {
        uint64_t m_packets;         //!< TS packets output
        uint64_t m_rsCorrected;     //!< Packets that RS corrected
        uint64_t m_rsBytesCorrected;
        uint64_t m_rsBitsCorrected; //!< In packets RS corrected, so BER after Viterbi is about this / (m_packets * 204 * 8)
        uint64_t m_rsFailed;        //!< Packets RS could not correct, output with transport_error_indicator set
        uint64_t m_syncLosses;

        Stats() { clear(); }

        void clear()
        {
            m_packets = 0;
            m_rsCorrected = 0;
            m_rsBytesCorrected = 0;
            m_rsBitsCorrected = 0;
            m_rsFailed = 0;
            m_syncLosses = 0;
        }
    };

    OuterDecoder();

    void setPacketCallback(PacketCallback callback) { m_callback = callback; }
    void reset();
    void decode(const uint8_t *bits, int nbBits); //!< One bit per byte, in transmission order
    bool synced() const { return m_synced; }
    const Stats& getStats() const { return m_stats; }

    // For the test transmitter
    static void randomize(uint8_t packets[8][tsPacketSize]); //!< Energy dispersal on a group of 8 packets
    static void rsEncode(const uint8_t *packet, uint8_t *rsPacket);

private:
    void search();
    void processByte(uint8_t byte);
    void processPacket(uint8_t *packet);

    static constexpr int m_syncPackets = 4;     //!< Sync bytes required to sync
    static constexpr int m_maxSyncMisses = 8;   //!< Consecutive packets without a sync byte before losing sync
    static constexpr int m_interleaverI = 12;
    static constexpr int m_interleaverM = 17;
    static constexpr uint8_t m_syncByte = 0x47;
    static constexpr uint8_t m_invertedSyncByte = 0xb8;
    static const ReedSolomon::RS<255, 239> m_rs;
    static const SyndromeTables m_syndromeTables;

    static bool hasErrors(const uint8_t *packet);
    static std::vector<uint8_t> makePRBS();
    static uint8_t getByte(const uint8_t *bits);

    PacketCallback m_callback;
    Stats m_stats;

    std::vector<uint8_t> m_bits;    //!< Bits being searched for sync
    bool m_synced;
    int m_bitCount;
    uint8_t m_byte;
    int m_bytePos;                  //!< Byte position in the 204-byte packet
    int m_syncMisses;

    std::vector<uint8_t> m_fifo[m_interleaverI];
    int m_fifoPos[m_interleaverI];
    int m_packetsInDeinterleaver;   //!< Until it has filled, its output is meaningless
    uint8_t m_packet[rsPacketSize];
    int m_packetPos;

    std::vector<uint8_t> m_prbs;    //!< 8 * 188 bytes, 0 at sync byte positions
    int m_prbsPacket;               //!< Packet within the group of 8
    bool m_prbsSynced;              //!< An inverted sync byte has been seen
};

} // namespace dvbt

#endif // MODEMDVB_DVBTOUTER_H
