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

#ifndef INCLUDE_DSP_DV_DVDMR_H
#define INCLUDE_DSP_DV_DVDMR_H

#include <cstdint>

#include "export.h"

/**
 * DMR (ETSI TS 102 361-1) layer 2 coding: burst field encoders and decoders.
 * Bit arrays hold one bit per byte, MSB first, in transmission order.
 *
 * Burst layout (264 bits): 24 CACH, 108 payload, 48 sync or signalling, 108 payload
 * where the 48 centre bits are the sync in a data burst or, in voice bursts B to F,
 * 8 EMB + 32 embedded signalling + 8 EMB. Data bursts carry 196 BPTC coded bits in
 * their two 98 bit halves with the 20 bit slot type split around the sync.
 */
class SDRBASE_API DVDMR
{
public:
    static constexpr int m_burstBits = 264;
    static constexpr int m_cachBits = 24;
    static constexpr int m_syncBits = 48;
    static constexpr int m_payloadBits = 108;
    static constexpr int m_bptcBits = 196;
    static constexpr int m_infoBits = 96;
    static constexpr int m_lcBytes = 9;
    static constexpr int m_embeddedLcBits = 128;
    static constexpr int m_vocoderFrameBits = 72;

    enum DataType
    {
        DataPIHeader = 0,
        DataVoiceLCHeader = 1,
        DataTerminatorWithLC = 2,
        DataCSBK = 3,
        DataMBCHeader = 4,
        DataMBCContinuation = 5,
        DataHeader = 6,
        DataRate12 = 7,
        DataRate34 = 8,
        DataIdle = 9,
        DataRate1 = 10,
        DataUnifiedSingleBlock = 11
    };

    //! CRC masks applied to the RS(12,9) parity of a full LC (B.3.12) and to the CCITT CRC of data PDUs (B.3.13)
    static constexpr uint8_t m_maskVoiceLCHeader = 0x96;
    static constexpr uint8_t m_maskTerminatorWithLC = 0x99;
    static constexpr uint8_t m_maskPIHeader = 0x69;
    static constexpr uint16_t m_maskCSBK = 0xA5A5;
    static constexpr uint16_t m_maskMBCHeader = 0xAAAA;
    static constexpr uint16_t m_maskDataHeader = 0xCCCC;

    // ---- Block product turbo code (B.1.1) ----------------------------------------------
    //! 96 information bits (I(95) first) -> 196 transmit bits
    static void bptcEncode(const uint8_t *info, uint8_t *tx);
    //! 196 received bits -> 96 information bits with Hamming row and column correction.
    //! Returns the number of corrected bits or -1 if a row or column stays inconsistent.
    static int bptcDecode(const uint8_t *tx, uint8_t *info);

    // ---- Full link control (B.3.6, B.3.12) --------------------------------------------
    //! 9 LC bytes + RS(12,9) parity xor mask -> 96 information bits
    static void fullLCEncode(const uint8_t *lc, uint8_t mask, uint8_t *info);
    //! 96 information bits -> 9 LC bytes with RS correction. Returns corrected symbols or -1.
    static int fullLCDecode(const uint8_t *info, uint8_t mask, uint8_t *lc);

    // ---- Embedded signalling (B.2.1, B.3.11) --------------------------------------------
    //! 9 LC bytes -> 128 embedded signalling bits (4 fragments of 32) with Hamming(16,11,4) and column parity
    static void embeddedLCEncode(const uint8_t *lc, uint8_t *tx);
    //! 128 embedded signalling bits -> 9 LC bytes. Row correction, column parity check and the
    //! 5 bit checksum are verified. Returns corrected bits or -1.
    static int embeddedLCDecode(const uint8_t *tx, uint8_t *lc);

    // ---- Burst fields ---------------------------------------------------------------------
    //! CACH: TACT (AT, TC, LCSS) with Hamming(7,4) and 17 payload bits -> 24 interleaved bits
    static void cachEncode(bool at, int tc, int lcss, const uint8_t *payload, uint8_t *tx);
    //! 24 received CACH bits -> TACT fields and 17 payload bits. Returns corrected bits or -1.
    static int cachDecode(const uint8_t *rx, bool& at, int& tc, int& lcss, uint8_t *payload);
    //! Slot type: colour code and data type with Golay(20,8) -> 20 bits
    static void slotTypeEncode(int colorCode, int dataType, uint8_t *tx);
    //! 20 slot type bits -> colour code and data type. Returns corrected bits or -1.
    static int slotTypeDecode(const uint8_t *rx, int& colorCode, int& dataType);
    //! EMB: colour code, PI, LCSS with QR(16,7,6) -> 16 bits
    static void embEncode(int colorCode, bool pi, int lcss, uint8_t *tx);
    //! 16 EMB bits -> fields. Returns corrected bits or -1.
    static int embDecode(const uint8_t *rx, int& colorCode, bool& pi, int& lcss);
    //! Soft variant from 16 LLRs: maximum likelihood codeword. Returns the number of hard
    //! decisions it differs from (a confidence: 0 to 2 is what the hard decoder corrects).
    static int embDecodeSoft(const float *llr, int& colorCode, bool& pi, int& lcss);

    //! On-air bit k of the CACH carries TACT/payload bit m_cachInterleave[k]
    static const int m_cachInterleave[24];

private:
    static void bptcFill(const uint8_t *info, uint8_t m[13][15]);
    static void bptcExtract(const uint8_t m[13][15], uint8_t *info);
};

#endif // INCLUDE_DSP_DV_DVDMR_H
