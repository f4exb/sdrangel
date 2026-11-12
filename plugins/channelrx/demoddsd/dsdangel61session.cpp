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
#include <cmath>
#include <cstdio>

#include "dsp/dv/dvangel61aprs.h"
#include "dsp/dv/vocoscodec.h"

#include "dsdangel61session.h"

DSDAngel61Session::DSDAngel61Session(DVAngel61::Profile profile, int frameSamples, int minimumFrames) :
    m_profile(profile),
    m_frameSamples(frameSamples),
    m_minimumFrames(minimumFrames)
{
    DSDAngel61Session::reset();
}

void DSDAngel61Session::reset()
{
    m_sample = 0;
    m_active = false;
    m_lastConfirmedSample = 0;
    m_lastHeardSample = 0;
    m_heardUnconfirmed = 0;
    m_heardTier = -1;
    m_heardTierFrames = 0;
    m_preHeardSample = 0;
    m_preHeard = 0;
    m_preHeardTier = -1;
    m_preHeardTierFrames = 0;
    m_startLost = 0;
    m_startTier = -1;
    m_startTierFrames = 0;
    m_gatewayRecords.clear();
    m_gatewayMessages.clear();
    m_gatewayLastSent = false;
    m_hasRouting = m_hasPath = m_hasStation = m_hasPosition = false;
    m_routing = DVAngel61::Routing();
    m_path = DVAngel61::Path();
    m_station = DVAngel61::Station();
    m_position = DVAngel61::Position();
    m_lastType = -1;
    m_lastCounter = -1;
    m_requestedTier = 0;
    m_judgedTier = -1;
    m_framesExpected = 0;
    m_framesLost = 0;
    m_voiceFrames = 0;
    m_maxConsecutiveLost = 0;
    m_snrSum = 0.0;
    m_snrCount = 0;
    m_minSnrDB = 0.0f;
    m_frequencyHz = 0.0f;
    m_unhandledMessages = 0;
    m_begins = m_ends = 0;
    m_ended.clear();
    m_reassembler = DVAngel61Reassembler();
    m_fileReceiver.reset();
    m_fileDelivered = false;
    m_heardControls.clear();
    m_datagrams.clear();
    m_tunnelControls.clear();
    m_compressionControls.clear();
    m_peerTunnelValid = false;
    m_datagramsRefused = 0;
    m_datagramsDiscarded = 0;
    m_decompressors.clear();
    m_decompressorUsed.clear();
    m_messages.clear();
    m_voice.clear();
    m_voiceStarted = false;
    m_positions.clear();
    m_aprsPackets.clear();
    m_correspondentUpdates.clear();
    m_dtmfSource = 0;
    m_dtmfSequence = -1;
    m_dtmfNext = 0;
    m_dtmfReceived.clear();
    m_dtmfText.clear();
    m_dtmfLastSample = -1;
    m_dtmfKeys.clear();
}

void DSDAngel61Session::frameHeard(bool headerOk, float snrDb, float frequencyHz, const DVAngel61::Header *header)
{
    // The SNR is over the frames whose header was accepted, in the transmission: a confirmed frame's
    // own counts from the one that begins it to the one that ends it (confirmed calls countSnr)
    m_frameSnr = headerOk && std::isfinite(snrDb) ? std::pow(10.0, snrDb / 10.0) : -1.0;
    m_frequencyHz = frequencyHz;

    if (m_active) {
        countSnr();
    }

    // A header of the profile heard in a transmission: it goes on, and the frame is lost unless it is
    // confirmed. What the header says is used only for the tier judged when nothing of the tier is
    // confirmed, and then only when several agree (and the station slot, if heard)
    if (!headerOk || !header || (DVAngel61::profileOf(header->m_type) != m_profile) || !DVAngel61::isKnownType(header->m_type)) {
        return;
    }

    const bool tiered = DVAngel61::isVoice(header->m_type) || DVAngel61::isData(header->m_type);
    const int tier = tiered ? DVAngel61::tierOf(header->m_type) : -1;

    if (m_active)
    {
        m_lastHeardSample = m_sample;
        m_heardUnconfirmed++;

        if (tiered)
        {
            m_heardTierFrames = tier == m_heardTier ? m_heardTierFrames + 1 : 1;
            m_heardTier = tier;
        }
    }
    else
    {
        // Before a transmission begins (its preamble lost): the run the first confirmed frame may end
        if (m_sample - m_preHeardSample > (int64_t) (EndAfterFrames + 1) * m_frameSamples)
        {
            m_preHeard = 0;
            m_preHeardTier = -1;
            m_preHeardTierFrames = 0;
        }

        m_preHeardSample = m_sample;
        m_preHeard++;

        if (tiered)
        {
            m_preHeardTierFrames = tier == m_preHeardTier ? m_preHeardTierFrames + 1 : 1;
            m_preHeardTier = tier;
        }
    }
}

