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

#include <QTime>

#include "chirpchatdemoddecoder.h"
#include "chirpchatdemoddecodertty.h"
#include "chirpchatdemoddecoderascii.h"
#include "chirpchatdemoddecoderlora.h"
#include "chirpchatdemodmsg.h"

ChirpChatDemodDecoder::ChirpChatDemodDecoder() :
    m_codingScheme(ChirpChatDemodSettings::CodingTTY),
    m_nbSymbolBits(5),
    m_nbParityBits(1),
    m_hasCRC(true),
    m_hasHeader(true),
    m_outputMessageQueue(nullptr)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

ChirpChatDemodDecoder::~ChirpChatDemodDecoder()
{}

void ChirpChatDemodDecoder::setNbSymbolBits(unsigned int spreadFactor, unsigned int deBits)
{
    m_spreadFactor = spreadFactor;

    if (deBits >= spreadFactor) {
        m_deBits = m_spreadFactor - 1;
    } else {
        m_deBits = deBits;
    }

    m_nbSymbolBits = m_spreadFactor - m_deBits;
}

void ChirpChatDemodDecoder::decodeSymbols(const std::vector<unsigned short>& symbols, QString& str)
{
    switch(m_codingScheme)
    {
    case ChirpChatDemodSettings::CodingTTY:
        if (m_nbSymbolBits == 5) {
            ChirpChatDemodDecoderTTY::decodeSymbols(symbols, str);
        }
        break;
    case ChirpChatDemodSettings::CodingASCII:
        if (m_nbSymbolBits == 5) {
            ChirpChatDemodDecoderASCII::decodeSymbols(symbols, str);
        }
        break;
    default:
        break;
    }
}

void ChirpChatDemodDecoder::decodeSymbols(const std::vector<unsigned short>& symbols, QByteArray& bytes)
{
    switch(m_codingScheme)
    {
    case ChirpChatDemodSettings::CodingLoRa:
        if (m_nbSymbolBits >= 5)
        {
            ChirpChatDemodDecoderLoRa::decodeBytes(
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
            ChirpChatDemodDecoderLoRa::getCodingMetrics(
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
    default:
        break;
    }
}

bool ChirpChatDemodDecoder::handleMessage(const Message& cmd)
{
    if (ChirpChatDemodMsg::MsgDecodeSymbols::match(cmd))
    {
        qDebug("ChirpChatDemodDecoder::handleMessage: MsgDecodeSymbols");
        ChirpChatDemodMsg::MsgDecodeSymbols& msg = (ChirpChatDemodMsg::MsgDecodeSymbols&) cmd;
        float msgSignalDb = msg.getSingalDb();
        float msgNoiseDb = msg.getNoiseDb();
        unsigned int msgSyncWord = msg.getSyncWord();
        QDateTime dt = QDateTime::currentDateTime();
        QString msgTimestamp = dt.toString(Qt::ISODateWithMs);

        if (m_codingScheme == ChirpChatDemodSettings::CodingLoRa)
        {
            QByteArray msgBytes;
            decodeSymbols(msg.getSymbols(), msgBytes);

            if (m_outputMessageQueue)
            {
                ChirpChatDemodMsg::MsgReportDecodeBytes *outputMsg = ChirpChatDemodMsg::MsgReportDecodeBytes::create(msgBytes);
                outputMsg->setSyncWord(msgSyncWord);
                outputMsg->setSignalDb(msgSignalDb);
                outputMsg->setNoiseDb(msgNoiseDb);
                outputMsg->setMsgTimestamp(msgTimestamp);
                outputMsg->setPacketSize(getPacketLength());
                outputMsg->setNbParityBits(getNbParityBits());
                outputMsg->setHasCRC(getHasCRC());
                outputMsg->setNbSymbols(getNbSymbols());
                outputMsg->setNbCodewords(getNbCodewords());
                outputMsg->setEarlyEOM(getEarlyEOM());
                outputMsg->setHeaderParityStatus(getHeaderParityStatus());
                outputMsg->setHeaderCRCStatus(getHeaderCRCStatus());
                outputMsg->setPayloadParityStatus(getPayloadParityStatus());
                outputMsg->setPayloadCRCStatus(getPayloadCRCStatus());
                m_outputMessageQueue->push(outputMsg);
            }
        }
        else if (m_codingScheme == ChirpChatDemodSettings::CodingFT)
        {

        }
        else
        {
            QString msgString;
            decodeSymbols(msg.getSymbols(), msgString);

            if (m_outputMessageQueue)
            {
                ChirpChatDemodMsg::MsgReportDecodeString *outputMsg = ChirpChatDemodMsg::MsgReportDecodeString::create(msgString);
                outputMsg->setSyncWord(msgSyncWord);
                outputMsg->setSignalDb(msgSignalDb);
                outputMsg->setNoiseDb(msgNoiseDb);
                outputMsg->setMsgTimestamp(msgTimestamp);
                m_outputMessageQueue->push(outputMsg);
            }
        }

        return true;
    }

    return false;
}

void ChirpChatDemodDecoder::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}
