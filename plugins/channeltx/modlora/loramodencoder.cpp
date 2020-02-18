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

#include "loramodencoder.h"
#include "loramodencodertty.h"
#include "loramodencoderascii.h"
#include "loramodencoderlora.h"

LoRaModEncoder::LoRaModEncoder() :
    m_codingScheme(LoRaModSettings::CodingTTY),
    m_nbSymbolBits(5),
    m_nbParityBits(1),
    m_hasCRC(true),
    m_hasHeader(true)
{}

LoRaModEncoder::~LoRaModEncoder()
{}

void LoRaModEncoder::setNbSymbolBits(unsigned int spreadFactor, unsigned int deBits)
{
    m_spreadFactor = spreadFactor;

    if (deBits >= spreadFactor) {
        m_deBits = m_spreadFactor - 1;
    } else {
        m_deBits = deBits;
    }

    m_nbSymbolBits = m_spreadFactor - m_deBits;
}

void LoRaModEncoder::encodeString(const QString& str, std::vector<unsigned short>& symbols)
{
    switch (m_codingScheme)
    {
    case LoRaModSettings::CodingTTY:
        if (m_nbSymbolBits == 5) {
            LoRaModEncoderTTY::encodeString(str, symbols);
        }
        break;
    case LoRaModSettings::CodingASCII:
        if (m_nbSymbolBits == 7) {
            LoRaModEncoderASCII::encodeString(str, symbols);
        }
        break;
    case LoRaModSettings::CodingLoRa:
        if (m_nbSymbolBits >= 5)
        {
            QByteArray bytes = str.toUtf8();
            encodeBytesLoRa(bytes, symbols);
        }
        break;
    default:
        break;
    }
}

void LoRaModEncoder::encodeBytes(const QByteArray& bytes, std::vector<unsigned short>& symbols)
{
    switch (m_codingScheme)
    {
    case LoRaModSettings::CodingLoRa:
        encodeBytesLoRa(bytes, symbols);
        break;
    default:
        break;
    };
}

void LoRaModEncoder::encodeBytesLoRa(const QByteArray& bytes, std::vector<unsigned short>& symbols)
{
    QByteArray payload(bytes);

    if (m_hasCRC) {
        LoRaModEncoderLoRa::addChecksum(payload);
    }

    LoRaModEncoderLoRa::encodeBytes(payload, symbols, m_nbSymbolBits, m_hasHeader, m_hasCRC, m_nbParityBits);
}