void DSDAngel61Session::countSnr()
{
    if (m_frameSnr < 0.0) {
        return;
    }

    const float db = (float) (10.0 * std::log10(std::max(1e-12, m_frameSnr)));
    m_minSnrDB = m_snrCount ? std::min(m_minSnrDB, db) : db;
    m_snrSum += m_frameSnr;
    m_snrCount++;
    m_frameSnr = -1.0;      // counted once
}

void DSDAngel61Session::advance(int n)
{
    m_sample += n;

    // Nothing confirmed or heard for EndAfterFrames frame periods (or the lock gone): the transmission ended
    if (m_active && (m_sample - std::max(m_lastConfirmedSample, m_lastHeardSample) > (int64_t) (EndAfterFrames + 1) * m_frameSamples)) {
        end(m_sample);
    }

    m_reassembler.expire(m_sample / (SampleRate / 1000));

    // DTMF (3.8): keys waiting on a lost one, and a sequence with no new key for 2 s ended
    if (!m_dtmfReceived.empty()) {
        dtmfRelease(m_sample, false);
    }

    if (!m_dtmfText.empty() && (m_sample - m_dtmfLastSample > (int64_t) DTMFSequenceEndMs * (SampleRate / 1000)))
    {
        dtmfRelease(m_sample, true);
        dtmfEnd();
    }
}

void DSDAngel61Session::confirmed(const DVAngel61::Header& header, int firstCounter, int frames, const uint8_t *block, int64_t sample,
    int channelErrors, int channelBits)
{
    const int type = header.m_type;
    int gap = 0;
    // The frames heard since the last confirmed unit, less this unit's own
    const int heardLost = std::max(0, m_heardUnconfirmed - frames);

    if (m_active && (m_lastCounter >= 0))
    {
        gap = (firstCounter - m_lastCounter - 1) & 0x3F;

        if (gap > EndAfterFrames + heardLost)
        {
            // Too far on for the same transmission, even with the frames heard between: a new one
            m_heardUnconfirmed = heardLost;
            end(sample);
            gap = 0;
        }
    }

    m_heardUnconfirmed = 0;

    if (!m_active)
    {
        begin(sample);

        // The frames heard just before this, the first confirmed one: lost at the start, kept for a
        // transmission with no voice or data confirmed (end)
        if ((m_preHeard > frames) && (m_sample - m_preHeardSample <= (int64_t) (EndAfterFrames + 1) * m_frameSamples))
        {
            m_startLost = m_preHeard - frames;
            m_startTier = m_preHeardTier;
            m_startTierFrames = m_preHeardTierFrames;
        }

        m_preHeard = 0;
        m_preHeardTier = -1;
        m_preHeardTierFrames = 0;
    }

    // The gateway's frame periods: the lost ones idle, a data group's four idle, then this frame
    for (int i = 0; i < gap; i++) {
        gatewayIdle((firstCounter - gap + i) & 0x3F, false);
    }

    if (frames == 1) {
        gatewayRecord(header, block);
    } else {
        for (int i = 0; i < frames; i++) {
            gatewayIdle((firstCounter + i) & 0x3F, header.m_lastFrame && (i == frames - 1));
        }
    }

    countSnr();
    m_lastConfirmedSample = sample;
    m_framesExpected += gap + frames;
    m_framesLost += gap;
    m_maxConsecutiveLost = std::max(m_maxConsecutiveLost, gap);
    m_lastType = type;
    m_lastCounter = (firstCounter + frames - 1) & 0x3F;
    m_requestedTier = header.m_tier;

    // The voice's time: frames lost since the last one, while the transmission is a voice one
    if (m_voiceStarted && (gap > 0)) {
        voiceErasures(gap, sample);
    }

    if (DVAngel61::isVoice(type))
    {
        m_voiceFrames += frames;
        m_judgedTier = DVAngel61::tierOf(type);

        voiceFrame(type, block, sample, channelErrors, channelBits);    // V1, or WB tier 3's Opus (O1)
    }
    else if (m_voiceStarted && DVAngel61::isIdle(type) && !header.m_lastFrame)
    {
        voiceErasures(frames, sample);          // a pause in the voice source (2.8)
    }
    else if (DVAngel61::isData(type))
    {
        m_judgedTier = DVAngel61::tierOf(type);
    }

    for (int s = 0; s < DVAngel61::slotsOf(type); s++) {
        processSlot(block + DVAngel61::slotOffset(type, s), sample);
    }

    const int bytes = DVAngel61::messageFieldBytes(type);

    if (bytes > 0)
    {
        DVAngel61::MessageFragment fragment;
        DVAngel61::unpackMessageField(block + DVAngel61::messageFieldOffset(type), bytes, fragment);
        m_fieldFromGroup = frames > 1;
        processMessageField(fragment, sample);
        m_fieldFromGroup = false;
    }

    if (header.m_lastFrame) {
        end(sample + (int64_t) frames * m_frameSamples);
    }
}

