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

#include <algorithm>

#include "meshtasticdemodsettings.h"
#include "meshtasticdemoddecoderlora.h"

void MeshtasticDemodDecoderLoRa::decodeHeader(
        const std::vector<unsigned short>& inSymbols,
        unsigned int headerNbSymbolBits,
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

    std::vector<uint8_t> codewords(headerNbSymbolBits);

    // Header symbols de-interleave thus headerSymbols (8) symbols into nbSymbolBits (5..12) codewords using header FEC (4/8)
    diagonalDeinterleaveSx(symbols.data(), headerSymbols, codewords.data(), headerNbSymbolBits, headerParityBits);

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
        headerParityStatus = (int) MeshtasticDemodSettings::ParityError;
    }
    else
    {
        if (error) {
            headerParityStatus = (int) MeshtasticDemodSettings::ParityCorrected;
        } else {
            headerParityStatus = (int) MeshtasticDemodSettings::ParityOK;
        }

        if (((bytes[2] & 0x1F) != 0) || (bytes[0] == 0)) {
            headerCRCStatus = false;
        } else {
            headerCRCStatus = true;
        }
    }

    hasCRC = (bytes[1] & 1) != 0;
    nbParityBits = (bytes[1] >> 1) & 0x7;
    packetLength = bytes[0];
}

