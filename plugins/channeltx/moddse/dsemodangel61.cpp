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

#include <chrono>

#include <QStringList>

#include "dsp/dv/dvdtmf.h"

#include "dsemodangel61.h"

static uint64_t callsignCode(const QString& text)
{
    return DVAngel61::encodeCallsign(text.trimmed().toUpper().toStdString());
}

DSEModAngel61::DSEModAngel61(DVAngel61::Profile profile) :
    m_profile(profile),
    m_slots(profile),
    m_positionValid(false),
    m_state(StateIdle),
    m_stopRequested(false),
    m_dataOnly(false),
    m_counter(0),
    m_frameCount(0),
    m_groupsSent(0),
    m_dataFramesSent(0),
    m_voiceFramesSent(0),
    m_voiceDataFramesSent(0),
    m_packetsSent(0),
    m_lastType(-1),
    m_tier(0),
    m_linkTier(0),
    m_requestTier(0),
    m_tierCap(3),
    m_groupFrame(DVAngel61::GroupFrames),
    m_groupConstellation(0),
    m_messageId(0),
    m_apiContentType(0),
    m_tunnelEnabled(false),
    m_tunnelAdvertisePending(false),
    m_burstFrames(0),
    m_lastAdvertiseMs(0),
    m_endedMs(0),
    m_repairsSent(0),
    m_tunnelDropped(0),
    m_nextTransferId(0),
    m_answerDestination(0),
    m_dtmfOnly(false),
    m_dtmfOnlyPending(false),
    m_dtmfHoldFrames(0),
    m_relaySequence(-1),
    m_relayNext(0),
    m_relayNextStart(false),
    m_relaying(false),
    m_relayDraining(false),
    m_relayDrainFrames(0),
    m_relayFrames(0),
    m_relayedTexts(0),
    m_relayedMessages(0),
    m_relayHoldFrames(0)
{
}

DSEModSettings::DSEModMode DSEModAngel61::getMode() const
{
    switch (m_profile)
    {
    case DVAngel61::ProfileWB:
        return DSEModSettings::DSEModModeAngel61WB;
    case DVAngel61::ProfileHFNB:
        return DSEModSettings::DSEModModeAngel61HFNB;
    case DVAngel61::ProfileHFWB:
        return DSEModSettings::DSEModModeAngel61HFWB;
    case DVAngel61::ProfileNB:
    default:
        return DSEModSettings::DSEModModeAngel61;
    }
}

const char *DSEModAngel61::getName() const
{
    switch (m_profile)
    {
    case DVAngel61::ProfileWB:
        return "Angel61 WB";
    case DVAngel61::ProfileHFNB:
        return "Angel61 HF-NB";
    case DVAngel61::ProfileHFWB:
        return "Angel61 HF-WB";
    case DVAngel61::ProfileNB:
    default:
        return "Angel61 NB";
    }
}

int DSEModAngel61::tierOf(const DSEModSettings& settings) const
{
    int tier;

    switch (settings.m_angel61Rate)
    {
    case DSEModSettings::DSEModAngel61RateLow:
        tier = 0;
        break;
    case DSEModSettings::DSEModAngel61RateMedium:
        tier = 1;
        break;
    case DSEModSettings::DSEModAngel61RateHigh:
        tier = 2;
        break;
    case DSEModSettings::DSEModAngel61RateTier3:
        tier = 3;
        break;
    default:
        tier = m_linkTier & 3;
        break;
    }

    return std::min(tier, m_tierCap);
}

bool DSEModAngel61::transmissionChanges(const DSEModSettings& settings) const
{
    auto destination = [](const QString& text) {
        return text.trimmed().isEmpty() ? DVAngel61::CallsignBroadcast : callsignCode(text);
    };
    auto path = [](const QString& text) {
        const QStringList list = text.split(',', Qt::SkipEmptyParts);
        return std::make_pair(list.size() > 0 ? callsignCode(list[0]) : 0, list.size() > 1 ? callsignCode(list[1]) : 0);
    };

    return (tierOf(settings) != m_tier)
        || (callsignCode(settings.m_angel61Source) != callsignCode(m_settings.m_angel61Source))
        || (destination(settings.m_angel61Destination) != destination(m_settings.m_angel61Destination))
        || (path(settings.m_angel61Path) != path(m_settings.m_angel61Path));
}

int DSEModAngel61::relayTierCap(DVAngel61::Profile profile, int inputType)
{
    if (DVAngel61::isOpus(inputType)) {
        return 3;
    }

    const int codebooks = DVAngel61::codebooksOf(inputType);

    for (int tier = 3; tier >= 0; tier--)
    {
        if (!DVAngel61::isOpus(DVAngel61::voiceType(profile, tier)) && (DVAngel61::tierCodebooks(profile, tier) <= codebooks)) {
            return tier;
        }
    }

    return -1;
}

int DSEModAngel61::dataType(int tier) const
{
    if (isHF()) {
        return DVAngel61::typeOf(m_profile, DVAngel61::KindGroupData + (tier & 3));
    }

    // VHF (2.8): the 16-QAM data frame when the tier's threshold is at or above its own, tier 3
    return DVAngel61::typeOf(m_profile, tier >= 3 ? DVAngel61::KindDataQAM16 : DVAngel61::KindData);
}

