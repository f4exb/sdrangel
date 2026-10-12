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

#ifndef MODEMDVB_DVBS2MODULATOR_H
#define MODEMDVB_DVBS2MODULATOR_H

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "dvbs2.h"
#include "dvbt2fec.h"

namespace dvbs2 {

//! DVB-S2 transmitter, for testing the receiver: a transport stream (or two, as a multiple input stream) to PLFRAMEs of
//! symbols. CCM, or VCM with a sequence of MODCODs, a frame each in turn. A MODCOD may be a VL-SNR frame's
//! (EN 302 307-2 5.5.2), as vlsnr(index) gives: negative
class MODEMDVB_API Modulator
{
public:
    //! The MODCOD for a VL-SNR frame of the VL-SNR header's index (0 to 11, not the dummy's: vlsnrDummyFrame)
    static int vlsnr(int index) { return -1 - index; }
    static bool isVLSNR(int modcod) { return modcod < 0; }
    static int vlsnrIndex(int modcod) { return -1 - modcod; }

    struct Config
    {
        int m_modcod = 13;              //!< 8PSK 2/3
        FECType m_fecType = FECType::Normal;
        bool m_pilots = true;
        int m_scramblingCode = 0;
        //! MATYPE's RO: 0 0.35, 1 0.25, 2 0.20; S2X's 4 0.15, 5 0.10, 6 0.05 (RO alternating between 11 and the
        //! code less 4)
        int m_rollOff = 0;
        std::vector<int> m_vcm;         //!< MODCODs, a frame each in turn, if not empty
        int m_streams = 1;              //!< 2 for a multiple input stream: the packets given in ISI m_isi[0], null packets in m_isi[1]
        int m_isi[2] = {5, 9};
        //! The outer ring of APSK at the pilots' amplitude (the standard's other choice), rather than unit energy
        bool m_unitOuterRing = false;
        //! A generic continuous stream (as GSE is carried) of the packets' bytes, rather than a transport stream
        bool m_generic = false;
        //! Time slicing (EN 302 307-1 Annex M), if not empty: each stream's frames in a time slice, its TSN this (the
        //! first stream's the first), their BBHEADERs a single stream's
        std::vector<int> m_tsns;
    };

    explicit Modulator(const Config& config);
    ~Modulator();
    Modulator(const Modulator&) = delete;
    Modulator& operator=(const Modulator&) = delete;

    //! 188-byte packets starting 0x47. Appends the PLFRAMEs they complete
    void modulate(const uint8_t *packets, int nbPackets, std::vector<Cf>& symbols);
    //! A dummy PLFRAME
    void dummyFrame(std::vector<Cf>& symbols);
    //! A VL-SNR dummy PLFRAME (set 2's length)
    void vlsnrDummyFrame(std::vector<Cf>& symbols);
    //! Packets a frame of the first MODCOD takes, on average
    double packetsPerFrame() const;
    const Config& config() const { return m_config; }

private:
    //! A stream's user packets, as they go into BBFRAMEs: normal mode, each packet's CRC-8 in place of the next's sync
    struct Stream
    {
        std::vector<uint8_t> m_data;
        int m_syncd = 0;            //!< Bytes from m_data's start to the first packet's
        uint8_t m_crc = 0;
        int m_isi = 0;
    };

    struct Code
    {
        Modulation m_modulation;
        CodeRate m_rate;
        FECType m_type;                 //!< The configured, but an S2X MODCOD's own (a VL-SNR frame's code's)
        int m_kbch;                     //!< A VL-SNR frame's less its shortening
        int m_kldpc;
        std::unique_ptr<dvbt2::BCH> m_bch;
        std::unique_ptr<dvbt2::LDPC> m_ldpc;
        std::vector<int> m_map;
        std::vector<Cf> m_points;
    };

    void addPacket(Stream& stream, const uint8_t *packet);
    //! The data field's bytes in a BBFRAME of the MODCOD
    int dataBytes(int modcod) const;
    bool frameReady(const Stream& stream, int modcod) const { return (int) stream.m_data.size() >= dataBytes(modcod); }
    //! The next BBFRAME of kbch bits (its last byte's padded), scrambled, its data taken from the stream
    void bbFrame(Stream& stream, int kbch, std::vector<uint8_t>& bb);
    void frame(Stream& stream, int modcod, std::vector<Cf>& symbols);
    void vlsnrFrame(Stream& stream, int index, std::vector<Cf>& symbols);
    Code& code(int modcod);
    int nextModcod() const;

    static const Cf m_turns[4];

    Config m_config;
    Scrambler m_scrambler;
    std::map<int, std::unique_ptr<Code>> m_codes;
    Stream m_streams[2];
    int m_frameCount;
    std::vector<uint8_t> m_bits;
    std::vector<Cf> m_cells;
};

} // namespace dvbs2

#endif // MODEMDVB_DVBS2MODULATOR_H
