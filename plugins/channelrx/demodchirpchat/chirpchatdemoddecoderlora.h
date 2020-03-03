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

#ifndef INCLUDE_CHIRPCHATDEMODDECODERLORA_H
#define INCLUDE_CHIRPCHATDEMODDECODERLORA_H

#include <vector>
#include <QByteArray>

class ChirpChatDemodDecoderLoRa
{
public:
    enum ParityStatus
    {
        ParityUndefined,
        ParityError,
        ParityCorrected,
        ParityOK
    };

    static void decodeBytes(
        QByteArray& bytes,
        const std::vector<unsigned short>& inSymbols,
        unsigned int nbSymbolBits,
        bool hasHeader,
        bool& hasCRC,
        unsigned int& nbParityBits,
        unsigned int& packetLength,
        bool& earlyEOM,
        int& headerParityStatus,
        bool& headerCRCStatus,
        int& payloadParityStatus,
        bool& payloadCRCStatus
    );

    static void getCodingMetrics(
        unsigned int nbSymbolBits,
        unsigned int nbParityBits,
        unsigned int packetLength,
        bool hasHeader,
        bool hasCRC,
        unsigned int& numSymbols,
        unsigned int& numCodewords
    );

private:
    static void decodeHeader(
        const std::vector<unsigned short>& inSymbols,
        unsigned int nbSymbolBits,
        bool& hasCRC,
        unsigned int& nbParityBits,
        unsigned int& packetLength,
        int& headerParityStatus,
        bool& headerCRCStatus
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
     *  https://en.wikipedia.org/wiki/Gray_code
     **********************************************************************/

    /*
    * This function converts an unsigned binary
    * number to reflected binary Gray code.
    *
    * The operator >> is shift right. The operator ^ is exclusive or.
    */
    static inline unsigned short binaryToGray16(unsigned short num)
    {
        return num ^ (num >> 1);
    }

    /***********************************************************************
     * Diagonal deinterleaver
     **********************************************************************/
    static inline void diagonalDeinterleaveSx(
        const uint16_t *symbols,
        const unsigned int numSymbols,
        uint8_t *codewords,
        const unsigned int nbSymbolBits,
        const unsigned int nbParityBits)
    {
        for (unsigned int x = 0; x < numSymbols / (4 + nbParityBits); x++)
        {
            const unsigned int cwOff = x*nbSymbolBits;
            const unsigned int symOff = x*(4 + nbParityBits);

            for (unsigned int k = 0; k < 4 + nbParityBits; k++)
            {
                for (unsigned int m = 0; m < nbSymbolBits; m++)
                {
                    const unsigned int i = (m + k) % nbSymbolBits;
                    const auto bit = (symbols[symOff + k] >> m) & 0x1;
                    codewords[cwOff + i] |= (bit << k);
                }
            }
        }
    }

    /***********************************************************************
     *  Whitening generator reverse engineered from Sx1272 data stream.
     *  Same as above but using the actual interleaved LFSRs.
     **********************************************************************/
    static inline void Sx1272ComputeWhiteningLfsr(uint8_t *buffer, uint16_t bufferSize, const int bitOfs, const unsigned int nbParityBits)
    {
        static const uint64_t seed1[2] = {0x6572D100E85C2EFF,0xE85C2EFFFFFFFFFF};   // lfsr start values
        static const uint64_t seed2[2] = {0x05121100F8ECFEEF,0xF8ECFEEFEFEFEFEF};   // lfsr start values for single parity mode (1 == nbParityBits)
        const uint8_t m = 0xff >> (4 - nbParityBits);
        uint64_t r[2] = {(1 == nbParityBits)?seed2[0]:seed1[0],(1 == nbParityBits)?seed2[1]:seed1[1]};
        int i,j;

        for (i = 0; i < bitOfs;i++)
        {
            r[i & 1] = (r[i & 1] >> 8) | (((r[i & 1] >> 32) ^ (r[i & 1] >> 24) ^ (r[i & 1] >> 16) ^ r[i & 1]) << 56);   // poly: 0x1D
        }

        for (j = 0; j < bufferSize; j++,i++)
        {
            buffer[j] ^= r[i & 1] & m;
            r[i & 1] = (r[i & 1] >> 8) | (((r[i & 1] >> 32) ^ (r[i & 1] >> 24) ^ (r[i & 1] >> 16) ^ r[i & 1]) << 56);
        }
    }

    /***********************************************************************
     * Decode 8 bits into a 4 bit word with single bit correction.
     * Non standard version used in sx1272.
     * Set error to true when a parity error was detected
     * Set bad to true when the result could not be corrected
     **********************************************************************/
    static inline unsigned char decodeHamming84sx(const unsigned char b, bool &error, bool &bad)
    {
        auto b0 = (b >> 0) & 0x1;
        auto b1 = (b >> 1) & 0x1;
        auto b2 = (b >> 2) & 0x1;
        auto b3 = (b >> 3) & 0x1;
        auto b4 = (b >> 4) & 0x1;
        auto b5 = (b >> 5) & 0x1;
        auto b6 = (b >> 6) & 0x1;
        auto b7 = (b >> 7) & 0x1;

        auto p0 = (b0 ^ b1 ^ b2 ^ b4);
        auto p1 = (b1 ^ b2 ^ b3 ^ b5);
        auto p2 = (b0 ^ b1 ^ b3 ^ b6);
        auto p3 = (b0 ^ b2 ^ b3 ^ b7);

        auto parity = (p0 << 0) | (p1 << 1) | (p2 << 2) | (p3 << 3);
        if (parity != 0) error = true;
        switch (parity & 0xf)
        {
            case 0xD: return (b ^ 1) & 0xf;
            case 0x7: return (b ^ 2) & 0xf;
            case 0xB: return (b ^ 4) & 0xf;
            case 0xE: return (b ^ 8) & 0xf;
            case 0x0:
            case 0x1:
            case 0x2:
            case 0x4:
            case 0x8: return b & 0xf;
            default: bad = true; return b & 0xf;
        }
    }

    /***********************************************************************
     * Simple 8-bit checksum routine
     **********************************************************************/
    static inline uint8_t checksum8(const uint8_t *p, const size_t len)
    {
        uint8_t acc = 0;

        for (size_t i = 0; i < len; i++)
        {
            acc = (acc >> 1) + ((acc & 0x1) << 7); //rotate
            acc += p[i]; //add
        }

        return acc;
    }

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
     * Check parity for 5/4 code.
     * return true if parity is valid.
     **********************************************************************/
    static inline unsigned char checkParity54(const unsigned char b, bool &error)
    {
        auto x = b ^ (b >> 2);
        x = x ^ (x >> 1) ^ (b >> 4);

        if (x & 1) {
            error = true;
        }

        return b & 0xf;
    }

    /***********************************************************************
    * Check parity for 6/4 code.
    * return true if parity is valid.
    **********************************************************************/
    static inline unsigned char checkParity64(const unsigned char b, bool &error)
    {
        auto x = b ^ (b >> 1) ^ (b >> 2);
        auto y = x ^ b ^ (b >> 3);
        x ^= b >> 4;
        y ^= b >> 5;

        if ((x | y) & 1) {
            error = true;
        }

        return b & 0xf;
    }

    /***********************************************************************
     * Decode 7 bits into a 4 bit word with single bit correction.
     * Non standard version used in sx1272.
     * Set error to true when a parity error was detected
     * Non correctable errors are indistinguishable from single or no errors
     * therefore no 'bad' variable is proposed
     **********************************************************************/
    static inline unsigned char decodeHamming74sx(const unsigned char b, bool &error)
    {
        auto b0 = (b >> 0) & 0x1;
        auto b1 = (b >> 1) & 0x1;
        auto b2 = (b >> 2) & 0x1;
        auto b3 = (b >> 3) & 0x1;
        auto b4 = (b >> 4) & 0x1;
        auto b5 = (b >> 5) & 0x1;
        auto b6 = (b >> 6) & 0x1;

        auto p0 = (b0 ^ b1 ^ b2 ^ b4);
        auto p1 = (b1 ^ b2 ^ b3 ^ b5);
        auto p2 = (b0 ^ b1 ^ b3 ^ b6);

        auto parity = (p0 << 0) | (p1 << 1) | (p2 << 2);

        if (parity != 0) {
            error = true;
        }

        switch (parity)
        {
            case 0x5: return (b ^ 1) & 0xf;
            case 0x7: return (b ^ 2) & 0xf;
            case 0x3: return (b ^ 4) & 0xf;
            case 0x6: return (b ^ 8) & 0xf;
            case 0x0:
            case 0x1:
            case 0x2:
            case 0x4: break;
        }

        return b & 0xf;
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
};

#endif // INCLUDE_CHIRPCHATDEMODDECODERLORA_H