float DSEModAngel61::getLinearLevel() const
{
    // 16-QAM's peaks are 2.3 dB higher than QPSK's for the same RMS (2.1): a transmission that may
    // carry a 16-QAM frame, its voice or its data, is at 16-QAM's level throughout, so that the level
    // does not step from frame to frame
    const bool qam16 = (!m_dataOnly && (DVAngel61::constellationOf(DVAngel61::voiceType(m_profile, m_tier)) == DVAngel61::QAM16))
        || (m_dataOnly && (DVAngel61::constellationOf(dataType(m_tier)) == DVAngel61::QAM16));
    return qam16 ? QAM16Level : QPSKLevel;
}

bool DSEModAngel61::wantsDataOnly(const DSEModSettings& settings)
{
    return (settings.m_modAFInput == DSEModSettings::DSEModInputNone) && !settings.m_angel61Message.trimmed().isEmpty();
}

void DSEModAngel61::start(const DSEModSettings& settings)
{
    m_settings = settings;
    m_tier = tierOf(settings);
    // A relay's transmission is the caller's: this station's own traffic - its file transfer, its
    // tunnel's advertise and datagrams, its symbol - is not sent under the caller's callsign (its
    // queued messages wait for the relay to end: endSession)
    const bool relaying = m_relayNextStart;

    if (relaying)
    {
        m_heldMessages.insert(m_heldMessages.end(), m_messages.begin(), m_messages.end());
        m_messages.clear();
    }

    // A transfer keeps its block size to the end (3.4): one begun at a larger field than this
    // tier's starts again, with a new transfer id, at this tier's size
    if (!relaying && m_fileSender.isActive() && (m_fileSender.getOffer().m_blockSize > fieldBytes(m_tier)))
    {
        const std::string name = m_fileSender.getOffer().m_name;
        const std::string mediaType = m_fileSender.getOffer().m_mediaType;
        const std::vector<uint8_t> data = m_fileSender.getData();
        startFile(name, data, mediaType);
    }

    // The tunnel's advertise when it opens or its terms change, at the head of the first
    // transmission, and of one 30 s or more after the last (3.5)
    const int64_t steadyMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    if (!relaying) {
        m_sentDatagrams.clear();
    }

    if (!relaying && m_tunnelEnabled && (m_tunnelAdvertisePending || (m_frameCount == 0) || (steadyMs - m_lastAdvertiseMs >= TunnelAdvertiseMs)))
    {
        queueTunnelControl(m_tunnelAdvertise);
        m_tunnelAdvertisePending = false;
        m_lastAdvertiseMs = steadyMs;
    }

    m_routing.m_source = callsignCode(settings.m_angel61Source);
    m_routing.m_destination = settings.m_angel61Destination.trimmed().isEmpty() ? DVAngel61::CallsignBroadcast : callsignCode(settings.m_angel61Destination);
    m_slots.setRouting(m_routing);
    if (!relaying) {
        m_fileSender.setGroup(DVAngel61::isGroup(m_routing.m_destination));
    }

    QStringList path = settings.m_angel61Path.split(',', Qt::SkipEmptyParts);
    DVAngel61::Path repeaters;
    repeaters.m_repeater1 = path.size() > 0 ? callsignCode(path[0]) : 0;
    repeaters.m_repeater2 = path.size() > 1 ? callsignCode(path[1]) : 0;

    if (repeaters.m_repeater1) {
        m_slots.setPath(repeaters);
    } else {
        m_slots.clearPath();
    }

    // Station and parameters (2.5): revision 3, and the family of the voice the tier uses (3.1): V1
    // with its model set, or O1 (WB tier 3's Opus, no model)
    DVAngel61::Station station;
    station.m_protocolVersion = DVAngel61::ProtocolVersion;
    station.m_codecFamily = isOpusTier() ? (int) DVAngel61::CodecFamilyO1 : DVAngel61::CodecFamilyV1;
    station.m_codecModel = isOpusTier() ? 0 : DVAngel61::CodecModelV1;
    station.m_nextTier = m_tier;
    station.m_tunnelOffered = m_tunnelEnabled && !relaying;
    const QString symbol = relaying ? QString() : settings.m_angel61Symbol.trimmed();

    if (symbol.size() >= 2)
    {
        station.m_symbolTable = symbol.at(0).toLatin1();
        station.m_symbolCode = symbol.at(1).toLatin1();
    }

    m_slots.setStation(station);
    m_relaying = m_relayNextStart;
    m_relayNextStart = false;
    m_relayDraining = false;
    m_relayDrainFrames = 0;
    m_relayFrames = 0;
    m_relayedTexts = 0;
    m_relayedMessages = 0;
    m_relayHoldFrames = 0;
    m_relayReassembler = DVAngel61Reassembler();

    // A relay's position slots are the caller's, if it sends one (relayFrame): never this station's
    // own under the caller's callsign
    if (m_positionValid && settings.m_sendPosition && !m_relaying) {
        m_slots.setPosition(m_position, (m_position.m_speed > 0) && (m_position.m_speed != DVAngel61::Position::SpeedUnknown));
    } else {
        m_slots.clearPosition();
    }

    m_slots.reset();
    // A data transmission: data for the API, a message with no speech input, a file, or a tunnel's
    // datagrams with no speech input. With speech the datagrams go under the voice
    const bool noSpeech = settings.m_modAFInput == DSEModSettings::DSEModInputNone;
    const bool dataOnlyNext = m_dataOnlyNext && !m_relaying;
    // (keyed up for the tunnel alone: its datagrams, not the configured message as well)
    const bool tunnelOnly = dataOnlyNext && !m_dataOnlyNextMessage && m_apiData.empty() && !wantsDataOnly(settings);
    // (a file only while it has blocks to send: waiting for its answers, it made a voice transmission
    // data-only, which ended after its preamble with the speech dropped)
    m_dataOnly = !m_apiData.empty() || wantsDataOnly(settings) || (!relaying && fileWantsTransmission())
        || (noSpeech && tunnelWantsTransmission()) || dataOnlyNext;
    m_dataOnlyNext = false;
    m_dataOnlyNextMessage = false;

    // Keyed up for a file's answer (3.4): the transmission goes to the station that sent the file,
    // whose turn it hands back (3.5's reply slot), whatever the configured destination; keyed up
    // for the tunnel, to its peer, and for MaxTunnelBurstMs at most
    if (tunnelOnly && m_answerDestination && !fileWantsTransmission())
    {
        m_routing.m_destination = m_answerDestination;
        m_slots.setRouting(m_routing);
    }
    else if (tunnelOnly && m_tunnelEnabled && !fileWantsTransmission() && !settings.m_angel61TunnelPeer.trimmed().isEmpty())
    {
        m_routing.m_destination = callsignCode(settings.m_angel61TunnelPeer);
        m_slots.setRouting(m_routing);
    }

    m_burstFrames = tunnelOnly ? MaxTunnelBurstMs / DVAngel61::frameMs(m_profile) : 0;
    m_answerDestination = 0;

    // What of a message went out in the last transmission starts again: its fragments were for that one's fields
    // (a repair's are the original fragments, and keep their size: 3.5)
    for (PendingMessage& m : m_messages)
    {
        if (m.m_repair) {
            continue;
        }

        m.m_fieldBytes = 0;
        m.m_next = 0;
    }

    const std::string text = settings.m_angel61Message.trimmed().toStdString();

    if (m_dataOnly)
    {
        // The message in the data fields, every fragment twice
        const std::vector<uint8_t> bytes = !m_apiData.empty() ? m_apiData
            : tunnelOnly ? std::vector<uint8_t>() : std::vector<uint8_t>(text.begin(), text.end());

        if (!bytes.empty())
        {
            PendingMessage m;
            m.m_bytes = bytes;
            m.m_contentType = m_apiData.empty() ? DVAngel61::ContentText : m_apiContentType;
            m.m_id = m_messageId;
            m.m_repeats = 2;
            m_messageId = (m_messageId + 1) & 0xFF;
            m_messages.push_back(m);
        }

        m_apiData.clear();
    }
    else if (!text.empty() && (m_slots.pendingFragments() == 0))
    {
        m_slots.queueText(text);    // the slots of a keyed transmission
    }

    // DTMF queued for a transmission of its own before it started (3.8)
    m_dtmfOnly = m_dtmfOnlyPending && !m_dataOnly;
    m_dtmfOnlyPending = false;
    m_relaySequence = -1;
    m_relayNext = 0;

    m_counter = 0;
    m_frameCount = 0;
    m_packetsSent = 0;
    m_groupFrame = DVAngel61::GroupFrames;
    m_stopRequested = false;
    m_state = StatePreamble;
}

