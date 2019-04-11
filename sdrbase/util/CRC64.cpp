///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include "util/CRC64.h"

/**
* poly is: x^64 + x^62 + x^57 + x^55 + x^54 + x^53 + x^52 + x^47 + x^46 + x^45 + x^40 + x^39 +
*          x^38 + x^37 + x^35 + x^33 + x^32 + x^31 + x^29 + x^27 + x^24 + x^23 + x^22 + x^21 +
*          x^19 + x^17 + x^13 + x^12 + x^10 + x^9  + x^7  + x^4  + x^1  + 1
*
* represented here with lsb = highest degree term
*
* 1100100101101100010101111001010111010111100001110000111101000010_
* ||  |  | || ||   | | ||||  | | ||| | ||||    |||    |||| |    | |
* ||  |  | || ||   | | ||||  | | ||| | ||||    |||    |||| |    | +- x^64 (implied)
* ||  |  | || ||   | | ||||  | | ||| | ||||    |||    |||| |    |
* ||  |  | || ||   | | ||||  | | ||| | ||||    |||    |||| |    +--- x^62
* ||  |  | || ||   | | ||||  | | ||| | ||||    |||    |||| +-------- x^57
* .......................................................................
* ||
* |+---------------------------------------------------------------- x^1
* +----------------------------------------------------------------- x^0 (1)
*/
const uint64_t CRC64::m_poly = 0xC96C5795D7870F42ull;

CRC64::CRC64()
{
	build_crc_table();
}

CRC64::~CRC64()
{}

/**
* input is dividend: as 0000000000000000000000000000000000000000000000000000000000000000<8-bit byte>
* where the lsb of the 8-bit byte is the coefficient of the highest degree term (x^71) of the dividend
* so division is really for input byte * x^64
*
* you may wonder how 72 bits will fit in 64-bit data type... well as the shift-right occurs, 0's are supplied
* on the left (most significant) side ... when the 8 shifts are done, the right side (where the input
* byte was placed) is discarded
*
* when done, table[XX] (where XX is a byte) is equal to the CRC of 00 00 00 00 00 00 00 00 XX
*/
void CRC64::build_crc_table()
{
    for(int i = 0; i < 256; ++i)
    {
    	uint64_t crc = i;

    	for(unsigned int j = 0; j < 8; ++j)
    	{
    		if(crc & 1) // is current coefficient set?
            {
                crc >>= 1;   // yes, then assume it gets zero'd (by implied x^64 coefficient of dividend)
    			crc ^= m_poly; // and add rest of the divisor
            }
    		else // no? then move to next coefficient
    		{
    			crc >>= 1;
            }
    	}

        m_crcTable[i] = crc;
    }
}

/**
* will give an example CRC calculation for input array {0xDE, 0xAD}
*
* each byte represents a group of 8 coefficients for 8 dividend terms
*
* the actual polynomial dividend is:
*
* = DE       AD       00 00 00 00 00 00 00 00 (hex)
* = 11011110 10101101 0000000000000000000...0 (binary)
*   || ||||  | | || |
*   || ||||  | | || +------------------------ x^71
*   || ||||  | | |+-------------------------- x^69
*   || ||||  | | +--------------------------- x^68
*   || ||||  | +----------------------------- x^66
*   || ||||  +------------------------------- x^64
*   || ||||
*   || |||+---------------------------------- x^78
*   || ||+----------------------------------- x^77
*   || |+------------------------------------ x^76
*   || +------------------------------------- x^75
*   |+--------------------------------------- x^73
*   +---------------------------------------- x^72
*
*
* the basic idea behind how the table lookup results can be used with one
* another is that:
*
* Mod(A * x^n, P(x)) = Mod(x^n * Mod(A, P(X)), P(X))
*
* in other words, an input data shifted towards the higher degree terms
* changes the pre-computed crc of the input data by shifting it also
* the same amount towards higher degree terms (mod the polynomial)
*
* here is an example:
*
* 1) input:
*
*    00 00 00 00 00 00 00 00 AD DE
*
* 2) index crc table for byte DE (really for dividend 00 00 00 00 00 00 00 00 DE)
*
*    we get A8B4AFBDC5A6ACA4
*
* 3) apply that to the input stream:
*
*    00 00 00 00 00 00 00 00 AD DE
*       A8 B4 AF BD C5 A6 AC A4
*    -----------------------------
*    00 A8 B4 AF BD C5 A6 AC 09
*
* 4) index crc table for byte 09 (really for dividend 00 00 00 00 00 00 00 00 09)
*
*    we get 448FCBB7FCB9E309
*
* 5) apply that to the input stream
*
*    00 A8 B4 AF BD C5 A6 AC 09
*    44 8F CB B7 FC B9 E3 09
*    --------------------------
*    44 27 7F 18 41 7C 45 A5
*
*/
uint64_t CRC64::calculate_crc(uint8_t *stream, int length)
{
    uint64_t crc = 0;

    for (int i = 0 ; i < length; ++i)
    {
        uint8_t index = stream[i] ^ crc;
        uint64_t lookup = m_crcTable[index];

        crc >>= 8;
        crc ^= lookup;
    }

    return crc;
}


