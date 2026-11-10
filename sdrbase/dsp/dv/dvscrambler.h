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

#ifndef INCLUDE_DSP_DV_DVSCRAMBLER_H
#define INCLUDE_DSP_DV_DVSCRAMBLER_H

#include <cstdint>

#include "export.h"

/**
 * Pseudo random sequences used for scrambling (whitening) by the digital voice
 * protocols. Scrambling and descrambling are the same XOR.
 */
class SDRBASE_API DVScrambler
{
public:
    /**
     * x^9 + x^5 + 1 sequence (NXDN and YSF): 9 bit register, output its LSB, feedback bit 0 xor bit 4
     * into the MSB. The table is 512 long and indexed modulo 512 (one bit longer than the LFSR
     * period), which is what both protocols' decoders do.
     */
    class SDRBASE_API PN95
    {
    public:
        explicit PN95(uint32_t seed);
        uint8_t getBit(unsigned int index) const { return m_bits[index % 512]; }
        uint8_t getByte(unsigned int index) const { return m_bytes[index % 64]; }
        void apply(uint8_t *bits, int nbBits, unsigned int firstIndex = 0) const;

        static const PN95& nxdn(); //!< seed 0xE4 (NXDN TS 1-A 4.6)
        static const PN95& ysf();  //!< seed 0x1C9 (YSF whitening)

    private:
        uint8_t m_bits[512];
        uint8_t m_bytes[64];
    };

    /**
     * Angel61 payload scrambler (ANGEL61.md 2.7): x^15 + x^14 + 1 Fibonacci LFSR, state seeded
     * from the frame counter as (counter << 9) | 0x1FF, output bit 14 of the state, then
     * s = ((s << 1) | ((s >> 14 ^ s >> 13) & 1)) & 0x7FFF.
     */
    class SDRBASE_API PN15
    {
    public:
        explicit PN15(int counter) : m_state((((uint32_t) counter) << 9 | 0x1FF) & 0x7FFF) {}
        uint8_t next()
        {
            uint8_t bit = (m_state >> 14) & 1;
            m_state = ((m_state << 1) | (((m_state >> 14) ^ (m_state >> 13)) & 1)) & 0x7FFF;
            return bit;
        }
        //! XOR nbBits bits with the sequence from the seed
        static void apply(uint8_t *bits, int nbBits, int counter);
        //! Descramble soft decisions: negate the LLRs where the sequence is 1
        static void apply(float *llr, int nbBits, int counter);

    private:
        uint32_t m_state;
    };

    //! D-STAR radio header scrambler: x^7 + x^4 + 1 from all ones, applied to the 660 header bits
    static void dstarHeader(const uint8_t *in, uint8_t *out, int nbBits = 660);
    //! dPMR scrambler: x^9 + x^5 + 1 from all ones (TS 102 490 clause 7.4), sequence bit i
    static void dpmrSequence(uint8_t *sequence, int nbBits);
    static void dpmr(uint8_t *bits, int nbBits);
    //! DMR data burst scrambling is not defined; DMR uses the CRC masks in DVDMR instead.

private:
    static const uint8_t *dstarTable();
};

#endif // INCLUDE_DSP_DV_DVSCRAMBLER_H
