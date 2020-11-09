///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// Inspired by: https://github.com/myriadrf/LoRa-SDR                             //
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

#ifndef PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODERLORA_H_
#define PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODERLORA_H_

#include <vector>
#include <QByteArray>

class ChirpChatModEncoderLoRa
{
public:
    static void addChecksum(QByteArray& bytes);
    static void encodeBytes(
        const QByteArray& bytes,
        std::vector<unsigned short>& symbols,
        unsigned int nbSymbolBits,
        bool hasHeader,
        bool hasCRC,
        unsigned int nbParityBits
    );

private:
    static void encodeFec(
        std::vector<uint8_t> &codewords,
        unsigned int nbParityBits,
        unsigned int& cOfs,
        unsigned int& dOfs,
        const uint8_t *bytes,
        const unsigned int codewordCount
    );

    static const unsigned int headerParityBits = 4;
    static const unsigned int headerSymbols = 8;
    static const unsigned int headerCodewords = 5;

    /***********************************************************************
     * Round functions
     **********************************************************************/
    static inline unsigned roundUp(unsigned num, unsigned factor)
    {
        return ((num + factor - 1) / factor) * factor;
    }

    /***********************************************************************
     * Encode a 4 bit word into a 8 bits with parity
     * Non standard version used in sx1272.
     * https://en.wikipedia.org/wiki/Hamming_code
     **********************************************************************/
    static inline unsigned char encodeHamming84sx(const unsigned char x)
    {
        auto d0 = (x >> 0) & 0x1;
        auto d1 = (x >> 1) & 0x1;
        auto d2 = (x >> 2) & 0x1;
        auto d3 = (x >> 3) & 0x1;

        unsigned char b = x & 0xf;
        b |= (d0 ^ d1 ^ d2) << 4;
        b |= (d1 ^ d2 ^ d3) << 5;
        b |= (d0 ^ d1 ^ d3) << 6;
        b |= (d0 ^ d2 ^ d3) << 7;
        return b;
    }

    /***********************************************************************
     * Encode a 4 bit word into a 7 bits with parity.
     * Non standard version used in sx1272.
     **********************************************************************/
    static inline unsigned char encodeHamming74sx(const unsigned char x)
    {
        auto d0 = (x >> 0) & 0x1;
        auto d1 = (x >> 1) & 0x1;
        auto d2 = (x >> 2) & 0x1;
        auto d3 = (x >> 3) & 0x1;

        unsigned char b = x & 0xf;
        b |= (d0 ^ d1 ^ d2) << 4;
        b |= (d1 ^ d2 ^ d3) << 5;
        b |= (d0 ^ d1 ^ d3) << 6;
        return b;
    }

    /***********************************************************************
     * Encode a 4 bit word into a 6 bits with parity.
     **********************************************************************/
    static inline unsigned char encodeParity64(const unsigned char b)
    {
        auto x = b ^ (b >> 1) ^ (b >> 2);
        auto y = x ^ b ^ (b >> 3);
        return ((x & 1) << 4) | ((y & 1) << 5) | (b & 0xf);
    }

    /***********************************************************************
     * Encode a 4 bit word into a 5 bits with parity.
     **********************************************************************/
    static inline unsigned char encodeParity54(const unsigned char b)
    {
        auto x = b ^ (b >> 2);
        x = x ^ (x >> 1);
        return (b & 0xf) | ((x << 4) & 0x10);
    }

    /***********************************************************************
     *  CRC reverse engineered from Sx1272 data stream.
     *  Modified CCITT crc with masking of the output with an 8bit lfsr
     **********************************************************************/
    static inline uint16_t crc16sx(uint16_t crc, const uint16_t poly)
    {
        for (int i = 0; i < 8; i++)
        {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ poly;
            } else {
                crc <<= 1;
            }
        }