bool DSEModAngel61::queueDTMF(const std::string& keys, int toneMs, int gapMs, bool newSequence, bool hold, bool only)
{
    const std::string valid = DVDTMF::validKeys(keys);

    if (!m_slots.queueDTMF(valid, toneMs, gapMs, newSequence)) {
        return false;
    }

    // A Send's keys play for (tone + gap) each at the receiver, from the frame that carries the first
    // (the next free slot, within a frame or two): the voice waits that long
    if (hold)
    {
        const int playMs = (int) valid.size() * DVAngel61::dtmfUnits(toneMs) * DVAngel61::DTMF::UnitMs
            + (int) valid.size() * DVAngel61::dtmfUnits(gapMs) * DVAngel61::DTMF::UnitMs;
        const int frameMs = DVAngel61::frameMs(m_profile);
        m_dtmfHoldFrames += (playMs + frameMs - 1) / frameMs + 1;
    }

    if (only)
    {
        if (isActive()) {
            m_dtmfOnly = !m_dataOnly;
        } else {
            m_dtmfOnlyPending = true;
        }
    }

    return true;
}

void DSEModAngel61::relayDTMF(const DVAngel61::DTMF& dtmf)
{
    // A new sequence starts at its window's first key (its early keys may have been lost: from 0,
    // a window from 32 on was taken as relayed already), and its keys are a new sequence here until
    // one of them has gone into the queue (the next window's were appended to the last sequence)
    if (dtmf.m_sequence != m_relaySequence)
    {
        m_relaySequence = dtmf.m_sequence;
        m_relayNext = dtmf.m_first & 0x3F;
        m_relaySequenceQueued = false;
    }

    std::string keys;

    for (int i = 0; i < (int) dtmf.m_keys.size(); i++)
    {
        const int index = (dtmf.m_first + i) & 0x3F;

        // Ahead of the next key (or it): new; behind it: relayed already
        if (((index - m_relayNext) & 0x3F) < 32)
        {
            keys += dtmf.m_keys[i];
            m_relayNext = (index + 1) & 0x3F;
        }
    }

    if (!keys.empty() && queueDTMF(keys, dtmf.m_toneMs, dtmf.m_gapMs, !m_relaySequenceQueued, false, false)) {
        m_relaySequenceQueued = true;
    }
}

void DSEModAngel61::stop()
{
    // (not again once the drain has run its time: the processor asks for the stop on every frame
    // after the call's input ended, and each ask put the drain back, which then sent a frame that
    // was not the last, for ever)
    const bool drainOver = m_stopRequested || (m_relayDrainFrames * DVAngel61::frameMs(m_profile) > RelayDrainMs);

    if (relayPending() && (m_state != StateIdle) && !drainOver)
    {
        // The call has ended and its text or messages are not all out: data frames for the messages
        // (HF: groups), idle frames for the text, until they are or RelayDrainMs has passed
        m_relayDraining = true;

        if (!m_messages.empty()) {
            m_dataOnly = true;
        }

        return;
    }

    m_stopRequested = true;
}

