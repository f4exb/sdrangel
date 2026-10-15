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

#ifndef MODEMDVB_DVBTMODULATOR_H
#define MODEMDVB_DVBTMODULATOR_H

#include <cstdint>
#include <vector>

#include "dvbt.h"
#include "dvbtinner.h"

class FFTEngine;

namespace dvbt {

// DVB-T transmitter, for testing the receiver. Hierarchical modes have a high priority stream (codeRate) and a low
// priority one (lpCodeRate), each with its own outer and inner coding
class MODEMDVB_API Modulator
{
public:
    //! cellId is signalled in the TPS if not negative
    Modulator(Mode mode, Constellation constellation, CodeRate codeRate, GuardInterval guard, int cellId = -1,
        Hierarchy hierarchy = Hierarchy::None, CodeRate lpCodeRate = CodeRate::R1_2);
    ~Modulator();

    //! Packets are 188 bytes, starting 0x47, a multiple of 8 of each stream (lpPackets for the low priority stream
    //! of a hierarchical mode, which carries null packets if there are none). Appends samples, with unit average
    //! power, for each OFDM symbol completed, which takes the bits of both streams
    void modulate(const uint8_t *packets, int nbPackets, std::vector<Cf>& samples, const uint8_t *lpPackets = nullptr,
        int nbLpPackets = 0);
    //! Packets a symbol carries, on average, of the high (or the only) and the low priority streams
    double hpPacketsPerSymbol() const { return m_streams[0].m_infoBits / (rsPacketSize * 8.0); }
    double lpPacketsPerSymbol() const { return m_hierarchy == Hierarchy::None ? 0.0 : m_streams[1].m_infoBits / (rsPacketSize * 8.0); }

private:
    // Outer coding, interleaving and convolutional coding of one stream
    struct Stream
    {
        CodeRate m_rate = CodeRate::R1_2;
        int m_infoBits = 0;                 //!< A symbol takes
        std::vector<uint8_t> m_fifo[12];    //!< Outer interleaver, branch j delays j * 17 bytes
        int m_fifoPos[12] = {};
        int m_bytePos = 0;
        uint8_t m_encoderState = 0;
        std::vector<uint8_t> m_xy;          //!< Encoded X/Y pairs waiting for a symbol
        std::vector<uint8_t> m_punctured;

        void encode(const uint8_t *packets, int nbPackets);
    };

    void modulateSymbol(std::vector<Cf>& samples);

    Mode m_mode;
    Constellation m_constellation;
    CodeRate m_codeRate;
    GuardInterval m_guard;
    int m_cellId;
    const Tables& m_tables;
    Hierarchy m_hierarchy;
    CodeRate m_lpCodeRate;
    InnerDecoder m_inner;
    FFTEngine *m_fft;

    Stream m_streams[2];                //!< High (or the only) and low priority
    std::vector<Cf> m_cells;
    std::vector<float> m_tpsValue;      //!< Current TPS carrier values
    uint8_t m_tpsBits[symbolsPerFrame];
    int m_symbolIndex;
    int m_frameNumber;
};

} // namespace dvbt

#endif // MODEMDVB_DVBTMODULATOR_H
