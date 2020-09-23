///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_LFSR_H
#define INCLUDE_LFSR_H

#include <stdint.h>

#include "export.h"

// Linear feedback shift register that can be used for scrambling or generating
// PN (Pseudo Noise) random sequence.
class SDRBASE_API LFSR
{
public:
    // Create and initialise LFSR with specified number of bits, polynomial and
    // initial state (which must be non-zero, unless a multiplicative scrambler).
    // The +1 is implicit in the polynomial so x^1 + 1 should be passed as 0x01
    LFSR(uint32_t polynomial, uint32_t init_value = ~0U, int rand_bit = -1) :
        m_rand_bit(rand_bit),
        m_polynomial(polynomial),
        m_init_value(init_value)
    {
        init();
    }

    // Initialise LFSR state
    void init()
    {
        m_sr = m_init_value;
    }

    // Shift the LFSR one bit and return output of XOR
    int shift();

    // Multiplicative scramble a single bit using LFSR
    int scramble(int bit_in);

    // Multiplicative scramble of data using LFSR - LSB first
    void scramble(uint8_t *data, int length);
    // Descramble data using LFSR - LSB first
    void descramble(uint8_t *data, int length);

    // XOR data with rand_bit from LFSR generating pseudo noise (PN) sequence - LSB first
    void randomize(uint8_t *data, int length);

    // Get current shift-register value
    uint32_t getSR()
    {
        return m_sr;
    }

    // Set the polynomial
    void setPolynomial(uint32_t polynomial)
    {
        m_polynomial = polynomial;
    }

    // Get the polynomial
    uint32_t getPolynomial()
    {
        return m_polynomial;
    }

private:
    int m_rand_bit;                     // Which bit from the SR to use in randomize()
    uint32_t m_polynomial;              // Polynomial coefficients (+1 is implicit)
    uint32_t m_init_value;              // Value to initialise SR to when init() is called
    uint32_t m_sr;                      // Shift register
};

// http://www.jrmiller.demon.co.uk/products/figs/man9k6.pdf
// In Matlab: comm.Scrambler(2, '1 + z^-12 + z^-17', 0)
// Call scramble()
class SDRBASE_API ScramblerG3RUG : public LFSR
{
public:
    ScramblerG3RUG() : LFSR(0x10800, 0x0) {}
};

// https://public.ccsds.org/Pubs/131x0b3e1.pdf
// x^8+x^7+x^5+x^3+1
// In Matlab: comm.PNSequence('Polynomial', 'x^8+x^7+x^5+x^3+1', 'InitialConditions', [1 1 1 1 1 1 1 1])
// Call randomize()
class SDRBASE_API RandomizeCCSDS : public LFSR
{
public:
    RandomizeCCSDS() : LFSR(0x95, 0xff, 7) {}
};

#endif
