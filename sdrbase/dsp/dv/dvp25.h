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

#ifndef INCLUDE_DSP_DV_DVP25_H
#define INCLUDE_DSP_DV_DVP25_H

#include <cstdint>

#include "export.h"

/**
 * APCO P25 Phase 1 (TIA-102.BAAA-A) error control: network identifier, link
 * control, encryption sync, header and low speed data coding.
 *
 * All codes are systematic with the data bits first, MSB first. Bit arrays hold
 * one bit per byte, hexbit arrays one 6 bit symbol per byte.
 */
class SDRBASE_API DVP25
{
public:
    enum DUID
    {
        DUIDHDU = 0x0,
        DUIDTDU = 0x3,
        DUIDLDU1 = 0x5,
        DUIDTSDU = 0x7,
        DUIDLDU2 = 0xA,
        DUIDPDU = 0xC,
        DUIDTDULC = 0xF
    };

    static const int m_syncBits = 48;
    static const int m_nidBits = 64;
    static const int m_imbeBits = 144;
    static const int m_lcBits = 240;      //!< Encoded LC or ESS in an LDU
    static const int m_lsdBits = 32;      //!< Encoded low speed data in an LDU
    static const int m_headerBits = 648;  //!< Encoded HDU payload
    static const uint64_t m_frameSync = 0x5575F5FF77FFULL;

    // ---- Network identifier: BCH(63,16) + parity bit ---------------------------------------
    //! NAC (12 bits) and DUID (4 bits) -> 64 bit NID, MSB first on air. The last bit is
    //! DUID bit 1 xor DUID bit 0 as the (64,16) generator matrix of the standard gives it.
    static uint64_t nidEncode(int nac, int duid);
    //! 64 NID bits -> NAC and DUID with up to 11 corrected bits. Returns corrected bits or -1.
    static int nidDecode(const uint8_t *bits, int& nac, int& duid);

    // ---- Link control and encryption sync in LDUs --------------------------------------------
    //! 9 LC bytes -> 240 bits: 12 hexbits, RS(24,12,13), 24 Hamming(10,6,3) codewords
    static void lcEncode(const uint8_t *lc9, uint8_t *bits240);
    //! 240 bits -> 9 LC bytes. Returns corrected hexbits or -1.
    static int lcDecode(const uint8_t *bits240, uint8_t *lc9);
    //! MI(72) ALGID(8) KID(16) -> 240 bits: 16 hexbits, RS(24,16,9), Hamming(10,6,3)
    static void essEncode(const uint8_t *mi9, uint8_t algid, uint16_t kid, uint8_t *bits240);
    //! 240 bits -> MI, ALGID, KID. Returns corrected hexbits or -1.
    static int essDecode(const uint8_t *bits240, uint8_t *mi9, uint8_t& algid, uint16_t& kid);

    // ---- Header data unit -------------------------------------------------------------------
    //! MI(72) MFID(8) ALGID(8) KID(16) TGID(16) -> 648 bits: 20 hexbits, RS(36,20,17), Golay(18,6,8)
    static void headerEncode(const uint8_t *mi9, uint8_t mfid, uint8_t algid, uint16_t kid, uint16_t tgid, uint8_t *bits648);
    //! 648 bits -> header fields. Returns corrected hexbits or -1.
    static int headerDecode(const uint8_t *bits648, uint8_t *mi9, uint8_t& mfid, uint8_t& algid, uint16_t& kid, uint16_t& tgid);

    // ---- Low speed data ------------------------------------------------------------------
    //! 16 bits of low speed data -> 32 bits: two cyclic(16,8,5) codewords
    static void lsdEncode(uint16_t lsd, uint8_t *bits32);
    //! 32 bits -> 16 bits. Returns corrected bits or -1.
    static int lsdDecode(const uint8_t *bits32, uint16_t& lsd);

    // ---- Terminator with link control ----------------------------------------------------------
    //! LC -> 288 bits of TDULC payload: RS(24,12,13) then 12 Golay(24,12,8) codewords of hexbit pairs
    static void tdulcEncode(const uint8_t *lc9, uint8_t *bits288);
    //! 288 bits -> 9 LC bytes. Returns corrected hexbits or -1.
    static int tdulcDecode(const uint8_t *bits288, uint8_t *lc9);

    // ---- Trunking and data blocks: rate 1/2 trellis, 196 bits per 96 bit block -----------------
    static const int m_blockBits = 196;
    static const int m_blockBytes = 12;   //!< 10 payload bytes and a CRC-16
    //! 12 byte block (the CRC is computed here over the first 10) -> 196 interleaved trellis bits
    static void blockEncode(const uint8_t *bytes12, uint8_t *bits196);
    //! 12 bytes as they are (an unconfirmed packet data block: no CRC) -> 196 bits
    static void blockEncodeRaw(const uint8_t *bytes12, uint8_t *bits196);
    //! 196 LLRs (positive favours 0) -> 12 byte block by soft Viterbi decoding. Returns the
    //! number of hard decisions the decoded codeword differs from, or -1 when the CRC fails
    //! (an unconfirmed packet data block has no CRC: checkCrc false takes all 12 bytes as data).
    static int blockDecode(const float *llr196, uint8_t *bytes12, bool checkCrc = true);
    //! CRC-16 of a block: CCITT polynomial, augmented form, result inverted (TIA-102.BAAA 6.3)
    static uint16_t blockCrc(const uint8_t *bytes, int nbBytes);

    // ---- Confirmed packet data blocks: rate 3/4 trellis, 196 bits per 144 bit block ------------
    static const int m_block34Bytes = 18; //!< serial number (7 bits), CRC-9 and 16 data bytes
    //! 18 byte block (the CRC-9 over the serial number and the data is computed here) -> 196 bits
    static void block34Encode(const uint8_t *bytes18, uint8_t *bits196);
    //! 196 LLRs -> 18 byte block. Returns the hard decision disagreements, or -1 when the CRC-9 fails
    static int block34Decode(const float *llr196, uint8_t *bytes18);
    //! Packet CRC-32 (TIA-102.BAAA 7.4): the register over the bits MSB first from zero, inverted
    static uint32_t packetCrc(const uint8_t *bytes, int nbBytes);
    //! Trunking signalling block fields (TIA-102.AABC): opcode name, or a hexadecimal fallback
    static const char *tsbkOpcodeName(int opcode);

    // ---- Status symbols ----------------------------------------------------------------------
    //! Pad to a multiple of 70 bits with nulls and insert a status dibit after every 35 data dibits.
    //! Returns the number of output bits.
    static int insertStatusSymbols(const uint8_t *data, int nbDataBits, uint8_t status, uint8_t *out);
    //! Remove the status dibits from nbBits on-air bits. Returns the number of data bits.
    static int removeStatusSymbols(const uint8_t *in, int nbBits, uint8_t *data);

private:
    static void hammingEncodeHexbits(const uint8_t *hexbits, int nbHexbits, uint8_t *bits);
    static int hammingDecodeHexbits(const uint8_t *bits, int nbHexbits, uint8_t *hexbits);
};

#endif // INCLUDE_DSP_DV_DVP25_H
