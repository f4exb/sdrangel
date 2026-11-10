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

#ifndef INCLUDE_DSP_DV_DVP25P2_H
#define INCLUDE_DSP_DV_DVP25P2_H

#include <cstdint>

#include "export.h"

/**
 * APCO P25 Phase 2 (TIA-102.BBAB/BBAC, two slot TDMA at 6000 symbols/s)
 * channel coding: the ISCH and DUID codes, the superframe scrambler, the
 * voice codeword interleave, the SACCH, FACCH and encryption sync
 * Reed-Solomon coding and the MAC PDU CRC.
 *
 * A burst is 180 dibits (30 ms): a 20 dibit ISCH then 160 payload dibits
 * with the four DUID dibits at burst dibits 20, 57, 142 and 179. Twelve
 * bursts (timeslots 0-11) make a superframe; the ISCH is the sync word in
 * timeslots 2, 3, 6, 7, 10 and 11 and carries the timeslot number in the
 * others. Timeslots alternate between the two channels (0, 1, 0, 1, ...,
 * 1, 0). The bit orders and the layouts follow OP25 (p25p2_*.cc), against
 * which this code is checked.
 *
 * Bit arrays hold one bit per byte, first transmitted first; dibit arrays
 * hold the 2 bit symbols (MSB first on air); hexbit arrays one 6 bit
 * symbol per byte.
 */
class SDRBASE_API DVP25P2
{
public:
    static const int m_burstDibits = 180;
    static const int m_ischDibits = 20;
    static const int m_timeslots = 12;
    static const int m_superframeDibits = 12 * 180;
    static const uint64_t m_syncISCH = 0x575D57F7FFULL;

    //! Data unit identifiers as the (8,4) DUID code carries them
    enum DUID
    {
        DUID4V = 0,
        DUIDSACCHScrambled = 3,
        DUIDLCCHScrambled = 4,
        DUID2V = 6,
        DUIDFACCHScrambled = 9,
        DUIDSACCH = 12,
        DUIDLCCH = 13,
        DUIDFACCH = 15
    };

    enum ISCHType { ISCHNone, ISCHSync, ISCHInfo };

    enum MacOpcode { MacSignal = 0, MacPTT = 1, MacEndPTT = 2, MacIdle = 3, MacActive = 4, MacHangtime = 6 };

    //! Information ISCH content
    struct ISCH
    {
        int m_channel = 0;      //!< 0 or 1
        int m_location = 0;     //!< Timeslot pair within the superframe, 0-2
        int m_frame = 0;        //!< Superframe indicator (0-1)
        int m_count = 0;        //!< Superframe count within an ultraframe (0-3)

        int getTimeslot() const { return 4 * m_location + m_channel; }
    };

    static const int m_duidDibits[4];       //!< Burst dibits carrying the DUID: 20, 57, 142, 179
    static const int m_voiceDibits[4];      //!< First burst dibit of the four voice codewords (36 dibits each)
    static const int m_essBDibit = 94;      //!< ESS-B: 12 dibits (4 hexbits) in a 4V burst; ESS-A: 28 hexbits from the same dibit in a 2V burst

    static int timeslotChannel(int timeslot);
    static bool timeslotHasSync(int timeslot) { return (timeslot % 4) >= 2; }

    // ---- ISCH (40,9,16) and DUID (8,4) codes -------------------------------------------------
    //! Information ISCH -> 40 bit codeword (MSB first on air)
    static uint64_t ischEncode(const ISCH& isch);
    //! Soft decision decoding of the 40 ISCH LLRs (positive favours 0) against the sync word
    //! and the 128 information codewords. distance is the Hamming distance from the hard
    //! decisions to the chosen codeword; the caller sets the acceptance limit (7 corrects
    //! every pattern the codes are guaranteed to).
    static ISCHType ischDecode(const float *llr, ISCH& isch, int& distance);
    //! DUID -> the 8 bits of the four DUID dibits
    static uint8_t duidEncode(int duid);
    //! Soft decision decoding of the 8 DUID LLRs. Returns the DUID and the Hamming distance.
    static int duidDecode(const float *llr, int& distance);

    // ---- Scrambler -------------------------------------------------------------------------
    //! The 4320 bit scrambling sequence of a superframe for a system (WACN, system ID, NAC).
    //! Payload dibit d (20-179) of timeslot t is scrambled by mask dibit t * 180 + d - 10.
    static void scramblerMask(uint32_t wacn, uint16_t sysid, uint16_t nac, uint8_t *bits);
    static int maskDibit(int timeslot, int burstDibit) { return timeslot * m_burstDibits + burstDibit - 10; }

    // The voice codewords are AMBE+2 3600x2450 frames in the DMR on-air order (the OP25
    // interleave table reduces to the DMR one, see dvtest): DVAMBE handles them.

    // ---- SACCH and FACCH --------------------------------------------------------------------
    static const int m_sacchBits = 312;     //!< Coded SACCH bits in a burst
    static const int m_sacchPduBits = 168;
    static const int m_facchBits = 270;
    static const int m_facchPduBits = 144;
    //! Burst dibit of coded bit pair k (the DUID dibits and, for the FACCH, dibits 89-109 are skipped)
    static int sacchDibit(int k);
    static int facchDibit(int k);
    //! Coded bits -> PDU bits (CRC12 checked). Returns the number of RS errata or -1.
    static int sacchDecode(const uint8_t *bits, uint8_t *pdu);
    static int facchDecode(const uint8_t *bits, uint8_t *pdu);
    //! The LCCH (the TDMA control channel's outbound signalling, DUID 13): coded as the SACCH, but
    //! its 180 information bits are a 164 bit MAC PDU (its colour code in bits 152-163) and a
    //! CRC-16 (CCITT, from zero, inverted) rather than 168 and a CRC-12 (OP25, SDRTrunk)
    static const int m_lcchPduBits = 164;
    static int lcchDecode(const uint8_t *bits, uint8_t *pdu);
    static void lcchEncode(const uint8_t *pdu, uint8_t *bits);
    static uint16_t crc16(const uint8_t *bits, int nbBits);
    //! PDU bits -> coded bits (CRC12 appended, RS(63,35) parity, punctured hexbits dropped)
    static void sacchEncode(const uint8_t *pdu, uint8_t *bits);
    static void facchEncode(const uint8_t *pdu, uint8_t *bits);
    //! MAC PDU CRC12: x^12 + x^11 + x^7 + x^4 + x^2 + x + 1, no initial value, result inverted
    static uint16_t crc12(const uint8_t *bits, int nbBits);

    // ---- Encryption sync (ESS) --------------------------------------------------------------
    //! 16 ESS-B hexbits (4 per 4V burst) and 28 ESS-A hexbits (2V burst) -> algid, key ID, MI.
    //! Returns the number of corrected hexbits or -1.
    static int essDecode(const uint8_t *essB, const uint8_t *essA, uint8_t& algid, uint16_t& keyid, uint8_t *mi);
    static void essEncode(uint8_t algid, uint16_t keyid, const uint8_t *mi, uint8_t *essB, uint8_t *essA);

    static const char *duidName(int duid);
    static const char *macOpcodeName(int opcode);
    static const char *algorithmName(int algid);

private:
    static int acchDecode(const uint8_t *bits, int nbBits, int firstHexbit, uint8_t *pdu, int pduBits, int crcBits = 12);
    static void acchEncode(const uint8_t *pdu, int pduBits, int firstHexbit, uint8_t *bits, int nbBits, int crcBits = 12);
};

#endif // INCLUDE_DSP_DV_DVP25P2_H
