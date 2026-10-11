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

#ifndef MODEMDVB_DVBT2MODULATOR_H
#define MODEMDVB_DVBT2MODULATOR_H

#include <memory>
#include <random>
#include <vector>

#include "dvbt2.h"
#include "dvbt2fec.h"

class FFTEngine;

namespace dvbt2 {

//! DVB-T2 transmitter, for testing the receiver: one type 1 PLP carrying a transport stream, SISO or MISO (both
//! groups of transmitters' signals), no PAPR reduction (but reserved carriers may be signalled). FEF parts, if any,
//! are a P1 then noise
class MODEMDVB_API Modulator
{
public:
    struct Config
    {
        int m_fftSize = 32768;
        bool m_extended = true;
        GuardInterval m_guard = GuardInterval::G1_128;
        PilotPattern m_pattern = PilotPattern::PP7;
        bool m_toneReservation = false;
        L1Constellation m_l1Mod = L1Constellation::QAM64;
        Constellation m_constellation = Constellation::QAM256;
        CodeRate m_codeRate = CodeRate::R2_3;
        FECType m_fecType = FECType::Normal;
        bool m_rotation = true;
        int m_dataSymbols = 0;      //!< 0 for the most that fit in 250 ms
        int m_frames = 2;           //!< T2-frames per super-frame
        int m_tiBlocks = 1;         //!< 0 for no time interleaving
        int m_interleavingFrames = 1; //!< PI: T2-frames an Interleaving Frame is spread over (then one TI-block)
        int m_frameInterval = 1;    //!< IJUMP: the PLP is in every IJUMP-th T2-frame (m_frames a multiple of PI x IJUMP)
        bool m_highEfficiency = false;
        int m_cellId = 0;
        int m_bandwidth = 8000000;
        int m_fefLength = 0;        //!< Samples in each FEF part, from its P1 to the next T2-frame's, 0 for none
        int m_fefInterval = 1;      //!< T2-frames between FEF parts (m_frames must be a multiple of it)
        int m_fefS1 = 2;            //!< Of the FEF parts' P1s: 2 non-T2, 3 T2-Lite SISO
        bool m_lite = false;        //!< T2-Lite profile (16K LDPC only, with 1/3 and 2/5 too)
        bool m_miso = false;        //!< MISO: modulate() gives the second group of transmitters' signal too
    };

    explicit Modulator(const Config& config);
    ~Modulator();

    //! TS packets each frame takes, on average
    int packetsPerFrame() const { return m_packetsPerFrame; }
    //! Samples in a frame
    int samplesPerFrame() const;
    const L1Pre& l1Pre() const { return m_pre; }
    const Config& config() const { return m_config; }
    //! Modulate whole frames from the packets (packetsPerFrame() each), appending to samples. Any remainder is kept
    //! In MISO, appends the second group of transmitters' samples to samples2, if given
    void modulate(const uint8_t *packets, int nbPackets, std::vector<Cf>& samples, std::vector<Cf> *samples2 = nullptr);

private:
    static constexpr int m_maxTICells = (1 << 19) + (1 << 15);  //!< MTI (6.5.2)

    static int s2Field1(int fftSize, GuardInterval guard, bool lite);   //!< S2 field 1 (table 19(a))
    void setup();
    void encodeFECBlock(const std::vector<uint8_t>& bbframe, std::vector<Cf>& cells);
    void encodeInterleavingFrame();
    void buildFrameCells(std::vector<Cf>& plpCells, std::vector<Cf>& samples);
    int blocksNeeded() const;       //!< For the next frame
    void buildFrame(std::vector<Cf>& samples);
    void addP1(std::vector<Cf>& samples);
    void makeP1(int s1, int s2, std::vector<Cf>& p1);
    void addFEF(std::vector<Cf>& samples);
    void addSymbol(int l, const std::vector<Cf>& cells, std::vector<Cf>& samples);

    Config m_config;
    const Tables *m_tables;
    L1Pre m_pre;
    L1Post m_post;
    L1Codec m_l1Codec;
    std::unique_ptr<LDPC> m_ldpc;
    std::unique_ptr<BCH> m_bch;
    std::unique_ptr<BBFrameEncoder> m_bbEncoder;
    std::unique_ptr<CellInterleaver> m_cellInterleaver;
    std::vector<int> m_bitMap;
    int m_ncells;
    int m_blocksPerFrame;           //!< In an Interleaving Frame
    std::vector<Cf> m_ifCells;      //!< Of the Interleaving Frame being sent
    int m_packetsPerFrame;
    int m_lf;                       //!< Symbols per frame, excluding P1
    bool m_frameClosing;
    int m_frameIdx;
    std::vector<std::vector<uint8_t>> m_bbframes;
    FFTEngine *m_fft;
    std::vector<Cf> m_p1;
    std::vector<Cf> *m_tx2 = nullptr;   //!< The second group's samples, during modulate()
    std::vector<Cf> m_carrierCells;
    float misoSign(int k, int l) const;
    std::vector<Cf> m_fefP1;
    std::mt19937 m_fefRng;
};

} // namespace dvbt2

#endif // MODEMDVB_DVBT2MODULATOR_H
