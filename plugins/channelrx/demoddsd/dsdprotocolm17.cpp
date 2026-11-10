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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dsp/dv/dvbits.h"

#include "dsdprotocolm17.h"

namespace {
    bool dsdDebug() { static bool d = getenv("DSD_DEBUG") != nullptr; return d; }
}

DSDProtocolM17::DSDProtocolM17(const DSDSymbolHistory& history) :
    DSDProtocol(history),
    m_active(false),
    m_syncIndex(-1),
    m_pendingSync(-1),
    m_pattern(DVSync::M17LSF),
    m_pendingPattern(DVSync::M17LSF),
    m_burstFirst(-1),
    m_burstLast(-1),
    m_burstSample(0),
    m_misses(0),
    m_burstValid(false),
    m_normalEnd(false),
    m_lsfValid(false),
    m_proven(false),
    m_syncRun(0),
    m_lichSegments(0),
    m_frameCount(0),
    m_lastFrameNumber(0),
    m_packetFrames(0),
    m_revision(0)
{
    std::memset(m_lsfBytes, 0, sizeof(m_lsfBytes));
}

std::vector<DVSync::Pattern> DSDProtocolM17::getSearchPatterns() const
{
    // A transmission starts with its link setup, but a receiver may join a stream or a packet
    // transmission that is already running
    return { DVSync::M17LSF, DVSync::M17Stream, DVSync::M17Packet };
}

void DSDProtocolM17::start(DVSync::Pattern pattern, int64_t syncIndex)
{
    m_active = true;
    m_misses = 0;
    m_burstValid = false;
    m_normalEnd = false;
    m_lsfValid = false;
    m_proven = false;
    m_syncRun = 0;
    m_lichSegments = 0;
    m_frameCount = 0;
    m_lastFrameNumber = 0;
    m_packetBytes.clear();
    m_packetFrames = 0;
    m_protocolFrames.clear();
    m_syncIndex = syncIndex;
    m_pattern = pattern;
    std::memset(m_lsfBytes, 0, sizeof(m_lsfBytes));
}

void DSDProtocolM17::resync(DVSync::Pattern pattern, int64_t syncIndex)
{
    m_misses = 0;
    m_syncIndex = syncIndex;
    m_pattern = pattern;
    noteOnTime();

    if (pattern == DVSync::M17EOT)
    {
        // The end of transmission marker: what was being received is complete
        finishPacket();
        m_normalEnd = true;
        m_active = false;
    }
}

bool DSDProtocolM17::getNextSync(int64_t& syncIndex, std::vector<DVSync::Pattern>& patterns) const
{
    if (!m_active) {
        return false;
    }

    syncIndex = m_syncIndex + DVM17::FrameSymbols;
    patterns = { DVSync::M17Stream, DVSync::M17Packet, DVSync::M17LSF, DVSync::M17EOT };
    return true;
}

void DSDProtocolM17::getConfirmSpan(int64_t syncIndex, int64_t& first, int64_t& last) const
{
    first = syncIndex - DVM17::SyncSymbols + 1;
    last = syncIndex + m_payloadSymbols;
}

bool DSDProtocolM17::confirmWithoutSync(int64_t syncIndex, bool present)
{
    if (!present)
    {
        finishPacket();
        m_active = false;
        return true;
    }

    // Every M17 frame has a sync word: a frame without one is only kept when its payload still
    // decodes, which for a stream frame the Golay coded LICH says cheaply
    if ((m_pattern == DVSync::M17Stream) && m_history.has(syncIndex + 1) && m_history.has(syncIndex + m_payloadSymbols))
    {
        float llr[DVM17::FrameBits];
        float payload[DVM17::PayloadBits];
        std::fill(llr, llr + DVM17::SyncBits, 0.0f);
        m_history.llrs(syncIndex + 1, m_payloadSymbols, llr + DVM17::SyncBits);
        DVM17::framePayload(llr, payload);
        uint8_t chunk[5];

        uint16_t frameNumber = 0;
        uint8_t bytes[16];
        int segment = -1;

        // The LICH's four Golay words decode to something about half the time each even in
        // noise, so accepting a frame on that alone let a chance lock keep itself alive. The
        // frame counter has to continue the stream as well, which noise does not do
        if ((DVM17::unpackLICH(payload, chunk) >= 0)
            && DVM17::decodeStreamFrame(payload, frameNumber, bytes, segment, chunk)
            && (((m_lastFrameNumber + 1) & 0x7FFF) == (frameNumber & 0x7FFF)))
        {
            m_misses = 0;
            m_syncIndex = syncIndex;
            noteOnTime();
            // The frame's time, as pushSymbol takes it for a frame whose sync was found: its last
            // symbol's (the previous frame's otherwise, and its voice was put 40 ms early)
            m_burstSample = m_history.sampleAt(syncIndex + m_payloadSymbols);
            decodeFrame(syncIndex, DVSync::M17Stream);
            return true;
        }
    }

    m_syncRun = 0;

    if (++m_misses >= 3)
    {
        finishPacket();
        m_active = false;
    }

    m_syncIndex = syncIndex;
    return false;
}

