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

#ifndef INCLUDE_DSP_DV_DVCRC_H
#define INCLUDE_DSP_DV_DVCRC_H

#include <cstdint>

#include "export.h"

/**
 * Cyclic redundancy checks of the digital voice protocols, over bit arrays
 * (one bit per byte, first transmitted first) or bytes.
 */
class SDRBASE_API DVCRC
{
public:
    //! Generic MSB first CRC over a bit array: register of `order` bits, polynomial without its
    //! x^order term, initial value and final xor.
    static uint32_t compute(const uint8_t *bits, int nbBits, int order, uint32_t polynomial, uint32_t init, uint32_t finalXor);
    //! Append the `order` CRC bits (MSB first) after bits[0..nbBits-1]
    static void append(uint8_t *bits, int nbBits, int order, uint32_t polynomial, uint32_t init, uint32_t finalXor);
    //! Check bits[0..nbBits+order-1]: true when the CRC field matches
    static bool check(const uint8_t *bits, int nbBits, int order, uint32_t polynomial, uint32_t init, uint32_t finalXor);

    //! D-STAR radio header: CRC-CCITT reflected (0x8408, init and final xor 0xffff) over the first 39 header bytes
    static uint16_t dstarHeader(const uint8_t *header);
    //! The same D-STAR CRC (reflected 0x8408, init and final XOR 0xffff, LSB of each byte first) over any bytes (D-PRS)
    static uint16_t dstar16(const uint8_t *bytes, int nbBytes);
    //! CRC-16 CCITT over bytes, MSB first, polynomial 0x1021
    static uint16_t ccitt16(const uint8_t *bytes, int nbBytes, uint16_t init, uint16_t finalXor);
    //! YSF FICH and DCH: ccitt16 with init 0 and final xor 0xffff
    static uint16_t ysf16(const uint8_t *bytes, int nbBytes) { return ccitt16(bytes, nbBytes, 0, 0xffff); }
    //! NXDN CRC6: x^6 + x^5 + x^2 + x + 1, initial value all ones
    static uint8_t nxdn6(const uint8_t *bits, int nbBits) { return (uint8_t) compute(bits, nbBits, 6, 0x27, 0x3f, 0); }
    //! NXDN CRC12: x^12 + x^11 + x^3 + x^2 + x + 1, initial value all ones
    static uint16_t nxdn12(const uint8_t *bits, int nbBits) { return (uint16_t) compute(bits, nbBits, 12, 0x80f, 0xfff, 0); }
    //! NXDN CRC15: x^15 + x^14 + x^13 + x^11 + x^7 + x^5 + x^3 + x^2 + x + 1, initial value all ones (from the
    //! specification; not yet exercised by a test vector, as nxdn16, dmrCcitt and dmr9 below)
    static uint16_t nxdn15(const uint8_t *bits, int nbBits) { return (uint16_t) compute(bits, nbBits, 15, 0x4cc5, 0x7fff, 0); }
    //! NXDN data call message CRC-32 (TS 1-A: the register all ones, over the user data bits)
    static uint32_t nxdn32(const uint8_t *bits, int nbBits) { return compute(bits, nbBits, 32, 0x04c11db7, 0xffffffff, 0); }
    //! NXDN CRC16: x^16 + x^12 + x^5 + 1, initial value all ones
    static uint16_t nxdn16(const uint8_t *bits, int nbBits) { return (uint16_t) compute(bits, nbBits, 16, 0x1021, 0xffff, 0); }
    //! dPMR CRC7: x^7 + x^3 + 1, no initial value
    static uint8_t dpmr7(const uint8_t *bits, int nbBits) { return (uint8_t) compute(bits, nbBits, 7, 0x09, 0, 0); }
    //! dPMR CRC8: x^8 + x^2 + x + 1, no initial value
    static uint8_t dpmr8(const uint8_t *bits, int nbBits) { return (uint8_t) compute(bits, nbBits, 8, 0x07, 0, 0); }
    //! DMR CRC-CCITT over bytes (data header, CSBK, ETSI B.3.8): ccitt16 with init 0 and inverted result, before the CRC mask
    static uint16_t dmrCcitt(const uint8_t *bytes, int nbBytes) { return ccitt16(bytes, nbBytes, 0, 0xffff); }

    //! The IEEE CRC-32 of bytes (reflected, 0xEDB88320, all ones in and out): file checksums (ANGEL61.md 3.4)
    static uint32_t crc32(const uint8_t *bytes, int nbBytes);
    //! DMR CRC9 (confirmed data blocks), including the all-ones inversion polynomial
    static uint16_t dmr9(const uint8_t *bits, int nbBits) { return (uint16_t) compute(bits, nbBits, 9, 0x059, 0, 0x1ff); }
    //! DMR 5 bit link control checksum: sum of the 9 LC bytes modulo 31
    static uint8_t dmrLcChecksum(const uint8_t *lc);
};

#endif // INCLUDE_DSP_DV_DVCRC_H