void DSDAngel61Session::processSlot(const uint8_t *slot, int64_t sample)
{
    switch (DVAngel61::slotID(slot))
    {
    case DVAngel61::SlotRouting:
        m_hasRouting = DVAngel61::unpackRoutingSlot(slot, m_routing) || m_hasRouting;
        break;

    case DVAngel61::SlotPath:
        m_hasPath = DVAngel61::unpackPathSlot(slot, m_path) || m_hasPath;
        break;

    case DVAngel61::SlotStation:
        m_hasStation = DVAngel61::unpackStationSlot(slot, m_station) || m_hasStation;
        break;

    case DVAngel61::SlotLongCallsign:
    {
        // Encoding a long callsign's text makes its code known to decodeCallsign (2.6)
        std::string callsign;

        if (DVAngel61::unpackLongCallsignSlot(slot, callsign)) {
            DVAngel61::encodeCallsign(callsign);
        }
        break;
    }

    case DVAngel61::SlotPosition:
    {
        DVAngel61::Position position;

        if (DVAngel61::unpackPositionSlot(slot, position) && (position.m_fixAge != DVAngel61::Position::NoFix))
        {
            const bool changed = !m_hasPosition
                || (std::fabs(position.m_latitude - m_position.m_latitude) > 1e-6)
                || (std::fabs(position.m_longitude - m_position.m_longitude) > 1e-6)
                || (position.m_altitude != m_position.m_altitude)
                || (position.m_speed != m_position.m_speed)
                || (position.m_course != m_position.m_course);
            m_position = position;
            m_hasPosition = true;

            if (changed)
            {
                PositionReport report;
                report.m_source = m_hasRouting ? m_routing.m_source : DVAngel61::CallsignNone;
                report.m_position = position;
                report.m_sample = sample;
                m_positions.push_back(report);
                emitAPRS(DVAngel61APRS::position(position,
                    m_hasStation ? m_station.m_symbolTable : 0, m_hasStation ? m_station.m_symbolCode : 0));
            }
        }
        break;
    }

    case DVAngel61::SlotDTMF:
    {
        DVAngel61::DTMF dtmf;

        if (DVAngel61::unpackDTMFSlot(slot, dtmf)) {
            dtmfSlot(dtmf, sample);
        }
        break;
    }

    case DVAngel61::SlotText:
    {
        DVAngel61::TextFragment fragment;

        if (DVAngel61::unpackTextSlot(slot, fragment))
        {
            DVAngel61Reassembler::Message completed;
            const uint64_t source = m_hasRouting ? m_routing.m_source : DVAngel61::CallsignNone;

            if (m_reassembler.addTextFragment(source, fragment, sample / (SampleRate / 1000), completed))
            {
                Message message;
                message.m_source = source;
                message.m_destination = m_hasRouting ? m_routing.m_destination : DVAngel61::CallsignNone;   // (not the last transmission's)
                message.m_contentType = DVAngel61::ContentText;
                message.m_text = completed.text();
                message.m_fromSlots = true;
                message.m_sample = sample;
                deliver(message);
            }
        }
        break;
    }

    default:
        break;
    }
}