void DSEModAngel61::relayFrame(const DVAngel61::Header& header, const std::vector<uint8_t>& block)
{
    const int type = header.m_type;
    const int timeMs = m_relayFrames++ * DVAngel61::frameMs(DVAngel61::profileOf(type));
    const uint64_t source = m_routing.m_source;

    for (int s = 0; s < DVAngel61::slotsOf(type); s++)
    {
        const uint8_t *slot = block.data() + DVAngel61::slotOffset(type, s);

        switch (DVAngel61::slotID(slot))
        {
        case DVAngel61::SlotDTMF:
        {
            DVAngel61::DTMF dtmf;

            if (DVAngel61::unpackDTMFSlot(slot, dtmf)) {
                relayDTMF(dtmf);
            }
            break;
        }

        case DVAngel61::SlotText:
        {
            DVAngel61::TextFragment fragment;
            DVAngel61Reassembler::Message completed;

            if (DVAngel61::unpackTextSlot(slot, fragment)
                && m_relayReassembler.addTextFragment(source, fragment, timeMs, completed)
                && m_slots.queueText(completed.text()))
            {
                m_relayedTexts++;
            }
            break;
        }

        case DVAngel61::SlotPosition:
        {
            DVAngel61::Position position;

            if (DVAngel61::unpackPositionSlot(slot, position)) {
                m_slots.setPosition(position, (position.m_speed > 0) && (position.m_speed != DVAngel61::Position::SpeedUnknown));
            }
            break;
        }

        default:
            break;
        }
    }

    // The message field (data under voice, a data frame): whole messages, fragmented again for this
    // station's fields
    const int bytes = DVAngel61::messageFieldBytes(type);

    if (bytes <= 0) {
        return;
    }

    DVAngel61::MessageFragment fragment;
    DVAngel61::unpackMessageField(block.data() + DVAngel61::messageFieldOffset(type), bytes, fragment);

    if ((fragment.m_contentType != DVAngel61::ContentText) && (fragment.m_contentType != DVAngel61::ContentBinary)
        && (fragment.m_contentType != DVAngel61::ContentAPRS)) {
        return;
    }

    DVAngel61Reassembler::Message completed;

    if (m_relayReassembler.addMessageFragment(source, fragment, timeMs, completed)) {
        relayMessage(completed.m_contentType, completed.m_flags, completed.m_data);
    }
}

void DSEModAngel61::relayMessage(int contentType, int flags, const std::vector<uint8_t>& message)
{
    // (at most MaxRelayedMessages waiting: they come from the network, and a queue that grew with
    // them made the call's every voice frame a data-under-voice one, and its drain endless)
    if (!DVAngel61::relayable(contentType) || message.empty() || ((int) m_messages.size() >= MaxRelayedMessages)) {
        return;
    }

    PendingMessage m;
    m.m_bytes = message;
    m.m_contentType = contentType;
    m.m_flags = flags & 0x0F;
    m.m_id = m_messageId;
    m_messageId = (m_messageId + 1) & 0xFF;
    m_messages.push_back(m);
    m_relayedMessages++;
}

void DSEModAngel61::endSession()
{
    abort();
    m_tierCap = 3;
    m_relayNextStart = false;
    m_relaying = false;
    m_relayDraining = false;
    m_apiData.clear();
    m_messages.clear();
    m_messages.swap(m_heldMessages);    // this station's own, set aside while it relayed
    m_slots.clearText();
    m_dataOnlyNext = false;
    m_dataOnlyNextMessage = false;
}

void DSEModAngel61::abort()
{
    m_state = StateIdle;
    m_stopRequested = false;
    m_groupFrame = DVAngel61::GroupFrames;
    // what was held for this transmission is not the next one's
    m_dtmfHoldFrames = 0;
    m_dtmfOnly = false;
    m_slots.clearDTMF();
}

int DSEModAngel61::queuedFragments(int bytes) const
{
    int count = 0;

    // (this station's own set aside while it relays too: they are bounded as well)
    for (const std::deque<PendingMessage> *queue : { &m_messages, &m_heldMessages })
    {
        for (const PendingMessage& m : *queue)
        {
            const int total = m.m_fieldBytes ? (int) m.m_fragments.size() : ((int) m.m_bytes.size() + 2 + bytes - 1) / std::max(1, bytes);
            count += (total - (int) m.m_next) + total * (m.m_repeats - 1);
        }
    }

    return count;
}

