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
#include "meshtasticmodencodertty.h"
#include "meshtasticmodencoderascii.h"
#include "meshtasticmodencoderlora.h"
#include "meshtasticmodencoderft.h"
#include "meshtasticpacket.h"

MeshtasticModEncoder::MeshtasticModEncoder() :
    m_codingScheme(MeshtasticModSettings::CodingTTY),
    m_nbSymbolBits(5),
    m_nbParityBits(1),
    m_hasCRC(true),
    m_hasHeader(true)
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
    if (settings.m_codingScheme == MeshtasticModSettings::CodingFT)
    {
        MeshtasticModEncoderFT::encodeMsg(
            settings.m_myCall,
            settings.m_urCall,
            settings.m_myLoc,
            settings.m_myRpt,
            settings.m_textMessage,
            settings.m_messageType,
            m_nbSymbolBits,
            symbols
        );
    }
    else
    {
        if (settings.m_messageType == MeshtasticModSettings::MessageBytes) {
            encodeBytes(settings.m_bytesMessage, symbols);
        } else if (settings.m_messageType == MeshtasticModSettings::MessageBeacon) {
            encodeString(settings.m_beaconMessage, symbols);
        } else if (settings.m_messageType == MeshtasticModSettings::MessageCQ) {
            encodeString(settings.m_cqMessage, symbols);
        } else if (settings.m_messageType == MeshtasticModSettings::MessageReply) {
            encodeString(settings.m_replyMessage, symbols);
        } else if (settings.m_messageType == MeshtasticModSettings::MessageReport) {
            encodeString(settings.m_reportMessage, symbols);
        } else if (settings.m_messageType == MeshtasticModSettings::MessageReplyReport) {
            encodeString(settings.m_replyReportMessage, symbols);
        } else if (settings.m_messageType == MeshtasticModSettings::MessageRRR) {
            encodeString(settings.m_rrrMessage, symbols);
        } else if (settings.m_messageType == MeshtasticModSettings::Message73) {
            encodeString(settings.m_73Message, symbols);
        } else if (settings.m_messageType == MeshtasticModSettings::MessageQSOText) {
            encodeString(settings.m_qsoTextMessage, symbols);
        } else if (settings.m_messageType == MeshtasticModSettings::MessageText) {
            encodeString(settings.m_textMessage, symbols);
        }
    }
}

void MeshtasticModEncoder::encodeString(const QString& str, std::vector<unsigned short>& symbols)
{
    switch (m_codingScheme)
    {
    case MeshtasticModSettings::CodingTTY:
        if (m_nbSymbolBits == 5) {
            MeshtasticModEncoderTTY::encodeString(str, symbols);
        }
        break;
    case MeshtasticModSettings::CodingASCII:
        if (m_nbSymbolBits == 7) {
            MeshtasticModEncoderASCII::encodeString(str, symbols);
        }
        break;
    case MeshtasticModSettings::CodingLoRa:
        if (m_nbSymbolBits >= 5)
        {
            QByteArray bytes;
            QString summary;
            QString error;

            if (modemmeshtastic::Packet::isCommand(str))
            {
                if (!modemmeshtastic::Packet::buildFrameFromCommand(str, bytes, summary, error))
                {
                    qWarning() << "MeshtasticModEncoder::encodeString: Meshtastic command error:" << error;
                    return;
                }

                qInfo() << "MeshtasticModEncoder::encodeString:" << summary;
            }
            else
            {
                bytes = str.toUtf8();
            }

            encodeBytesLoRa(bytes, symbols);
        }
        break;
    default:
        break;
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
