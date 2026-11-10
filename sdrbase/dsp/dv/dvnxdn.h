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

#ifndef INCLUDE_DSP_DV_DVNXDN_H
#define INCLUDE_DSP_DV_DVNXDN_H

#include <cstdint>

#include "export.h"

/**
 * NXDN (NXDN TS 1-A common air interface) channel coding: LICH, SACCH and
 * FACCH1 encoders and decoders, scrambling.
 *
 * A frame is 384 bits (192 symbols, 80 ms): 20 FSW, 16 LICH, 60 SACCH,
 * 288 bits of 4 VCH or 2 FACCH1. All but the FSW are scrambled. Bit arrays hold
 * one bit per byte, MSB first, in transmission order.
 */
class SDRBASE_API DVNXDN
{
public:
    static const int m_frameBits = 384;
    static const int m_fswBits = 20;
    static const int m_lichBits = 16;
    static const int m_sacchInfoBits = 26;    //!< structure (2) + RAN (6) + 18 message bits
    static const int m_sacchCodedBits = 36;   //!< info + CRC6 + 4 tail bits
    static const int m_sacchTxBits = 60;
    static const int m_facch1InfoBits = 80;
    static const int m_facch1CodedBits = 96;  //!< info + CRC12 + 4 tail bits
    static const int m_facch1TxBits = 144;
    static const int m_udchInfoBits = 184;    //!< structure (2) + RAN (6) + 176 message bits
    static const int m_udchCodedBits = 203;   //!< info + CRC15 + 4 tail bits
    static const int m_udchTxBits = 348;      //!< the SACCH and payload area of a frame
    static const int m_cacInfoBits = 155;     //!< SR (8) + 144 message bits + 3 null
    static const int m_cacCodedBits = 175;    //!< info + CRC16 + 4 tail bits
    static const int m_cacTxBits = 300;       //!< the outbound RCCH frame after the LICH
    static const int m_vchBits = 72;
    static const int m_scrambledSymbols = 182; //!< LICH to end of frame

    // ---- Convolutional code -----------------------------------------------------------------
    //! Rate 1/2 K=5: G1 = 1 + D^3 + D^4, G2 = 1 + D + D^2 + D^4. Output 2*nbBits, G1 then G2.
    static void convEncode(const uint8_t *in, int nbBits, uint8_t *out);

    //! Remove the coded bits listed in punctureList then interleave: tx[k] = punctured[interleave[k]]
    static void punctureInterleave(const uint8_t *coded, int nbCoded, const int *punctureList, int nbPuncture,
        const int *interleave, int nbTx, uint8_t *tx);
    //! Inverse of punctureInterleave on LLRs: punctured positions become 0 (erasures)
    static void deinterleaveDepuncture(const float *rx, int nbTx, const int *interleave, const int *punctureList, int nbPuncture,
        int nbCoded, float *coded);

    // ---- LICH -------------------------------------------------------------------------------
    //! RF channel type (2), functional channel type (2), option (2), direction (1) and even parity
    //! over bits 0..5 and the parity bit -> 16 bits (each information bit is carried in the MSB of a symbol)
    static void lichEncode(int rfChannelType, int functionalType, int option, int direction, uint8_t *bits);
    //! 16 bits -> fields. Returns true when the parity matches.
    static bool lichDecode(const uint8_t *bits, int& rfChannelType, int& functionalType, int& option, int& direction);

    // ---- SACCH and FACCH1 ----------------------------------------------------------------------
    //! SACCH: 26 information bits -> 60 on-air bits (CRC6, tail, convolution, puncture, interleave)
    static void sacchEncode(const uint8_t *info, uint8_t *tx);
    //! 60 on-air bits -> 26 information bits. Returns true when the CRC matches.
    static bool sacchDecode(const uint8_t *rx, uint8_t *info);
    static bool sacchDecodeSoft(const float *llr, uint8_t *info);
    //! FACCH1: 80 information bits -> 144 on-air bits (CRC12, tail, convolution, puncture, interleave)
    static void facch1Encode(const uint8_t *info, uint8_t *tx);
    //! 144 on-air bits -> 80 information bits. Returns true when the CRC matches.
    static bool facch1Decode(const uint8_t *rx, uint8_t *info);
    static bool facch1DecodeSoft(const float *llr, uint8_t *info);

    //! UDCH (user data channel, LICH functional type 1): 184 information bits -> 348 on-air bits
    //! (CRC15, tail, convolution, puncture, interleave), the SACCH and payload area of a frame
    static void udchEncode(const uint8_t *info, uint8_t *tx);
    //! 348 on-air bits -> 184 information bits. Returns true when the CRC matches.
    static bool udchDecode(const uint8_t *rx, uint8_t *info);
    static bool udchDecodeSoft(const float *llr, uint8_t *info);
    //! CAC (the outbound trunked control channel, RCCH: LICH RF channel type 0, outbound): 155
    //! information bits (SR, the layer 3 data, 3 null) -> 300 on-air bits (TS 1-A 4.5.1.1: CRC-16,
    //! K=5 code punctured as the UDCH's, a 12 x 25 interleave)
    static void cacEncode(const uint8_t *info, uint8_t *tx);
    static bool cacDecodeSoft(const float *llr, uint8_t *info);

    // ---- Scrambling ----------------------------------------------------------------------------
    //! Scramble or descramble the 182 symbols after the FSW: the MSB of symbol i is inverted when
    //! PN(9,5) bit i (seed 0xE4) is set. bits points at the first LICH bit.
    static void scramble(uint8_t *bits);
    //! Same on LLRs (two per symbol)
    static void scrambleSoft(float *llr);

    static const int m_sacchInterleave[60];
    static const int m_sacchPunctureList[12];
    static const int m_facch1Interleave[144];
    static const int m_facch1PunctureList[48];
    static const int m_udchInterleave[348];
    static const int m_udchPunctureList[58];

private:
    static bool sacchFromCoded(const float *coded, uint8_t *info);
    static bool facch1FromCoded(const float *coded, uint8_t *info);
    static bool udchFromCoded(const float *coded, uint8_t *info);
};

#endif // INCLUDE_DSP_DV_DVNXDN_H