bool DSEModAngel61::nextMessageField(int bytes, DVAngel61::MessageFragment& fragment)
{
    while (!m_messages.empty())
    {
        PendingMessage& m = m_messages.front();

        // A repair's fragments go in fields of their size, or not at all (3.5)
        if (m.m_repair)
        {
            if (bytes != m.m_fieldBytes)
            {
                if (++m.m_otherFields < MaxOtherFields) {
                    return false;
                }

                m_messages.pop_front();
                continue;
            }

            fragment = m.m_fragments[m.m_next++];
            m.m_otherFields = 0;

            if (m.m_next >= m.m_fragments.size()) {
                m_messages.pop_front();
            }

            m_repairsSent++;
            return true;
        }

        // A message part sent waits for fields of its fragments' size rather than a smaller one's,
        // for a while: fragmented again, it started over, and a relay whose frames' kinds changed
        // with the call's never finished one. A larger field takes it at once (an HF call's data
        // groups after its voice-data frames: waiting, the drain sent groups without it)
        if ((bytes < m.m_fieldBytes) && (m.m_next > 0) && (++m.m_otherFields < MaxOtherFields)) {
            return false;
        }

        if (m.m_fieldBytes != bytes)
        {
            m.m_fragments.clear();
            m.m_next = 0;
            m.m_fieldBytes = bytes;

            if (DVAngel61::fragmentMessage(m.m_bytes, m.m_contentType, m.m_id, bytes, m.m_fragments, m.m_flags) <= 0)
            {
                m_messages.pop_front();   // too long for this field: 255 fragments at most (3.2)
                continue;
            }
        }

        fragment = m.m_fragments[m.m_next++];
        m.m_otherFields = 0;

        if (m.m_next >= m.m_fragments.size())
        {
            // A datagram sent: kept, as it was, until the next transmission, for a repair (3.5)
            if ((m.m_contentType == DVAngel61::ContentIPDatagram) && (m_sentDatagrams.size() < 64))
            {
                SentDatagram& sent = m_sentDatagrams[m.m_id];
                sent.m_fieldBytes = m.m_fieldBytes;
                sent.m_fragments = m.m_fragments;
            }

            if (--m.m_repeats > 0) {
                m.m_next = 0;
            } else {
                m_messages.pop_front();
            }
        }

        return true;
    }

    return false;
}

bool DSEModAngel61::queueMessage(const std::string& text)
{
    // During a transmission: the slot fragments (7 bit ASCII, up to 180 characters)
    std::string ascii;

    for (char c : text) {
        ascii += ((unsigned char) c < 128) ? c : '?';
    }

    if (ascii.size() > 15 * DVAngel61::TextFragment::Characters) {
        ascii.resize(15 * DVAngel61::TextFragment::Characters);
    }

    return m_slots.queueText(ascii);
}

bool DSEModAngel61::startFile(const std::string& name, const std::vector<uint8_t>& data, const std::string& mediaType)
{
    // Blocks of the data field of the tier the next transmission will use (3.4)
    m_nextTransferId = (m_nextTransferId + 1) & 0xFF;
    m_fileSender.setOfferInterval(isHF() ? HFOfferIntervalGroups : DVAngel61FileSender::OfferIntervalFrames);
    return m_fileSender.start(name, data, m_nextTransferId, fieldBytes(tierOf(m_settings)), mediaType);
}

bool DSEModAngel61::queueDatagram(const std::vector<uint8_t>& datagram)
{
    if (!m_tunnelEnabled || datagram.empty()) {
        return false;
    }

    const int tier = tierOf(m_settings);
    const int bytes = fieldBytes(tier);

    if ((int) datagram.size() > maxDatagram(tier))
    {
        m_tunnelDropped++;
        return false;
    }

    const int count = ((int) datagram.size() + 2 + bytes - 1) / bytes;

    if (queuedFragments(bytes) + count > MaxTunnelFragments)
    {
        m_tunnelDropped++;
        return false;
    }

    // Compressed when the peer takes it (3.6); only a datagram going on the air reaches the compressor
    PendingMessage m;
    const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    m_compressor.compress(datagram, nowMs, m.m_bytes, m.m_flags);
    m.m_contentType = DVAngel61::ContentIPDatagram;
    m.m_id = m_messageId;
    m_messageId = (m_messageId + 1) & 0xFF;
    (m_relaying ? m_heldMessages : m_messages).push_back(m);   // (not in a relayed call: after it)
    return true;
}

bool DSEModAngel61::queueTunnelControl(const DVAngel61IP::Control& control)
{
    const std::vector<uint8_t> bytes = control.encode();

    if (bytes.empty() || (queuedFragments(fieldBytes(tierOf(m_settings))) >= MaxTunnelFragments)) {
        return false;
    }

    PendingMessage m;
    m.m_bytes = bytes;
    m.m_contentType = DVAngel61::ContentTunnelControl;
    m.m_id = m_messageId;
    m_messageId = (m_messageId + 1) & 0xFF;

    // In front of the datagrams (3.5), but not of a message already part sent; while relaying, of
    // this station's own set aside for after the relay
    if (m_relaying)
    {
        m_heldMessages.push_front(m);
        return true;
    }

    std::deque<PendingMessage>::iterator at = m_messages.begin();

    if ((at != m_messages.end()) && (at->m_fieldBytes != 0)) {
        ++at;
    }

    m_messages.insert(at, m);
    return true;
}

bool DSEModAngel61::queueFileControl(const DVAngel61FileControl& control, const std::string& destination)
{
    const std::vector<uint8_t> bytes = control.encode();

    if (bytes.empty()) {
        return false;
    }

    PendingMessage m;
    m.m_bytes = bytes;
    m.m_contentType = DVAngel61::ContentFileControl;
    m.m_id = m_messageId;
    m.m_repeats = 2;    // the sender waits for it: every fragment twice, as a data transmission's message
    m_messageId = (m_messageId + 1) & 0xFF;
    m_answerDestination = destination.empty() ? 0 : callsignCode(QString::fromStdString(destination));

    // What is owed goes first (3.5): after tunnel control and a message already part sent, ahead of
    // the datagrams; while relaying, with this station's own set aside for after the relay
    std::deque<PendingMessage>& queue = m_relaying ? m_heldMessages : m_messages;
    std::deque<PendingMessage>::iterator at = queue.begin();

    if (!m_relaying && (at != queue.end()) && (at->m_fieldBytes != 0)) {
        ++at;
    }

    while ((at != queue.end()) && (at->m_contentType == DVAngel61::ContentTunnelControl)) {
        ++at;
    }

    queue.insert(at, m);
    return true;
}

