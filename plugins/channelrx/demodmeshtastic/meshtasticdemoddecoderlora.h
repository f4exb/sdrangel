///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_MESHTASTICDEMODDECODERLORA_H
#define INCLUDE_MESHTASTICDEMODDECODERLORA_H

#include <cmath>
#include <limits>
#include <vector>
#include <QByteArray>

class MeshtasticDemodDecoderLoRa
{
public:
    static void decodeBytes(
        QByteArray& bytes,
        const std::vector<unsigned short>& inSymbols,
        unsigned int payloadNbSymbolBits,
        unsigned int headerNbSymbolBits,
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

    static void decodeBytesSoft(
        QByteArray& bytes,
        const std::vector<std::vector<float>>& inMagnitudes,
        const std::vector<unsigned short>& inSymbols,
        unsigned int spreadFactor,
        unsigned int bandwidth,
        unsigned int payloadNbSymbolBits,
        unsigned int headerNbSymbolBits,
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
        unsigned int payloadNbSymbolBits,
        unsigned int headerNbSymbolBits,
        unsigned int nbParityBits,
        unsigned int packetLength,
        bool hasHeader,
        bool hasCRC,
        unsigned int& numSymbols,
        unsigned int& numCodewords
    );

    static void decodeHeader(
        const std::vector<unsigned short>& inSymbols,
        unsigned int headerNbSymbolBits,
        bool& hasCRC,
        unsigned int& nbParityBits,
        unsigned int& packetLength,
        int& headerParityStatus,
        bool& headerCRCStatus
    );

private:
    static constexpr unsigned int headerParityBits = 4;
    static constexpr unsigned int headerSymbols = 8;
    static constexpr unsigned int headerCodewords = 5;

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

    static inline int modInt(int a, int b)
    {
        if (b <= 0) {
            return 0;
        }

        return (a % b + b) % b;
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
        const int cwLen = 4 + static_cast<int>(nbParityBits);

        for (unsigned int x = 0; x < numSymbols / (4 + nbParityBits); x++)
        {
            const unsigned int cwOff = x*nbSymbolBits;
            const unsigned int symOff = x*(4U + nbParityBits);

            for (int i = 0; i < cwLen; i++)
            {
                const uint16_t sym = symbols[symOff + i];

                for (int j = 0; j < static_cast<int>(nbSymbolBits); j++)
                {
                    const uint8_t bit = (sym >> (static_cast<int>(nbSymbolBits) - 1 - j)) & 0x1;
                    const int row = ((i - j - 1) % static_cast<int>(nbSymbolBits) + static_cast<int>(nbSymbolBits)) % static_cast<int>(nbSymbolBits);
                    codewords[cwOff + static_cast<unsigned int>(row)] |= (bit << (cwLen - 1 - i));
                }
            }
        }
    }

    /***********************************************************************
     * Dewhitening sequence used by LoRa payload bytes.
     * Matches the sequence used by gr-lora_sdr.
     **********************************************************************/
    static constexpr uint8_t s_whiteningSeq[] = {
        0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE1, 0xC2, 0x85, 0x0B, 0x17, 0x2F, 0x5E, 0xBC, 0x78, 0xF1, 0xE3,
        0xC6, 0x8D, 0x1A, 0x34, 0x68, 0xD0, 0xA0, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08, 0x11, 0x23, 0x47,
        0x8E, 0x1C, 0x38, 0x71, 0xE2, 0xC4, 0x89, 0x12, 0x25, 0x4B, 0x97, 0x2E, 0x5C, 0xB8, 0x70, 0xE0,
        0xC0, 0x81, 0x03, 0x06, 0x0C, 0x19, 0x32, 0x64, 0xC9, 0x92, 0x24, 0x49, 0x93, 0x26, 0x4D, 0x9B,
        0x37, 0x6E, 0xDC, 0xB9, 0x72, 0xE4, 0xC8, 0x90, 0x20, 0x41, 0x82, 0x05, 0x0A, 0x15, 0x2B, 0x56,
        0xAD, 0x5B, 0xB6, 0x6D, 0xDA, 0xB5, 0x6B, 0xD6, 0xAC, 0x59, 0xB2, 0x65, 0xCB, 0x96, 0x2C, 0x58,
        0xB0, 0x61, 0xC3, 0x87, 0x0F, 0x1F, 0x3E, 0x7D, 0xFB, 0xF6, 0xED, 0xDB, 0xB7, 0x6F, 0xDE, 0xBD,
        0x7A, 0xF5, 0xEB, 0xD7, 0xAE, 0x5D, 0xBA, 0x74, 0xE8, 0xD1, 0xA2, 0x44, 0x88, 0x10, 0x21, 0x43,
        0x86, 0x0D, 0x1B, 0x36, 0x6C, 0xD8, 0xB1, 0x63, 0xC7, 0x8F, 0x1E, 0x3C, 0x79, 0xF3, 0xE7, 0xCE,
        0x9C, 0x39, 0x73, 0xE6, 0xCC, 0x98, 0x31, 0x62, 0xC5, 0x8B, 0x16, 0x2D, 0x5A, 0xB4, 0x69, 0xD2,
        0xA4, 0x48, 0x91, 0x22, 0x45, 0x8A, 0x14, 0x29, 0x52, 0xA5, 0x4A, 0x95, 0x2A, 0x54, 0xA9, 0x53,
        0xA7, 0x4E, 0x9D, 0x3B, 0x77, 0xEE, 0xDD, 0xBB, 0x76, 0xEC, 0xD9, 0xB3, 0x67, 0xCF, 0x9E, 0x3D,
        0x7B, 0xF7, 0xEF, 0xDF, 0xBF, 0x7E, 0xFD, 0xFA, 0xF4, 0xE9, 0xD3, 0xA6, 0x4C, 0x99, 0x33, 0x66,
        0xCD, 0x9A, 0x35, 0x6A, 0xD4, 0xA8, 0x51, 0xA3, 0x46, 0x8C, 0x18, 0x30, 0x60, 0xC1, 0x83, 0x07,
        0x0E, 0x1D, 0x3A, 0x75, 0xEA, 0xD5, 0xAA, 0x55, 0xAB, 0x57, 0xAF, 0x5F, 0xBE, 0x7C, 0xF9, 0xF2,
        0xE5, 0xCA, 0x94, 0x28, 0x50, 0xA1, 0x42, 0x84, 0x09, 0x13, 0x27, 0x4F, 0x9F, 0x3F, 0x7F
    };

    static inline void dewhitenPayloadBytes(uint8_t* payload, unsigned int length)
    {
        const unsigned int whiteningSize = static_cast<unsigned int>(sizeof(s_whiteningSeq) / sizeof(s_whiteningSeq[0]));

        for (unsigned int i = 0; i < length; ++i) {
            payload[i] ^= s_whiteningSeq[i % whiteningSize];
        }
    }

    /***********************************************************************
     * Canonical gr-lora_sdr hard-decision codeword decoder.
     * crApp is the LoRa coding-rate parity bits count in [1..4].
     **********************************************************************/
    static inline unsigned char decodeCodewordHard(const unsigned char b, unsigned int crApp, bool &error, bool &bad)
    {
        if ((crApp < 1U) || (crApp > 4U)) {
            bad = true;
            return b & 0xF;
        }

        const unsigned int cwLen = 4U + crApp;
        bool codeword[8] = {false, false, false, false, false, false, false, false};

        for (unsigned int i = 0; i < cwLen; i++) {
            codeword[i] = ((b >> (cwLen - 1U - i)) & 0x1U) != 0U;
        }

        // hamming_dec nibble ordering: {codeword[3], codeword[2], codeword[1], codeword[0]}.
        uint8_t nibbleBits[4] = {
            static_cast<uint8_t>(codeword[3]),
            static_cast<uint8_t>(codeword[2]),
            static_cast<uint8_t>(codeword[1]),
            static_cast<uint8_t>(codeword[0])
        };

        switch (crApp)
        {
        case 4:
        {
            int ones = 0;
            for (unsigned int i = 0; i < cwLen; i++) {
                ones += codeword[i] ? 1 : 0;
            }

            if ((ones % 2) == 0) {
                break; // do not correct even-weight patterns
            }

            // fall through to crApp=3 syndrome logic
        }
        // no break
        case 3:
        {
            const bool s0 = codeword[0] ^ codeword[1] ^ codeword[2] ^ codeword[4];
            const bool s1 = codeword[1] ^ codeword[2] ^ codeword[3] ^ codeword[5];
            const bool s2 = codeword[0] ^ codeword[1] ^ codeword[3] ^ codeword[6];
            const int syndrome = static_cast<int>(s0) + (static_cast<int>(s1) << 1) + (static_cast<int>(s2) << 2);

            if (syndrome != 0) {
                error = true;
            }

            switch (syndrome)
            {
            case 5: nibbleBits[3] ^= 0x1U; break;
            case 7: nibbleBits[2] ^= 0x1U; break;
            case 3: nibbleBits[1] ^= 0x1U; break;
            case 6: nibbleBits[0] ^= 0x1U; break;
            default: break;
            }
            break;
        }
        case 2:
        {
            const bool s0 = codeword[0] ^ codeword[1] ^ codeword[2] ^ codeword[4];
            const bool s1 = codeword[1] ^ codeword[2] ^ codeword[3] ^ codeword[5];
            if (s0 || s1) {
                error = true;
            }
            break;
        }
        case 1:
        default:
        {
            int ones = 0;
            for (unsigned int i = 0; i < cwLen; i++) {
                ones += codeword[i] ? 1 : 0;
            }
            if ((ones % 2) == 0) {
                error = true;
            }
            break;
        }
        }

        bad = false;
        return static_cast<unsigned char>((nibbleBits[0] << 3) | (nibbleBits[1] << 2) | (nibbleBits[2] << 1) | nibbleBits[3]);
    }

    static inline unsigned char decodeCodewordSoft(const std::vector<float>& codewordLLR, unsigned int crApp)
    {
        static const unsigned char cwLUT[16] = {
            0, 23, 45, 58, 78, 89, 99, 116,
            139, 156, 166, 177, 197, 210, 232, 255
        };
        static const unsigned char cwLUTCr5[16] = {
            0, 24, 40, 48, 72, 80, 96, 120,
            136, 144, 160, 184, 192, 216, 232, 240
        };

        if ((crApp < 1U) || (crApp > 4U)) {
            return 0;
        }

        const unsigned int cwLen = 4U + crApp;
        const unsigned char *lut = (crApp == 1U) ? cwLUTCr5 : cwLUT;
        float bestScore = std::numeric_limits<float>::lowest();
        unsigned int bestIdx = 0U;

        for (unsigned int n = 0; n < 16U; n++)
        {
            const unsigned char cw = static_cast<unsigned char>(lut[n] >> (8U - cwLen));
            float score = 0.0f;

            for (unsigned int j = 0; j < cwLen; j++)
            {
                const bool bit = ((cw >> (cwLen - 1U - j)) & 0x1U) != 0U;
                const float v = std::fabs(codewordLLR[j]);
                score += (((bit && (codewordLLR[j] > 0.0f)) || (!bit && (codewordLLR[j] < 0.0f))) ? v : -v);
            }

            if (score > bestScore) {
                bestScore = score;
                bestIdx = n;
            }
        }

        const unsigned char dataNibbleSoft = static_cast<unsigned char>(cwLUT[bestIdx] >> 4);
        return static_cast<unsigned char>(
            (((dataNibbleSoft & 0x1U) != 0U) << 3) |
            (((dataNibbleSoft & 0x2U) != 0U) << 2) |
            (((dataNibbleSoft & 0x4U) != 0U) << 1) |
            ((dataNibbleSoft & 0x8U) != 0U)
        );
    }

    /***********************************************************************
     * Decode 8 bits into a 4 bit word with single bit correction.
     * Set error to true when a parity error was detected.
     **********************************************************************/
    static inline unsigned char decodeHamming84sx(const unsigned char b, bool &error, bool &bad)
    {
        return decodeCodewordHard(b, 4U, error, bad);
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
        bool bad = false;
        return decodeCodewordHard(b, 1U, error, bad);
    }

    /***********************************************************************
    * Check parity for 6/4 code.
    * return true if parity is valid.
    **********************************************************************/
    static inline unsigned char checkParity64(const unsigned char b, bool &error)
    {
        bool bad = false;
        return decodeCodewordHard(b, 2U, error, bad);
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
        bool bad = false;
        return decodeCodewordHard(b, 3U, error, bad);
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

    /**
     * CRC routine used by gr-lora_sdr crc_verif block.
     */
    static inline uint16_t crc16gr(const uint8_t *data, unsigned int length)
    {
        uint16_t crc = 0x0000;

        for (unsigned int i = 0; i < length; i++)
        {
            uint8_t b = data[i];

            for (unsigned char j = 0; j < 8; j++)
            {
                if ((((crc & 0x8000) >> 8) ^ (b & 0x80)) != 0) {
                    crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
                } else {
                    crc = static_cast<uint16_t>(crc << 1);
                }

                b <<= 1;
            }
        }

        return crc;
    }
};

#endif // INCLUDE_MESHTASTICDEMODDECODERLORA_H
