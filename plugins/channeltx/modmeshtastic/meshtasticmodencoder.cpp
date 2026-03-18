///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <QDebug>

#include "meshtasticmodencoder.h"
#include "meshtasticmodencoderlora.h"
#include "meshtasticpacket.h"

const MeshtasticModSettings::CodingScheme MeshtasticModEncoder::m_codingScheme = MeshtasticModSettings::CodingLoRa;
const bool MeshtasticModEncoder::m_hasCRC = true;
const bool MeshtasticModEncoder::m_hasHeader = true;

MeshtasticModEncoder::MeshtasticModEncoder() :
    m_nbSymbolBits(5),
    m_nbParityBits(1)
{}

MeshtasticModEncoder::~MeshtasticModEncoder()
{}

void MeshtasticModEncoder::setNbSymbolBits(unsigned int spreadFactor, unsigned int deBits)
{
    m_spreadFactor = spreadFactor;

    if (deBits >= spreadFactor) {
        m_deBits = m_spreadFactor - 1;
    } else {
        m_deBits = deBits;
    }

    m_nbSymbolBits = m_spreadFactor - m_deBits;
}

void MeshtasticModEncoder::encode(MeshtasticModSettings settings, std::vector<unsigned short>& symbols)
{
    if (m_nbSymbolBits >= 5)
    {
        QByteArray bytes;
        QString summary;
        QString error;

        if (modemmeshtastic::Packet::isCommand(settings.m_textMessage))
        {
            if (!modemmeshtastic::Packet::buildFrameFromCommand(settings.m_textMessage, bytes, summary, error))
            {
                qWarning() << "MeshtasticModEncoder::encode: Meshtastic command error:" << error;
                return;
            }

            qInfo() << "MeshtasticModEncoder::encode:" << summary;
        }
        else
        {
            bytes = settings.m_textMessage.toUtf8();
        }

        encodeBytesLoRa(bytes, symbols);
    }
}

void MeshtasticModEncoder::encodeBytes(const QByteArray& bytes, std::vector<unsigned short>& symbols)
{
    switch (m_codingScheme)
    {
    case MeshtasticModSettings::CodingLoRa:
    {
        QByteArray payload(bytes);

        if (modemmeshtastic::Packet::isCommand(QString::fromUtf8(bytes)))
        {
            QString summary;
            QString error;

            if (!modemmeshtastic::Packet::buildFrameFromCommand(QString::fromUtf8(bytes), payload, summary, error))
            {
                qWarning() << "MeshtasticModEncoder::encodeBytes: Meshtastic command error:" << error;
                return;
            }

            qInfo() << "MeshtasticModEncoder::encodeBytes:" << summary;
        }

        encodeBytesLoRa(payload, symbols);
        break;
    }
    default:
        break;
    };
}

void MeshtasticModEncoder::encodeBytesLoRa(const QByteArray& bytes, std::vector<unsigned short>& symbols)
{
    QByteArray payload(bytes);

    if ((payload.size() + (m_hasCRC ? 2 : 0)) > 255)
    {
        qWarning() << "MeshtasticModEncoder::encodeBytesLoRa: payload too large:" << payload.size();
        return;
    }

    if (m_hasCRC) {
        MeshtasticModEncoderLoRa::addChecksum(payload);
    }

    MeshtasticModEncoderLoRa::encodeBytes(payload, symbols, m_nbSymbolBits, m_hasHeader, m_hasCRC, m_nbParityBits);
}