void DSEModAngel61::setTunnel(bool enabled, const DVAngel61IP::Control& advertise)
{
    if ((!m_tunnelEnabled && enabled) || (enabled && (advertise.encode() != m_tunnelAdvertise.encode()))) {
        m_tunnelAdvertisePending = true;
    }

    m_tunnelEnabled = enabled;
    m_tunnelAdvertise = advertise;

    if (!enabled) {
        m_tunnelAdvertisePending = false;
    }
}

void DSEModAngel61::handlePeerTunnelControl(const DVAngel61IP::Control& control)
{
    switch (control.m_kind)
    {
    case DVAngel61IP::Control::Repair:
    {
        // The fragments the peer lacks of a datagram of the last transmission, sent again as they
        // were, with its message id, at the head of the next (3.5); a datagram no longer held - the
        // window past, or another transmission since - is not
        const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        auto it = m_sentDatagrams.find(control.m_messageId);

        if ((m_state != StateIdle) || (it == m_sentDatagrams.end()) || (nowMs - m_endedMs > TunnelRepairWindowMs)) {
            break;
        }

        PendingMessage m;
        m.m_contentType = DVAngel61::ContentIPDatagram;
        m.m_id = control.m_messageId;
        m.m_fieldBytes = it->second.m_fieldBytes;
        m.m_repair = true;

        for (const auto& range : control.m_ranges)
        {
            for (int i = range.first; (i < range.first + range.second) && (i < (int) it->second.m_fragments.size()); i++) {
                m.m_fragments.push_back(it->second.m_fragments[i]);
            }
        }

        if (!m.m_fragments.empty())
        {
            std::deque<PendingMessage>::iterator at = m_messages.begin();

            while ((at != m_messages.end()) && ((at->m_contentType == DVAngel61::ContentTunnelControl) || at->m_repair)) {
                ++at;
            }

            m_messages.insert(at, m);
        }

        break;
    }
    case DVAngel61IP::Control::Advertise:
        m_compressor.setPeer(control.m_compression);
        break;
    case DVAngel61IP::Control::Close:
        m_compressor.setPeer(0);
        break;
    case DVAngel61IP::Control::ContextState:
        m_compressor.refresh(control.m_contexts);
        break;
    default:
        break;
    }
}

void DSEModAngel61::idleBlock(bool last, DVAngel61::Header& header, std::vector<uint8_t>& block)
{
    header = DVAngel61::Header();
    header.m_type = DVAngel61::typeOf(m_profile, DVAngel61::KindIdle);
    header.m_counter = m_counter;
    header.m_lastFrame = last;
    header.m_tier = m_requestTier;

    block.assign(DVAngel61::infoBits(header.m_type), 0);
    m_slots.nextSlots(m_counter, DVAngel61::slotsOf(header.m_type), last, block.data() + DVAngel61::slotOffset(header.m_type, 0));
    DVAngel61::finishBlock(block.data(), header);
}

