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

#include <codec2/codec2.h>

#include "m17/M17Modulator.h"

#include "m17modprocessor.h"

MESSAGE_CLASS_DEFINITION(M17ModProcessor::MsgSendSMS, Message)
MESSAGE_CLASS_DEFINITION(M17ModProcessor::MsgSendAudioFrame, Message)
MESSAGE_CLASS_DEFINITION(M17ModProcessor::MsgStartAudio, Message)
MESSAGE_CLASS_DEFINITION(M17ModProcessor::MsgStopAudio, Message)

M17ModProcessor::M17ModProcessor() :
    m_m17Modulator("MYCALL", ""),
    m_lichSegmentIndex(0),
    m_audioFrameIndex(0),
    m_audioFrameNumber(0)
{
    m_basebandFifo.setSize(96000);
    m_basebandFifoLow = 4096;
    m_basebandFifoHigh = 96000 - m_basebandFifoLow;
    m_decimator.initialize(8000.0, 3000.0, 6);
    m_codec2 = ::codec2_create(CODEC2_MODE_3200);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

M17ModProcessor::~M17ModProcessor()
{
    codec2_destroy(m_codec2);
}

bool M17ModProcessor::handleMessage(const Message& cmd)
{
    if (MsgSendSMS::match(cmd))
    {
        const MsgSendSMS& notif = (const MsgSendSMS&) cmd;
        QByteArray packetBytes = notif.getSMSText().toUtf8();
        packetBytes.prepend(0x05); // SMS standard type
        packetBytes.truncate(798); // Maximum packet size is 798 payload + 2 bytes CRC = 800 bytes (32*25)
        processPacket(notif.getSourceCall(), notif.getDestCall(), notif.getCAN(), packetBytes);
        // test(notif.getSourceCall(), notif.getDestCall());
        return true;
    }
    else if (MsgSendAudioFrame::match(cmd))
    {
        MsgSendAudioFrame& notif = (MsgSendAudioFrame&) cmd;
        m_audioFrame = notif.getAudioFrame();
        processAudioFrame();
        return true;
    }
    else if (MsgStartAudio::match(cmd))
    {
        MsgStartAudio& notif = (MsgStartAudio&) cmd;
        qDebug("M17ModProcessor::handleMessage: MsgStartAudio: %s to %s",
            qPrintable(notif.getSourceCall()), qPrintable(notif.getDestCall()));
        audioStart(notif.getSourceCall(), notif.getDestCall(), notif.getCAN());
        return true;
    }
    else if (MsgStopAudio::match(cmd))
    {
        qDebug("M17ModProcessor::handleMessage: MsgStopAudio");
        audioStop();
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

void M17ModProcessor::test(const QString& sourceCall, const QString& destCall)
{
    m_m17Modulator.source(sourceCall.toStdString());
    m_m17Modulator.dest(destCall.toStdString());

    for (int i = 0; i < 25; i++) {
        send_preamble();
    }
}

void M17ModProcessor::processPacket(const QString& sourceCall, const QString& destCall, uint8_t can, const QByteArray& packetBytes)
{
    qDebug("M17ModProcessor::processPacket: %s to %s: %s", qPrintable(sourceCall), qPrintable(destCall), qPrintable(packetBytes));
    m_m17Modulator.source(sourceCall.toStdString());
    m_m17Modulator.dest(destCall.toStdString());

    send_preamble(); // preamble

    // LSF
    std::array<uint8_t, 30> lsf;
    std::array<int8_t, 368> lsf_frame = mobilinkd::M17Modulator::make_lsf(lsf, sourceCall.toStdString(), destCall.toStdString(), can);
    output_baseband(mobilinkd::M17Modulator::LSF_SYNC_WORD, lsf_frame);

    // Packets
    int remainderCount = packetBytes.size();
    int packetCount = 0;
    std::array<int8_t, 368> packet_frame;
    // std::copy(mobilinkd::M17Modulator::DATA_SYNC_WORD.begin(), mobilinkd::M17Modulator::DATA_SYNC_WORD.end(), fullframe_symbols.begin());
    mobilinkd::M17Modulator::packet_t packet;

    while (remainderCount > 25)
    {
        std::copy(packetBytes.begin() + (packetCount*25), packetBytes.begin() + ((packetCount+1)*25), packet.begin());
        packet_frame = m_m17Modulator.make_packet_frame(packetCount, 25, false, packet);
        output_baseband(mobilinkd::M17Modulator::PACKET_SYNC_WORD, packet_frame);
        remainderCount -= 25;
        packetCount++;
    }

    std::copy(packetBytes.begin() + (packetCount*25), packetBytes.begin() + (packetCount*25) + remainderCount, packet.begin());
    packet_frame = m_m17Modulator.make_packet_frame(packetCount, remainderCount, true, packet);
    output_baseband(mobilinkd::M17Modulator::PACKET_SYNC_WORD, packet_frame);
    qDebug("M17ModProcessor::processPacket: last: packetCount: %d remainderCount: %d", packetCount, remainderCount);

    send_eot(); // EOT
}

void M17ModProcessor::audioStart(const QString& sourceCall, const QString& destCall, uint8_t can)
{
    qDebug("M17ModProcessor::audioStart");
    m_m17Modulator.source(sourceCall.toStdString());
    m_m17Modulator.dest(destCall.toStdString());
    m_m17Modulator.can(can);
    m_audioFrameNumber = 0;

    send_preamble(); // preamble

    // LSF
    std::array<uint8_t, 30> lsf;
    std::array<int8_t, 368> lsf_frame = mobilinkd::M17Modulator::make_lsf(lsf, sourceCall.toStdString(), destCall.toStdString(), can, true);
    output_baseband(mobilinkd::M17Modulator::LSF_SYNC_WORD, lsf_frame);

    // Prepare LICH
    for (size_t i = 0; i < m_lich.size(); ++i)
    {
        std::array<uint8_t, 5> segment;
        std::copy(lsf.begin() + i*5, lsf.begin() + (i + 1)*5, segment.begin());
        mobilinkd::M17Modulator::lich_segment_t lich_segment = mobilinkd::M17Modulator::make_lich_segment(segment, i);
        std::copy(lich_segment.begin(), lich_segment.end(), m_lich[i].begin());
    }
}

void M17ModProcessor::audioStop()
{
    qDebug("M17ModProcessor::audioStop");
    if (m_audioFrameIndex > 0) // send remainder audio + null samples
    {
        std::fill(m_audioFrame.begin() + m_audioFrameIndex, m_audioFrame.end(), 0);
        processAudioFrame();
        m_audioFrameIndex = 0;
    }

    send_eot(); // EOT
}

void M17ModProcessor::send_preamble()
{
    // Preamble is simple... bytes -> symbols -> baseband.
    std::array<uint8_t, 48> preamble_bytes;
    preamble_bytes.fill(0x77);
    std::array<int8_t, 192> preamble_symbols = mobilinkd::M17Modulator::bytes_to_symbols(preamble_bytes);
    std::array<int16_t, 1920> preamble_baseband = m_m17Modulator.symbols_to_baseband(preamble_symbols);
    m_basebandFifo.write(preamble_baseband.data(), 1920);
}

void M17ModProcessor::processAudioFrame()
{
    std::array<uint8_t, 16> audioPayload = encodeAudio(m_audioFrame);
    std::array<int8_t, 272> audioDataBits = mobilinkd::M17Modulator::make_stream_data_frame(m_audioFrameNumber++, audioPayload);

    if (m_audioFrameNumber == 0x8000) {
        m_audioFrameNumber = 0;
    }

    std::array<uint8_t, 96>& lich = m_lich[m_lichSegmentIndex++];

    if (m_lichSegmentIndex == 6) {
        m_lichSegmentIndex = 0;
    }

    std::array<int8_t, 368> temp;
    auto it = std::copy(lich.begin(), lich.end(), temp.begin());
    std::copy(audioDataBits.begin(), audioDataBits.end(), it);
    mobilinkd::M17Modulator::interleave_and_randomize(temp);

    output_baseband(mobilinkd::M17Modulator::STREAM_SYNC_WORD, temp);
}

std::array<uint8_t, 16> M17ModProcessor::encodeAudio(std::array<int16_t, 320*6>& audioFrame)
{
    std::array<int16_t, 320> audioFrame8k;
    m_decimator.decimate(audioFrame.data(), audioFrame8k.data(), 320);
    std::array<uint8_t, 16> result;
    codec2_encode(m_codec2, &result[0], const_cast<int16_t*>(&audioFrame8k[0]));
    codec2_encode(m_codec2, &result[8], const_cast<int16_t*>(&audioFrame8k[160]));
    return result;
}

void M17ModProcessor::send_eot()
{
    std::array<uint8_t, 2> EOT_SYNC = { 0x55, 0x5D };
    std::array<uint8_t, 48> eot_bytes;

    for (unsigned int i = 0; i < eot_bytes.size(); i += 2) {
        std::copy(EOT_SYNC.begin(), EOT_SYNC.end(), eot_bytes.begin() + i);
    }

    std::array<int8_t, 192> eot_symbols = mobilinkd::M17Modulator::bytes_to_symbols(eot_bytes);
    std::array<int16_t, 1920> eot_baseband = m_m17Modulator.symbols_to_baseband(eot_symbols);
    m_basebandFifo.write(eot_baseband.data(), 1920);
}

void M17ModProcessor::output_baseband(std::array<uint8_t, 2> sync_word, const std::array<int8_t, 368>& frame)
{
    std::array<int8_t, 184> symbols = mobilinkd::M17Modulator::bits_to_symbols(frame); // 368 bits -> 184 dibit symbols
    std::array<int8_t, 8> sw = mobilinkd::M17Modulator::bytes_to_symbols(sync_word); // 16 bits -> 8 dibit symbols

    std::array<int8_t, 192> temp; // 384 = 368 + 16 bits -> 192 dibit symbols
    auto fit = std::copy(sw.begin(), sw.end(), temp.begin()); // start with sync word dibits
    std::copy(symbols.begin(), symbols.end(), fit); // then the frame dibits
    std::array<int16_t, 1920> baseband = m_m17Modulator.symbols_to_baseband(temp); // 1920 48 kS/s int16_t samples
    m_basebandFifo.write(baseband.data(), 1920);
}
