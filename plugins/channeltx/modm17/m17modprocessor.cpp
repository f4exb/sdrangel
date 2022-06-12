///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include "m17/M17Modulator.h"

#include "m17modprocessor.h"

M17ModProcessor::M17ModProcessor()
{
    m_basebandFifo.setSampleSize(sizeof(int16_t), 48000);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

M17ModProcessor::~M17ModProcessor()
{}

bool M17ModProcessor::handleMessage(const Message& cmd)
{
    if (MsgSendSMS::match(cmd))
    {
        const MsgSendSMS& notif = (const MsgSendSMS&) cmd;
        QByteArray packetBytes = notif.getSMSText().toUtf8();
        packetBytes.prepend(0x05); // SMS standard type
        packetBytes.truncate(798); // Maximum packet size is 798 payload + 2 bytes CRC = 800 bytes (32*25)
        processPacket(notif.getSourceCall(), notif.getDestCall(), packetBytes);
        return true;
    }

    return false;
}


void M17ModProcessor::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

void M17ModProcessor::processPacket(const QString& sourceCall, const QString& destCall, const QByteArray& packetBytes)
{
    mobilinkd::M17Modulator modulator(sourceCall.toStdString(), destCall.toStdString());
    // preamble
    std::array<uint8_t, 48> preamble_bytes;
    mobilinkd::M17Modulator::make_preamble(preamble_bytes);
    std::array<int8_t, 48 * 4> fullframe_symbols = mobilinkd::M17Modulator::bytes_to_symbols(preamble_bytes);
    std::array<int16_t, 1920> baseband = mobilinkd::M17Modulator::symbols_to_baseband(fullframe_symbols);
    m_basebandFifo.write((const quint8*) baseband.data(), 1920);
    // LSF
    mobilinkd::M17Modulator::lich_t lichSegments; // Not used for packet
    mobilinkd::M17Modulator::frame_t frame;
    modulator.make_link_setup(lichSegments, frame);
    std::array<int8_t, 46 * 4> frame_symbols = mobilinkd::M17Modulator::bytes_to_symbols(frame);
    std::copy(mobilinkd::M17Modulator::LSF_SYNC_WORD.begin(), mobilinkd::M17Modulator::LSF_SYNC_WORD.end(), fullframe_symbols.begin());
    std::copy(frame_symbols.begin(), frame_symbols.end(), fullframe_symbols.begin()+2);
    baseband = mobilinkd::M17Modulator::symbols_to_baseband(fullframe_symbols);
    m_basebandFifo.write((const quint8*) baseband.data(), 1920);
    // Packets
    std::copy(mobilinkd::M17Modulator::DATA_SYNC_WORD.begin(), mobilinkd::M17Modulator::DATA_SYNC_WORD.end(), fullframe_symbols.begin());
    mobilinkd::M17Modulator::packet_t packet;
    int remainderCount = packetBytes.size();
    int packetCount = 0;

    while (remainderCount > 25)
    {
        std::copy(packetBytes.begin() + (packetCount*25), packetBytes.begin() + ((packetCount+1)*25), packet.begin());
        frame = modulator.make_packet_frame(packetCount, false, packet, 25);
        std::copy(frame_symbols.begin(), frame_symbols.end(), fullframe_symbols.begin()+2);
        baseband = mobilinkd::M17Modulator::symbols_to_baseband(fullframe_symbols);
        m_basebandFifo.write((const quint8*) baseband.data(), 1920);
        remainderCount -= 25;
        packetCount++;
    }

    std::copy(packetBytes.begin() + (packetCount*25), packetBytes.begin() + (packetCount*25) + remainderCount, packet.begin());
    frame = modulator.make_packet_frame(packetCount, true, packet, remainderCount);
    std::copy(frame_symbols.begin(), frame_symbols.end(), fullframe_symbols.begin()+2);
    baseband = mobilinkd::M17Modulator::symbols_to_baseband(fullframe_symbols);
    m_basebandFifo.write((const quint8*) baseband.data(), 1920);
}