void DSDAngel61Session::processMessageField(const DVAngel61::MessageFragment& fragment, int64_t sample)
{
    if (fragment.m_contentType > DVAngel61::ContentTunnelControl)
    {
        m_unhandledMessages++;      // reserved
        return;
    }

    const uint64_t source = m_hasRouting ? m_routing.m_source : DVAngel61::CallsignNone;

    if (fragment.m_contentType == DVAngel61::ContentFileData)
    {
        // A block of a file (3.4): one field, never fragmented
        m_fileReceiver.addBlock(source, fragment.m_id, fragment.blockIndex(), fragment.m_payload.data(), (int) fragment.m_payload.size());
        return;
    }

    if (fragment.m_contentType == DVAngel61::ContentFileOffer) {
        m_fileReceiver.noteOfferFragment(fragment.m_id);
    }

    DVAngel61Reassembler::Message completed;

    if (!m_reassembler.addMessageFragment(source, fragment, sample / (SampleRate / 1000), completed)) {
        return;
    }

    if (completed.m_contentType == DVAngel61::ContentFileOffer)
    {
        // To CQ or a talkgroup: received, never answered (3.4)
        // Only a new transfer is delivered again: a repeated offer of one that completed would
        // otherwise write the file once more for each repeat
        if (m_fileReceiver.setOffer(source, completed.m_data, m_hasRouting && DVAngel61::isGroup(m_routing.m_destination))) {
            m_fileDelivered = false;
        }
    }
    else if (completed.m_contentType == DVAngel61::ContentFileControl)
    {
        // The answer to a file somebody is transmitting: the modulator decides whether it asked
        DVAngel61FileControl control;

        if (DVAngel61FileControl::decode(completed.m_data, control)) {
            m_heardControls.push_back(control);
        }
    }
    else if (completed.m_contentType == DVAngel61::ContentIPDatagram)
    {
        // A datagram (3.5), its header perhaps compressed (3.6): checked before it goes anywhere
        if ((m_decompressors.find(source) == m_decompressors.end()) && ((int) m_decompressors.size() >= MaxDecompressorSources))
        {
            auto oldest = std::min_element(m_decompressorUsed.begin(), m_decompressorUsed.end(),
                [](const std::pair<const uint64_t, int64_t>& a, const std::pair<const uint64_t, int64_t>& b) { return a.second < b.second; });

            if (oldest != m_decompressorUsed.end())
            {
                m_decompressors.erase(oldest->first);
                m_decompressorUsed.erase(oldest);
            }
        }

        DVAngel61HC::Decompressor& decompressor = m_decompressors[source];
        m_decompressorUsed[source] = sample;
        std::vector<uint8_t> datagram;

        if (decompressor.decompress(completed.m_data, completed.m_flags, datagram))
        {
            if (DVAngel61IP::classify(datagram) == DVAngel61IP::Accepted) {
                m_datagrams.push_back(std::make_pair(source, datagram));
            } else {
                m_datagramsRefused++;
            }
        }
        else
        {
            m_datagramsDiscarded++;
        }

        DVAngel61IP::Control state;
        state.m_kind = DVAngel61IP::Control::ContextState;
        state.m_contexts = decompressor.takeStale();

        if (!state.m_contexts.empty()) {
            m_compressionControls.push_back(std::make_pair(source, state));
        }

        DVAngel61IP::Control ack;
        ack.m_kind = DVAngel61IP::Control::ContextAck;
        ack.m_contexts = decompressor.takeAckRequests();

        if (!ack.m_contexts.empty()) {
            m_compressionControls.push_back(std::make_pair(source, ack));
        }
    }
    else if (completed.m_contentType == DVAngel61::ContentTunnelControl)
    {
        DVAngel61IP::Control control;

        if (DVAngel61IP::Control::decode(completed.m_data, control))
        {
            m_tunnelControls.push_back(std::make_pair(source, control));

            // Who answers a repair request (3.5): its advertise says so
            if ((control.m_kind == DVAngel61IP::Control::Advertise) && (control.m_flags & DVAngel61IP::Control::FlagRepairs) && (m_repairingSources.size() < 64)) {
                m_repairingSources.insert(source);
            } else if ((control.m_kind == DVAngel61IP::Control::Advertise) || (control.m_kind == DVAngel61IP::Control::Close)) {
                m_repairingSources.erase(source);
            }

            if (control.m_kind == DVAngel61IP::Control::Advertise)
            {
                m_peerTunnel = control;
                m_peerTunnelValid = true;
            }
            else if (control.m_kind == DVAngel61IP::Control::Close)
            {
                m_peerTunnelValid = false;
                m_decompressors.erase(source);
                m_decompressorUsed.erase(source);
            }
        }
    }
    else
    {
        Message message;
        message.m_source = source;
        message.m_destination = m_hasRouting ? m_routing.m_destination : DVAngel61::CallsignNone;
        message.m_contentType = completed.m_contentType;
        message.m_text = completed.text();
        message.m_fromSlots = false;
        message.m_sample = sample;
        deliver(message);

        // A data group's message for a gateway: no record carries it
        if (m_fieldFromGroup && DVAngel61::relayable(completed.m_contentType) && !completed.m_data.empty()
            && ((int) completed.m_data.size() <= DVAngel61::MaxRelayMessageBytes)
            && ((int) m_gatewayMessages.size() < MaxGatewayMessages))
        {
            m_gatewayMessages.push_back(DVAngel61::packRelayMessage(completed.m_contentType, completed.m_flags, completed.m_data));
        }
    }
}

void DSDAngel61Session::deliver(const Message& message)
{
    m_messages.push_back(message);

    if (!message.m_dtmf) {
        m_textMessages++;
    }

    // As APRS (3.3): an information field verbatim, text to a callsign as a message, text to a
    // group as a status
    if (message.m_contentType == DVAngel61::ContentAPRS)
    {
        emitAPRS(message.m_text);
    }
    else if (message.m_contentType == DVAngel61::ContentText)
    {
        if (!DVAngel61::isGroup(message.m_destination) && (message.m_destination != DVAngel61::CallsignNone)) {
            emitAPRS(DVAngel61APRS::message(DVAngel61::stationIdentity(DVAngel61::decodeCallsign(message.m_destination)), message.m_text));
        } else {
            emitAPRS(DVAngel61APRS::status(message.m_text));
        }
    }
}

void DSDAngel61Session::emitAPRS(const std::string& info)
{
    if (info.empty() || !m_hasRouting) {
        return;
    }

    std::string packet = DVAngel61APRS::tnc2(m_routing.m_source, m_path, m_hasPath, info);

    if (!packet.empty()) {
        m_aprsPackets.push_back(packet);
    }
}

void DSDAngel61Session::voiceErasures(int frames, int64_t sample)
{
    const bool opus = m_voiceRate == DSDVoiceFrame::RateOpus80;
    const int packets = opus ? 1 : DVAngel61::packetsPerFrame(m_profile);

    for (int f = 0; f < frames; f++)
    {
        DSDVoiceFrame voice;
        voice.m_sample = sample;
        voice.m_rate = m_voiceRate;
        voice.m_erasure = true;
        voice.m_packets = packets;
        voice.m_packetBytes = 0;
        voice.m_nbBits = 0;
        m_voice.push_back(voice);
    }
}