        return crc;
    }

    static inline uint8_t xsum8(uint8_t t)
    {
        t ^= t >> 4;
        t ^= t >> 2;
        t ^= t >> 1;

        return (t & 1);
    }

    static inline uint16_t sx1272DataChecksum(const uint8_t *data, int length)
    {
        uint16_t res = 0;
        uint8_t v = 0xff;
        uint16_t crc = 0;

        for (int i = 0; i < length; i++)
        {
            crc = crc16sx(res, 0x1021);
            v = xsum8(v & 0xB8) | (v << 1);
            res = crc ^ data[i];
        }

        res ^= v;
        v = xsum8(v & 0xB8) | (v << 1);
        res ^= v << 8;

        return res;
    }

    /***********************************************************************
     * Specific checksum for header
     **********************************************************************/
    static inline uint8_t headerChecksum(const uint8_t *h)
    {
        auto a0 = (h[0] >> 4) & 0x1;
        auto a1 = (h[0] >> 5) & 0x1;
        auto a2 = (h[0] >> 6) & 0x1;
        auto a3 = (h[0] >> 7) & 0x1;

        auto b0 = (h[0] >> 0) & 0x1;
        auto b1 = (h[0] >> 1) & 0x1;
        auto b2 = (h[0] >> 2) & 0x1;
        auto b3 = (h[0] >> 3) & 0x1;

        auto c0 = (h[1] >> 0) & 0x1;
        auto c1 = (h[1] >> 1) & 0x1;
        auto c2 = (h[1] >> 2) & 0x1;
        auto c3 = (h[1] >> 3) & 0x1;

        uint8_t res;
        res = (a0 ^ a1 ^ a2 ^ a3) << 4;
        res |= (a3 ^ b1 ^ b2 ^ b3 ^ c0) << 3;
        res |= (a2 ^ b0 ^ b3 ^ c1 ^ c3) << 2;
        res |= (a1 ^ b0 ^ b2 ^ c0 ^ c1 ^ c2) << 1;
        res |= a0 ^ b1 ^ c0 ^ c1 ^ c2 ^ c3;

        return res;
    }

    /***********************************************************************
     *  Whitening generator reverse engineered from Sx1272 data stream.
     *  Each bit of a codeword is combined with the output from a different position in the whitening sequence.
     **********************************************************************/
    static inline void Sx1272ComputeWhitening(uint8_t *buffer, uint16_t bufferSize, const int bitOfs, const int nbParityBits)
    {
        static const int ofs0[8] = {6,4,2,0,-112,-114,-302,-34 };	// offset into sequence for each bit
        static const int ofs1[5] = {6,4,2,0,-360 };					// different offsets used for single parity mode (1 == nbParityBits)
        static const int whiten_len = 510;							// length of whitening sequence
        static const uint64_t whiten_seq[8] = {						// whitening sequence
            0x0102291EA751AAFFL,0xD24B050A8D643A17L,0x5B279B671120B8F4L,0x032B37B9F6FB55A2L,
            0x994E0F87E95E2D16L,0x7CBCFC7631984C26L,0x281C8E4F0DAEF7F9L,0x1741886EB7733B15L
        };
        const int *ofs = (1 == nbParityBits) ? ofs1 : ofs0;
        int i, j;

        for (j = 0; j < bufferSize; j++)
        {
            uint8_t x = 0;

            for (i = 0; i < 4 + nbParityBits; i++)
            {
                int t = (ofs[i] + j + bitOfs + whiten_len) % whiten_len;

                if (whiten_seq[t >> 6] & ((uint64_t)1 << (t & 0x3F))) {
                    x |= 1 << i;
                }
            }

            buffer[j] ^= x;
        }
    }

    /***********************************************************************
     * Diagonal interleaver + deinterleaver
     **********************************************************************/
    static inline void diagonalInterleaveSx(
        const uint8_t *codewords,
        const size_t numCodewords,
        uint16_t *symbols,
        const size_t nbSymbolBits,
        const size_t nbParityBits
    )
    {
        for (size_t x = 0; x < numCodewords / nbSymbolBits; x++)
        {
            const size_t cwOff = x*nbSymbolBits;
            const size_t symOff = x*(4 + nbParityBits);

            for (size_t k = 0; k < 4 + nbParityBits; k++)
            {
                for (size_t m = 0; m < nbSymbolBits; m++)
                {
                    const size_t i = (m + k + nbSymbolBits) % nbSymbolBits;
                    const auto bit = (codewords[cwOff + i] >> k) & 0x1;
                    symbols[symOff + k] |= (bit << m);
                }
            }
        }
    }

    /***********************************************************************
     *  https://en.wikipedia.org/wiki/Gray_code
     **********************************************************************/

    /*
    * A more efficient version, for Gray codes of 16 or fewer bits.
    */
    static inline unsigned short grayToBinary16(unsigned short num)
    {
        num = num ^ (num >> 8);
        num = num ^ (num >> 4);
        num = num ^ (num >> 2);
        num = num ^ (num >> 1);
        return num;
    }
};

#endif // PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODERLORA_H_
