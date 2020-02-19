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

#include "lorademoddecoderlora.h"

void LoRaDemodDecoderLoRa::decodeBytes(
        QByteArray& inBytes,
        const std::vector<unsigned short>& inSymbols,
        unsigned int nbSymbolBits,
        bool hasHeader,
        bool& hasCRC,
        unsigned int& nbParityBits,
        unsigned int& packetLength,
        bool errorCheck,
        bool& headerParityStatus,
        bool& headerCRCStatus,
        bool& payloadParityStatus,
        bool& payloadCRCStatus
)
{
    if (inSymbols.size() < 8) { // need at least a header
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

    if (nbParityBits != 4)
    {
        diagonalDeinterleaveSx(symbols.data(), 8, codewords.data(), nbSymbolBits, 4);

        if (hasHeader) {
            Sx1272ComputeWhiteningLfsr(codewords.data() + headerCodewords, nbSymbolBits - headerCodewords, 0, headerParityBits);
        } else {
            Sx1272ComputeWhiteningLfsr(codewords.data(), nbSymbolBits, 0, headerParityBits);
        }

        cOfs += nbSymbolBits;
        sOfs += headerSymbols;

        if (numSymbols - sOfs > 0)
        {
            diagonalDeinterleaveSx(symbols.data() + sOfs, numSymbols - sOfs, codewords.data() + cOfs, nbSymbolBits, nbParityBits);

            if (hasHeader) {
                Sx1272ComputeWhiteningLfsr(codewords.data() + cOfs, numCodewords - cOfs, nbSymbolBits - headerCodewords, nbParityBits);
            } else {
                Sx1272ComputeWhiteningLfsr(codewords.data() + cOfs, numCodewords - cOfs, nbSymbolBits, nbParityBits);
            }
        }
    }
    else
    {
        diagonalDeinterleaveSx(symbols.data(), numSymbols, codewords.data(), nbSymbolBits, nbParityBits);

        if (hasHeader) {
            Sx1272ComputeWhiteningLfsr(codewords.data() + headerCodewords, numCodewords - headerCodewords, 0, nbParityBits);
        } else {
            Sx1272ComputeWhiteningLfsr(codewords.data(), numCodewords, 0, nbParityBits);
        }
    }

    bool error = false;
    bool bad = false;
    std::vector<uint8_t> bytes((codewords.size()+1) / 2);
    unsigned int dOfs = 0;
    cOfs = 0;

    unsigned int dataLength = 0;
    bool hasCRCInt;                 // payload has CRC indicator internal (explicit or implicit)
    unsigned int nbParityBitsInt;   // number of parity bits internal (explicit or implicit)
    unsigned int packetLengthInt;   // packet length internal (explicit or implicit)

    if (hasHeader)
    {
        bytes[0] = decodeHamming84sx(codewords[1], error, bad) & 0xf;
        bytes[0] |= decodeHamming84sx(codewords[0], error, bad) << 4;	// length

        bytes[1] = decodeHamming84sx(codewords[2], error, bad) & 0xf;	// coding rate and crc enable

        bytes[2] = decodeHamming84sx(codewords[4], error, bad) & 0xf;
        bytes[2] |= decodeHamming84sx(codewords[3], error, bad) << 4;	// checksum

        bytes[2] ^= headerChecksum(bytes.data());

        if (error)
        {
            qDebug("LoRaDemodDecoderLoRa::decodeBytes: header Hamming error");
            headerParityStatus = false;

            if (errorCheck) {
                return;
            }
        }
        else
        {
            headerParityStatus = true;

            if (bytes[2] != 0)
            {
                headerCRCStatus = false;
                qDebug("LoRaDemodDecoderLoRa::decodeBytes: header CRC error");

                if (errorCheck) {
                    return;
                }
            }
            else
            {
                headerCRCStatus = true;
            }
        }

        hasCRCInt = (bytes[1] & 1) != 0;
        nbParityBitsInt = (bytes[1] >> 1) & 0x7;

        if (nbParityBitsInt > 4) {
            return;
        }

        packetLengthInt = bytes[0];
        //dataLength = packetLengthInt + 3 + (hasCRCInt ? 2 : 0);  // include  header and crc
        dataLength = packetLengthInt + 5;

        cOfs = headerCodewords;
        dOfs = 6;
    }
    else
    {
        hasCRCInt = hasCRC;
        nbParityBitsInt = nbParityBits;
        packetLengthInt = packetLength;
        cOfs = 0;
        dOfs = 0;

        if (hasCRCInt) {
            dataLength = packetLengthInt + 2;
        } else {
            dataLength = packetLengthInt;
        }
    }

    if (dataLength > bytes.size()) {
        return;
    }

    for (; cOfs < nbSymbolBits; cOfs++, dOfs++)
    {
        if (dOfs % 2 == 1) {
            bytes[dOfs/2] |= decodeHamming84sx(codewords[cOfs], error, bad) << 4;
        } else {
            bytes[dOfs/2] = decodeHamming84sx(codewords[cOfs], error, bad) & 0xf;
        }
    }

    if (dOfs % 2 == 1)
    {
        if (nbParityBitsInt == 1) {
            bytes[dOfs/2] |= checkParity54(codewords[cOfs++], error) << 4;
        } else if (nbParityBitsInt == 2) {
            bytes[dOfs/2] |= checkParity64(codewords[cOfs++], error) << 4;
        } else if (nbParityBitsInt == 3){
            bytes[dOfs/2] |= decodeHamming74sx(codewords[cOfs++], error) << 4;
        } else if (nbParityBitsInt == 4){
            bytes[dOfs/2] |= decodeHamming84sx(codewords[cOfs++], error, bad) << 4;
        } else {
            bytes[dOfs/2] |= codewords[cOfs++] << 4;
        }

        dOfs++;
    }

    dOfs /= 2;

    if (error)
    {
        qDebug("LoRaDemodDecoderLoRa::decodeBytes: Hamming decode (1) error");
        payloadParityStatus = false;

        if (errorCheck) {
            return;
        }
    }
    else
    {
        payloadParityStatus = true;
    }

    //decode each codeword as 2 bytes with correction

    if (nbParityBitsInt == 1)
    {
        for (unsigned int i = dOfs; i < dataLength; i++)
        {
            bytes[i] = checkParity54(codewords[cOfs++],error);
            bytes[i] |= checkParity54(codewords[cOfs++], error) << 4;
        }
    }
    else if (nbParityBitsInt == 2)
    {
        for (unsigned int i = dOfs; i < dataLength; i++)
        {
            bytes[i] = checkParity64(codewords[cOfs++], error);
            bytes[i] |= checkParity64(codewords[cOfs++],error) << 4;
        }
    }
    else if (nbParityBitsInt == 3)
    {
        for (unsigned int i = dOfs; i < dataLength; i++)
        {
            bytes[i] = decodeHamming74sx(codewords[cOfs++], error) & 0xf;
            bytes[i] |= decodeHamming74sx(codewords[cOfs++], error) << 4;
        }
    }
    else if (nbParityBitsInt == 4)
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

    if (error)
    {
        qDebug("LoRaDemodDecoderLoRa::decodeBytes: Hamming decode (2) error");
        payloadParityStatus = false;

        if (errorCheck) {
            return;
        }
    }

    if (hasHeader)
    {
        dOfs = 3;
        dataLength -= 5;

        if (hasCRCInt) // always compute crc if present
        {
            uint16_t crc = sx1272DataChecksum(bytes.data() + dOfs, packetLengthInt - 2);
            uint16_t packetCRC = bytes[dOfs + packetLengthInt - 2] | (bytes[dOfs + packetLengthInt - 1] << 8);

            if (crc != packetCRC)
            {
                qDebug("LoRaDemodDecoderLoRa::decodeBytes: packet CRC error: calc: %x found: %x", crc, packetCRC);
                payloadCRCStatus = false;

                if (errorCheck) {
                    return;
                }
            }
            else
            {
                payloadCRCStatus = true;
            }

        }

        hasCRC = hasCRCInt;
        nbParityBits = nbParityBitsInt;
        packetLength = packetLengthInt;
    }
    else
    {
        dOfs = 0;

        if (hasCRCInt)
        {
            uint16_t crc = sx1272DataChecksum(bytes.data(), packetLengthInt - 2);
            uint16_t packetCRC = bytes[packetLengthInt - 2] | (bytes[packetLengthInt - 1] << 8);

            if (crc != packetCRC)
            {
                qDebug("LoRaDemodDecoderLoRa::decodeBytes: packet CRC error: calc: %x found: %x", crc, packetCRC);
                payloadCRCStatus = false;

                if (errorCheck) {
                    return;
                }
            }
            else
            {
                payloadCRCStatus = true;
            }

        }
    }

    qDebug("LoRaDemodDecoderLoRa::decodeBytes: dataLength: %u packetLengthInt: %u", dataLength, packetLengthInt);
    inBytes.resize(dataLength);
    std::copy(bytes.data() + dOfs, bytes.data() + dOfs + dataLength, inBytes.data());
}