void MeshtasticDemodDecoderLoRa::decodeBytes(
        QByteArray& inBytes,
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
)
{
    payloadCRCStatus = false;

    // need at least a header (8 symbols of 8 bit codewords) whether an actual header is sent or not
    if (inSymbols.size() < headerSymbols)
    {
        qDebug("MeshtasticDemodDecoderLoRa::decodeBytes: need at least %u symbols for header", headerSymbols);
        earlyEOM = true;
        return;
    }
    else
    {
        earlyEOM = false;
    }

    if (hasHeader)
    {
        if (headerNbSymbolBits < headerCodewords)
        {
            qDebug("MeshtasticDemodDecoderLoRa::decodeBytes: invalid header symbol bits: %u", headerNbSymbolBits);
            earlyEOM = true;
            headerCRCStatus = false;
            return;
        }

        decodeHeader(
            inSymbols,
            headerNbSymbolBits,
            hasCRC,
            nbParityBits,
            packetLength,
            headerParityStatus,
            headerCRCStatus
        );

        // Match gr-lora_sdr behavior: on explicit-header checksum failure,
        // do not continue payload decoding for this frame attempt.
        if (!headerCRCStatus)
        {
            earlyEOM = true;
            return;
        }
    }

    qDebug("MeshtasticDemodDecoderLoRa::decodeBytes: crc: %s nbParityBits: %u packetLength: %u payloadSFbits: %u headerSFbits: %u",
        hasCRC ? "on": "off", nbParityBits, packetLength, payloadNbSymbolBits, headerNbSymbolBits);

    if (nbParityBits > 4)
    {
        qDebug("MeshtasticDemodDecoderLoRa::decodeBytes: invalid parity bits in header: %u", nbParityBits);
        earlyEOM = true;
        headerCRCStatus = false;
        return;
    }

    const unsigned int payloadBlockSymbols = 4 + nbParityBits;
    unsigned int numSymbols = 0;
    unsigned int numCodewords = 0;

    if (hasHeader)
    {
        const unsigned int payloadSymbols = inSymbols.size() > headerSymbols
            ? static_cast<unsigned int>(inSymbols.size() - headerSymbols)
            : 0U;
        const unsigned int payloadBlocks = payloadSymbols / payloadBlockSymbols;

        numSymbols = headerSymbols + payloadBlocks * payloadBlockSymbols;
        numCodewords = headerNbSymbolBits + payloadBlocks * payloadNbSymbolBits;
    }
    else
    {
        const unsigned int payloadBlocks = static_cast<unsigned int>(inSymbols.size()) / payloadBlockSymbols;
        numSymbols = payloadBlocks * payloadBlockSymbols;
        numCodewords = payloadBlocks * payloadNbSymbolBits;
    }

    if (numSymbols < headerSymbols)
    {
        earlyEOM = true;
        return;
    }

    std::vector<uint16_t> symbols(numSymbols);
    std::copy_n(inSymbols.begin(), numSymbols, symbols.begin());

    //gray encode, when SF > PPM, depad the LSBs with rounding
    for (auto &sym : symbols) {
        sym = binaryToGray16(sym);
    }

    std::vector<uint8_t> codewords(numCodewords);

    // deinterleave / dewhiten the symbols into codewords
    unsigned int sOfs = 0;
    unsigned int cOfs = 0;

    // The first 8 LoRa symbols are always protected with 4/8 FEC.
    // In explicit-header mode this first block is interleaved over SF-2 bits
    // (header + first payload nibbles), while the remaining payload uses the
    // configured payload symbol width.
    if (hasHeader)
    {
        diagonalDeinterleaveSx(symbols.data(), headerSymbols, codewords.data(), headerNbSymbolBits, headerParityBits);

        cOfs = headerNbSymbolBits;
        sOfs = headerSymbols;

        if (numSymbols > sOfs)
        {
            diagonalDeinterleaveSx(symbols.data() + sOfs, numSymbols - sOfs, codewords.data() + cOfs, payloadNbSymbolBits, nbParityBits);
        }
    }
    else
    {
        diagonalDeinterleaveSx(symbols.data(), numSymbols, codewords.data(), payloadNbSymbolBits, nbParityBits);
    }

    // Now we have nbSymbolBits 8-bit codewords (4/8 FEC) possibly containing the actual header followed by the rest of payload codewords with their own FEC (4/5..4/8)

    std::vector<uint8_t> bytes((codewords.size()+1) / 2);
    unsigned int dOfs = 0;
    cOfs = 0;

    // Payload byte count plus optional outer CRC bytes; include header bytes
    // only for explicit-header mode.
    unsigned int dataLength = packetLength + (hasCRC ? 2 : 0);
    if (hasHeader) {
        dataLength += 3;
    }

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
        qDebug("MeshtasticDemodDecoderLoRa::decodeBytes: not enough data %lu vs %u", bytes.size(), dataLength);
        earlyEOM = true;
        return;
    }

    // decode the rest of the payload inside 8-bit codewords header with 4/8 FEC
    bool error = false;
    bool bad = false;

    const unsigned int firstBlockCodewords = hasHeader ? headerNbSymbolBits : payloadNbSymbolBits;

    for (; cOfs < firstBlockCodewords; cOfs++, dOfs++)
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

    // LoRa payload dewhitening is applied after FEC decode and excludes the CRC bytes.
    const unsigned int payloadByteOfs = hasHeader ? 3U : 0U;
    if (packetLength > 0U && (payloadByteOfs + packetLength) <= bytes.size()) {
        dewhitenPayloadBytes(bytes.data() + payloadByteOfs, packetLength);
    }

    if (bad) {
        payloadParityStatus = (int) MeshtasticDemodSettings::ParityError;
    } else if (error) {
        payloadParityStatus = (int) MeshtasticDemodSettings::ParityCorrected;
    } else {
        payloadParityStatus = (int) MeshtasticDemodSettings::ParityOK;
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
            if ((packetLength >= 2U) && ((dOfs + packetLength + 2U) <= bytes.size()))
            {
                // Match gr-lora_sdr crc_verif:
                //   crc16(first pay_len-2 bytes) XOR last 2 bytes inside payload
                //   compare against trailing CRC bytes.
                uint16_t crc = crc16gr(bytes.data() + dOfs, packetLength - 2U);
                crc = static_cast<uint16_t>(crc ^ static_cast<uint8_t>(bytes[dOfs + packetLength - 1U]));
                crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(static_cast<uint8_t>(bytes[dOfs + packetLength - 2U])) << 8));
                const uint16_t packetCRC = static_cast<uint16_t>(static_cast<uint8_t>(bytes[dOfs + packetLength]))
                    | (static_cast<uint16_t>(static_cast<uint8_t>(bytes[dOfs + packetLength + 1U])) << 8);

                payloadCRCStatus = (crc == packetCRC);
            }
            else
            {
                payloadCRCStatus = false;
            }
        }
        else
        {
            payloadCRCStatus = true;
        }
    }
    else
    {
        dOfs = 0;  // no header to skip

        if (hasCRC)
        {
            if ((packetLength >= 2U) && ((packetLength + 2U) <= bytes.size()))
            {
                uint16_t crc = crc16gr(bytes.data(), packetLength - 2U);
                crc = static_cast<uint16_t>(crc ^ static_cast<uint8_t>(bytes[packetLength - 1U]));
                crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(static_cast<uint8_t>(bytes[packetLength - 2U])) << 8));
                const uint16_t packetCRC = static_cast<uint16_t>(static_cast<uint8_t>(bytes[packetLength]))
                    | (static_cast<uint16_t>(static_cast<uint8_t>(bytes[packetLength + 1U])) << 8);
                payloadCRCStatus = (crc == packetCRC);
            }
            else
            {
                payloadCRCStatus = false;
            }
        }
        else
        {
            payloadCRCStatus = true;
        }
    }

    inBytes.resize(packetLength);
    std::copy(bytes.data() + dOfs, bytes.data() + dOfs + packetLength, inBytes.data());
}

