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

#include "meshcoremodencoderlora.h"

void MeshcoreModEncoderLoRa::addChecksum(QByteArray& bytes)
{
    // Standard LoRa payload CRC per lora_payload_crc (gr4-lora crc.hpp):
    //   1. CRC-16-CCITT (0x1021) over first (size - 2) bytes
    //   2. XOR result with last 2 bytes as uint16 LE
    // This matches the SX1262 hardware CRC in explicit header mode.
    if (bytes.size() < 2) {
        bytes.append(static_cast<char>(0));
        bytes.append(static_cast<char>(0));
        return;
    }

    uint16_t crc = 0x0000;
    for (int i = 0; i < bytes.size() - 2; i++) {
        crc ^= static_cast<uint16_t>(static_cast<uint8_t>(bytes[i])) << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }

    crc ^= static_cast<uint16_t>(static_cast<uint8_t>(bytes[bytes.size() - 1]));
    crc ^= static_cast<uint16_t>(static_cast<uint8_t>(bytes[bytes.size() - 2])) << 8;

    bytes.append(static_cast<char>(crc & 0xff));
    bytes.append(static_cast<char>((crc >> 8) & 0xff));
}

void MeshcoreModEncoderLoRa::encodeBytes(
        const QByteArray& bytes,
        std::vector<unsigned short>& symbols,
        unsigned int payloadNbSymbolBits,
        unsigned int headerNbSymbolBits,
        bool hasHeader,
        bool hasCRC,
        unsigned int nbParityBits
)
{
    if (payloadNbSymbolBits < 5) {
        return;
    }

    if (hasHeader && (headerNbSymbolBits < headerCodewords)) {
        return;
    }

    const unsigned int payloadNibbleCount = bytes.size() * 2U;
    const unsigned int firstBlockCodewords = hasHeader ? headerNbSymbolBits : payloadNbSymbolBits;
    const unsigned int headerSize = hasHeader ? headerCodewords : 0U;
    const unsigned int payloadInFirstBlock = firstBlockCodewords > headerSize
        ? std::min(payloadNibbleCount, firstBlockCodewords - headerSize)
        : 0U;
    const unsigned int remainingPayloadNibbles = payloadNibbleCount > payloadInFirstBlock
        ? (payloadNibbleCount - payloadInFirstBlock)
        : 0U;
    const unsigned int remainingCodewords = remainingPayloadNibbles > 0U
        ? roundUp(remainingPayloadNibbles, payloadNbSymbolBits)
        : 0U;
    const unsigned int numCodewords = firstBlockCodewords + remainingCodewords;

    unsigned int cOfs = 0;

    std::vector<uint8_t> codewords(numCodewords);

    if (hasHeader)
    {
        std::vector<uint8_t> hdr(3);
        unsigned int payloadSize = bytes.size() - (hasCRC ? 2 : 0); // actual payload size is without CRC
        hdr[0] = payloadSize % 256;
        hdr[1] = (hasCRC ? 1 : 0) | (nbParityBits << 1);
        // Standard LoRa header checksum per Tapparel & Burg Section III-A.
        // XOR-based checksum from 3 header nibbles:
        //   n0 = length_hi, n1 = length_lo, n2 = (cr<<1)|has_crc
        {
            uint8_t n0 = (hdr[0] >> 4) & 0x0F;
            uint8_t n1 = hdr[0] & 0x0F;
            uint8_t n2 = hdr[1] & 0x0F;
            bool a0 = (n0 >> 3) & 1, a1 = (n0 >> 2) & 1, a2 = (n0 >> 1) & 1, a3 = n0 & 1;
            bool a4 = (n1 >> 3) & 1, a5 = (n1 >> 2) & 1, a6 = (n1 >> 1) & 1, a7 = n1 & 1;
            bool a8 = (n2 >> 3) & 1, a9 = (n2 >> 2) & 1, a10 = (n2 >> 1) & 1, a11 = n2 & 1;
            bool c4 = a0 ^ a1 ^ a2 ^ a3;
            bool c3 = a0 ^ a4 ^ a5 ^ a6 ^ a11;
            bool c2 = a1 ^ a4 ^ a7 ^ a8 ^ a10;
            bool c1 = a2 ^ a5 ^ a7 ^ a9 ^ a10 ^ a11;
            bool c0 = a3 ^ a6 ^ a8 ^ a9 ^ a10 ^ a11;
            hdr[2] = static_cast<uint8_t>((c4 << 4) | (c3 << 3) | (c2 << 2) | (c1 << 1) | c0);
        }

        // Nibble decomposition and parity bit(s) addition. LSNibble first.
        codewords[cOfs++] = encodeHamming84sx(hdr[0] >> 4);
        codewords[cOfs++] = encodeHamming84sx(hdr[0] & 0xf);	// length
        codewords[cOfs++] = encodeHamming84sx(hdr[1] & 0xf);	// crc / fec info
        codewords[cOfs++] = encodeHamming84sx(hdr[2] >> 4);  	// checksum
        codewords[cOfs++] = encodeHamming84sx(hdr[2] & 0xf);
    }

    // Pre-FEC whitening: whiten data nibbles before Hamming FEC encoding.
    // Standard LoRa order: payload -> whiten -> Hamming FEC -> interleave -> gray.
    // CRC nibbles are NOT whitened — only actual payload data.
    const unsigned int payloadNibblesOnly = bytes.size() * 2U - (hasCRC ? 4U : 0U);
    const unsigned int totalPayloadNibbles = firstBlockCodewords > headerSize
        ? (firstBlockCodewords - headerSize + remainingCodewords)
        : 0U;

    if (totalPayloadNibbles > 0U)
    {
        std::vector<uint8_t> nibbles(totalPayloadNibbles, 0);
        const uint8_t *rawBytes = reinterpret_cast<const uint8_t*>(bytes.data());

        for (unsigned int i = 0; i < totalPayloadNibbles; i++)
        {
            unsigned int byteIdx = i / 2;
            if (byteIdx < static_cast<unsigned int>(bytes.size())) {
                nibbles[i] = (i % 2 == 0)
                    ? (rawBytes[byteIdx] & 0xf)
                    : ((rawBytes[byteIdx] >> 4) & 0xf);
            }
        }

        // Whiten payload nibbles only (not CRC nibbles).
        if (payloadNibblesOnly > 0) {
            loRaWhitenNibbles(nibbles.data(), std::min(payloadNibblesOnly, totalPayloadNibbles), 0);
        }

        // Fill first interleaver block (explicit header + first payload codewords) with 4/8 FEC.
        if (firstBlockCodewords > headerSize)
        {
            const unsigned int payloadNibblesInFirst = firstBlockCodewords - headerSize;

            for (unsigned int i = 0; i < payloadNibblesInFirst; i++) {
                codewords[cOfs++] = encodeHamming84sx(nibbles[i]);
            }
        }

        // Encode remaining payload blocks with payload coding rate.
        if (remainingCodewords > 0U)
        {
            const unsigned int payloadNibblesInFirst = firstBlockCodewords - headerSize;

            for (unsigned int i = 0; i < remainingCodewords; i++)
            {
                uint8_t nib = nibbles[payloadNibblesInFirst + i];
                if (nbParityBits == 1) {
                    codewords[cOfs++] = encodeParity54(nib);
                } else if (nbParityBits == 2) {
                    codewords[cOfs++] = encodeParity64(nib);
                } else if (nbParityBits == 3) {
                    codewords[cOfs++] = encodeHamming74sx(nib);
                } else {
                    codewords[cOfs++] = encodeHamming84sx(nib);
                }
            }
        }
    }

    const unsigned int numSymbols = hasHeader
        ? (headerSymbols + (remainingCodewords / payloadNbSymbolBits) * (4U + nbParityBits))
        : ((numCodewords / payloadNbSymbolBits) * (4U + nbParityBits));

    // interleave the codewords into symbols
    symbols.clear();
    symbols.resize(numSymbols);

    if (hasHeader)
    {
        diagonalInterleaveSx(codewords.data(), firstBlockCodewords, symbols.data(), headerNbSymbolBits, headerParityBits);

        // Add even parity bit at position headerNbSymbolBits for each
        // header symbol.  Standard LoRa header uses reduced rate:
        // sf_app = sf-2 data bits + 1 even parity bit + zero padding.
        for (unsigned int i = 0; i < headerSymbols; i++) {
            bool parity = false;
            for (unsigned int b = 0; b < headerNbSymbolBits; b++) {
                parity ^= static_cast<bool>((symbols[i] >> b) & 1);
            }
            symbols[i] |= (static_cast<unsigned short>(parity) << headerNbSymbolBits);
        }

        if (remainingCodewords > 0U) {
            diagonalInterleaveSx(
                codewords.data() + firstBlockCodewords,
                remainingCodewords,
                symbols.data() + headerSymbols,
                payloadNbSymbolBits,
                nbParityBits
            );
        }
    }
    else
    {
        diagonalInterleaveSx(codewords.data(), numCodewords, symbols.data(), payloadNbSymbolBits, nbParityBits);
    }

    // gray decode
    for (auto &sym : symbols) {
        sym = grayToBinary16(sym);
    }
}


