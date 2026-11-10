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

#ifndef INCLUDE_DSP_DV_DVYSF_H
#define INCLUDE_DSP_DV_DVYSF_H

#include <cstdint>

#include "export.h"

/**
 * Yaesu System Fusion (C4FM) channel coding: FICH, DCH and V/D type 2 voice.
 *
 * A frame is 480 dibits (100 ms): 20 sync, 100 FICH, 360 payload. Bit arrays hold
 * one bit per byte, dibit arrays one dibit per byte (first bit in the MSB).
 * Soft inputs are LLRs, two per dibit, in bit order.
 */
class SDRBASE_API DVYSF
{
public:
    //! Frame Information Channel fields, in their bit order
    struct FICH
    {
        int m_fi = 0;    //!< Frame information: 0 header, 1 communication, 2 terminator, 3 test
        int m_cs = 0;    //!< Callsign type (2 bits)
        int m_cm = 0;    //!< Call mode: 0 group CQ, 1 radio ID, 3 individual
        int m_bn = 0;    //!< Block number (2 bits)
        int m_bt = 0;    //!< Block total (2 bits)
        int m_fn = 0;    //!< Frame number (3 bits)
        int m_ft = 0;    //!< Frame total (3 bits)
        int m_dev = 0;   //!< Frequency deviation: 0 wide, 1 narrow
        int m_mr = 0;    //!< Message routing (3 bits): 0 direct wave
        int m_voip = 0;  //!< VoIP path: 0 local, 1 internet
        int m_dt = 0;    //!< Data type: 0 V/D type 1, 1 data FR, 2 V/D type 2, 3 voice FR
        int m_sql = 0;   //!< Squelch type bit
        int m_sq = 0;    //!< Squelch code (7 bits)
    };

    static const int m_frameDibits = 480;
    static const int m_syncDibits = 20;
    static const int m_fichBits = 32;
    static const int m_fichDibits = 100;
    static const int m_headerDCHDibits = 180;   //!< One DCH of a header/terminator frame (CSD1 or CSD2)
    static const int m_vd2DCHDibits = 100;      //!< DCH of a V/D type 2 communication frame
    static const int m_vd2VoiceBits = 104;
    static const int m_vd2VoiceDibits = 52;
    static const int m_ambeBits = 49;

    // ---- FICH ------------------------------------------------------------------
    static void fichToBits(const FICH& fich, uint8_t *bits);
    static void bitsToFich(const uint8_t *bits, FICH& fich);
    //! 32 bits -> CRC-16 -> 4 x Golay(24,12) -> 4 flush bits -> conv K=5 -> interleave: 100 dibits
    static void fichEncode(const FICH& fich, uint8_t *dibits);
    //! 100 dibits -> FICH. Returns the number of corrected Golay bits or -1 when the CRC fails.
    static int fichDecode(const uint8_t *dibits, FICH& fich);
    //! Soft input variant from 200 LLRs
    static int fichDecodeSoft(const float *llr, FICH& fich);

    // ---- DCH -------------------------------------------------------------------
    //! nbBytes (10 for V/D type 2, 20 for header/terminator) -> whitening -> CRC-16 -> 4 flush bits
    //! -> conv K=5 -> interleave (100 dibits for 10 bytes, 180 dibits for 20 bytes)
    static void dchEncode(const uint8_t *data, int nbBytes, uint8_t *dibits);
    //! Inverse of dchEncode. Returns true when the CRC matches.
    static bool dchDecode(const uint8_t *dibits, int nbBytes, uint8_t *data);
    //! Soft input variant from 2 * (8 * (nbBytes + 2) + 4) LLRs
    static bool dchDecodeSoft(const float *llr, int nbBytes, uint8_t *data);

    // ---- V/D type 2 voice ------------------------------------------------------
    //! 49 AMBE parameter bits -> 104 bit voice channel (27 bits x3 + 22 bits + 0) -> whitening -> interleave: 52 dibits
    static void vd2VoiceEncode(const uint8_t *ambe49, uint8_t *dibits);
    //! 52 dibits -> 49 parameter bits by majority vote of the tripled bits
    static void vd2VoiceDecode(const uint8_t *dibits, uint8_t *ambe49);
    //! Soft input variant from 104 LLRs: the tripled bits are combined before the decision
    //! errors: the repeated bits the majority vote outvoted (of 81), the channel's errors it corrected
    static void vd2VoiceDecodeSoft(const float *llr, uint8_t *ambe49, int *errors = nullptr);

    // ---- Voice full rate (VW) ------------------------------------------------------------
    //! An IMBE 7200x4400 frame's 144 on-air bits in YSF's voice full rate order to P25 Phase 1's,
    //! and back: the same frame (the 8 x 23 codewords, c1 to c6 modulated as P25's) under two
    //! interleaves (mbelib 3's MBE_LAYOUT_YSF_VW and MBE_LAYOUT_P25P1; the YSF table is dsdcc's)
    static void imbeYSFToP25(const uint8_t *ysf, uint8_t *p25);
    static void imbeYSFToP25(const float *ysf, float *p25);
    static void imbeP25ToYSF(const uint8_t *p25, uint8_t *ysf);
    //! The P25 on-air position of each YSF on-air bit (a permutation of 0..143)
    static const int *imbeYSFToP25Table();

    // ---- Tables ----------------------------------------------------------------
    static const int m_fichInterleave[100];   //!< raw[table[i]] = onAir[i]
    static const int m_dchInterleave[180];
    static const int m_vd2Interleave[104];    //!< bits

private:
    static void vd2Bits(const uint8_t *ambe49, uint8_t *bits104);
    static int fichFromCoded(const uint8_t *coded, FICH& fich);
    static bool dchFromDecoded(const uint8_t *bits, int nbBytes, uint8_t *data);
};

#endif // INCLUDE_DSP_DV_DVYSF_H