void MeshtasticDemodDecoderLoRa::decodeBytesSoft(
        QByteArray& inBytes,
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
)
{
    payloadCRCStatus = false;
    payloadParityStatus = (int) MeshtasticDemodSettings::ParityUndefined;

    if (inSymbols.size() < headerSymbols)
    {
        earlyEOM = true;
        return;
    }
    else
    {
        earlyEOM = false;
    }

    if (hasHeader)
    {
        if (headerNbSymbolBits < headerCodewords)
        {
            earlyEOM = true;
            headerCRCStatus = false;
            return;
        }

        decodeHeader(
            inSymbols,
            headerNbSymbolBits,
            hasCRC,
            nbParityBits,
            packetLength,
            headerParityStatus,
            headerCRCStatus
        );

        if (!headerCRCStatus)
        {
            earlyEOM = true;
            return;
        }
    }

    if ((nbParityBits < 1U) || (nbParityBits > 4U))
    {
        earlyEOM = true;
        headerCRCStatus = false;
        return;
    }

    const unsigned int payloadBlockSymbols = 4U + nbParityBits;
    unsigned int numSymbols = 0U;

    if (hasHeader)
    {
        const unsigned int payloadSymbols = inSymbols.size() > headerSymbols
            ? static_cast<unsigned int>(inSymbols.size() - headerSymbols)
            : 0U;
        const unsigned int payloadBlocks = payloadSymbols / payloadBlockSymbols;
        numSymbols = headerSymbols + payloadBlocks * payloadBlockSymbols;
    }
    else
    {
        const unsigned int payloadBlocks = static_cast<unsigned int>(inSymbols.size()) / payloadBlockSymbols;
        numSymbols = payloadBlocks * payloadBlockSymbols;
    }

    if ((numSymbols < headerSymbols) || (inMagnitudes.size() < numSymbols))
    {
        earlyEOM = true;
        return;
    }

    const unsigned int N = 1U << spreadFactor;
    const bool ldro = ((1U << spreadFactor) * 1000.0 / static_cast<double>(std::max(1U, bandwidth))) > 16.0;
    std::vector<std::vector<float>> llrs(numSymbols, std::vector<float>(spreadFactor, 0.0f));

    for (unsigned int symIdx = 0; symIdx < numSymbols; symIdx++)
    {
        const std::vector<float>& mags = inMagnitudes[symIdx];

        if (mags.size() < N)
        {
            earlyEOM = true;
            return;
        }

        const bool isHeaderSym = hasHeader && (symIdx < headerSymbols);
        const bool ldroSym = (!isHeaderSym) && ldro;
        const unsigned int symbolDiv = (isHeaderSym || ldroSym) ? 4U : 1U;

        for (unsigned int bit = 0; bit < spreadFactor; bit++)
        {
            float maxX1 = std::numeric_limits<float>::lowest();
            float maxX0 = std::numeric_limits<float>::lowest();

            for (unsigned int n = 0; n < N; n++)
            {
                unsigned int s = static_cast<unsigned int>(modInt(static_cast<int>(n) - 1, static_cast<int>(N)));
                s /= symbolDiv;
                s = s ^ (s >> 1U);
                const float v = mags[n];

                if ((s & (1U << bit)) != 0U) {
                    maxX1 = std::max(maxX1, v);
                } else {
                    maxX0 = std::max(maxX0, v);
                }
            }

            if (!std::isfinite(maxX1)) { maxX1 = 0.0f; }
            if (!std::isfinite(maxX0)) { maxX0 = 0.0f; }
            llrs[symIdx][spreadFactor - 1U - bit] = maxX1 - maxX0;
        }
    }

    auto decodeSoftBlock = [&llrs, spreadFactor](unsigned int symOfs, unsigned int cwLen, unsigned int sfApp, unsigned int crApp, std::vector<uint8_t>& nibbles) {
        if (sfApp == 0U) {
            return false;
        }

        std::vector<std::vector<float>> interBin(cwLen, std::vector<float>(sfApp, 0.0f));
        std::vector<std::vector<float>> deinterBin(sfApp, std::vector<float>(cwLen, 0.0f));

        for (unsigned int i = 0; i < cwLen; i++)
        {
            const std::vector<float>& symLlr = llrs[symOfs + i];
            const unsigned int start = (spreadFactor > sfApp) ? (spreadFactor - sfApp) : 0U;

            for (unsigned int j = 0; j < sfApp; j++) {
                interBin[i][j] = symLlr[start + j];
            }
        }

        for (unsigned int i = 0; i < cwLen; i++)
        {
            for (unsigned int j = 0; j < sfApp; j++)
            {
                const unsigned int row = static_cast<unsigned int>(modInt(static_cast<int>(i) - static_cast<int>(j) - 1, static_cast<int>(sfApp)));
                deinterBin[row][i] = interBin[i][j];
            }
        }

        for (unsigned int row = 0; row < sfApp; row++) {
            nibbles.push_back(decodeCodewordSoft(deinterBin[row], crApp));
        }

        return true;
    };

    std::vector<uint8_t> nibbles;
    nibbles.reserve((packetLength + (hasCRC ? 2U : 0U) * 2U) + 16U);
    unsigned int symOfs = 0U;

    if (hasHeader)
    {
        if (symOfs + headerSymbols > numSymbols) {
            earlyEOM = true;
            return;
        }

        if (!decodeSoftBlock(symOfs, 8U, headerNbSymbolBits, 4U, nibbles)) {
            earlyEOM = true;
            return;
        }

        symOfs += headerSymbols;
    }

    while (symOfs + payloadBlockSymbols <= numSymbols)
    {
        if (!decodeSoftBlock(symOfs, payloadBlockSymbols, payloadNbSymbolBits, nbParityBits, nibbles)) {
            earlyEOM = true;
            return;
        }

        symOfs += payloadBlockSymbols;
    }

    const unsigned int dataByteLen = packetLength + (hasCRC ? 2U : 0U);
    const unsigned int nibbleOfs = hasHeader ? 5U : 0U;
    const unsigned int neededNibbles = nibbleOfs + dataByteLen * 2U;

    if (nibbles.size() < neededNibbles)
    {
        earlyEOM = true;
        return;
    }

    std::vector<uint8_t> bytes(dataByteLen, 0U);

    for (unsigned int i = 0; i < dataByteLen; i++)
    {
        const uint8_t low = nibbles[nibbleOfs + i * 2U] & 0x0FU;
        const uint8_t high = nibbles[nibbleOfs + i * 2U + 1U] & 0x0FU;
        bytes[i] = static_cast<uint8_t>(low | (high << 4));
    }

    if (packetLength > 0U) {
        dewhitenPayloadBytes(bytes.data(), packetLength);
    }

    if (hasCRC)
    {
        if ((packetLength >= 2U) && (dataByteLen >= packetLength + 2U))
        {
            uint16_t crc = crc16gr(bytes.data(), packetLength - 2U);
            crc = static_cast<uint16_t>(crc ^ bytes[packetLength - 1U]);
            crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(bytes[packetLength - 2U]) << 8));
            const uint16_t packetCRC = static_cast<uint16_t>(bytes[packetLength])
                | (static_cast<uint16_t>(bytes[packetLength + 1U]) << 8);
            payloadCRCStatus = (crc == packetCRC);
        }
        else
        {
            payloadCRCStatus = false;
        }
    }
    else
    {
        payloadCRCStatus = true;
    }

    inBytes.resize(packetLength);
    std::copy(bytes.begin(), bytes.begin() + packetLength, inBytes.begin());
}

