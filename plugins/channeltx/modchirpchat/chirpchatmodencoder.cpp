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

#include "chirpchatmodencoder.h"
#include "chirpchatmodencodertty.h"
#include "chirpchatmodencoderascii.h"
#include "chirpchatmodencoderlora.h"

ChirpChatModEncoder::ChirpChatModEncoder() :
    m_codingScheme(ChirpChatModSettings::CodingTTY),
    m_nbSymbolBits(5),
    m_nbParityBits(1),
    m_hasCRC(true),
    m_hasHeader(true)
{}

ChirpChatModEncoder::~ChirpChatModEncoder()
{}

void ChirpChatModEncoder::setNbSymbolBits(unsigned int spreadFactor, unsigned int deBits)
{
    m_spreadFactor = spreadFactor;

    if (deBits >= spreadFactor) {
        m_deBits = m_spreadFactor - 1;
    } else {
        m_deBits = deBits;
    }

    m_nbSymbolBits = m_spreadFactor - m_deBits;
}

void ChirpChatModEncoder::encodeString(const QString& str, std::vector<unsigned short>& symbols)
{
    switch (m_codingScheme)
    {
    case ChirpChatModSettings::CodingTTY:
        if (m_nbSymbolBits == 5) {
            ChirpChatModEncoderTTY::encodeString(str, symbols);
        }
        break;
    case ChirpChatModSettings::CodingASCII:
        if (m_nbSymbolBits == 7) {
            ChirpChatModEncoderASCII::encodeString(str, symbols);
        }
        break;
    case ChirpChatModSettings::CodingLoRa:
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

void ChirpChatModEncoder::encodeBytes(const QByteArray& bytes, std::vector<unsigned short>& symbols)
{
    switch (m_codingScheme)
    {
    case ChirpChatModSettings::CodingLoRa:
        encodeBytesLoRa(bytes, symbols);
        break;
    default:
        break;
    };
}

void ChirpChatModEncoder::encodeBytesLoRa(const QByteArray& bytes, std::vector<unsigned short>& symbols)
{
    QByteArray payload(bytes);

    if (m_hasCRC) {
        ChirpChatModEncoderLoRa::addChecksum(payload);
    }

    ChirpChatModEncoderLoRa::encodeBytes(payload, symbols, m_nbSymbolBits, m_hasHeader, m_hasCRC, m_nbParityBits);
}