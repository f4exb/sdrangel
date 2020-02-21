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

#include "lorademoddecoder.h"
#include "lorademoddecodertty.h"
#include "lorademoddecoderascii.h"
#include "lorademoddecoderlora.h"

LoRaDemodDecoder::LoRaDemodDecoder() :
    m_codingScheme(LoRaDemodSettings::CodingTTY),
    m_nbSymbolBits(5),
    m_nbParityBits(1),
    m_hasCRC(true),
    m_hasHeader(true)
{}

LoRaDemodDecoder::~LoRaDemodDecoder()
{}

void LoRaDemodDecoder::setNbSymbolBits(unsigned int spreadFactor, unsigned int deBits)
{
    m_spreadFactor = spreadFactor;

    if (deBits >= spreadFactor) {
        m_deBits = m_spreadFactor - 1;
    } else {
        m_deBits = deBits;
    }

    m_nbSymbolBits = m_spreadFactor - m_deBits;
}

void LoRaDemodDecoder::decodeSymbols(const std::vector<unsigned short>& symbols, QString& str)
{
    switch(m_codingScheme)
    {
    case LoRaDemodSettings::CodingTTY:
        if (m_nbSymbolBits == 5) {
            LoRaDemodDecoderTTY::decodeSymbols(symbols, str);
        }
        break;
    case LoRaDemodSettings::CodingASCII:
        if (m_nbSymbolBits == 5) {
            LoRaDemodDecoderASCII::decodeSymbols(symbols, str);
        }
        break;
    default:
        break;
    }
}

void LoRaDemodDecoder::decodeSymbols(const std::vector<unsigned short>& symbols, QByteArray& bytes)
{
    switch(m_codingScheme)
    {
    case LoRaDemodSettings::CodingLoRa:
        if (m_nbSymbolBits >= 5)
        {
            LoRaDemodDecoderLoRa::decodeBytes(
                bytes,
                symbols,
                m_nbSymbolBits,
                m_hasHeader,
                m_hasCRC,
                m_nbParityBits,
                m_packetLength,
                m_earlyEOM,
                m_headerParityStatus,
                m_headerCRCStatus,
                m_payloadParityStatus,
                m_payloadCRCStatus
            );
            LoRaDemodDecoderLoRa::getCodingMetrics(
                m_nbSymbolBits,
                m_nbParityBits,
                m_packetLength,
                m_hasHeader,
                m_hasCRC,
                m_nbSymbols,
                m_nbCodewords
            );
        }
        break;
    }
}
