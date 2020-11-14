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

#include "chirpchatdemoddecoderlora.h"

void ChirpChatDemodDecoderLoRa::decodeHeader(
        const std::vector<unsigned short>& inSymbols,
        unsigned int nbSymbolBits,
        bool& hasCRC,
        unsigned int& nbParityBits,
        unsigned int& packetLength,
        int& headerParityStatus,
        bool& headerCRCStatus
)
{
    // with header (H: header 8-bit codeword P: payload-8 bit codeword):
    // nbSymbolBits = 5 |H|H|H|H|H|      codewords => 8 symbols (always) : static headerSymbols = 8
    // nbSymbolBits = 7 |H|H|H|H|H|P|P|
    // without header (P: payload 8-bit codeword):
    // nbSymbolBits = 5 |P|P|P|P|P|      codewords => 8 symbols (always)
    // nbSymbolBits = 7 |P|P|P|P|P|P|P|
    // Actual header is always represented with 5 8-bit codewords : static headerCodewords = 5
    // These 8-bit codewords are encoded with Hamming(4,8) FEC : static headerParityBits = 4

    std::vector<uint16_t> symbols(headerSymbols);
    std::copy(inSymbols.begin(), inSymbols.begin() + headerSymbols, symbols.begin());

    //gray encode
    for (auto &sym : symbols) {
        sym = binaryToGray16(sym);
    }

    std::vector<uint8_t> codewords(nbSymbolBits);

    // Header symbols de-interleave thus headerSymbols (8) symbols into nbSymbolBits (5..12) codewords using header FEC (4/8)
    diagonalDeinterleaveSx(symbols.data(), headerSymbols, codewords.data(), nbSymbolBits, headerParityBits);

    // whitening does not apply to the header codewords
    Sx1272ComputeWhiteningLfsr(codewords.data() + headerCodewords, nbSymbolBits - headerCodewords, 0, headerParityBits);

    bool error = false;
    bool bad = false;
    uint8_t bytes[3];

    // decode actual header inside 8-bit codewords header with 4/8 FEC (5 first codewords)
    bytes[0] = decodeHamming84sx(codewords[1], error, bad) & 0xf;
    bytes[0] |= decodeHamming84sx(codewords[0], error, bad) << 4;	// length

    bytes[1] = decodeHamming84sx(codewords[2], error, bad) & 0xf;	// coding rate and crc enable

    bytes[2] = decodeHamming84sx(codewords[4], error, bad) & 0xf;
    bytes[2] |= decodeHamming84sx(codewords[3], error, bad) << 4;	// checksum

    bytes[2] ^= headerChecksum(bytes);

    if (bad)
    {
        headerParityStatus = (int) ParityError;
    }
    else
    {
        if (error) {
            headerParityStatus = (int) ParityCorrected;
        } else {
            headerParityStatus = (int) ParityOK;
        }

        if (bytes[2] != 0) {
            headerCRCStatus = false;
        } else {
            headerCRCStatus = true;
        }
    }

    hasCRC = (bytes[1] & 1) != 0;
    nbParityBits = (bytes[1] >> 1) & 0x7;
    packetLength = bytes[0];
}

