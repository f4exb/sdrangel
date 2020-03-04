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

#include "chirpchatmodencoderlora.h"

void ChirpChatModEncoderLoRa::addChecksum(QByteArray& bytes)
{
    uint16_t crc = sx1272DataChecksum(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    bytes.append(crc & 0xff);
    bytes.append((crc >> 8) & 0xff);
}

void ChirpChatModEncoderLoRa::encodeBytes(
        const QByteArray& bytes,
        std::vector<unsigned short>& symbols,
        unsigned int nbSymbolBits,
        bool hasHeader,
        bool hasCRC,
        unsigned int nbParityBits
)
{
    if (nbSymbolBits < 5) {
        return;
    }

    const unsigned int numCodewords = roundUp(bytes.size()*2 + (hasHeader ? headerCodewords : 0), nbSymbolBits); // uses payload + CRC for encoding size
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

    unsigned int headerSize = cOfs;

    // fill nbSymbolBits codewords with 8 bit codewords using payload data (ecode and whiten)
    encodeFec(codewords, 4, cOfs, dOfs, reinterpret_cast<const uint8_t*>(bytes.data()), nbSymbolBits - headerSize);
    Sx1272ComputeWhitening(codewords.data() + headerSize, nbSymbolBits - headerSize, 0, headerParityBits);

    // encode and whiten the rest of the payload with 4 + nbParityBits bits codewords
    if (numCodewords > nbSymbolBits)
    {
        unsigned int cOfs2 = cOfs;
        encodeFec(codewords, nbParityBits, cOfs, dOfs, reinterpret_cast<const uint8_t*>(bytes.data()), numCodewords - nbSymbolBits);
        Sx1272ComputeWhitening(codewords.data() + cOfs2, numCodewords - nbSymbolBits, nbSymbolBits - headerSize, nbParityBits);
    }

    // header is always coded with 8 bits and yields exactly 8 symbols (headerSymbols)
    const unsigned int numSymbols = headerSymbols + (numCodewords / nbSymbolBits - 1) * (4 + nbParityBits);

    // interleave the codewords into symbols
    symbols.clear();
    symbols.resize(numSymbols);
    diagonalInterleaveSx(codewords.data(), nbSymbolBits, symbols.data(), nbSymbolBits, headerParityBits);

    if (numCodewords > nbSymbolBits) {
        diagonalInterleaveSx(codewords.data() + nbSymbolBits, numCodewords - nbSymbolBits, symbols.data() + headerSymbols, nbSymbolBits, nbParityBits);
    }

    // gray decode
    for (auto &sym : symbols) {
        sym = grayToBinary16(sym);
    }
}

void ChirpChatModEncoderLoRa::encodeFec(
        std::vector<uint8_t> &codewords,
        unsigned int nbParityBits,
        unsigned int& cOfs,
        unsigned int& dOfs,
        const uint8_t *bytes,
        const unsigned int codewordCount
)
{
    for (unsigned int i = 0; i < codewordCount; i++, dOfs++)
    {
        if (nbParityBits == 1)
        {
            if (dOfs % 2 == 1) {
                codewords[cOfs++] = encodeParity54(bytes[dOfs/2] >> 4);
            } else {
                codewords[cOfs++] = encodeParity54(bytes[dOfs/2] & 0xf);
            }
        }
        else if (nbParityBits == 2)
        {
            if (dOfs % 2 == 1) {
                codewords[cOfs++] = encodeParity64(bytes[dOfs/2] >> 4);
            } else {
                codewords[cOfs++] = encodeParity64(bytes[dOfs/2] & 0xf);
            }
        }
        else if (nbParityBits == 3)
        {
            if (dOfs % 2 == 1) {
                codewords[cOfs++] = encodeHamming74sx(bytes[dOfs/2] >> 4);
            } else {
                codewords[cOfs++] = encodeHamming74sx(bytes[dOfs/2] & 0xf);
            }
        }
        else if (nbParityBits == 4)
        {
            if (dOfs % 2 == 1) {
                codewords[cOfs++] = encodeHamming84sx(bytes[dOfs/2] >> 4);
            } else {
                codewords[cOfs++] = encodeHamming84sx(bytes[dOfs/2] & 0xf);
            }
        }
        else
        {
            if (dOfs % 2 == 1) {
                codewords[cOfs++] = bytes[dOfs/2] >> 4;
            } else {
                codewords[cOfs++] = bytes[dOfs/2] & 0xf;
            }
        }
    }
}