void MeshtasticDemodDecoderLoRa::getCodingMetrics(
    unsigned int payloadNbSymbolBits,
    unsigned int headerNbSymbolBits,
    unsigned int nbParityBits,
    unsigned int packetLength,
    bool hasHeader,
    bool hasCRC,
    unsigned int& numSymbols,
    unsigned int& numCodewords
)
{
    if (hasHeader)
    {
        const unsigned int payloadNibbles = (packetLength + (hasCRC ? 2 : 0)) * 2;
        const unsigned int firstPayloadNibbles = headerNbSymbolBits > headerCodewords ? (headerNbSymbolBits - headerCodewords) : 0;
        const unsigned int remainingPayloadNibbles = payloadNibbles > firstPayloadNibbles ? (payloadNibbles - firstPayloadNibbles) : 0;
        const unsigned int payloadBlocks = remainingPayloadNibbles > 0 ? roundUp(remainingPayloadNibbles, payloadNbSymbolBits) / payloadNbSymbolBits : 0;

        numCodewords = headerNbSymbolBits + payloadBlocks * payloadNbSymbolBits;
        numSymbols = headerSymbols + payloadBlocks * (4 + nbParityBits);
    }
    else
    {
        numCodewords = roundUp((packetLength + (hasCRC ? 2 : 0)) * 2, payloadNbSymbolBits);
        numSymbols = headerSymbols + (numCodewords / payloadNbSymbolBits - 1) * (4 + nbParityBits);
    }
}