void ChirpChatDemodDecoderLoRa::decodeBytes(
        QByteArray& inBytes,
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
)
{
    // need at least a header (8 symbols of 8 bit codewords) whether an actual header is sent or not
    if (inSymbols.size() < headerSymbols)
    {
        qDebug("ChirpChatDemodDecoderLoRa::decodeBytes: need at least %u symbols for header", headerSymbols);
        earlyEOM = true;
        return;
    }
    else
    {
        earlyEOM = false;
    }

    if (hasHeader)
    {
        decodeHeader(
            inSymbols,
            nbSymbolBits,
            hasCRC,
            nbParityBits,
            packetLength,
            headerParityStatus,
            headerCRCStatus
        );
    }

    qDebug("ChirpChatDemodDecoderLoRa::decodeBytes: crc: %s nbParityBits: %u packetLength: %u",
        hasCRC ? "on": "off", nbParityBits, packetLength);

    if (nbParityBits > 4)
    {
        qDebug("ChirpChatDemodDecoderLoRa::decodeBytes: invalid parity bits in header: %u", nbParityBits);
        return;
    }

    const unsigned int numSymbols = roundUp(inSymbols.size(), 4 + nbParityBits);
    const unsigned int numCodewords = (numSymbols / (4 + nbParityBits))*nbSymbolBits;
    std::vector<uint16_t> symbols(numSymbols);
    std::copy(inSymbols.begin(), inSymbols.end(), symbols.begin());

    //gray encode, when SF > PPM, depad the LSBs with rounding
    for (auto &sym : symbols) {
        sym = binaryToGray16(sym);
    }

    std::vector<uint8_t> codewords(numCodewords);

    // deinterleave / dewhiten the symbols into codewords
    unsigned int sOfs = 0;
    unsigned int cOfs = 0;

    // the first headerSymbols (8 symbols) are coded with 4/8 FEC (thus 8 bit codewords) whether an actual header is present or not
    // this corresponds to nbSymbolBits codewords (therefore LoRa imposes nbSymbolBits >= headerCodewords (5 codewords) this is controlled externally)

    if (nbParityBits != 4) // different FEC between header symbols and the rest of the packet
    {
        // Header symbols de-interleave thus headerSymbols (8) symbols into nbSymbolBits (5..12) codewords using header FEC (4/8)
        diagonalDeinterleaveSx(symbols.data(), headerSymbols, codewords.data(), nbSymbolBits, headerParityBits);

        if (hasHeader) { // whitening does not apply to the header codewords
            Sx1272ComputeWhiteningLfsr(codewords.data() + headerCodewords, nbSymbolBits - headerCodewords, 0, headerParityBits);
        } else {
            Sx1272ComputeWhiteningLfsr(codewords.data(), nbSymbolBits, 0, headerParityBits);
        }

        cOfs += nbSymbolBits;   // nbSymbolBits codewords in header
        sOfs += headerSymbols;  // headerSymbols symbols in header

        if (numSymbols - sOfs > 0) // remaining payload symbols after header symbols using their own FEC (4/5..4/7)
        {
            diagonalDeinterleaveSx(symbols.data() + sOfs, numSymbols - sOfs, codewords.data() + cOfs, nbSymbolBits, nbParityBits);

            if (hasHeader) {
                Sx1272ComputeWhiteningLfsr(codewords.data() + cOfs, numCodewords - cOfs, nbSymbolBits - headerCodewords, nbParityBits);
            } else {
                Sx1272ComputeWhiteningLfsr(codewords.data() + cOfs, numCodewords - cOfs, nbSymbolBits, nbParityBits);
            }
        }
    }
    else // uniform 4/8 FEC for all the packet
    {
        // De-interleave the whole packet thus numSymbols into nbSymbolBits (5..12) codewords using packet FEC (4/8)
        diagonalDeinterleaveSx(symbols.data(), numSymbols, codewords.data(), nbSymbolBits, nbParityBits);

        if (hasHeader) { // whitening does not apply to the header codewords
            Sx1272ComputeWhiteningLfsr(codewords.data() + headerCodewords, numCodewords - headerCodewords, 0, nbParityBits);
        } else {
            Sx1272ComputeWhiteningLfsr(codewords.data(), numCodewords, 0, nbParityBits);
        }
    }

    // Now we have nbSymbolBits 8-bit codewords (4/8 FEC) possibly containing the actual header followed by the rest of payload codewords with their own FEC (4/5..4/8)

    std::vector<uint8_t> bytes((codewords.size()+1) / 2);
    unsigned int dOfs = 0;
    cOfs = 0;

    unsigned int dataLength = packetLength + 3 + (hasCRC ? 2 : 0);  // include  header and CRC

    if (hasHeader)
    {
        cOfs = headerCodewords;
        dOfs = 6;
    }
    else
    {
        cOfs = 0;
        dOfs = 0;
    }

    if (dataLength > bytes.size())
    {
        qDebug("ChirpChatDemodDecoderLoRa::decodeBytes: not enough data %lu vs %u", bytes.size(), dataLength);
        earlyEOM = true;
        return;
    }

    // decode the rest of the payload inside 8-bit codewords header with 4/8 FEC
    bool error = false;
    bool bad = false;

    for (; cOfs < nbSymbolBits; cOfs++, dOfs++)
    {
        if (dOfs % 2 == 1) {
            bytes[dOfs/2] |= decodeHamming84sx(codewords[cOfs], error, bad) << 4;
        } else {
            bytes[dOfs/2] = decodeHamming84sx(codewords[cOfs], error, bad) & 0xf;
        }
    }

    if (dOfs % 2 == 1) // decode the start of the payload codewords with their own FEC when not on an even boundary
    {
        if (nbParityBits == 1) {
            bytes[dOfs/2] |= checkParity54(codewords[cOfs++], error) << 4;
        } else if (nbParityBits == 2) {
            bytes[dOfs/2] |= checkParity64(codewords[cOfs++], error) << 4;
        } else if (nbParityBits == 3){
            bytes[dOfs/2] |= decodeHamming74sx(codewords[cOfs++], error) << 4;
        } else if (nbParityBits == 4){
            bytes[dOfs/2] |= decodeHamming84sx(codewords[cOfs++], error, bad) << 4;
        } else {
            bytes[dOfs/2] |= codewords[cOfs++] << 4;
        }

        dOfs++;
    }

    dOfs /= 2;

    // decode the rest of the payload codewords with their own FEC

    if (nbParityBits == 1)
    {
        for (unsigned int i = dOfs; i < dataLength; i++)
        {
            bytes[i] = checkParity54(codewords[cOfs++],error);
            bytes[i] |= checkParity54(codewords[cOfs++], error) << 4;
        }
    }
    else if (nbParityBits == 2)
    {
        for (unsigned int i = dOfs; i < dataLength; i++)
        {
            bytes[i] = checkParity64(codewords[cOfs++], error);
            bytes[i] |= checkParity64(codewords[cOfs++],error) << 4;
        }
    }
    else if (nbParityBits == 3)
    {
        for (unsigned int i = dOfs; i < dataLength; i++)
        {
            bytes[i] = decodeHamming74sx(codewords[cOfs++], error) & 0xf;
            bytes[i] |= decodeHamming74sx(codewords[cOfs++], error) << 4;
        }
    }
    else if (nbParityBits == 4)
    {
        for (unsigned int i = dOfs; i < dataLength; i++)
        {
            bytes[i] = decodeHamming84sx(codewords[cOfs++], error, bad) & 0xf;
            bytes[i] |= decodeHamming84sx(codewords[cOfs++], error, bad) << 4;
        }
    }
    else
    {
        for (unsigned int i = dOfs; i < dataLength; i++)
        {
            bytes[i] = codewords[cOfs++] & 0xf;
            bytes[i] |= codewords[cOfs++] << 4;
        }
    }

    if (bad) {
        payloadParityStatus = (int) ParityError;
    } else if (error) {
        payloadParityStatus = (int) ParityCorrected;
    } else {
        payloadParityStatus = (int) ParityOK;
    }

    // finalization:
    //   adjust offsets dpending on header and CRC presence
    //   compute and verify payload CRC if present

    if (hasHeader)
    {
        dOfs = 3;        // skip header
        dataLength -= 3; // remove header

        if (hasCRC) // always compute crc if present skipping the header
        {
            uint16_t crc = sx1272DataChecksum(bytes.data() + dOfs, packetLength);
            uint16_t packetCRC = bytes[dOfs + packetLength] | (bytes[dOfs + packetLength + 1] << 8);

            if (crc != packetCRC) {
                payloadCRCStatus = false;
            } else {
                payloadCRCStatus = true;
            }
        }
    }
    else
    {
        dOfs = 0;  // no header to skip

        if (hasCRC)
        {
            uint16_t crc = sx1272DataChecksum(bytes.data(), packetLength);
            uint16_t packetCRC = bytes[packetLength] | (bytes[packetLength + 1] << 8);

            if (crc != packetCRC) {
                payloadCRCStatus = false;
            } else {
                payloadCRCStatus = true;
            }
        }
    }

    inBytes.resize(dataLength);
    std::copy(bytes.data() + dOfs, bytes.data() + dOfs + dataLength, inBytes.data());
}

void ChirpChatDemodDecoderLoRa::getCodingMetrics(
    unsigned int nbSymbolBits,
    unsigned int nbParityBits,
    unsigned int packetLength,
    bool hasHeader,
    bool hasCRC,
    unsigned int& numSymbols,
    unsigned int& numCodewords
)
{
    numCodewords = roundUp((packetLength + (hasCRC ? 2 : 0))*2 + (hasHeader ? headerCodewords : 0), nbSymbolBits); // uses payload + CRC for encoding size
    numSymbols = headerSymbols + (numCodewords / nbSymbolBits - 1) * (4 + nbParityBits); // header is always coded with 8 bits and yields exactly 8 symbols (headerSymbols)
}