void DSDAngel61Session::voiceFrame(int type, const uint8_t *block, int64_t sample, int channelErrors, int channelBits)
{
    if (DVAngel61::isOpus(type))
    {
        // O1 (3.1.2): the frame's one 80 ms packet, its 1472 bits MSB first into 184 bytes
        DSDVoiceFrame voice;
        voice.m_sample = sample;
        voice.m_rate = DSDVoiceFrame::RateOpus80;
        voice.m_packets = 1;
        voice.m_packetBytes = DVAngel61::OpusBits / 8;
        voice.m_nbBits = DVAngel61::OpusBits;
        voice.m_channelErrors = channelErrors;
        voice.m_channelBits = channelBits;

        for (int i = 0; i < DVAngel61::OpusBits; i++) {
            voice.m_bytes[i / 8] |= (uint8_t) ((block[i] & 1) << (7 - (i % 8)));
        }

        m_voice.push_back(voice);
        m_voiceStarted = true;
        m_voiceRate = DSDVoiceFrame::RateOpus80;
        return;
    }

    m_voiceRate = DSDVoiceFrame::RateVocos;
    const int packets = DVAngel61::packetsPerFrame(m_profile);
    const int codebooks = DVAngel61::codebooksOf(type);
    const int bits = DVAngel61::packetBits(type);
    const int bytes = VocosCodec::packetBytes(codebooks);
    DSDVoiceFrame voice;
    voice.m_sample = sample;
    voice.m_rate = DSDVoiceFrame::RateVocos;
    voice.m_packets = packets;
    voice.m_packetBytes = bytes;
    voice.m_nbBits = packets * bits;
    voice.m_channelErrors = channelErrors;
    voice.m_channelBits = channelBits;
    uint8_t packetBits[DVAngel61::CodebookBits * VocosCodec::MaxCodebooks];

    // Each packet's bits, MSB first, into whole bytes (3.1.1's layout), the spare bits zero
    for (int p = 0; (p < packets) && ((p + 1) * bytes <= DSDVoiceFrame::MaxBytes); p++)
    {
        DVAngel61::getVoicePacket(block, type, p, packetBits);
        uint8_t *packet = voice.m_bytes + p * bytes;

        for (int i = 0; i < bits; i++) {
            packet[i / 8] |= (uint8_t) ((packetBits[i] & 1) << (7 - (i % 8)));
        }
    }

    m_voice.push_back(voice);
    m_voiceStarted = true;
}

std::vector<DSDVoiceFrame> DSDAngel61Session::takeVoice()
{
    std::vector<DSDVoiceFrame> voice;
    voice.swap(m_voice);
    return voice;
}

void DSDAngel61Session::begin(int64_t sample)
{
    m_active = true;
    m_voiceStarted = false;
    m_voiceRate = DSDVoiceFrame::RateVocos;
    m_lastConfirmedSample = sample;
    m_lastHeardSample = sample;
    m_heardUnconfirmed = 0;
    m_heardTier = -1;
    m_heardTierFrames = 0;
    m_startLost = 0;
    m_startTier = -1;
    m_startTierFrames = 0;
    m_gatewayLastSent = false;
    m_hasRouting = m_hasPath = m_hasStation = m_hasPosition = false;
    m_lastType = -1;
    m_lastCounter = -1;
    m_requestedTier = 0;
    m_judgedTier = -1;
    m_framesExpected = 0;
    m_framesLost = 0;
    m_voiceFrames = 0;
    m_maxConsecutiveLost = 0;
    m_snrSum = 0.0;
    m_snrCount = 0;
    m_minSnrDB = 0.0f;
    m_beginSample = sample;
    m_textMessages = 0;
    m_begins++;
}

