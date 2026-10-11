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

#ifndef MODEMDVB_DVBSMODULATOR_H
#define MODEMDVB_DVBSMODULATOR_H

#include <cstdint>
#include <vector>

#include "dvbs.h"

namespace dvbs {

//! Root raised cosine pulse shaping of symbols to samples at any rate, for the test transmitters: each output the sum
//! of the symbols around it, weighted by the pulse at its distance from each (from a finely sampled table)
class MODEMDVB_API PulseShaper
{
public:
    PulseShaper();
    void configure(double sampleRate, double symbolRate, double rollOff, int span = 16);
    //! Appends the samples the symbols complete. Unit average power for unit power symbols
    void process(const Cf *symbols, int nbSymbols, std::vector<Cf>& samples);
    //! Changes the symbol rate by ppm (a transmitter's clock error)
    void setClockOffset(double ppm);

private:
    double m_sampleRate;
    double m_symbolRate;
    int m_span;
    static constexpr int m_resolution = 256;    //!< Table points a symbol
    std::vector<float> m_table;
    std::vector<Cf> m_history;              //!< The symbols the next outputs need
    double m_time;                          //!< Of the next output, in symbols from m_history[0]
    double m_step;                          //!< Symbols an output
    float m_gain;
};

//! DVB-S transmitter (EN 300 421 4.4, 4.5), for testing the receiver: transport stream packets to QPSK symbols
class MODEMDVB_API Modulator
{
public:
    explicit Modulator(CodeRate rate);

    //! 188-byte packets starting 0x47. Appends the QPSK symbols (unit power) they complete: packets are randomised a
    //! group of 8 at a time, so the last of a call may wait for the next
    void modulate(const uint8_t *packets, int nbPackets, std::vector<Cf>& symbols);
    //! Symbols a packet takes, on average
    double symbolsPerPacket() const;

private:
    void encodeGroup();

    std::vector<PunctureEntry> m_pattern;
    int m_period;
    std::vector<uint8_t> m_pending;     //!< Packets for the next group of 8
    std::vector<uint8_t> m_fifo[12];    //!< Outer interleaver, branch j delays 17 j bytes
    int m_fifoPos[12];
    uint8_t m_encoderState;
    std::vector<uint8_t> m_bits;        //!< Information bits waiting for a whole puncturing period
    std::vector<uint8_t> m_serial;      //!< Punctured bits waiting for a symbol's pair
};

} // namespace dvbs

#endif // MODEMDVB_DVBSMODULATOR_H