void DSEModAngel61::voiceBlock(DVAngel61::Header& header, std::vector<uint8_t>& block)
{
    // Data under voice (2.8): the tier's one-step kind while a message waits, when the tier has one.
    // Its field takes any fragment the queue makes for it, which is the smallest step that holds it.
    // Opus (WB tier 3) has none: one 80 ms packet, the rest of the block its slot
    int type = DVAngel61::voiceType(m_profile, m_tier);
    uint8_t first[DSEModVocoder::m_maxFrameBytes] = {};

    // A relay with no packet of the call ready (the network's frames late, or the call's end still to
    // come) sends an idle frame, not one of silence: no packet is lost (those that come go in the
    // next voice frame), the listener hears the same, and the frame's slots carry the call's text
    if (!pullVocoderFrame(first) && m_relaying)
    {
        idleBlock(false, header, block);
        m_relayHoldFrames++;
        return;
    }

    if (DVAngel61::isOpus(type))
    {
        header = DVAngel61::Header();
        header.m_type = type;
        header.m_counter = m_counter;
        header.m_lastFrame = false;
        header.m_tier = m_requestTier;
        block.assign(DVAngel61::infoBits(type), 0);
        uint8_t bits[DVAngel61::OpusBits];
        bytesToBits(first, DVAngel61::OpusBits, bits);
        DVAngel61::putOpus(block.data(), bits);
        m_packetsSent++;
        m_slots.nextSlots(m_counter, DVAngel61::slotsOf(type), false, block.data() + DVAngel61::slotOffset(type, 0));
        DVAngel61::finishBlock(block.data(), header);
        m_voiceFramesSent++;
        return;
    }

    const int oneStep = DVAngel61::voiceDataType(m_profile, m_tier, 1);

    if (!m_messages.empty() && (oneStep >= 0)) {
        type = oneStep;
    }

    // The frame's packets as the encoder made them (16 codebooks), or as a relay gives them: those
    // may carry fewer (a call arriving at a lower tier, or its data-under-voice frames). The frame's
    // kind then steps down with them, one or two steps (2.3), so that it never needs a codebook the
    // packets do not have: a relay only truncates (3.1.1)
    const int packets = DVAngel61::packetsPerFrame(m_profile);
    uint8_t frame[18][DSEModVocoder::m_maxFrameBytes] = {};
    int available = 16;

    for (int p = 0; p < packets; p++)
    {
        if (p == 0) {
            std::copy(first, first + DSEModVocoder::m_maxFrameBytes, frame[0]);
        } else {
            pullVocoderFrame(frame[p]);
        }
        const int codebooks = frame[p][DSEModVocoder::m_vocosCodebooksByte];
        available = std::min(available, codebooks > 0 ? codebooks : 16);
    }

    for (int steps = 1; (DVAngel61::codebooksOf(type) > available) && (steps <= 2); steps++)
    {
        const int lower = DVAngel61::voiceDataType(m_profile, m_tier, steps);

        if (lower >= 0) {
            type = lower;
        }
    }

    // Still more codebooks than the packets carry (a relayed call's data-under-voice frames below
    // this tier's two steps): the profile's kind at this tier or below with the most codebooks the
    // packets have - frame kinds describe themselves - and none, an idle frame. The missing codebooks
    // were zeros, which decode as index 0, not as absent: a relay only truncates (3.1.1)
    if (DVAngel61::codebooksOf(type) > available)
    {
        int best = -1;

        for (int t = m_tier; t >= 0; t--)
        {
            for (int steps = 0; steps <= 2; steps++)
            {
                const int candidate = steps == 0 ? DVAngel61::voiceType(m_profile, t) : DVAngel61::voiceDataType(m_profile, t, steps);

                if ((candidate >= 0) && !DVAngel61::isOpus(candidate) && (DVAngel61::codebooksOf(candidate) > 0)
                    && (DVAngel61::codebooksOf(candidate) <= available)
                    && ((best < 0) || (DVAngel61::codebooksOf(candidate) > DVAngel61::codebooksOf(best)))) {
                    best = candidate;
                }
            }
        }

        if (best < 0)
        {
            idleBlock(false, header, block);
            m_relayHoldFrames++;
            return;
        }

        type = best;
    }

    header = DVAngel61::Header();
    header.m_type = type;
    header.m_counter = m_counter;
    header.m_lastFrame = false;
    header.m_tier = m_requestTier;
    block.assign(DVAngel61::infoBits(type), 0);

    const int bits = DVAngel61::packetBits(type);
    uint8_t packetBits[DVAngel61::CodebookBits * 16];

    for (int p = 0; p < packets; p++)
    {
        bytesToBits(frame[p], bits, packetBits);
        DVAngel61::putVoicePacket(block.data(), type, p, packetBits);
    }

    m_packetsSent += packets;
    m_slots.nextSlots(m_counter, DVAngel61::slotsOf(type), false, block.data() + DVAngel61::slotOffset(type, 0));

    // (a kind with a field: a lower tier's plain voice kind has none, and a field of 0 bytes would
    // have dropped the message waiting)
    if ((type != DVAngel61::voiceType(m_profile, m_tier)) && (DVAngel61::messageFieldBytes(type) > 0))
    {
        const int bytes = DVAngel61::messageFieldBytes(type);
        DVAngel61::MessageFragment fragment;

        if (nextMessageField(bytes, fragment)) {
            DVAngel61::packMessageField(fragment, bytes, block.data() + DVAngel61::messageFieldOffset(type));
        }

        m_voiceDataFramesSent++;
    }

    DVAngel61::finishBlock(block.data(), header);
    m_voiceFramesSent++;
}

void DSEModAngel61::dataBlock(DVAngel61::Header& header, std::vector<uint8_t>& block)
{
    // VHF's single data frame: a slot and one message field
    const int type = dataType(m_tier);
    header = DVAngel61::Header();
    header.m_type = type;
    header.m_counter = m_counter;
    header.m_lastFrame = false;
    header.m_tier = m_requestTier;
    block.assign(DVAngel61::infoBits(type), 0);
    m_slots.nextSlots(m_counter, DVAngel61::slotsOf(type), false, block.data() + DVAngel61::slotOffset(type, 0));

    // What is waiting first - tunnel control, datagrams, messages - then the file (3.4)
    const int bytes = DVAngel61::messageFieldBytes(type);
    DVAngel61::MessageFragment fragment;

    if (nextMessageField(bytes, fragment) || (!m_relaying && m_fileSender.nextFieldBytes(bytes, fragment))) {
        DVAngel61::packMessageField(fragment, bytes, block.data() + DVAngel61::messageFieldOffset(type));
    }

    DVAngel61::finishBlock(block.data(), header);
    m_dataFramesSent++;
}