void DSDAngel61Session::end(int64_t sample)
{
    if (!m_active) {
        return;
    }

    m_active = false;
    m_ends++;

    // The end of a transmission ends its DTMF sequence (3.8)
    dtmfRelease(sample, true);
    dtmfEnd();
    m_dtmfSequence = -1;

    // A gateway's call ends with a last frame: the transmission's, or one standing in for it
    if (!m_gatewayLastSent && (m_lastCounter >= 0)) {
        gatewayIdle((m_lastCounter + 1) & 0x3F, true);
    }

    // The frames heard after the last confirmed one were lost
    if (m_heardUnconfirmed > 0)
    {
        m_framesExpected += m_heardUnconfirmed;
        m_framesLost += m_heardUnconfirmed;
        m_maxConsecutiveLost = std::max(m_maxConsecutiveLost, m_heardUnconfirmed);
        m_heardUnconfirmed = 0;
    }

    // No voice or data frame confirmed, and frames heard before the first confirmed one: they were
    // lost too (the preamble and the voice failed together), and their headers say the tier
    if ((m_judgedTier < 0) && (m_startLost > 0))
    {
        m_framesExpected += m_startLost;
        m_framesLost += m_startLost;
        m_maxConsecutiveLost = std::max(m_maxConsecutiveLost, m_startLost);

        if (m_heardTierFrames < HeardTierFrames)
        {
            m_heardTier = m_startTier;
            m_heardTierFrames = m_startTierFrames;
        }
    }

    // No voice or data frame confirmed: the tier heard in their headers, when three in a row agree on
    // it and the station slot, if heard (it is in the preamble's first frame), announced it. A tier
    // failing outright is judged; a header passing its CRC-8 wrongly is not enough to set one
    if ((m_judgedTier < 0) && (m_heardTierFrames >= HeardTierFrames) && (!m_hasStation || (m_station.m_nextTier == m_heardTier))) {
        m_judgedTier = m_heardTier;
    }

    // V1's decoder's last audio, and afresh for the next transmission (3.1.1); Opus has no delay to flush
    if (m_voiceStarted && (m_voiceRate == DSDVoiceFrame::RateVocos))
    {
        DSDVoiceFrame flush;
        flush.m_sample = sample;
        flush.m_rate = DSDVoiceFrame::RateVocos;
        flush.m_flush = true;
        flush.m_nbBits = 0;
        m_voice.push_back(flush);
        m_voiceStarted = false;
    }

    // The datagrams of it still incomplete: their missing fragments asked for (3.5)
    requestRepairs(sample);

    // The transmission as it ended (Ended)
    if ((int) m_ended.size() < MaxEnded)
    {
        Ended ended;
        ended.m_hasRouting = m_hasRouting;
        ended.m_routing = m_routing;
        ended.m_hasPosition = m_hasPosition;
        ended.m_position = m_position;
        ended.m_framesExpected = m_framesExpected;
        ended.m_framesLost = m_framesLost;
        ended.m_voiceFrames = m_voiceFrames;
        ended.m_meanSnrDB = getMeanSnrDB();
        ended.m_minSnrDB = getMinSnrDB();
        ended.m_textMessages = m_textMessages;
        ended.m_beginSample = m_beginSample;
        ended.m_endSample = sample;
        m_ended.push_back(ended);
    }

    if (!m_hasRouting || (m_routing.m_source == DVAngel61::CallsignNone) || DVAngel61::isGroup(m_routing.m_source)) {
        return;
    }

    // Expire what is old: the send tier after a minute, the holds after 16 minutes unheard
    if ((m_correspondents.find(m_routing.m_source) == m_correspondents.end()) && ((int) m_correspondents.size() >= MaxCorrespondents))
    {
        auto oldest = std::min_element(m_correspondents.begin(), m_correspondents.end(),
            [](const std::pair<const uint64_t, Correspondent>& a, const std::pair<const uint64_t, Correspondent>& b) { return a.second.m_sample < b.second.m_sample; });
        m_correspondents.erase(oldest);
    }

    Correspondent& c = m_correspondents[m_routing.m_source];
    const int64_t ms = SampleRate / 1000;

    if ((c.m_callsign != 0) && ((c.m_profile != m_profile) || (sample - c.m_sample > (int64_t) ExpiryMs * ms))) {
        c.m_sendTier = 0;
    }

    if ((c.m_callsign != 0) && ((c.m_profile != m_profile) || (sample - c.m_sample > (int64_t) HoldMaxMs * ms)))
    {
        std::fill(c.m_holdFailures, c.m_holdFailures + 4, 0);
        std::fill(c.m_holdUntil, c.m_holdUntil + 4, (int64_t) -1);
    }

    c.m_callsign = m_routing.m_source;
    c.m_destination = m_routing.m_destination;
    c.m_profile = m_profile;
    c.m_requestedTier = m_requestedTier;
    c.m_sample = sample;
    c.m_frames = m_framesExpected;
    c.m_blockErrorRate = getBlockErrorRate();
    c.m_meanSnrDB = getMeanSnrDB();
    c.m_minSnrDB = getMinSnrDB();
    c.m_maxConsecutiveLost = m_maxConsecutiveLost;

    // Idle-only transmissions, and short ones, say nothing about a tier
    if ((m_judgedTier >= 0) && (m_framesExpected >= m_minimumFrames)) {
        judgeRate(c, m_judgedTier, sample);
    }

    m_correspondentUpdates.push_back(c);
}

void DSDAngel61Session::gatewayRecord(const DVAngel61::Header& header, const uint8_t *block)
{
    if (m_gatewayRecords.size() >= (size_t) MaxGatewayRecords) {
        m_gatewayRecords.pop_front();       // nobody takes them
    }

    m_gatewayRecords.push_back(DVAngel61::packRecord(header, block));
    m_gatewayLastSent = m_gatewayLastSent || header.m_lastFrame;
}

void DSDAngel61Session::gatewayIdle(int counter, bool last)
{
    DVAngel61::Header header;
    header.m_type = DVAngel61::typeOf(m_profile, DVAngel61::KindIdle);
    header.m_counter = counter;
    header.m_lastFrame = last;
    header.m_tier = m_requestedTier;
    std::vector<uint8_t> block(DVAngel61::infoBits(header.m_type), 0);
    DVAngel61::finishBlock(block.data(), header);
    gatewayRecord(header, block.data());
}

