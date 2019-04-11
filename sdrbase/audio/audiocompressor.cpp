///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "audiocompressor.h"

const uint16_t AudioCompressor::ALAW_MAX = 0xFFF;
const uint16_t AudioCompressor::MULAW_MAX = 0x1FFF;
const uint16_t AudioCompressor::MULAW_BIAS = 33;


AudioCompressor::AudioCompressor()
{
    fillLUT2();
}

AudioCompressor::~AudioCompressor()
{}

void AudioCompressor::fillLUT()
{
    for (int i=0; i<8192; i++) {
        m_lut[i] = (24576/8192)*i;
    }

    for (int i=8192; i<2*8192; i++) {
        m_lut[i] = 24576 + 0.5f*(i-8192);
    }

    for (int i=2*8192; i<3*8192; i++) {
        m_lut[i] = 24576 + 4096 + 0.25f*(i-2*8192);
    }

    for (int i=3*8192; i<4*8192; i++) {
        m_lut[i] = 24576 + 4096 + 2048 + 0.125f*(i-3*8192);
    }
}

void AudioCompressor::fillLUT2()
{
    for (int i=0; i<4096; i++) {
        m_lut[i] = (24576/4096)*i;
    }

    for (int i=4096; i<2*4096; i++) {
        m_lut[i] = 24576 + 0.5f*(i-4096);
    }

    for (int i=2*4096; i<3*4096; i++) {
        m_lut[i] = 24576 + 2048 + 0.25f*(i-2*4096);
    }

    for (int i=3*4096; i<4*4096; i++) {
        m_lut[i] = 24576 + 2048 + 1024 + 0.125f*(i-3*4096);
    }

    for (int i=4*4096; i<5*4096; i++) {
        m_lut[i] = 24576 + 2048 + 1024 + 512 + 0.0625f*(i-4*4096);
    }

    for (int i=5*4096; i<6*4096; i++) {
        m_lut[i] = 24576 + 2048 + 1024 + 512 + 256 + 0.03125f*(i-5*4096);
    }

    for (int i=6*4096; i<7*4096; i++) {
        m_lut[i] = 24576 + 2048 + 1024 + 512 + 256 + 128 + 0.015625f*(i-6*4096);
    }

    for (int i=7*4096; i<8*4096; i++) {
        m_lut[i] = 24576 + 2048 + 1024 + 512 + 256 + 128 + 64 + 0.0078125f*(i-7*4096);
    }
}

void AudioCompressor::fillALaw()
{
    for (int i=-16384; i<16384; i++) {
        m_lut[i+16384] = ALaw_Encode(2*i);
    }
}

void AudioCompressor::fillULaw()
{
    for (int i=-16384; i<16384; i++) {
        m_lut[i+16384] = MuLaw_Encode(2*i);
    }
}

int16_t AudioCompressor::compress(int16_t sample)
{
    int16_t sign = sample < 0 ? -1 : 1;
    int16_t abs = sample < 0 ? -sample : sample;
    return sign * m_lut[abs];
}

int8_t AudioCompressor::compress8(int16_t sample)
{
    return m_lut[sample/2 + 16384];
}

/* http://dystopiancode.blogspot.com/2012/02/pcm-law-and-u-law-companding-algorithms.html
 *
 * First, the number is verified is its negative. If the number is negative he will be made
 * positive and the sign  variable (by default 0) will contain the value 0x80
 * (so when it's OR-ed to the coded result it will determine it's sign).
 *
 * Since the A-Law algorithm considers numbers in the range 0x000 - 0xFFF
 * (without considering the sign bit), if a number is bigger than 0xFFF, it will automatically
 * made equal to 0xFFF in order to avoid further problems.
 *
 * The first step in determining the coded value is finding the position of the first bit
 * who has a 1 value (excluding the sign bit). The search is started from position 11
 * and is continued until a bit with the value 1 is find or until a position smaller than 5 is met.
 *
 * If the position is smaller than 5 (there was no 1 bit found on the positions 11-5),
 * the least significant byte of the coded number is made equal the the bits 5,4,3,2
 * of the original number. Otherwise the least significant bit of the coded number is equal
 * to the first four bits who come after the first 1 bit encountered.
 *
 * In the end, the most significant byte of the coded number is computed according to the position
 * of the first 1 bit (if not such this was found, then the position is considered).
 *
 * Also, before returning the result, the even bits of the result will be complemented
 * (by XOR-ing with 0x55).
 */
int8_t AudioCompressor::ALaw_Encode(int16_t number)
{
    uint16_t mask = 0x800;
    uint8_t sign = 0;
    uint8_t position = 11;
    uint8_t lsb = 0;

    if (number < 0)
    {
       number = -number;
       sign = 0x80;
    }

    if (number > ALAW_MAX) {
       number = ALAW_MAX;
    }

    for (; ((number & mask) != mask && position >= 5); mask >>= 1, position--) {
    }

    lsb = (number >> ((position == 4) ? (1) : (position - 4))) & 0x0f;

    return (sign | ((position - 4) << 4) | lsb) ^ 0x55;
}

/* http://dystopiancode.blogspot.com/2012/02/pcm-law-and-u-law-companding-algorithms.html
 *
 * The µ-Law compression algorithm is very similar to the A-Law compression algorithm.
 * The main difference is that the µ-Law uses 13 bits instead of 12 bits, so the position
 * variable will be initialized with 12 instead of 11. In order to make sure that there will be
 * no number without a 1 bit in the first 12-5 positions, a bias value is added to the number
 * (in this case 33). So, since there is no special case (numbers who do not have a 1 bit in
 * the first 12-5 positions), this makes the algorithm less complex by eliminating some condtions.
 *
 * Also in the end all bits are complemented, not just the even ones.
 */
int8_t AudioCompressor::MuLaw_Encode(int16_t number)
{
   uint16_t mask = 0x1000;
   uint8_t sign = 0;
   uint8_t position = 12;
   uint8_t lsb = 0;

   if (number < 0)
   {
      number = -number;
      sign = 0x80;
   }

   number += MULAW_BIAS;

   if (number > MULAW_MAX) {
      number = MULAW_MAX;
   }

   for (; ((number & mask) != mask && position >= 5); mask >>= 1, position--) {
   }

   lsb = (number >> (position - 4)) & 0x0f;

   return (~(sign | ((position - 5) << 4) | lsb));
}
