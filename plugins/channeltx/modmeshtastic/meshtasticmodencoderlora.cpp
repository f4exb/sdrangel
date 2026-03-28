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

#include "meshtasticmodencoderlora.h"

void MeshtasticModEncoderLoRa::addChecksum(QByteArray& bytes)
{
    uint16_t crc = sx1272DataChecksum(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    bytes.append(crc & 0xff);
    bytes.append((crc >> 8) & 0xff);
}

void MeshtasticModEncoderLoRa::encodeBytes(
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
	unsigned int dOfs = 0;

    std::vector<uint8_t> codewords(numCodewords);

    if (hasHeader)
    {
        std::vector<uint8_t> hdr(3);
        unsigned int payloadSize = bytes.size() - (hasCRC ? 2 : 0); // actual payload size is without CRC
        hdr[0] = payloadSize % 256;
        hdr[1] = (hasCRC ? 1 : 0) | (nbParityBits << 1);
        hdr[2] = headerChecksum(hdr.data());

        // Nibble decomposition and parity bit(s) addition. LSNibble first.
        codewords[cOfs++] = encodeHamming84sx(hdr[0] >> 4);
        codewords[cOfs++] = encodeHamming84sx(hdr[0] & 0xf);	// length
        codewords[cOfs++] = encodeHamming84sx(hdr[1] & 0xf);	// crc / fec info
        codewords[cOfs++] = encodeHamming84sx(hdr[2] >> 4);  	// checksum
        codewords[cOfs++] = encodeHamming84sx(hdr[2] & 0xf);
    }

    // Fill first interleaver block (explicit header + first payload codewords) with 4/8 FEC.
    if (firstBlockCodewords > headerSize)
    {
        encodeFec(
            codewords,
            4,
            cOfs,
            dOfs,
            reinterpret_cast<const uint8_t*>(bytes.data()),
            bytes.size(),
            firstBlockCodewords - headerSize
        );
        Sx1272ComputeWhitening(codewords.data() + headerSize, firstBlockCodewords - headerSize, 0, headerParityBits);
    }

    // Encode and whiten remaining payload blocks with payload coding rate.
    if (remainingCodewords > 0U)
    {
        unsigned int cOfs2 = cOfs;
        encodeFec(
            codewords,
            nbParityBits,
            cOfs,
            dOfs,
            reinterpret_cast<const uint8_t*>(bytes.data()),
            bytes.size(),
            remainingCodewords
        );
        Sx1272ComputeWhitening(
            codewords.data() + cOfs2,
            remainingCodewords,
            static_cast<int>(firstBlockCodewords - headerSize),
            nbParityBits
        );
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

void MeshtasticModEncoderLoRa::encodeFec(
        std::vector<uint8_t> &codewords,
        unsigned int nbParityBits,
        unsigned int& cOfs,
        unsigned int& dOfs,
        const uint8_t *bytes,
        const unsigned int bytesLength,
        const unsigned int codewordCount
)
{
    for (unsigned int i = 0; i < codewordCount; i++, dOfs++)
    {
        const unsigned int byteIdx = dOfs / 2;
        const uint8_t byteVal = byteIdx < bytesLength ? bytes[byteIdx] : 0U;

        if (nbParityBits == 1)
        {
            if (dOfs % 2 == 1) {
                codewords[cOfs++] = encodeParity54(byteVal >> 4);
            } else {
                codewords[cOfs++] = encodeParity54(byteVal & 0xf);
            }
        }
        else if (nbParityBits == 2)
        {
            if (dOfs % 2 == 1) {
                codewords[cOfs++] = encodeParity64(byteVal >> 4);
            } else {
                codewords[cOfs++] = encodeParity64(byteVal & 0xf);
            }
        }
        else if (nbParityBits == 3)
        {
            if (dOfs % 2 == 1) {
                codewords[cOfs++] = encodeHamming74sx(byteVal >> 4);
            } else {
                codewords[cOfs++] = encodeHamming74sx(byteVal & 0xf);
            }
        }
        else if (nbParityBits == 4)
        {
            if (dOfs % 2 == 1) {
                codewords[cOfs++] = encodeHamming84sx(byteVal >> 4);
            } else {
                codewords[cOfs++] = encodeHamming84sx(byteVal & 0xf);
            }
        }
        else
        {
            if (dOfs % 2 == 1) {
                codewords[cOfs++] = byteVal >> 4;
            } else {
                codewords[cOfs++] = byteVal & 0xf;
            }
        }
    }
}