void DSDProtocolM17::noteOnTime()
{
    if (++m_syncRun >= ProvenSyncRun) {
        m_proven = true;
    }
}

void DSDProtocolM17::end()
{
    finishPacket();
    m_active = false;
}

bool DSDProtocolM17::pushSymbol(int64_t index, const DSDSoftSymbol& symbol, int64_t sampleIndex)
{
    if (!m_active) {
        return false;
    }

    if (index == m_syncIndex + m_payloadSymbols)
    {
        m_pendingSync = m_syncIndex;
        m_pendingPattern = m_pattern;
        m_burstFirst = m_syncIndex - DVM17::SyncSymbols + 1;
        m_burstLast = index;
        m_burstSample = sampleIndex;
        return true;
    }

    (void) symbol;
    return false;
}

void DSDProtocolM17::getBurstSpan(int64_t& first, int64_t& last) const
{
    first = m_burstFirst;
    last = m_burstLast;
}

void DSDProtocolM17::processBurst()
{
    m_burstValid = false;
    decodeFrame(m_pendingSync, m_pendingPattern);
}

void DSDProtocolM17::decodeFrame(int64_t syncIndex, DVSync::Pattern pattern)
{
    if (!m_history.has(syncIndex + 1) || !m_history.has(syncIndex + m_payloadSymbols)) {
        return;
    }

    float llr[DVM17::FrameBits];
    float payload[DVM17::PayloadBits];
    std::fill(llr, llr + DVM17::SyncBits, 0.0f);
    m_history.llrs(syncIndex + 1, m_payloadSymbols, llr + DVM17::SyncBits);
    DVM17::framePayload(llr, payload);
    m_frameCount++;

    switch (pattern)
    {
    case DVSync::M17LSF:
        decodeLSF(payload);
        break;

    case DVSync::M17Stream:
        decodeStream(payload, syncIndex);
        break;

    case DVSync::M17Packet:
        decodePacket(payload);
        break;

    default:
        break;
    }
}

void DSDProtocolM17::decodeLSF(const float *payload)
{
    DVM17::LSF lsf;
    int cost = 0;

    if (!DVM17::decodeLSFFrame(payload, lsf, &cost)) {
        return;
    }

    m_lsf = lsf;
    m_lsfValid = true;
    m_proven = true;
    m_lichSegments = 0x3F;   // the whole link setup is known
    DVM17::packLSF(m_lsf, m_lsfBytes);
    m_burstValid = true;

    if (dsdDebug()) {
        fprintf(stderr, "M17 link setup: %s > %s type %04x cost %d\n",
            DVM17::decodeCallsign(m_lsf.m_source).c_str(), DVM17::decodeCallsign(m_lsf.m_destination).c_str(),
            m_lsf.m_type, cost);
    }

    DSDProtocolFrame frame;
    frame.m_sample = m_burstSample;
    frame.m_format = "M17";
    frame.m_type = "LSF";
    frame.m_data.assign(m_lsfBytes, m_lsfBytes + DVM17::LSFBytes);
    m_protocolFrames.push_back(frame);
}

void DSDProtocolM17::decodeStream(const float *payload, int64_t syncIndex)
{
    uint16_t frameNumber = 0;
    uint8_t bytes[16];
    uint8_t chunk[5];
    int segment = -1;
    int cost = 0;

    if (!DVM17::decodeStreamFrame(payload, frameNumber, bytes, segment, chunk, &cost)) {
        return;
    }

    // The link setup arrives a sixth at a time for a receiver that joined late
    if ((segment >= 0) && (segment < DVM17::LICHSegments) && !m_lsfValid)
    {
        std::memcpy(m_lsfBytes + 5 * segment, chunk, 5);
        m_lichSegments |= (uint8_t) (1 << segment);

        if (m_lichSegments == 0x3F)
        {
            DVM17::LSF lsf;

            if (DVM17::unpackLSF(m_lsfBytes, lsf))
            {
                m_lsf = lsf;
                m_lsfValid = true;
                m_proven = true;

                if (dsdDebug()) {
                    fprintf(stderr, "M17 link setup from the LICH: %s > %s\n",
                        DVM17::decodeCallsign(m_lsf.m_source).c_str(), DVM17::decodeCallsign(m_lsf.m_destination).c_str());
                }
            }
            else
            {
                m_lichSegments = 0;   // a segment was wrong: collect them again
            }
        }
    }

    const bool continues = ((frameNumber & 0x7FFF) == 0) || ((frameNumber & 0x7FFF) == ((m_lastFrameNumber + 1) & 0x7FFF));
    m_burstValid = segment >= 0;
    m_lastFrameNumber = frameNumber;

    // Two Codec2 3200 packets, 20 ms each
    for (int i = 0; i < 2; i++)
    {
        DSDVoiceFrame voice;
        voice.m_sample = m_burstSample - (1 - i) * 960;   // 20 ms at 48 kHz
        voice.m_slot = 0;
        voice.m_rate = DSDVoiceFrame::RateCodec23200;
        voice.m_nbBits = 64;

        // The convolutional code's corrections over the frame's 272 transmitted code bits, on the
        // first of its two packets: the channel's bit errors (Codec2 carries no code of its own).
        // Only a frame that continues the stream (its LICH decodes by chance about half the time on
        // noise, and the noise after a transmission ends decodes as frames too)
        if ((i == 0) && continues)
        {
            voice.m_channelErrors = cost;
            voice.m_channelBits = DVM17::StreamCodedBits;
        }
        std::copy(bytes + 8 * i, bytes + 8 * (i + 1), voice.m_bytes);
        m_frames.push_back(voice);
    }

    if (frameNumber & 0x8000)
    {
        // The last frame of the transmission; the end of transmission marker follows
        m_normalEnd = true;
    }

    (void) syncIndex;
}

