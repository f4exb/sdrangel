///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_CHIRPCHATDEMODDECODER_H
#define INCLUDE_CHIRPCHATDEMODDECODER_H

#include <vector>
#include "chirpchatdemodsettings.h"

class ChirpChatDemodDecoder
{
public:
    ChirpChatDemodDecoder();
    ~ChirpChatDemodDecoder();

    void setCodingScheme(ChirpChatDemodSettings::CodingScheme codingScheme) { m_codingScheme = codingScheme; }
    void setNbSymbolBits(unsigned int spreadFactor, unsigned int deBits);
    void setLoRaParityBits(unsigned int parityBits) { m_nbParityBits = parityBits; }
    void setLoRaHasHeader(bool hasHeader) { m_hasHeader = hasHeader; }
    void setLoRaHasCRC(bool hasCRC) { m_hasCRC = hasCRC; }
    void setLoRaPacketLength(unsigned int packetLength) { m_packetLength = packetLength; }
    void decodeSymbols(const std::vector<unsigned short>& symbols, QString& str);      //!< For ASCII and TTY
    void decodeSymbols(const std::vector<unsigned short>& symbols, QByteArray& bytes); //!< For raw bytes (original LoRa)
    unsigned int getNbParityBits() const { return m_nbParityBits; }
    unsigned int getPacketLength() const { return m_packetLength; }
    bool getHasCRC() const { return m_hasCRC; }
    unsigned int getNbSymbols() const { return m_nbSymbols; }
    unsigned int getNbCodewords() const { return m_nbCodewords; }
    bool getEarlyEOM() const { return m_earlyEOM; }
    int getHeaderParityStatus() const { return m_headerParityStatus; }
    bool getHeaderCRCStatus() const { return m_headerCRCStatus; }
    int getPayloadParityStatus() const { return m_payloadParityStatus; }
    bool getPayloadCRCStatus() const { return m_payloadCRCStatus; }

private:
    ChirpChatDemodSettings::CodingScheme m_codingScheme;
    unsigned int m_spreadFactor;
    unsigned int m_deBits;
    unsigned int m_nbSymbolBits;
    // LoRa attributes
    unsigned int m_nbParityBits; //!< 1 to 4 Hamming FEC bits for 4 payload bits
    bool m_hasCRC;
    bool m_hasHeader;
    unsigned int m_packetLength;
    unsigned int m_nbSymbols;    //!< Number of encoded symbols: this is only dependent of nbSymbolBits, nbParityBits, packetLength, hasHeader and hasCRC
    unsigned int m_nbCodewords;  //!< Number of encoded codewords: this is only dependent of nbSymbolBits, nbParityBits, packetLength, hasHeader and hasCRC
    bool m_earlyEOM;
    int m_headerParityStatus;
    bool m_headerCRCStatus;
    int m_payloadParityStatus;
    bool m_payloadCRCStatus;
};

#endif // INCLUDE_CHIRPCHATDEMODDECODER_H
