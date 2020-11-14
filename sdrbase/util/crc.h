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

#ifndef INCLUDE_CRC_H
#define INCLUDE_CRC_H

#include <stdint.h>

#include "export.h"

// Class to calculate arbitrary CRCs (up to 32-bits)
class SDRBASE_API crc
{
public:
    // Create and initialise CRC with specified polynomial and parameters
    crc(int poly_bits, uint32_t polynomial, bool msb_first, uint32_t init_value, uint32_t final_xor) :
        m_polynomial(polynomial),
        m_poly_bits(poly_bits),
        m_msb_first(msb_first),
        m_init_value(init_value),
        m_final_xor(final_xor)
    {
        // Reverse polynomial for LSB first
        if (!msb_first)
        {
            int shift;

            shift = 32 - m_poly_bits;
            m_polynomial_rev = reverse (m_polynomial << shift, 32);
        }
        // Create LUT
        for (int i = 0; i < 256; i++)
        {
            m_crc = 0;
            calculate(i, 8);
            m_lut[i] = m_crc;
        }
        init();
    }

    // Initialise CRC state
    void init()
    {
        m_crc = m_init_value;
    }

    // Calculate CRC for supplied data
    void calculate(uint32_t data, int data_bits);
    void calculate(const uint8_t *data, int length);

    // Get final CRC
    uint32_t get()
    {
        uint32_t t;

        t = m_final_xor ^ m_crc;
        return t;
    }

private:
    static uint32_t reverse(uint32_t val, int bits);

    uint32_t m_crc;
    uint32_t m_polynomial;
    uint32_t m_polynomial_rev;
    uint32_t m_poly_bits;
    bool m_msb_first;
    uint32_t m_init_value;
    uint32_t m_final_xor;
    uint32_t m_lut[256];
};

class SDRBASE_API crc16ansi : public crc
{
public:
    crc16ansi() : crc(16, 0x8005, false, 0x0000, 0) {}
};

class SDRBASE_API crc16ccitt : public crc
{
public:
    crc16ccitt() : crc(16, 0x1021, true, 0xffff, 0) {}
};

class SDRBASE_API crc16itut : public crc
{
public:
    crc16itut() : crc(16, 0x1021, false, 0x0000, 0) {}
};

class SDRBASE_API crc16x25 : public crc
{
public:
    crc16x25() : crc(16, 0x1021, false, 0xffff, 0xffff) {}
};

class SDRBASE_API crc32 : public crc
{
public:
    crc32() : crc(32, 0x04C11DB7, false, 0xffffffff, 0xffffffff) {}
};

// Should probably try to use SSE 4.2's CRC32C instruction if available
// See _mm_crc32_u8 and _mm_crc32_u64 intrinsics
class SDRBASE_API crc32c : public crc
{
public:
    crc32c() : crc(32, 0x1EDC6F41, false, 0xffffffff, 0) {}
};

// ADS-B - https://mode-s.org/decode/adsb/introduction.html
class SDRBASE_API crcadsb : public crc
{
public:
    crcadsb() : crc(24, 0xfff409, true, 0, 0) {}
};

#endif