void DSDProtocolM17::decodePacket(const float *payload)
{
    uint8_t bytes[DVM17::PacketBytes];
    bool last = false;
    int counter = 0;
    int cost = 0;

    if (!DVM17::decodePacketFrame(payload, bytes, last, counter, &cost)) {
        return;
    }

    // A packet is at most DVM17::MaxPacketBytes over 33 frames, and its non final frames count
    // up from zero. A transmitter that never sends the final marker - or noise that keeps
    // passing for packet frames - would otherwise grow this without end
    if (!last && (counter != m_packetFrames))
    {
        if (dsdDebug()) {
            fprintf(stderr, "M17 packet: frame counter %d out of turn (expected %d), packet dropped\n",
                counter, m_packetFrames);
        }

        m_packetBytes.clear();
        m_packetFrames = 0;
        return;
    }

    m_burstValid = true;
    int nbBytes = last ? std::min(counter, (int) DVM17::PacketBytes) : (int) DVM17::PacketBytes;

    if (m_packetBytes.size() + (size_t) nbBytes > (size_t) DVM17::MaxPacketBytes)
    {
        if (dsdDebug()) {
            fprintf(stderr, "M17 packet: longer than %d bytes, dropped\n", (int) DVM17::MaxPacketBytes);
        }

        m_packetBytes.clear();
        m_packetFrames = 0;
        return;
    }

    m_packetBytes.insert(m_packetBytes.end(), bytes, bytes + nbBytes);
    m_packetFrames++;

    if (dsdDebug()) {
        fprintf(stderr, "M17 packet frame %d: %d bytes%s\n", m_packetFrames, nbBytes, last ? " (last)" : "");
    }

    if (last) {
        finishPacket();
    }
}

void DSDProtocolM17::finishPacket()
{
    if (m_packetBytes.empty()) {
        return;
    }

    std::vector<uint8_t> content;
    int protocol = DVM17::parsePacket(m_packetBytes, content);
    m_packetBytes.clear();
    m_packetFrames = 0;

    if (protocol < 0)
    {
        if (dsdDebug()) {
            fprintf(stderr, "M17 packet: CRC failed\n");
        }

        return;
    }

    m_revision++;
    m_proven = true;
    m_packet.m_valid = true;
    m_packet.m_protocol = protocol;
    m_packet.m_content = content;
    m_packet.m_revision = m_revision;
    DSDProtocolFrame frame;
    frame.m_sample = m_burstSample;
    frame.m_format = "M17";
    frame.m_type = "packet";
    frame.m_data = content;
    m_protocolFrames.push_back(frame);

    if (protocol == DVM17::ProtocolSMS)
    {
        // A text message: NUL terminated in the packet
        std::string text(content.begin(), content.end());

        while (!text.empty() && (text.back() == '\0')) {
            text.pop_back();
        }

        m_textMessage.m_valid = true;
        m_textMessage.m_source = getSource();
        m_textMessage.m_destination = getDestination();
        m_textMessage.m_text = text;
        m_textMessage.m_revision = m_revision;
    }
    else if (protocol == DVM17::ProtocolAPRS)
    {
        // An APRS packet: the information field of the station that sent it
        std::string info(content.begin(), content.end());

        while (!info.empty() && ((info.back() == '\0') || (info.back() == '\n') || (info.back() == '\r'))) {
            info.pop_back();
        }

        std::string source = getSource();

        if (!source.empty() && !info.empty()) {
            m_aprsPackets.push_back(source + ">APM17:" + info);
        }
    }
}

bool DSDProtocolM17::takeAPRSPacket(std::string& packet)
{
    if (m_aprsPackets.empty()) {
        return false;
    }

    packet = m_aprsPackets.front();
    m_aprsPackets.erase(m_aprsPackets.begin());
    return true;
}

std::string DSDProtocolM17::getStatusText() const
{
    if (!m_lsfValid) {
        return "no link setup";
    }

    char buffer[128];
    const char *type = m_lsf.isStream() ? (m_lsf.getDataType() == 2 ? "voice" : "stream") : "packet";
    snprintf(buffer, sizeof(buffer), "%s > %s | %s | CAN %d",
        DVM17::decodeCallsign(m_lsf.m_source).c_str(), DVM17::decodeCallsign(m_lsf.m_destination).c_str(),
        type, m_lsf.getCAN());
    return buffer;
}