bool DSEModAngel61::nextBlock(DVAngel61::Header& header, std::vector<uint8_t>& block)
{
    if (m_state == StateIdle) {
        return false;
    }

    if (m_state == StatePreamble)
    {
        // The preamble's two idle frames (2.8)
        idleBlock(false, header, block);

        if (m_counter + 1 >= 2) {
            m_state = StateRun;
        }
    }
    else if (m_relayDraining && ((++m_relayDrainFrames * DVAngel61::frameMs(m_profile) > RelayDrainMs) || !relayPending()))
    {
        // A relay's text and messages out (or given up): the last frame next
        m_relayDraining = false;
        m_stopRequested = true;
        idleBlock(false, header, block);
    }
    else if ((m_stopRequested || m_dtmfOnly || (m_dataOnly && !dataContinues() && !m_relayDraining)) && !dtmfDue())
    {
        // The last frame: an idle frame with the flag, its slot the routing. A transmission with DTMF
        // still to send or to play is held until it has (3.8)
        idleBlock(true, header, block);
        m_state = StateIdle;
        m_stopRequested = false;
        m_dtmfOnly = false;
        m_dtmfHoldFrames = 0;
        m_endedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    else if (m_dtmfOnly || (!m_dataOnly && (m_stopRequested || m_relayDraining || (m_dtmfHoldFrames > 0))))
    {
        // Idle frames while a Send's keys play (3.8): the voice's packets for the frame are dropped,
        // so that the speech after the keys keeps its time
        idleBlock(false, header, block);

        if (!m_dtmfOnly)
        {
            uint8_t packet[DSEModVocoder::m_maxFrameBytes];

            for (int p = 0; p < getFramesPerBlock(); p++) {
                pullVocoderFrame(packet);
            }
        }
    }
    else if (m_dataOnly)
    {
        // HF's data goes in groups (nextCells); between them, idle frames up to a group's start
        if (isHF()) {
            idleBlock(false, header, block);
        } else {
            dataBlock(header, block);
        }
    }
    else
    {
        voiceBlock(header, block);
    }

    // A Send's hold counts down on every frame, a data transmission's too: counted only where it
    // held back voice, a DTMF Send during a data transmission kept it from ever ending
    if (m_dtmfHoldFrames > 0) {
        m_dtmfHoldFrames--;
    }

    m_lastType = header.m_type;
    m_counter = (m_counter + 1) & 0x3F;
    m_frameCount++;
    return true;
}

bool DSEModAngel61::nextSymbols(std::vector<std::complex<float>>& symbols)
{
    if (isHF()) {
        return false;
    }

    DVAngel61::Header header;
    std::vector<uint8_t> block;

    if (!nextBlock(header, block)) {
        return false;
    }

    symbols.resize(DVAngel61::layout(m_profile).m_cells);
    DVAngel61::buildFrame(header, block.data(), symbols.data());
    return true;
}

void DSEModAngel61::buildGroup()
{
    // Four frames from counter c (c mod 4 = 2), one block: a slot a frame and one message field
    const int type = dataType(m_tier);
    DVAngel61::Header any;
    any.m_type = type;
    any.m_counter = m_counter;
    any.m_tier = m_requestTier;
    DVAngel61::Header headers[DVAngel61::GroupFrames];
    DVAngel61::groupHeaders(any, false, headers);

    std::vector<uint8_t> block(DVAngel61::infoBits(type), 0);

    for (int f = 0; f < DVAngel61::GroupFrames; f++)
    {
        // One slot per frame, so that the slot queue's clock and routing cadence run per frame
        m_slots.nextSlots((m_counter + f) & 0x3F, 1, false, block.data() + DVAngel61::slotOffset(type, f));
    }

    // What is waiting first - tunnel control, datagrams, messages - then the file (3.4)
    const int bytes = DVAngel61::messageFieldBytes(type);
    DVAngel61::MessageFragment fragment;

    if (nextMessageField(bytes, fragment) || (!m_relaying && m_fileSender.nextFieldBytes(bytes, fragment))) {
        DVAngel61::packMessageField(fragment, bytes, block.data() + DVAngel61::messageFieldOffset(type));
    }

    DVAngel61::finishBlock(block.data(), headers, DVAngel61::GroupFrames);
    m_groupCells.resize((size_t) DVAngel61::GroupFrames * DVAngel61::layout(m_profile).m_cells);
    DVAngel61::buildGroup(headers, block.data(), m_groupCells.data());
    m_groupConstellation = DVAngel61::constellationOf(type);
    m_groupFrame = 0;
    m_groupsSent++;
    m_lastType = type;
}

bool DSEModAngel61::nextCells(std::vector<std::complex<float>>& cells, int& constellation)
{
    if (!isHF() || (m_state == StateIdle)) {
        return false;
    }

    // The rest of a group (its frames count towards a relay's drain: they are frames of it)
    if (m_groupFrame < DVAngel61::GroupFrames)
    {
        if (m_relayDraining) {
            m_relayDrainFrames++;
        }

        if (m_dtmfHoldFrames > 0) {
            m_dtmfHoldFrames--;
        }

        const int n = DVAngel61::layout(m_profile).m_cells;
        cells.assign(m_groupCells.begin() + (size_t) m_groupFrame * n, m_groupCells.begin() + (size_t) (m_groupFrame + 1) * n);
        constellation = m_groupConstellation;
        m_groupFrame++;
        m_counter = (m_counter + 1) & 0x3F;
        m_frameCount++;
        return true;
    }

    // A data transmission's group, when something waits and the counter is at a group's start
    if ((m_state == StateRun) && m_dataOnly && !m_stopRequested && dataContinues() && (DVAngel61::groupIndex(m_counter) == 0)
        && !(m_relayDraining && ((m_relayDrainFrames + DVAngel61::GroupFrames) * DVAngel61::frameMs(m_profile) > RelayDrainMs)))
    {
        buildGroup();
        return nextCells(cells, constellation);
    }

    DVAngel61::Header header;
    std::vector<uint8_t> block;

    if (!nextBlock(header, block)) {
        return false;
    }

    cells.resize(DVAngel61::layout(m_profile).m_cells);
    DVAngel61::buildFrame(header, block.data(), cells.data());
    constellation = DVAngel61::constellationOf(header.m_type);
    return true;
}

void DSEModAngel61::setPosition(bool valid, double latitude, double longitude, float altitude, int speed, int course, int source)
{
    m_positionValid = valid;
    m_position = DVAngel61::Position();

    if (valid)
    {
        m_position.m_latitude = latitude;
        m_position.m_longitude = longitude;
        m_position.m_altitude = (int) altitude;
        m_position.m_speed = speed;
        m_position.m_course = course;
        m_position.m_fixAge = 0;
        m_position.m_source = source;
    }
}
