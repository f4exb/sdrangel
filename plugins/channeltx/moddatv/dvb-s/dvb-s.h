///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

// DVB-S coder - See ETSI EN 300 421 V1.1.2
class DVBS {

    static const uint8_t tsSync = 0x47;                 //!< Transport stream sync byte - should be at start of every TS packet
    static const int interleaveDepth = 12;              //!< Interleaving depth
    static const int rsK = 239;                         //!< Reed-Solomon input length
    static const int rs2T = 16;                         //!< Reed-Solomon number check/parity symbols
    static const int rsN = rsK + rs2T;                  //!< Reed-Solomon block length

public:

    static const int tsPacketLen = 188;                 //!< Length of transport stream packets
    static const int rsPacketLen = 204;                 //!< Length of packet after adding RS parity bytes
    static const int m_maxIQSymbols = 204*8;            //!< Maximum number of IQ symbols per packet

    enum CodeRate {RATE_1_2, RATE_2_3, RATE_3_4, RATE_5_6, RATE_7_8};

    DVBS();
    ~DVBS();

    int encode(const uint8_t *ts, uint8_t *iq);
    void setCodeRate(CodeRate codeRate);

private:

    uint8_t *m_packet;                                  //!< Buffer to hold input data + partiy bytes

    int m_prbsPacketCount;                              //!< Counts 0-7
    int m_prbsIdx;                                      //!< Index in to m_prbsLUT

    uint8_t **m_interleaveFIFO;                         //!< FIFOs for convolutional interleaving
    int *m_interleaveLen;                               //!< Length of each FIFO
    int *m_interleaveIdx;                               //!< Write index in to FIFOs

    CodeRate m_codeRate;                                //!< Convolution coding rate
    unsigned m_delayLine;                               //!< Delay line for convolution encoder
    int m_punctureState;                                //!< Puncturing state
    uint8_t m_prevIQ;                                   //!< Saved bit from previous packet
    bool m_prevIQValid;                                 //!< Whether there is a saved bit

protected:

    static const uint8_t m_prbsLUT[tsPacketLen*8-1];    //!< PRBS lookup table

    static const uint8_t rsPoly[rs2T];                  //!< Reed-Solomon generator polynomial

    static const uint8_t m_gfLog[256];                  //!< GF(2^8) log lookup table
    static const uint8_t m_gfExp[512];                  //!< GF(2^8) exponent lookup table

    static uint8_t gfMul(uint8_t a, uint8_t b);         //!< GF(2^8) multiplication

    void scramble(const uint8_t *packetIn, uint8_t *packetOut);
    void reedSolomon(uint8_t *packet);
    void interleave(uint8_t *packet);
    int convolution(const uint8_t *packet, uint8_t *iq);

};

