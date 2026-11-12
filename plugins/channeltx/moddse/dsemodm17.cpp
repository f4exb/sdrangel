///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <algorithm>
#include <cstring>

#include <QDebug>

#include "dsemodm17.h"

DSEModM17::DSEModM17() :
    m_state(StateIdle),
    m_dataOnly(false),
    m_stopRequested(false),
    m_segment(0),
    m_frameNumber(0),
    m_frameCount(0),
    m_voiceFrameCount(0),
    m_packetOffset(0),
    m_packetFrame(0)
{
    std::memset(m_lsfBytes, 0, sizeof(m_lsfBytes));
}

DSEModM17::~DSEModM17()
{
}

bool DSEModM17::wantsDataOnly(const DSEModSettings& settings)
{
    return (settings.m_modAFInput == DSEModSettings::DSEModInputNone) && !settings.m_m17Message.trimmed().isEmpty();
}

DVM17::LSF DSEModM17::makeLSF(const DSEModSettings& settings, bool stream)
{
    DVM17::LSF lsf;
    lsf.m_source = DVM17::encodeCallsign(settings.m_m17Source.trimmed().toUpper().toStdString());
    QString destination = settings.m_m17Destination.trimmed().toUpper();
    lsf.m_destination = destination.isEmpty() ? DVM17::CallsignBroadcast
        : DVM17::encodeCallsign(destination.toStdString());
    lsf.m_type = 0;
    lsf.setStream(stream);
    lsf.setDataType(stream ? 2 : 1);   // voice, or data
    lsf.setCAN(settings.m_m17CAN);
    return lsf;
}

void DSEModM17::start(const DSEModSettings& settings)
{
    m_settings = settings;
    m_dataOnly = !m_message.empty() || wantsDataOnly(settings);
    m_lsf = makeLSF(settings, !m_dataOnly);
    DVM17::packLSF(m_lsf, m_lsfBytes);
    m_segment = 0;
    m_frameNumber = 0;
    m_frameCount = 0;
    m_voiceFrameCount = 0;
    m_packet.clear();
    m_packetOffset = 0;
    m_packetFrame = 0;
    m_stopRequested = false;

    if (m_dataOnly)
    {
        std::string text = m_message.empty() ? settings.m_m17Message.trimmed().toStdString() : m_message;
        std::vector<uint8_t> content(text.begin(), text.end());
        content.push_back(0);   // the SMS protocol's messages are NUL terminated
        m_packet = DVM17::buildPacket(DVM17::ProtocolSMS, content);
        m_message.clear();
    }

    m_state = StatePreamble;
    qDebug("DSEModM17::start: %s > %s %s CAN %d",
        DVM17::decodeCallsign(m_lsf.m_source).c_str(), DVM17::decodeCallsign(m_lsf.m_destination).c_str(),
        m_dataOnly ? "packet" : "stream", m_lsf.getCAN());
}

void DSEModM17::stop()
{
    m_stopRequested = true;
}

void DSEModM17::abort()
{
    m_state = StateIdle;
    m_stopRequested = false;
}

bool DSEModM17::queueMessage(const std::string& text)
{
    // A packet transmission of its own: it is sent when the transmission starts
    if ((m_state != StateIdle) || text.empty()) {
        return false;
    }

    m_message = text.substr(0, 780);
    return true;
}

void DSEModM17::buildStreamFrame(std::vector<int8_t>& levels, bool last)
{
    // Two Codec2 3200 frames of 20 ms make the 16 byte payload of a 40 ms frame
    uint8_t payload[16];

    for (int i = 0; i < 2; i++)
    {
        uint8_t frame[DSEModVocoder::m_maxFrameBytes];

        if (pullVocoderFrame(frame)) {
            m_voiceFrameCount++;
        }

        std::copy(frame, frame + 8, payload + 8 * i);
    }

    uint16_t number = (uint16_t) ((m_frameNumber & 0x7FFF) | (last ? 0x8000 : 0));
    levels.resize(DVM17::FrameSymbols);
    DVM17::buildStreamFrame(m_lsfBytes, m_segment, number, payload, levels.data());
    m_segment = (m_segment + 1) % DVM17::LICHSegments;
    m_frameNumber = (uint16_t) ((m_frameNumber + 1) & 0x7FFF);
}

void DSEModM17::buildPacketFrame(std::vector<int8_t>& levels)
{
    int remaining = (int) m_packet.size() - m_packetOffset;
    int nbBytes = std::min(remaining, (int) DVM17::PacketBytes);
    bool last = nbBytes >= remaining;
    uint8_t bytes[DVM17::PacketBytes] = { 0 };
    std::copy(m_packet.begin() + m_packetOffset, m_packet.begin() + m_packetOffset + nbBytes, bytes);
    // The counter is the frame's number, or in the last frame the bytes it carries (the CRC included)
    levels.resize(DVM17::FrameSymbols);
    DVM17::buildPacketFrame(bytes, DVM17::PacketBytes, last, last ? nbBytes : m_packetFrame, levels.data());
    m_packetOffset += nbBytes;
    m_packetFrame++;
}

bool DSEModM17::nextFrame(std::vector<int8_t>& levels)
{
    levels.clear();

    switch (m_state)
    {
    case StatePreamble:
        // One frame of +3 -3: the receiver's clock and gain settle on it
        levels.resize(m_preambleSymbols);

        for (int i = 0; i < m_preambleSymbols; i++) {
            levels[i] = (i % 2) ? -3 : 3;
        }

        m_state = StateLSF;
        m_frameCount++;
        return true;

    case StateLSF:
        levels.resize(DVM17::FrameSymbols);
        DVM17::buildLSFFrame(m_lsf, levels.data());
        m_state = m_dataOnly ? StatePacket : StateStream;
        m_frameCount++;
        return true;

    case StateStream:
        buildStreamFrame(levels, m_stopRequested);
        m_frameCount++;

        if (m_stopRequested) {
            m_state = StateEOT;
        }

        return true;

    case StatePacket:
        buildPacketFrame(levels);
        m_frameCount++;

        if (m_packetOffset >= (int) m_packet.size()) {
            m_state = StateEOT;
        }

        return true;

    case StateEOT:
        levels.resize(DVM17::FrameSymbols);
        DVM17::buildEOTFrame(levels.data());
        m_state = StateIdle;
        m_stopRequested = false;
        m_frameCount++;
        return true;

    case StateIdle:
    default:
        return false;
    }
}
