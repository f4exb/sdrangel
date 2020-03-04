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

#ifndef PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODER_H_
#define PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODER_H_

#include <vector>
#include "chirpchatmodsettings.h"

class ChirpChatModEncoder
{
public:
    ChirpChatModEncoder();
    ~ChirpChatModEncoder();

    void setCodingScheme(ChirpChatModSettings::CodingScheme codingScheme) { m_codingScheme = codingScheme; }
    void setNbSymbolBits(unsigned int spreadFactor, unsigned int deBits);
    void setLoRaParityBits(unsigned int parityBits) { m_nbParityBits = parityBits; }
    void setLoRaHasHeader(bool hasHeader) { m_hasHeader = hasHeader; }
    void setLoRaHasCRC(bool hasCRC) { m_hasCRC = hasCRC; }
    void encodeString(const QString& str, std::vector<unsigned short>& symbols);
    void encodeBytes(const QByteArray& bytes, std::vector<unsigned short>& symbols);

private:
    // LoRa functions
    void encodeBytesLoRa(const QByteArray& bytes, std::vector<unsigned short>& symbols);

    // General attributes
    ChirpChatModSettings::CodingScheme m_codingScheme;
    unsigned int m_spreadFactor;
    unsigned int m_deBits;
    unsigned int m_nbSymbolBits;
    // LoRa attributes
    unsigned int m_nbParityBits; //!< 1 to 4 Hamming FEC bits for 4 payload bits
    bool m_hasCRC;
    bool m_hasHeader;
};

#endif // PLUGINS_CHANNELTX_MODCHIRPCHAT_CHIRPCHATMODENCODER_H_