std::vector<std::vector<uint8_t>> DSDAngel61Session::takeGatewayMessages()
{
    std::vector<std::vector<uint8_t>> messages(m_gatewayMessages.begin(), m_gatewayMessages.end());
    m_gatewayMessages.clear();
    return messages;
}

std::vector<std::vector<uint8_t>> DSDAngel61Session::takeGatewayRecords()
{
    std::vector<std::vector<uint8_t>> records(m_gatewayRecords.begin(), m_gatewayRecords.end());
    m_gatewayRecords.clear();
    return records;
}

int DSDAngel61Session::getSendTier() const
{
    if (!m_hasRouting) {
        return 0;
    }

    auto it = m_correspondents.find(m_routing.m_source);
    return it == m_correspondents.end() ? 0 : it->second.m_sendTier;
}

float DSDAngel61Session::getMeanSnrDB() const
{
    return m_snrCount ? (float) (10.0 * std::log10(std::max(1e-12, m_snrSum / m_snrCount))) : 0.0f;
}

std::string DSDAngel61Session::getStatusText() const
{
    char buffer[200];
    const std::string routing = m_hasRouting
        ? DVAngel61::decodeCallsign(m_routing.m_source) + " > " + DVAngel61::decodeCallsign(m_routing.m_destination)
        : std::string("-- > --");
    const std::string mode = m_lastType >= 0 ? DVAngel61::typeName(m_lastType) : DVAngel61::profileName(m_profile);
    // What each rate rule reads (2.10): HF the mean SNR3k, VHF the least Es/N0
    const bool hf = DVAngel61::isHF(m_profile);
    // What the station slot announces (2.5): the family of the voice it will use next, and the tier
    std::string station;
    if (m_hasStation)
    {
        const int family = m_station.m_codecFamily;
        station = std::string(" (") + (family == DVAngel61::CodecFamilyV1 ? "V1" : family == DVAngel61::CodecFamilyO1 ? "O1"
            : family == DVAngel61::CodecFamilyL1 ? "L1" : "family " + std::to_string(family))
            + (m_station.m_protocolVersion != DVAngel61::ProtocolVersion ? " rev " + std::to_string(m_station.m_protocolVersion) : std::string())
            + ", next tier " + std::to_string(m_station.m_nextTier) + ")";
    }
    snprintf(buffer, sizeof(buffer), "%s | %s%s | BLER %.1f%% | %s %.1f dB | req %d send %d",
        routing.c_str(), mode.c_str(), station.c_str(), 100.0f * getBlockErrorRate(),
        hf ? "SNR3k" : "Es/N0 min", hf ? getMeanSnrDB() : getMinSnrDB(), m_requestedTier, getSendTier());
    // The DTMF sequence arriving (3.8)
    return m_dtmfText.empty() ? std::string(buffer) : std::string(buffer) + " | DTMF " + m_dtmfText;
}

bool DSDAngel61Session::takeCompletedFile(uint64_t& source, std::string& name, std::vector<uint8_t>& data)
{
    // (a file the operator is still to confirm is not delivered: 3.4)
    if (m_fileDelivered || (m_fileReceiver.getState() != DVAngel61FileReceiver::Complete) || !m_fileReceiver.isConfirmed()) {
        return false;
    }

    m_fileDelivered = true;
    source = m_fileReceiver.getSource();
    name = DVAngel61FileReceiver::sanitiseName(m_fileReceiver.getOffer().m_name);
    data = m_fileReceiver.getData();
    return true;
}

std::vector<DVAngel61FileControl> DSDAngel61Session::takeHeardFileControls()
{
    std::vector<DVAngel61FileControl> controls;
    controls.swap(m_heardControls);
    return controls;
}

std::vector<std::pair<uint64_t, std::vector<uint8_t>>> DSDAngel61Session::takeDatagrams()
{
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> datagrams;
    datagrams.swap(m_datagrams);
    return datagrams;
}

std::vector<std::pair<uint64_t, DVAngel61IP::Control>> DSDAngel61Session::takeTunnelControls()
{
    std::vector<std::pair<uint64_t, DVAngel61IP::Control>> controls;
    controls.swap(m_tunnelControls);
    return controls;
}

void DSDAngel61Session::requestRepairs(int64_t sample)
{
    // A datagram of this transmission incomplete when it ends: its missing fragments asked of the
    // station that sent it, in the reply slot, when it said it answers (3.5). One round only: a
    // second failure drops it, a third second of latency being worth less than the airtime
    if (!m_hasRouting || !m_repairingSources.count(m_routing.m_source)) {
        return;
    }

    const uint64_t source = m_routing.m_source;

    for (const DVAngel61Reassembler::Incomplete& m : m_reassembler.incomplete(source, DVAngel61::ContentIPDatagram, m_beginSample / (SampleRate / 1000)))
    {
        if (m_repairsRequested.count(std::make_pair(source, m.m_id))) {
            continue;
        }

        if (m_repairsRequested.size() >= 256) {
            m_repairsRequested.clear();
        }

        m_repairsRequested.insert(std::make_pair(source, m.m_id));
        DVAngel61IP::Control repair;
        repair.m_kind = DVAngel61IP::Control::Repair;
        repair.m_messageId = m.m_id;
        repair.m_ranges.assign(m.m_missing.begin(), m.m_missing.begin() + std::min<size_t>(m.m_missing.size(), 255));
        m_compressionControls.push_back(std::make_pair(source, repair));
    }

    (void) sample;
}

std::vector<std::pair<uint64_t, DVAngel61IP::Control>> DSDAngel61Session::takeCompressionControls()
{
    std::vector<std::pair<uint64_t, DVAngel61IP::Control>> controls;
    controls.swap(m_compressionControls);
    return controls;
}

void DSDAngel61Session::dtmfSlot(const DVAngel61::DTMF& dtmf, int64_t sample)
{
    const uint64_t source = m_hasRouting ? m_routing.m_source : DVAngel61::CallsignNone;

    // Another sequence, or another station's: the one arriving has ended
    if ((m_dtmfSequence >= 0) && ((dtmf.m_sequence != m_dtmfSequence) || (source != m_dtmfSource)))
    {
        dtmfRelease(sample, true);
        dtmfEnd();
        m_dtmfSequence = -1;
    }

    if (m_dtmfSequence < 0)
    {
        m_dtmfSequence = dtmf.m_sequence;
        m_dtmfSource = source;
        m_dtmfNext = 0;
        m_dtmfReceived.clear();
    }

    for (int i = 0; i < (int) dtmf.m_keys.size(); i++)
    {
        const int index = (dtmf.m_first + i) & 0x3F;

        // Behind the next to release: heard already (every key comes twice)
        if ((((index - m_dtmfNext) & 0x3F) >= 32) || m_dtmfReceived.count(index) || ((int) m_dtmfReceived.size() >= DTMFMaxKeys)) {
            continue;
        }

        m_dtmfReceived[index] = DTMFReceived{ dtmf.m_keys[i], dtmf.m_toneMs, dtmf.m_gapMs, sample };
        m_dtmfLastSample = sample;
    }

    dtmfRelease(sample, false);
}

void DSDAngel61Session::dtmfRelease(int64_t sample, bool flush)
{
    while (!m_dtmfReceived.empty())
    {
        auto it = m_dtmfReceived.find(m_dtmfNext);

        if (it != m_dtmfReceived.end())
        {
            DTMFKey key;
            key.m_key = it->second.m_key;
            key.m_toneMs = it->second.m_toneMs;
            key.m_gapMs = it->second.m_gapMs;
            key.m_sample = sample;
            m_dtmfKeys.push_back(key);

            if ((int) m_dtmfText.size() < DTMFMaxText) {
                m_dtmfText += key.m_key;
            }

            m_dtmfReceived.erase(it);
            m_dtmfNext = (m_dtmfNext + 1) & 0x3F;
            continue;
        }

        // The next key is missing: given up once a later one has waited long enough for its second send
        int64_t earliest = sample;

        for (const auto& r : m_dtmfReceived) {
            earliest = std::min(earliest, r.second.m_sample);
        }

        if (!flush && (sample - earliest < (int64_t) DTMFSkipFrames * m_frameSamples)) {
            break;
        }

        if ((int) m_dtmfText.size() < DTMFMaxText) {
            m_dtmfText += '?';
        }

        m_dtmfNext = (m_dtmfNext + 1) & 0x3F;
    }
}

void DSDAngel61Session::dtmfEnd()
{
    if (m_dtmfText.empty()) {
        return;
    }

    Message message;
    message.m_source = m_dtmfSource;
    message.m_destination = m_hasRouting ? m_routing.m_destination : DVAngel61::CallsignNone;
    message.m_contentType = DVAngel61::ContentText;
    message.m_text = m_dtmfText;
    message.m_dtmf = true;
    message.m_sample = m_sample;
    m_messages.push_back(message);
    m_dtmfText.clear();
}

std::vector<DSDAngel61Session::DTMFKey> DSDAngel61Session::takeDTMFKeys()
{
    std::vector<DTMFKey> keys;
    keys.swap(m_dtmfKeys);
    return keys;
}

std::vector<DSDAngel61Session::Message> DSDAngel61Session::takeMessages()
{
    std::vector<Message> messages;
    messages.swap(m_messages);
    return messages;
}

std::vector<DSDAngel61Session::PositionReport> DSDAngel61Session::takePositions()
{
    std::vector<PositionReport> positions;
    positions.swap(m_positions);
    return positions;
}

std::vector<std::string> DSDAngel61Session::takeAPRSPackets()
{
    std::vector<std::string> packets;
    packets.swap(m_aprsPackets);
    return packets;
}

std::vector<DSDAngel61Session::Correspondent> DSDAngel61Session::takeCorrespondentUpdates()
{
    std::vector<Correspondent> updates;
    updates.swap(m_correspondentUpdates);
    return updates;
}
